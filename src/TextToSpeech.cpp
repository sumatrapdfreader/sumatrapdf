#include "base/Base.h"
#include "base/Win.h"

#include <roapi.h>
#include <windows.media.speechsynthesis.h>
// must come after the windows.media headers: both define SpeechRecognizerState
// and this order compiles in both msvc and clang
#include <sapi.h>

#include <TextToSpeech.h>

#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "winmm.lib")

/*
Two implementations:
- Windows.Media.SpeechSynthesis (WinRT, Windows 10+), preferred because it
  sees the modern OneCore voices. It only synthesizes to a WAV stream so we
  play it ourselves with waveOut, which also gives us pause position from
  word boundary cues embedded in the stream.
- SAPI 5 as fallback for systems without WinRT.

WinRT functions are resolved dynamically from combase.dll / shcore.dll so
that we don't import them statically (the exe must still load on Windows 7).
*/

enum class TtsBackend {
    Unknown,
    WinRt,
    Sapi
};
static TtsBackend gTtsBackend = TtsBackend::Unknown;

// shared state
static bool gTtsActive = false;

// copy of the text passed to last speak request and the position (in WCHARs)
// of the last word boundary reached, for resuming stopped speech
static WStr gTtsSpokenText;

static Str gTtsVoiceId;

// playback speed multiplier, 1.0 is normal speed
constexpr float kTtsSpeedMin = 0.5f;
constexpr float kTtsSpeedMax = 3.0f;
static float gTtsSpeed = 1.0f;

static HWND gTtsNotifyHwnd = nullptr;
static UINT gTtsNotifyMsg = 0;
static WPARAM gTtsNotifyWParam = 0;
static LPARAM gTtsNotifyLParam = 0;

static void TtsPostNotifyMsg() {
    if (gTtsNotifyHwnd && gTtsNotifyMsg) {
        PostMessageW(gTtsNotifyHwnd, gTtsNotifyMsg, gTtsNotifyWParam, gTtsNotifyLParam);
    }
}

static Str TtsVoiceLangForSort(const TtsVoiceInfo& voice) {
    return len(voice.lang) == 0 ? StrL("ffff") : voice.lang;
}

static bool TtsVoiceLess(const TtsVoiceInfo& a, const TtsVoiceInfo& b) {
    int langCmp = lstrcmpiA(TtsVoiceLangForSort(a).s, TtsVoiceLangForSort(b).s);
    if (langCmp != 0) {
        return langCmp < 0;
    }

    return lstrcmpiA(a.name ? a.name.s : "", b.name ? b.name.s : "") < 0;
}

static void TtsSortVoicesByLanguage(Vec<TtsVoiceInfo>& voices) {
    for (int i = 1; i < len(voices); i++) {
        TtsVoiceInfo value = voices[i];
        int j = i - 1;

        while (j >= 0 && TtsVoiceLess(value, voices[j])) {
            voices[j + 1] = voices[j];
            j--;
        }

        voices[j + 1] = value;
    }
}

//--- SAPI 5 implementation

static ISpVoice* gSapiVoice = nullptr;
static bool gSapiCoInitialized = false;
static ULONG gSapiStreamNum = 0;
static ULONG gSapiLastWordPos = 0;

// Voice token lookup and metadata

static ISpObjectToken* SapiFindVoiceTokenById(Str voiceId) {
    if (len(voiceId) == 0) {
        return nullptr;
    }

    WCHAR* wantedId = CWStrTemp(voiceId);
    if (!wantedId) {
        return nullptr;
    }

    ISpObjectTokenCategory* category = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
                                  (void**)&category);
    if (FAILED(hr) || !category) {
        return nullptr;
    }

    hr = category->SetId(SPCAT_VOICES, FALSE);
    if (FAILED(hr)) {
        category->Release();
        return nullptr;
    }

    IEnumSpObjectTokens* enumTokens = nullptr;
    hr = category->EnumTokens(nullptr, nullptr, &enumTokens);
    category->Release();

    if (FAILED(hr) || !enumTokens) {
        return nullptr;
    }

    ISpObjectToken* result = nullptr;
    ISpObjectToken* token = nullptr;
    ULONG fetched = 0;

    while (enumTokens->Next(1, &token, &fetched) == S_OK && fetched > 0) {
        WCHAR* idW = nullptr;
        hr = token->GetId(&idW);

        if (SUCCEEDED(hr) && idW && str::EqI(ToUtf8Temp(idW), voiceId)) {
            result = token;
            token = nullptr;
            CoTaskMemFree(idW);
            break;
        }

        if (idW) {
            CoTaskMemFree(idW);
        }

        token->Release();
        token = nullptr;
        fetched = 0;
    }

    enumTokens->Release();
    return result;
}

static Str SapiGetVoiceLanguage(ISpObjectToken* token) {
    if (!token) {
        return {};
    }

    ISpDataKey* attributes = nullptr;
    HRESULT hr = token->OpenKey(L"Attributes", &attributes);
    if (FAILED(hr) || !attributes) {
        return {};
    }

    WCHAR* langW = nullptr;
    hr = attributes->GetStringValue(L"Language", &langW);
    attributes->Release();

    if (FAILED(hr) || !langW) {
        return {};
    }

    Str lang = str::Dup(ToUtf8Temp(langW));
    CoTaskMemFree(langW);
    return lang;
}

static void SapiSetNotify() {
    if (!gSapiVoice) {
        return;
    }

    ISpEventSource* eventSource = nullptr;
    HRESULT hr = gSapiVoice->QueryInterface(IID_ISpEventSource, (void**)&eventSource);
    if (FAILED(hr) || !eventSource) {
        return;
    }

    const ULONGLONG events = SPFEI(SPEI_END_INPUT_STREAM) | SPFEI(SPEI_WORD_BOUNDARY);
    eventSource->SetInterest(events, events);

    if (gTtsNotifyHwnd && gTtsNotifyMsg) {
        eventSource->SetNotifyWindowMessage(gTtsNotifyHwnd, gTtsNotifyMsg, gTtsNotifyWParam, gTtsNotifyLParam);
    }

    eventSource->Release();
}

// SAPI rate is -10 .. 10 on a logarithmic scale where 10 is ~3x and -10 ~1/3x,
// so rate = 10 * log3(speed)
static void SapiApplySpeed() {
    if (!gSapiVoice) {
        return;
    }
    double rate = 10.0 * log((double)gTtsSpeed) / log(3.0);
    long rateAdjust = (long)(rate < 0 ? rate - 0.5 : rate + 0.5);
    gSapiVoice->SetRate(rateAdjust);
}

static bool SapiInit() {
    if (gSapiVoice) {
        return true;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        gSapiCoInitialized = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&gSapiVoice);
    if (FAILED(hr)) {
        gSapiVoice = nullptr;

        if (gSapiCoInitialized) {
            CoUninitialize();
            gSapiCoInitialized = false;
        }

        return false;
    }

    if (len(gTtsVoiceId) > 0) {
        ISpObjectToken* token = SapiFindVoiceTokenById(gTtsVoiceId);
        if (token) {
            gSapiVoice->SetVoice(token);
            token->Release();
        }
    }

    SapiApplySpeed();
    SapiSetNotify();
    return true;
}

static void SapiRelease() {
    if (gSapiVoice) {
        gSapiVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
        gSapiVoice->Release();
        gSapiVoice = nullptr;
    }

    gSapiStreamNum = 0;
    gSapiLastWordPos = 0;

    if (gSapiCoInitialized) {
        CoUninitialize();
        gSapiCoInitialized = false;
    }
}

static void SapiGetVoices(Vec<TtsVoiceInfo>& voices) {
    if (!SapiInit()) {
        return;
    }

    ISpObjectTokenCategory* category = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
                                  (void**)&category);
    if (FAILED(hr) || !category) {
        return;
    }

    hr = category->SetId(SPCAT_VOICES, FALSE);
    if (FAILED(hr)) {
        category->Release();
        return;
    }

    IEnumSpObjectTokens* enumTokens = nullptr;
    hr = category->EnumTokens(nullptr, nullptr, &enumTokens);
    category->Release();

    if (FAILED(hr) || !enumTokens) {
        return;
    }

    ISpObjectToken* token = nullptr;
    ULONG fetched = 0;

    while (enumTokens->Next(1, &token, &fetched) == S_OK && fetched > 0) {
        WCHAR* idW = nullptr;
        WCHAR* nameW = nullptr;

        HRESULT idHr = token->GetId(&idW);
        HRESULT nameHr = token->GetStringValue(nullptr, &nameW);

        if (SUCCEEDED(idHr) && idW && SUCCEEDED(nameHr) && nameW) {
            TtsVoiceInfo info{};
            info.id = str::Dup(ToUtf8Temp(idW));
            info.name = str::Dup(ToUtf8Temp(nameW));
            info.lang = SapiGetVoiceLanguage(token);
            voices.Append(info);
        }

        if (idW) {
            CoTaskMemFree(idW);
        }
        if (nameW) {
            CoTaskMemFree(nameW);
        }

        token->Release();
        token = nullptr;
        fetched = 0;
    }

    enumTokens->Release();
}

static bool SapiSetVoiceById(Str voiceId) {
    if (!SapiInit()) {
        return false;
    }

    HRESULT hr = E_FAIL;

    if (len(voiceId) == 0) {
        hr = gSapiVoice->SetVoice(nullptr);
    } else {
        ISpObjectToken* token = SapiFindVoiceTokenById(voiceId);
        if (!token) {
            return false;
        }

        hr = gSapiVoice->SetVoice(token);
        token->Release();
    }

    return SUCCEEDED(hr);
}

static void SapiClearEvent(SPEVENT* eventItem) {
    if (!eventItem) {
        return;
    }

    switch (eventItem->elParamType) {
        case SPET_LPARAM_IS_TOKEN:
        case SPET_LPARAM_IS_OBJECT:
            if (eventItem->lParam) {
                IUnknown* unknown = reinterpret_cast<IUnknown*>(eventItem->lParam);
                unknown->Release();
            }
            break;

        case SPET_LPARAM_IS_POINTER:
        case SPET_LPARAM_IS_STRING:
            if (eventItem->lParam) {
                CoTaskMemFree(reinterpret_cast<void*>(eventItem->lParam));
            }
            break;

        default:
            break;
    }

    eventItem->eEventId = SPEI_UNDEFINED;
    eventItem->elParamType = SPET_LPARAM_IS_UNDEFINED;
    eventItem->ulStreamNum = 0;
    eventItem->ullAudioStreamOffset = 0;
    eventItem->wParam = 0;
    eventItem->lParam = 0;
}

static void SapiProcessEvents() {
    if (!gSapiVoice) {
        return;
    }

    ISpEventSource* eventSource = nullptr;
    HRESULT hr = gSapiVoice->QueryInterface(IID_ISpEventSource, (void**)&eventSource);
    if (FAILED(hr) || !eventSource) {
        return;
    }

    SPEVENT eventItem = {};
    ULONG fetched = 0;

    while (eventSource->GetEvents(1, &eventItem, &fetched) == S_OK && fetched > 0) {
        if (eventItem.eEventId == SPEI_END_INPUT_STREAM && eventItem.ulStreamNum == gSapiStreamNum) {
            gTtsActive = false;
            gSapiStreamNum = 0;
        }

        if (eventItem.eEventId == SPEI_WORD_BOUNDARY && eventItem.ulStreamNum == gSapiStreamNum) {
            // lParam is the character position of the word in the spoken text
            gSapiLastWordPos = (ULONG)eventItem.lParam;
        }

        SapiClearEvent(&eventItem);

        eventItem = {};
        fetched = 0;
    }

    eventSource->Release();
}

static bool SapiSpeak(WStr textW) {
    if (!SapiInit()) {
        return false;
    }

    ULONG streamNum = 0;
    HRESULT hr = gSapiVoice->Speak(textW.s, SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, &streamNum);
    if (FAILED(hr)) {
        return false;
    }

    gSapiLastWordPos = 0;
    gSapiStreamNum = streamNum;
    return true;
}

static void SapiStop() {
    if (gSapiVoice) {
        gSapiVoice->Speak(nullptr, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
    }

    gSapiStreamNum = 0;
    gSapiLastWordPos = 0;
}

//--- Windows.Media.SpeechSynthesis implementation

namespace WMSS = ABI::Windows::Media::SpeechSynthesis;
namespace WMC = ABI::Windows::Media::Core;

using SynthAsyncOp = __FIAsyncOperation_1_Windows__CMedia__CSpeechSynthesis__CSpeechSynthesisStream;
using SynthAsyncHandler =
    __FIAsyncOperationCompletedHandler_1_Windows__CMedia__CSpeechSynthesis__CSpeechSynthesisStream;

typedef HRESULT(WINAPI* Sig_RoInitialize)(RO_INIT_TYPE initType);
typedef HRESULT(WINAPI* Sig_RoGetActivationFactory)(HSTRING activatableClassId, REFIID iid, void** factory);
typedef HRESULT(WINAPI* Sig_WindowsCreateString)(PCNZWCH sourceString, UINT32 length, HSTRING* string);
typedef HRESULT(WINAPI* Sig_WindowsDeleteString)(HSTRING string);
typedef PCWSTR(WINAPI* Sig_WindowsGetStringRawBuffer)(HSTRING string, UINT32* length);
typedef HRESULT(WINAPI* Sig_CreateStreamOverRandomAccessStream)(IUnknown* randomAccessStream, REFIID riid, void** ppv);

static Sig_RoInitialize pRoInitialize = nullptr;
static Sig_RoGetActivationFactory pRoGetActivationFactory = nullptr;
static Sig_WindowsCreateString pWindowsCreateString = nullptr;
static Sig_WindowsDeleteString pWindowsDeleteString = nullptr;
static Sig_WindowsGetStringRawBuffer pWindowsGetStringRawBuffer = nullptr;
static Sig_CreateStreamOverRandomAccessStream pCreateStreamOverRandomAccessStream = nullptr;

static WMSS::ISpeechSynthesizer* gWinSynth = nullptr;
static WMSS::IInstalledVoicesStatic* gWinVoicesStatic = nullptr;
static bool gWinCoInitialized = false;
static bool gWinInitFailed = false;

// pending synthesis operation, completion signaled via notify message
static SynthAsyncOp* gWinSynthOp = nullptr;

// playback of the synthesized WAV stream
static HWAVEOUT gWinWaveOut = nullptr;
static WAVEHDR gWinWaveHdr{};
static u8* gWinWavData = nullptr; // the whole WAV file (binary)
static DWORD gWinAvgBytesPerSec = 0;
static DWORD gWinSamplesPerSec = 0;
static LONG gWinWaveDone = 0; // set from the waveOut callback thread

// word boundary cues extracted from the synthesized stream: position in
// the spoken text (in WCHARs) and the time the word starts playing
struct WinTtsCue {
    int inputPos;
    int timeMs;
};
static Vec<WinTtsCue> gWinCues;

static Str HStringToUtf8Dup(HSTRING hs) {
    UINT32 len = 0;
    PCWSTR s = pWindowsGetStringRawBuffer(hs, &len);
    if (!s) {
        return {};
    }
    return str::Dup(ToUtf8Temp(WStr(s, (int)len)));
}

class WinTtsSynthCompletedHandler : public SynthAsyncHandler {
    LONG refCount = 1;

  public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IAgileObject || riid == __uuidof(SynthAsyncHandler)) {
            *ppv = static_cast<SynthAsyncHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&refCount); }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG res = (ULONG)InterlockedDecrement(&refCount);
        if (0 == res) {
            delete this;
        }
        return res;
    }

    // can be called on a background thread; actual handling happens
    // on the UI thread in WinTtsProcessEvents()
    STDMETHODIMP Invoke(SynthAsyncOp*, AsyncStatus) override {
        TtsPostNotifyMsg();
        return S_OK;
    }
};

static void WinTtsCancelSynth() {
    if (!gWinSynthOp) {
        return;
    }

    IAsyncInfo* info = nullptr;
    if (SUCCEEDED(gWinSynthOp->QueryInterface(IID_PPV_ARGS(&info))) && info) {
        info->Cancel();
        info->Release();
    }
    gWinSynthOp->Release();
    gWinSynthOp = nullptr;
}

static void WinTtsStopPlayback() {
    if (gWinWaveOut) {
        waveOutReset(gWinWaveOut);
        if (gWinWaveHdr.dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(gWinWaveOut, &gWinWaveHdr, sizeof(gWinWaveHdr));
        }
        waveOutClose(gWinWaveOut);
        gWinWaveOut = nullptr;
    }

    gWinWaveHdr = {};
    free(gWinWavData);
    gWinWavData = nullptr;
    gWinAvgBytesPerSec = 0;
    gWinSamplesPerSec = 0;
    InterlockedExchange(&gWinWaveDone, 0);
}

// takes effect at the next SynthesizeTextToStreamAsync() i.e. the next
// spoken chunk (needs Windows 10 1709+, no-op on older versions)
static void WinTtsApplySpeed() {
    if (!gWinSynth) {
        return;
    }
    WMSS::ISpeechSynthesizer2* synth2 = nullptr;
    if (FAILED(gWinSynth->QueryInterface(IID_PPV_ARGS(&synth2))) || !synth2) {
        return;
    }
    WMSS::ISpeechSynthesizerOptions* options = nullptr;
    if (SUCCEEDED(synth2->get_Options(&options)) && options) {
        WMSS::ISpeechSynthesizerOptions2* options2 = nullptr;
        if (SUCCEEDED(options->QueryInterface(IID_PPV_ARGS(&options2))) && options2) {
            options2->put_SpeakingRate((DOUBLE)gTtsSpeed);
            options2->Release();
        }
        options->Release();
    }
    synth2->Release();
}

static bool WinTtsInit() {
    if (gWinSynth) {
        return true;
    }
    if (gWinInitFailed) {
        return false;
    }
    gWinInitFailed = true;

    HMODULE combase = LoadLibraryW(L"combase.dll");
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (!combase || !shcore) {
        return false;
    }

    pRoInitialize = (Sig_RoInitialize)GetProcAddress(combase, "RoInitialize");
    pRoGetActivationFactory = (Sig_RoGetActivationFactory)GetProcAddress(combase, "RoGetActivationFactory");
    pWindowsCreateString = (Sig_WindowsCreateString)GetProcAddress(combase, "WindowsCreateString");
    pWindowsDeleteString = (Sig_WindowsDeleteString)GetProcAddress(combase, "WindowsDeleteString");
    pWindowsGetStringRawBuffer = (Sig_WindowsGetStringRawBuffer)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    pCreateStreamOverRandomAccessStream =
        (Sig_CreateStreamOverRandomAccessStream)GetProcAddress(shcore, "CreateStreamOverRandomAccessStream");

    if (!pRoInitialize || !pRoGetActivationFactory || !pWindowsCreateString || !pWindowsDeleteString ||
        !pWindowsGetStringRawBuffer || !pCreateStreamOverRandomAccessStream) {
        return false;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        gWinCoInitialized = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    // ok if it fails because COM is already initialized
    pRoInitialize(RO_INIT_SINGLETHREADED);

    WStr clsName(RuntimeClass_Windows_Media_SpeechSynthesis_SpeechSynthesizer);
    HSTRING cls = nullptr;
    hr = pWindowsCreateString(clsName.s, (UINT32)clsName.len, &cls);
    if (FAILED(hr)) {
        return false;
    }

    IActivationFactory* factory = nullptr;
    hr = pRoGetActivationFactory(cls, IID_PPV_ARGS(&factory));
    pWindowsDeleteString(cls);
    if (FAILED(hr) || !factory) {
        return false;
    }

    factory->QueryInterface(IID_PPV_ARGS(&gWinVoicesStatic)); // optional

    IInspectable* inspectable = nullptr;
    hr = factory->ActivateInstance(&inspectable);
    factory->Release();
    if (FAILED(hr) || !inspectable) {
        return false;
    }

    hr = inspectable->QueryInterface(IID_PPV_ARGS(&gWinSynth));
    inspectable->Release();
    if (FAILED(hr) || !gWinSynth) {
        gWinSynth = nullptr;
        if (gWinVoicesStatic) {
            gWinVoicesStatic->Release();
            gWinVoicesStatic = nullptr;
        }
        return false;
    }

    // restricted environments (e.g. Windows Sandbox) have the synthesizer
    // but no voices installed; report failure so that we fall back to
    // SAPI, which might have voices of its own
    UINT32 nVoices = 0;
    if (gWinVoicesStatic) {
        __FIVectorView_1_Windows__CMedia__CSpeechSynthesis__CVoiceInformation* allVoices = nullptr;
        if (SUCCEEDED(gWinVoicesStatic->get_AllVoices(&allVoices)) && allVoices) {
            allVoices->get_Size(&nVoices);
            allVoices->Release();
        }
    }
    if (nVoices == 0) {
        log("WinTtsInit: no voices installed\n");
        gWinSynth->Release();
        gWinSynth = nullptr;
        if (gWinVoicesStatic) {
            gWinVoicesStatic->Release();
            gWinVoicesStatic = nullptr;
        }
        return false;
    }

    // ask for word boundary metadata in the synthesized stream so that we
    // know where we are when stopping (best effort, needs Windows 10 1703+)
    WMSS::ISpeechSynthesizer2* synth2 = nullptr;
    if (SUCCEEDED(gWinSynth->QueryInterface(IID_PPV_ARGS(&synth2))) && synth2) {
        WMSS::ISpeechSynthesizerOptions* options = nullptr;
        if (SUCCEEDED(synth2->get_Options(&options)) && options) {
            options->put_IncludeWordBoundaryMetadata(true);
            options->put_IncludeSentenceBoundaryMetadata(false);
            options->Release();
        }
        synth2->Release();
    }

    WinTtsApplySpeed();

    gWinInitFailed = false;
    return true;
}

static void WinTtsRelease() {
    WinTtsCancelSynth();
    WinTtsStopPlayback();
    gWinCues.Reset();

    if (gWinVoicesStatic) {
        gWinVoicesStatic->Release();
        gWinVoicesStatic = nullptr;
    }
    if (gWinSynth) {
        gWinSynth->Release();
        gWinSynth = nullptr;
    }
    if (gWinCoInitialized) {
        CoUninitialize();
        gWinCoInitialized = false;
    }
}

static void WinTtsGetVoices(Vec<TtsVoiceInfo>& voices) {
    if (!WinTtsInit() || !gWinVoicesStatic) {
        return;
    }

    __FIVectorView_1_Windows__CMedia__CSpeechSynthesis__CVoiceInformation* allVoices = nullptr;
    HRESULT hr = gWinVoicesStatic->get_AllVoices(&allVoices);
    if (FAILED(hr) || !allVoices) {
        return;
    }

    UINT32 n = 0;
    allVoices->get_Size(&n);

    for (UINT32 i = 0; i < n; i++) {
        WMSS::IVoiceInformation* vi = nullptr;
        if (FAILED(allVoices->GetAt(i, &vi)) || !vi) {
            continue;
        }

        HSTRING id = nullptr;
        HSTRING name = nullptr;
        HSTRING lang = nullptr;
        vi->get_Id(&id);
        vi->get_DisplayName(&name);
        vi->get_Language(&lang);

        if (id && name) {
            TtsVoiceInfo info{};
            info.id = HStringToUtf8Dup(id);
            info.name = HStringToUtf8Dup(name);
            info.lang = lang ? HStringToUtf8Dup(lang) : nullptr;
            voices.Append(info);
        }

        if (id) {
            pWindowsDeleteString(id);
        }
        if (name) {
            pWindowsDeleteString(name);
        }
        if (lang) {
            pWindowsDeleteString(lang);
        }

        vi->Release();
    }

    allVoices->Release();
}

static bool WinTtsSetVoiceById(Str voiceId) {
    if (!WinTtsInit() || !gWinVoicesStatic) {
        return false;
    }

    if (len(voiceId) == 0) {
        WMSS::IVoiceInformation* def = nullptr;
        if (FAILED(gWinVoicesStatic->get_DefaultVoice(&def)) || !def) {
            return false;
        }
        HRESULT hr = gWinSynth->put_Voice(def);
        def->Release();
        return SUCCEEDED(hr);
    }

    __FIVectorView_1_Windows__CMedia__CSpeechSynthesis__CVoiceInformation* allVoices = nullptr;
    if (FAILED(gWinVoicesStatic->get_AllVoices(&allVoices)) || !allVoices) {
        return false;
    }

    bool didSet = false;
    UINT32 n = 0;
    allVoices->get_Size(&n);

    for (UINT32 i = 0; i < n && !didSet; i++) {
        WMSS::IVoiceInformation* vi = nullptr;
        if (FAILED(allVoices->GetAt(i, &vi)) || !vi) {
            continue;
        }

        HSTRING id = nullptr;
        vi->get_Id(&id);
        if (id) {
            UINT32 len = 0;
            PCWSTR s = pWindowsGetStringRawBuffer(id, &len);
            if (s && str::EqI(ToUtf8Temp(WStr(s, (int)len)), voiceId)) {
                didSet = SUCCEEDED(gWinSynth->put_Voice(vi));
            }
            pWindowsDeleteString(id);
        }
        vi->Release();
    }

    allVoices->Release();
    return didSet;
}

static bool WinTtsSpeak(WStr textW) {
    if (!WinTtsInit()) {
        return false;
    }

    WinTtsCancelSynth();
    WinTtsStopPlayback();
    gWinCues.Reset();

    HSTRING text = nullptr;
    HRESULT hr = pWindowsCreateString(textW.s, (UINT32)textW.len, &text);
    if (FAILED(hr)) {
        return false;
    }

    SynthAsyncOp* op = nullptr;
    hr = gWinSynth->SynthesizeTextToStreamAsync(text, &op);
    pWindowsDeleteString(text);
    if (FAILED(hr) || !op) {
        return false;
    }

    auto handler = new WinTtsSynthCompletedHandler();
    op->put_Completed(handler);
    handler->Release();

    gWinSynthOp = op;
    return true;
}

// extract word boundary cues: where each word starts in the spoken text
// and when it starts playing
static void WinTtsExtractCues(WMSS::ISpeechSynthesisStream* stream) {
    gWinCues.Reset();

    WMC::ITimedMetadataTrackProvider* provider = nullptr;
    if (FAILED(stream->QueryInterface(IID_PPV_ARGS(&provider))) || !provider) {
        return;
    }

    __FIVectorView_1_Windows__CMedia__CCore__CTimedMetadataTrack* tracks = nullptr;
    HRESULT hr = provider->get_TimedMetadataTracks(&tracks);
    provider->Release();
    if (FAILED(hr) || !tracks) {
        return;
    }

    UINT32 nTracks = 0;
    tracks->get_Size(&nTracks);

    for (UINT32 i = 0; i < nTracks; i++) {
        WMC::ITimedMetadataTrack* track = nullptr;
        if (FAILED(tracks->GetAt(i, &track)) || !track) {
            continue;
        }

        __FIVectorView_1_Windows__CMedia__CCore__CIMediaCue* cues = nullptr;
        if (SUCCEEDED(track->get_Cues(&cues)) && cues) {
            UINT32 nCues = 0;
            cues->get_Size(&nCues);

            for (UINT32 j = 0; j < nCues; j++) {
                WMC::IMediaCue* cue = nullptr;
                if (FAILED(cues->GetAt(j, &cue)) || !cue) {
                    continue;
                }

                WMC::ISpeechCue* speechCue = nullptr;
                if (SUCCEEDED(cue->QueryInterface(IID_PPV_ARGS(&speechCue))) && speechCue) {
                    __FIReference_1_int* posRef = nullptr;
                    speechCue->get_StartPositionInInput(&posRef);
                    if (posRef) {
                        INT32 pos = 0;
                        posRef->get_Value(&pos);
                        posRef->Release();

                        ABI::Windows::Foundation::TimeSpan ts{};
                        cue->get_StartTime(&ts);

                        WinTtsCue wc;
                        wc.inputPos = (int)pos;
                        wc.timeMs = (int)(ts.Duration / 10000);
                        gWinCues.Append(wc);
                    }
                    speechCue->Release();
                }
                cue->Release();
            }
            cues->Release();
        }
        track->Release();
    }

    tracks->Release();

    // sort by time (insertion sort, the cues are mostly sorted already)
    for (int i = 1; i < len(gWinCues); i++) {
        WinTtsCue value = gWinCues[i];
        int j = i - 1;
        while (j >= 0 && gWinCues[j].timeMs > value.timeMs) {
            gWinCues[j + 1] = gWinCues[j];
            j--;
        }
        gWinCues[j + 1] = value;
    }
}

// reads the whole synthesized WAV file into gWinWavData
static bool WinTtsReadStreamBytes(WMSS::ISpeechSynthesisStream* stream) {
    IStream* istm = nullptr;
    HRESULT hr = pCreateStreamOverRandomAccessStream((IUnknown*)stream, IID_PPV_ARGS(&istm));
    if (FAILED(hr) || !istm) {
        return false;
    }

    bool ok = false;
    Str data = ReadIStream(istm);
    constexpr int kMaxWavSize = 512 * 1024 * 1024;
    if (!str::IsNull(data) && data.len > 0 && data.len < kMaxWavSize) {
        gWinWavData = (u8*)data.s;
        gWinWaveHdr.dwBufferLength = (DWORD)data.len; // temporarily holds the file size
        ok = true;
    } else {
        str::Free(data);
    }
    istm->Release();
    return ok;
}

static DWORD WavGetU32(const u8* d) {
    DWORD res;
    memcpy(&res, d, 4);
    return res;
}

// finds "fmt " and "data" chunks in a RIFF WAVE file
static bool WinTtsParseWav(const u8* d, size_t n, WAVEFORMATEX* wfx, const u8** dataOut, DWORD* dataSizeOut) {
    if (n < 12 + 8 || !str::EqN(Str((char*)(d), 4), StrL("RIFF"), 4) ||
        !str::EqN(Str((char*)(d + 8), 4), StrL("WAVE"), 4)) {
        return false;
    }

    bool haveFmt = false;
    const u8* data = nullptr;
    DWORD dataSize = 0;

    size_t off = 12;
    while (off + 8 <= n) {
        Str chunkId = Str((char*)(d + off), 4);
        DWORD chunkSize = WavGetU32(d + off + 4);
        off += 8;
        if (chunkSize > n - off) {
            break;
        }

        if (str::EqN(chunkId, StrL("fmt "), 4) && chunkSize >= 16) {
            size_t toCopy = (size_t)chunkSize;
            if (toCopy > sizeof(WAVEFORMATEX)) {
                toCopy = sizeof(WAVEFORMATEX);
            }
            *wfx = {};
            memcpy(wfx, d + off, toCopy);
            wfx->cbSize = 0;
            haveFmt = true;
        } else if (str::EqN(chunkId, StrL("data"), 4)) {
            data = d + off;
            dataSize = chunkSize;
        }

        off += chunkSize + (chunkSize & 1); // chunks are word-aligned
    }

    if (!haveFmt || !data || dataSize == 0) {
        return false;
    }

    *dataOut = data;
    *dataSizeOut = dataSize;
    return true;
}

static void CALLBACK WinTtsWaveOutCb(HWAVEOUT, UINT msg, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    if (msg != WOM_DONE) {
        return;
    }
    InterlockedExchange(&gWinWaveDone, 1);
    TtsPostNotifyMsg();
}

static bool WinTtsStartPlayback() {
    DWORD wavSize = gWinWaveHdr.dwBufferLength;
    gWinWaveHdr = {};

    WAVEFORMATEX wfx{};
    const u8* data = nullptr;
    DWORD dataSize = 0;
    if (!WinTtsParseWav(gWinWavData, wavSize, &wfx, &data, &dataSize)) {
        logf("WinTtsStartPlayback: failed to parse WAV, size: %d\n", (int)wavSize);
        return false;
    }

    MMRESULT res = waveOutOpen(&gWinWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)WinTtsWaveOutCb, 0, CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) {
        logf("WinTtsStartPlayback: waveOutOpen() failed: %d, format tag: %d\n", (int)res, (int)wfx.wFormatTag);
        gWinWaveOut = nullptr;
        return false;
    }

    gWinAvgBytesPerSec = wfx.nAvgBytesPerSec;
    gWinSamplesPerSec = wfx.nSamplesPerSec;

    gWinWaveHdr.lpData = (LPSTR)data;
    gWinWaveHdr.dwBufferLength = dataSize;
    if (waveOutPrepareHeader(gWinWaveOut, &gWinWaveHdr, sizeof(gWinWaveHdr)) != MMSYSERR_NOERROR ||
        waveOutWrite(gWinWaveOut, &gWinWaveHdr, sizeof(gWinWaveHdr)) != MMSYSERR_NOERROR) {
        log("WinTtsStartPlayback: waveOutPrepareHeader() or waveOutWrite() failed\n");
        WinTtsStopPlayback();
        return false;
    }

    logf("WinTtsStartPlayback: playing %d bytes, %d Hz, %d word cues\n", (int)dataSize, (int)wfx.nSamplesPerSec,
         len(gWinCues));
    return true;
}

static void WinTtsProcessEvents() {
    // a pending synthesis finished: start playing the result
    if (gWinSynthOp) {
        IAsyncInfo* info = nullptr;
        if (FAILED(gWinSynthOp->QueryInterface(IID_PPV_ARGS(&info))) || !info) {
            return;
        }

        AsyncStatus status = AsyncStatus::Started;
        info->get_Status(&status);
        info->Release();

        if (status == AsyncStatus::Started) {
            return; // still synthesizing
        }

        SynthAsyncOp* op = gWinSynthOp;
        gWinSynthOp = nullptr;

        bool ok = false;
        if (status == AsyncStatus::Completed) {
            WMSS::ISpeechSynthesisStream* stream = nullptr;
            HRESULT hr = op->GetResults(&stream);
            if (SUCCEEDED(hr) && stream) {
                WinTtsExtractCues(stream);
                bool didRead = WinTtsReadStreamBytes(stream);
                if (!didRead) {
                    log("WinTtsProcessEvents: failed to read synthesized stream\n");
                }
                ok = didRead && WinTtsStartPlayback();
                stream->Release();
            } else {
                logf("WinTtsProcessEvents: GetResults() failed: 0x%x\n", (int)hr);
            }
        } else {
            logf("WinTtsProcessEvents: synthesis failed, status: %d\n", (int)status);
        }
        op->Release();

        if (!ok) {
            WinTtsStopPlayback();
            gTtsActive = false;
        }
        return;
    }

    // playback finished
    if (InterlockedCompareExchange(&gWinWaveDone, 0, 1) == 1) {
        if (gWinWaveOut) {
            WinTtsStopPlayback();
            gTtsActive = false;
        }
    }
}

// position (in WCHARs) in the spoken text of the word being played;
// -1 if playback has not started yet (still synthesizing)
static int WinTtsLastWordPosWide() {
    if (!gWinWaveOut) {
        return -1;
    }
    if (len(gWinCues) == 0) {
        return 0;
    }

    MMTIME mmt{};
    mmt.wType = TIME_MS;
    if (waveOutGetPosition(gWinWaveOut, &mmt, sizeof(mmt)) != MMSYSERR_NOERROR) {
        return 0;
    }

    DWORD ms;
    if (mmt.wType == TIME_MS) {
        ms = mmt.u.ms;
    } else if (mmt.wType == TIME_BYTES && gWinAvgBytesPerSec) {
        ms = (DWORD)((u64)mmt.u.cb * 1000 / gWinAvgBytesPerSec);
    } else if (mmt.wType == TIME_SAMPLES && gWinSamplesPerSec) {
        ms = (DWORD)((u64)mmt.u.sample * 1000 / gWinSamplesPerSec);
    } else {
        return 0;
    }

    int pos = 0;
    for (WinTtsCue& cue : gWinCues) {
        if (cue.timeMs > (int)ms) {
            break;
        }
        pos = cue.inputPos;
    }
    return pos;
}

static void WinTtsStop() {
    WinTtsCancelSynth();
    WinTtsStopPlayback();
}

//--- public interface, dispatches to one of the implementations

static bool IsWinRtBackend() {
    if (gTtsBackend == TtsBackend::Unknown) {
        // an escape hatch, also for testing the SAPI implementation
        bool forceSapi = len(GetEnvVariableTemp(StrL("SUMATRA_TTS_FORCE_SAPI"))) > 0;
        if (!forceSapi && WinTtsInit()) {
            gTtsBackend = TtsBackend::WinRt;
            log("Tts: using Windows.Media.SpeechSynthesis\n");
        } else {
            gTtsBackend = TtsBackend::Sapi;
            log("Tts: using SAPI\n");
        }
    }
    return gTtsBackend == TtsBackend::WinRt;
}

void TtsSetNotifyWindow(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    gTtsNotifyHwnd = hwnd;
    gTtsNotifyMsg = msg;
    gTtsNotifyWParam = wp;
    gTtsNotifyLParam = lp;

    SapiSetNotify();
}

void TtsProcessEvents() {
    if (gTtsBackend == TtsBackend::WinRt) {
        WinTtsProcessEvents();
    } else {
        SapiProcessEvents();
    }
}

bool TtsSpeakUtf8(Str text) {
    if (len(text) == 0) {
        return false;
    }

    TempWStr textW = ToWStrTemp(text);
    if (!textW) {
        return false;
    }

    bool ok;
    if (IsWinRtBackend()) {
        ok = WinTtsSpeak(textW);
    } else {
        ok = SapiSpeak(textW);
    }
    if (!ok) {
        return false;
    }

    wstr::Free(gTtsSpokenText);
    gTtsSpokenText = wstr::Dup(textW);
    gTtsActive = true;
    return true;
}

bool TtsIsSpeaking() {
    return gTtsActive;
}

int TtsGetSpokenPosUtf8() {
    int wpos;
    if (gTtsBackend == TtsBackend::WinRt) {
        wpos = WinTtsLastWordPosWide();
    } else {
        wpos = (int)gSapiLastWordPos;
    }

    if (!gTtsSpokenText.s || wpos < 0) {
        return -1;
    }
    if (wpos == 0) {
        if (!gTtsActive) {
            return -1;
        }
        return 0;
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, gTtsSpokenText.s, wpos, nullptr, 0, nullptr, nullptr);
    return n > 0 ? n : -1;
}

void TtsStop() {
    if (gTtsBackend == TtsBackend::WinRt) {
        WinTtsStop();
    } else {
        SapiStop();
    }
    gTtsActive = false;
}

Vec<TtsVoiceInfo> TtsGetVoices() {
    Vec<TtsVoiceInfo> voices;
    if (IsWinRtBackend()) {
        WinTtsGetVoices(voices);
    } else {
        SapiGetVoices(voices);
    }
    TtsSortVoicesByLanguage(voices);
    return voices;
}

bool TtsSetVoiceById(Str voiceId) {
    bool ok;
    if (IsWinRtBackend()) {
        ok = WinTtsSetVoiceById(voiceId);
    } else {
        ok = SapiSetVoiceById(voiceId);
    }
    if (!ok) {
        return false;
    }

    str::ReplacePtr(&gTtsVoiceId, voiceId ? str::Dup(voiceId) : Str{});
    return true;
}

Str TtsGetVoiceId() {
    return gTtsVoiceId;
}

// with the WinRT backend the new speed applies from the next spoken chunk;
// SAPI adjusts speech in progress
void TtsSetSpeed(float speed) {
    if (speed < kTtsSpeedMin) {
        speed = kTtsSpeedMin;
    } else if (speed > kTtsSpeedMax) {
        speed = kTtsSpeedMax;
    }
    gTtsSpeed = speed;

    // both no-op if that backend is not initialized
    WinTtsApplySpeed();
    SapiApplySpeed();
}

float TtsGetSpeed() {
    return gTtsSpeed;
}

void TtsFreeVoices(Vec<TtsVoiceInfo>& voices) {
    for (TtsVoiceInfo& voice : voices) {
        str::Free(voice.id);
        str::Free(voice.name);
        str::Free(voice.lang);
    }
    voices.Reset();
}

void TtsRelease() {
    WinTtsRelease();
    SapiRelease();

    gTtsActive = false;
    gTtsBackend = TtsBackend::Unknown;
    wstr::FreePtr(&gTtsSpokenText);

    str::Free(gTtsVoiceId);
    gTtsVoiceId = {};
}

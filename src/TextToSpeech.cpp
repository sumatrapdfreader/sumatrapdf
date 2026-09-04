#include "base/Base.h"
#include "base/Win.h"

#include <roapi.h>
#include <windows.media.speechsynthesis.h>
// must come after the windows.media headers: both define SpeechRecognizerState
// and this order compiles in both msvc and clang
#include <sapi.h>

#include <TextToSpeech.h>
#include "SumatraLog.h"

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
static bool gTtsQueuedStarted = false;

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
    int langCmp = str::CmpI(TtsVoiceLangForSort(a), TtsVoiceLangForSort(b));
    if (langCmp != 0) {
        return langCmp < 0;
    }

    return str::CmpI(a.name ? a.name : StrL(""), b.name ? b.name : StrL("")) < 0;
}

static bool TtsForceSapi() {
    return len(GetEnvVariableTemp(StrL("SUMATRA_TTS_FORCE_SAPI"))) > 0;
}

static bool TtsVoiceIdInList(const Vec<TtsVoiceInfo>& voices, Str id) {
    if (len(id) == 0) {
        return false;
    }
    for (const TtsVoiceInfo& v : voices) {
        if (v.id && str::EqI(v.id, id)) {
            return true;
        }
    }
    return false;
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
static ULONG gSapiQueuedStreamNum = 0;
static WStr gSapiQueuedText;

static void SapiClearQueued() {
    gSapiQueuedStreamNum = 0;
    wstr::Free(gSapiQueuedText);
    gSapiQueuedText = {};
}

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

    // equivalent to SPFEI(END_INPUT_STREAM)|SPFEI(WORD_BOUNDARY); written this way
    // so FLAGCHECK is only or'd once (avoids misc-redundant-expression on SPFEI|SPFEI)
    const ULONGLONG events = (1ull << SPEI_END_INPUT_STREAM) | (1ull << SPEI_WORD_BOUNDARY) | SPFEI_FLAGCHECK;
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

    SapiClearQueued();
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
            VecAppend(voices, info);
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
            dbgtts("sapi-end stream=%u\n", (u32)eventItem.ulStreamNum);
            if (gSapiQueuedStreamNum) {
                gSapiStreamNum = gSapiQueuedStreamNum;
                gSapiQueuedStreamNum = 0;
                wstr::Free(gTtsSpokenText);
                gTtsSpokenText = gSapiQueuedText;
                gSapiQueuedText = {};
                gSapiLastWordPos = 0;
                gTtsQueuedStarted = true;
                gTtsActive = true;
                dbgtts("sapi-queued-start stream=%u\n", (u32)gSapiStreamNum);
            } else {
                gTtsActive = false;
                gSapiStreamNum = 0;
            }
        }

        if (eventItem.eEventId == SPEI_WORD_BOUNDARY && eventItem.ulStreamNum == gSapiStreamNum) {
            // lParam is the character position of the word in the spoken text
            gSapiLastWordPos = (ULONG)eventItem.lParam;
            dbgtts("sapi-word pos=%u\n", (u32)gSapiLastWordPos);
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

    SapiClearQueued();
    ULONG streamNum = 0;
    HRESULT hr = gSapiVoice->Speak(textW.s, SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, &streamNum);
    if (FAILED(hr)) {
        dbgtts("sapi-speak failed hr=0x%x chars=%d\n", (int)hr, textW.len);
        return false;
    }

    gSapiLastWordPos = 0;
    gSapiStreamNum = streamNum;
    dbgtts("sapi-speak stream=%u chars=%d\n", (u32)streamNum, textW.len);
    return true;
}

static bool SapiQueue(WStr textW) {
    if (!SapiInit() || !gSapiVoice || gSapiQueuedStreamNum) {
        return false;
    }

    ULONG streamNum = 0;
    HRESULT hr = gSapiVoice->Speak(textW.s, SPF_ASYNC | SPF_IS_NOT_XML, &streamNum);
    if (FAILED(hr)) {
        dbgtts("sapi-queue failed hr=0x%x chars=%d\n", (int)hr, textW.len);
        return false;
    }

    gSapiQueuedStreamNum = streamNum;
    wstr::Free(gSapiQueuedText);
    gSapiQueuedText = wstr::Dup(textW);
    dbgtts("sapi-queue stream=%u chars=%d\n", (u32)streamNum, textW.len);
    return true;
}

static void SapiStop() {
    if (gSapiVoice) {
        gSapiVoice->Speak(nullptr, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
    }

    SapiClearQueued();
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
static LARGE_INTEGER gWinPlayQpcStart{};
static DWORD gWinPlayDurationMs = 0;

// word boundary cues extracted from the synthesized stream: position in
// the spoken text (in WCHARs) and the time the word starts playing
struct WinTtsCue {
    int inputPos;
    int timeMs;
};
static Vec<WinTtsCue> gWinCues;

static bool gWinSynthIsQueue = false;
static u8* gWinQueuedWav = nullptr;
static DWORD gWinQueuedWavSize = 0;
static Vec<WinTtsCue> gWinQueuedCues;
static WStr gWinQueuedText;

static void WinTtsClearQueued() {
    free(gWinQueuedWav);
    gWinQueuedWav = nullptr;
    gWinQueuedWavSize = 0;
    VecReset(gWinQueuedCues);
    wstr::Free(gWinQueuedText);
    gWinQueuedText = {};
    gWinSynthIsQueue = false;
}

static Str HStringToUtf8Dup(HSTRING hs) {
    UINT32 len = 0;
    PCWSTR s = pWindowsGetStringRawBuffer(hs, &len);
    if (!s) {
        return {};
    }
    return str::Dup(ToUtf8Temp(WStr(s, (int)len)));
}

class WinTtsSynthCompletedHandler : public SynthAsyncHandler {
    AtomicInt refCount = 1;

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

    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)AtomicIntInc(&refCount); }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG res = (ULONG)InterlockedDecrement(&refCount);
        if (0 == res) {
            delete this;
        }
        return res;
    }

    // can be called on a background thread; actual handling happens
    // on the UI thread in WinTtsProcessEvents()
    STDMETHODIMP Invoke(SynthAsyncOp* /*asyncInfo*/, AsyncStatus /*status*/) override {
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
    gWinPlayQpcStart = {};
    gWinPlayDurationMs = 0;
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
        logf("tts: WinTtsInit: no voices installed\n");
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
    WinTtsClearQueued();
    WinTtsStopPlayback();
    VecReset(gWinCues);

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
            info.lang = lang ? HStringToUtf8Dup(lang) : Str();
            VecAppend(voices, info);
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

static bool WinTtsStartSynth(WStr textW) {
    HSTRING text = nullptr;
    HRESULT hr = pWindowsCreateString(textW.s, (UINT32)textW.len, &text);
    if (FAILED(hr)) {
        return false;
    }

    SynthAsyncOp* op = nullptr;
    hr = gWinSynth->SynthesizeTextToStreamAsync(text, &op);
    pWindowsDeleteString(text);
    if (FAILED(hr) || !op) {
        dbgtts("winrt-synth failed hr=0x%x chars=%d\n", (int)hr, textW.len);
        return false;
    }

    auto* handler = new WinTtsSynthCompletedHandler();
    op->put_Completed(handler);
    handler->Release();

    gWinSynthOp = op;
    return true;
}

static bool WinTtsSpeak(WStr textW) {
    if (!WinTtsInit()) {
        return false;
    }

    WinTtsCancelSynth();
    WinTtsClearQueued();
    WinTtsStopPlayback();
    VecReset(gWinCues);
    gWinSynthIsQueue = false;

    if (!WinTtsStartSynth(textW)) {
        dbgtts("winrt-speak failed chars=%d\n", textW.len);
        return false;
    }
    dbgtts("winrt-speak start chars=%d\n", textW.len);
    return true;
}

static bool WinTtsQueue(WStr textW) {
    if (!WinTtsInit() || gWinSynthOp || gWinQueuedWav) {
        return false;
    }
    if (!WinTtsStartSynth(textW)) {
        return false;
    }
    gWinSynthIsQueue = true;
    wstr::Free(gWinQueuedText);
    gWinQueuedText = wstr::Dup(textW);
    dbgtts("winrt-queue start chars=%d\n", textW.len);
    return true;
}

// extract word boundary cues: where each word starts in the spoken text
// and when it starts playing
static void WinTtsExtractCues(WMSS::ISpeechSynthesisStream* stream, Vec<WinTtsCue>& dest) {
    VecReset(dest);

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
                        VecAppend(dest, wc);
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
    for (int i = 1; i < len(dest); i++) {
        WinTtsCue value = dest[i];
        int j = i - 1;
        while (j >= 0 && dest[j].timeMs > value.timeMs) {
            dest[j + 1] = dest[j];
            j--;
        }
        dest[j + 1] = value;
    }
}

// reads the whole synthesized WAV file into gWinWavData
static bool WinTtsReadStreamBytes(WMSS::ISpeechSynthesisStream* stream, u8** dataOut, DWORD* sizeOut) {
    IStream* istm = nullptr;
    HRESULT hr = pCreateStreamOverRandomAccessStream((IUnknown*)stream, IID_PPV_ARGS(&istm));
    if (FAILED(hr) || !istm) {
        return false;
    }

    bool ok = false;
    Str data = ReadIStream(istm);
    constexpr int kMaxWavSize = 512 * 1024 * 1024;
    if (!str::IsNull(data) && data.len > 0 && data.len < kMaxWavSize) {
        *dataOut = (u8*)data.s;
        *sizeOut = (DWORD)data.len;
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
    if (n < 12 + 8 || !str::EqN(Str((char*)d, 4), StrL("RIFF"), 4) ||
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
            toCopy = std::min(toCopy, sizeof(WAVEFORMATEX));
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

static void CALLBACK WinTtsWaveOutCb(HWAVEOUT /*hwo*/, UINT msg, DWORD_PTR /*instance*/, DWORD_PTR /*param1*/,
                                     DWORD_PTR /*param2*/) {
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
        logf("tts: WinTtsStartPlayback: failed to parse WAV, size: %d\n", (int)wavSize);
        return false;
    }

    MMRESULT res = waveOutOpen(&gWinWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)WinTtsWaveOutCb, 0, CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) {
        logf("tts: WinTtsStartPlayback: waveOutOpen() failed: %d, format tag: %d\n", (int)res, (int)wfx.wFormatTag);
        gWinWaveOut = nullptr;
        return false;
    }

    gWinAvgBytesPerSec = wfx.nAvgBytesPerSec;
    gWinSamplesPerSec = wfx.nSamplesPerSec;

    gWinWaveHdr.lpData = (LPSTR)data;
    gWinWaveHdr.dwBufferLength = dataSize;
    if (waveOutPrepareHeader(gWinWaveOut, &gWinWaveHdr, sizeof(gWinWaveHdr)) != MMSYSERR_NOERROR ||
        waveOutWrite(gWinWaveOut, &gWinWaveHdr, sizeof(gWinWaveHdr)) != MMSYSERR_NOERROR) {
        logf("tts: WinTtsStartPlayback: waveOutPrepareHeader() or waveOutWrite() failed\n");
        WinTtsStopPlayback();
        return false;
    }
    QueryPerformanceCounter(&gWinPlayQpcStart);
    gWinPlayDurationMs = wfx.nAvgBytesPerSec ? (DWORD)((u64)dataSize * 1000 / wfx.nAvgBytesPerSec) : 0;

    dbgtts("winrt-play bytes=%d hz=%d cues=%d tag=%d avgBps=%u\n", (int)dataSize, (int)wfx.nSamplesPerSec,
           len(gWinCues), (int)wfx.wFormatTag, (u32)wfx.nAvgBytesPerSec);
    return true;
}

static bool WinTtsPlayQueued() {
    if (!gWinQueuedWav) {
        return false;
    }

    WinTtsStopPlayback();
    gWinWavData = gWinQueuedWav;
    gWinQueuedWav = nullptr;
    gWinWaveHdr.dwBufferLength = gWinQueuedWavSize;
    gWinQueuedWavSize = 0;
    gWinCues = gWinQueuedCues;
    VecReset(gWinQueuedCues);
    wstr::Free(gTtsSpokenText);
    gTtsSpokenText = gWinQueuedText;
    gWinQueuedText = {};
    gWinSynthIsQueue = false;

    if (!WinTtsStartPlayback()) {
        return false;
    }
    gTtsQueuedStarted = true;
    gTtsActive = true;
    dbgtts("winrt-queued-start cues=%d\n", len(gWinCues));
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
        dbgtts("winrt-synth done status=%d queue=%d\n", (int)status, (int)gWinSynthIsQueue);

        SynthAsyncOp* op = gWinSynthOp;
        gWinSynthOp = nullptr;
        bool isQueue = gWinSynthIsQueue;
        gWinSynthIsQueue = false;

        bool ok = false;
        if (status == AsyncStatus::Completed) {
            WMSS::ISpeechSynthesisStream* stream = nullptr;
            HRESULT hr = op->GetResults(&stream);
            if (SUCCEEDED(hr) && stream) {
                if (isQueue) {
                    WinTtsExtractCues(stream, gWinQueuedCues);
                    DWORD sz = 0;
                    u8* data = nullptr;
                    bool didRead = WinTtsReadStreamBytes(stream, &data, &sz);
                    if (didRead) {
                        gWinQueuedWav = data;
                        gWinQueuedWavSize = sz;
                        if (gWinWaveOut) {
                            ok = true;
                            dbgtts("winrt-queue ready bytes=%u\n", (u32)sz);
                        } else {
                            ok = WinTtsPlayQueued();
                        }
                    } else {
                        logf("tts: WinTtsProcessEvents: failed to read queued stream\n");
                    }
                } else {
                    WinTtsExtractCues(stream, gWinCues);
                    DWORD sz = 0;
                    bool didRead = WinTtsReadStreamBytes(stream, &gWinWavData, &sz);
                    if (didRead) {
                        gWinWaveHdr.dwBufferLength = sz;
                        ok = WinTtsStartPlayback();
                    } else {
                        logf("tts: WinTtsProcessEvents: failed to read synthesized stream\n");
                    }
                }
                stream->Release();
            } else {
                logf("tts: WinTtsProcessEvents: GetResults() failed: 0x%x\n", (int)hr);
            }
        } else {
            logf("tts: WinTtsProcessEvents: synthesis failed, status: %d\n", (int)status);
        }
        op->Release();

        if (!ok) {
            dbgtts("winrt-synth play failed queue=%d\n", (int)isQueue);
            if (isQueue && gWinWaveOut) {
                WinTtsClearQueued();
            } else {
                WinTtsStopPlayback();
                WinTtsClearQueued();
                gTtsActive = false;
            }
        }
        return;
    }

    // playback finished
    if (InterlockedCompareExchange(&gWinWaveDone, 0, 1) == 1) {
        dbgtts("winrt-play done queued=%d synth=%d\n", gWinQueuedWav ? 1 : 0, gWinSynthOp ? 1 : 0);
        if (gWinQueuedWav) {
            if (!WinTtsPlayQueued()) {
                WinTtsStopPlayback();
                gTtsActive = false;
            }
        } else if (gWinSynthOp && gWinSynthIsQueue) {
            WinTtsStopPlayback();
            gTtsActive = true;
        } else if (gWinWaveOut) {
            WinTtsStopPlayback();
            gTtsActive = false;
        }
    }
}

// position (in WCHARs) in the spoken text of the word being played;
// -1 if playback has not started yet (still synthesizing)
static DWORD WinTtsClockMs() {
    if (gWinPlayQpcStart.QuadPart == 0) {
        return 0;
    }
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    if (freq.QuadPart <= 0) {
        return 0;
    }
    u64 ms = (u64)(now.QuadPart - gWinPlayQpcStart.QuadPart) * 1000 / (u64)freq.QuadPart;
    if (gWinPlayDurationMs && ms > gWinPlayDurationMs) {
        ms = gWinPlayDurationMs;
    }
    return (DWORD)ms;
}

static int WinTtsLastWordPosWide() {
    if (!gWinWaveOut) {
        return -1;
    }
    if (len(gWinCues) == 0) {
        return 0;
    }

    // WAVE_MAPPER often reports TIME_BYTES/TIME_MS as 0; the QPC clock does not.
    DWORD ms = WinTtsClockMs();

    int pos = 0;
    for (WinTtsCue& cue : gWinCues) {
        if (cue.timeMs > (int)ms) {
            break;
        }
        pos = cue.inputPos;
    }
    DBG_TTS({
        static DWORD sLastMs = 0xFFFFFFFFu;
        if (ms / 250 != sLastMs / 250) {
            sLastMs = ms;
            dbgtts("play-ms=%u pos=%d cues=%d dur=%u\n", (u32)ms, pos, len(gWinCues), gWinPlayDurationMs);
        }
    });
    return pos;
}

static void WinTtsStop() {
    WinTtsCancelSynth();
    WinTtsClearQueued();
    WinTtsStopPlayback();
}

//--- public interface, dispatches to one of the implementations

static bool IsWinRtBackend() {
    if (gTtsBackend == TtsBackend::Unknown) {
        if (!TtsForceSapi() && WinTtsInit()) {
            gTtsBackend = TtsBackend::WinRt;
            dbgtts("backend=winrt\n");
        } else {
            gTtsBackend = TtsBackend::Sapi;
            dbgtts("backend=sapi forceSapi=%d\n", (int)TtsForceSapi());
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
    if (len(textW) == 0) {
        return false;
    }

    gTtsQueuedStarted = false;

    bool ok;
    if (IsWinRtBackend()) {
        ok = WinTtsSpeak(textW);
    } else {
        ok = SapiSpeak(textW);
    }
    if (!ok) {
        dbgtts("speak failed utf8=%d\n", text.len);
        return false;
    }

    wstr::Free(gTtsSpokenText);
    gTtsSpokenText = wstr::Dup(textW);
    gTtsActive = true;
    dbgtts("speak ok utf8=%d active=1\n", text.len);
    return true;
}

// Next utterance: synthesize (WinRT) or SAPI-queue without stopping playback.
bool TtsQueueUtf8(Str text) {
    if (len(text) == 0) {
        return false;
    }

    TempWStr textW = ToWStrTemp(text);
    if (len(textW) == 0) {
        return false;
    }

    bool ok;
    if (IsWinRtBackend()) {
        ok = WinTtsQueue(textW);
    } else {
        ok = SapiQueue(textW);
    }
    dbgtts("queue %s utf8=%d\n", ok ? StrL("ok") : StrL("fail"), text.len);
    return ok;
}

// One-shot: the queued utterance just became the playing one.
bool TtsDidStartQueued() {
    bool v = gTtsQueuedStarted;
    gTtsQueuedStarted = false;
    return v;
}

bool TtsIsSpeaking() {
    return gTtsActive;
}

// utf8 offset of the most recently spoken word within the text passed
// to TtsSpeakUtf8, -1 if not known
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
    dbgtts("stop backend=%d active=%d\n", (int)gTtsBackend, (int)gTtsActive);
    if (gTtsBackend == TtsBackend::WinRt) {
        WinTtsStop();
    } else {
        SapiStop();
    }
    gTtsActive = false;
    gTtsQueuedStarted = false;
}

// WinRT OneCore plus SAPI (NaturalVoiceSAPIAdapter etc.). Same token id once.
Vec<TtsVoiceInfo> TtsGetVoices() {
    Vec<TtsVoiceInfo> voices;
    if (!TtsForceSapi()) {
        WinTtsGetVoices(voices);
    }

    Vec<TtsVoiceInfo> sapi;
    SapiGetVoices(sapi);
    for (TtsVoiceInfo& v : sapi) {
        if (TtsVoiceIdInList(voices, v.id)) {
            str::Free(v.id);
            str::Free(v.name);
            str::Free(v.lang);
            continue;
        }
        VecAppend(voices, v);
    }
    VecReset(sapi);

    TtsSortVoicesByLanguage(voices);
    dbgtts("voices winrt+sapi=%d forceSapi=%d\n", len(voices), (int)TtsForceSapi());
    return voices;
}

// Use WinRT if it knows the id, else SAPI. Empty id is the system default.
bool TtsSetVoiceById(Str voiceId) {
    TtsBackend prev = gTtsBackend;
    TtsBackend next = TtsBackend::Unknown;
    bool ok = false;

    if (!TtsForceSapi() && WinTtsInit() && WinTtsSetVoiceById(voiceId)) {
        next = TtsBackend::WinRt;
        ok = true;
    } else if (SapiSetVoiceById(voiceId)) {
        next = TtsBackend::Sapi;
        ok = true;
    }
    if (!ok) {
        return false;
    }

    if (prev != TtsBackend::Unknown && prev != next) {
        if (prev == TtsBackend::WinRt) {
            WinTtsStop();
        } else {
            SapiStop();
        }
    }

    gTtsBackend = next;
    str::ReplacePtr(&gTtsVoiceId, voiceId ? str::Dup(voiceId) : Str{});
    dbgtts("set-voice backend=%d\n", (int)next);
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
    VecReset(voices);
}

void TtsRelease() {
    WinTtsRelease();
    SapiRelease();

    gTtsActive = false;
    gTtsQueuedStarted = false;
    gTtsBackend = TtsBackend::Unknown;
    wstr::FreePtr(&gTtsSpokenText);

    str::Free(gTtsVoiceId);
    gTtsVoiceId = {};
}

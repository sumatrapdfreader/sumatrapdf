/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#if !COMPILER_MINGW
#include <roapi.h>
#include <windows.media.speechsynthesis.h>
// must come after the windows.media headers: both define SpeechRecognizerState
// and this order compiles in both msvc and clang
#include <sapi.h>

#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "winmm.lib")
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Translations.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "Notifications.h"
#include "Menu.h"
#include "Toolbar.h"
#include "Theme.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "Selection.h"
#include "SumatraPDF.h"
#include "ReadAloud.h"
#include "SumatraLog.h"

// Read-aloud, top to bottom:
//   1. text-to-speech backend (WinRT speech synthesis, SAPI 5 fallback)
//   2. mapping the spoken text back to page positions, for highlighting
//   3. the playback bar shown over the canvas
//   4. the session: what to read, chunking it, the menus that start it

// ---------------- 1. text-to-speech backend ----------------

#if COMPILER_MINGW

// WinRT speech synthesis headers aren't available for the mingw cross-compile
bool TtsSpeakUtf8(Str) {
    return false;
}
bool TtsQueueUtf8(Str) {
    return false;
}
bool TtsDidStartQueued() {
    return false;
}
void TtsStop() {}
void TtsRelease() {}
bool TtsIsSpeaking() {
    return false;
}
int TtsGetSpokenPosUtf8() {
    return -1;
}
void TtsSetNotifyWindow(HWND, UINT, WPARAM, LPARAM) {}
void TtsProcessEvents() {}
Vec<TtsVoiceInfo> TtsGetVoices() {
    return Vec<TtsVoiceInfo>();
}
void TtsFreeVoices(Vec<TtsVoiceInfo>&) {}
bool TtsSetVoiceById(Str) {
    return false;
}
Str TtsGetVoiceId() {
    return Str();
}
void TtsSetSpeed(float) {}
float TtsGetSpeed() {
    return 1.0f;
}

#else

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

#endif // COMPILER_MINGW

// ---------------- 2. spoken text -> page positions (highlight) ----------------

struct ReadAloudRawByte {
    char c = 0;
    ReadAloudByteLoc loc{};
};

static bool IsReadAloudLowerAscii(char c) {
    return c >= 'a' && c <= 'z';
}

static bool IsReadAloudLineBreak(char c) {
    return c == '\r' || c == '\n';
}

static bool IsReadAloudHorizontalSpace(char c) {
    return c == ' ' || c == '\t';
}

static bool ReadAloudHighlightGrow(ReadAloudHighlightMap* map) {
    if (map->len + 1 < map->cap) {
        return true;
    }
    int newCap = map->cap == 0 ? 256 : map->cap * 2;
    ReadAloudByteLoc* newLocs = (ReadAloudByteLoc*)realloc(map->locs, sizeof(ReadAloudByteLoc) * (size_t)newCap);
    if (!newLocs) {
        return false;
    }
    map->locs = newLocs;
    map->cap = newCap;
    return true;
}

static bool ReadAloudHighlightAppend(ReadAloudHighlightMap* map, const ReadAloudByteLoc& loc) {
    if (!ReadAloudHighlightGrow(map)) {
        return false;
    }
    map->locs[map->len] = loc;
    map->len++;
    return true;
}

static bool ReadAloudHighlightAppendRaw(Vec<ReadAloudRawByte>& raw, char c, const ReadAloudByteLoc& loc) {
    ReadAloudRawByte rb;
    rb.c = c;
    rb.loc = loc;
    VecAppend(raw, rb);
    return true;
}

static void ReadAloudByteLocSetFromRect(ReadAloudByteLoc& loc, int pageNo, const Rect& r) {
    loc.pageNo = pageNo;
    loc.x = r.x;
    loc.y = r.y;
    loc.dx = r.dx;
    loc.dy = r.dy;
}

static bool ReadAloudByteLocHasRect(const ReadAloudByteLoc& loc) {
    return loc.pageNo > 0 && (loc.x || loc.dx);
}

static Rect ReadAloudByteLocToRect(const ReadAloudByteLoc& loc) {
    return {loc.x, loc.y, loc.dx, loc.dy};
}

static bool IsLineBreakGlyph(const Rect* coords, int idx, int c) {
    return c == '\n' && !coords[idx].x && !coords[idx].dx;
}

static bool CleanRawBytes(Vec<ReadAloudRawByte>& raw, ReadAloudHighlightMap* map, str::Builder& cleanedOut) {
    if (!map) {
        logf("tts: CleanRawBytes: null map\n");
        return false;
    }

    cleanedOut.Reset();
    map->len = 0;

    bool lastWasSpace = false;
    for (int i = 0; i < len(raw);) {
        char c = raw[i].c;
        ReadAloudByteLoc loc = raw[i].loc;

        if (c == '-' && i + 1 < len(raw) && IsReadAloudLineBreak(raw[i + 1].c)) {
            int after = i + 1;
            while (after < len(raw) && IsReadAloudLineBreak(raw[after].c)) {
                after++;
            }
            while (after < len(raw) && IsReadAloudHorizontalSpace(raw[after].c)) {
                after++;
            }

            bool prevIsLower = i > 0 && IsReadAloudLowerAscii(raw[i - 1].c);
            bool nextIsLower = after < len(raw) && IsReadAloudLowerAscii(raw[after].c);
            if (prevIsLower && nextIsLower) {
                i = after;
                lastWasSpace = false;
                continue;
            }
        }

        if (IsReadAloudLineBreak(c)) {
            int lineBreaks = 0;
            while (i < len(raw) && IsReadAloudLineBreak(raw[i].c)) {
                if (raw[i].c == '\n') {
                    lineBreaks++;
                }
                i++;
            }
            while (i < len(raw) && IsReadAloudHorizontalSpace(raw[i].c)) {
                i++;
            }

            if (!lastWasSpace && map->len > 0) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("tts: CleanRawBytes: failed appending line-break space\n");
                    return false;
                }
                lastWasSpace = true;
            }
            if (lineBreaks >= 2) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("tts: CleanRawBytes: failed appending paragraph space\n");
                    return false;
                }
            }
            continue;
        }

        if (IsReadAloudHorizontalSpace(c)) {
            if (!lastWasSpace && map->len > 0) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("tts: CleanRawBytes: failed appending horizontal space\n");
                    return false;
                }
                lastWasSpace = true;
            }
            i++;
            continue;
        }

        if (!ReadAloudHighlightAppend(map, loc) || !cleanedOut.AppendChar(c)) {
            logf("tts: CleanRawBytes: failed appending char 0x%02x\n", (unsigned char)c);
            return false;
        }
        lastWasSpace = false;
        i++;
    }

    return true;
}

void ReadAloudHighlightFree(ReadAloudHighlightMap* map) {
    if (!map) {
        return;
    }
    free(map->locs);
    map->locs = nullptr;
    map->len = 0;
    map->cap = 0;
}

bool ReadAloudHighlightBuildFromPage(EngineBase* engine, int pageNo, ReadAloudHighlightMap* map,
                                     str::Builder& cleanedOut) {
    if (!engine || !map) {
        return false;
    }

    PageText pageText = engine->ExtractPageText(pageNo);
    if (len(pageText.text) == 0 || pageText.nCodepoints <= 0) {
        FreePageText(&pageText);
        return false;
    }

    Vec<ReadAloudRawByte> raw;
    int byteIdx = 0;
    for (int i = 0; i < pageText.nCodepoints; i++) {
        ReadAloudByteLoc loc;
        Rect r = pageText.coords[i];
        if (r.x || r.dx) {
            ReadAloudByteLocSetFromRect(loc, pageNo, r);
        }
        int n = 0;
        Utf8CodepointAtByte(pageText.text, byteIdx, &n);
        for (int j = 0; j < n; j++) {
            ReadAloudHighlightAppendRaw(raw, pageText.text.s[byteIdx + j], loc);
        }
        byteIdx += n;
    }
    FreePageText(&pageText);

    return CleanRawBytes(raw, map, cleanedOut);
}

static void ReadAloudAppendPageGlyphs(Vec<ReadAloudRawByte>& raw, EngineBase* engine, int pageNo, int startGlyph,
                                      int endGlyph) {
    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (len(text) == 0) {
        dbgtts("AppendPageGlyphs: page %d has no text (textLen=%d)\n", pageNo, textLen);
        return;
    }

    startGlyph = std::max(startGlyph, 0);
    if (endGlyph < 0 || endGlyph > textLen) {
        endGlyph = textLen;
    }

    ReadAloudByteLoc noLoc;
    int byteIdx = Utf8CodepointToByteIndex(text, startGlyph);
    for (int g = startGlyph; g < endGlyph; g++) {
        int charStart = byteIdx;
        int c = Utf8CodepointNext(text, byteIdx);
        if (IsLineBreakGlyph(coords, g, c)) {
            ReadAloudHighlightAppendRaw(raw, '\r', noLoc);
            ReadAloudHighlightAppendRaw(raw, '\n', noLoc);
            continue;
        }

        ReadAloudByteLoc loc;
        Rect r = coords[g];
        if (r.x || r.dx) {
            ReadAloudByteLocSetFromRect(loc, pageNo, r);
        }

        Str utf8(text.s + charStart, byteIdx - charStart);
        if (len(utf8) == 0) {
            continue;
        }
        for (int i = 0; i < utf8.len; i++) {
            ReadAloudHighlightAppendRaw(raw, utf8.s[i], loc);
        }
    }
}

bool ReadAloudHighlightBuildFromTextSelection(TextSelection* ts, ReadAloudHighlightMap* map, str::Builder& cleanedOut) {
    if (!ts || !ts->engine || !map) {
        return false;
    }

    int fromPage = 0, fromGlyph = 0, toPage = 0, toGlyph = 0;
    ts->GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);

    Vec<ReadAloudRawByte> raw;
    for (int page = fromPage; page <= toPage; page++) {
        int glyph = page == fromPage ? fromGlyph : 0;
        int endGlyph = page == toPage ? toGlyph : -1;
        ReadAloudAppendPageGlyphs(raw, ts->engine, page, glyph, endGlyph);
    }

    return CleanRawBytes(raw, map, cleanedOut);
}

bool ReadAloudGetViewportStart(DisplayModel* dm, int* startPageOut, int* startGlyphOut) {
    if (!dm || !startPageOut || !startGlyphOut) {
        logf("tts: GetViewportStart: null args (dm=%p)\n", dm);
        return false;
    }

    *startPageOut = 0;
    *startGlyphOut = 0;

    int pageCount = dm->PageCount();
    Rect viewArea = dm->GetViewPort();
    viewArea.x = 0;
    viewArea.y = 0;
    dbgtts("GetViewportStart: viewArea=(%d,%d %dx%d)\n", viewArea.x, viewArea.y, viewArea.dx, viewArea.dy);

    int firstVisiblePage = 0;
    EngineBase* engine = dm->GetEngine();
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        PageInfo* pageInfo = dm->GetPageInfo(pageNo);
        if (!pageInfo || pageInfo->visibleRatio <= 0.0) {
            continue;
        }
        if (firstVisiblePage == 0) {
            firstVisiblePage = pageNo;
        }

        Rect* coords = nullptr;
        int textLen = 0;
        Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
        if (len(text) == 0) {
            continue;
        }

        int g = 0;
        int byteIdx = 0;
        while (g < textLen) {
            while (g < textLen) {
                int nextByte = byteIdx;
                int c = Utf8CodepointNext(text, nextByte);
                if (!IsLineBreakGlyph(coords, g, c)) {
                    break;
                }
                byteIdx = nextByte;
                g++;
            }
            if (g >= textLen) {
                break;
            }

            int lineStart = g;
            while (g < textLen) {
                int nextByte = byteIdx;
                int c = Utf8CodepointNext(text, nextByte);
                if (IsLineBreakGlyph(coords, g, c)) {
                    break;
                }
                byteIdx = nextByte;
                g++;
            }

            Rect lineBbox;
            for (int i = lineStart; i < g; i++) {
                Rect r = coords[i];
                if (r.x || r.dx) {
                    lineBbox = lineBbox.IsEmpty() ? r : lineBbox.Union(r);
                }
            }
            if (lineBbox.IsEmpty()) {
                continue;
            }

            Rect screenLine = dm->CvtToScreen(pageNo, ToRectF(lineBbox));
            if (!screenLine.Intersect(viewArea).IsEmpty()) {
                dbgtts("GetViewportStart: found visible line at page %d glyph %d (screenLine=%d,%d %dx%d)\n", pageNo,
                       lineStart, screenLine.x, screenLine.y, screenLine.dx, screenLine.dy);
                *startPageOut = pageNo;
                *startGlyphOut = lineStart;
                return true;
            }
        }
    }

    if (firstVisiblePage == 0) {
        logf("tts: GetViewportStart: no visible pages (pageCount=%d)\n", pageCount);
        return false;
    }

    dbgtts("GetViewportStart: no visible line in viewport, falling back to page %d glyph 0\n", firstVisiblePage);
    *startPageOut = firstVisiblePage;
    *startGlyphOut = 0;
    return true;
}

static bool ReadAloudGetGlyphAtCursor(DisplayModel* dm, Point screenPt, int* pageOut, int* glyphOut) {
    if (!dm || !pageOut || !glyphOut || !dm->textSelection) {
        return false;
    }
    if (!dm->IsOverText(screenPt)) {
        return false;
    }

    int pageNo = dm->GetPageNoByPoint(screenPt);
    if (!dm->ValidPageNo(pageNo)) {
        return false;
    }

    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return false;
    }

    PointF pt = dm->CvtFromScreen(screenPt, pageNo);

    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (len(text) == 0) {
        return false;
    }

    // find the glyph under the cursor without mutating the live selection:
    // StartAt() would overwrite startGlyph and corrupt an existing selection
    // when this is called from the context menu (issue #5718)
    // Same adjustment as TextSelection::IsOverGlyph: FindClosestGlyph can return
    // the index after the glyph under the cursor when clicking its right half.
    int glyph = dm->textSelection->FindClosestGlyphAt(pageNo, pt.x, pt.y);
    Point pti = ToPoint(pt);
    if (glyph == textLen || (glyph >= 0 && glyph < textLen && !coords[glyph].Contains(pti))) {
        glyph--;
    }
    if (glyph < 0 || glyph >= textLen) {
        return false;
    }

    *pageOut = pageNo;
    *glyphOut = glyph;
    return true;
}

bool ReadAloudCanReadFromCursor(DisplayModel* dm, Point screenPt) {
    int pageNo = 0;
    int glyph = 0;
    return ReadAloudGetGlyphAtCursor(dm, screenPt, &pageNo, &glyph);
}

bool ReadAloudGetCursorStart(DisplayModel* dm, Point screenPt, int* startPageOut, int* startGlyphOut) {
    if (!startPageOut || !startGlyphOut) {
        logf("tts: GetCursorStart: null args\n");
        return false;
    }

    *startPageOut = 0;
    *startGlyphOut = 0;

    int pageNo = 0;
    int glyph = 0;
    if (!ReadAloudGetGlyphAtCursor(dm, screenPt, &pageNo, &glyph)) {
        logf("tts: GetCursorStart: no text at cursor (%d,%d)\n", screenPt.x, screenPt.y);
        return false;
    }

    dbgtts("GetCursorStart: page %d glyph %d\n", pageNo, glyph);
    *startPageOut = pageNo;
    *startGlyphOut = glyph;
    return true;
}

bool ReadAloudHighlightBuildFromDocument(DisplayModel* dm, int startPage, int startGlyph, ReadAloudHighlightMap* map,
                                         str::Builder& cleanedOut) {
    if (!dm || !map || !dm->ValidPageNo(startPage)) {
        logf("tts: BuildFromDocument: invalid args (dm=%p map=%p startPage=%d)\n", dm, map, startPage);
        return false;
    }

    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        logf("tts: BuildFromDocument: no engine\n");
        return false;
    }

    Vec<ReadAloudRawByte> raw;
    int pageCount = dm->PageCount();
    dbgtts("BuildFromDocument: startPage=%d startGlyph=%d pageCount=%d\n", startPage, startGlyph, pageCount);
    for (int page = startPage; page <= pageCount; page++) {
        int glyph = page == startPage ? startGlyph : 0;
        ReadAloudAppendPageGlyphs(raw, engine, page, glyph, -1);
    }

    if (len(raw) == 0) {
        logf("tts: BuildFromDocument: no raw bytes extracted\n");
        return false;
    }

    if (!CleanRawBytes(raw, map, cleanedOut)) {
        logf("tts: BuildFromDocument: CleanRawBytes failed (raw.size=%zu)\n", len(raw));
        return false;
    }

    dbgtts("BuildFromDocument: ok raw=%zu cleanedLen=%d mapLen=%d\n", len(raw), (int)cleanedOut.len, map->len);
    return true;
}

void ReadAloudHighlightTimerStart(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    SetTimer(win->hwndCanvas, kReadAloudHighlightTimerID, kReadAloudHighlightDelayInMs, nullptr);
}

void ReadAloudHighlightTimerStop(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    KillTimer(win->hwndCanvas, kReadAloudHighlightTimerID);
}

static int gReadAloudPaintLogState = 0;

static void ReadAloudPaintLogOnce(int code, [[maybe_unused]] Str fmt) {
    if (gReadAloudPaintLogState == code) {
        return;
    }
    gReadAloudPaintLogState = code;
    dbgtts("%s\n", fmt);
}

static int ReadAloudWordEndUtf8(Str text, int pos) {
    if (len(text) == 0 || pos < 0) {
        return pos;
    }
    if (pos >= text.len) {
        return text.len;
    }
    while (pos < text.len && (IsReadAloudHorizontalSpace(text.s[pos]) || IsReadAloudLineBreak(text.s[pos]))) {
        pos++;
    }
    int end = pos;
    while (end < text.len && !IsReadAloudHorizontalSpace(text.s[end]) && !IsReadAloudLineBreak(text.s[end])) {
        end++;
    }
    return end;
}

static bool IsReadAloudSentPunct(int c) {
    return c == '.' || c == '!' || c == '?' || c == 0x3002 || c == 0xFF01 || c == 0xFF1F || c == 0x2026;
}

static bool IsReadAloudCloser(int c) {
    return c == '"' || c == '\'' || c == ')' || c == ']' || c == '}' || c == 0x2019 || c == 0x201D || c == 0x00BB;
}

// Byte after punctuation (and closers / spaces). 0 if this looks like "e.g. the".
static int ReadAloudAfterSentEnd(Str text, int afterPunct) {
    int after = afterPunct;
    while (after < text.len) {
        int t = after;
        int d = Utf8CodepointNext(text, t);
        if (!IsReadAloudCloser(d)) {
            break;
        }
        after = t;
    }
    while (after < text.len) {
        int t = after;
        int d = Utf8CodepointNext(text, t);
        if (d != ' ' && d != '\t') {
            break;
        }
        after = t;
    }
    if (after < text.len) {
        int t = after;
        int d = Utf8CodepointNext(text, t);
        if (d >= 'a' && d <= 'z') {
            return 0;
        }
    }
    return after;
}

bool ReadAloudSentenceRange(Str text, int pos, int* startOut, int* endOut) {
    if (!startOut || !endOut || len(text) == 0) {
        return false;
    }
    if (pos < 0) {
        return false;
    }
    if (pos > text.len) {
        pos = text.len;
    }

    int start = 0;
    int i = 0;
    while (i < pos) {
        int next = i;
        int c = Utf8CodepointNext(text, next);
        if (next <= i) {
            break;
        }
        if (IsReadAloudSentPunct(c)) {
            int after = ReadAloudAfterSentEnd(text, next);
            if (after > 0 && after <= pos) {
                start = after;
            }
            i = next;
            continue;
        }
        if (c == ' ' && next < text.len && text.s[next] == ' ') {
            int after = next;
            while (after < text.len && text.s[after] == ' ') {
                after++;
            }
            if (after <= pos) {
                start = after;
            }
            i = after;
            continue;
        }
        i = next;
    }

    int end = text.len;
    i = pos;
    while (i < text.len) {
        int next = i;
        int c = Utf8CodepointNext(text, next);
        if (next <= i) {
            break;
        }
        if (IsReadAloudSentPunct(c)) {
            int after = ReadAloudAfterSentEnd(text, next);
            if (after > 0) {
                end = after;
                break;
            }
        }
        if (c == ' ' && next < text.len && text.s[next] == ' ') {
            end = next;
            break;
        }
        i = next;
    }

    if (end <= start) {
        return false;
    }
    *startOut = start;
    *endOut = end;
    return true;
}

struct ReadAloudLineRun {
    int pageNo = 0;
    RectF bbox;
};

static bool ReadAloudLineContinues(const ReadAloudLineRun& run, int pageNo, const RectF& g) {
    if (run.pageNo != pageNo) {
        return false;
    }
    float aBot = run.bbox.y + run.bbox.dy;
    float bBot = g.y + g.dy;
    float h = std::max(run.bbox.dy, g.dy);
    float tol = std::max(h * 0.4f, 2.0f);
    if (std::abs(aBot - bBot) > tol) {
        return false;
    }
    float gap = g.x - (run.bbox.x + run.bbox.dx);
    if (gap < 0) {
        gap = run.bbox.x - (g.x + g.dx);
    }
    float maxGap = std::max(h * 2.5f, 8.0f);
    return gap <= maxGap;
}

static void ReadAloudFlushLine(DisplayModel* dm, Rect canvasRc, const ReadAloudLineRun& run, int minThick, int thickDiv,
                               Vec<Rect>& out) {
    if (run.pageNo <= 0 || run.bbox.IsEmpty()) {
        return;
    }
    PageInfo* pi = dm->GetPageInfo(run.pageNo);
    if (!pi || pi->visibleRatio <= 0.0) {
        return;
    }
    Rect sr = dm->CvtToScreen(run.pageNo, run.bbox);
    sr = sr.Intersect(canvasRc);
    if (sr.IsEmpty() || sr.dx <= 0) {
        return;
    }
    int thick = thickDiv > 0 ? sr.dy / thickDiv : minThick;
    if (thick < minThick) {
        thick = minThick;
    }
    if (thick > sr.dy) {
        thick = sr.dy;
    }
    if (thick < 1) {
        thick = 1;
    }
    Rect u = {sr.x, sr.y + sr.dy - thick, sr.dx, thick};
    if (!u.IsEmpty()) {
        VecAppend(out, u);
    }
}

static void ReadAloudAppendUnderlines(DisplayModel* dm, Rect canvasRc, ReadAloudHighlightMap* map, int startAbs,
                                      int endAbs, int minThick, int thickDiv, Vec<Rect>& out) {
    if (!dm || !map || !map->locs || startAbs < 0 || endAbs > map->len || endAbs <= startAbs) {
        return;
    }
    ReadAloudLineRun run;
    for (int i = startAbs; i < endAbs; i++) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        RectF g = ToRectF(ReadAloudByteLocToRect(loc));
        if (g.IsEmpty()) {
            continue;
        }
        if (run.pageNo == 0) {
            run.pageNo = loc.pageNo;
            run.bbox = g;
            continue;
        }
        if (ReadAloudLineContinues(run, loc.pageNo, g)) {
            run.bbox = run.bbox.Union(g);
            continue;
        }
        ReadAloudFlushLine(dm, canvasRc, run, minThick, thickDiv, out);
        run.pageNo = loc.pageNo;
        run.bbox = g;
    }
    ReadAloudFlushLine(dm, canvasRc, run, minThick, thickDiv, out);
}

bool ReadAloudGetProgressPage(WindowTab* tab, int* pageOut, int* pageCountOut) {
    if (!tab || !pageOut || !pageCountOut) {
        return false;
    }

    *pageOut = 0;
    *pageCountOut = 0;

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }
    *pageCountOut = dm->PageCount();

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0) {
        return false;
    }

    int absPos = -1;
    WindowTab* sourceTab = GetReadAloudSourceTab();
    if (sourceTab == tab && TtsIsSpeaking()) {
        int spokenPos = TtsGetSpokenPosUtf8();
        if (spokenPos >= 0) {
            absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart + spokenPos;
        }
    } else if (tab->readAloudResumePos >= 0) {
        absPos = tab->readAloudResumePos;
    } else if (tab->readAloudChunkEnd > 0) {
        absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart;
    }

    if (absPos < 0 || absPos >= map->len) {
        return false;
    }

    int pageNo = map->locs[absPos].pageNo;
    if (pageNo <= 0) {
        return false;
    }

    *pageOut = pageNo;
    return true;
}

static bool ReadAloudGetCurrentWordAbsRange(WindowTab* tab, int* startAbsOut, int* endAbsOut) {
    if (!tab || !startAbsOut || !endAbsOut) {
        return false;
    }

    *startAbsOut = 0;
    *endAbsOut = 0;

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0 || len(tab->readAloudText) == 0) {
        return false;
    }

    int spokenPos = TtsGetSpokenPosUtf8();
    if (spokenPos < 0) {
        return false;
    }

    int chunkLen = tab->readAloudChunkEnd > tab->readAloudChunkStart
                       ? tab->readAloudChunkEnd - tab->readAloudChunkStart
                       : tab->readAloudText.len - tab->readAloudChunkStart;
    Str chunkText = Str(tab->readAloudText.s + tab->readAloudChunkStart, chunkLen);
    int wordStartAbs = tab->readAloudHighlightBase + tab->readAloudChunkStart + spokenPos;
    int wordEndAbs =
        tab->readAloudHighlightBase + tab->readAloudChunkStart + ReadAloudWordEndUtf8(chunkText, spokenPos);
    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        return false;
    }
    wordEndAbs = std::min(wordEndAbs, map->len);
    if (wordEndAbs <= wordStartAbs) {
        return false;
    }

    *startAbsOut = wordStartAbs;
    *endAbsOut = wordEndAbs;
    return true;
}

static int ReadAloudGlyphDy(ReadAloudHighlightMap* map, int startAbs, int endAbs) {
    if (!map || !map->locs) {
        return 0;
    }
    for (int i = startAbs; i < endAbs && i < map->len; i++) {
        if (ReadAloudByteLocHasRect(map->locs[i]) && map->locs[i].dy > 0) {
            return map->locs[i].dy;
        }
    }
    return 0;
}

// Shrink a punct-based sentence to the visual paragraph around the word so
// heading-heavy PDF text (man pages, etc.) does not underline the whole page.
static void ReadAloudClampVisual(ReadAloudHighlightMap* map, int wordStartAbs, int wordEndAbs, int* startAbs,
                                 int* endAbs) {
    if (!map || !startAbs || !endAbs) {
        return;
    }
    int lineDy = ReadAloudGlyphDy(map, wordStartAbs, wordEndAbs);
    if (lineDy <= 0) {
        lineDy = ReadAloudGlyphDy(map, *startAbs, *endAbs);
    }
    if (lineDy <= 0) {
        return;
    }
    int maxGap = lineDy * 7 / 4;

    int lastY = 0;
    int lastPage = 0;
    bool have = false;
    int s = wordStartAbs;
    for (int i = wordStartAbs; i >= *startAbs; i--) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        if (!have) {
            lastY = loc.y;
            lastPage = loc.pageNo;
            have = true;
            s = i;
            continue;
        }
        if (loc.pageNo != lastPage) {
            break;
        }
        int yGap = lastY - loc.y;
        if (yGap > maxGap) {
            break;
        }
        s = i;
        lastY = loc.y;
        lastPage = loc.pageNo;
    }

    have = false;
    int e = wordEndAbs;
    for (int i = wordStartAbs; i < *endAbs; i++) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        if (!have) {
            lastY = loc.y;
            lastPage = loc.pageNo;
            have = true;
            e = i + 1;
            continue;
        }
        if (loc.pageNo != lastPage) {
            break;
        }
        int yGap = loc.y - lastY;
        if (yGap > maxGap) {
            break;
        }
        e = i + 1;
        lastY = loc.y;
        lastPage = loc.pageNo;
    }

    if (s >= *startAbs && s < *endAbs) {
        *startAbs = s;
    }
    if (e > *startAbs && e <= *endAbs) {
        *endAbs = e;
    }
}

static bool ReadAloudGetSentenceAbsRange(WindowTab* tab, int wordStartAbs, int wordEndAbs, int* startAbsOut,
                                         int* endAbsOut) {
    if (!tab || !startAbsOut || !endAbsOut) {
        return false;
    }

    *startAbsOut = 0;
    *endAbsOut = 0;

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0 || len(tab->readAloudText) == 0) {
        return false;
    }

    int rel = wordStartAbs - tab->readAloudHighlightBase;
    int sentRelStart = 0;
    int sentRelEnd = 0;
    if (!ReadAloudSentenceRange(tab->readAloudText, rel, &sentRelStart, &sentRelEnd)) {
        return false;
    }

    int chunkStartAbs = tab->readAloudHighlightBase + tab->readAloudChunkStart;
    int chunkLen = tab->readAloudChunkEnd > tab->readAloudChunkStart
                       ? tab->readAloudChunkEnd - tab->readAloudChunkStart
                       : tab->readAloudText.len - tab->readAloudChunkStart;
    int chunkEndAbs = chunkStartAbs + chunkLen;

    int startAbs = tab->readAloudHighlightBase + sentRelStart;
    int endAbs = tab->readAloudHighlightBase + sentRelEnd;
    startAbs = std::max(startAbs, chunkStartAbs);
    endAbs = std::min(endAbs, chunkEndAbs);
    startAbs = std::max(startAbs, 0);
    endAbs = std::min(endAbs, map->len);
    if (endAbs <= startAbs) {
        return false;
    }

    ReadAloudClampVisual(map, wordStartAbs, wordEndAbs, &startAbs, &endAbs);
    if (endAbs <= startAbs) {
        return false;
    }

    *startAbsOut = startAbs;
    *endAbsOut = endAbs;
    return true;
}

static bool ReadAloudGetCurrentWordScreenRect(MainWindow* win, Rect* rectOut) {
    if (!rectOut || !win) {
        return false;
    }

    *rectOut = Rect();

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win) {
        return false;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }

    int wordStartAbs = 0;
    int wordEndAbs = 0;
    if (!ReadAloudGetCurrentWordAbsRange(tab, &wordStartAbs, &wordEndAbs)) {
        return false;
    }

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    Rect unionRect;
    bool hasRect = false;
    for (int i = wordStartAbs; i < wordEndAbs; i++) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        Rect sr = dm->CvtToScreen(loc.pageNo, ToRectF(ReadAloudByteLocToRect(loc)));
        if (!hasRect) {
            unionRect = sr;
            hasRect = true;
        } else {
            unionRect = unionRect.Union(sr);
        }
    }

    if (!hasRect) {
        return false;
    }

    *rectOut = unionRect;
    return true;
}

static bool ReadAloudIsWordRectVisibleInViewport(MainWindow* win, const Rect& wordRect) {
    if (!win) {
        return false;
    }
    return !wordRect.Intersect(win->canvasRc).IsEmpty();
}

static bool ReadAloudIsWordRectFullyVisibleInViewport(MainWindow* win, const Rect& wordRect, int margin) {
    if (!win) {
        return false;
    }
    Rect canvas = win->canvasRc;
    if (wordRect.x < margin || wordRect.y < margin) {
        return false;
    }
    if (wordRect.x + wordRect.dx > canvas.dx - margin) {
        return false;
    }
    if (wordRect.y + wordRect.dy > canvas.dy - margin) {
        return false;
    }
    return true;
}

void ReadAloudOnUserViewChanged(MainWindow* win) {
    if (!win || win->readAloudScrollFromCode || !TtsIsSpeaking()) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win || !tab->readAloudAutoScroll) {
        return;
    }

    Rect wordRect;
    if (!ReadAloudGetCurrentWordScreenRect(win, &wordRect) || !ReadAloudIsWordRectVisibleInViewport(win, wordRect)) {
        tab->readAloudAutoScroll = false;
        dbgtts("auto-scroll disabled (user scrolled away from highlight)\n");
    }
}

void ReadAloudUpdateAutoScroll(MainWindow* win) {
    if (!win || !TtsIsSpeaking()) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win || !tab->readAloudAutoScroll) {
        return;
    }

    Rect wordRect;
    if (!ReadAloudGetCurrentWordScreenRect(win, &wordRect)) {
        return;
    }

    int margin = DpiScale(48);
    if (ReadAloudIsWordRectFullyVisibleInViewport(win, wordRect, margin)) {
        return;
    }

    Rect canvas = win->canvasRc;

    int dx = 0;
    int dy = 0;
    if (wordRect.y < margin) {
        dy = wordRect.y - margin;
    } else if (wordRect.y + wordRect.dy > canvas.dy - margin) {
        dy = wordRect.y + wordRect.dy - (canvas.dy - margin);
    }
    if (wordRect.x < margin) {
        dx = wordRect.x - margin;
    } else if (wordRect.x + wordRect.dx > canvas.dx - margin) {
        dx = wordRect.x + wordRect.dx - (canvas.dx - margin);
    }

    if (dx == 0 && dy == 0) {
        return;
    }

    int maxStep = std::max(canvas.dy / 4, DpiScale(120));
    if (dx > maxStep) {
        dx = maxStep;
    } else if (dx < -maxStep) {
        dx = -maxStep;
    }
    if (dy > maxStep) {
        dy = maxStep;
    } else if (dy < -maxStep) {
        dy = -maxStep;
    }

    win->readAloudScrollFromCode = true;
    win->MoveDocBy(dx, dy);
    win->readAloudScrollFromCode = false;
}

void PaintReadAloudHighlight(MainWindow* win, Gfx* gfx) {
    if (!TtsIsSpeaking()) {
        gReadAloudPaintLogState = 0;
        return;
    }
    if (!win) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win) {
        ReadAloudPaintLogOnce(1, StrL("PaintHighlight: no matching source tab"));
        return;
    }

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0) {
        ReadAloudPaintLogOnce(2, StrL("PaintHighlight: no highlight map"));
        return;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        ReadAloudPaintLogOnce(3, StrL("PaintHighlight: tab is not a fixed-layout document"));
        return;
    }

    int wordStartAbs = 0;
    int wordEndAbs = 0;
    if (!ReadAloudGetCurrentWordAbsRange(tab, &wordStartAbs, &wordEndAbs)) {
        if (gReadAloudPaintLogState != 4) {
            gReadAloudPaintLogState = 4;
            dbgtts("PaintHighlight: no spoken position (textLen=%d)\n", len(tab->readAloudText));
        }
        return;
    }

    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        ReadAloudPaintLogOnce(5, StrL("PaintHighlight: wordStartAbs out of range"));
        return;
    }
    wordEndAbs = std::min(wordEndAbs, map->len);
    if (wordEndAbs <= wordStartAbs) {
        ReadAloudPaintLogOnce(6, StrL("PaintHighlight: empty word range"));
        return;
    }

    constexpr Color kSentenceCol = MkRgb(0x3b, 0x82, 0xf6);
    constexpr Color kWordCol = MkRgb(0xf5, 0x9e, 0x0b);
    int minThick = DpiScale(2);

    Vec<Rect> sentenceRects;
    int sentStartAbs = 0;
    int sentEndAbs = 0;
    if (ReadAloudGetSentenceAbsRange(tab, wordStartAbs, wordEndAbs, &sentStartAbs, &sentEndAbs)) {
        ReadAloudAppendUnderlines(dm, win->canvasRc, map, sentStartAbs, sentEndAbs, minThick, 10, sentenceRects);
    }
    if (len(sentenceRects) > 0) {
        gfx->FillRects(sentenceRects.els, len(sentenceRects), kSentenceCol);
    }

    Vec<Rect> wordRects;
    ReadAloudAppendUnderlines(dm, win->canvasRc, map, wordStartAbs, wordEndAbs, DpiScale(3), 7, wordRects);
    if (len(wordRects) == 0) {
        ReadAloudPaintLogOnce(7, StrL("PaintHighlight: no screen rects for current word"));
        return;
    }
    gfx->FillRects(wordRects.els, len(wordRects), kWordCol);
}

// ---------------- 3. playback bar ----------------

struct ReadAloudPlaybackBar : WindowBase {
    ReadAloudPlaybackBar() = default;
    ~ReadAloudPlaybackBar() override = default;

    HWND Create(HWND parentCanvas);
    void SetSession(WindowTab* tab);
    void BuildLayout();
    void SyncLabels();
    void SyncColors();
    void UpdateLayout(bool forceLayout = false);
    void OnPaint(WindowBase::PaintEvent* ev);

    WindowTab* sessionTab = nullptr;
    HWND hwndCanvas = nullptr;
    VirtButton* btnPause = nullptr;
    VirtButton* btnStop = nullptr;
    VirtSlider* speedSlider = nullptr;
    VirtText* speedLabel = nullptr;
    VirtText* status = nullptr;
    bool showResume = false;
    int lastX = 0;
    int lastY = 0;
    int lastDx = 0;
    int lastDy = 0;
    Func1List<MainWindow*> onWindowMoved;
};

constexpr int kBarMargin = 8;
constexpr int kBarPadX = 12;
constexpr int kBarPadY = 6;
constexpr int kBtnGap = 8;
constexpr int kBtnPadX = 10;
constexpr int kBtnPadY = 3;

static Str ReadAloudScopeLabel(WindowTab* tab) {
    if (!tab) {
        return {};
    }
    switch (tab->readAloudScope) {
        case WindowTab::ReadAloudScopeSelection:
            return Tr("Selection");
        case WindowTab::ReadAloudScopeViewport:
            return Tr("Top of view");
        case WindowTab::ReadAloudScopeCursor:
            return Tr("From cursor");
        case WindowTab::ReadAloudScopeSmart:
        default:
            return Tr("Smart start");
    }
}

static TempStr ReadAloudPlaybackBarTextTemp(WindowTab* tab) {
    if (!tab) {
        return {};
    }

    Str docName = tab->GetTabTitle();
    if (len(docName) == 0) {
        docName = Tr("document");
    }

    int pageNo = 0;
    int pageCount = 0;
    bool hasPage = ReadAloudGetProgressPage(tab, &pageNo, &pageCount);
    Str scope = ReadAloudScopeLabel(tab);

    bool isPaused = CanContinueReadAloud(tab) && !TtsIsSpeaking();
    if (hasPage && pageCount > 0) {
        const char* pattern = isPaused ? Tr("Paused \xC2\xB7 %s \xC2\xB7 page %d of %d \xC2\xB7 %s").s
                                       : Tr("Reading \xC2\xB7 %s \xC2\xB7 page %d of %d \xC2\xB7 %s").s;
        return fmt(pattern, docName, pageNo, pageCount, scope);
    }
    const char* pattern = isPaused ? Tr("Paused \xC2\xB7 %s \xC2\xB7 %s").s : Tr("Reading \xC2\xB7 %s \xC2\xB7 %s").s;
    return fmt(pattern, docName, scope);
}

static void OnPauseClicked(ReadAloudPlaybackBar* bar, VirtMouseEvent*) {
    dbgtts("bar pause-click speaking=%d resume=%d\n", (int)TtsIsSpeaking(), (int)bar->showResume);
    ReadAloudPlaybackPauseOrResume();
    bar->UpdateLayout(true);
    HwndRepaintNow(bar->hwnd);
}

static void OnStopClicked(ReadAloudPlaybackBar*, VirtMouseEvent*) {
    dbgtts("bar stop-click\n");
    ReadAloudPlaybackStop();
}

static void SyncSpeedLabel(ReadAloudPlaybackBar* bar) {
    int idx = bar->speedSlider ? bar->speedSlider->value : ReadAloudClosestSpeedIdx();
    bar->speedLabel->SetText(ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx)));
}

static void OnSpeedSliderDrag(ReadAloudPlaybackBar* bar) {
    SyncSpeedLabel(bar);
    HwndInvalidate(bar->hwnd);
}

static void OnSpeedSliderCommit(ReadAloudPlaybackBar* bar) {
    ReadAloudSetSpeedIdx(bar->speedSlider->value);
    SyncSpeedLabel(bar);
    HwndInvalidate(bar->hwnd);
}

static void OnSpeedSliderTooltip(ReadAloudPlaybackBar* bar, VirtTooltipEvent* ev) {
    int idx = bar->speedSlider->ValueFromLocalX(ev->ptLocal.x);
    ev->tip = ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx));
}

static void OnBarWndProc(WindowBase::WndProcEvent* ev) {
    UINT msg = ev->msg;
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) {
        int x = GET_X_LPARAM(ev->lparam);
        int y = GET_Y_LPARAM(ev->lparam);
        dbgtts("bar-mouse msg=0x%x x=%d y=%d\n", (int)msg, x, y);
    }
}

HWND ReadAloudPlaybackBar::Create(HWND parentCanvas) {
    onPaint = MkMethod1<ReadAloudPlaybackBar, WindowBase::PaintEvent*, &ReadAloudPlaybackBar::OnPaint>(this);
    onWndProc = MkFunc1Void(OnBarWndProc);
    hwndCanvas = parentCanvas;
    CreateCustomArgs args;
    // Owned popup, not a canvas child: WebView2 fills the canvas and steals
    // clicks from sibling HWNDs (issue #6031). Same pattern as the overlay
    // scrollbar / find bar.
    args.owner = GetAncestor(parentCanvas, GA_ROOT);
    args.style = WS_POPUP;
    args.exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    args.font = GetAppBiggerFont();
    args.visible = false;
    args.isRtl = IsUIRtl();
    CreateCustom(args);
    if (hwnd) {
        BuildLayout();
    }
    return hwnd;
}

// [Pause] [Stop] [slider] [1.5x] [status…]. The HWND is WS_EX_LAYOUTRTL, but we
// paint into a DoubleBuffer DC that is not mirrored, so HBox.rtl (not GDI's
// flip) is what reverses the row.
void ReadAloudPlaybackBar::BuildLayout() {
    PlatformFont* pf = font;
    int gap = DpiScale(kBtnGap);
    int padX = DpiScale(kBarPadX);
    int padY = DpiScale(kBarPadY);
    int btnPadX = DpiScale(kBtnPadX);
    int btnPadY = DpiScale(kBtnPadY);
    Insets btnPad{btnPadY, btnPadX, btnPadY, btnPadX};

    btnPause = new VirtButton({}, pf);
    btnPause->textPadding = btnPad;
    btnPause->flags &= ~vwfFocusable;
    btnPause->flags |= vwfCapturesMouse;
    btnPause->onClick = MkFunc1(OnPauseClicked, this);

    btnStop = new VirtButton(Tr("Stop"), pf);
    btnStop->textPadding = btnPad;
    btnStop->flags &= ~vwfFocusable;
    btnStop->flags |= vwfCapturesMouse;
    btnStop->onClick = MkFunc1(OnStopClicked, this);

    speedSlider = new VirtSlider();
    speedSlider->minVal = 0;
    speedSlider->maxVal = std::max(ReadAloudSpeedCount() - 1, 0);
    speedSlider->value = ReadAloudClosestSpeedIdx();
    speedSlider->onValueChanged = MkFunc0(OnSpeedSliderDrag, this);
    speedSlider->onValueCommitted = MkFunc0(OnSpeedSliderCommit, this);
    speedSlider->onGetTooltip = MkFunc1(OnSpeedSliderTooltip, this);

    speedLabel = NewVirtText({
        .font = pf,
        .isRtl = IsUIRtl(),
    });

    status = NewVirtText({
        .font = pf,
        .isRtl = IsUIRtl(),
        .ellipsis = true,
    });

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->rtl = IsUIRtl();
    row->AddChild(btnPause);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(btnStop);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(speedSlider);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(speedLabel);
    row->AddChild(new Spacer(gap, 0));
    row->AddChild(status, 1);
    layout = new Padding(row, Insets{padY, padX, padY, padX});
}

void ReadAloudPlaybackBar::SyncLabels() {
    showResume = sessionTab && CanContinueReadAloud(sessionTab) && !TtsIsSpeaking();
    btnPause->SetText(showResume ? Tr("Resume") : Tr("Pause"));
    if (!speedSlider->IsAdjusting()) {
        speedSlider->SetValue(ReadAloudClosestSpeedIdx(), false);
        SyncSpeedLabel(this);
    }
    status->SetText(ReadAloudPlaybackBarTextTemp(sessionTab));
}

void ReadAloudPlaybackBar::SyncColors() {
    Color colBg = ThemeNotificationsBackgroundColor();
    Color colTxt = ThemeNotificationsTextColor();
    Color colBorder = kColGray;
    Color colBtnBg = AccentColor(colBg, 8, -8);
    Color colBtnHover = AccentColor(colBg, 16, -16);
    VirtButton* btns[] = {btnPause, btnStop};
    for (VirtButton* b : btns) {
        b->SetColor(kColBtnBg, colBtnBg);
        b->SetColor(kColBtnBgHover, colBtnHover);
        b->SetColor(kColBtnBorder, colBorder);
        b->SetColor(kColBtnText, colTxt);
    }
    Color thumb = colTxt;
    speedSlider->SetColor(kColSliderTrack, AccentColor(colBg, 28, -28));
    speedSlider->SetColor(kColSliderFill, thumb);
    speedSlider->SetColor(kColSliderThumb, thumb);
    speedSlider->SetColor(kColSliderThumbHover, AccentColor(thumb, 18, -18));
    speedLabel->SetColor(kColText, colTxt);
    status->SetColor(kColText, colTxt);
}

void ReadAloudPlaybackBar::SetSession(WindowTab* tab) {
    sessionTab = tab;
    if (!tab || !hwnd) {
        return;
    }

    UpdateLayout();
    if (!HwndIsVisible(hwnd)) {
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    HwndInvalidate(hwnd);
}

void ReadAloudPlaybackBar::UpdateLayout(bool forceLayout) {
    if (!hwnd || !layout || !hwndCanvas) {
        return;
    }

    SyncLabels();

    Rect canvas = HwndMapLtrClientRectToScreen(hwndCanvas, HwndClientRect(hwndCanvas));
    int margin = DpiScale(kBarMargin);
    int barDx = std::max(canvas.dx - (2 * margin), 0);
    int barDy = lastDy;
    if (forceLayout || barDy <= 0 || barDx != lastDx) {
        barDy = layout->Layout(ExpandInf()).dy;
    }

    int x = canvas.x + margin;
    int y = canvas.y + canvas.dy - barDy - margin;
    if (y < canvas.y + margin) {
        y = canvas.y + margin;
    }

    bool samePos = (x == lastX && y == lastY && barDx == lastDx && barDy == lastDy);
    lastX = x;
    lastY = y;
    lastDx = barDx;
    lastDy = barDy;
    if (!samePos) {
        dbgtts("bar-move x=%d y=%d %dx%d\n", x, y, barDx, barDy);
        SetWindowPos(hwnd, HWND_TOP, x, y, barDx, barDy, SWP_NOACTIVATE);
    }
    if (!samePos || forceLayout) {
        dbgtts("bar-layout force=%d samePos=%d\n", (int)forceLayout, (int)samePos);
        DoLayout({barDx, barDy});
    }
}

void ReadAloudPlaybackBar::OnPaint(WindowBase::PaintEvent* ev) {
    Rect rc = HwndClientRect(hwnd);

    Color colBg = ThemeNotificationsBackgroundColor();
    Color colBorder = kColGray;

    SyncColors();
    Gfx* gfx = GfxCreateWithDoubleBuffer(this, ev->hdc);
    gfx->FillRect(rc, colBg);
    if (vroot) {
        vroot->Paint(gfx, rc);
    }
    gfx->DrawRect(rc, colBorder);
    delete gfx;
}

static void ReadAloudPlaybackBarOnWindowMoved(ReadAloudPlaybackBar* bar, MainWindow*) {
    if (!bar->hwnd || !HwndIsVisible(bar->hwnd)) {
        return;
    }
    bar->UpdateLayout();
}

static ReadAloudPlaybackBar* ReadAloudPlaybackBarEnsure(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return nullptr;
    }
    if (!win->readAloudPlaybackBar) {
        auto* bar = new ReadAloudPlaybackBar();
        bar->Create(win->hwndCanvas);
        bar->onWindowMoved = MkFunc1(ReadAloudPlaybackBarOnWindowMoved, bar);
        win->RegisterOnWindowMoved(&bar->onWindowMoved);
        win->readAloudPlaybackBar = bar;
    }
    return win->readAloudPlaybackBar;
}

void ReadAloudPlaybackBarDestroy(MainWindow* win) {
    if (!win || !win->readAloudPlaybackBar) {
        return;
    }
    win->UnregisterOnWindowMoved(&win->readAloudPlaybackBar->onWindowMoved);
    delete win->readAloudPlaybackBar;
    win->readAloudPlaybackBar = nullptr;
}

void ReadAloudPlaybackBarHide(MainWindow* win) {
    if (!win || !win->readAloudPlaybackBar || !win->readAloudPlaybackBar->hwnd) {
        return;
    }
    win->readAloudPlaybackBar->sessionTab = nullptr;
    SetWindowPos(win->readAloudPlaybackBar->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

// the tab is going away; the bar has no reason to exist without it
void ReadAloudPlaybackBarForgetTab(MainWindow* win, WindowTab* tab) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar || bar->sessionTab != tab) {
        return;
    }
    bar->sessionTab = nullptr;
    if (bar->hwnd) {
        SetWindowPos(bar->hwnd, nullptr, 0, 0, 0, 0,
                     SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
}

void ReadAloudPlaybackBarRelayout(HWND hwndCanvas) {
    MainWindow* win = FindMainWindowByHwnd(hwndCanvas);
    if (!win || !win->readAloudPlaybackBar || !win->readAloudPlaybackBar->hwnd) {
        return;
    }
    if (!HwndIsVisible(win->readAloudPlaybackBar->hwnd)) {
        return;
    }
    win->readAloudPlaybackBar->UpdateLayout();
}

// Highlight timer (~80ms): refresh Pause/page text without SetWindowPos.
// Relayouting every tick ate mouse-up (issue #6031).
void ReadAloudPlaybackBarTick(MainWindow* win) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar || !bar->hwnd || !HwndIsVisible(bar->hwnd)) {
        return;
    }
    if (bar->speedSlider && bar->speedSlider->IsAdjusting()) {
        return;
    }
    bool wasResume = bar->showResume;
    TempStr statusBefore = str::DupTemp(bar->status ? bar->status->s : Str{});
    bar->SyncLabels();
    if (wasResume != bar->showResume) {
        bar->UpdateLayout(true);
        return;
    }
    SetWindowPos(bar->hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // skip a full paint when Pause/page text is unchanged: D2D BeginDraw on the
    // bar's memory DC throws a first-chance C++ EH inside d3d11 every time
    if (!str::Eq(statusBefore, bar->status ? bar->status->s : Str{})) {
        HwndInvalidate(bar->hwnd);
    }
}

TempStr ReadAloudPlaybackBarStateTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    Vec<TtsVoiceInfo> voices = TtsGetVoices();
    int nVoices = len(voices);
    TtsFreeVoices(voices);
    out.Append(fmt("voices=%d speaking=%d\n", nVoices, (int)TtsIsSpeaking()));

    if (len(gWindows) == 0) {
        out.Append(StrL("NOTREADY no-window\n"));
        return finish(2);
    }
    MainWindow* win = gWindows[0];
    ReadAloudPlaybackBar* bar = win->readAloudPlaybackBar;
    if (!bar || !bar->hwnd || !HwndIsVisible(bar->hwnd) || !bar->btnPause || !bar->btnStop || !bar->speedSlider ||
        !bar->speedLabel) {
        out.Append(StrL("NOTREADY no-bar\n"));
        return finish(2);
    }

    Rect pause = bar->btnPause->bounds;
    Rect stop = bar->btnStop->bounds;
    Rect speed = bar->speedSlider->bounds;
    Rect speedLab = bar->speedLabel->bounds;
    int idx = bar->speedSlider->value;
    out.Append(fmt("OK visible=1 resume=%d hwnd=%d\n", (int)bar->showResume, (int)(uintptr_t)bar->hwnd));
    out.Append(fmt("pause=%d,%d,%d,%d\n", pause.x, pause.y, pause.dx, pause.dy));
    out.Append(fmt("stop=%d,%d,%d,%d\n", stop.x, stop.y, stop.dx, stop.dy));
    out.Append(fmt("speed=%d,%d,%d,%d\n", speed.x, speed.y, speed.dx, speed.dy));
    out.Append(fmt("speedLabel=%d,%d,%d,%d\n", speedLab.x, speedLab.y, speedLab.dx, speedLab.dy));
    out.Append(fmt("speedIdx=%d speedCount=%d label=%s\n", idx, ReadAloudSpeedCount(),
                   ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx))));
    out.Append(fmt("status=%s\n", bar->status ? bar->status->s : Str{}));
    return finish(0);
}

void ReadAloudPlaybackBarUpdateSession(WindowTab* tab) {
    if (!tab) {
        // no read-aloud source any more (callers pass GetReadAloudSourceTab()),
        // so no bar should be up. Hiding also drops the tab each bar points at,
        // which is about to be deleted on the tab-close path
        for (MainWindow* win : gWindows) {
            ReadAloudPlaybackBarHide(win);
        }
        return;
    }
    if (!tab->win || len(tab->readAloudText) == 0) {
        ReadAloudPlaybackBarHide(tab->win);
        return;
    }

    ReadAloudPlaybackBar* bar = ReadAloudPlaybackBarEnsure(tab->win);
    if (!bar) {
        return;
    }
    bar->SetSession(tab);

    // hide bars on other windows
    for (MainWindow* win : gWindows) {
        if (win != tab->win && win->readAloudPlaybackBar && HwndIsVisible(win->readAloudPlaybackBar->hwnd)) {
            ReadAloudPlaybackBarHide(win);
        }
    }
}

// ---------------- 4. read-aloud session ----------------

static WindowTab* gReadAloudSourceTab = nullptr;
static WindowTab* gReadAloudSessionTab = nullptr;
static HMENU gReadAloudAppSubmenu = nullptr;
static HMENU gReadAloudContextSubmenu = nullptr;

static void ReadAloudClearSourceTab();

void SetReadAloudAppSubmenu(HMENU menu) {
    gReadAloudAppSubmenu = menu;
}

HMENU GetReadAloudAppSubmenu() {
    return gReadAloudAppSubmenu;
}

bool IsReadAloudAppSubmenu(HMENU menu) {
    return menu && menu == gReadAloudAppSubmenu;
}

void SetReadAloudContextSubmenu(HMENU menu) {
    gReadAloudContextSubmenu = menu;
}

bool IsReadAloudContextSubmenu(HMENU menu) {
    return menu && menu == gReadAloudContextSubmenu;
}

HMENU GetReadAloudContextSubmenu() {
    return gReadAloudContextSubmenu;
}

static void ReadAloudShowNotif(WindowTab* tab, Str msg);

static void ReadAloudSaveVoicePref(Str voiceId) {
    if (!gSettings) {
        return;
    }
    str::ReplaceWithCopy(&gSettings->readAloudVoiceId, voiceId);
    ScheduleSaveSettings();
}

// WinRT speech synthesis is too slow for whole-document requests; speak in chunks.
static constexpr int kReadAloudMaxChunkLen = 1024;

static bool IsReadAloudSentencePunct(int c) {
    return c == '.' || c == '!' || c == '?' || c == 0x3002 || c == 0xFF01 || c == 0xFF1F || c == 0x2026;
}

// Prefer a sentence end in the window so TTS does not drop intonation mid-clause
// (issue #6110). Skip "e.g. the" (lowercase after the period). Else last space.
static int ReadAloudFindChunkEnd(Str text, int start, int maxLen) {
    int textLen = text.len;
    if (start >= textLen) {
        return textLen;
    }

    int limit = start + maxLen;
    if (limit >= textLen) {
        return textLen;
    }

    int bestSent = start;
    int bestSpace = start;
    int i = start;
    while (i < limit) {
        int c = Utf8CodepointNext(text, i);
        if (c == ' ' || c == '\t') {
            bestSpace = i;
            continue;
        }
        if (!IsReadAloudSentencePunct(c)) {
            continue;
        }

        int after = i;
        while (after < textLen) {
            int t = after;
            int d = Utf8CodepointNext(text, t);
            if (!IsReadAloudCloser(d)) {
                break;
            }
            after = t;
        }
        while (after < textLen) {
            int t = after;
            int d = Utf8CodepointNext(text, t);
            if (d != ' ' && d != '\t') {
                break;
            }
            after = t;
        }
        if (after < textLen) {
            int t = after;
            int d = Utf8CodepointNext(text, t);
            if (d >= 'a' && d <= 'z') {
                continue;
            }
        }
        if (after > start && after <= limit) {
            bestSent = after;
        }
    }

    if (bestSent > start) {
        return bestSent;
    }
    if (bestSpace > start) {
        return bestSpace;
    }
    return limit;
}

static void ReadAloudQueueNext(WindowTab* tab) {
    if (!tab || tab->readAloudQueuedEnd > 0 || len(tab->readAloudText) == 0) {
        return;
    }
    if (tab->readAloudChunkEnd >= tab->readAloudText.len) {
        return;
    }

    int start = tab->readAloudChunkEnd;
    int end = ReadAloudFindChunkEnd(tab->readAloudText, start, kReadAloudMaxChunkLen);
    if (start >= end) {
        return;
    }

    TempStr chunk = str::DupTemp(Str(tab->readAloudText.s + start, end - start));
    if (!TtsQueueUtf8(chunk)) {
        return;
    }
    tab->readAloudQueuedEnd = end;
    dbgtts("queue-next %d..%d of %d\n", start, end, tab->readAloudText.len);
}

static void ReadAloudOnQueuedStarted(WindowTab* tab) {
    if (!tab || tab->readAloudQueuedEnd <= tab->readAloudChunkEnd) {
        tab->readAloudQueuedEnd = 0;
        return;
    }
    tab->readAloudChunkStart = tab->readAloudChunkEnd;
    tab->readAloudChunkEnd = tab->readAloudQueuedEnd;
    tab->readAloudQueuedEnd = 0;
    dbgtts("queued-now %d..%d of %d\n", tab->readAloudChunkStart, tab->readAloudChunkEnd, tab->readAloudText.len);
}

// Promote a prefetched chunk and start the next prefetch.
void ReadAloudAfterTtsEvents() {
    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab) {
        return;
    }
    if (TtsDidStartQueued()) {
        ReadAloudOnQueuedStarted(tab);
    }
    if (TtsIsSpeaking()) {
        ReadAloudQueueNext(tab);
    }
}

static bool ReadAloudHasMoreChunks(WindowTab* tab) {
    if (!tab || len(tab->readAloudText) == 0) {
        return false;
    }
    return tab->readAloudChunkEnd < tab->readAloudText.len;
}

static void ReadAloudFinishSession(WindowTab* tab, MainWindow* win) {
    if (!tab) {
        return;
    }

    dbgtts("finish-session\n");
    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
        ReadAloudPlaybackBarHide(tab->win);
    }
    str::Free(tab->readAloudText);
    tab->readAloudText = {};
    tab->readAloudResumePos = -1;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    tab->readAloudQueuedEnd = 0;
    if (tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }
    tab->readAloudHighlightBase = 0;
    tab->readAloudAutoScroll = false;
    tab->readAloudScope = 0;
    ReadAloudClearSourceTab();
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    if (win) {
        ToolbarUpdateStateForWindow(win, true);
    }
}

static bool ReadAloudSpeakChunk(WindowTab* tab, Str errMsg) {
    if (!tab || len(tab->readAloudText) == 0) {
        return false;
    }

    int start = tab->readAloudChunkEnd;
    int textLen = tab->readAloudText.len;
    int end = ReadAloudFindChunkEnd(tab->readAloudText, start, kReadAloudMaxChunkLen);
    if (start >= end) {
        return false;
    }

    int chunkLen = end - start;
    TempStr chunk = str::DupTemp(Str(tab->readAloudText.s + start, (int)((size_t)chunkLen)));
    if (!TtsSpeakUtf8(chunk)) {
        logf("tts: SpeakChunk: TtsSpeakUtf8 failed\n");
        dbgtts("chunk speak failed %d..%d of %d\n", start, end, textLen);
        ReadAloudShowNotif(tab, errMsg);
        return false;
    }
    dbgtts("chunk %d..%d of %d mapBase=%d\n", start, end, textLen, tab->readAloudHighlightBase);

    tab->readAloudChunkStart = start;
    tab->readAloudChunkEnd = end;
    tab->readAloudQueuedEnd = 0;
    ReadAloudQueueNext(tab);
    ToolbarUpdateStateForWindow(tab->win, true);
    HwndInvalidate(tab->win->hwndCanvas);
    return true;
}

// Text cleanup for speech
static TempStr CleanReadAloudTextTemp(Str text) {
    if (len(text) == 0) {
        return {};
    }

    str::Builder out;
    int i = 0;
    bool lastWasSpace = false;

    while (i < text.len) {
        char c = text.s[i];

        // Remove likely soft hyphenation caused by PDF line wrapping:
        // "cap-\nturing" -> "capturing"
        //
        // Conservative rule: only join lowercase ASCII on both sides.
        // This avoids damaging many intentional hyphen cases.
        if (c == '-' && i + 1 < text.len && IsReadAloudLineBreak(text.s[i + 1])) {
            int after = i + 1;

            while (after < text.len && IsReadAloudLineBreak(text.s[after])) {
                after++;
            }

            bool prevIsLower = i > 0 && IsReadAloudLowerAscii(text.s[i - 1]);
            bool nextIsLower = after < text.len && IsReadAloudLowerAscii(text.s[after]);

            if (prevIsLower && nextIsLower) {
                i = after;
                lastWasSpace = false;
                continue;
            }
        }

        // Convert extracted visual line breaks into spaces.
        if (IsReadAloudLineBreak(c)) {
            int lineBreaks = 0;

            while (i < text.len && IsReadAloudLineBreak(text.s[i])) {
                if (text.s[i] == '\n') {
                    lineBreaks++;
                }
                i++;
            }

            while (i < text.len && IsReadAloudHorizontalSpace(text.s[i])) {
                i++;
            }

            if (!lastWasSpace && len(out) > 0) {
                out.AppendChar(' ');
                lastWasSpace = true;
            }

            // Keep a slightly stronger pause for paragraph breaks.
            if (lineBreaks >= 2) {
                out.AppendChar(' ');
            }

            continue;
        }

        // Collapse spaces and tabs.
        if (IsReadAloudHorizontalSpace(c)) {
            if (!lastWasSpace && len(out) > 0) {
                out.AppendChar(' ');
                lastWasSpace = true;
            }

            i++;
            continue;
        }

        out.AppendChar(c);
        lastWasSpace = false;
        i++;
    }

    if (len(out) == 0) {
        return {};
    }
    return ToStrTemp(out);
}

// Read-aloud lifetime and commands
static void ReadAloudSetSourceTab(WindowTab* tab) {
    gReadAloudSourceTab = tab;
}

static void ReadAloudClearSourceTab() {
    gReadAloudSourceTab = nullptr;
}

static void StopReadAloudIfSourceTab(WindowTab* tab) {
    if (!tab || gReadAloudSourceTab != tab) {
        return;
    }

    if (TtsIsSpeaking()) {
        TtsStop();
    }

    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
    }
    ReadAloudClearSourceTab();
}

// last-resort guard: whatever a close path forgets, a tab that is being
// destroyed can't be left pointed at by the read-aloud state
void ReadAloudForgetTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    if (gReadAloudSourceTab == tab) {
        TtsStop();
        gReadAloudSourceTab = nullptr;
    }
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    for (MainWindow* win : gWindows) {
        ReadAloudPlaybackBarForgetTab(win, tab);
    }
}

void StopReadAloudIfSourceWindow(MainWindow* win) {
    if (!win || !gReadAloudSourceTab || gReadAloudSourceTab->win != win) {
        return;
    }

    if (TtsIsSpeaking()) {
        TtsStop();
    }

    ReadAloudClearSourceTab();
}

// reset "Continue reading" state, called when its document goes away
void ResetReadAloudStateForTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    StopReadAloudIfSourceTab(tab);
    str::Free(tab->readAloudText);
    tab->readAloudText = {};
    tab->readAloudResumePos = -1;
    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
    }
    if (tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }
    tab->readAloudHighlightBase = 0;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    tab->readAloudQueuedEnd = 0;
    tab->readAloudAutoScroll = false;
    tab->readAloudScope = 0;
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    if (tab->win) {
        ReadAloudPlaybackBarHide(tab->win);
    }
}

// stop reading and remember where we stopped so that "Continue reading"
// can pick up from there
void ReadAloudStopRememberPos() {
    // drain pending word-boundary events for an accurate position
    TtsProcessEvents();
    WindowTab* tab = gReadAloudSourceTab;
    if (tab && TtsIsSpeaking()) {
        int pos = TtsGetSpokenPosUtf8();
        int absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart + (pos >= 0 ? pos : 0);
        int maxPos = tab->readAloudHighlightBase + tab->readAloudText.len;
        if (absPos >= 0 && absPos < maxPos) {
            tab->readAloudResumePos = absPos;
        }
    }
    TtsStop();
    ReadAloudClearSourceTab();
    if (tab && tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
        ReadAloudPlaybackBarUpdateSession(tab);
    }
}

void ReadAloudPlaybackPauseOrResume() {
    WindowTab* tab = gReadAloudSessionTab;
    if (!tab) {
        tab = GetReadAloudSourceTab();
    }
    dbgtts("pause-or-resume speaking=%d session=%d source=%d canContinue=%d\n", (int)TtsIsSpeaking(), tab ? 1 : 0,
           GetReadAloudSourceTab() ? 1 : 0, tab ? (int)CanContinueReadAloud(tab) : 0);
    if (!tab || !tab->win) {
        return;
    }

    if (TtsIsSpeaking() && GetReadAloudSourceTab() == tab) {
        dbgtts("pause now\n");
        ReadAloudStopRememberPos();
        ToolbarUpdateStateForWindow(tab->win, true);
    } else if (CanContinueReadAloud(tab)) {
        dbgtts("resume now\n");
        ReadAloudContinueInTab(tab);
    }
}

// preset playback speeds offered in the Speed menu and cycled by the speed
// button on the playback bar
constexpr float kReadAloudSpeeds[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 2.25f, 2.5f, 2.75f, 3.0f, 3.25f, 3.5f};

// e.g. "1x", "0.75x", "1.5x"
TempStr ReadAloudSpeedLabelTemp(float speed) {
    int hundredths = (int)lroundf(speed * 100.0f);
    int whole = hundredths / 100;
    int frac = hundredths % 100;
    if (frac == 0) {
        return fmt("%dx", whole);
    }
    if (frac % 10 == 0) {
        return fmt("%d.%dx", whole, frac / 10);
    }
    return fmt("%d.%02dx", whole, frac);
}

int ReadAloudSpeedCount() {
    return dimofi(kReadAloudSpeeds);
}

float ReadAloudSpeedAt(int idx) {
    int n = dimofi(kReadAloudSpeeds);
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= n) {
        idx = n - 1;
    }
    return kReadAloudSpeeds[idx];
}

int ReadAloudClosestSpeedIdx() {
    float curr = TtsGetSpeed();
    int idx = 0;
    float bestDist = -1;
    for (int i = 0; i < dimofi(kReadAloudSpeeds); i++) {
        float dist = kReadAloudSpeeds[i] - curr;
        if (dist < 0) {
            dist = -dist;
        }
        if (bestDist < 0 || dist < bestDist) {
            bestDist = dist;
            idx = i;
        }
    }
    return idx;
}

static void ReadAloudSetSpeed(float speed) {
    TtsSetSpeed(speed);
    gSettings->readAloudSpeed = TtsGetSpeed();
    dbgtts("SetSpeed: %s\n", ReadAloudSpeedLabelTemp(TtsGetSpeed()));
    ScheduleSaveSettings();

    // the WinRT backend applies the new speed only to newly synthesized
    // chunks, so re-speak from the current position
    WindowTab* tab = GetReadAloudSourceTab();
    if (tab && TtsIsSpeaking()) {
        ReadAloudStopRememberPos();
        if (CanContinueReadAloud(tab)) {
            ReadAloudContinueInTab(tab);
        }
    }
}

// dir is +1 (next speed) or -1 (previous speed), wraps around
void ReadAloudSetSpeedIdx(int idx) {
    ReadAloudSetSpeed(ReadAloudSpeedAt(idx));
}

void ReadAloudPlaybackCycleSpeed(int dir) {
    int n = dimofi(kReadAloudSpeeds);
    int idx = (ReadAloudClosestSpeedIdx() + dir + n) % n;
    ReadAloudSetSpeed(kReadAloudSpeeds[idx]);
}

void ReadAloudPlaybackStop() {
    dbgtts("stop-click\n");
    // always halt TTS, even if the session tab is already gone (issue #6053)
    TtsStop();
    WindowTab* tab = gReadAloudSessionTab;
    if (!tab) {
        tab = GetReadAloudSourceTab();
    }
    if (!tab) {
        ReadAloudClearSourceTab();
        for (MainWindow* w : gWindows) {
            ReadAloudPlaybackBarHide(w);
        }
        return;
    }
    ReadAloudFinishSession(tab, tab->win);
}

static void ReadAloudShowNotif(WindowTab* tab, Str msg) {
    NotificationCreateArgs args;
    args.hwndParent = tab->win->hwndCanvas;
    args.msg = msg;
    args.timeoutMs = 2000;
    ShowNotification(args);
}

// remembers cleaned text on the tab and starts speaking it in TTS-sized chunks
static void ReadAloudStartText(WindowTab* tab, Str cleaned, ReadAloudHighlightMap* newMap, int highlightBase,
                               Str errMsg) {
    if (len(cleaned) == 0) {
        logf("tts: StartText: empty cleaned text\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int cleanedLen = cleaned.len;
    int mapLen = newMap ? newMap->len : -1;
    dbgtts("StartText: cleanedLen=%d mapLen=%d highlightBase=%d\n", cleanedLen, mapLen, highlightBase);

    if (newMap) {
        if (!tab->readAloudHighlight) {
            tab->readAloudHighlight = new ReadAloudHighlightMap{};
        }
        ReadAloudHighlightFree(tab->readAloudHighlight);
        if (newMap->len > 0 && newMap->locs) {
            *tab->readAloudHighlight = *newMap;
            newMap->locs = nullptr;
            newMap->len = 0;
            newMap->cap = 0;
        } else {
            dbgtts("StartText: highlight map empty (len=%d locs=%p)\n", newMap->len, newMap->locs);
        }
    } else if (highlightBase == 0 && tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }

    str::ReplaceWithCopy(&tab->readAloudText, cleaned);
    tab->readAloudHighlightBase = highlightBase;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    tab->readAloudQueuedEnd = 0;
    tab->readAloudResumePos = -1;
    tab->readAloudAutoScroll = true;
    gReadAloudSessionTab = tab;
    ReadAloudSetSourceTab(tab);
    ReadAloudHighlightTimerStart(tab->win);

    if (!ReadAloudSpeakChunk(tab, errMsg)) {
        ReadAloudFinishSession(tab, tab->win);
        return;
    }
    ReadAloudPlaybackBarUpdateSession(tab);
}

static void ReadAloudStartFromViewportTop(WindowTab* tab, Str errMsg) {
    dbgtts("StartFromViewportTop\n");
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        logf("tts: StartFromViewportTop: not a fixed-layout document\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int startPage = 0;
    int startGlyph = 0;
    if (!ReadAloudGetViewportStart(dm, &startPage, &startGlyph)) {
        logf("tts: StartFromViewportTop: GetViewportStart failed\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromDocument(dm, startPage, startGlyph, &map, cleaned)) {
        logf("tts: StartFromViewportTop: BuildFromDocument failed (page=%d glyph=%d)\n", startPage, startGlyph);
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

static void ReadAloudStartFromSelection(WindowTab* tab, Str errMsg) {
    DisplayModel* dm = tab->AsFixed();
    if (!dm || dm->textSelection->result.len <= 0) {
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromTextSelection(dm->textSelection, &map, cleaned)) {
        bool isTextOnlySelection = false;
        TempStr text = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnlySelection);
        TempStr cleanedStr = CleanReadAloudTextTemp(text);
        ReadAloudStartText(tab, cleanedStr, nullptr, 0, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

void ReadAloudInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        logf("tts: InTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: InTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    bool isTextOnlySelection = false;
    TempStr text = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnlySelection);

    if (len(text) > 0 && isTextOnlySelection) {
        dbgtts("InTab: using selection path (len=%d)\n", len(text));
        tab->readAloudScope = WindowTab::ReadAloudScopeSmart;
        ReadAloudStartFromSelection(tab, Tr("No text available to read aloud"));
    } else {
        dbgtts("InTab: using viewport-top path (hasSelection=%d isTextOnly=%d)\n", len(text) > 0, isTextOnlySelection);
        tab->readAloudScope = WindowTab::ReadAloudScopeSmart;
        ReadAloudStartFromViewportTop(tab, Tr("No text available to read aloud"));
    }
}

static void ReadAloudStartFromCursor(WindowTab* tab, Point screenPt, Str errMsg) {
    dbgtts("StartFromCursor\n");
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        logf("tts: StartFromCursor: not a fixed-layout document\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int startPage = 0;
    int startGlyph = 0;
    if (!ReadAloudGetCursorStart(dm, screenPt, &startPage, &startGlyph)) {
        logf("tts: StartFromCursor: GetCursorStart failed\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromDocument(dm, startPage, startGlyph, &map, cleaned)) {
        logf("tts: StartFromCursor: BuildFromDocument failed (page=%d glyph=%d)\n", startPage, startGlyph);
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

static void ReadAloudFromCursorInTab(WindowTab* tab, Point screenPt) {
    if (!tab || !tab->win) {
        logf("tts: FromCursorInTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: FromCursorInTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeCursor;
    ReadAloudStartFromCursor(tab, screenPt, Tr("No text available to read aloud"));
}

void ReadAloudFromViewportTopInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        logf("tts: FromViewportTopInTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: FromViewportTopInTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeViewport;
    ReadAloudStartFromViewportTop(tab, Tr("No text available to read aloud"));
}

void ReadAloudSelectionInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: SelectionInTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeSelection;
    ReadAloudStartFromSelection(tab, Tr("No text available to read aloud"));
}

// true if read aloud was paused and can be resumed in this tab
bool CanContinueReadAloud(WindowTab* tab) {
    if (!tab || len(tab->readAloudText) == 0) {
        return false;
    }
    int pos = tab->readAloudResumePos;
    int maxPos = tab->readAloudHighlightBase + tab->readAloudText.len;
    return pos >= 0 && pos < maxPos;
}

void ReadAloudContinueInTab(WindowTab* tab) {
    if (!CanContinueReadAloud(tab) || !tab->win) {
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("tts: ContinueInTab: CopySelection permission denied\n");
        ReadAloudShowNotif(tab, Tr("This document doesn't allow copying text."));
        return;
    }

    int resumeInText = tab->readAloudResumePos - tab->readAloudHighlightBase;
    tab->readAloudChunkEnd = resumeInText;
    tab->readAloudChunkStart = resumeInText;
    tab->readAloudQueuedEnd = 0;
    tab->readAloudResumePos = -1;
    tab->readAloudAutoScroll = true;
    ReadAloudSetSourceTab(tab);
    ReadAloudHighlightTimerStart(tab->win);

    if (!ReadAloudSpeakChunk(tab, Tr("No text available to read aloud"))) {
        ReadAloudFinishSession(tab, tab->win);
        return;
    }
    ReadAloudPlaybackBarUpdateSession(tab);
}

WindowTab* GetReadAloudSourceTab() {
    return gReadAloudSourceTab;
}

// Voice selection menu
static TempStr TtsLangIdToLocaleNameTemp(Str lang) {
    if (len(lang) == 0) {
        return str::DupTemp(StrL("unknown"));
    }

    // Windows.Media.SpeechSynthesis voices report a locale name like "en-US",
    // SAPI voices a hex language id like "409"
    if (str::ContainsChar(lang, '-')) {
        return str::DupTemp(lang);
    }

    char* langZ = CStrTemp(lang);
    char* end = nullptr;
    unsigned long langId = strtoul(langZ, &end, 16);
    if (end == langZ || langId == 0) {
        return str::DupTemp(lang);
    }

    WCHAR localeName[LOCALE_NAME_MAX_LENGTH] = {};
    int n = LCIDToLocaleName((LCID)langId, localeName, dimof(localeName), 0);
    if (n <= 0) {
        return str::DupTemp(lang);
    }

    return ToUtf8Temp(localeName);
}

static void BuildReadAloudVoiceMenuItems(HMENU voiceMenu) {
    if (!voiceMenu) {
        return;
    }

    Str currentVoiceId = TtsGetVoiceId();

    UINT defaultFlags = MF_STRING;
    if (len(currentVoiceId) == 0) {
        defaultFlags |= MF_CHECKED;
    }

    AppendMenuW(voiceMenu, defaultFlags, CmdTtsVoiceDefault, L"System default");
    AppendMenuW(voiceMenu, MF_SEPARATOR, 0, nullptr);

    Vec<TtsVoiceInfo> voices = TtsGetVoices();

    Str lastLang = {};

    UINT cmd = CmdTtsVoiceFirst;
    for (TtsVoiceInfo& voice : voices) {
        if (cmd > CmdTtsVoiceLast) {
            break;
        }

        Str lang = len(voice.lang) == 0 ? StrL("") : voice.lang;

        if (lastLang && !str::EqI(lastLang, lang)) {
            AppendMenuW(voiceMenu, MF_SEPARATOR, 0, nullptr);
        }

        UINT flags = MF_STRING;
        if (str::Eq(voice.id, currentVoiceId)) {
            flags |= MF_CHECKED;
        }

        TempStr localeName = TtsLangIdToLocaleNameTemp(voice.lang);
        TempStr label = fmt("%s - %s", voice.name, localeName);
        AppendMenuW(voiceMenu, flags, cmd, CWStrTemp(label));

        lastLang = lang;
        cmd++;
    }

    TtsFreeVoices(voices);
    RemoveBadMenuSeparators(voiceMenu);
}

static void BuildReadAloudMenuItems(HMENU menu, MainWindow* win, bool includeCursorItem, bool canReadFromCursor) {
    WindowTab* currTab = win ? win->CurrentTab() : nullptr;
    bool isSpeaking = TtsIsSpeaking();
    bool canContinue = CanContinueReadAloud(currTab);
    bool hasSelection = currTab && win->showSelection && currTab->selectionOnPage && len(*currTab->selectionOnPage) > 0;

    if (isSpeaking) {
        AppendMenuW(menu, MF_STRING, CmdTtsMenuPauseReading, CWStrTemp(Tr("Pause Reading")));
    } else if (canContinue) {
        AppendMenuW(menu, MF_STRING, CmdTtsMenuContinueReading, CWStrTemp(Tr("Continue Reading")));
    }
    // always listed: the playback bar can be off-screen or lose z-order (issue #6053)
    UINT stopFlags = MF_STRING;
    if (!isSpeaking && !canContinue) {
        stopFlags |= MF_GRAYED;
    }
    AppendMenuW(menu, stopFlags, CmdTtsMenuStopReading, CWStrTemp(Tr("Stop Reading")));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CmdTtsMenuReadCurrentPage, CWStrTemp(Tr("Start Reading From Top")));
    if (includeCursorItem) {
        AppendMenuW(menu, canReadFromCursor ? MF_STRING : MF_STRING | MF_GRAYED, CmdTtsMenuReadFromCursor,
                    CWStrTemp(Tr("Start Reading From Cursor Position")));
    }
    AppendMenuW(menu, hasSelection ? MF_STRING : MF_STRING | MF_GRAYED, CmdTtsMenuReadSelection,
                CWStrTemp(Tr("Start Reading Selection")));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    HMENU voiceMenu = CreatePopupMenu();
    if (voiceMenu) {
        BuildReadAloudVoiceMenuItems(voiceMenu);
        AppendMenuW(menu, MF_POPUP | MF_STRING, (UINT_PTR)voiceMenu, CWStrTemp(Tr("Voice")));
    }

    HMENU speedMenu = CreatePopupMenu();
    if (speedMenu) {
        int currIdx = ReadAloudClosestSpeedIdx();
        for (int i = 0; i < dimofi(kReadAloudSpeeds); i++) {
            UINT flags = MF_STRING;
            if (i == currIdx) {
                flags |= MF_CHECKED;
            }
            TempStr label = ReadAloudSpeedLabelTemp(kReadAloudSpeeds[i]);
            AppendMenuW(speedMenu, flags, CmdTtsSpeedFirst + (UINT)i, CWStrTemp(label));
        }
        AppendMenuW(menu, MF_POPUP | MF_STRING, (UINT_PTR)speedMenu, CWStrTemp(Tr("Speed")));
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    UINT showFlags = MF_STRING;
    if (gSettings->toolbarShowReadAloud) {
        showFlags |= MF_CHECKED;
    }
    AppendMenuW(menu, showFlags, CmdToggleToolbarShowReadAloud, CWStrTemp(Tr("Show In Toolbar")));
}

void RebuildReadAloudMenu(MainWindow* win, HMENU menu, bool includeCursorItem, bool canReadFromCursor) {
    if (!menu || !win) {
        return;
    }
    MenuEmpty(menu);
    BuildReadAloudMenuItems(menu, win, includeCursorItem, canReadFromCursor);
    RemoveBadMenuSeparators(menu);
}

static void HandleReadAloudMenuSelection(MainWindow* win, UINT selected) {
    if (!win || selected == 0) {
        return;
    }

    WindowTab* currTab = win->CurrentTab();

    if (selected == CmdTtsMenuPauseReading) {
        ReadAloudStopRememberPos();
        ToolbarUpdateStateForWindow(win, true);
    } else if (selected == CmdTtsMenuStopReading) {
        ReadAloudPlaybackStop();
    } else if (selected == CmdTtsMenuReadCurrentPage) {
        if (currTab) {
            if (TtsIsSpeaking()) {
                TtsStop();
            }
            ReadAloudFromViewportTopInTab(currTab);
        }
    } else if (selected == CmdTtsMenuReadFromCursor) {
        if (currTab && win->contextMenuPtValid) {
            if (TtsIsSpeaking()) {
                TtsStop();
            }
            ReadAloudFromCursorInTab(currTab, win->contextMenuPt);
        }
    } else if (selected == CmdTtsMenuContinueReading) {
        if (TtsIsSpeaking()) {
            TtsStop();
        }
        ReadAloudContinueInTab(currTab);
    } else if (selected == CmdTtsMenuReadSelection) {
        if (TtsIsSpeaking()) {
            TtsStop();
        }
        ReadAloudSelectionInTab(currTab);
    } else if (selected == CmdTtsVoiceDefault) {
        if (TtsSetVoiceById(StrL(""))) {
            ReadAloudSaveVoicePref(StrL(""));
        }
    } else if (selected >= CmdTtsVoiceFirst && selected <= CmdTtsVoiceLast) {
        Vec<TtsVoiceInfo> voices = TtsGetVoices();
        int voiceIndex = (int)(selected - CmdTtsVoiceFirst);
        if (voiceIndex >= 0 && voiceIndex < len(voices)) {
            if (TtsSetVoiceById(voices[voiceIndex].id)) {
                ReadAloudSaveVoicePref(voices[voiceIndex].id);
            }
        }
        TtsFreeVoices(voices);
    } else if (selected >= CmdTtsSpeedFirst && selected <= CmdTtsSpeedLast) {
        int speedIndex = (int)(selected - CmdTtsSpeedFirst);
        if (speedIndex >= 0 && speedIndex < dimofi(kReadAloudSpeeds)) {
            ReadAloudSetSpeed(kReadAloudSpeeds[speedIndex]);
        }
    }
}

bool HandleReadAloudMenuCommand(MainWindow* win, int cmdId) {
    if (cmdId == CmdTtsVoiceDefault || (cmdId >= CmdTtsMenuReadCurrentPage && cmdId <= CmdTtsMenuStopReading) ||
        (cmdId >= CmdTtsVoiceFirst && cmdId <= CmdTtsVoiceLast) ||
        (cmdId >= CmdTtsSpeedFirst && cmdId <= CmdTtsSpeedLast)) {
        HandleReadAloudMenuSelection(win, (UINT)cmdId);
        return true;
    }
    return false;
}

// the menu shown by the dropdown arrow on the Read Aloud toolbar button
void ShowTtsVoiceMenu(MainWindow* win, Rect buttonScreen) {
    if (!win || buttonScreen.IsEmpty()) {
        return;
    }

    RECT rc = ToRECT(buttonScreen);

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    BuildReadAloudMenuItems(menu, win, false, false);

    UINT selected = (UINT)TrackPopupMenu(menu, TPM_RETURNCMD, rc.left, rc.bottom, 0, win->hwndFrame, nullptr);
    // the click that dismissed the menu is delivered to the toolbar afterwards;
    // this is what makes the toolbar ignore it instead of re-opening the menu
    ToolbarNoteDropdownClosed();

    DestroyMenu(menu);
    if (selected == 0) {
        return;
    }
    // the menu also carries real Cmd* ids (Show In Toolbar), which
    // HandleReadAloudMenuSelection knows nothing about - let the frame have them
    if (HandleReadAloudMenuCommand(win, (int)selected)) {
        return;
    }
    HwndSendCommand(win->hwndFrame, (int)selected);
}

// handles kWmTtsEvent posted by the tts backend
void ReadAloudOnTtsEvent(MainWindow* win) {
    TtsProcessEvents();
    ReadAloudAfterTtsEvents();

    WindowTab* tab = gReadAloudSourceTab;

    if (TtsIsSpeaking() && tab && tab->win) {
        HwndInvalidate(tab->win->hwndCanvas);
        // Tick, not UpdateSession: relayout rebuilds virt tops and
        // clears pressed, so Pause/Stop/Speed mouse-up is lost (issue #6106)
        dbgtts("event speaking pos=%d\n", TtsGetSpokenPosUtf8());
        ReadAloudPlaybackBarTick(tab->win);
    }

    // also gets here for word boundary events while still speaking;
    // only the end of speech needs handling
    if (TtsIsSpeaking() || !tab) {
        return;
    }
    dbgtts("event idle hasMore=%d chunkEnd=%d textLen=%d\n", (int)ReadAloudHasMoreChunks(tab), tab->readAloudChunkEnd,
           tab->readAloudText.len);
    if (ReadAloudHasMoreChunks(tab) && ReadAloudSpeakChunk(tab, Tr("No text available to read aloud"))) {
        return;
    }
    ReadAloudFinishSession(tab, win);
}

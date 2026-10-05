/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"
#include "base/Win.h"

#if OS_WIN
#include <sapi.h>
#pragma comment(lib, "sapi.lib")
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "SumatraConfig.h"
#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Translations.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayMode.h"
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
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "ReadAloud.h"

#include "SumatraLog.h"

// Read-aloud, top to bottom:
//   1. text-to-speech backend (SAPI 5 / AVSpeechSynthesizer)
//   2. the highlight of the words being spoken, on the canvas
//   3. the playback bar shown over the canvas
//   4. the session: what to read, chunking it, the menus that start it
//
// ng: orig prefers a Windows.Media.SpeechSynthesis (WinRT) backend and falls
// back to SAPI; only SAPI is ported, so there is one implementation and no
// backend switch. The map from the spoken bytes to page rectangles lives in
// src/ReadAloudHighlight.cpp so that test_util can link it.

// ng: orig enumerates the voices every time a menu pops up. gpui builds every
// menu model on every frame, so the list is enumerated once and kept; the
// engine's voices don't change while we run
static Vec<TtsVoiceInfo> gReadAloudVoices;
static bool gReadAloudVoicesValid = false;

static const Vec<TtsVoiceInfo>& ReadAloudVoices() {
    if (!gReadAloudVoicesValid) {
        gReadAloudVoicesValid = true;
        gReadAloudVoices = TtsGetVoices();
    }
    return gReadAloudVoices;
}

static void ReadAloudFreeVoiceCache() {
    TtsFreeVoices(gReadAloudVoices);
    gReadAloudVoicesValid = false;
}

// ---------------- 1. text-to-speech backend ----------------

#if OS_DARWIN

void TtsMacRelease();

void TtsRelease() {
    ReadAloudFreeVoiceCache();
    TtsMacRelease();
}

#elif OS_WASM

void TtsWasmRelease();

void TtsRelease() {
    ReadAloudFreeVoiceCache();
    TtsWasmRelease();
}

#elif OS_LINUX

void TtsLinuxRelease();

void TtsRelease() {
    ReadAloudFreeVoiceCache();
    TtsLinuxRelease();
}

#elif !OS_WIN

bool TtsIsAvailable() {
    return false;
}
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
void TtsRelease() {
    ReadAloudFreeVoiceCache();
}
bool TtsIsSpeaking() {
    return false;
}
int TtsGetSpokenPosUtf8() {
    return -1;
}
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
bool TtsOnEngineCrash(void*) {
    return false;
}
bool TtsTakeEngineCrash() {
    return false;
}
bool TtsEngineCrashed() {
    return false;
}
bool TtsTestEngineCrash() {
    return false;
}

#else

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

// dirs of the voice engine DLLs we loaded; read by the crash handler, so no heap
constexpr int kMaxSapiEngineDirs = 8;
static WCHAR gSapiEngineDirs[kMaxSapiEngineDirs][MAX_PATH];
static AtomicInt gSapiEngineDirCount = 0;
// set by the crash handler after it ended a crashed voice engine thread
static AtomicBool gSapiCrashed = 0;
static bool gSapiCrashTaken = false;

static void SapiClearQueued() {
    gSapiQueuedStreamNum = 0;
    wstr::Free(gSapiQueuedText);
    gSapiQueuedText = {};
}

// "C:\dir\file.dll" => "C:\dir"
static void CutFileName(WCHAR* path) {
    WCHAR* sep = wcsrchr(path, L'\\');
    if (sep) {
        *sep = 0;
    }
}

static void SapiAddEngineDir(const WCHAR* dir) {
    int count = AtomicIntGet(&gSapiEngineDirCount);
    for (int i = 0; i < count; i++) {
        if (_wcsicmp(gSapiEngineDirs[i], dir) == 0) {
            return;
        }
    }
    if (count >= kMaxSapiEngineDirs) {
        return;
    }
    // string before count: the crash handler reads without a lock
    wcscpy_s(gSapiEngineDirs[count], MAX_PATH, dir);
    AtomicIntSet(&gSapiEngineDirCount, count + 1);
}

// so a crash in the current voice's engine can end only its thread. Windows
// voices are skipped: their dir holds system DLLs every thread runs
static void SapiRememberEngineDir() {
    ISpObjectToken* token = nullptr;
    if (!gSapiVoice || FAILED(gSapiVoice->GetVoice(&token)) || !token) {
        return;
    }
    WCHAR* clsid = nullptr;
    HRESULT hr = token->GetStringValue(L"CLSID", &clsid);
    token->Release();
    if (FAILED(hr) || !clsid) {
        return;
    }

    WCHAR key[MAX_PATH];
    swprintf_s(key, L"CLSID\\%s\\InprocServer32", clsid);
    CoTaskMemFree(clsid);
    WCHAR path[MAX_PATH];
    DWORD cb = sizeof(path);
    if (RegGetValueW(HKEY_CLASSES_ROOT, key, nullptr, RRF_RT_REG_SZ, nullptr, path, &cb) != ERROR_SUCCESS) {
        return;
    }

    WCHAR winDir[MAX_PATH];
    UINT n = GetWindowsDirectoryW(winDir, MAX_PATH);
    if (n > 0 && n < MAX_PATH && _wcsnicmp(path, winDir, n) == 0) {
        return;
    }
    CutFileName(path);
    SapiAddEngineDir(path);
}

// SAPI may wait on locks the ended engine thread held: never call it again
// (gSapiVoice is leaked on purpose)
static bool SapiCrashed() {
    if (!AtomicBoolGet(&gSapiCrashed)) {
        return false;
    }
    if (gSapiVoice) {
        log(StrL("SapiCrashed: voice engine thread crashed, SAPI disabled\n"));
        gSapiVoice = nullptr;
        SapiClearQueued();
        gSapiStreamNum = 0;
        gSapiLastWordPos = 0;
        gTtsActive = false;
    }
    return true;
}

// Voice token lookup and metadata

static ISpObjectToken* SapiFindVoiceTokenById(Str voiceId) {
    if (len(voiceId) == 0) {
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
    if (SapiCrashed() || !gSapiVoice) {
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
    if (SapiCrashed() || !gSapiVoice) {
        return;
    }
    double rate = 10.0 * log((double)gTtsSpeed) / log(3.0);
    long rateAdjust = (long)(rate < 0 ? rate - 0.5 : rate + 0.5);
    gSapiVoice->SetRate(rateAdjust);
}

static bool SapiInit() {
    if (SapiCrashed()) {
        return false;
    }
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

    SapiRememberEngineDir();
    SapiApplySpeed();
    SapiSetNotify();
    return true;
}

static void SapiRelease() {
    if (SapiCrashed()) {
        return;
    }
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

    if (FAILED(hr)) {
        return false;
    }
    SapiRememberEngineDir();
    return true;
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
    if (SapiCrashed() || !gSapiVoice) {
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
    if (!SapiCrashed() && gSapiVoice) {
        gSapiVoice->Speak(nullptr, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
    }

    SapiClearQueued();
    gSapiStreamNum = 0;
    gSapiLastWordPos = 0;
}

//--- public interface

bool TtsIsAvailable() {
    return true;
}

void TtsSetNotifyWindow(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    gTtsNotifyHwnd = hwnd;
    gTtsNotifyMsg = msg;
    gTtsNotifyWParam = wp;
    gTtsNotifyLParam = lp;

    SapiSetNotify();
}

void TtsProcessEvents() {
    SapiProcessEvents();
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

    if (!SapiSpeak(textW)) {
        dbgtts("speak failed utf8=%d\n", text.len);
        return false;
    }

    wstr::Free(gTtsSpokenText);
    gTtsSpokenText = wstr::Dup(textW);
    gTtsActive = true;
    dbgtts("speak ok utf8=%d active=1\n", text.len);
    return true;
}

// Next utterance: SAPI-queue it without stopping playback.
bool TtsQueueUtf8(Str text) {
    if (len(text) == 0) {
        return false;
    }

    TempWStr textW = ToWStrTemp(text);
    if (len(textW) == 0) {
        return false;
    }

    bool ok = SapiQueue(textW);
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
    int wpos = (int)gSapiLastWordPos;

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
    dbgtts("stop active=%d\n", (int)gTtsActive);
    SapiStop();
    gTtsActive = false;
    gTtsQueuedStarted = false;
}

Vec<TtsVoiceInfo> TtsGetVoices() {
    Vec<TtsVoiceInfo> voices;
    SapiGetVoices(voices);
    TtsSortVoicesByLanguage(voices);
    dbgtts("voices sapi=%d\n", len(voices));
    return voices;
}

// Empty id is the system default.
bool TtsSetVoiceById(Str voiceId) {
    if (!SapiSetVoiceById(voiceId)) {
        return false;
    }

    str::ReplacePtr(&gTtsVoiceId, voiceId ? str::Dup(voiceId) : Str{});
    dbgtts("set-voice ok\n");
    return true;
}

Str TtsGetVoiceId() {
    return gTtsVoiceId;
}

// SAPI adjusts speech in progress
void TtsSetSpeed(float speed) {
    if (speed < kTtsSpeedMin) {
        speed = kTtsSpeedMin;
    } else if (speed > kTtsSpeedMax) {
        speed = kTtsSpeedMax;
    }
    gTtsSpeed = speed;

    // no-op if the backend is not initialized
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
    ReadAloudFreeVoiceCache();
    SapiRelease();

    gTtsActive = false;
    gTtsQueuedStarted = false;
    wstr::FreePtr(&gTtsSpokenText);

    str::Free(gTtsVoiceId);
    gTtsVoiceId = {};
}

// crash handler callback, on the crashed thread: no heap, no locks
bool TtsOnEngineCrash(void* faultAddr) {
    HMODULE mod = nullptr;
    DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExW(flags, (LPCWSTR)faultAddr, &mod)) {
        return false;
    }
    WCHAR dir[MAX_PATH];
    DWORD n = GetModuleFileNameW(mod, dir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return false;
    }
    CutFileName(dir);

    int count = AtomicIntGet(&gSapiEngineDirCount);
    for (int i = 0; i < count; i++) {
        if (_wcsicmp(gSapiEngineDirs[i], dir) != 0) {
            continue;
        }
        AtomicBoolSet(&gSapiCrashed, true);
        TtsPostNotifyMsg();
        return true;
    }
    return false;
}

// true once, on the UI thread, after a voice engine crash
bool TtsTakeEngineCrash() {
    if (!SapiCrashed() || gSapiCrashTaken) {
        return false;
    }
    gSapiCrashTaken = true;
    return true;
}

bool TtsEngineCrashed() {
    return AtomicBoolGet(&gSapiCrashed);
}

static DWORD WINAPI TtsTestCrashThread(void*) {
    CrashMe();
    return 0;
}

// -for-testing: crash like a voice engine, on a thread started outside our exe
// (SHCreateThread's starts in shcore) with our dir registered as an engine's
bool TtsTestEngineCrash() {
    if (!gForTesting) {
        return false;
    }
    WCHAR dir[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, dir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return false;
    }
    CutFileName(dir);
    SapiAddEngineDir(dir);
    return SHCreateThread(TtsTestCrashThread, nullptr, 0, nullptr) != FALSE;
}

#endif // OS_DARWIN / !OS_WIN / OS_WIN

// orig has this in AppSettings.cpp; see ReadAloud.h
bool ApplyReadAloudVoiceFromSettings() {
    if (!gSettings) {
        return false;
    }

    float speed = gSettings->readAloudSpeed;
    TtsSetSpeed(speed > 0 ? speed : 1.0f);

    Str voiceId = gSettings->readAloudVoiceId;
    if (len(voiceId) == 0) {
        TtsSetVoiceById(StrL(""));
        return false;
    }

    if (!TtsSetVoiceById(voiceId)) {
        logf("ApplyReadAloudVoiceFromSettings: voice '%s' not available, using system default\n", voiceId);
        str::ReplaceWithCopy(&gSettings->readAloudVoiceId, Str{});
        TtsSetVoiceById(StrL(""));
        return true;
    }
    return false;
}

// ---------------- 2. the highlight on the canvas ----------------

// ng: orig arms a WM_TIMER on the canvas; the shell's tick counts this down
void ReadAloudHighlightTimerStart(MainWindow* win) {
    if (!win) {
        return;
    }
    win->readAloudTimerOn = true;
    win->readAloudTimerLeftMs = kReadAloudHighlightDelayInMs;
}

void ReadAloudHighlightTimerStop(MainWindow* win) {
    if (!win) {
        return;
    }
    win->readAloudTimerOn = false;
}

static int gReadAloudPaintLogState = 0;

static void ReadAloudPaintLogOnce(int code, [[maybe_unused]] Str fmt) {
    if (gReadAloudPaintLogState == code) {
        return;
    }
    gReadAloudPaintLogState = code;
    dbgtts("%s\n", fmt);
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

    int pageNo = dm->FindPageNoByLoc(map->locs[absPos].pageLoc);
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
        int pageNo = dm->FindPageNoByLoc(loc.pageLoc);
        if (pageNo < 1) {
            continue;
        }
        Rect sr = dm->CvtToScreen(pageNo, ToRectF(ReadAloudByteLocToRect(loc)));
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

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return;
    }

    // In non-continuous modes (single page, facing, book view) the spoken word
    // can be on a page that isn't laid out, and scrolling cannot reach it, so
    // turn to that page first. The scrolling below still runs: zoomed in, the
    // word can be off screen on a page that is itself visible.
    if (!IsContinuous(dm->GetDisplayMode())) {
        int pageNo = 0;
        int pageCount = 0;
        if (ReadAloudGetProgressPage(tab, &pageNo, &pageCount) && !dm->PageVisible(pageNo)) {
            win->readAloudScrollFromCode = true;
            dm->GoToPage(pageNo, false);
            win->readAloudScrollFromCode = false;
        }
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

// ng: orig fills the underlines with Gfx::FillRects; CanvasFillRects is the
// same thing on a gpui PaintCtx
void PaintReadAloudHighlight(MainWindow* win, gp::PaintCtx* ctx) {
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
    Rect canvasRc = Rect(0, 0, win->canvasRc.dx, win->canvasRc.dy);

    Vec<Rect> sentenceRects;
    int sentStartAbs = 0;
    int sentEndAbs = 0;
    if (ReadAloudGetSentenceAbsRange(tab, wordStartAbs, wordEndAbs, &sentStartAbs, &sentEndAbs)) {
        ReadAloudAppendUnderlines(dm, canvasRc, map, sentStartAbs, sentEndAbs, minThick, 10, sentenceRects);
    }
    if (len(sentenceRects) > 0) {
        CanvasFillRects(ctx, sentenceRects.els, len(sentenceRects), kSentenceCol, 0xff, 0);
    }

    Vec<Rect> wordRects;
    ReadAloudAppendUnderlines(dm, canvasRc, map, wordStartAbs, wordEndAbs, DpiScale(3), 7, wordRects);
    if (len(wordRects) == 0) {
        ReadAloudPaintLogOnce(7, StrL("PaintHighlight: no screen rects for current word"));
        return;
    }
    DBG_TTS(dbgtts("paint word=%d..%d rects=%d at %d,%d %dx%d sentRects=%d\n", wordStartAbs, wordEndAbs, len(wordRects),
                   wordRects[0].x, wordRects[0].y, wordRects[0].dx, wordRects[0].dy, len(sentenceRects)));
    CanvasFillRects(ctx, wordRects.els, len(wordRects), kWordCol, 0xff, 0);
}

// ng: orig's kReadAloudHighlightTimerID handler in Canvas.cpp
void ReadAloudTick(MainWindow* win, int elapsedMs) {
    if (!win || !win->readAloudTimerOn) {
        return;
    }
    win->readAloudTimerLeftMs -= elapsedMs;
    if (win->readAloudTimerLeftMs > 0) {
        return;
    }
    win->readAloudTimerLeftMs = kReadAloudHighlightDelayInMs;

    if (!GetReadAloudSourceTab()) {
        ReadAloudHighlightTimerStop(win);
        return;
    }
#if OS_DARWIN || OS_WASM
    // These backends report completion through callbacks rather than a window
    // message, so the timer also advances or finishes the session.
    ReadAloudOnTtsEvent(win);
#else
    TtsProcessEvents();
    ReadAloudAfterTtsEvents();
#endif
    int pos = TtsGetSpokenPosUtf8();
    static int sLastTtsPos = -999;
    if (pos != sLastTtsPos) {
        sLastTtsPos = pos;
        DBG_TTS(dbgtts("tick pos=%d speaking=%d\n", pos, (int)TtsIsSpeaking()));
        DBG_TTS(dbgtts("bar %s", ReadAloudPlaybackBarStateTemp(nullptr)));
        ReadAloudUpdateAutoScroll(win);
        AppShellInvalidate(win);
    }
    ReadAloudPlaybackBarTick(win);
}

// ---------------- 3. playback bar ----------------

struct ReadAloudPlaybackBarView;

// ng: orig's bar is an owned WS_POPUP window with VirtButton / VirtSlider in
// it, kept aligned with the canvas by UpdateLayout / onWindowMoved. Here it is
// an element the canvas places at its own bottom edge, so all of that is gone.
struct ReadAloudPlaybackBar {
    WindowTab* sessionTab = nullptr;
    gp::Entity<ReadAloudPlaybackBarView> view;
    gp::SliderState speedSlider;
    // the slider reports a float; remember what we last pushed into it so a
    // speed change from a menu doesn't fight a drag
    int sliderIdx = -1;
    bool showResume = false;
    gp::Bounds pauseBounds;
    gp::Bounds stopBounds;
    gp::Bounds speedBounds;
    gp::Bounds speedLabelBounds;
    // orig's popup over the bottom of the canvas, where the platform can have
    // one (gui/ToolWindow.h); null: an element of the canvas
    ToolWindow* tw = nullptr;
    // the row in that window, as of its last frame
    gp::Bounds rowBounds{};
};

constexpr int kBarMargin = 8;
constexpr int kBarPadX = 12;
constexpr int kBarPadY = 6;
constexpr int kBtnGap = 8;

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

// the bar's buttons
constexpr int kBarCmdPause = 1;
constexpr int kBarCmdStop = 2;

struct ReadAloudPlaybackBarView {
    MainWindow* win = nullptr;

    static void OnCmd(ReadAloudPlaybackBarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
    static void OnSpeed(ReadAloudPlaybackBarView* self, gp::Ctx* cx, const gp::SliderEvent* ev);
    static void OnSpeedMove(ReadAloudPlaybackBarView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
    static void OnSpeedHover(ReadAloudPlaybackBarView* self, gp::Ctx* cx, const gp::HoverEvent* ev);
};

void ReadAloudPlaybackBarView::OnCmd(ReadAloudPlaybackBarView* self, gp::Ctx* cx, const gp::ClickEvent*,
                                     int64_t cmdId) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if ((int)cmdId == kBarCmdPause) {
        dbgtts("bar pause-click speaking=%d\n", (int)TtsIsSpeaking());
        ReadAloudPlaybackPauseOrResume();
    } else {
        dbgtts("bar stop-click\n");
        ReadAloudPlaybackStop();
    }
    gp::Notify(cx);
}

void ReadAloudPlaybackBarView::OnSpeed(ReadAloudPlaybackBarView* self, gp::Ctx* cx, const gp::SliderEvent*) {
    MainWindow* win = self->win;
    ReadAloudPlaybackBar* bar = IsMainWindowValidAndNotClosing(win) ? win->readAloudPlaybackBar : nullptr;
    if (!bar) {
        return;
    }
    int idx = limitValue((int)(bar->speedSlider.value.End() + 0.5f), 0, ReadAloudSpeedCount() - 1);
    if (idx != bar->sliderIdx) {
        bar->sliderIdx = idx;
        ReadAloudSetSpeedIdx(idx);
    }
    gp::Notify(cx);
}

// the bar's own window, when the event is in it
static ToolWindow* BarWindowOf(MainWindow* win, gp::Ctx* cx) {
    ReadAloudPlaybackBar* bar = IsMainWindowValidAndNotClosing(win) ? win->readAloudPlaybackBar : nullptr;
    if (!bar || !bar->tw || ToolWindowGpui(bar->tw) != cx->win) {
        return nullptr;
    }
    return bar->tw;
}

void ReadAloudPlaybackBarView::OnSpeedMove(ReadAloudPlaybackBarView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    float pct = ev->el.w > 0 ? (ev->x - ev->el.x) / ev->el.w : 0;
    int idx = limitValue((int)(pct * (float)(ReadAloudSpeedCount() - 1) + 0.5f), 0, ReadAloudSpeedCount() - 1);
    TempStr tip = ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx));
    ToolWindow* tw = BarWindowOf(self->win, cx);
    if (!tw) {
        HoverTooltipShow(cx, tip, ev->el);
        return;
    }
    // the bar's window is as high as the bar: the tooltip is the frame's
    Point off = ToolWindowOffsetInOwner(tw);
    gp::Bounds at = ev->el;
    at.x += (float)off.x;
    at.y += (float)off.y;
    HoverTooltipShowIn(cx->app, self->win->gpuiWin, tip, at);
}

void ReadAloudPlaybackBarView::OnSpeedHover(ReadAloudPlaybackBarView* self, gp::Ctx* cx, const gp::HoverEvent* ev) {
    if (ev->hovered) {
        return;
    }
    if (BarWindowOf(self->win, cx)) {
        HoverTooltipHideIn(self->win->gpuiWin);
        return;
    }
    HoverTooltipHide(cx);
}

// --- orig's popup: the bar in a window of its own ---------------------------

// the row's height until its first frame says: a button, the padding, the edge
constexpr float kBarGuessDy = 38;

static WindowTab* BarSessionTab(MainWindow* win) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    WindowTab* tab = bar ? bar->sessionTab : nullptr;
    return tab && tab->win == win ? tab : nullptr;
}

// orig's ReadAloudPlaybackBar::UpdateLayout
static Rect ReadAloudBarPlace(MainWindow* win, ToolWindow*) {
    if (!BarSessionTab(win)) {
        return {};
    }
    ReadAloudPlaybackBar* bar = win->readAloudPlaybackBar;
    float dy = bar->rowBounds.h > 0 ? bar->rowBounds.h : kBarGuessDy;
    return ToolWindowDockedBarRect(win, kBarMargin, dy);
}

static gp::El* ReadAloudBarRowBuild(MainWindow* win, gp::Ctx* cx, bool ownWindow);

static gp::El* ReadAloudBarToolBuild(MainWindow* win, gp::Ctx* cx) {
    return ReadAloudBarRowBuild(win, cx, true);
}

static void ReadAloudBarToolOnClosed(MainWindow* win) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (bar && bar->tw && !ToolWindowIsLive(bar->tw)) {
        bar->tw = nullptr;
    }
}

// the window is there while the bar has a session to show
static void ReadAloudBarSyncWindow(MainWindow* win) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar) {
        return;
    }
    bool want = BarSessionTab(win) != nullptr && ToolWindowsAvailable();
    if (!want) {
        if (bar->tw) {
            ToolWindowClose(bar->tw);
            bar->tw = nullptr;
        }
        return;
    }
    if (bar->tw && ToolWindowIsLive(bar->tw)) {
        ToolWindowInvalidate(bar->tw);
        return;
    }
    Rect r = ReadAloudBarPlace(win, nullptr);
    if (r.IsEmpty()) {
        return;
    }
    ToolWindowDesc desc = ToolWindowDockedDesc("readaloudbar");
    desc.build = ReadAloudBarToolBuild;
    desc.place = ReadAloudBarPlace;
    desc.onClosed = ReadAloudBarToolOnClosed;
    bar->tw = ToolWindowOpen(desc, win, r);
}

static ReadAloudPlaybackBar* ReadAloudPlaybackBarEnsure(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    if (!win->readAloudPlaybackBar) {
        auto* bar = new ReadAloudPlaybackBar();
        bar->speedSlider = gp::SliderStateNew(0, (float)(ReadAloudSpeedCount() - 1),
                                              gp::SliderSingle((float)ReadAloudClosestSpeedIdx()));
        win->readAloudPlaybackBar = bar;
    }
    return win->readAloudPlaybackBar;
}

void ReadAloudPlaybackBarDestroy(MainWindow* win) {
    if (!win || !win->readAloudPlaybackBar) {
        return;
    }
    if (win->readAloudPlaybackBar->tw) {
        ToolWindowClose(win->readAloudPlaybackBar->tw);
    }
    delete win->readAloudPlaybackBar;
    win->readAloudPlaybackBar = nullptr;
}

void ReadAloudPlaybackBarHide(MainWindow* win) {
    if (!win || !win->readAloudPlaybackBar) {
        return;
    }
    win->readAloudPlaybackBar->sessionTab = nullptr;
    ReadAloudBarSyncWindow(win);
    AppShellInvalidate(win);
}

// the tab is going away; the bar has no reason to exist without it
void ReadAloudPlaybackBarForgetTab(MainWindow* win, WindowTab* tab) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar || bar->sessionTab != tab) {
        return;
    }
    bar->sessionTab = nullptr;
    ReadAloudBarSyncWindow(win);
    AppShellInvalidate(win);
}

// the highlight tick: refresh the Pause / page text
void ReadAloudPlaybackBarTick(MainWindow* win) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar || !bar->sessionTab) {
        return;
    }
    if (bar->tw) {
        // the bar is all that changes
        ToolWindowInvalidate(bar->tw);
        return;
    }
    AppShellInvalidate(win);
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
    bar->sessionTab = tab;
    ReadAloudBarSyncWindow(tab->win);
    AppShellInvalidate(tab->win);

    // hide bars on other windows
    for (MainWindow* win : gWindows) {
        if (win != tab->win) {
            ReadAloudPlaybackBarHide(win);
        }
    }
}

static gp::El* BarButton(ReadAloudPlaybackBar* bar, gp::Ctx* cx, Str id, Str label, int cmdId, gp::Bounds* boundsOut) {
    gp::El* el = gpc::Button::New(cx, GpuiDup(cx->a, id))
                     ->WithSize(gp::UiSize::Small)
                     ->Label(GpuiDup(cx->a, label))
                     ->OnClick(gp::ListenTo(bar->view, &ReadAloudPlaybackBarView::OnCmd, (int64_t)cmdId))
                     ->IntoEl();
    el->BoundsOut(boundsOut);
    return el;
}

// the bar in the canvas; in a window of its own it is not the frame's
gp::El* ReadAloudPlaybackBarBuild(MainWindow* win, gp::Ctx* cx) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    if (!bar || bar->tw) {
        return nullptr;
    }
    return ReadAloudBarRowBuild(win, cx, false);
}

static gp::El* ReadAloudBarRowBuild(MainWindow* win, gp::Ctx* cx, bool ownWindow) {
    ReadAloudPlaybackBar* bar = win ? win->readAloudPlaybackBar : nullptr;
    WindowTab* tab = bar ? bar->sessionTab : nullptr;
    if (!tab || tab->win != win) {
        return nullptr;
    }
    if (!bar->view.IsValid()) {
        bar->view = gp::EntityNewState<ReadAloudPlaybackBarView>(cx->app);
    }
    auto* view = (ReadAloudPlaybackBarView*)gp::EntityGet(cx->app, bar->view.id);
    view->win = win;

    // a speed set from a menu has to reach the slider, but not while the user
    // is dragging it (orig's VirtSlider::IsAdjusting)
    if (!bar->speedSlider.dragging) {
        int idx = ReadAloudClosestSpeedIdx();
        if (idx != bar->sliderIdx) {
            bar->sliderIdx = idx;
            gp::SliderSetValue(&bar->speedSlider, gp::SliderSingle((float)idx));
        }
    }

    bar->showResume = CanContinueReadAloud(tab) && !TtsIsSpeaking();
    float margin = (float)DpiScale(kBarMargin);
    float gap = (float)DpiScale(kBtnGap);
    const gp::Theme& th = gp::ThemeNow(cx->app);

    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->Gap(gap)
                      ->PadX((float)DpiScale(kBarPadX))
                      ->PadY((float)DpiScale(kBarPadY))
                      ->Bg(ToGpui(ThemeNotificationsBackgroundColor()))
                      ->Border(1, ToGpui(kColGray));
    if (ownWindow) {
        // the window is the bar: orig's popup, as wide as the canvas less the
        // margins
        row->W(gp::kFill)->Shrink0()->BoundsOut(&bar->rowBounds);
    } else {
        row->Absolute()->Left(margin)->Right(margin)->Bottom(margin);
    }
    row->Child(BarButton(bar, cx, StrL("ra-pause"), bar->showResume ? Tr("Resume") : Tr("Pause"), kBarCmdPause,
                         &bar->pauseBounds));
    row->Child(BarButton(bar, cx, StrL("ra-stop"), Tr("Stop"), kBarCmdStop, &bar->stopBounds));
    // ng: the listener goes to the component; Slider::IntoEl() replaces the
    // one set on the state
    row->Child(gpc::Slider::New(cx, GStrL("ra-speed"), &bar->speedSlider)
                   ->OnChange(gp::ListenTo(bar->view, &ReadAloudPlaybackBarView::OnSpeed))
                   ->W(160)
                   ->IntoEl()
                   ->OnMouseMove(gp::ListenTo(bar->view, &ReadAloudPlaybackBarView::OnSpeedMove))
                   ->OnHover(gp::ListenTo(bar->view, &ReadAloudPlaybackBarView::OnSpeedHover))
                   ->BoundsOut(&bar->speedBounds));
    TempStr speedLabel = ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(ReadAloudClosestSpeedIdx()));
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, speedLabel))
                   ->Font(13)
                   ->Fg(th.foreground)
                   ->Shrink0()
                   ->BoundsOut(&bar->speedLabelBounds));
    row->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gp::TextEl(cx->a, GpuiDup(cx->a, ReadAloudPlaybackBarTextTemp(tab)))->Font(13)->Fg(th.foreground)));
    return row;
}

// ng: for -dbg-control. "show" puts the bar up for the current tab as a
// paused session with nothing spoken, so a test can look at it and use its
// slider and Stop without a sound; "hide" takes it down again
TempStr ReadAloudPlaybackBarTestTemp(Str action, int* exitCodeOut) {
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (str::Eq(action, StrL("show")) && tab) {
        if (len(tab->readAloudText) == 0) {
            str::ReplaceWithCopy(&tab->readAloudText, StrL("test"));
            tab->readAloudHighlightBase = 0;
            tab->readAloudResumePos = 0;
        }
        ReadAloudPlaybackBarUpdateSession(tab);
    } else if (str::Eq(action, StrL("hide")) && win) {
        ReadAloudPlaybackBarHide(win);
    }
    return ReadAloudPlaybackBarStateTemp(exitCodeOut);
}

TempStr ReadAloudPlaybackBarStateTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    out.Append(fmt("voices=%d speaking=%d\n", len(ReadAloudVoices()), (int)TtsIsSpeaking()));

    if (len(gWindows) == 0) {
        out.Append(StrL("NOTREADY no-window\n"));
        return finish(2);
    }
    MainWindow* win = gWindows[0];
    ReadAloudPlaybackBar* bar = win->readAloudPlaybackBar;
    if (!bar || !bar->sessionTab) {
        out.Append(StrL("NOTREADY no-bar\n"));
        return finish(2);
    }

    Rect pause = FromGpui(bar->pauseBounds);
    Rect stop = FromGpui(bar->stopBounds);
    Rect speed = FromGpui(bar->speedBounds);
    Rect speedLab = FromGpui(bar->speedLabelBounds);
    int idx = ReadAloudClosestSpeedIdx();
    out.Append(fmt("OK visible=1 resume=%d\n", (int)bar->showResume));
    out.Append(fmt("pause=%d,%d,%d,%d\n", pause.x, pause.y, pause.dx, pause.dy));
    out.Append(fmt("stop=%d,%d,%d,%d\n", stop.x, stop.y, stop.dx, stop.dy));
    out.Append(fmt("speed=%d,%d,%d,%d\n", speed.x, speed.y, speed.dx, speed.dy));
    out.Append(fmt("speedLabel=%d,%d,%d,%d\n", speedLab.x, speedLab.y, speedLab.dx, speedLab.dy));
    out.Append(fmt("speedIdx=%d speedCount=%d label=%s\n", idx, ReadAloudSpeedCount(),
                   ReadAloudSpeedLabelTemp(ReadAloudSpeedAt(idx))));
    out.Append(fmt("status=%s\n", ReadAloudPlaybackBarTextTemp(bar->sessionTab)));
    return finish(0);
}

// ---------------- 4. read-aloud session ----------------

static WindowTab* gReadAloudSourceTab = nullptr;
static WindowTab* gReadAloudSessionTab = nullptr;

static void ReadAloudClearSourceTab();
static void ReadAloudShowNotif(WindowTab* tab, Str msg);

static void ReadAloudSaveVoicePref(Str voiceId) {
    if (!gSettings) {
        return;
    }
    str::ReplaceWithCopy(&gSettings->readAloudVoiceId, voiceId);
    ScheduleSaveSettings();
}

// speak in chunks so a whole-document request doesn't go to the engine at once
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
    if (!TtsQueueUtf8(chunk) || !IsWindowTabValid(tab)) {
        return;
    }
    tab->readAloudQueuedEnd = end;
    dbgtts("queue-next %d..%d of %d\n", start, end, tab->readAloudText.len);
}

// Promote a prefetched chunk and start the next prefetch.
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

// a voice engine thread crashed and was ended: stop reading, forget that voice
static void ReadAloudOnEngineCrash() {
    if (!TtsTakeEngineCrash()) {
        return;
    }
    WindowTab* tab = GetReadAloudSourceTab();
    ReadAloudPlaybackStop();
    ReadAloudSaveVoicePref({});
    ReadAloudFreeVoiceCache();
    TtsSetVoiceById(StrL(""));
    if (tab && tab->win) {
        ReadAloudShowNotif(tab, Tr("Read aloud voice crashed and was turned off"));
    }
}

void ReadAloudAfterTtsEvents() {
    ReadAloudOnEngineCrash();
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
        AppShellInvalidate(tab->win);
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

enum class SpeakChunkResult {
    Ok,
    Failed,
    TabGone,
};

static SpeakChunkResult ReadAloudSpeakChunk(WindowTab* tab, Str errMsg) {
    if (!tab || len(tab->readAloudText) == 0) {
        return SpeakChunkResult::Failed;
    }

    int start = tab->readAloudChunkEnd;
    int textLen = tab->readAloudText.len;
    int end = ReadAloudFindChunkEnd(tab->readAloudText, start, kReadAloudMaxChunkLen);
    if (start >= end) {
        return SpeakChunkResult::Failed;
    }

    TempStr chunk = str::DupTemp(Str(tab->readAloudText.s + start, end - start));
    // A speech backend may pump UI messages, allowing the tab to be closed
    // before the call returns. Its destructor has already reset the session.
    bool ok = TtsSpeakUtf8(chunk);
    if (!IsWindowTabValid(tab)) {
        if (ok) {
            TtsStop();
        }
        return SpeakChunkResult::TabGone;
    }
    if (!ok) {
        logf("tts: SpeakChunk: TtsSpeakUtf8 failed\n");
        dbgtts("chunk speak failed %d..%d of %d\n", start, end, textLen);
        ReadAloudShowNotif(tab, errMsg);
        return SpeakChunkResult::Failed;
    }
    dbgtts("chunk %d..%d of %d mapBase=%d\n", start, end, textLen, tab->readAloudHighlightBase);

    tab->readAloudChunkStart = start;
    tab->readAloudChunkEnd = end;
    tab->readAloudQueuedEnd = 0;
    ReadAloudQueueNext(tab);
    if (!IsWindowTabValid(tab)) {
        TtsStop();
        return SpeakChunkResult::TabGone;
    }
    ToolbarUpdateStateForWindow(tab->win, true);
    AppShellInvalidate(tab->win);
    return SpeakChunkResult::Ok;
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
        AppShellInvalidate(tab->win);
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
        AppShellInvalidate(tab->win);
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
// slider on the playback bar
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
}

void ReadAloudSetSpeedIdx(int idx) {
    ReadAloudSetSpeed(ReadAloudSpeedAt(idx));
}

// dir is +1 (next speed) or -1 (previous speed), wraps around
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
    args.win = tab->win;
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

    SpeakChunkResult res = ReadAloudSpeakChunk(tab, errMsg);
    if (res == SpeakChunkResult::TabGone) {
        return;
    }
    if (res == SpeakChunkResult::Failed) {
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

void ReadAloudFromCursorInTab(WindowTab* tab, Point screenPt) {
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

    SpeakChunkResult res = ReadAloudSpeakChunk(tab, Tr("No text available to read aloud"));
    if (res == SpeakChunkResult::TabGone) {
        return;
    }
    if (res == SpeakChunkResult::Failed) {
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

    // SAPI voices report a hex language id like "409"
    if (str::ContainsChar(lang, '-')) {
        return str::DupTemp(lang);
    }

#if OS_WIN
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
#else
    return str::DupTemp(lang);
#endif
}

// ng: orig appends to an HMENU; the port fills a MenuModel the shell turns
// into a gpui PopupMenu
static void MenuAppendItem(MenuModel* menu, Str title, int cmdId, bool enabled, bool checked) {
    MenuAppendString(menu, title, cmdId);
    MenuItemModel& it = menu->items[menu->items.len - 1];
    it.disabled = !enabled;
    it.checked = checked;
}

static void BuildReadAloudVoiceMenuItems(MenuModel* voiceMenu) {
    Str currentVoiceId = TtsGetVoiceId();

    MenuAppendItem(voiceMenu, StrL("System default"), CmdTtsVoiceDefault, true, len(currentVoiceId) == 0);
    MenuAppendSeparator(voiceMenu);

    const Vec<TtsVoiceInfo>& voices = ReadAloudVoices();

    Str lastLang = {};

    int cmd = CmdTtsVoiceFirst;
    for (const TtsVoiceInfo& voice : voices) {
        if (cmd > CmdTtsVoiceLast) {
            break;
        }

        Str lang = len(voice.lang) == 0 ? StrL("") : voice.lang;

        if (lastLang && !str::EqI(lastLang, lang)) {
            MenuAppendSeparator(voiceMenu);
        }

        TempStr localeName = TtsLangIdToLocaleNameTemp(voice.lang);
        TempStr label = fmt("%s - %s", voice.name, localeName);
        MenuAppendItem(voiceMenu, label, cmd, true, str::Eq(voice.id, currentVoiceId));

        lastLang = lang;
        cmd++;
    }

    RemoveBadMenuSeparators(voiceMenu);
}

static void BuildReadAloudMenuItems(MenuModel* menu, MainWindow* win, bool includeCursorItem, bool canReadFromCursor) {
    WindowTab* currTab = win ? win->CurrentTab() : nullptr;
    bool isSpeaking = TtsIsSpeaking();
    bool canContinue = CanContinueReadAloud(currTab);
    bool hasSelection = currTab && win->showSelection && currTab->selectionOnPage && len(*currTab->selectionOnPage) > 0;

    if (isSpeaking) {
        MenuAppendItem(menu, Tr("Pause Reading"), CmdTtsMenuPauseReading, true, false);
    } else if (canContinue) {
        MenuAppendItem(menu, Tr("Continue Reading"), CmdTtsMenuContinueReading, true, false);
    }
    // always listed: the playback bar can be off-screen or lose z-order (issue #6053)
    MenuAppendItem(menu, Tr("Stop Reading"), CmdTtsMenuStopReading, isSpeaking || canContinue, false);
    MenuAppendSeparator(menu);
    MenuAppendItem(menu, Tr("Start Reading From Top"), CmdTtsMenuReadCurrentPage, true, false);
    if (includeCursorItem) {
        MenuAppendItem(menu, Tr("Start Reading From Cursor Position"), CmdTtsMenuReadFromCursor, canReadFromCursor,
                       false);
    }
    MenuAppendItem(menu, Tr("Start Reading Selection"), CmdTtsMenuReadSelection, hasSelection, false);
    MenuAppendSeparator(menu);

    MenuModel* voiceMenu = MenuAppendSubmenu(menu, Tr("Voice"));
    BuildReadAloudVoiceMenuItems(voiceMenu);

    MenuModel* speedMenu = MenuAppendSubmenu(menu, Tr("Speed"));
    int currIdx = ReadAloudClosestSpeedIdx();
    for (int i = 0; i < dimofi(kReadAloudSpeeds); i++) {
        TempStr label = ReadAloudSpeedLabelTemp(kReadAloudSpeeds[i]);
        MenuAppendItem(speedMenu, label, CmdTtsSpeedFirst + i, true, i == currIdx);
    }

    MenuAppendSeparator(menu);
    MenuAppendItem(menu, Tr("Show In Toolbar"), CmdToggleToolbarShowReadAloud, true,
                   gSettings && gSettings->toolbarShowReadAloud);
}

[[maybe_unused]] static TempStr ReadAloudMenuRowsTemp(MenuModel* menu) {
    str::Builder b;
    for (const MenuItemModel& it : menu->items) {
        if (len(b) > 0) {
            b.Append(StrL(", "));
        }
        if (it.separator) {
            b.Append(StrL("---"));
            continue;
        }
        b.Append(it.title);
        if (it.submenu) {
            b.Append(fmt("> (%s)", ReadAloudMenuRowsTemp(it.submenu)));
        }
        if (it.disabled) {
            b.Append(StrL("(off)"));
        }
        if (it.checked) {
            b.Append(StrL("(on)"));
        }
    }
    return ToStrTemp(b);
}

void RebuildReadAloudMenu(MainWindow* win, MenuModel* menu, bool includeCursorItem, bool canReadFromCursor) {
    if (!menu || !win) {
        return;
    }
    MenuEmpty(menu);
    if (!TtsIsAvailable()) {
        return;
    }
    BuildReadAloudMenuItems(menu, win, includeCursorItem, canReadFromCursor);
    RemoveBadMenuSeparators(menu);
    DBG_TTS({
        static int lastLen = -1;
        TempStr rows = ReadAloudMenuRowsTemp(menu);
        if (len(rows) != lastLen) {
            lastLen = len(rows);
            dbgtts("menu: %s\n", rows);
        }
    });
}

static void HandleReadAloudMenuSelection(MainWindow* win, int selected) {
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
        const Vec<TtsVoiceInfo>& voices = ReadAloudVoices();
        int voiceIndex = selected - CmdTtsVoiceFirst;
        if (voiceIndex >= 0 && voiceIndex < len(voices)) {
            if (TtsSetVoiceById(voices[voiceIndex].id)) {
                ReadAloudSaveVoicePref(voices[voiceIndex].id);
            }
        }
    } else if (selected >= CmdTtsSpeedFirst && selected <= CmdTtsSpeedLast) {
        int speedIndex = selected - CmdTtsSpeedFirst;
        if (speedIndex >= 0 && speedIndex < dimofi(kReadAloudSpeeds)) {
            ReadAloudSetSpeed(kReadAloudSpeeds[speedIndex]);
        }
    }
    AppShellInvalidate(win);
}

bool HandleReadAloudMenuCommand(MainWindow* win, int cmdId) {
    if (cmdId == CmdTtsVoiceDefault || (cmdId >= CmdTtsMenuReadCurrentPage && cmdId <= CmdTtsMenuStopReading) ||
        (cmdId >= CmdTtsVoiceFirst && cmdId <= CmdTtsVoiceLast) ||
        (cmdId >= CmdTtsSpeedFirst && cmdId <= CmdTtsSpeedLast)) {
        HandleReadAloudMenuSelection(win, cmdId);
        return true;
    }
    return false;
}

// handles kWmTtsEvent posted by the tts backend
void ReadAloudOnTtsEvent(MainWindow* win) {
    TtsProcessEvents();
    ReadAloudAfterTtsEvents();

    WindowTab* tab = gReadAloudSourceTab;

    if (TtsIsSpeaking() && tab && tab->win) {
        dbgtts("event speaking pos=%d\n", TtsGetSpokenPosUtf8());
        AppShellInvalidate(tab->win);
        ReadAloudPlaybackBarTick(tab->win);
    }

    // also gets here for word boundary events while still speaking;
    // only the end of speech needs handling
    if (TtsIsSpeaking() || !tab) {
        return;
    }
    dbgtts("event idle hasMore=%d chunkEnd=%d textLen=%d\n", (int)ReadAloudHasMoreChunks(tab), tab->readAloudChunkEnd,
           tab->readAloudText.len);
    if (ReadAloudHasMoreChunks(tab)) {
        SpeakChunkResult res = ReadAloudSpeakChunk(tab, Tr("No text available to read aloud"));
        if (res != SpeakChunkResult::Failed) {
            return;
        }
    }
    ReadAloudFinishSession(tab, win);
}

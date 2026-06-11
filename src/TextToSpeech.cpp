#include "utils/BaseUtil.h"

#include <sapi.h>
#include <TextToSpeech.h>

#pragma comment(lib, "sapi.lib")

static ISpVoice* gTtsVoice = nullptr;
static bool gTtsCoInitialized = false;
static bool gTtsActive = false;
static ULONG gTtsStreamNum = 0;

static char* gTtsVoiceId = nullptr;

static HWND gTtsNotifyHwnd = nullptr;
static UINT gTtsNotifyMsg = 0;
static WPARAM gTtsNotifyWParam = 0;
static LPARAM gTtsNotifyLParam = 0;

static char* TtsGetVoiceLanguage(ISpObjectToken* token);
static const char* TtsVoiceLangForSort(const TtsVoiceInfo& voice);
static bool TtsVoiceLess(const TtsVoiceInfo& a, const TtsVoiceInfo& b);
static void TtsSortVoicesByLanguage(Vec<TtsVoiceInfo>& voices);

// Voice token lookup and metadata

static ISpObjectToken* TtsFindVoiceTokenById(const char* voiceId) {
    if (str::IsEmpty(voiceId)) {
        return nullptr;
    }

    TempWStr wantedId = ToWStrTemp(voiceId);
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

static char* TtsGetVoiceLanguage(ISpObjectToken* token) {
    if (!token) {
        return nullptr;
    }

    ISpDataKey* attributes = nullptr;
    HRESULT hr = token->OpenKey(L"Attributes", &attributes);
    if (FAILED(hr) || !attributes) {
        return nullptr;
    }

    WCHAR* langW = nullptr;
    hr = attributes->GetStringValue(L"Language", &langW);
    attributes->Release();

    if (FAILED(hr) || !langW) {
        return nullptr;
    }

    char* lang = str::Dup(ToUtf8Temp(langW));
    CoTaskMemFree(langW);
    return lang;
}

static const char* TtsVoiceLangForSort(const TtsVoiceInfo& voice) {
    return str::IsEmpty(voice.lang) ? "ffff" : voice.lang;
}

static bool TtsVoiceLess(const TtsVoiceInfo& a, const TtsVoiceInfo& b) {
    int langCmp = lstrcmpiA(TtsVoiceLangForSort(a), TtsVoiceLangForSort(b));
    if (langCmp != 0) {
        return langCmp < 0;
    }

    return lstrcmpiA(a.name ? a.name : "", b.name ? b.name : "") < 0;
}

static void TtsSortVoicesByLanguage(Vec<TtsVoiceInfo>& voices) {
    for (int i = 1; i < voices.Size(); i++) {
        TtsVoiceInfo value = voices[i];
        int j = i - 1;

        while (j >= 0 && TtsVoiceLess(value, voices[j])) {
            voices[j + 1] = voices[j];
            j--;
        }

        voices[j + 1] = value;
    }
}

// Lifecycle

static bool TtsInit() {
    if (gTtsVoice) {
        return true;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        gTtsCoInitialized = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&gTtsVoice);
    if (FAILED(hr)) {
        gTtsVoice = nullptr;

        if (gTtsCoInitialized) {
            CoUninitialize();
            gTtsCoInitialized = false;
        }

        return false;
    }

    if (!str::IsEmpty(gTtsVoiceId)) {
        ISpObjectToken* token = TtsFindVoiceTokenById(gTtsVoiceId);
        if (token) {
            gTtsVoice->SetVoice(token);
            token->Release();
        }
    }

    if (gTtsNotifyHwnd && gTtsNotifyMsg) {
        TtsSetNotifyWindow(gTtsNotifyHwnd, gTtsNotifyMsg, gTtsNotifyWParam, gTtsNotifyLParam);
    }

    return true;
}

void TtsRelease() {
    if (gTtsVoice) {
        gTtsVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
        gTtsVoice->Release();
        gTtsVoice = nullptr;
    }

    gTtsActive = false;
    gTtsStreamNum = 0;

    if (gTtsCoInitialized) {
        CoUninitialize();
        gTtsCoInitialized = false;
    }

    free(gTtsVoiceId);
    gTtsVoiceId = nullptr;
}

// Voice enumeration and selection

Vec<TtsVoiceInfo> TtsGetVoices() {
    Vec<TtsVoiceInfo> voices;

    if (!TtsInit()) {
        return voices;
    }

    ISpObjectTokenCategory* category = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
                                  (void**)&category);
    if (FAILED(hr) || !category) {
        return voices;
    }

    hr = category->SetId(SPCAT_VOICES, FALSE);
    if (FAILED(hr)) {
        category->Release();
        return voices;
    }

    IEnumSpObjectTokens* enumTokens = nullptr;
    hr = category->EnumTokens(nullptr, nullptr, &enumTokens);
    category->Release();

    if (FAILED(hr) || !enumTokens) {
        return voices;
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
            info.lang = TtsGetVoiceLanguage(token);
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
    TtsSortVoicesByLanguage(voices);
    return voices;
}

bool TtsSetVoiceById(const char* voiceId) {
    if (!TtsInit()) {
        return false;
    }

    HRESULT hr = E_FAIL;

    if (str::IsEmpty(voiceId)) {
        hr = gTtsVoice->SetVoice(nullptr);
    } else {
        ISpObjectToken* token = TtsFindVoiceTokenById(voiceId);
        if (!token) {
            return false;
        }

        hr = gTtsVoice->SetVoice(token);
        token->Release();
    }

    if (FAILED(hr)) {
        return false;
    }

    str::ReplaceWithCopy(&gTtsVoiceId, voiceId ? voiceId : "");
    return true;
}

const char* TtsGetVoiceId() {
    return gTtsVoiceId ? gTtsVoiceId : "";
}

void TtsFreeVoices(Vec<TtsVoiceInfo>& voices) {
    for (TtsVoiceInfo& voice : voices) {
        free(voice.id);
        free(voice.name);
        free(voice.lang);
    }
    voices.Reset();
}

// Notification and SAPI event handling

void TtsSetNotifyWindow(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    gTtsNotifyHwnd = hwnd;
    gTtsNotifyMsg = msg;
    gTtsNotifyWParam = wp;
    gTtsNotifyLParam = lp;

    if (!gTtsVoice) {
        return;
    }

    ISpEventSource* eventSource = nullptr;
    HRESULT hr = gTtsVoice->QueryInterface(IID_ISpEventSource, (void**)&eventSource);
    if (FAILED(hr) || !eventSource) {
        return;
    }

    const ULONGLONG events = SPFEI(SPEI_END_INPUT_STREAM);
    eventSource->SetInterest(events, events);

    if (gTtsNotifyHwnd && gTtsNotifyMsg) {
        eventSource->SetNotifyWindowMessage(gTtsNotifyHwnd, gTtsNotifyMsg, gTtsNotifyWParam, gTtsNotifyLParam);
    }

    eventSource->Release();
}

static void TtsClearEvent(SPEVENT* eventItem) {
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

void TtsProcessEvents() {
    if (!gTtsVoice) {
        return;
    }

    ISpEventSource* eventSource = nullptr;
    HRESULT hr = gTtsVoice->QueryInterface(IID_ISpEventSource, (void**)&eventSource);
    if (FAILED(hr) || !eventSource) {
        return;
    }

    SPEVENT eventItem = {};
    ULONG fetched = 0;

    while (eventSource->GetEvents(1, &eventItem, &fetched) == S_OK && fetched > 0) {
        if (eventItem.eEventId == SPEI_END_INPUT_STREAM && eventItem.ulStreamNum == gTtsStreamNum) {
            gTtsActive = false;
            gTtsStreamNum = 0;
        }

        TtsClearEvent(&eventItem);

        eventItem = {};
        fetched = 0;
    }

    eventSource->Release();
}

// Speech control

bool TtsSpeakUtf8(const char* text) {
    if (str::IsEmpty(text)) {
        return false;
    }

    if (!TtsInit()) {
        return false;
    }

    TempWStr textW = ToWStrTemp(text);
    if (!textW) {
        return false;
    }

    ULONG streamNum = 0;
    HRESULT hr = gTtsVoice->Speak(textW, SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, &streamNum);

    if (SUCCEEDED(hr)) {
        gTtsStreamNum = streamNum;
        gTtsActive = true;
        return true;
    }

    return false;
}

bool TtsIsSpeaking() {
    return gTtsActive;
}

void TtsStop() {
    if (gTtsVoice) {
        gTtsVoice->Speak(nullptr, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
    }

    gTtsActive = false;
    gTtsStreamNum = 0;
}

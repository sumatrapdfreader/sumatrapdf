/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include <dlfcn.h>

#include "ChapterTable.h"
#include "ReadAloud.h"

enum SpdMode {
    SpdModeThreaded = 1,
};

enum SpdPriority {
    SpdPriorityText = 3,
};

enum SpdDataMode {
    SpdDataSsml = 1,
};

enum SpdNotification {
    SpdNotifyBegin = 1,
    SpdNotifyEnd = 2,
    SpdNotifyIndexMarks = 4,
    SpdNotifyCancel = 8,
};

enum SpdNotificationType {
    SpdEventBegin,
    SpdEventEnd,
    SpdEventIndexMark,
    SpdEventCancel,
};

using SpdCallback = void (*)(size_t messageId, size_t clientId, SpdNotificationType state);
using SpdCallbackMark = void (*)(size_t messageId, size_t clientId, SpdNotificationType state, char* mark);

// The callbacks are the public prefix of SPDConnection. The rest is private.
struct SpdConnection {
    SpdCallback callbackBegin;
    SpdCallback callbackEnd;
    SpdCallback callbackCancel;
    SpdCallback callbackPause;
    SpdCallback callbackResume;
    SpdCallbackMark callbackMark;
};

struct SpdVoice {
    char* name;
    char* language;
    char* variant;
};

struct SpdApi {
    void* lib = nullptr;
    SpdConnection* (*open)(const char*, const char*, const char*, SpdMode) = nullptr;
    void (*close)(SpdConnection*) = nullptr;
    int (*say)(SpdConnection*, SpdPriority, const char*) = nullptr;
    int (*cancel)(SpdConnection*) = nullptr;
    int (*setDataMode)(SpdConnection*, SpdDataMode) = nullptr;
    int (*setNotificationOn)(SpdConnection*, SpdNotification) = nullptr;
    int (*setVoiceRate)(SpdConnection*, int) = nullptr;
    int (*setVoice)(SpdConnection*, const char*) = nullptr;
    SpdVoice** (*listVoices)(SpdConnection*) = nullptr;
    void (*freeVoices)(SpdVoice**) = nullptr;
};

constexpr float kTtsSpeedMin = 0.5f;
constexpr float kTtsSpeedMax = 3.0f;

static SpdApi gSpd;
static SpdConnection* gTtsConnection = nullptr;
static Mutex gTtsMutex;
static Str gTtsSpokenText;
static Str gTtsQueuedText;
static Str gTtsVoiceId;
static float gTtsSpeed = 1.0f;
static int gTtsSpokenPos = 0;
static int gTtsActiveMessage = 0;
static int gTtsQueuedMessage = 0;
static bool gTtsActive = false;
static bool gTtsQueuedStarted = false;
static bool gTtsInitTried = false;

void TtsLinuxRelease();

template <typename T>
static bool LoadSpdFn(T& fn, const char* name) {
    fn = (T)dlsym(gSpd.lib, name);
    return fn != nullptr;
}

static void TtsOnBegin(size_t messageId, size_t, SpdNotificationType) {
    ScopedMutex lock(&gTtsMutex);
    if ((int)messageId != gTtsQueuedMessage) {
        return;
    }
    str::ReplacePtr(&gTtsSpokenText, gTtsQueuedText);
    gTtsQueuedText = {};
    gTtsActiveMessage = gTtsQueuedMessage;
    gTtsQueuedMessage = 0;
    gTtsSpokenPos = 0;
    gTtsActive = true;
    gTtsQueuedStarted = true;
}

static void TtsOnEnd(size_t messageId, size_t, SpdNotificationType) {
    ScopedMutex lock(&gTtsMutex);
    if ((int)messageId == gTtsQueuedMessage) {
        str::FreePtr(&gTtsQueuedText);
        gTtsQueuedMessage = 0;
    }
    if ((int)messageId == gTtsActiveMessage && gTtsQueuedMessage == 0) {
        gTtsActive = false;
        gTtsActiveMessage = 0;
    }
}

static void TtsOnMark(size_t messageId, size_t, SpdNotificationType, char* mark) {
    if (!mark) {
        return;
    }
    char* end = nullptr;
    long pos = strtol(mark, &end, 10);
    if (end == mark || *end != 0 || pos < 0 || pos > INT_MAX) {
        return;
    }
    ScopedMutex lock(&gTtsMutex);
    if ((int)messageId == gTtsActiveMessage) {
        gTtsSpokenPos = (int)pos;
    }
}

static bool TtsInit() {
    if (gTtsConnection) {
        return true;
    }
    if (gTtsInitTried) {
        return false;
    }
    gTtsInitTried = true;

    gSpd.lib = dlopen("libspeechd.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!gSpd.lib) {
        gSpd.lib = dlopen("libspeechd.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!gSpd.lib || !LoadSpdFn(gSpd.open, "spd_open") || !LoadSpdFn(gSpd.close, "spd_close") ||
        !LoadSpdFn(gSpd.say, "spd_say") || !LoadSpdFn(gSpd.cancel, "spd_cancel") ||
        !LoadSpdFn(gSpd.setDataMode, "spd_set_data_mode") ||
        !LoadSpdFn(gSpd.setNotificationOn, "spd_set_notification_on") ||
        !LoadSpdFn(gSpd.setVoiceRate, "spd_set_voice_rate") || !LoadSpdFn(gSpd.setVoice, "spd_set_synthesis_voice") ||
        !LoadSpdFn(gSpd.listVoices, "spd_list_synthesis_voices") || !LoadSpdFn(gSpd.freeVoices, "free_spd_voices")) {
        if (gSpd.lib) {
            dlclose(gSpd.lib);
        }
        gSpd = {};
        return false;
    }

    gTtsConnection = gSpd.open("sumatrapdf", "read-aloud", nullptr, SpdModeThreaded);
    if (!gTtsConnection) {
        dlclose(gSpd.lib);
        gSpd = {};
        return false;
    }
    gTtsConnection->callbackBegin = TtsOnBegin;
    gTtsConnection->callbackEnd = TtsOnEnd;
    gTtsConnection->callbackCancel = TtsOnEnd;
    gTtsConnection->callbackMark = TtsOnMark;
    gSpd.setNotificationOn(gTtsConnection, SpdNotifyBegin);
    gSpd.setNotificationOn(gTtsConnection, SpdNotifyEnd);
    gSpd.setNotificationOn(gTtsConnection, SpdNotifyCancel);
    gSpd.setNotificationOn(gTtsConnection, SpdNotifyIndexMarks);
    gSpd.setDataMode(gTtsConnection, SpdDataSsml);
    return true;
}

static TempStr TtsSsmlTemp(Str text) {
    str::Builder out;
    out.Append(StrL("<speak>"));
    bool atWordStart = true;
    for (int i = 0; i < len(text); i++) {
        char c = text.s[i];
        bool ws = str::IsWs(c);
        if (!ws && atWordStart) {
            out.Append(fmt("<mark name=\"%d\"/>", i));
        }
        atWordStart = ws;
        if (c == '&') {
            out.Append(StrL("&amp;"));
        } else if (c == '<') {
            out.Append(StrL("&lt;"));
        } else if (c == '>') {
            out.Append(StrL("&gt;"));
        } else {
            out.AppendChar(c);
        }
    }
    out.Append(StrL("</speak>"));
    return ToStrTemp(out);
}

static int TtsRate() {
    if (gTtsSpeed < 1.0f) {
        return (int)(((gTtsSpeed - 1.0f) / (1.0f - kTtsSpeedMin)) * 100.0f);
    }
    return (int)(((gTtsSpeed - 1.0f) / (kTtsSpeedMax - 1.0f)) * 100.0f);
}

bool TtsIsAvailable() {
    return TtsInit();
}

bool TtsSpeakUtf8(Str text) {
    if (len(text) == 0 || !TtsInit()) {
        return false;
    }
    TtsStop();
    gSpd.setVoiceRate(gTtsConnection, TtsRate());
    if (len(gTtsVoiceId) > 0) {
        gSpd.setVoice(gTtsConnection, CStrTemp(gTtsVoiceId));
    }
    TempStr ssml = TtsSsmlTemp(text);
    int message = gSpd.say(gTtsConnection, SpdPriorityText, CStrTemp(ssml));
    if (message < 0) {
        return false;
    }
    ScopedMutex lock(&gTtsMutex);
    str::ReplacePtr(&gTtsSpokenText, str::Dup(text));
    gTtsSpokenPos = 0;
    gTtsActiveMessage = message;
    gTtsActive = true;
    return true;
}

bool TtsQueueUtf8(Str text) {
    if (len(text) == 0 || !TtsInit()) {
        return false;
    }
    {
        ScopedMutex lock(&gTtsMutex);
        if (!gTtsActive || gTtsQueuedMessage != 0) {
            return false;
        }
    }
    TempStr ssml = TtsSsmlTemp(text);
    int message = gSpd.say(gTtsConnection, SpdPriorityText, CStrTemp(ssml));
    if (message < 0) {
        return false;
    }
    ScopedMutex lock(&gTtsMutex);
    str::ReplacePtr(&gTtsQueuedText, str::Dup(text));
    gTtsQueuedMessage = message;
    return true;
}

bool TtsDidStartQueued() {
    ScopedMutex lock(&gTtsMutex);
    bool started = gTtsQueuedStarted;
    gTtsQueuedStarted = false;
    return started;
}

void TtsStop() {
    if (gTtsConnection) {
        gSpd.cancel(gTtsConnection);
    }
    ScopedMutex lock(&gTtsMutex);
    str::FreePtr(&gTtsSpokenText);
    str::FreePtr(&gTtsQueuedText);
    gTtsSpokenPos = 0;
    gTtsActiveMessage = 0;
    gTtsQueuedMessage = 0;
    gTtsActive = false;
    gTtsQueuedStarted = false;
}

bool TtsIsSpeaking() {
    ScopedMutex lock(&gTtsMutex);
    return gTtsActive;
}

int TtsGetSpokenPosUtf8() {
    ScopedMutex lock(&gTtsMutex);
    return gTtsActive ? gTtsSpokenPos : -1;
}

void TtsProcessEvents() {}

Vec<TtsVoiceInfo> TtsGetVoices() {
    Vec<TtsVoiceInfo> voices;
    if (!TtsInit()) {
        return voices;
    }
    SpdVoice** available = gSpd.listVoices(gTtsConnection);
    for (int i = 0; available && available[i]; i++) {
        SpdVoice* voice = available[i];
        TtsVoiceInfo info{};
        info.id = str::Dup(Str(voice->name ? voice->name : (char*)""));
        info.name = str::Dup(Str(voice->name ? voice->name : (char*)""));
        info.lang = str::Dup(Str(voice->language ? voice->language : (char*)""));
        VecAppend(voices, info);
    }
    if (available) {
        gSpd.freeVoices(available);
    }
    return voices;
}

void TtsFreeVoices(Vec<TtsVoiceInfo>& voices) {
    for (TtsVoiceInfo& voice : voices) {
        str::Free(voice.id);
        str::Free(voice.name);
        str::Free(voice.lang);
    }
    VecReset(voices);
}

bool TtsSetVoiceById(Str voiceId) {
    if (len(voiceId) == 0 && len(gTtsVoiceId) > 0) {
        TtsLinuxRelease();
        gTtsInitTried = false;
        return TtsInit();
    }
    if (len(voiceId) > 0 && (!TtsInit() || gSpd.setVoice(gTtsConnection, CStrTemp(voiceId)) < 0)) {
        return false;
    }
    str::ReplacePtr(&gTtsVoiceId, len(voiceId) > 0 ? str::Dup(voiceId) : Str{});
    return true;
}

Str TtsGetVoiceId() {
    return gTtsVoiceId;
}

void TtsSetSpeed(float speed) {
    gTtsSpeed = limitValue(speed, kTtsSpeedMin, kTtsSpeedMax);
    if (gTtsConnection) {
        gSpd.setVoiceRate(gTtsConnection, TtsRate());
    }
}

float TtsGetSpeed() {
    return gTtsSpeed;
}

void TtsLinuxRelease() {
    TtsStop();
    if (gTtsConnection) {
        gTtsConnection->callbackBegin = nullptr;
        gTtsConnection->callbackEnd = nullptr;
        gTtsConnection->callbackCancel = nullptr;
        gTtsConnection->callbackMark = nullptr;
        gSpd.close(gTtsConnection);
        gTtsConnection = nullptr;
    }
    if (gSpd.lib) {
        dlclose(gSpd.lib);
    }
    gSpd = {};
    str::FreePtr(&gTtsVoiceId);
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

void TtsTestPumpOnNextSpeak() {}

/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include <emscripten/emscripten.h>

#include "ChapterTable.h"
#include "ReadAloud.h"

constexpr float kTtsSpeedMin = 0.5f;
constexpr float kTtsSpeedMax = 3.5f;

static Str gTtsSpokenText;
static Str gTtsQueuedText;
static Str gTtsVoiceId;
static float gTtsSpeed = 1.0f;
static int gTtsSpokenPos = 0;
static int gTtsActiveToken = 0;
static int gTtsQueuedToken = 0;
static bool gTtsActive = false;
static bool gTtsQueuedStarted = false;
static bool gTtsWaitingQueued = false;

// NOLINTNEXTLINE
EM_JS(int, WasmTtsAvailable, (),
      { return typeof speechSynthesis != "undefined" && typeof SpeechSynthesisUtterance != "undefined"; });

// NOLINTNEXTLINE
EM_JS(int, WasmTtsSpeak, (const char* text, int textLen, const char* voiceId, int voiceIdLen, float rate, int queued), {
    if (typeof speechSynthesis == "undefined" || typeof SpeechSynthesisUtterance == "undefined") return 0;
    Module.__sumatraTtsToken = (Module.__sumatraTtsToken || 0) + 1;
    var token = Module.__sumatraTtsToken;
    var utterance = new SpeechSynthesisUtterance(UTF8ToString(text, textLen));
    utterance.rate = rate;
    var wanted = UTF8ToString(voiceId, voiceIdLen);
    if (wanted) {
        var voices = speechSynthesis.getVoices();
        for (var i = 0; i < voices.length; ++i) {
            if (voices[i].voiceURI == wanted) {
                utterance.voice = voices[i];
                break;
            }
        }
    }
    utterance.onstart = function() {
        _sumatra_wasm_tts_event(1, 0, token, queued);
    };
    utterance.onboundary = function(event) {
        if (!event.name || event.name == "word") _sumatra_wasm_tts_event(2, event.charIndex, token, queued);
    };
    utterance.onend = function() {
        _sumatra_wasm_tts_event(3, 0, token, queued);
    };
    utterance.onerror = function() {
        _sumatra_wasm_tts_event(3, 0, token, queued);
    };
    if (!queued) speechSynthesis.cancel();
    speechSynthesis.speak(utterance);
    return token;
});

// NOLINTNEXTLINE
EM_JS(void, WasmTtsStop, (), {
    if (typeof speechSynthesis != "undefined") speechSynthesis.cancel();
    Module.__sumatraTtsToken = (Module.__sumatraTtsToken || 0) + 1;
});

// NOLINTNEXTLINE
EM_JS(int, WasmTtsVoiceCount, (), {
    if (typeof speechSynthesis == "undefined") return 0;
    return speechSynthesis.getVoices().length;
});

// prop: 0 = voice URI, 1 = display name, 2 = BCP 47 language
// NOLINTNEXTLINE
EM_JS(int, WasmTtsVoiceProp, (int idx, int prop, char* out, int outCap), {
    var voices = speechSynthesis.getVoices();
    if (idx < 0 || idx >= voices.length) return 0;
    var voice = voices[idx];
    var value = prop == 0 ? voice.voiceURI : prop == 1 ? voice.name : voice.lang;
    if (lengthBytesUTF8(value) + 1 > outCap) return 0;
    stringToUTF8(value, out, outCap);
    return 1;
});

// 1 = found, 0 = missing, -1 = browser has not populated its voice list yet
// NOLINTNEXTLINE
EM_JS(int, WasmTtsVoiceExists, (const char* id, int idLen), {
    var voices = speechSynthesis.getVoices();
    if (!voices.length) return -1;
    var wanted = UTF8ToString(id, idLen);
    for (var i = 0; i < voices.length; ++i) {
        if (voices[i].voiceURI == wanted) return 1;
    }
    return 0;
});

static int Utf8BytePosFromUtf16(Str text, int utf16Pos) {
    int bytePos = 0;
    int units = 0;
    while (bytePos < len(text) && units < utf16Pos) {
        int prev = bytePos;
        int rune = Utf8CodepointNext(text, bytePos);
        int runeUnits = rune > 0xffff ? 2 : 1;
        if (units + runeUnits > utf16Pos) {
            return prev;
        }
        units += runeUnits;
    }
    return bytePos;
}

extern "C" EMSCRIPTEN_KEEPALIVE void sumatra_wasm_tts_event(int kind, int charIndex, int token, int queued) {
    if (kind == 1 && queued && token == gTtsQueuedToken) {
        str::ReplacePtr(&gTtsSpokenText, gTtsQueuedText);
        gTtsQueuedText = {};
        gTtsQueuedToken = 0;
        gTtsActiveToken = token;
        gTtsSpokenPos = 0;
        gTtsActive = true;
        gTtsQueuedStarted = true;
        gTtsWaitingQueued = false;
        return;
    }
    if (kind == 3 && queued && token == gTtsQueuedToken) {
        str::FreePtr(&gTtsQueuedText);
        gTtsQueuedToken = 0;
        if (gTtsWaitingQueued) {
            gTtsActive = false;
            gTtsWaitingQueued = false;
        }
        return;
    }
    if (token != gTtsActiveToken) {
        return;
    }
    if (kind == 2) {
        gTtsSpokenPos = Utf8BytePosFromUtf16(gTtsSpokenText, charIndex);
    } else if (kind == 3) {
        if (gTtsQueuedToken) {
            gTtsWaitingQueued = true;
        } else {
            gTtsActive = false;
        }
    }
}

bool TtsIsAvailable() {
    return WasmTtsAvailable() != 0;
}

bool TtsSpeakUtf8(Str text) {
    if (len(text) == 0) {
        return false;
    }
    const char* voiceId = gTtsVoiceId ? gTtsVoiceId.s : "";
    int token = WasmTtsSpeak(text.s, text.len, voiceId, gTtsVoiceId.len, gTtsSpeed, false);
    if (token == 0) {
        return false;
    }
    str::ReplacePtr(&gTtsSpokenText, str::Dup(text));
    str::FreePtr(&gTtsQueuedText);
    gTtsSpokenPos = 0;
    gTtsActiveToken = token;
    gTtsQueuedToken = 0;
    gTtsActive = true;
    gTtsQueuedStarted = false;
    gTtsWaitingQueued = false;
    return true;
}

bool TtsQueueUtf8(Str text) {
    if (!gTtsActive || gTtsQueuedToken || len(text) == 0) {
        return false;
    }
    const char* voiceId = gTtsVoiceId ? gTtsVoiceId.s : "";
    int token = WasmTtsSpeak(text.s, text.len, voiceId, gTtsVoiceId.len, gTtsSpeed, true);
    if (token == 0) {
        return false;
    }
    str::ReplacePtr(&gTtsQueuedText, str::Dup(text));
    gTtsQueuedToken = token;
    return true;
}

bool TtsDidStartQueued() {
    bool started = gTtsQueuedStarted;
    gTtsQueuedStarted = false;
    return started;
}

void TtsStop() {
    WasmTtsStop();
    str::FreePtr(&gTtsSpokenText);
    str::FreePtr(&gTtsQueuedText);
    gTtsSpokenPos = 0;
    gTtsActiveToken = 0;
    gTtsQueuedToken = 0;
    gTtsActive = false;
    gTtsQueuedStarted = false;
    gTtsWaitingQueued = false;
}

bool TtsIsSpeaking() {
    return gTtsActive;
}

int TtsGetSpokenPosUtf8() {
    return gTtsActive ? gTtsSpokenPos : -1;
}

void TtsProcessEvents() {}

static Str TtsVoiceProp(int idx, int prop) {
    char buf[1024]{};
    if (!WasmTtsVoiceProp(idx, prop, buf, dimof(buf))) {
        return {};
    }
    return str::Dup(Str(buf));
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

Vec<TtsVoiceInfo> TtsGetVoices() {
    Vec<TtsVoiceInfo> voices;
    int count = WasmTtsVoiceCount();
    for (int i = 0; i < count; i++) {
        TtsVoiceInfo info{TtsVoiceProp(i, 0), TtsVoiceProp(i, 1), TtsVoiceProp(i, 2)};
        if (len(info.id) > 0) {
            VecAppend(voices, info);
        } else {
            str::Free(info.name);
            str::Free(info.lang);
        }
    }
    for (int i = 1; i < len(voices); i++) {
        TtsVoiceInfo value = voices[i];
        int j = i - 1;
        while (j >= 0 && TtsVoiceLess(value, voices[j])) {
            voices[j + 1] = voices[j];
            j--;
        }
        voices[j + 1] = value;
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
    if (len(voiceId) > 0 && WasmTtsVoiceExists(voiceId.s, voiceId.len) == 0) {
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
}

float TtsGetSpeed() {
    return gTtsSpeed;
}

void TtsWasmRelease() {
    TtsStop();
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

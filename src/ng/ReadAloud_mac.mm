/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

// ng: objc's BOOL is bool (or signed char); ours is int. Rename theirs for
// the import. Point / Rect / Size are handled by MacTypesHide.h.
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#import <AVFoundation/AVFoundation.h>
#undef BOOL
#include "base/MacTypesShow.h"

#include "base/UITask.h"

#include "ChapterTable.h"
#include "ReadAloud.h"

constexpr float kTtsSpeedMin = 0.5f;
constexpr float kTtsSpeedMax = 3.0f;

static AVSpeechSynthesizer* gTtsSynth = nil;
static AVSpeechUtterance* gTtsUtterance = nil;
static NSString* gTtsQueuedText = nil;
static NSString* gTtsSpokenText = nil;
static Str gTtsVoiceId;
static float gTtsSpeed = 1.0f;
static int gTtsLastWordPos = 0;
static bool gTtsActive = false;
static bool gTtsQueuedStarted = false;

static NSString* TtsString(Str text) {
    if (len(text) == 0) {
        return nil;
    }
    return [[NSString alloc] initWithBytes:text.s length:(NSUInteger)text.len encoding:NSUTF8StringEncoding];
}

static Str TtsDupString(NSString* text) {
    const char* s = text.UTF8String;
    return s ? str::Dup(Str(s)) : Str{};
}

static AVSpeechSynthesisVoice* TtsVoice() {
    if (len(gTtsVoiceId) == 0) {
        return nil;
    }
    NSString* voiceId = TtsString(gTtsVoiceId);
    return voiceId ? [AVSpeechSynthesisVoice voiceWithIdentifier:voiceId] : nil;
}

static float TtsRate() {
    float rate = AVSpeechUtteranceDefaultSpeechRate * gTtsSpeed;
    return limitValue(rate, AVSpeechUtteranceMinimumSpeechRate, AVSpeechUtteranceMaximumSpeechRate);
}

static AVSpeechUtterance* TtsMakeUtterance(NSString* text) {
    AVSpeechUtterance* utterance = [AVSpeechUtterance speechUtteranceWithString:text];
    utterance.voice = TtsVoice();
    utterance.rate = TtsRate();
    return utterance;
}

@interface SumatraSpeechDelegate : NSObject <AVSpeechSynthesizerDelegate>
@end

static SumatraSpeechDelegate* gTtsDelegate = nil;

static bool TtsStart(NSString* text, bool queued) {
    if (!gTtsSynth || text.length == 0) {
        return false;
    }
    gTtsUtterance = TtsMakeUtterance(text);
    gTtsSpokenText = text;
    gTtsLastWordPos = 0;
    gTtsActive = true;
    gTtsQueuedStarted = queued;
    [gTtsSynth speakUtterance:gTtsUtterance];
    return true;
}

@implementation SumatraSpeechDelegate

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    willSpeakRangeOfSpeechString:(NSRange)range
                      utterance:(AVSpeechUtterance*)utterance {
    if (utterance != gTtsUtterance) {
        return;
    }
    NSUInteger pos = MIN(range.location, utterance.speechString.length);
    NSString* prefix = [utterance.speechString substringToIndex:pos];
    gTtsLastWordPos = (int)[prefix lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    didFinishSpeechUtterance:(AVSpeechUtterance*)utterance {
    if (utterance != gTtsUtterance) {
        return;
    }
    gTtsUtterance = nil;
    if (gTtsQueuedText) {
        NSString* queued = gTtsQueuedText;
        gTtsQueuedText = nil;
        TtsStart(queued, true);
        return;
    }
    gTtsActive = false;
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    didCancelSpeechUtterance:(AVSpeechUtterance*)utterance {
    if (utterance == gTtsUtterance) {
        gTtsActive = false;
        gTtsUtterance = nil;
    }
}

@end

static bool TtsInit() {
    if (gTtsSynth) {
        return true;
    }
    gTtsDelegate = [[SumatraSpeechDelegate alloc] init];
    gTtsSynth = [[AVSpeechSynthesizer alloc] init];
    gTtsSynth.delegate = gTtsDelegate;
    return true;
}

bool TtsIsAvailable() {
    return true;
}

static bool gTtsTestPumpOnSpeak = false;

bool TtsSpeakUtf8(Str text) {
    // A Windows speech call pumps messages, so a queued close runs inside it.
    if (gTtsTestPumpOnSpeak) {
        gTtsTestPumpOnSpeak = false;
        uitask::DrainQueue();
        return false;
    }
    NSString* value = TtsString(text);
    if (!value || !TtsInit()) {
        return false;
    }
    TtsStop();
    return TtsStart(value, false);
}

bool TtsQueueUtf8(Str text) {
    if (!gTtsActive || gTtsQueuedText) {
        return false;
    }
    NSString* value = TtsString(text);
    if (!value) {
        return false;
    }
    gTtsQueuedText = value;
    return true;
}

bool TtsDidStartQueued() {
    bool started = gTtsQueuedStarted;
    gTtsQueuedStarted = false;
    return started;
}

void TtsStop() {
    gTtsActive = false;
    gTtsQueuedStarted = false;
    gTtsQueuedText = nil;
    gTtsSpokenText = nil;
    gTtsUtterance = nil;
    gTtsLastWordPos = 0;
    [gTtsSynth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
}

bool TtsIsSpeaking() {
    return gTtsActive;
}

int TtsGetSpokenPosUtf8() {
    return gTtsActive && gTtsSpokenText ? gTtsLastWordPos : -1;
}

void TtsProcessEvents() {}

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
    for (AVSpeechSynthesisVoice* voice in [AVSpeechSynthesisVoice speechVoices]) {
        TtsVoiceInfo info{};
        info.id = TtsDupString(voice.identifier);
        info.name = TtsDupString(voice.name);
        info.lang = TtsDupString(voice.language);
        VecAppend(voices, info);
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
    if (len(voiceId) > 0) {
        NSString* value = TtsString(voiceId);
        if (!value || ![AVSpeechSynthesisVoice voiceWithIdentifier:value]) {
            return false;
        }
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

void TtsMacRelease() {
    TtsStop();
    gTtsSynth.delegate = nil;
    gTtsSynth = nil;
    gTtsDelegate = nil;
    str::Free(gTtsVoiceId);
    gTtsVoiceId = {};
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

void TtsTestPumpOnNextSpeak() {
    gTtsTestPumpOnSpeak = true;
}

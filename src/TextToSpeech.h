#pragma once

struct TtsVoiceInfo {
    char* id;
    char* name;
    char* lang;
};

bool TtsSpeakUtf8(const char* text);
void TtsStop();
void TtsRelease();

bool TtsIsSpeaking();

void TtsSetNotifyWindow(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void TtsProcessEvents();

Vec<TtsVoiceInfo> TtsGetVoices();
void TtsFreeVoices(Vec<TtsVoiceInfo>& voices);

bool TtsSetVoiceById(const char* voiceId);
const char* TtsGetVoiceId();

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

// utf8 offset of the most recently spoken word within the text passed
// to TtsSpeakUtf8, -1 if not known
int TtsGetSpokenPosUtf8();

void TtsSetNotifyWindow(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void TtsProcessEvents();

Vec<TtsVoiceInfo> TtsGetVoices();
void TtsFreeVoices(Vec<TtsVoiceInfo>& voices);

bool TtsSetVoiceById(const char* voiceId);
const char* TtsGetVoiceId();

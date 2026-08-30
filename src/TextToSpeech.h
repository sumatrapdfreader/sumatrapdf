
struct TtsVoiceInfo {
    Str id;
    Str name;
    Str lang;
};

bool TtsSpeakUtf8(Str text);
bool TtsQueueUtf8(Str text);
bool TtsDidStartQueued();
void TtsStop();
void TtsRelease();

bool TtsIsSpeaking();

int TtsGetSpokenPosUtf8();

void TtsSetNotifyWindow(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void TtsProcessEvents();

Vec<TtsVoiceInfo> TtsGetVoices();
void TtsFreeVoices(Vec<TtsVoiceInfo>& voices);

bool TtsSetVoiceById(Str voiceId);
Str TtsGetVoiceId();

void TtsSetSpeed(float speed);
float TtsGetSpeed();

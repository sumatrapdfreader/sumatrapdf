/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct DisplayModel;
struct Gfx;
struct MainWindow;
struct WindowTab;
struct TextSelection;
struct ReadAloudPlaybackBar;
namespace str {
struct Builder;
}

// --- text-to-speech backend (WinRT speech synthesis, SAPI 5 fallback) ---

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

// --- highlight of the words being spoken ---

constexpr int kReadAloudHighlightTimerID = 8;
constexpr int kReadAloudHighlightDelayInMs = 80;

struct ReadAloudByteLoc {
    int pageNo = -1;
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
};

struct ReadAloudHighlightMap {
    int len = 0;
    int cap = 0;
    ReadAloudByteLoc* locs = nullptr;
};

void ReadAloudHighlightFree(ReadAloudHighlightMap* map);

bool ReadAloudHighlightBuildFromPage(EngineBase* engine, int pageNo, ReadAloudHighlightMap* map,
                                     str::Builder& cleanedOut);

bool ReadAloudHighlightBuildFromTextSelection(TextSelection* ts, ReadAloudHighlightMap* map, str::Builder& cleanedOut);

bool ReadAloudGetViewportStart(DisplayModel* dm, int* startPageOut, int* startGlyphOut);

bool ReadAloudCanReadFromCursor(DisplayModel* dm, Point screenPt);

bool ReadAloudGetCursorStart(DisplayModel* dm, Point screenPt, int* startPageOut, int* startGlyphOut);

bool ReadAloudHighlightBuildFromDocument(DisplayModel* dm, int startPage, int startGlyph, ReadAloudHighlightMap* map,
                                         str::Builder& cleanedOut);

void ReadAloudHighlightTimerStart(MainWindow* win);
void ReadAloudHighlightTimerStop(MainWindow* win);

void ReadAloudOnUserViewChanged(MainWindow* win);
void ReadAloudUpdateAutoScroll(MainWindow* win);

bool ReadAloudGetProgressPage(WindowTab* tab, int* pageOut, int* pageCountOut);

void PaintReadAloudHighlight(MainWindow* win, Gfx* gfx);

bool ReadAloudSentenceRange(Str text, int pos, int* startOut, int* endOut);

// --- playback bar shown over the canvas while reading ---

void ReadAloudPlaybackBarUpdateSession(WindowTab* tab);
void ReadAloudPlaybackBarHide(MainWindow* win);
void ReadAloudPlaybackBarForgetTab(MainWindow* win, WindowTab* tab);
void ReadAloudPlaybackBarRelayout(HWND hwndCanvas);
void ReadAloudPlaybackBarTick(MainWindow* win);

void ReadAloudPlaybackPauseOrResume();
void ReadAloudPlaybackStop();
void ReadAloudPlaybackCycleSpeed(int dir);
void ReadAloudPlaybackBarDestroy(MainWindow* win);
TempStr ReadAloudPlaybackBarStateTemp(int* exitCodeOut);

// --- read-aloud session: what to read, chunking, menus ---

// posted by the tts backend, handled by the frame window
constexpr UINT kWmTtsEvent = WM_APP + 0x421;

constexpr UINT CmdTtsVoiceDefault = 0x7100;
constexpr UINT CmdTtsVoiceFirst = 0x7101;
constexpr UINT CmdTtsVoiceLast = 0x71ff;
constexpr UINT CmdTtsMenuReadCurrentPage = 0x7200;
constexpr UINT CmdTtsMenuContinueReading = 0x7201;
constexpr UINT CmdTtsMenuReadSelection = 0x7202;
constexpr UINT CmdTtsMenuPauseReading = 0x7203;
constexpr UINT CmdTtsMenuReadFromCursor = 0x7204;
constexpr UINT CmdTtsMenuStopReading = 0x7205;
constexpr UINT CmdTtsSpeedFirst = 0x7300;
constexpr UINT CmdTtsSpeedLast = 0x730f;

WindowTab* GetReadAloudSourceTab();
void ReadAloudForgetTab(WindowTab*);
void ReadAloudAfterTtsEvents();
bool CanContinueReadAloud(WindowTab* tab);

void ReadAloudInTab(WindowTab* tab);
void ReadAloudContinueInTab(WindowTab* tab);
void ReadAloudSelectionInTab(WindowTab* tab);
void ReadAloudFromViewportTopInTab(WindowTab* tab);
void ReadAloudStopRememberPos();
void ResetReadAloudStateForTab(WindowTab* tab);
void StopReadAloudIfSourceWindow(MainWindow* win);

// handles kWmTtsEvent: advances the session, refreshes highlight and playback bar
void ReadAloudOnTtsEvent(MainWindow* win);

TempStr ReadAloudSpeedLabelTemp(float speed);
int ReadAloudSpeedCount();
float ReadAloudSpeedAt(int idx);
int ReadAloudClosestSpeedIdx();
void ReadAloudSetSpeedIdx(int idx);

void RebuildReadAloudMenu(MainWindow* win, HMENU menu, bool includeCursorItem = false, bool canReadFromCursor = false);
bool HandleReadAloudMenuCommand(MainWindow* win, int cmdId);
void SetReadAloudAppSubmenu(HMENU menu);
HMENU GetReadAloudAppSubmenu();
bool IsReadAloudAppSubmenu(HMENU menu);
void SetReadAloudContextSubmenu(HMENU menu);
bool IsReadAloudContextSubmenu(HMENU menu);
HMENU GetReadAloudContextSubmenu();
void ShowTtsVoiceMenu(MainWindow* win, Rect buttonScreen);

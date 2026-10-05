/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct DisplayModel;
struct MainWindow;
struct WindowTab;
struct TextSelection;
struct MenuModel;
struct ReadAloudPlaybackBar;
namespace gpui {
struct Ctx;
struct El;
struct PaintCtx;
} // namespace gpui

// --- text-to-speech backend ---

struct TtsVoiceInfo {
    Str id;
    Str name;
    Str lang;
};

// ng: orig also has a Windows.Media.SpeechSynthesis (WinRT) backend it prefers
// over SAPI. Windows uses SAPI here, macOS uses AVSpeechSynthesizer, Linux
// uses Speech Dispatcher and wasm uses the Web Speech API.
bool TtsIsAvailable();

bool TtsSpeakUtf8(Str text);
bool TtsQueueUtf8(Str text);
bool TtsDidStartQueued();
void TtsStop();
void TtsRelease();

bool TtsIsSpeaking();

int TtsGetSpokenPosUtf8();

#if OS_WIN
void TtsSetNotifyWindow(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
#endif
void TtsProcessEvents();

Vec<TtsVoiceInfo> TtsGetVoices();
void TtsFreeVoices(Vec<TtsVoiceInfo>& voices);

bool TtsSetVoiceById(Str voiceId);
Str TtsGetVoiceId();

void TtsSetSpeed(float speed);
float TtsGetSpeed();

// applies ReadAloudSpeed / ReadAloudVoiceId; true if the voice was not
// available and the setting was cleared. ng: orig has this in AppSettings.cpp,
// which is in the `app` lib here and cannot call the engine (src/tools/AppStubs.cpp)
bool ApplyReadAloudVoiceFromSettings();

bool TtsOnEngineCrash(void* faultAddr);
bool TtsTakeEngineCrash();
bool TtsEngineCrashed();
bool TtsTestEngineCrash();

// --- highlight of the words being spoken ---

constexpr int kReadAloudHighlightDelayInMs = 80;

struct ReadAloudByteLoc {
    Location pageLoc;
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

// ng: orig runs this off a WM_TIMER on the canvas; the shell's tick does
void ReadAloudHighlightTimerStart(MainWindow* win);
void ReadAloudHighlightTimerStop(MainWindow* win);
void ReadAloudTick(MainWindow* win, int elapsedMs);

void ReadAloudOnUserViewChanged(MainWindow* win);
void ReadAloudUpdateAutoScroll(MainWindow* win);

bool ReadAloudGetProgressPage(WindowTab* tab, int* pageOut, int* pageCountOut);

void PaintReadAloudHighlight(MainWindow* win, gpui::PaintCtx* ctx);

bool ReadAloudSentenceRange(Str text, int pos, int* startOut, int* endOut);

// ng: orig has all of section 2 in ReadAloud.cpp. The half that only needs the
// engine and the display model is src/ReadAloudHighlight.cpp so that
// test_util can link ReadAloudHighlight_ut without the whole UI; these are the
// pieces the session half uses
bool IsReadAloudLowerAscii(char c);
bool IsReadAloudLineBreak(char c);
bool IsReadAloudHorizontalSpace(char c);
bool IsReadAloudCloser(int c);
int ReadAloudWordEndUtf8(Str text, int pos);
bool ReadAloudByteLocHasRect(const ReadAloudByteLoc& loc);
Rect ReadAloudByteLocToRect(const ReadAloudByteLoc& loc);
void ReadAloudClampVisual(ReadAloudHighlightMap* map, int wordStartAbs, int wordEndAbs, int* startAbs, int* endAbs);
void ReadAloudAppendUnderlines(DisplayModel* dm, Rect canvasRc, ReadAloudHighlightMap* map, int startAbs, int endAbs,
                               int minThick, int thickDiv, Vec<Rect>& out);

// --- playback bar shown over the canvas while reading ---

void ReadAloudPlaybackBarUpdateSession(WindowTab* tab);
void ReadAloudPlaybackBarHide(MainWindow* win);
void ReadAloudPlaybackBarForgetTab(MainWindow* win, WindowTab* tab);
void ReadAloudPlaybackBarTick(MainWindow* win);

void ReadAloudPlaybackPauseOrResume();
void ReadAloudPlaybackStop();
void ReadAloudPlaybackCycleSpeed(int dir);
void ReadAloudPlaybackBarDestroy(MainWindow* win);
TempStr ReadAloudPlaybackBarStateTemp(int* exitCodeOut);
TempStr ReadAloudPlaybackBarTestTemp(Str action, int* exitCodeOut);
// ng: orig's bar is a WS_POPUP window it keeps aligned with the canvas; here it
// is an element the canvas puts at the bottom of its own rect
gpui::El* ReadAloudPlaybackBarBuild(MainWindow* win, gpui::Ctx* cx);

// --- read-aloud session: what to read, chunking, menus ---

#if OS_WIN
// posted by the tts backend, handled by the native window (gui/NativeWindow.cpp)
constexpr UINT kWmTtsEvent = WM_APP + 0x421;
#endif

constexpr int CmdTtsVoiceDefault = 0x7100;
constexpr int CmdTtsVoiceFirst = 0x7101;
constexpr int CmdTtsVoiceLast = 0x71ff;
constexpr int CmdTtsMenuReadCurrentPage = 0x7200;
constexpr int CmdTtsMenuContinueReading = 0x7201;
constexpr int CmdTtsMenuReadSelection = 0x7202;
constexpr int CmdTtsMenuPauseReading = 0x7203;
constexpr int CmdTtsMenuReadFromCursor = 0x7204;
constexpr int CmdTtsMenuStopReading = 0x7205;
constexpr int CmdTtsSpeedFirst = 0x7300;
constexpr int CmdTtsSpeedLast = 0x730f;

WindowTab* GetReadAloudSourceTab();
void ReadAloudForgetTab(WindowTab*);
void ReadAloudAfterTtsEvents();
bool CanContinueReadAloud(WindowTab* tab);

void ReadAloudInTab(WindowTab* tab);
void ReadAloudContinueInTab(WindowTab* tab);
void ReadAloudSelectionInTab(WindowTab* tab);
void ReadAloudFromViewportTopInTab(WindowTab* tab);
void ReadAloudFromCursorInTab(WindowTab* tab, Point screenPt);
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

// ng: orig rebuilds an HMENU when WM_INITMENUPOPUP arrives; the port rebuilds
// the whole menu model on every frame, so the app submenu / context submenu
// bookkeeping (SetReadAloudAppSubmenu & co) is gone
void RebuildReadAloudMenu(MainWindow* win, MenuModel* menu, bool includeCursorItem = false,
                          bool canReadFromCursor = false);
bool HandleReadAloudMenuCommand(MainWindow* win, int cmdId);

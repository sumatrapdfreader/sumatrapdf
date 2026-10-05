/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AIChatCommon.h. What changed: HANDLE / FILETIME / DWORD are gone
// from the interface (the process plumbing is platform-specific in the .cpp),
// the "not installed" TaskDialog is a gpui dialog, and
// AIChatProvider::virtualHostW (a WCHAR* for WebView2) is gone because
// BrowserView takes the utf-8 host.

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct WindowTab;

constexpr int kAIChatProviderCount = 4;

enum class AIChatBackend {
    Claude,
    Grok,
    Codex,
    AntiGravity,
    None,
};

struct AIChatSessionInfo {
    Str sessionId;
    Str display;
    Str project;
    i64 timestamp;
};

struct AIChatLogger {
    Mutex* mutex;
    Str logFileName;
    Str logTag;
};

struct AIChatNotInstalledDialogArgs {
    Str windowTitle;
    Str mainInstruction;
    Str docUri;
};

// ng: HANDLE -> void*, DWORD -> u32, so the header is portable
struct AIChatProcessLaunchResult {
    bool ok = false;
    void* hProcess = nullptr;
    void* hReadPipe = nullptr;
    u32 processId = 0;
};

// updates flowing from a provider (reader thread or history parsing)
// to the chat UI
enum class AIChatUpdateType {
    Text,
    Tool,
    Error,
    Flush,
    SessionId,
    Finished,
};

// context for parsing a provider's stdout stream on the reader thread
// ng: orig identifies the window by its HWND; here it is the MainWindow*, which
// OnAIChatUpdate re-validates against gWindows before touching it
struct AIChatStreamCtx {
    MainWindow* win = nullptr;
    int providerId = 0;
    Str sessionId; // owned; the session the output belongs to
};

void AIChatPostUpdate(AIChatStreamCtx* ctx, AIChatUpdateType type, Str text);
void AIChatStreamSetSessionId(AIChatStreamCtx* ctx, Str sessionId);

// everything needed to build a provider's command line
struct AIChatCmdArgs {
    Str exePath;
    Str model;
    Str sessionId;
    Str filePath; // document the user is reading
    Str dir;      // cwd for the process
    // Raw user input (not pre-escaped). Providers must pass it through
    // QuoteCmdLineArgTemp when embedding it in a CreateProcessW command line.
    Str escapedInput;
    int option = 0;    // effort / sandbox index
    bool flag = false; // skip permissions / always approve / skip sandbox
    bool isNewSession = false;
};

// a chat CLI backend (Claude Code, Grok Build, OpenAI Codex); the panel UI
// in AIChatPanel.cpp is shared, providers supply everything backend-specific
struct AIChatProvider {
    AIChatBackend backend = AIChatBackend::None;
    AIChatLogger* logger = nullptr;
    Str name;    // "Claude Code", used in user-visible messages
    Str exeName; // "claude", used in error messages
    Str virtualHost;
    Str webViewDataDirPrefix;         // e.g. "ClaudeWebView"
    Str docUri;                       // documentation anchor for the not-installed dialog
    Str defaultModel;                 // fallback when the saved model isn't in the list
    SeqStrings optionItems = nullptr; // items of the effort / sandbox combo
    int optionCount = 0;
    int optionDefault = 1;
    Str checkboxLabel; // "Skip Permissions" / "Always Approve" / "Skip Sandbox"
    // session id is generated before launching the process (Claude);
    // otherwise the provider reports it mid-stream via AIChatStreamSetSessionId
    bool generatesSessionId = false;
    // the provider signals completion before the process exits, so the
    // process is terminated on Finished instead of waited for
    bool terminateOnFinish = false;

    virtual ~AIChatProvider() = default;

    virtual TempStr TitleTemp() = 0;                   // translated "Claude chat"
    virtual TempStr NotInstalledInstructionTemp() = 0; // translated dialog text
    virtual TempStr FindExecutableTemp() = 0;
    // built-in models plus extras from settings
    virtual void BuildModelsList(StrVec& models) = 0;
    // settings accessors (resolved on each call: gSettings can be reloaded)
    virtual Str GetModel() = 0;
    virtual void SetModel(Str) = 0;
    virtual int GetOption() = 0;
    virtual void SetOption(int) = 0;
    virtual bool GetFlag() = 0;
    virtual void SetFlag(bool) = 0;
    virtual Str GetBgColor() = 0;
    virtual void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) = 0;
    // replay a session from disk into the chat via AIChatHistory* helpers
    virtual void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) = 0;
    virtual TempStr BuildCmdLineTemp(const AIChatCmdArgs& args) = 0;
    // parse one line of the process' stdout (reader thread); emit updates
    // via AIChatPostUpdate / AIChatStreamSetSessionId
    virtual void ParseStreamLine(Str line, AIChatStreamCtx* ctx) = 0;

    bool IsInstalled() { return len(FindExecutableTemp()) > 0; }
};

bool IsAIChatAvailable();
bool IsAIChatSupportedForFile(Str filePath, Kind engineKind = nullptr);
bool IsAIChatSupportedForTab(WindowTab* tab);

TempStr AIChatJsEscapeTemp(Str s);
TempStr AIChatJsonStrTemp(Str json, Str key);

// ng: the user's home directory; orig calls GetSpecialFolderTemp(CSIDL_PROFILE)
TempStr AIChatHomeDirTemp();

void AIChatFreeSessions(Vec<AIChatSessionInfo>& sessions);
void AIChatSortSessionsByTimestampDesc(Vec<AIChatSessionInfo>& sessions);
i64 AIChatFileTimeToMs(const FILETIME& ft);

void AIChatLog(AIChatLogger* logger, Str direction, Str text);

// in-memory record of the most recent chat traffic, for debugging failures
void AIChatDebugReset();
TempStr AIChatDebugGetTemp();

// Run one chat turn synchronously (headless) with the given backend, file and
// message, using the same provider code the panel does; returns "OK\n<text>" or
// "FAIL: <reason>\n--- debug log ---\n<log>". For -dbg-control tests.
TempStr AIChatTestResultTemp(int backend, Str filePath, Str message, int* exitCode);

// Inject a canned (user, assistant) turn into the chat webview (opening the grok
// panel if needed) to debug webview rendering without a live provider call.
TempStr AIChatTestReplayResultTemp(Str userMsg, Str response, int* exitCode);

void AIChatShowNotInstalledDialog(MainWindow* win, const AIChatNotInstalledDialogArgs& args);
bool IsAIChatNotInstalledDialogVisible();
void CloseAIChatNotInstalledDialog();
gpui::El* AIChatNotInstalledDialogBuild(MainWindow* win, gpui::Ctx* cx);

TempStr AIChatFindExecutableTemp(const StrVec& fullPathCandidates, Str searchExeName, Str searchNameNoExt = {});

void AIChatAppendModelUnique(StrVec& models, Str model);
int AIChatFindModelInList(const StrVec& models, Str model);
Str AIChatResolveModel(const StrVec& models, Str model, Str defaultModel);
TempStr AIChatModelDisplayNameTemp(Str model, Str defaultDisplay);

TempStr AIChatFormatChatHtmlTemp(Str virtualHost, Str bgColor);

void AIChatCloseProcess(void** processHandle, bool terminateIfRunning);
bool AIChatLaunchProcessWithStdoutPipe(Str cmdLine, Str cwd, AIChatProcessLaunchResult* out);
bool AIChatRunCapture(Str cmdLine, int timeoutMs, str::Builder& out);
int AIChatReadPipeChunk(void* hReadPipe, char* buf, int size);
void AIChatCloseReadPipe(void* hReadPipe);
// reads the whole pipe into `out` and closes it
void AIChatReadPipeToEnd(void* hReadPipe, str::Builder& out);
// waits up to timeoutMs for the process to exit; true if it did
bool AIChatWaitForProcess(void* hProcess, int timeoutMs);
void AIChatTerminateProcess(void* hProcess);

TempStr AIChatGenerateSessionIdTemp();
// a version 4 (random) UUID out of 16 random bytes; in AIChatProcess.cpp so
// that it can be tested without the UI
TempStr AIChatFormatSessionIdTemp(const u8* bytes);

AIChatBackend AIChatGetTabPanelOpen(WindowTab* tab);
void AIChatSetTabPanelOpen(WindowTab* tab, AIChatBackend backend);
void AIChatSyncPanelsToCurrentTab(MainWindow* win);
void AIChatApplySavedSidebarDx(MainWindow* win);
void AIChatUpdateSidebarDx(MainWindow* win, int dx, bool persist);
void AIChatWaitForTabProcessesToFinish(MainWindow* win, bool (*tabHasRunningProcess)(WindowTab*));

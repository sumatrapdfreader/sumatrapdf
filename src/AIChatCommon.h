/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
struct WebViewResourceResult;

constexpr int kAIChatProviderCount = 3;

enum class AIChatBackend {
    Claude,
    Grok,
    Codex,
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

struct AIChatProcessLaunchResult {
    bool ok = false;
    HANDLE hProcess = nullptr;
    HANDLE hReadPipe = nullptr;
    DWORD processId = 0;
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
struct AIChatStreamCtx {
    HWND hwndFrame = nullptr;
    int providerId = 0;
    Str sessionId; // owned; the session the output belongs to
};

// post an update to be applied on the UI thread (implemented in AIChatPanel.cpp)
void AIChatPostUpdate(AIChatStreamCtx* ctx, AIChatUpdateType type, Str text);
// record a session id the provider assigned mid-stream
void AIChatStreamSetSessionId(AIChatStreamCtx* ctx, Str sessionId);

// everything needed to build a provider's command line
struct AIChatCmdArgs {
    Str exePath;
    Str model;
    Str sessionId;
    Str filePath; // document the user is reading
    Str dir;      // cwd for the process
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
    const WCHAR* virtualHostW = nullptr;
    Str webViewDataDirPrefix; // e.g. "ClaudeWebView"
    Str docUri;               // documentation anchor for the not-installed dialog
    Str defaultModel;         // fallback when the saved model isn't in the list
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
    // settings accessors (resolved on each call: gGlobalPrefs can be reloaded)
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

    bool IsInstalled() {
        return len(FindExecutableTemp()) > 0;
    }
};

bool IsAIChatAvailable();
bool IsAIChatSupportedForFile(Str filePath, Kind engineKind = nullptr);
bool IsAIChatSupportedForTab(WindowTab* tab);

TempStr AIChatJsEscapeTemp(Str s);
TempStr AIChatJsonStrTemp(Str json, Str key);

MainWindow* AIChatFindMainWindowByFrame(HWND hwndFrame);

void AIChatFreeSessions(Vec<AIChatSessionInfo>& sessions);
void AIChatSortSessionsByTimestampDesc(Vec<AIChatSessionInfo>& sessions);
i64 AIChatFileTimeToMs(const FILETIME& ft);

void AIChatLog(AIChatLogger* logger, Str direction, Str text);
void AIChatShowNotInstalledDialog(const AIChatNotInstalledDialogArgs& args);

TempStr AIChatFindExecutableTemp(const StrVec& fullPathCandidates, WStr searchExeName, WStr searchNameNoExt = nullptr);

void AIChatAppendModelUnique(StrVec& models, Str model);
int AIChatFindModelInList(const StrVec& models, Str model);
Str AIChatResolveModel(const StrVec& models, Str model, Str defaultModel);
TempStr AIChatModelDisplayNameTemp(Str model, Str defaultDisplay);

bool AIChatGetMarkedJsResource(void* ctx, Str path, WebViewResourceResult* res);
TempStr AIChatFormatChatHtmlTemp(Str virtualHost, Str bgColor);

void AIChatCloseProcess(HANDLE* processHandle, bool terminateIfRunning);
bool AIChatLaunchProcessWithStdoutPipe(Str cmdLine, Str cwd, AIChatProcessLaunchResult* out);

int AIChatLabelMaxTextDx(HWND labelHwnd, int labelDx);
TempStr AIChatFitPanelTitleTemp(HWND labelHwnd, HFONT font, Str prefix, Str docName, int maxDx);
TempStr AIChatGenerateSessionIdTemp();

AIChatBackend AIChatGetTabPanelOpen(WindowTab* tab);
void AIChatSetTabPanelOpen(WindowTab* tab, AIChatBackend backend);
void AIChatSyncPanelsToCurrentTab(MainWindow* win);
void AIChatApplySavedSidebarDx(MainWindow* win);
void AIChatUpdateSidebarDx(MainWindow* win, int dx, bool persist);
void AIChatWaitForTabProcessesToFinish(MainWindow* win, bool (*tabHasRunningProcess)(WindowTab*));

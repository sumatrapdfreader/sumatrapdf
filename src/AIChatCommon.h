/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
struct WebViewResourceResult;

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
TempStr AIChatModelDisplayNameTemp(Str model, Str defaultDisplay);

bool AIChatGetMarkedJsResource(void* ctx, Str path, WebViewResourceResult* res);
TempStr AIChatFormatChatHtmlTemp(Str virtualHost, Str bgColor);

void AIChatCloseProcess(HANDLE* processHandle, bool terminateIfRunning);
bool AIChatLaunchProcessWithStdoutPipe(Str cmdLine, Str cwd, AIChatProcessLaunchResult* out);

int AIChatLabelMaxTextDx(HWND labelHwnd, int labelDx);
TempStr AIChatFitPanelTitleTemp(HWND labelHwnd, HFONT font, Str prefix, Str docName, int maxDx);
Str AIChatGenerateSessionId();

AIChatBackend AIChatGetTabPanelOpen(WindowTab* tab);
void AIChatSetTabPanelOpen(WindowTab* tab, AIChatBackend backend);
void AIChatSyncPanelsToCurrentTab(MainWindow* win);
void AIChatApplySavedSidebarDx(MainWindow* win);
void AIChatUpdateSidebarDx(MainWindow* win, int dx, bool persist);
void AIChatWaitForTabProcessesToFinish(MainWindow* win, bool (*tabHasRunningProcess)(WindowTab*));
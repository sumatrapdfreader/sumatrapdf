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
    char* sessionId;
    char* display;
    char* project;
    i64 timestamp;
};

struct AIChatLogger {
    Mutex* mutex;
    const char* logFileName;
    const char* logTag;
};

struct AIChatNotInstalledDialogArgs {
    const char* windowTitle;
    const char* mainInstruction;
    const char* docUri;
};

struct AIChatProcessLaunchResult {
    bool ok = false;
    HANDLE hProcess = nullptr;
    HANDLE hReadPipe = nullptr;
    DWORD processId = 0;
};

bool IsAIChatAvailable();
bool IsAIChatSupportedForFile(const char* filePath, Kind engineKind = nullptr);
bool IsAIChatSupportedForTab(WindowTab* tab);

TempStr AIChatJsEscapeTemp(const char* s);
TempStr AIChatJsonStrTemp(const char* json, const char* key);

MainWindow* AIChatFindMainWindowByFrame(HWND hwndFrame);

void AIChatFreeSessions(Vec<AIChatSessionInfo>& sessions);
void AIChatSortSessionsByTimestampDesc(Vec<AIChatSessionInfo>& sessions);
i64 AIChatFileTimeToMs(const FILETIME& ft);

void AIChatLog(AIChatLogger* logger, const char* direction, const char* text);
void AIChatShowNotInstalledDialog(const AIChatNotInstalledDialogArgs& args);

TempStr AIChatFindExecutableTemp(const StrVec& fullPathCandidates, const WCHAR* searchExeName,
                                 const WCHAR* searchNameNoExt = nullptr);

void AIChatAppendModelUnique(StrVec& models, const char* model);
int AIChatFindModelInList(const StrVec& models, const char* model);
TempStr AIChatModelDisplayNameTemp(const char* model, const char* defaultDisplay);

bool AIChatGetMarkedJsResource(void* ctx, const char* path, WebViewResourceResult* res);
TempStr AIChatFormatChatHtmlTemp(const char* virtualHost, const char* bgColor);

void AIChatCloseProcess(HANDLE* processHandle, bool terminateIfRunning);
bool AIChatLaunchProcessWithStdoutPipe(const char* cmdLine, const char* cwd, AIChatProcessLaunchResult* out);

int AIChatLabelMaxTextDx(HWND labelHwnd, int labelDx);
TempStr AIChatFitPanelTitleTemp(HWND labelHwnd, HFONT font, const char* prefix, const char* docName, int maxDx);
char* AIChatGenerateSessionId();

AIChatBackend AIChatGetTabPanelOpen(WindowTab* tab);
void AIChatSetTabPanelOpen(WindowTab* tab, AIChatBackend backend);
void AIChatSyncPanelsToCurrentTab(MainWindow* win);
void AIChatWaitForTabProcessesToFinish(MainWindow* win, bool (*tabHasRunningProcess)(WindowTab*));
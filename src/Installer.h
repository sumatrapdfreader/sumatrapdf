/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct VirtRoot;

constexpr int kInstallerWinDy = 340;

enum class PreviousInstallationType {
    None = 0,
    User = 1,
    Machine = 2,
    Both = 3
};

struct PreviousInstallationInfo {
    Str installationDir;
    PreviousInstallationType typ = PreviousInstallationType::None;
    bool searchFilterInstalled = false;
    bool previewInstalled = false;
    bool allUsers = false;

    PreviousInstallationInfo() = default;
    ~PreviousInstallationInfo();
};

// This is the height of the lower part
extern int gBottomPartDy;

extern int gButtonDy;

constexpr int kWmAppInstallationFinished = (WM_APP + 1);
constexpr int kWmAppStartInstallation = (WM_APP + 2);

extern Str gFirstError;
extern Str gDefaultMsg;
extern HWND gHwndFrame;
extern Str gMsgError;

extern Gdiplus::Color kColorMsgWelcome;
extern Gdiplus::Color kColorMsgOk;
extern Gdiplus::Color kColorMsgInstallation;
extern Gdiplus::Color kColorMsgFailed;
extern Gdiplus::Color gCol1;
extern Gdiplus::Color gCol1Shadow;
extern Gdiplus::Color gCol2;
extern Gdiplus::Color gCol2Shadow;
extern Gdiplus::Color gCol3;
extern Gdiplus::Color gCol3Shadow;
extern Gdiplus::Color gCol4;
extern Gdiplus::Color gCol4Shadow;
extern Gdiplus::Color gCol5;
extern Gdiplus::Color gCol5Shadow;

// virt: the window's virtual controls, painted on top of the frame (can be null)
void OnPaintFrame(HWND hwnd, bool skipMessage, VirtRoot* virt = nullptr);
void AnimStep();

void NotifyFailed(Str msg);

void SetMsg(Str msg, Gdiplus::Color color);
void SetDefaultMsg();

int KillProcessesWithModule(Str modulePath, bool waitUntilTerminated);

TempStr GetShortcutPathTemp(int csidl);

bool ExtractInstallerFiles(Str dir);
bool ExtractLibsumatrapdfToDir(Str destDir);

TempStr GetExistingInstallationDirTemp();
void GetPreviousInstallInfo(PreviousInstallationInfo* info);
bool IsOurExeInstalled();

bool IsPathUnderProgramFiles(Str path);
bool InstallNeedsElevation(Str installDir, bool allUsers);

TempStr GetInstallationFilePathTemp(Str installDir, Str name);

void RegisterPreviewer(bool allUsers, Str installDir);
void UnRegisterPreviewer();

void RegisterSearchFilter(bool allUsers, Str installDir);
void UnRegisterSearchFilter();

// Unregister shell extensions and kill processes holding install-dir files
// so ExtractInstallerFiles can overwrite PdfFilter.dll / PdfPreview.dll / etc.
// Call before extracting over an existing install. removedOut (optional) is
// filled for RestoreShellExtensions if install fails later.
struct ShellExtInstallState {
    bool searchFilter = false;
    bool preview = false;
    bool allUsers = false;
    Str installDir;
};
void FreeInstallationFilesInUse(Str installDir, bool allUsers, ShellExtInstallState* removedOut = nullptr);
void RestoreShellExtensions(const ShellExtInstallState& state);

bool CheckInstallUninstallPossible(HWND hwnd, bool silent = false);
Str GetInstallerLogPath();

bool IsDirInPath(Str path, Str dir);
bool WriteRegExpandSz(HKEY root, Str keyName, Str valueName, Str value);

TempStr GetRegPathUninstTemp(Str appName);

void RemoveAppShortcuts();

bool WriteUninstallerRegistryInfo(HKEY hkey, bool allUsers, Str installDir);
bool WriteExtendedFileExtensionInfo(HKEY hkey, Str installedExePath);
bool RemoveUninstallerRegistryInfo(HKEY hkey);
void RemoveInstallRegistryKeys(HKEY hkey);
int GetInstallerWinDx();

void ReRegisterFileAssociations();
void LogNonDefaultRegisteredExtensions();
void CollectNonDefaultRegisteredExtensions(StrVec& out);
void LaunchDefaultAppDialogForExtension(HWND hwnd, Str ext);

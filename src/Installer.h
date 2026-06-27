/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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

#define WM_APP_INSTALLATION_FINISHED (WM_APP + 1)
#define WM_APP_START_INSTALLATION (WM_APP + 2)

extern Str gFirstError;
extern Str gDefaultMsg;
extern HWND gHwndFrame;
extern Str gMsgError;

extern Gdiplus::Color COLOR_MSG_WELCOME;
extern Gdiplus::Color COLOR_MSG_OK;
extern Gdiplus::Color COLOR_MSG_INSTALLATION;
extern Gdiplus::Color COLOR_MSG_FAILED;
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

void OnPaintFrame(HWND hwnd, bool skipoMessage);
void AnimStep();

void NotifyFailed(Str msg);

void SetMsg(Str msg, Gdiplus::Color color);
void SetDefaultMsg();

int KillProcessesWithModule(Str modulePath, bool waitUntilTerminated);

TempStr GetShortcutPathTemp(int csidl);

bool ExtractInstallerFiles(Str dir);
u32 GetLibmupdfDllSize();
bool ExtractLibmupdfDll(Str destDir);

Str GetExistingInstallationDir();
void GetPreviousInstallInfo(PreviousInstallationInfo* info);
bool IsOurExeInstalled();

TempStr GetInstallationFilePathTemp(Str installDir, Str name);

void RegisterPreviewer(bool allUsers, Str installDir);
void UnRegisterPreviewer();

void RegisterSearchFilter(bool allUsers, Str installDir);
void UnRegisterSearchFilter();

void UninstallBrowserPlugin();

bool CheckInstallUninstallPossible(HWND hwnd, bool silent = false);
Str GetInstallerLogPath();

TempStr GetRegPathUninstTemp(Str appName);

// Installer.cpp
void RemoveAppShortcuts();

// RegistryInstaller.cpp

bool WriteUninstallerRegistryInfo(HKEY hkey, bool allUsers, Str installedExePat);
bool WriteExtendedFileExtensionInfo(HKEY hkey, Str installedExePat);
bool RemoveUninstallerRegistryInfo(HKEY hkey);
void RemoveInstallRegistryKeys(HKEY hkey);
int GetInstallerWinDx();

void ReRegisterFileAssociations();

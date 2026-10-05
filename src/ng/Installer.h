/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's installer is a second win32 UI inside the same exe, driven by an
// lzsa payload (libsumatrapdf.dll, PdfFilter.dll, PdfPreview.dll) appended to
// the installer build. This port links one static exe and dropped the
// previewer / search filter in step 17a, so the payload is gone: installing is
// copying ourselves plus the registry work. The window is gpui
// (Installer.cpp / Uninstaller.cpp), everything here is Windows-only.

constexpr int kInstallerWinDy = 340;

// DWORD 0|1 in the uninstall key: whether the install created a desktop shortcut
#define kRegDesktopShortcut "DesktopShortcut"

enum class PreviousInstallationType {
    None = 0,
    User = 1,
    Machine = 2,
    Both = 3
};

struct PreviousInstallationInfo {
    Str installationDir;
    PreviousInstallationType typ = PreviousInstallationType::None;
    bool allUsers = false;
    // installs before the DesktopShortcut registry value always created one
    bool desktopShortcut = true;

    PreviousInstallationInfo() = default;
    ~PreviousInstallationInfo();
};

struct Flags;

// ng: orig reads the global `gCli`; the port's parsed command line is private
// to SumatraPDF.cpp, so it is passed in and kept in `gCli` here
int RunInstaller(Flags* cli);
int RunUninstaller(Flags* cli);

#if OS_WIN

extern Flags* gCli;

// the installer / uninstaller message under the logo and its color
extern Str gFirstError;
extern Str gDefaultMsg;
extern Str gMsg;
extern Color gMsgColor;
extern Str gMsgError;

extern Color kColorMsgWelcome;
extern Color kColorMsgOk;
extern Color kColorMsgInstallation;
extern Color kColorMsgFailed;

constexpr Color kInstallerWinBgColor = MkRgb(0xff, 0xf2, 0); // yellow

// This display is inspired by http://letteringjs.com/
struct LetterInfo {
    // part that doesn't change
    char c;
    Color col, colShadow;
    float rotation;
    float dyOff; // displacement
};

constexpr int kSumatraLettersCount = 10;
extern LetterInfo gLetters[kSumatraLettersCount];

void RevealingLettersAnimStart();
void AnimStep();
bool IsRevealingLettersAnimRunning();

void NotifyFailed(Str msg);

void SetMsg(Str msg, Color color);
void SetDefaultMsg();

int KillProcessesWithModule(Str modulePath, bool waitUntilTerminated);

TempStr GetShortcutPathTemp(int csidl);

TempStr GetExistingInstallationDirTemp();
void GetPreviousInstallInfo(PreviousInstallationInfo* info);
bool IsOurExeInstalled();

bool IsPathUnderProgramFiles(Str path);
bool InstallNeedsElevation(Str installDir, bool allUsers);

TempStr GetInstallationFilePathTemp(Str installDir, Str name);

bool CheckInstallUninstallPossible(bool silent = false);
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
void LaunchDefaultAppDialogForExtension(Str ext);

// ng: orig writes to the real HKLM / HKCU. `-install-reg-root <key>` points
// both of them at subkeys of HKCU\<key> for this process
// (RegOverridePredefKey), so an installer run can be exercised without
// touching the user's file associations.
bool SetInstallRegistryTestRoot(Str keyName);

#else

inline bool IsOurExeInstalled() {
    return false;
}
inline void ReRegisterFileAssociations() {}

#endif

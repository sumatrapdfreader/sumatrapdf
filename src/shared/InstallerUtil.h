/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by InstallerUtil_win.cpp and each app's Installer.cpp, InstallerCommon.cpp, Uninstaller.cpp ---

constexpr DWORD kTenSecondsInMs = 10 * 1000;
bool IsProcWithModule(DWORD processId, Str modulePath);
bool KillProcWithId(DWORD processId, bool waitUntilTerminated);
Str ReadableProcName(Str procPath);
extern PreviousInstallationInfo gPrevInstall;
extern Flags gCliNew;
bool HasPreviousInstall();
void ClearReadOnly(Str path);
bool IsDiskFullError(DWORD err);
void CopySettingsFile();
void CreateAppShortcuts(bool forAllUsers, bool withDesktop, Str installedExePath);
void AddInstallDirToPath(bool allUsers, Str installDir);
TempStr GetInstalledExePathTemp(Flags* cli);
void StartSumatra();
TempStr GetDefaultInstallationDirTemp(bool forAllUsers, bool ignorePrev);
void RelaunchMaybeElevatedFromTempDirectory(Flags* cli);
void InitSelfDelete();
void RemoveInstallDirFromPath(bool allUsers, Str installDir);
TempStr GetInstalledExePathTemp();
Str GetEnvRegKey(bool allUsers);

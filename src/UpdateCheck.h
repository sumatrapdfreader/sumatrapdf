/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class UpdateCheck {
    UserInitiated, // user used menu "Check update"
    Automatic,     // an automatic, periodic check done at startup
};

void StartAsyncUpdateCheck(MainWindow* win, UpdateCheck updateCheckType);
void DownloadAndInstallPendingUpdate(MainWindow* win);
void StartInstallerAutoUpgrade(Str installerPath);
void UpdateSelfTo(Str dstPath);

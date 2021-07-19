/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/SquareTreeParser.h"
#include "utils/HttpUtil.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"

#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "AppUtil.h"
#include "AppTools.h"
#include "AppPrefs.h"
#include "Version.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "Flags.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "SumatraDialogs.h"

constexpr int kSecondsInDay = 60 * 60 * 24;

// this is a command to run on exit to auto-update ourselves
const WCHAR* autoUpdateExitCmd{nullptr};

// the default is for pre-release version.
// for release we override BuildConfig.h and set to
// clang-format off
#if defined(SUMATRA_UPDATE_INFO_URL)
constexpr const WCHAR* kUpdateInfoURL = SUMATRA_UPDATE_INFO_URL;
#else
constexpr const WCHAR* kUpdateInfoURL = L"https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/sumpdf-prerelease-update.txt";

//constexpr const WCHAR* kUpdateInfoURL = L"https://www.sumatrapdfreader.org/api/updatecheck";
#endif
#ifndef WEBSITE_DOWNLOAD_PAGE_URL
#if defined(PRE_RELEASE_VER)
#define WEBSITE_DOWNLOAD_PAGE_URL L"https://www.sumatrapdfreader.org/prerelease.html"
#else
#define WEBSITE_DOWNLOAD_PAGE_URL L"https://www.sumatrapdfreader.org/download-free-pdf-viewer.html"
#endif
#endif
// clang-format on

// prevent multiple update tasks from happening simultaneously
// (this might e.g. happen if a user checks manually very quickly after startup)
bool gUpdateCheckInProgress = false;

/*
The format of update information downloaded from the server:

[SumatraPDF]
Latest: 13682
Installer64:
https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerel-13682-64-install.exe
Installer32: https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerel-13682-install.exe
PortableExe64: https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerel-13682-64.exe
PortableExe32: https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerel-13682.exe
PortableZip64: https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerel-13682-64.zip
PortableZip32: https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerel-13682.zip

*/
static DWORD ShowAutoUpdateDialog(HWND hParent, HttpRsp* rsp, bool silent) {
    if (rsp->error != 0) {
        return rsp->error;
    }
    if (rsp->httpStatusCode != 200) {
        return ERROR_INTERNET_INVALID_URL;
    }
    if (!str::StartsWith(rsp->url.Get(), kUpdateInfoURL)) {
        return ERROR_INTERNET_INVALID_URL;
    }
    str::Str* data = &rsp->data;
    if (0 == data->size()) {
        return ERROR_INTERNET_CONNECTION_ABORTED;
    }

    // See https://code.google.com/p/sumatrapdf/issues/detail?id=725
    // If a user configures os-wide proxy that is not regular ie proxy
    // (which we pick up) we might get complete garbage in response to
    // our query. Make sure to check whether the returned data is sane.
    if (!str::StartsWith(data->Get(), '[' == data->at(0) ? "[SumatraPDF]" : "SumatraPDF")) {
        return ERROR_INTERNET_LOGIN_FAILURE;
    }

    SquareTree tree(data->Get());
    SquareTreeNode* node = tree.root ? tree.root->GetChild("SumatraPDF") : nullptr;
    const char* latestVer = node ? node->GetValue("Latest") : nullptr;
    if (!latestVer || !IsValidProgramVersion(latestVer)) {
        logf("ShowAutoUpdateDialog: '%s' is not a valid version\n", latestVer ? latestVer : "");
        return ERROR_INTERNET_INCORRECT_FORMAT;
    }

    const char* myVer = UPDATE_CHECK_VERA;
    // myVer = L"3.1"; // for ad-hoc debugging of auto-update code
    bool hasUpdate = CompareVersion(latestVer, myVer) > 0;
    if (!hasUpdate) {
        logf("ShowAutoUpdateDialog: myVer >= latestVer ('%s' >= '%s')\n", myVer, latestVer);
        /* if automated => don't notify that there is no new version */
        if (!silent) {
            uint flags = MB_ICONINFORMATION | MB_OK | MB_SETFOREGROUND | MB_TOPMOST;
            MessageBoxW(hParent, _TR("You have the latest version."), _TR("SumatraPDF Update"), flags);
        }
        return 0;
    }

    // if automated, respect gGlobalPrefs->versionToSkip
    if (silent && str::EqI(gGlobalPrefs->versionToSkip, latestVer)) {
        logf(
            "ShowAutoUpdateDialog: skipping because silent and gGlobalPrefs->versionToSkip == latestVer ('%s' == "
            "'%s')\n",
            gGlobalPrefs->versionToSkip, latestVer);
        return 0;
    }
    // if silent we do auto-update. for now only in pre-release builds
    if (gCli->testAutoUpdate && gIsPreReleaseBuild && silent) {
        // figure out which executable to download
        const char* dlURLA{nullptr};
        const char* dlKey{nullptr};
        bool isDll = IsDllBuild();
        if (IsProcess64()) {
            if (isDll) {
                dlKey = "Installer64";
            } else {
                dlKey = "PortableExe64";
            }
        } else {
            if (isDll) {
                dlKey = "Installer32";
            } else {
                dlKey = "PortableExe32";
            }
        }

        dlURLA = node->GetValue(dlKey);
        if (!dlURLA) {
            // shouldn't happen
            logf("ShowAutoUpdateDialog: didn't find valud for key '%s'\nAuto update data:\n%s\n", dlKey, data->Get());
            goto AskUser;
        }
        logf("ShowAutoUpdateDialog: starting to download '%s'\n", dlURLA);
        WCHAR* dlURL = strconv::Utf8ToWstr(dlURLA); // must make a copy to be valid in a thread
        RunAsync([dlURL, isDll] {                   // NOLINT
            auto installerPath = path::GetTempFilePath(L"sumatra-installer");
            // the installer must be named .exe or it won't be able to self-elevate
            // with "runas"
            installerPath = str::JoinTemp(installerPath, L".exe");
            bool ok = HttpGetToFile(dlURL, installerPath);
            logf("ShowAutoUpdateDialog: HttpGetToFile(): ok=%d, downloaded to '%s'\n", (int)ok,
                 ToUtf8Temp(installerPath).Get());
            str::Free(dlURL);
            if (ok) {
                str::WStr cmd(installerPath);
                if (isDll) {
                    // this should be an installer
                    // TODO: add -silent when tested that it's working
                    cmd.Append(L" -install");
                } else {
                    auto selfPath = GetExePathTemp();
                    cmd.Append(L" -copy-self-to \"");
                    cmd.Append(selfPath);
                    cmd.Append(L"\"");
                }
                // technically should protect with a mutex or sth.
                autoUpdateExitCmd = cmd.StealData();
                auto s = ToUtf8Temp(autoUpdateExitCmd);
                logf("ShowAutoUpdateDialog: set exit cmd to '%s'\n", s.Get());
            }
        });

        return 0;
    }

AskUser:
    // ask whether to download the new version and allow the user to
    // either open the browser, do nothing or don't be reminded of
    // this update ever again
    bool skipThisVersion = false;
    INT_PTR res = Dialog_NewVersionAvailable(hParent, myVer, latestVer, &skipThisVersion);
    if (skipThisVersion) {
        str::ReplaceWithCopy(&gGlobalPrefs->versionToSkip, latestVer);
    }
    if (IDYES == res) {
        SumatraLaunchBrowser(WEBSITE_DOWNLOAD_PAGE_URL);
    }
    prefs::Save();

    return 0;
}

static void ProcessAutoUpdateCheckResult(HWND hwnd, HttpRsp* rsp, bool autoCheck) {
    DWORD error = ShowAutoUpdateDialog(hwnd, rsp, autoCheck);
    if (error != 0 && !autoCheck) {
        // notify the user about network error during a manual update check
        AutoFreeWstr msg(str::Format(_TR("Can't connect to the Internet (error %#x)."), error));
        MessageBoxWarning(hwnd, msg, _TR("SumatraPDF Update"));
    }
}

// start auto-update check by downloading auto-update information from url
// on a background thread and processing the retrieved data on ui thread
// if autoCheck is true, this is a check *not* triggered by explicit action
// of the user and therefore will show less UI
void UpdateCheckAsync(WindowInfo* win, bool autoCheck) {
    if (gUpdateCheckInProgress) {
        logf("UpdateCheckAsync: skipping because gUpdateCheckInProgress\n");
        return;
    }

    if (!HasPermission(Perm::InternetAccess)) {
        logf("UpdateCheckAsync: skipping because no internet access\n");
        return;
    }

    // For auto-check, only check if at least a day passed since last check
    if (autoCheck) {
        // don't check if the timestamp or version to skip can't be updated
        // (mainly in plugin mode, stress testing and restricted settings)
        if (!HasPermission(Perm::SavePreferences)) {
            logf("UpdateCheckAsync: skipping auto check because no prefs access\n");
            return;
        }

        // when testing, ignore the time check
        if (!gCli->testAutoUpdate) {
            // don't check for updates at the first start, so that privacy
            // sensitive users can disable the update check in time
            FILETIME never = {0};
            if (FileTimeEq(gGlobalPrefs->timeOfLastUpdateCheck, never)) {
                return;
            }

            FILETIME currentTimeFt;
            GetSystemTimeAsFileTime(&currentTimeFt);
            int secs = FileTimeDiffInSecs(currentTimeFt, gGlobalPrefs->timeOfLastUpdateCheck);
            // if secs < 0 => somethings wrong, so ignore that case
            if ((secs >= 0) && (secs < kSecondsInDay)) {
                return;
            }
        }
    }

    GetSystemTimeAsFileTime(&gGlobalPrefs->timeOfLastUpdateCheck);
    gUpdateCheckInProgress = true;
    HWND hwnd = win->hwndFrame;
    str::WStr url = kUpdateInfoURL;
    url.Append(L"?v=");
    url.Append(UPDATE_CHECK_VER);
    HttpGetAsync(url.Get(), [=](HttpRsp* rsp) {
        gUpdateCheckInProgress = false;
        uitask::Post([=] { ProcessAutoUpdateCheckResult(hwnd, rsp, autoCheck); });
    });
}

void TryAutoUpdateSelf() {
    if (!autoUpdateExitCmd) {
        return;
    }
    logf(L"TryAutoUpdateSelf: launching '%s'\n", autoUpdateExitCmd);
    LaunchProcess(autoUpdateExitCmd);
    str::Free(autoUpdateExitCmd);
}

void CopySelfTo(const WCHAR* path) {
    CrashIf(!path);
    logf(L"CopySelfTo: '%s'\n", path);

    if (!file::Exists(path)) {
        logf("CopySelfTo: failed because destination doesn't exist\n");
        return;
    }

    const WCHAR* src = GetExePathTemp().Get();
    bool ok = file::Copy(path, src, false);
    // TODO: maybe retry if copy fails under the theory that the file
    // might be temporarily locked
    if (!ok) {
        logf("CopySelfTo: failed to copy self to file\n");
    } else {
        logf("CopySelfTo: copied self to file\n");
    }
}

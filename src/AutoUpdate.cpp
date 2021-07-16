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

#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "AppUtil.h"
#include "AppTools.h"
#include "AppPrefs.h"
#include "Version.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "SumatraDialogs.h"

#define SECS_IN_DAY 60 * 60 * 24

// the default is for pre-release version.
// for release we override BuildConfig.h and set to
// clang-format off
#if defined(SUMATRA_UPDATE_INFO_URL)
static const WCHAR* gUpdateInfoURL = SUMATRA_UPDATE_INFO_URL;
#else
static const WCHAR* gUpdateInfoURL = L"https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/sumpdf-prerelease-update.txt";

//static const WCHAR* gUpdateInfoURL = L"https://www.sumatrapdfreader.org/update-check-rel.txt";
#endif
#ifndef WEBSITE_DOWNLOAD_PAGE_URL
#if defined(PRE_RELEASE_VER)
#define WEBSITE_DOWNLOAD_PAGE_URL L"https://www.sumatrapdfreader.org/prerelease.html"
#else
#define WEBSITE_DOWNLOAD_PAGE_URL L"https://www.sumatrapdfreader.org/download-free-pdf-viewer.html"
#endif
#endif
// clang-format on

/* The format used for SUMATRA_UPDATE_INFO_URL looks as follows:

[SumatraPDF]
# the first line must start with SumatraPDF (optionally as INI header)
Latest 2.6
# Latest must be the version number of the version currently offered for download
Stable 2.5.3
# Stable is optional and indicates the oldest version for which automated update
# checks don't yet report the available update
*/
static DWORD ShowAutoUpdateDialog(HWND hParent, HttpRsp* rsp, bool silent) {
    if (rsp->error != 0) {
        return rsp->error;
    }
    if (rsp->httpStatusCode != 200) {
        return ERROR_INTERNET_INVALID_URL;
    }
    if (!str::StartsWith(rsp->url.Get(), gUpdateInfoURL)) {
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
        return ERROR_INTERNET_INCORRECT_FORMAT;
    }

    const char* myVer = UPDATE_CHECK_VERA;
    // myVer = L"3.1"; // for ad-hoc debugging of auto-update code
    bool hasUpdate = CompareVersion(latestVer, myVer) > 0;
    if (!hasUpdate) {
        /* if automated => don't notify that there is no new version */
        if (!silent) {
            uint flags = MB_ICONINFORMATION | MB_OK | MB_SETFOREGROUND | MB_TOPMOST;
            MessageBoxW(hParent, _TR("You have the latest version."), _TR("SumatraPDF Update"), flags);
        }
        return 0;
    }

    if (silent) {
        const char* stable = node->GetValue("Stable");
        if (stable && IsValidProgramVersion(stable) && CompareVersion(stable, myVer) <= 0) {
            // don't update just yet if the older version is still marked as stable
            return 0;
        }
    }

    // if automated, respect gGlobalPrefs->versionToSkip
    if (silent && str::EqI(gGlobalPrefs->versionToSkip, latestVer)) {
        return 0;
    }

    // figure out which executable to download
    const char* dlLink{nullptr};
    const char* dlKey{nullptr};
    if (IsProcess64()) {
        if (IsDllBuild()) {
            dlKey = "Installer64";
        } else {
            dlKey = "PortableExe64";
        }
    } else {
        if (IsDllBuild()) {
            dlKey = "Installer32";
        } else {
            dlKey = "PortableExe32";
        }
    }

    dlLink = node->GetValue(dlKey);
    logf("dlLink: '%s'\n", dlLink);

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

// prevent multiple update tasks from happening simultaneously
// (this might e.g. happen if a user checks manually very quickly after startup)
bool gUpdateTaskInProgress = false;

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
    if (!HasPermission(Perm::InternetAccess)) {
        return;
    }

    // For auto-check, only check if at least a day passed since last check
    if (false && autoCheck) {
        // don't check if the timestamp or version to skip can't be updated
        // (mainly in plugin mode, stress testing and restricted settings)
        if (!HasPermission(Perm::SavePreferences)) {
            return;
        }

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
        if ((secs >= 0) && (secs < SECS_IN_DAY)) {
            return;
        }
    }

    GetSystemTimeAsFileTime(&gGlobalPrefs->timeOfLastUpdateCheck);
    if (gUpdateTaskInProgress) {
        return;
    }
    gUpdateTaskInProgress = true;
    HWND hwnd = win->hwndFrame;
    str::WStr url = gUpdateInfoURL;
    url.Append(L"?v=");
    url.Append(UPDATE_CHECK_VER);
    HttpGetAsync(url.Get(), [=](HttpRsp* rsp) {
        gUpdateTaskInProgress = false;
        uitask::Post([=] { ProcessAutoUpdateCheckResult(hwnd, rsp, autoCheck); });
    });
}

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/SquareTreeParser.h"
#include "utils/HttpUtil.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"

#include "wingui/Layout.h"
#include "wingui/UIModels.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "Flags.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "SumatraDialogs.h"
#include "UpdateCheck.h"

#include "utils/Log.h"

static const char* kindNotifUpdateCheckInProgress = "notifUpdateCheckInProgress";

// for testing. if true will ignore version checks etc. and act like there's an update
constexpr bool gForceAutoUpdate = false;

// certificate on www.sumatrapdfreader.org is not supported by win7 and win8.1
// (doesn't have the ciphers they understand)
// so we first try sumatra-website.onrender.com which should work
// clang-format off
#if defined(PRE_RELEASE_VER)
//https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/sumpdf-prerelease-update.txt
constexpr const char* kUpdateInfoURL = "https://www.sumatrapdfreader.org/updatecheck-pre-release.txt";
constexpr const char* kUpdateInfoURL2 = "https://sumatra-website.onrender.com/updatecheck-pre-release.txt";
#else
constexpr const char* kUpdateInfoURL = "https://www.sumatrapdfreader.org/update-check-rel.txt";
constexpr const char* kUpdateInfoURL2 = "https://sumatra-website.onrender.com/update-check-rel.txt";
#endif

#ifndef WEBSITE_DOWNLOAD_PAGE_URL
#if defined(PRE_RELEASE_VER)
#define WEBSITE_DOWNLOAD_PAGE_URL "https://www.sumatrapdfreader.org/prerelease"
#else
#define WEBSITE_DOWNLOAD_PAGE_URL "https://www.sumatrapdfreader.org/download-free-pdf-viewer"
#endif
#endif
// clang-format on

// prevent multiple update tasks from happening simultaneously
// (this might e.g. happen if a user checks manually very quickly after startup)
bool gUpdateCheckInProgress = false;

struct UpdateInfo {
    HWND hwndParent = nullptr;
    const char* latestVer = nullptr;
    const char* installer64 = nullptr;
    const char* installerArm64 = nullptr;
    const char* installer32 = nullptr;
    const char* portable64 = nullptr;
    const char* portableArm64 = nullptr;
    const char* portable32 = nullptr;

    const char* dlURL = nullptr;
    const char* installerPath = nullptr;

    UpdateInfo() = default;
    ~UpdateInfo() {
        str::Free(latestVer);
        str::Free(installer64);
        str::Free(installerArm64);
        str::Free(installer32);
        str::Free(portable64);
        str::Free(portableArm64);
        str::Free(portable32);
        str::Free(dlURL);
        str::Free(installerPath);
    }
};

/*
The format of update information downloaded from the server:

[SumatraPDF]
Latest: 14276
Installer64:
https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-64-install.exe
Installer32: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-install.exe
PortableExe64: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-64.exe
PortableExe32: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel.exe
PortableZip64: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-64.zip
PortableZip32: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel.zip
*/
static UpdateInfo* ParseUpdateInfo(const char* d) {
    // if a user configures os-wide proxy that is not a regular ie proxy
    // (which we pick up) we might get garbage http response
    // check if response looks valid
    if (!str::StartsWith(d, '[' == d[0] ? "[SumatraPDF]" : "SumatraPDF")) {
        return nullptr;
    }

    SquareTree tree(d);
    if (!tree.root) {
        return nullptr;
    }
    SquareTreeNode* node = tree.root->GetChild("SumatraPDF");
    if (!node) {
        return nullptr;
    }

    const char* latestVer = node->GetValue("Latest");
    if (!IsValidProgramVersion(latestVer)) {
        return nullptr;
    }
    auto res = new UpdateInfo();
    res->latestVer = str::Dup(latestVer);

    // those are optional. if missing, we'll just tell the user to go to website to download
    res->installer64 = str::Dup(node->GetValue("Installer64"));
    res->installerArm64 = str::Dup(node->GetValue("InstallerArm64"));
    res->installer32 = str::Dup(node->GetValue("Installer32"));
    res->portable64 = str::Dup(node->GetValue("PortableExe64"));
    res->portableArm64 = str::Dup(node->GetValue("PortableExeArm64"));
    res->portable32 = str::Dup(node->GetValue("PortableExe32"));

    // figure out which executable to download
    const char* dlURL = nullptr;
    bool isDll = IsDllBuild();
    if (IsArmBuild()) {
        dlURL = isDll ? res->installerArm64 : res->portableArm64;
    } else if (IsProcess64()) {
        dlURL = isDll ? res->installer64 : res->portable64;
    } else {
        dlURL = isDll ? res->installer32 : res->portable32;
    }
    res->dlURL = str::Dup(dlURL);
    return res;
}

static bool ShouldCheckForUpdate(UpdateCheck updateCheckType) {
    if (gUpdateCheckInProgress) {
        logf("CheckForUpdate: skipping because gUpdateCheckInProgress\n");
        return false;
    }

    if (gForceAutoUpdate) {
        return true;
    }

    if (!HasPermission(Perm::InternetAccess)) {
        logf("CheckForUpdate: skipping because no internet access\n");
        return false;
    }

    if (updateCheckType == UpdateCheck::UserInitiated) {
        return true;
    }

    // don't check if the timestamp or version to skip can't be updated
    // (mainly in plugin mode, stress testing and restricted settings)
    if (!HasPermission(Perm::SavePreferences)) {
        logf("CheckForUpdate: skipping auto check because no prefs access\n");
        return false;
    }

    // only applies to automatic update check
    if (!gGlobalPrefs->checkForUpdates) {
        return false;
    }

    // don't check for updates at the first start, so that privacy
    // sensitive users can disable the update check in time
    FILETIME never{};
    if (FileTimeEq(gGlobalPrefs->timeOfLastUpdateCheck, never)) {
        return false;
    }

    // only check if at least a day passed since last check
    FILETIME currentTimeFt;
    GetSystemTimeAsFileTime(&currentTimeFt);
    int secsSinceLastUpdate = FileTimeDiffInSecs(currentTimeFt, gGlobalPrefs->timeOfLastUpdateCheck);

    constexpr int kSecondsInDay = 60 * 60 * 24;
    constexpr int kSecondsInWeek = 7 * 60 * 60 * 24;

    int secsBetweenChecks = gIsPreReleaseBuild ? kSecondsInWeek : kSecondsInDay;
    bool checkUpdate = secsSinceLastUpdate > secsBetweenChecks;
#if 0
    logf("CheckForUpdate: secsBetweenChecks: %d, secsSinceLastUpdate: %d, checkUpdate: %d\n", secsBetweenChecks,
         secsSinceLastUpdate, (int)checkUpdate);
#endif
    return checkUpdate;
}

static void NotifyUserOfUpdate(UpdateInfo* updateInfo) {
    const WCHAR* mainInstr = _TR("New version available");
    WCHAR* verTmp = ToWstrTemp(updateInfo->latestVer);
    WCHAR* content =
        str::Format(_TR("You have version '%s' and version '%s' is available.\nDo you want to install new version?"),
                    CURR_VERSION_STR, verTmp);

    constexpr int kBtnIdDontInstall = 100;
    constexpr int kBtnIdInstall = 101;
    const WCHAR* title = _TR("SumatraPDF Update");
    TASKDIALOGCONFIG dialogConfig{};
    TASKDIALOG_BUTTON buttons[2];

    buttons[0].nButtonID = kBtnIdDontInstall;
    buttons[0].pszButtonText = _TR("Don't install");
    buttons[1].nButtonID = kBtnIdInstall;
    buttons[1].pszButtonText = _TR("Install and relaunch");

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = title;
    dialogConfig.pszMainInstruction = mainInstr;
    dialogConfig.pszContent = content;
    dialogConfig.pszVerificationText = _TR("Skip this version");
    dialogConfig.nDefaultButton = kBtnIdInstall;
    dialogConfig.dwFlags = flags;
    dialogConfig.cxWidth = 0;
    dialogConfig.pfCallback = nullptr;
    dialogConfig.dwCommonButtons = 0;
    dialogConfig.cButtons = dimof(buttons);
    dialogConfig.pButtons = &buttons[0];
    dialogConfig.pszMainIcon = TD_INFORMATION_ICON;
    dialogConfig.hwndParent = updateInfo->hwndParent;

    int buttonPressedId = 0;
    BOOL verificationFlagChecked = false;

    auto hr = TaskDialogIndirect(&dialogConfig, &buttonPressedId, nullptr, &verificationFlagChecked);
    CrashIf(hr == E_INVALIDARG);
    bool doInstall = (hr == S_OK) && (buttonPressedId == kBtnIdInstall);

    const char* installerPath = updateInfo->installerPath;
    if (!doInstall && verificationFlagChecked) {
        str::ReplaceWithCopy(&gGlobalPrefs->versionToSkip, updateInfo->latestVer);
    }

    // persist the versionToSkip and timeOfLastUpdateCheck
    SaveSettings();
    if (!doInstall) {
        file::Delete(installerPath);
        return;
    }

    // if installer not downloaded tell user to download from website
    if (!installerPath || !file::Exists(installerPath)) {
        SumatraLaunchBrowser(WEBSITE_DOWNLOAD_PAGE_URL);
        return;
    }

    // TODO: we don't really handle a case when it's a dll build but not installed
    // maybe in that case go to website
    str::Str cmd;
    if (IsDllBuild()) {
        // no need for sleep because it shows the installer dialog anyway
        cmd.Append(" -install");
    } else {
        // we're asking to over-write over ourselves, so also wait 2 secs to allow
        // our process to exit
        cmd.AppendFmt(R"( -sleep-ms 500 -exit-when-done -update-self-to "%s")", GetExePathTemp());
    }
    logf("NotifyUserOfUpdate: installer cmd: '%s'\n", cmd.Get());
    CreateProcessHelper(installerPath, cmd.Get());
    PostQuitMessage(0);
}

static DWORD ShowAutoUpdateDialog(HWND hwndParent, HttpRsp* rsp, UpdateCheck updateCheckType) {
    gUpdateCheckInProgress = false;

    // for store builds we do update check but ignore the result
    if (gIsStoreBuild) {
        return 0;
    }

    const char* url = rsp->url.Get();

    if (rsp->error != 0) {
        logf("ShowAutoUpdateDialog: http get of '%s' failed with %d\n", url, (int)rsp->error);
        return rsp->error;
    }
    if (rsp->httpStatusCode != 200) {
        logf("ShowAutoUpdateDialog: http get of '%s' failed with code %d\n", url, (int)rsp->httpStatusCode);
        return ERROR_INTERNET_INVALID_URL;
    }

    bool isValidURL = str::StartsWith(url, kUpdateInfoURL) || str::StartsWith(url, kUpdateInfoURL2);
    if (!isValidURL) {
        logf("ShowAutoUpdateDialog: '%s' is not a valid url\n", url);
        return ERROR_INTERNET_INVALID_URL;
    }
    str::Str* data = &rsp->data;
    if (0 == data->size()) {
        logf("ShowAutoUpdateDialog: empty response from url '%s'\n", url);
        return ERROR_INTERNET_CONNECTION_ABORTED;
    }

    UpdateInfo* updateInfo = ParseUpdateInfo(data->Get());
    if (!updateInfo) {
        logf("ShowAutoUpdateDialog: ParseUpdateInfo() failed. URL: '%s'\nAuto update data:\n%s\n", url, data->Get());
        return ERROR_INTERNET_INCORRECT_FORMAT;
    }
    updateInfo->hwndParent = hwndParent;

    MainWindow* win = FindMainWindowByHwnd(hwndParent);
    if (!win) {
        // could be destroyed since we issued update check
        return 0;
    }
    HWND hwndForNotif = win->hwndCanvas;
    if (!gForceAutoUpdate) {
        auto latestVer = updateInfo->latestVer;
        const char* myVer = UPDATE_CHECK_VERA;
        // myVer = L"3.1"; // for ad-hoc debugging of auto-update code
        bool hasUpdate = CompareVersion(latestVer, myVer) > 0;
        if (!hasUpdate) {
            logf("ShowAutoUpdateDialog: myVer >= latestVer ('%s' >= '%s')\n", myVer, latestVer);
            /* if automated => don't notify that there is no new version */
            if (updateCheckType == UpdateCheck::UserInitiated) {
                RemoveNotificationsForGroup(hwndForNotif, kindNotifUpdateCheckInProgress);
                uint flags = MB_ICONINFORMATION | MB_OK | MB_SETFOREGROUND | MB_TOPMOST;
                MessageBoxW(hwndParent, _TR("You have the latest version."), _TR("SumatraPDF Update"), flags);
            }
            return 0;
        }

        if (updateCheckType == UpdateCheck::Automatic) {
            // if user wanted to skip this version, we skip it in automated check
            if (str::EqI(gGlobalPrefs->versionToSkip, latestVer)) {
                logf("ShowAutoUpdateDialog: skipping auto-update of ver '%s' because of gGlobalPrefs->versionToSkip\n",
                     latestVer);
                return 0;
            }
        }
    }

    if (!updateInfo->dlURL) {
        // shouldn't happen but it's fine, we just tell the user
        logf("ShowAutoUpdateDialog: didn't find download url. Auto update data:\n%s\n", data->Get());
        RemoveNotificationsForGroup(win->hwndCanvas, kindNotifUpdateCheckInProgress);
        NotifyUserOfUpdate(updateInfo);
        return 0;
    }

    // download the installer to make update feel instant to the user
    logf("ShowAutoUpdateDialog: starting to download '%s'\n", updateInfo->dlURL);
    gUpdateCheckInProgress = true;
    RunAsync([hwndForNotif, updateInfo] { // NOLINT
        char* installerPath = path::GetTempFilePath("sumatra-installer");
        // the installer must be named .exe or it won't be able to self-elevate
        // with "runas"
        installerPath = str::JoinTemp(installerPath, ".exe");
        bool ok = HttpGetToFile(updateInfo->dlURL, installerPath);
        logf("ShowAutoUpdateDialog: HttpGetToFile(): ok=%d, downloaded to '%s'\n", (int)ok, installerPath);
        if (ok) {
            updateInfo->installerPath = str::Dup(installerPath);
        } else {
            file::Delete(installerPath);
        }

        // process the rest on ui thread to avoid threading issues
        uitask::Post([hwndForNotif, updateInfo] {
            RemoveNotificationsForGroup(hwndForNotif, kindNotifUpdateCheckInProgress);
            NotifyUserOfUpdate(updateInfo);
            gUpdateCheckInProgress = false;
            delete updateInfo;
        });
    });
    return 0;
}

static void BuildUpdateURL(str::Str& url, const char* baseURL, UpdateCheck updateCheckType) {
    url = baseURL;
    url.Append("?v=");
    url.Append(UPDATE_CHECK_VERA);
    url.Append("&os=");
    char* osVerTemp = GetWindowsVerTemp();
    url.Append(osVerTemp);
    url.Append("&64bit=");
    if (IsProcess64()) {
        url.Append("yes");
    } else {
        url.Append("no");
    }
    const char* lang = trans::GetCurrentLangCode();
    url.Append("&lang=");
    url.Append(lang);
    char* webView2ver = GetWebView2VersionTemp();
    if (webView2ver) {
        url.Append("&webview=");
        url.Append(webView2ver);
    }
    if (gIsStoreBuild) {
        url.Append("&store");
    }
    if (UpdateCheck::UserInitiated == updateCheckType) {
        url.Append("&force");
    }
}

// start auto-update check by downloading auto-update information from url
// on a background thread and processing the retrieved data on ui thread
// if autoCheck is true, this is a check *not* triggered by explicit action
// of the user and therefore will show less UI
void CheckForUpdateAsync(MainWindow* win, UpdateCheck updateCheckType) {
    if (!ShouldCheckForUpdate(updateCheckType)) {
        return;
    }

    if (UpdateCheck::UserInitiated == updateCheckType) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.msg = _TRA("Checking for update...");
        args.warning = true;
        args.timeoutMs = 0;
        args.groupId = kindNotifUpdateCheckInProgress;
        ShowNotification(args);
    }
    GetSystemTimeAsFileTime(&gGlobalPrefs->timeOfLastUpdateCheck);
    gUpdateCheckInProgress = true;

    HWND hwnd = win->hwndFrame;
    RunAsync([=] {
        str::Str url;
        BuildUpdateURL(url, kUpdateInfoURL2, updateCheckType);
        char* uri = url.Get();
        HttpRsp* rsp = new HttpRsp;
        rsp->url.SetCopy(uri);
        bool ok = HttpGet(uri, rsp);
        if (!ok) {
            delete rsp;
            BuildUpdateURL(url, kUpdateInfoURL, updateCheckType);
            uri = url.Get();
            rsp = new HttpRsp;
            rsp->url.SetCopy(uri);
            HttpGet(uri, rsp);
        }
        uitask::Post([=] {
            DWORD err = ShowAutoUpdateDialog(hwnd, rsp, updateCheckType);
            if ((err != 0) && (updateCheckType == UpdateCheck::UserInitiated)) {
                RemoveNotificationsForGroup(win->hwndCanvas, kindNotifUpdateCheckInProgress);
                // notify the user about network error during a manual update check
                TempStr msg = str::FormatTemp(_TRA("Can't connect to the Internet (error %#x)."), err);
                MessageBoxWarning(hwnd, msg, _TRA("SumatraPDF Update"));
            }
        });
    });
}

// the assumption is that this is a portable version downloaded to temp directory
// we should copy ourselves over the existing file, launch ourselves and
// tell our new copy to delete ourselves
void UpdateSelfTo(const char* path) {
    CrashIf(!path);
    if (!file::Exists(path)) {
        logf("UpdateSelfTo: failed because destination doesn't exist\n");
        return;
    }

    auto sleepMs = gCli->sleepMs;
    logf("UpdateSelfTo: '%s', sleep for %d ms\n", path, sleepMs);
    // sleeping for a bit to make sure that the program that launched us
    // had time to exit so that we can overwrite it
    ::Sleep(gCli->sleepMs);

    const char* srcPath = GetExePathTemp();
    bool ok = file::Copy(path, srcPath, false);
    // TODO: maybe retry if copy fails under the theory that the file
    // might be temporarily locked
    if (!ok) {
        logf("UpdateSelfTo: failed to copy self to file\n");
        return;
    }
    logf("UpdateSelfTo: copied self to file\n");

    AutoFreeStr args = str::Format(R"(-sleep-ms 500 -delete-file "%s")", srcPath);
    CreateProcessHelper(path, args.Get());
}

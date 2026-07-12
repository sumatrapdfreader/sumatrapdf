/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "base/SquareTreeParser.h"
#include "base/Http.h"
#include "base/Win.h"
#include "base/File.h"

#include "wingui/Layout.h"
#include "wingui/UIModels.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "Flags.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "HomePage.h"
#include "Installer.h"
#include "UpdateCheck.h"

static Kind kNotifUpdateCheckInProgress = StrL("notifUpdateCheckInProgress").s;

// certificate on www.sumatrapdfreader.org is not supported by win7 and win8.1
// (doesn't have the ciphers they understand) so we have a backup on backblaze

// clang-format off
#if defined(PRE_RELEASE_VER) || defined(DEBUG)
static const Str kUpdateInfoURL = StrL("https://www.sumatrapdfreader.org/updatecheck-pre-release.txt");
static const Str kUpdateInfoURL2 =
    StrL("https://kjk-files.s3.us-west-001.backblazeb2.com/software/sumatrapdf/sumpdf-prerelease-update.txt");
#else
static const Str kUpdateInfoURL = StrL("https://www.sumatrapdfreader.org/update-check-rel.txt");
// Note: I don't have backup for this
static const Str kUpdateInfoURL2 = StrL("https://www.sumatrapdfreader.org/update-check-rel.txt");
#endif

#ifndef kWebisteDownloadPageURL
#if defined(PRE_RELEASE_VER)
#define kWebisteDownloadPageURL "https://www.sumatrapdfreader.org/prerelease"
#else
#define kWebisteDownloadPageURL "https://www.sumatrapdfreader.org/download-free-pdf-viewer"
#endif
#endif
// clang-format on

// prevent multiple update tasks from happening simultaneously
// (this might e.g. happen if a user checks manually very quickly after startup)
bool gUpdateCheckInProgress = false;

// when true, NotifyUserOfUpdate skips the install-confirmation dialog and just
// installs (set when the user clicks "Download and update" in the pre-release
// update notification)
static bool gUpdateAutoInstall = false;

// the bottom-left "update available" notification (with the download link)
static Kind kNotifUpdateAvailable = StrL("notifUpdateAvailable").s;

struct UpdateInfo {
    HWND hwndParent = nullptr;
    Str latestVer;

    Str installer64;
    Str portable64;

    Str installer32;
    Str portable32;

    Str installerArm64;
    Str portableArm64;

    Str dlURL;
    Str installerPath;

    UpdateInfo() = default;
    ~UpdateInfo() {
        str::Free(latestVer);
        str::Free(installer64);
        str::Free(portable64);
        str::Free(installer32);
        str::Free(portable32);
        str::Free(installerArm64);
        str::Free(portableArm64);
        str::Free(dlURL);
        str::Free(installerPath);
    }
};

// an available update surfaced by the pre-release startup notification; the
// "Download and update" link downloads & installs it (owned here until then)
static UpdateInfo* gPendingUpdate = nullptr;

/*
The format of update information downloaded from the server:

[SumatraPDF]
Latest: 14276
Installer64: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-64-install.exe
Installer32: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-install.exe
PortableExe64: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-64.exe
PortableExe32: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel.exe
PortableZip64: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-64.zip
PortableZip32: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel.zip

[Promo]
[
    Name = MarkLexis
    URL = https://marklexis.arslexis.io
    Info = Bookmarking web application
]
*/
static UpdateInfo* ParseUpdateInfo(Str d) {
    // if a user configures os-wide proxy that is not a regular ie proxy
    // (which we pick up) we might get garbage http response
    // check if response looks valid
    if (!d) {
        return nullptr;
    }
    Str prefix = (d.s[0] == '[') ? StrL("[SumatraPDF]") : StrL("SumatraPDF");
    if (!str::StartsWith(d, prefix)) {
        return nullptr;
    }

    SquareTreeNode* root = ParseSquareTree(d);
    if (!root) {
        return nullptr;
    }
    AutoDelete delRoot(root);

    SetPromoString(SerializeSquareTreeNodeTemp(root->GetChild(StrL("Promo"))));

    SquareTreeNode* node = root->GetChild(StrL("SumatraPDF"));
    if (!node) {
        return nullptr;
    }

    Str latestVer = node->GetValue(StrL("Latest"));
    if (!IsValidProgramVersion(latestVer)) {
        return nullptr;
    }
    auto res = new UpdateInfo();
    res->latestVer = str::Dup(latestVer);

    // those are optional. if missing, we'll just tell the user to go to website to download
    res->installer64 = str::Dup(node->GetValue(StrL("Installer64")));
    res->installerArm64 = str::Dup(node->GetValue(StrL("InstallerArm64")));
    res->installer32 = str::Dup(node->GetValue(StrL("Installer32")));

    res->portable64 = str::Dup(node->GetValue(StrL("PortableExe64")));
    res->portableArm64 = str::Dup(node->GetValue(StrL("PortableExeArm64")));
    res->portable32 = str::Dup(node->GetValue(StrL("PortableExe32")));

    // figure out which executable to download
    Str dlURL;
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

    // when forcing, we download pre-release, which shows greater version than our build
    // so we don't want to download during automatic check, only when user initiated

#if defined(FORCE_AUTO_UPDATE)
    if (updateCheckType == UpdateCheck::UserInitiated) {
        return true;
    } else {
        return false;
    }
#endif

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

    // pre-release builds check on every startup (testers want the newest build);
    // skip the daily/weekly throttle below
    if (gIsPreReleaseBuild) {
        return true;
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

void StartInstallerAutoUpgrade(Str installerPath) {
    str::Builder cmd;
    if (IsOurExeInstalled()) {
        // no need for sleep because it shows the installer dialog anyway
        if (gIsPreReleaseBuild) {
            cmd.Append(" -fast-install");
        } else {
            cmd.Append(" -install");
        }
    } else {
        // we're asking to over-write over ourselves, so also wait 2 secs to allow
        // our process to exit
        cmd.Append(fmt(R"( -sleep-ms 2000 -exit-when-done -update-self-to "%s")", GetSelfExePathTemp()));
    }
    logf("StartInstallerAutoUpgrade: installer cmd: '%s'\n", ToStr(cmd));
    CreateProcessHelper(installerPath, ToStr(cmd));
}

static void ExitAfterStartingUpdater() {
    // Exit immediately so the updater can overwrite our exe. PostQuitMessage(0)
    // is unreliable when the dialog was shown from a uitask during startup.
    if (gPluginMode) {
        PostQuitMessage(0);
        return;
    }
    ::ExitProcess(0);
}

static void NotifyUserOfUpdate(UpdateInfo* updateInfo) {
    auto installerPathAuto = updateInfo->installerPath;
    // auto-install path: the user already opted in via the "Download and update"
    // link, so skip the confirmation dialog and just install (issue: pre-release
    // one-click update)
    if (gUpdateAutoInstall) {
        gUpdateAutoInstall = false;
        SaveSettings(); // persist timeOfLastUpdateCheck
        if (installerPathAuto && file::Exists(installerPathAuto)) {
            StartInstallerAutoUpgrade(installerPathAuto);
            ExitAfterStartingUpdater();
        } else {
            logf("NotifyUserOfUpdate: auto-install requested but installer not downloaded\n");
        }
        return;
    }

    auto mainInstr = _TRA("New version available");
    auto ver = updateInfo->latestVer;
    auto fmtStr = _TRA("You have version '%s' and version '%s' is available.\nDo you want to install new version?");
    auto content = str::Dup(fmt(fmtStr.s, StrL(CURR_VERSION_STRA), ver));

    auto installerPath = updateInfo->installerPath;
    bool didDownloadInstaller = file::Exists(installerPath);

    constexpr int kBtnIdDontInstall = 100;
    constexpr int kBtnIdInstall = 101;
    auto title = _TRA("SumatraPDF Update");
    TASKDIALOGCONFIG dialogConfig{};
    TASKDIALOG_BUTTON buttons[2];

    buttons[0].nButtonID = kBtnIdDontInstall;
    auto s = _TRA("Don't install");
    buttons[0].pszButtonText = CWStrTemp(s);
    buttons[1].nButtonID = kBtnIdInstall;
    if (didDownloadInstaller) {
        s = _TRA("Install and relaunch");
    } else {
        s = _TRA("Download update");
    }
    buttons[1].pszButtonText = CWStrTemp(s);

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = CWStrTemp(title);
    dialogConfig.pszMainInstruction = CWStrTemp(mainInstr);
    dialogConfig.pszContent = CWStrTemp(content);
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

    auto hr = TaskDialogIndirect(&dialogConfig, &buttonPressedId, nullptr, nullptr);
    ReportIf(hr == E_INVALIDARG);
    bool doInstall = (hr == S_OK) && (buttonPressedId == kBtnIdInstall);

    // persist timeOfLastUpdateCheck
    SaveSettings();
    if (!doInstall) {
        file::Delete(installerPath);
        return;
    }

    // if installer not downloaded tell user to download from website
    if (!didDownloadInstaller) {
        SumatraLaunchBrowser(kWebisteDownloadPageURL);
        return;
    }

    StartInstallerAutoUpgrade(installerPath);
    ExitAfterStartingUpdater();
}

struct UpdateProgressData {
    HWND hwndForNotif = nullptr;
    i64 nDownloaded = 0;
};

struct DownloadUpdateAsyncData {
    HWND hwndForNotif = nullptr;
    UpdateInfo* updateInfo = nullptr;
    HttpProgress httpProgress = {};

    DownloadUpdateAsyncData() = default;
    ~DownloadUpdateAsyncData() { delete updateInfo; }
};

static void DownloadUpdateFinish(DownloadUpdateAsyncData* data) {
    auto hwndForNotif = data->hwndForNotif;
    auto updateInfo = data->updateInfo;
    data->updateInfo = nullptr;
    RemoveNotificationsForGroup(hwndForNotif, kNotifUpdateCheckInProgress);
    NotifyUserOfUpdate(updateInfo);
    delete updateInfo;
    gUpdateCheckInProgress = false;
    delete data;
}

static void UpdateDownloadProgressNotif(UpdateProgressData* data) {
    TempStr size = FormatFileSizeTransTemp(data->nDownloaded);
    logf("UpdateDownloadProgressNotif: %s\n", size);
    auto wnd = GetNotificationForGroup(data->hwndForNotif, kNotifUpdateCheckInProgress);
    if (wnd) {
        TempStr msg = fmt("Downloading update: %s\n", size);
        NotificationUpdateMessage(wnd, msg, 0, true);
    } else {
        logf("UpdateDownloadProgressNotif: no wnd\n");
    }
    delete data;
}

static void UpdateProgressCb(UpdateProgressData* data, HttpProgress* progress) {
    logf("UpdateProgressCb: n: %d\n", (int)progress->nDownloaded);
    auto fnData = new UpdateProgressData;
    fnData->hwndForNotif = data->hwndForNotif;
    fnData->nDownloaded = progress->nDownloaded;
    auto fn = MkFunc0<UpdateProgressData>(UpdateDownloadProgressNotif, fnData);
    uitask::Post(fn, nullptr);
}

static void DownloadUpdateAsync(DownloadUpdateAsyncData* data) {
    auto hwndForNotif = data->hwndForNotif;
    auto updateInfo = data->updateInfo;

    TempStr installerPath = GetTempFilePathTemp("sumatra-installer");
    // the installer must be named .exe or it won't be able to self-elevate
    // with "runas"
    installerPath = str::JoinTemp(installerPath, StrL(".exe"));
    UpdateProgressData pd;
    pd.hwndForNotif = hwndForNotif;
    auto cb = MkFunc1<UpdateProgressData, HttpProgress*>(UpdateProgressCb, &pd);
    bool ok = HttpGetToFile(updateInfo->dlURL, installerPath, cb);
    logf("ShowAutoUpdateDialog: HttpGetToFile(): ok=%d, downloaded to '%s'\n", (int)ok, installerPath);
    if (ok) {
        updateInfo->installerPath = str::Dup(installerPath);
    } else {
        file::Delete(installerPath);
    }

    // process the rest on ui thread to avoid threading issues
    auto fn = MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateFinish, data);
    uitask::Post(fn, "TaskShowAutoUpdateDialog");
}

// pre-release builds surface an available update with a bottom-left notification
// whose "Download and update" link triggers a one-click download + install
static void ShowUpdateAvailableNotification(MainWindow* win, UpdateInfo* updateInfo) {
    if (!win || !updateInfo) {
        return;
    }
    TempStr link = fmt("[%s](CmdInstallPrereleaseUpdate)", _TRA("Download and install latest version"));
    TempStr msg =
        fmt(_TRA("Version %s available (you have %s). %s").s, updateInfo->latestVer, StrL(CURR_VERSION_STRA), link);
    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = msg;
    args.warning = true; // yellowish background so it stands out
    args.groupId = kNotifUpdateAvailable;
    args.timeoutMs = 0; // persist until the user clicks the link or closes it
    args.corner = NotifCorner::BottomLeft;
    args.xMargin = 2;
    args.yMargin = 2;
    ShowNotification(args);
}

// called when the user clicks "Download and update" in the pre-release update
// notification: download the pending update and (via gUpdateAutoInstall) install
// it without the confirmation dialog
void DownloadAndInstallPendingUpdate(MainWindow* win) {
    if (!win || !gPendingUpdate) {
        return;
    }
    UpdateInfo* updateInfo = gPendingUpdate;
    gPendingUpdate = nullptr;
    gUpdateAutoInstall = true;

    HWND hwndForNotif = win->hwndCanvas;
    updateInfo->hwndParent = win->hwndFrame;
    RemoveNotificationsForGroup(hwndForNotif, kNotifUpdateAvailable);

    // progress notification updated by UpdateDownloadProgressNotif (same group)
    NotificationCreateArgs nargs;
    nargs.hwndParent = hwndForNotif;
    nargs.msg = _TRA("Downloading update...");
    nargs.warning = true;
    nargs.groupId = kNotifUpdateCheckInProgress;
    nargs.timeoutMs = 0;
    nargs.corner = NotifCorner::BottomLeft;
    nargs.xMargin = 2;
    nargs.yMargin = 2;
    ShowNotification(nargs);

    gUpdateCheckInProgress = true;
    auto fnData = new DownloadUpdateAsyncData;
    fnData->hwndForNotif = hwndForNotif;
    fnData->updateInfo = updateInfo;
    auto fn = MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateAsync, fnData);
    RunAsync(fn, "DownloadUpdateAsync");
}

static bool ShouldDownloadUpdate(UpdateInfo* updateInfo, UpdateCheck updateCheckType) {
    if (gIsStoreBuild) {
        // I assume store will take care of updates
        return false;
    }
    Str latestVer = updateInfo->latestVer;
    Str myVer = StrL(UPDATE_CHECK_VERA);
    if (gIsDebugBuild) {
        // in debug build we compare against pre-rel version, like "17616"
        // but our version is like "3.6" so it triggers update
        myVer = StrL("50000");
    }
    bool hasUpdate = CompareProgramVersion(latestVer, myVer) > 0;
    return hasUpdate;
}

static HRESULT CALLBACK TaskDialogHyperlinkCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                    LONG_PTR lpRefData) {
    if (msg == TDN_HYPERLINK_CLICKED) {
        WCHAR* url = (WCHAR*)lParam;
        SumatraLaunchBrowser(ToUtf8Temp(url));
    }
    return S_OK;
}

static const Str kExpectedDlHost = StrL("https://www.sumatrapdfreader.org/");

static void NotifySuspiciousUpdate(HWND hwndParent, Str dlURL) {
    logf("NotifySuspiciousUpdate: suspicious download url '%s'\n", dlURL);
    ReportIfFast(true);
    auto title = _TRA("SumatraPDF Update");
    auto content = fmt(R"(Suspicious update.

Download link should come from <a href="%s">%s</a> but is %s.

Visit <a href="%s">%s</a> to download the latest version.)",
                       kExpectedDlHost, kExpectedDlHost, dlURL, kExpectedDlHost, kExpectedDlHost);

    TASKDIALOGCONFIG dialogConfig{};
    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }

    constexpr int kBtnIdVisitWebsite = 100;
    TASKDIALOG_BUTTON buttons[1];
    buttons[0].nButtonID = kBtnIdVisitWebsite;
    buttons[0].pszButtonText = CWStrTemp(_TRA("Visit &Website"));

    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = CWStrTemp(title);
    dialogConfig.pszContent = CWStrTemp(content);
    dialogConfig.dwFlags = flags;
    dialogConfig.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    dialogConfig.cButtons = dimof(buttons);
    dialogConfig.pButtons = buttons;
    dialogConfig.nDefaultButton = kBtnIdVisitWebsite;
    dialogConfig.pszMainIcon = TD_WARNING_ICON;
    dialogConfig.hwndParent = hwndParent;
    dialogConfig.pfCallback = TaskDialogHyperlinkCallback;
    int buttonPressedId = 0;
    TaskDialogIndirect(&dialogConfig, &buttonPressedId, nullptr, nullptr);
    if (buttonPressedId == kBtnIdVisitWebsite) {
        SumatraLaunchBrowser(kExpectedDlHost);
    }
}

static DWORD MaybeStartUpdateDownload(HWND hwndParent, HttpRsp* rsp, UpdateCheck updateCheckType) {
    // for store builds we do update check but ignore the result
#if 0
    if (gIsStoreBuild) {
        return 0;
    }
#endif

    Str url = rsp->url;

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
    str::Builder* data = &rsp->data;
    if (0 == len(*data)) {
        logf("ShowAutoUpdateDialog: empty response from url '%s'\n", url);
        return ERROR_INTERNET_CONNECTION_ABORTED;
    }

    UpdateInfo* updateInfo = ParseUpdateInfo(ToStr(*data));
    if (!updateInfo) {
        logf("ShowAutoUpdateDialog: ParseUpdateInfo() failed. URL: '%s'\nAuto update data:\n%s\n", url, ToStr(*data));
        return ERROR_INTERNET_INCORRECT_FORMAT;
    }
    updateInfo->hwndParent = hwndParent;

    MainWindow* win = FindMainWindowByHwnd(hwndParent);
    if (!win) {
        // could be destroyed since we issued update check
        delete updateInfo;
        return 0;
    }
    HWND hwndForNotif = win->hwndCanvas;
    if (!ShouldDownloadUpdate(updateInfo, updateCheckType)) {
        Str myVer = StrL(UPDATE_CHECK_VERA);
        logf("ShowAutoUpdateDialog: myVer >= latestVer ('%s' >= '%s')\n", myVer, updateInfo->latestVer);
        /* if automated => don't notify that there is no new version */
        if (updateCheckType == UpdateCheck::UserInitiated) {
            auto wnd = GetNotificationForGroup(hwndForNotif, kNotifUpdateCheckInProgress);
            if (wnd) {
                NotificationUpdateMessage(wnd, _TRA("You have the latest version."), 5 * 1000, true);
            }
        }
        delete updateInfo;
        return 0;
    }

    if (!updateInfo->dlURL) {
        // currently for release builds we don't set this and redirecto to a website instead
        logf("ShowAutoUpdateDialog: didn't find download url. Auto update data:\n%s\n", ToStr(*data));
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifUpdateCheckInProgress);
        NotifyUserOfUpdate(updateInfo);
        delete updateInfo;
        return 0;
    }

    if (!str::StartsWith(updateInfo->dlURL, kExpectedDlHost)) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifUpdateCheckInProgress);
        NotifySuspiciousUpdate(hwndParent, updateInfo->dlURL);
        delete updateInfo;
        return 0;
    }

    // pre-release automatic check: don't download yet. Show a bottom-left
    // notification whose "Download and update" link does the download + install.
    if (updateCheckType == UpdateCheck::Automatic && gIsPreReleaseBuild) {
        RemoveNotificationsForGroup(hwndForNotif, kNotifUpdateCheckInProgress);
        delete gPendingUpdate;       // drop any update from a previous check
        gPendingUpdate = updateInfo; // take ownership (freed when installed/replaced)
        ShowUpdateAvailableNotification(win, updateInfo);
        return 0;
    }

    // download the installer to make update feel instant to the user
    logf("ShowAutoUpdateDialog: starting to download '%s'\n", updateInfo->dlURL);
    gUpdateCheckInProgress = true;

    auto fnData = new DownloadUpdateAsyncData;
    fnData->hwndForNotif = hwndForNotif;
    fnData->updateInfo = updateInfo;
    auto fn = MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateAsync, fnData);
    RunAsync(fn, "DownloadUpdateAsync");
    return 0;
}

static void BuildUpdateURL(str::Builder& url, Str baseURL, UpdateCheck updateCheckType) {
    url.Reset(baseURL);
    url.Append("?v=");
    url.Append(UPDATE_CHECK_VERA);
    TempStr osVerTemp = GetWindowsVerTemp();
    url.Append("&os=");
    url.Append(osVerTemp);
    url.Append("&64bit=");
    url.Append(IsProcess64() ? "yes" : "no");
    url.Append("&arm=");
    url.Append(IsArmBuild() ? "yes" : "no");
    Str lang = trans::GetCurrentLangCode();
    url.Append("&lang=");
    url.Append(lang);
    TempStr webView2ver = GetWebView2VersionTemp();
    if (webView2ver) {
        url.Append("&webview=");
        url.Append(webView2ver);
    }
    if (gIsStoreBuild) {
        url.Append("&store");
    }
    url.Append("&simd=");
    url.Append(LatestSupportedSIMD().s);
    url.Append("&withPromo");
    if (UpdateCheck::UserInitiated == updateCheckType) {
        url.Append("&force");
    }
}

struct UpdateCheckAsyncData {
    MainWindow* win = nullptr;
    UpdateCheck updateCheckType = UpdateCheck::Automatic;
    HttpRsp* rsp = nullptr;
    UpdateCheckAsyncData() = default;
    ~UpdateCheckAsyncData() { delete rsp; }
};

static void UpdateCheckFinish(UpdateCheckAsyncData* data) {
    log("UpdateCheckFinish\n");
    gUpdateCheckInProgress = false;

    AutoDelete delData(data);

    auto updateCheckType = data->updateCheckType;
    auto rsp = data->rsp;
    MainWindow* win = nullptr;
    if (IsMainWindowValid(data->win)) {
        win = data->win;
    } else {
        if (len(gWindows) > 0) {
            win = gWindows[0];
        }
    }
    if (!win) {
        return;
    }
    HWND hwnd = win->hwndFrame;
    DWORD err = MaybeStartUpdateDownload(hwnd, rsp, updateCheckType);
    if ((err != 0) && (updateCheckType == UpdateCheck::UserInitiated)) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifUpdateCheckInProgress);
        // notify the user about network error during a manual update check
        TempStr msg = fmt(_TRA("Can't connect to the Internet (error %#x).").s, err);
        MessageBoxWarning(hwnd, msg, _TRA("SumatraPDF Update"));
    }
}

static void UpdateCheckAsync(UpdateCheckAsyncData* data) {
    auto updateCheckType = data->updateCheckType;
    str::Builder url;
    BuildUpdateURL(url, kUpdateInfoURL, updateCheckType);
    Str uri = ToStr(url);
    HttpRsp* rsp = new HttpRsp;
    str::ReplaceWithCopy(&rsp->url, uri);
    bool ok = HttpGet(uri, rsp);
    if (!ok) {
        delete rsp;
        BuildUpdateURL(url, kUpdateInfoURL2, updateCheckType);
        uri = ToStr(url);
        rsp = new HttpRsp;
        str::ReplaceWithCopy(&rsp->url, uri);
        HttpGet(uri, rsp);
    }
    data->rsp = rsp;
    auto fn = MkFunc0<UpdateCheckAsyncData>(UpdateCheckFinish, data);
    uitask::Post(fn, "TaskUpdateCheckFinish");
}

// start auto-update check by downloading auto-update information from url
// on a background thread and processing the retrieved data on ui thread
// if autoCheck is true, this is a check *not* triggered by explicit action
// of the user and therefore will show less UI
void StartAsyncUpdateCheck(MainWindow* win, UpdateCheck updateCheckType) {
    if (!ShouldCheckForUpdate(updateCheckType)) {
        return;
    }

    if (UpdateCheck::UserInitiated == updateCheckType) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.msg = _TRA("Checking for update...");
        args.warning = true;
        args.timeoutMs = 0;
        args.groupId = kNotifUpdateCheckInProgress;
        ShowNotification(args);
    }
    GetSystemTimeAsFileTime(&gGlobalPrefs->timeOfLastUpdateCheck);
    gUpdateCheckInProgress = true;

    // data freed in UpdateCheckFinish()
    auto data = new UpdateCheckAsyncData();
    data->win = win;
    data->updateCheckType = updateCheckType;
    auto fn = MkFunc0<UpdateCheckAsyncData>(UpdateCheckAsync, data);
    RunAsync(fn, "UpdateCheckAsync");
}

// the assumption is that this is a portable version downloaded to temp directory
// we should copy ourselves over the existing file, launch ourselves and
// tell our new copy to delete ourselves
void UpdateSelfTo(Str path) {
    ReportIf(!path);
    if (!file::Exists(path)) {
        logf("UpdateSelfTo: failed because destination doesn't exist\n");
        return;
    }

    auto sleepMs = gCli->sleepMs;
    logf("UpdateSelfTo: '%s', sleep for %d ms\n", path, sleepMs);
    // sleeping for a bit to make sure that the program that launched us
    // had time to exit so that we can overwrite it
    ::Sleep(gCli->sleepMs);

    TempStr srcPath = GetSelfExePathTemp();
    bool ok = file::OverwriteAtomicRetry(srcPath, path, 20, 250);
    if (!ok) {
        logf("UpdateSelfTo: failed to overwrite self file\n");
        return;
    }
    logf("UpdateSelfTo: copied self to file\n");

    TempStr args = fmt(R"(-sleep-ms 500 -delete-file "%s")", srcPath);
    CreateProcessHelper(path, args);
}

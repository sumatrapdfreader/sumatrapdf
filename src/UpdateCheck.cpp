/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "base/SquareTreeParser.h"
#include "base/Http.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/Crypto.h"

#include "gui/Layout.h"
#include "gui/UIModels.h"
#include "gui/win/WinGui.h"
#include "gui/win/WebView.h"

#include "Settings.h"
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
#include "UpdateCheckCommon.h"

static Kind kNotifUpdateCheckInProgress = StrL("notifUpdateCheckInProgress").s;

// certificate on www.sumatrapdfreader.org is not supported by win7 and win8.1
// (doesn't have the ciphers they understand) so we have a backup on backblaze

// clang-format off
// tried in order; later entries are backups if earlier HTTP gets fail
#if defined(PRE_RELEASE_VER) || defined(DEBUG)
static const Str updateInfoURLs[] = {
    StrL("https://www.sumatrapdfreader.org/updatecheck-pre-release.txt"),
    StrL("https://kjk-files.s3.us-west-001.backblazeb2.com/software/sumatrapdf/sumpdf-prerelease-update.txt"),
};
#else
static const Str updateInfoURLs[] = {
    StrL("https://www.sumatrapdfreader.org/update-check-rel.txt"),
};
#endif

#ifndef kWebisteDownloadPageURL
#ifdef PRE_RELEASE_VER
#define kWebisteDownloadPageURL "https://www.sumatrapdfreader.org/prerelease"
#else
#define kWebisteDownloadPageURL "https://www.sumatrapdfreader.org/download-free-pdf-viewer"
#endif
#endif
// clang-format on

// prevent multiple update tasks from happening simultaneously
// (this might e.g. happen if a user checks manually very quickly after startup)
static bool gUpdateCheckInProgress = false;

// when true, NotifyUserOfUpdate skips the install-confirmation dialog and just
// installs (set when the user clicks "Download and update" in the pre-release
// update notification)
static bool gUpdateAutoInstall = false;

// the bottom-left "update available" notification (with the download link)
static Kind kNotifUpdateAvailable = StrL("notifUpdateAvailable").s;

/*
The format of update information downloaded from the server:

[SumatraPDF]
Latest: 14276
BuiltOn: 2026-08-21
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

static bool ShouldCheckForUpdate(UpdateCheck updateCheckType) {
    UpdateCheckFacts f;
    f.inProgress = gUpdateCheckInProgress;
    // when forcing, we download pre-release, which shows a greater version
    // than our build, so only a user-initiated check may download it
#ifdef FORCE_AUTO_UPDATE
    f.forcePreReleaseOnly = true;
#endif
    f.userInitiated = updateCheckType == UpdateCheck::UserInitiated;
    f.hasInternet = HasPermission(Perm::InternetAccess);
    f.canSavePrefs = HasPermission(Perm::SavePreferences);
    f.checkForUpdates = gSettings->checkForUpdates;
    FILETIME never{};
    f.firstStart = FileTimeEq(gSettings->timeOfLastUpdateCheck, never);
    f.isPreRelease = gIsPreReleaseBuild;
    FILETIME currentTimeFt;
    GetSystemTimeAsFileTime(&currentTimeFt);
    f.secsSinceLast = FileTimeDiffInSecs(currentTimeFt, gSettings->timeOfLastUpdateCheck);
    return DecideUpdateCheck(f) == UpdateCheckChoice::Check;
}

void StartInstallerAutoUpgrade(Str installerPath) {
    TempStr expectedSigner = GetExecutableSignerTemp(GetSelfExePathTemp());
    TempStr installerSigner = GetExecutableSignerTemp(installerPath);
    if (len(expectedSigner) == 0 || len(installerSigner) == 0 || !str::Eq(expectedSigner, installerSigner) ||
        !IsPEFileSigned(installerPath)) {
        logf("StartInstallerAutoUpgrade: refusing an update with an untrusted signature\n");
        return;
    }
    // installed: setup UI. Portable: wait, then overwrite this exe.
    TempStr cmd = InstallerUpgradeArgsTemp(IsOurExeInstalled(), gIsPreReleaseBuild, GetSelfExePathTemp());
    logf("StartInstallerAutoUpgrade: installer cmd: '%s'\n", cmd);
    CreateProcessHelper(installerPath, cmd);
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
        // persist timeOfLastUpdateCheck before a possible ExitProcess
        ScheduleSaveSettings();
        FlushScheduledSaveSettings();
        if (installerPathAuto && file::Exists(installerPathAuto)) {
            StartInstallerAutoUpgrade(installerPathAuto);
            ExitAfterStartingUpdater();
        } else {
            logf("NotifyUserOfUpdate: auto-install requested but installer not downloaded\n");
        }
        return;
    }

    auto mainInstr = Tr("New version available");
    auto ver = updateInfo->latestVer;
    auto fmtStr = Tr("You have version '%s' and version '%s' is available.\nDo you want to install the new version?");
    TempStr content = fmt(fmtStr.s, StrL(CURR_VERSION_STRA), ver);

    auto installerPath = updateInfo->installerPath;
    bool didDownloadInstaller = file::Exists(installerPath);

    constexpr int kBtnIdDontInstall = 100;
    constexpr int kBtnIdInstall = 101;
    auto title = Tr("SumatraPDF Update");
    TASKDIALOGCONFIG dialogConfig{};
    TASKDIALOG_BUTTON buttons[2];

    buttons[0].nButtonID = kBtnIdDontInstall;
    auto s = Tr("Don't install");
    buttons[0].pszButtonText = CWStrTemp(s);
    buttons[1].nButtonID = kBtnIdInstall;
    if (didDownloadInstaller) {
        s = Tr("Install and relaunch");
    } else {
        s = Tr("Download update");
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
    dialogConfig.dwFlags = (TASKDIALOG_FLAGS)flags;
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

    // persist timeOfLastUpdateCheck before a possible ExitProcess
    ScheduleSaveSettings();
    FlushScheduledSaveSettings();
    if (!doInstall) {
        file::Delete(installerPath);
        return;
    }

    // if installer not downloaded tell user to download from website
    if (!didDownloadInstaller) {
        SumatraLaunchBrowser(StrL(kWebisteDownloadPageURL));
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
    auto* hwndForNotif = data->hwndForNotif;
    auto* updateInfo = data->updateInfo;
    data->updateInfo = nullptr;
    RemoveNotificationsForGroup(hwndForNotif, kNotifUpdateCheckInProgress);
    NotifyUserOfUpdate(updateInfo);
    delete updateInfo;
    gUpdateCheckInProgress = false;
    delete data;
}

static void UpdateDownloadProgressNotif(UpdateProgressData* data) {
    TempStr size = FormatFileSizeShortTransTemp(data->nDownloaded);
    logf("UpdateDownloadProgressNotif: %s\n", size);
    auto* wnd = GetNotificationForGroup(data->hwndForNotif, kNotifUpdateCheckInProgress);
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
    auto* fnData = new UpdateProgressData;
    fnData->hwndForNotif = data->hwndForNotif;
    fnData->nDownloaded = progress->nDownloaded;
    auto fn = MkFunc0<UpdateProgressData>(UpdateDownloadProgressNotif, fnData);
    uitask::Post(fn, nullptr);
}

static void DownloadUpdateAsync(DownloadUpdateAsyncData* data) {
    auto* hwndForNotif = data->hwndForNotif;
    auto* updateInfo = data->updateInfo;

    // sum<hex>.tmp.exe stays after install; the .tmp stub is unused. Sweep leftovers first.
    constexpr int kStaleUpdateExeSec = 24 * 60 * 60;
    DeleteStaleUpdateTemps(GetTempDirTemp(), {}, kStaleUpdateExeSec);

    TempStr stub = GetTempFilePathTemp(StrL("sumatra-installer"));
    // the installer must be named .exe or it won't be able to self-elevate with "runas"
    TempStr installerPath;
    if (len(stub) > 0) {
        file::Delete(stub);
        installerPath = str::JoinTemp(stub, StrL(".exe"));
    }
    UpdateProgressData pd;
    pd.hwndForNotif = hwndForNotif;
    auto cb = MkFunc1<UpdateProgressData, HttpProgress*>(UpdateProgressCb, &pd);
    constexpr i64 kMaxUpdateDownloadSize = 256LL * 1024 * 1024;
    bool ok = len(installerPath) > 0 && HttpGetToFile(updateInfo->dlURL, installerPath, cb, kMaxUpdateDownloadSize);
    logf("ShowAutoUpdateDialog: HttpGetToFile(): ok=%d, downloaded to '%s'\n", (int)ok, installerPath);
    TempStr expectedSigner = GetExecutableSignerTemp(GetSelfExePathTemp());
    TempStr installerSigner = ok ? GetExecutableSignerTemp(installerPath) : TempStr{};
    ok = ok && expectedSigner && installerSigner && str::Eq(expectedSigner, installerSigner) &&
         IsPEFileSigned(installerPath);
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
    TempStr link = fmt("[%s](CmdInstallPrereleaseUpdate)", Tr("Update"));
    // pre-release "Latest" is a build number (e.g. 17616); show as 3.7.17616
    TempStr displayVer = updateInfo->latestVer;
    if (!str::ContainsChar(displayVer, '.')) {
        displayVer = fmt("%s.%s", StrL(CURR_VERSION_MAJOR_STRA), displayVer);
    }
    TempStr msg;
    if (updateInfo->builtOn) {
        msg = fmt(Tr("Version %s from %s available. %s").s, displayVer, updateInfo->builtOn, link);
    } else {
        msg = fmt(Tr("Version %s available. %s").s, displayVer, link);
    }
    NotificationCreateArgs args;
    args.win = win;
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
// download + install the update surfaced by the pre-release update notification
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
    nargs.msg = Tr("Downloading update...");
    nargs.warning = true;
    nargs.groupId = kNotifUpdateCheckInProgress;
    nargs.timeoutMs = 0;
    nargs.corner = NotifCorner::BottomLeft;
    nargs.xMargin = 2;
    nargs.yMargin = 2;
    ShowNotification(nargs);

    gUpdateCheckInProgress = true;
    auto* fnData = new DownloadUpdateAsyncData;
    fnData->hwndForNotif = hwndForNotif;
    fnData->updateInfo = updateInfo;
    auto fn = MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateAsync, fnData);
    RunAsync(fn, StrL("DownloadUpdateAsync"));
}

static bool ShouldDownloadUpdate(UpdateInfo* updateInfo) {
    Str myVer = StrL(UPDATE_CHECK_VERA);
    if (gIsDebugBuild) {
        // in debug build we compare against pre-rel version, like "17616"
        // but our version is like "3.6" so it triggers update
        myVer = StrL("50000");
    }
    // a store build lets the store install updates
    return ShouldOfferProgramUpdate(updateInfo->latestVer, myVer, gIsStoreBuild);
}

static HRESULT CALLBACK TaskDialogHyperlinkCallback(HWND /*hwnd*/, UINT msg, WPARAM /*wParam*/, LPARAM lParam,
                                                    LONG_PTR /*lpRefData*/) {
    if (msg == TDN_HYPERLINK_CLICKED) {
        WCHAR* url = (WCHAR*)lParam;
        SumatraLaunchBrowser(ToUtf8Temp(url));
    }
    return S_OK;
}

constexpr int kUrlHexHead = 40;

static TempStr HexHeadTemp(Str s, int nMax) {
    if (!s.s || len(s) <= 0) {
        return StrL("");
    }
    int n = std::min(len(s), nMax);
    return str::MemToHexTemp(Str(s.s, n));
}

static void NotifySuspiciousUpdate(HWND hwndParent, Str dlURL) {
    logf("NotifySuspiciousUpdate: suspicious download url '%s'\n", dlURL);
    logf("  urlLen=%d hostLen=%d host='%s'\n", len(dlURL), len(kExpectedDlHost), kExpectedDlHost);
    logf("  url hex[0..%d]=%s\n", kUrlHexHead, HexHeadTemp(dlURL, kUrlHexHead));
    logf("  host hex[0..%d]=%s\n", kUrlHexHead, HexHeadTemp(kExpectedDlHost, kUrlHexHead));
    ReportIf(true);
    auto title = Tr("SumatraPDF Update");
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
    buttons[0].pszButtonText = CWStrTemp(Tr("Visit &Website"));

    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = CWStrTemp(title);
    dialogConfig.pszContent = CWStrTemp(content);
    dialogConfig.dwFlags = (TASKDIALOG_FLAGS)flags;
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

// Shown only for a user-initiated update check that couldn't download/parse the
// update info. Tells the user and points them at the download page so they can
// update manually (e.g. if TLS validation or the network failed).
static void NotifyUpdateCheckFailed(HWND hwndParent, DWORD err) {
    logf("NotifyUpdateCheckFailed: err=%#x\n", (unsigned)err);
    auto title = Tr("SumatraPDF Update");
    auto mainInstr = Tr("Couldn't check for updates");
    TempStr msg = fmt(Tr("Couldn't download update information (error %#x).").s, err);
    TempStr content = fmt(R"(%s

Visit <a href="%s">%s</a> to download the latest version.)",
                          msg, StrL(kWebisteDownloadPageURL), StrL(kWebisteDownloadPageURL));

    TASKDIALOGCONFIG dialogConfig{};
    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }

    constexpr int kBtnIdVisitWebsite = 100;
    TASKDIALOG_BUTTON buttons[1];
    buttons[0].nButtonID = kBtnIdVisitWebsite;
    buttons[0].pszButtonText = CWStrTemp(Tr("Visit &Website"));

    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = CWStrTemp(title);
    dialogConfig.pszMainInstruction = CWStrTemp(mainInstr);
    dialogConfig.pszContent = CWStrTemp(content);
    dialogConfig.dwFlags = (TASKDIALOG_FLAGS)flags;
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
        SumatraLaunchBrowser(StrL(kWebisteDownloadPageURL));
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
    str::Builder* data = &rsp->data;
    UpdateInfoFetch kind = ClassifyUpdateInfoFetch(url, (int)rsp->error, (int)rsp->httpStatusCode, len(*data),
                                                   updateInfoURLs, dimof(updateInfoURLs));
    if (kind == UpdateInfoFetch::TransportError) {
        logf("ShowAutoUpdateDialog: http get of '%s' failed with %d\n", url, (int)rsp->error);
        return rsp->error;
    }
    if (kind == UpdateInfoFetch::BadStatus) {
        logf("ShowAutoUpdateDialog: http get of '%s' failed with code %d\n", url, (int)rsp->httpStatusCode);
        return ERROR_INTERNET_INVALID_URL;
    }
    if (kind == UpdateInfoFetch::UnlistedUrl) {
        logf("ShowAutoUpdateDialog: '%s' is not a valid url\n", url);
        return ERROR_INTERNET_INVALID_URL;
    }
    if (kind == UpdateInfoFetch::EmptyBody) {
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
    if (!ShouldDownloadUpdate(updateInfo)) {
        Str myVer = StrL(UPDATE_CHECK_VERA);
        logf("ShowAutoUpdateDialog: myVer >= latestVer ('%s' >= '%s')\n", myVer, updateInfo->latestVer);
        /* if automated => don't notify that there is no new version */
        if (updateCheckType == UpdateCheck::UserInitiated) {
            auto* wnd = GetNotificationForGroup(hwndForNotif, kNotifUpdateCheckInProgress);
            if (wnd) {
                NotificationUpdateMessage(wnd, Tr("You have the latest version."), 5 * 1000, true);
            }
        }
        delete updateInfo;
        return 0;
    }

    if (len(updateInfo->dlURL) == 0) {
        // currently for release builds we don't set this and redirecto to a website instead
        logf("ShowAutoUpdateDialog: didn't find download url. Auto update data:\n%s\n", ToStr(*data));
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        NotifyUserOfUpdate(updateInfo);
        delete updateInfo;
        return 0;
    }

    if (!IsTrustedUpdateDlUrl(updateInfo->dlURL)) {
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
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

    auto* fnData = new DownloadUpdateAsyncData;
    fnData->hwndForNotif = hwndForNotif;
    fnData->updateInfo = updateInfo;
    auto fn = MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateAsync, fnData);
    RunAsync(fn, StrL("DownloadUpdateAsync"));
    return 0;
}

// Shared by update check and minidump upload: v, os, 64bit, arm, lang, webview, store, simd.
void AppendClientInfoQuery(str::Builder& url) {
    url.Append(StrL("?v="));
    url.Append(StrL(UPDATE_CHECK_VERA));
    TempStr osVerTemp = GetWindowsVerTemp();
    url.Append(StrL("&os="));
    url.Append(osVerTemp);
    url.Append(StrL("&64bit="));
    url.Append(Str(IsProcess64() ? "yes" : "no"));
    url.Append(StrL("&arm="));
    url.Append(Str(IsArmBuild() ? "yes" : "no"));
    Str lang = trans::GetCurrentLangCode();
    url.Append(StrL("&lang="));
    url.Append(lang);
    TempStr webView2ver = GetWebView2VersionTemp();
    if (webView2ver) {
        url.Append(StrL("&webview="));
        url.Append(webView2ver);
    }
    if (gIsStoreBuild) {
        url.Append(StrL("&store"));
    }
    url.Append(StrL("&simd="));
    url.Append(Str(LatestSupportedSIMD().s));
}

struct UpdateCheckAsyncData {
    MainWindow* win = nullptr;
    UpdateCheck updateCheckType = UpdateCheck::Automatic;
    HttpRsp* rsp = nullptr;
    UpdateCheckAsyncData() = default;
    ~UpdateCheckAsyncData() { delete rsp; }
};

static void UpdateCheckFinish(UpdateCheckAsyncData* data) {
    log(StrL("UpdateCheckFinish\n"));
    gUpdateCheckInProgress = false;

    AutoDelete delData(data);

    auto updateCheckType = data->updateCheckType;
    auto* rsp = data->rsp;
    MainWindow* win = nullptr;
    if (IsMainWindowValidAndNotClosing(data->win)) {
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
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        // a manual check that couldn't fetch update info: tell the user and point
        // them at the website so they can update manually
        NotifyUpdateCheckFailed(hwnd, err);
    }
}

static void UpdateCheckAsync(UpdateCheckAsyncData* data) {
    auto updateCheckType = data->updateCheckType;
    HttpRsp* rsp = nullptr;
    for (auto updateInfoURL : updateInfoURLs) {
        if (rsp) {
            delete rsp;
        }
        str::Builder url;
        BuildUpdateURL(url, updateInfoURL, updateCheckType);
        Str uri = ToStr(url);
        rsp = new HttpRsp;
        str::ReplaceWithCopy(&rsp->url, uri);
        bool ok = HttpGet(uri, rsp);
        logf("UpdateCheckAsync: response from '%s': error=%d, status=%d, %d bytes\n%s\n", rsp->url, (int)rsp->error,
             (int)rsp->httpStatusCode, len(rsp->data), ToStr(rsp->data));
        if (ok) {
            break;
        }
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

    logf("StartAsyncUpdateCheck: updateCheckType=%d\n", (int)updateCheckType);
    if (UpdateCheck::UserInitiated == updateCheckType) {
        NotificationCreateArgs args;
        args.win = win;
        args.msg = Tr("Checking for update...");
        args.warning = true;
        args.timeoutMs = 0;
        args.groupId = kNotifUpdateCheckInProgress;
        ShowNotification(args);
    }
    GetSystemTimeAsFileTime(&gSettings->timeOfLastUpdateCheck);
    gUpdateCheckInProgress = true;

    // data freed in UpdateCheckFinish()
    auto* data = new UpdateCheckAsyncData();
    data->win = win;
    data->updateCheckType = updateCheckType;
    auto fn = MkFunc0<UpdateCheckAsyncData>(UpdateCheckAsync, data);
    RunAsync(fn, StrL("UpdateCheckAsync"));
}

#if IS_DEBUG
#include "base/tests/UtAssert.h"

void UpdateTempFileTests() {
    utassert(IsUpdateTempFileName(StrL("sum17E3.tmp")));
    utassert(IsUpdateTempFileName(StrL("sum17E3.tmp.exe")));
    utassert(IsUpdateTempFileName(StrL("SUM17e3.TMP.EXE")));
    utassert(!IsUpdateTempFileName(StrL("sum17E3.tmp.exe.bak")));
    utassert(!IsUpdateTempFileName(StrL("SumatraPDF.exe")));
    utassert(!IsUpdateTempFileName(StrL("sum.tmp.exe")));
    utassert(!IsUpdateTempFileName(StrL("sum17E.tmp.exe")));
    utassert(!IsUpdateTempFileName(StrL("sumatra.tmp.exe")));

    TempStr dir = GetTempFilePathTemp(StrL("upd"));
    utassert(len(dir) > 0);
    utassert(file::Delete(dir));
    utassert(dir::Create(dir));
    defer {
        dir::RemoveAll(dir);
    };

    TempStr stub = path::JoinTemp(dir, StrL("sumABCD.tmp"));
    TempStr exe = path::JoinTemp(dir, StrL("sumABCD.tmp.exe"));
    TempStr fresh = path::JoinTemp(dir, StrL("sum1234.tmp.exe"));
    TempStr keep = path::JoinTemp(dir, StrL("keep.exe"));
    TempStr other = path::JoinTemp(dir, StrL("sumatra.tmp.exe"));
    TempStr busy = path::JoinTemp(dir, StrL("sumBEEF.tmp"));
    utassert(file::WriteFile(stub, StrL("")));
    utassert(file::WriteFile(exe, StrL("installer")));
    utassert(file::WriteFile(fresh, StrL("new")));
    utassert(file::WriteFile(keep, StrL("x")));
    utassert(file::WriteFile(other, StrL("x")));
    utassert(file::WriteFile(busy, StrL("busy")));

    DeleteStaleUpdateTemps(dir, exe, 0);
    utassert(!file::Exists(stub));
    utassert(file::Exists(exe));
    utassert(!file::Exists(fresh));
    utassert(file::Exists(keep));
    utassert(file::Exists(other));
    utassert(file::Exists(busy));

    utassert(file::WriteFile(stub, StrL("")));
    DeleteStaleUpdateTemps(dir, {}, 100000000);
    utassert(!file::Exists(stub));
    utassert(file::Exists(exe));
    utassert(file::Exists(keep));
    utassert(file::Exists(other));
    utassert(file::Exists(busy));
}
#endif

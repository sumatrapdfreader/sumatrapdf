/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's UpdateCheck.cpp. The download and the parsing are orig's; what
// differs is the presentation (orig's three TaskDialogs are the port's
// AlertDialog-backed MsgBox), and that off Windows the http get goes through
// gpui (base's Http is winhttp) and there is no installer to run.

#include "gui/GpuiBridge.h"

#include "base/UITask.h"
#include "base/SquareTreeParser.h"
#include "base/Http.h"
#include "base/File.h"
#include "base/Crypto.h"
#if OS_WIN
#include "base/Win.h"
#endif
#if OS_DARWIN
#include <sys/sysctl.h>
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "DocController.h"
#include "Commands.h"
#include "TipMarkup.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "HomePage.h"
#include "SumatraDialogs.h"
#include "UpdateCheck.h"

#include "SumatraLog.h"

static Kind kNotifUpdateCheckInProgress = "notifUpdateCheckInProgress";

#if !OS_WIN
// base/Win.h is Windows-only; these are what the update query needs from it
static bool IsProcess64() {
    return sizeof(void*) == 8;
}

static bool IsArmBuild() {
#if defined(__aarch64__) || defined(__arm__)
    return true;
#else
    return false;
#endif
}
#endif

// ng: each platform has its own builds and update info, uploaded to
// software/sumatrapdfng/<platform>/ in R2, and its own update check url on
// the website, which also counts that platform's checks apart from the rest.
// Wasm has no builds of its own to offer and still asks orig's urls.

// clang-format off
// tried in order; later entries are backups if earlier HTTP gets fail
#if OS_WIN
static const Str updateInfoURLs[] = {
    StrL("https://www.sumatrapdfreader.org/update-check-ng-win.txt"),
};
#elif OS_DARWIN
static const Str updateInfoURLs[] = {
    StrL("https://www.sumatrapdfreader.org/update-check-ng-mac.txt"),
};
#elif OS_LINUX
static const Str updateInfoURLs[] = {
    StrL("https://www.sumatrapdfreader.org/update-check-ng-linux.txt"),
};
#elif defined(PRE_RELEASE_VER) || defined(DEBUG)
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
static Kind kNotifUpdateAvailable = "notifUpdateAvailable";

struct UpdateInfo {
    MainWindow* win = nullptr;
    Str latestVer;

    Str installer64;
    Str portable64;

    Str installer32;
    Str portable32;

    Str installerArm64;
    Str portableArm64;

    Str dlURL;
    Str installerPath;
    Str builtOn; // optional "yyyy-mm-dd" from the update-check file
    // ng: optional, where the browser gets this build when we can't install it
    Str downloadPage;

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
        str::Free(builtOn);
        str::Free(downloadPage);
    }
};

// an available update surfaced by the pre-release startup notification; the
// "Download and update" link downloads & installs it (owned here until then)
static UpdateInfo* gPendingUpdate = nullptr;

bool HasPendingPreReleaseUpdate() {
    return gPendingUpdate != nullptr;
}

static void CurrentFileTime(FILETIME* ft) {
    GetSystemTimeAsFileTime(ft);
}

/*
The format of update information downloaded from the server:

[SumatraPDF]
Latest: 14276
BuiltOn: 2026-08-21
Installer64: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-64-install.exe
...

ng: macOS and Linux have no installer to run, so their info names what the
browser should open instead:

DownloadPage: https://www.sumatrapdfreader.org/dlng/mac/14276/SumatraPDF-mac-arm64.zip
*/
static UpdateInfo* ParseUpdateInfo(Str d) {
    // if a user configures os-wide proxy that is not a regular ie proxy
    // (which we pick up) we might get garbage http response
    // check if response looks valid
    if (len(d) == 0) {
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
    auto* res = new UpdateInfo();
    res->latestVer = str::Dup(latestVer);
    Str onDate = node->GetValue(StrL("BuiltOn"));
    if (onDate) {
        res->builtOn = str::Dup(onDate);
    }

    // those are optional. if missing, we'll just tell the user to go to website to download
    res->installer64 = str::Dup(node->GetValue(StrL("Installer64")));
    res->installerArm64 = str::Dup(node->GetValue(StrL("InstallerArm64")));
    res->installer32 = str::Dup(node->GetValue(StrL("Installer32")));

    res->portable64 = str::Dup(node->GetValue(StrL("PortableExe64")));
    res->portableArm64 = str::Dup(node->GetValue(StrL("PortableExeArm64")));
    res->portable32 = str::Dup(node->GetValue(StrL("PortableExe32")));
    res->downloadPage = str::Dup(node->GetValue(StrL("DownloadPage")));

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

    if (!HasPermission(Perm::InternetAccess)) {
        logf("CheckForUpdate: skipping because no internet access\n");
        return false;
    }

    if (updateCheckType == UpdateCheck::UserInitiated) {
        logf("CheckForUpdate: checking, user initiated\n");
        return true;
    }

    // don't check if the timestamp or version to skip can't be updated
    // (mainly in plugin mode, stress testing and restricted settings)
    if (!HasPermission(Perm::SavePreferences)) {
        logf("CheckForUpdate: skipping auto check because no prefs access\n");
        return false;
    }

    // only applies to automatic update check
    if (!gSettings->checkForUpdates) {
        logf("CheckForUpdate: skipping auto check because CheckForUpdates is false\n");
        return false;
    }

    // don't check for updates at the first start, so that privacy
    // sensitive users can disable the update check in time
    FILETIME never{};
    if (FileTimeEq(gSettings->timeOfLastUpdateCheck, never)) {
        logf("CheckForUpdate: skipping auto check, first start (TimeOfLastUpdateCheck not set)\n");
        return false;
    }

    // pre-release builds check on every startup (testers want the newest build);
    // skip the daily/weekly throttle below
    if (gIsPreReleaseBuild) {
        logf("CheckForUpdate: checking, pre-release build checks on every startup\n");
        return true;
    }

    // only check if at least a day passed since last check
    FILETIME currentTimeFt;
    CurrentFileTime(&currentTimeFt);
    int secsSinceLastUpdate = FileTimeDiffInSecs(currentTimeFt, gSettings->timeOfLastUpdateCheck);

    constexpr int kSecondsInDay = 60 * 60 * 24;
    constexpr int kSecondsInWeek = 7 * 60 * 60 * 24;

    int secsBetweenChecks = gIsPreReleaseBuild ? kSecondsInWeek : kSecondsInDay;
    bool checkUpdate = secsSinceLastUpdate > secsBetweenChecks;
    logf("CheckForUpdate: %s auto check, %d secs since the last one, %d secs between checks\n",
         checkUpdate ? StrL("doing") : StrL("skipping"), secsSinceLastUpdate, secsBetweenChecks);
    return checkUpdate;
}

static void OpenWebPage(Str url) {
    logf("UpdateCheck: opening '%s'\n", url);
    SumatraLaunchBrowser(url);
}

#if OS_WIN

void StartInstallerAutoUpgrade(Str installerPath) {
    TempStr expectedSigner = GetExecutableSignerTemp(GetSelfExePathTemp());
    TempStr installerSigner = GetExecutableSignerTemp(installerPath);
    if (len(expectedSigner) == 0 || len(installerSigner) == 0 || !str::Eq(expectedSigner, installerSigner) ||
        !IsPEFileSigned(installerPath)) {
        logf("StartInstallerAutoUpgrade: refusing an update with an untrusted signature\n");
        return;
    }
    str::Builder cmd;
    // ng: orig asks the installer (IsOurExeInstalled()); the installer is step
    // 17, so "not portable" stands in for "installed"
    if (!IsRunningInPortableMode()) {
        // no need for sleep because it shows the installer dialog anyway
        if (gIsPreReleaseBuild) {
            cmd.Append(StrL(" -fast-install"));
        } else {
            cmd.Append(StrL(" -install"));
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
    // Exit immediately so the updater can overwrite our exe
    ::ExitProcess(0);
}

// the assumption is that this is a portable version downloaded to temp directory
// we should copy ourselves over the existing file, launch ourselves and
// tell our new copy to delete ourselves
void UpdateSelfTo(Str dstPath, int sleepMs) {
    ReportIf(len(dstPath) == 0);
    if (!file::Exists(dstPath)) {
        logf("UpdateSelfTo: failed because destination doesn't exist\n");
        return;
    }

    logf("UpdateSelfTo: '%s', sleep for %d ms\n", dstPath, sleepMs);
    // sleeping for a bit to make sure that the program that launched us
    // had time to exit so that we can overwrite it
    SleepInMs(sleepMs);

    // OverwriteAtomicRetry(dst, src): copy this process (new build) onto dstPath
    TempStr srcPath = GetSelfExePathTemp();
    bool ok = file::OverwriteAtomicRetry(dstPath, srcPath, 20, 250);
    if (!ok) {
        logf("UpdateSelfTo: failed to overwrite '%s' with '%s'\n", dstPath, srcPath);
        return;
    }
    logf("UpdateSelfTo: copied self to '%s'\n", dstPath);

    TempStr args = fmt(R"(-sleep-ms 500 -delete-file "%s")", srcPath);
    CreateProcessHelper(dstPath, args);
}

#else

// no installer off Windows: the user is sent to the download page instead
void StartInstallerAutoUpgrade(Str) {}

static void ExitAfterStartingUpdater() {}

void UpdateSelfTo(Str, int) {}

#endif

static const Str kExpectedDlHost = StrL("https://www.sumatrapdfreader.org/");

static bool IsTrustedUpdateDlUrl(Str dlURL) {
    return str::StartsWith(dlURL, kExpectedDlHost);
}

// The build's own download when the update info names a trusted one.
static void OpenDownloadPage(UpdateInfo* updateInfo) {
    Str page = updateInfo->downloadPage;
    if (len(page) == 0 || !IsTrustedUpdateDlUrl(page)) {
        page = StrL(kWebisteDownloadPageURL);
    }
    OpenWebPage(page);
}

static void OnInstallAnswer(UpdateInfo* updateInfo, int res) {
    AutoDelete delInfo(updateInfo);
    Str installerPath = updateInfo->installerPath;
    bool didDownloadInstaller = file::Exists(installerPath);

    // persist timeOfLastUpdateCheck before a possible exit
    ScheduleSaveSettings();
    FlushScheduledSaveSettings();
    if (res != MbRetYes) {
        file::Delete(installerPath);
        return;
    }
    // if installer not downloaded tell user to download from website
    if (!didDownloadInstaller) {
        OpenDownloadPage(updateInfo);
        return;
    }
    StartInstallerAutoUpgrade(installerPath);
    ExitAfterStartingUpdater();
}

// ng: orig's TaskDialog with "Don't install" / "Install and relaunch" buttons;
// the port's message box has Yes / No
static void NotifyUserOfUpdate(UpdateInfo* updateInfo) {
    Str installerPathAuto = updateInfo->installerPath;
    // auto-install path: the user already opted in via the "Download and update"
    // link, so skip the confirmation dialog and just install
    if (gUpdateAutoInstall) {
        gUpdateAutoInstall = false;
        ScheduleSaveSettings();
        FlushScheduledSaveSettings();
        if (installerPathAuto && file::Exists(installerPathAuto)) {
            StartInstallerAutoUpgrade(installerPathAuto);
            ExitAfterStartingUpdater();
        } else {
            logf("NotifyUserOfUpdate: auto-install requested but installer not downloaded\n");
            OpenDownloadPage(updateInfo);
        }
        delete updateInfo;
        return;
    }

    Str mainInstr = Tr("New version available");
    Str ver = updateInfo->latestVer;
    Str fmtStr = Tr("You have version '%s' and version '%s' is available.\nDo you want to install the new version?");
    TempStr content = fmt("%s\n\n%s", mainInstr, fmt(fmtStr.s, StrL(CURR_VERSION_STRA), ver));
    logf("NotifyUserOfUpdate: %s -> %s\n", StrL(CURR_VERSION_STRA), ver);

    uint flags = MbYesNo | MbIconInformation;
    MsgBox(updateInfo->win, content, Tr("SumatraPDF Update"), flags,
           MkFunc1<UpdateInfo, int>(OnInstallAnswer, updateInfo));
}

struct UpdateProgressData {
    MainWindow* win = nullptr;
    i64 nDownloaded = 0;
};

struct DownloadUpdateAsyncData {
    MainWindow* win = nullptr;
    UpdateInfo* updateInfo = nullptr;

    DownloadUpdateAsyncData() = default;
    ~DownloadUpdateAsyncData() { delete updateInfo; }
};

static void DownloadUpdateFinish(DownloadUpdateAsyncData* data) {
    MainWindow* win = data->win;
    UpdateInfo* updateInfo = data->updateInfo;
    data->updateInfo = nullptr;
    RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
    gUpdateCheckInProgress = false;
    delete data;
    NotifyUserOfUpdate(updateInfo);
}

static void UpdateDownloadProgressNotif(UpdateProgressData* data) {
    TempStr size = FormatFileSizeShortTransTemp(data->nDownloaded);
    auto* wnd = GetNotificationForGroup(data->win, kNotifUpdateCheckInProgress);
    if (wnd) {
        NotificationUpdateMessage(wnd, fmt("Downloading update: %s", size), 0);
    }
    delete data;
}

static void UpdateProgressCb(UpdateProgressData* data, HttpProgress* progress) {
    auto* fnData = new UpdateProgressData;
    fnData->win = data->win;
    fnData->nDownloaded = progress->nDownloaded;
    uitask::Post(MkFunc0<UpdateProgressData>(UpdateDownloadProgressNotif, fnData), nullptr);
}

static void DownloadUpdateAsync(DownloadUpdateAsyncData* data) {
#if OS_WIN
    UpdateInfo* updateInfo = data->updateInfo;
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
    pd.win = data->win;
    auto cb = MkFunc1<UpdateProgressData, HttpProgress*>(UpdateProgressCb, &pd);
    constexpr i64 kMaxUpdateDownloadSize = 256LL * 1024 * 1024;
    bool ok = len(installerPath) > 0 && HttpGetToFile(updateInfo->dlURL, installerPath, cb, kMaxUpdateDownloadSize);
    logf("DownloadUpdateAsync: HttpGetToFile(): ok=%d, downloaded to '%s'\n", (int)ok, installerPath);
    TempStr expectedSigner = GetExecutableSignerTemp(GetSelfExePathTemp());
    TempStr installerSigner = ok ? GetExecutableSignerTemp(installerPath) : TempStr{};
    ok = ok && expectedSigner && installerSigner && str::Eq(expectedSigner, installerSigner) &&
         IsPEFileSigned(installerPath);
    if (ok) {
        updateInfo->installerPath = str::Dup(installerPath);
    } else {
        file::Delete(installerPath);
    }
#endif

    // process the rest on ui thread to avoid threading issues
    uitask::Post(MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateFinish, data), "TaskShowAutoUpdateDialog");
}

// pre-release builds surface an available update with a bottom-left notification
// whose "Update" link triggers a one-click download + install
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
    args.timeoutMs = kNotifNoTimeout; // persist until the user clicks the link or closes it
    args.corner = NotifCorner::BottomLeft;
    args.xMargin = 2;
    args.yMargin = 2;
    ShowNotification(args);
}

// download + install the update surfaced by the pre-release update notification
void DownloadAndInstallPendingUpdate(MainWindow* win) {
    if (!win || !gPendingUpdate) {
        return;
    }
    UpdateInfo* updateInfo = gPendingUpdate;
    gPendingUpdate = nullptr;
    gUpdateAutoInstall = true;
    updateInfo->win = win;
    RemoveNotificationsForGroup(win, kNotifUpdateAvailable);

    // progress notification updated by UpdateDownloadProgressNotif (same group)
    NotificationCreateArgs nargs;
    nargs.win = win;
    nargs.msg = Tr("Downloading update...");
    nargs.warning = true;
    nargs.groupId = kNotifUpdateCheckInProgress;
    nargs.timeoutMs = kNotifNoTimeout;
    nargs.corner = NotifCorner::BottomLeft;
    nargs.xMargin = 2;
    nargs.yMargin = 2;
    ShowNotification(nargs);

    gUpdateCheckInProgress = true;
    auto* fnData = new DownloadUpdateAsyncData;
    fnData->win = win;
    fnData->updateInfo = updateInfo;
    RunAsync(MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateAsync, fnData), StrL("DownloadUpdateAsync"));
}

static bool ShouldDownloadUpdate(UpdateInfo* updateInfo) {
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
    return CompareProgramVersion(latestVer, myVer) > 0;
}

static void OnVisitWebsiteAnswer(int res) {
    if (res == MbRetYes) {
        OpenWebPage(StrL(kWebisteDownloadPageURL));
    }
}

static void NotifySuspiciousUpdate(MainWindow* win, Str dlURL) {
    logf("NotifySuspiciousUpdate: suspicious download url '%s'\n", dlURL);
    TempStr content =
        fmt("Suspicious update.\n\nDownload link should come from %s but is %s.\n\nVisit the website to download the "
            "latest version?",
            kExpectedDlHost, dlURL);
    MsgBox(win, content, Tr("SumatraPDF Update"), MbYesNo | MbIconWarning, MkFunc1Void(OnVisitWebsiteAnswer));
}

// Shown only for a user-initiated update check that couldn't download/parse the
// update info. Tells the user and points them at the download page so they can
// update manually (e.g. if TLS validation or the network failed).
static void NotifyUpdateCheckFailed(MainWindow* win, DWORD err) {
    logf("NotifyUpdateCheckFailed: err=%#x\n", (unsigned)err);
    TempStr msg = fmt(Tr("Couldn't download update information (error %#x).").s, err);
    TempStr content = fmt("%s\n\n%s\n\n%s", Tr("Couldn't check for updates"), msg,
                          StrL("Visit the website to download the latest version?"));
    MsgBox(win, content, Tr("SumatraPDF Update"), MbYesNo | MbIconWarning, MkFunc1Void(OnVisitWebsiteAnswer));
}

static DWORD MaybeStartUpdateDownload(MainWindow* win, HttpRsp* rsp, UpdateCheck updateCheckType) {
    Str url = rsp->url;

    if (rsp->error != 0) {
        logf("MaybeStartUpdateDownload: http get of '%s' failed with %d\n", url, (int)rsp->error);
        return rsp->error;
    }
    if (rsp->httpStatusCode != 200) {
        logf("MaybeStartUpdateDownload: http get of '%s' failed with code %d\n", url, (int)rsp->httpStatusCode);
        return (DWORD)rsp->httpStatusCode;
    }

    bool isValidURL = false;
    for (auto updateInfoURL : updateInfoURLs) {
        if (str::StartsWith(url, updateInfoURL)) {
            isValidURL = true;
            break;
        }
    }
    if (!isValidURL) {
        logf("MaybeStartUpdateDownload: '%s' is not a valid url\n", url);
        return 1;
    }
    str::Builder* data = &rsp->data;
    if (0 == len(*data)) {
        logf("MaybeStartUpdateDownload: empty response from url '%s'\n", url);
        return 2;
    }

    UpdateInfo* updateInfo = ParseUpdateInfo(ToStr(*data));
    if (!updateInfo) {
        logf("MaybeStartUpdateDownload: ParseUpdateInfo() failed. URL: '%s'\n", url);
        return 3;
    }
    updateInfo->win = win;

    if (!ShouldDownloadUpdate(updateInfo)) {
        Str myVer = StrL(UPDATE_CHECK_VERA);
        logf("MaybeStartUpdateDownload: myVer >= latestVer ('%s' >= '%s')\n", myVer, updateInfo->latestVer);
        /* if automated => don't notify that there is no new version */
        if (updateCheckType == UpdateCheck::UserInitiated) {
            auto* wnd = GetNotificationForGroup(win, kNotifUpdateCheckInProgress);
            if (wnd) {
                NotificationUpdateMessage(wnd, Tr("You have the latest version."), kNotif5SecsTimeOut);
            }
        }
        delete updateInfo;
        return 0;
    }

    if (len(updateInfo->dlURL) == 0) {
        // currently for release builds we don't set this and redirect to a website instead
        logf("MaybeStartUpdateDownload: didn't find download url\n");
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        NotifyUserOfUpdate(updateInfo);
        return 0;
    }

    if (!IsTrustedUpdateDlUrl(updateInfo->dlURL)) {
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        NotifySuspiciousUpdate(win, updateInfo->dlURL);
        delete updateInfo;
        return 0;
    }

    // pre-release automatic check: don't download yet. Show a bottom-left
    // notification whose "Update" link does the download + install.
    if (updateCheckType == UpdateCheck::Automatic && gIsPreReleaseBuild) {
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        delete gPendingUpdate;       // drop any update from a previous check
        gPendingUpdate = updateInfo; // take ownership (freed when installed/replaced)
        ShowUpdateAvailableNotification(win, updateInfo);
        return 0;
    }

    // download the installer to make update feel instant to the user
    logf("MaybeStartUpdateDownload: starting to download '%s'\n", updateInfo->dlURL);
    gUpdateCheckInProgress = true;

    auto* fnData = new DownloadUpdateAsyncData;
    fnData->win = win;
    fnData->updateInfo = updateInfo;
    RunAsync(MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateAsync, fnData), StrL("DownloadUpdateAsync"));
    return 0;
}

#if OS_DARWIN
// "mac-15.6.1"
static TempStr OsNameTemp() {
    char ver[32]{};
    size_t n = sizeof(ver) - 1;
    if (sysctlbyname("kern.osproductversion", ver, &n, nullptr, 0) != 0 || !ver[0]) {
        return StrL("mac");
    }
    return fmt("mac-%s", Str(ver));
}
#elif OS_LINUX
// The distribution's ID from os-release, e.g. "ubuntu" for ID=ubuntu or ID="ubuntu".
static TempStr OsNameTemp() {
    FILE* f = fopen("/etc/os-release", "r");
    if (!f) {
        f = fopen("/usr/lib/os-release", "r");
    }
    if (!f) {
        return StrL("linux");
    }
    constexpr Str kKey = StrL("ID=");
    TempStr res = StrL("linux");
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        Str s(line);
        if (!str::StartsWith(s, kKey)) {
            continue;
        }
        // keep what is safe in a url; that drops the quotes and the newline
        str::Builder id;
        for (int i = len(kKey); i < len(s); i++) {
            char c = s.s[i];
            if (isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-') {
                id.AppendChar(c);
            }
        }
        if (len(id) > 0) {
            res = str::DupTemp(ToStr(id));
        }
        break;
    }
    fclose(f);
    return res;
}
#elif !OS_WIN
static TempStr OsNameTemp() {
    return StrL("posix");
}
#endif

// Shared by update check and minidump upload: v, os, 64bit, arm, lang, store, simd.
void AppendClientInfoQuery(str::Builder& url) {
    url.Append(StrL("?v="));
    url.Append(StrL(UPDATE_CHECK_VERA));
    url.Append(StrL("&os="));
#if OS_WIN
    url.Append(GetWindowsVerTemp());
#else
    url.Append(OsNameTemp());
#endif
    url.Append(StrL("&64bit="));
    url.Append(Str(IsProcess64() ? "yes" : "no"));
    url.Append(StrL("&arm="));
    url.Append(Str(IsArmBuild() ? "yes" : "no"));
    Str lang = trans::GetCurrentLangCode();
    url.Append(StrL("&lang="));
    url.Append(lang);
    if (gIsStoreBuild) {
        url.Append(StrL("&store"));
    }
#if OS_WIN
    url.Append(StrL("&simd="));
    url.Append(LatestSupportedSIMD());
#endif
}

static void BuildUpdateURL(str::Builder& url, Str baseURL, UpdateCheck updateCheckType) {
    url.Reset(baseURL);
    AppendClientInfoQuery(url);
    url.Append(StrL("&withPromo"));
    if (UpdateCheck::UserInitiated == updateCheckType) {
        url.Append(StrL("&force"));
    }
}

struct UpdateCheckAsyncData {
    MainWindow* win = nullptr;
    UpdateCheck updateCheckType = UpdateCheck::Automatic;
    HttpRsp* rsp = nullptr;
    int nextUrl = 0;
    UpdateCheckAsyncData() = default;
    ~UpdateCheckAsyncData() { delete rsp; }
};

// ng: base's Http is winhttp. Off Windows gpui's http client answers instead
static bool UpdateHttpGet(Str uri, HttpRsp* rsp) {
#if OS_WIN
    return HttpGet(uri, rsp);
#else
    gp::HttpRsp gr;
    bool ok = gp::HttpGet(ToGpui(uri), &gr);
    rsp->httpStatusCode = (DWORD)gr.status;
    rsp->error = ok ? 0 : (DWORD)-1;
    if (ok && gr.body.len > 0) {
        rsp->data.Append(Str((char*)gr.body.els, gr.body.len));
    }
    gp::HttpRspFree(&gr);
    return ok && gr.status == 200;
#endif
}

static void UpdateCheckFinish(UpdateCheckAsyncData* data) {
    gUpdateCheckInProgress = false;
    AutoDelete delData(data);

    auto updateCheckType = data->updateCheckType;
    auto* rsp = data->rsp;
    MainWindow* win = nullptr;
    if (IsMainWindowValidAndNotClosing(data->win)) {
        win = data->win;
    } else if (len(gWindows) > 0) {
        win = gWindows[0];
    }
    if (!win || !rsp) {
        return;
    }
    DWORD err = MaybeStartUpdateDownload(win, rsp, updateCheckType);
    if ((err != 0) && (updateCheckType == UpdateCheck::UserInitiated)) {
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        // a manual check that couldn't fetch update info: tell the user and point
        // them at the website so they can update manually
        NotifyUpdateCheckFailed(win, err);
    }
}

#if OS_WASM
static void UpdateCheckWasmStart(UpdateCheckAsyncData* data);

static void UpdateCheckWasmDone(UpdateCheckAsyncData* data, gp::HttpAsyncResult result) {
    HttpRsp* rsp = data->rsp;
    gp::HttpRsp* gr = result.response;
    rsp->error = result.ok ? 0 : (DWORD)-1;
    rsp->httpStatusCode = gr ? (DWORD)gr->status : 0;
    if (gr && gr->body.len > 0) {
        rsp->data.Append(Str((char*)gr->body.els, gr->body.len));
    }
    logf("UpdateCheckAsync: response from '%s': error=%d, status=%d, %d bytes\n", rsp->url, (int)rsp->error,
         (int)rsp->httpStatusCode, (int)len(rsp->data));
    if (result.ok && gr && gr->status == 200) {
        UpdateCheckFinish(data);
        return;
    }
    UpdateCheckWasmStart(data);
}

static void UpdateCheckWasmStart(UpdateCheckAsyncData* data) {
    if (data->nextUrl >= dimofi(updateInfoURLs)) {
        UpdateCheckFinish(data);
        return;
    }

    delete data->rsp;
    data->rsp = nullptr;
    str::Builder url;
    BuildUpdateURL(url, updateInfoURLs[data->nextUrl++], data->updateCheckType);
    data->rsp = new HttpRsp;
    str::ReplaceWithCopy(&data->rsp->url, ToStr(url));
    gp::HttpReq req;
    req.url = ToGpui(data->rsp->url);
    req.method = GStrL("GET");
    if (!gp::HttpSendAsync(req, gp::MkFunc1<UpdateCheckAsyncData, gp::HttpAsyncResult>(UpdateCheckWasmDone, data))) {
        data->rsp->error = (DWORD)-1;
        UpdateCheckWasmStart(data);
    }
}
#endif

static void UpdateCheckAsync(UpdateCheckAsyncData* data) {
    auto updateCheckType = data->updateCheckType;
    HttpRsp* rsp = nullptr;
    for (auto updateInfoURL : updateInfoURLs) {
        delete rsp;
        str::Builder url;
        BuildUpdateURL(url, updateInfoURL, updateCheckType);
        Str uri = ToStr(url);
        rsp = new HttpRsp;
        str::ReplaceWithCopy(&rsp->url, uri);
        bool ok = UpdateHttpGet(uri, rsp);
        logf("UpdateCheckAsync: response from '%s': error=%d, status=%d, %d bytes\n", rsp->url, (int)rsp->error,
             (int)rsp->httpStatusCode, (int)len(rsp->data));
        if (ok) {
            break;
        }
    }
    data->rsp = rsp;
    uitask::Post(MkFunc0<UpdateCheckAsyncData>(UpdateCheckFinish, data), "TaskUpdateCheckFinish");
}

// start auto-update check by downloading auto-update information from url
// on a background thread and processing the retrieved data on ui thread
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
        args.timeoutMs = kNotifNoTimeout;
        args.groupId = kNotifUpdateCheckInProgress;
        ShowNotification(args);
    }
    CurrentFileTime(&gSettings->timeOfLastUpdateCheck);
    gUpdateCheckInProgress = true;

    // data freed in UpdateCheckFinish()
    auto* data = new UpdateCheckAsyncData();
    data->win = win;
    data->updateCheckType = updateCheckType;
#if OS_WASM
    UpdateCheckWasmStart(data);
#else
    RunAsync(MkFunc0<UpdateCheckAsyncData>(UpdateCheckAsync, data), StrL("UpdateCheckAsync"));
#endif
}

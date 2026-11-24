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
#include <unistd.h>
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
#include "UpdateCheckCommon.h"

#include "SumatraLog.h"

static Kind kNotifUpdateCheckInProgress = "notifUpdateCheckInProgress";

#if !OS_WIN
// base/Win.h is Windows-only; these are what the update query needs from it
bool IsProcess64() {
    return sizeof(void*) == 8;
}

bool IsArmBuild() {
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
// Wasm has no builds of its own to offer and asks orig's release url.

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
#else
static const Str updateInfoURLs[] = {
    StrL("https://www.sumatrapdfreader.org/update-check-rel.txt"),
};
#endif

#ifndef kWebisteDownloadPageURL
#define kWebisteDownloadPageURL "https://www.sumatrapdfreader.org/download-free-pdf-viewer"
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

/*
The format of update information downloaded from the server:

[SumatraPDF]
Latest: 14276
BuiltOn: 2026-08-21
Installer64: https://www.sumatrapdfreader.org/dl/prerel/14276/SumatraPDF-prerel-64-install.exe
...

ng: Linux has no installer to run, so its info names what the browser should
open instead:

DownloadPage: https://www.sumatrapdfreader.org/dlng/linux/26.10.03/...

A mac build replaces itself with the .zip of its architecture and opens the
.dmg in the browser when it can't (see ParseUpdateInfo):

ZipArm64: https://www.sumatrapdfreader.org/dlng/mac/26.10.03/SumatraPDF-mac-arm64.zip
ZipArm64Sha256: <hex>
DmgArm64: https://www.sumatrapdfreader.org/dlng/mac/26.10.03/SumatraPDF-mac-arm64.dmg
*/

static void OpenWebPage(Str url) {
    logf("UpdateCheck: opening '%s'\n", url);
    SumatraLaunchBrowser(url);
}

#if OS_WIN

// true when the installer started; only then may we exit for it to replace us
bool StartInstallerAutoUpgrade(Str installerPath) {
    TempStr expectedSigner = GetExecutableSignerTemp(GetSelfExePathTemp());
    TempStr installerSigner = GetExecutableSignerTemp(installerPath);
    if (len(expectedSigner) == 0 || len(installerSigner) == 0 || !str::Eq(expectedSigner, installerSigner) ||
        !IsPEFileSigned(installerPath)) {
        logf("StartInstallerAutoUpgrade: refusing an update with an untrusted signature\n");
        return false;
    }
    // orig asks the installer (IsOurExeInstalled()). Here "not portable" means installed.
    TempStr cmd = InstallerUpgradeArgsTemp(!IsRunningInPortableMode(), gIsPreReleaseBuild, GetSelfExePathTemp());
    logf("StartInstallerAutoUpgrade: installer cmd: '%s'\n", cmd);
    bool ok = CreateProcessHelper(installerPath, cmd);
    if (!ok) {
        logf("StartInstallerAutoUpgrade: starting the installer failed\n");
    }
    return ok;
}

static void ExitAfterStartingUpdater() {
    // Exit immediately so the updater can overwrite our exe
    ::ExitProcess(0);
}

#elif OS_DARWIN

// installerPath is the downloaded .zip of SumatraPDF.app
bool StartInstallerAutoUpgrade(Str installerPath) {
    return MacInstallUpdateZip(installerPath);
}

static void ExitAfterStartingUpdater() {
    // as ExitProcess() on Windows; the relauncher waits for us to be gone
    _exit(0);
}

#else

// no installer off Windows: the user is sent to the download page instead
bool StartInstallerAutoUpgrade(Str) {
    return false;
}

static void ExitAfterStartingUpdater() {}

#endif

// The build's own download when the update info names a trusted one.
static void OpenDownloadPage(UpdateInfo* updateInfo) {
    Str page = updateInfo->downloadPage;
    if (len(page) == 0 || !IsTrustedUpdateDlUrl(page)) {
        page = StrL(kWebisteDownloadPageURL);
    }
    OpenWebPage(page);
}

static void OnInstallAnswer(UpdateInfo* updateInfo, int res) {
    gUpdateCheckInProgress = false;
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
    if (StartInstallerAutoUpgrade(installerPath)) {
        ExitAfterStartingUpdater();
    } else {
        OpenDownloadPage(updateInfo);
    }
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
            if (StartInstallerAutoUpgrade(installerPathAuto)) {
                ExitAfterStartingUpdater();
            } else {
                OpenDownloadPage(updateInfo);
            }
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
    TempStr content = fmt("%s\n\n%s", mainInstr, fmt(fmtStr.s, currentVersion, ver));
    logf("NotifyUserOfUpdate: %s -> %s\n", currentVersion, ver);

    // as orig's modal dialog: no other check starts until this is answered
    gUpdateCheckInProgress = true;
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
    // the window that asked may have closed during the download: without one
    // MsgBox answers Cancel and the downloaded installer is deleted
    if (!IsMainWindowValidAndNotClosing(updateInfo->win)) {
        updateInfo->win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    }
    NotifyUserOfUpdate(updateInfo);
}

static void UpdateDownloadProgressNotif(UpdateProgressData* data) {
    TempStr size = FormatFileSizeShortTransTemp(data->nDownloaded);
    auto* wnd = GetNotificationForGroup(data->win, kNotifUpdateCheckInProgress);
    if (wnd) {
        NotificationUpdateMessage(wnd, fmt(Tr("Downloading update: %s").s, size), 0);
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
#elif OS_DARWIN
    UpdateInfo* updateInfo = data->updateInfo;
    TempStr zipPath = MacUpdateZipPathTemp();
    constexpr int kMaxUpdateDownloadSize = 256 * 1024 * 1024;
    gp::HttpRsp gr;
    bool ok = len(zipPath) > 0 && gp::HttpGet(ToGpui(updateInfo->dlURL), &gr) && gr.status == 200 && gr.body.len > 0 &&
              gr.body.len < kMaxUpdateDownloadSize;
    Str body = ok ? Str((char*)gr.body.els, gr.body.len) : Str();
    if (ok && len(updateInfo->dlSha256) > 0) {
        u8 digest[32];
        CalcSHA2Digest(body, digest);
        TempStr got = str::MemToHexTemp(Str((char*)digest, 32));
        ok = str::EqI(got, updateInfo->dlSha256);
        if (!ok) {
            logf("DownloadUpdateAsync: sha256 is %s, expected %s\n", got, updateInfo->dlSha256);
        }
    }
    ok = ok && file::WriteFile(zipPath, body);
    gp::HttpRspFree(&gr);
    logf("DownloadUpdateAsync: ok=%d, downloaded '%s' to '%s'\n", (int)ok, updateInfo->dlURL, zipPath);
    if (ok) {
        updateInfo->installerPath = str::Dup(zipPath);
    } else if (len(zipPath) > 0) {
        file::Delete(zipPath);
    }
#endif

    // process the rest on ui thread to avoid threading issues
    uitask::Post(MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateFinish, data), "TaskShowAutoUpdateDialog");
}

// an automatic check surfaces an available update with a bottom-left notification
// whose "Update" link triggers a one-click download + install
static void ShowUpdateAvailableNotification(MainWindow* win, UpdateInfo* updateInfo) {
    if (!win || !updateInfo) {
        return;
    }
    TempStr link = fmt("[%s](CmdInstallPrereleaseUpdate)", Tr("Update"));
    Str displayVer = updateInfo->latestVer;
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

// download + install the update surfaced by the update notification
void DownloadAndInstallPendingUpdate(MainWindow* win) {
    if (!win || !gPendingUpdate) {
        return;
    }
    // a check or download is running: it offers the update itself. Starting
    // a second one ran two downloads, and the first to finish installed
    // without asking
    if (gUpdateCheckInProgress) {
        return;
    }
    UpdateInfo* updateInfo = gPendingUpdate;
    gPendingUpdate = nullptr;
    RemoveNotificationsForGroup(win, kNotifUpdateAvailable);

    // nothing this build can install (e.g. run from the disk image)
    if (len(updateInfo->dlURL) == 0) {
        OpenDownloadPage(updateInfo);
        delete updateInfo;
        return;
    }
    gUpdateAutoInstall = true;
    updateInfo->win = win;

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
#if OS_DARWIN
    // a version not built for our architecture (yet)
    if (len(updateInfo->dlURL) == 0) {
        return false;
    }
#endif
    // a store build lets the store install updates
    return ShouldOfferProgramUpdate(updateInfo->latestVer, currentVersion, gIsStoreBuild);
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

#if OS_LINUX
static void PickLinuxPackage(UpdateInfo* updateInfo, Str data);
#endif

static DWORD MaybeStartUpdateDownload(MainWindow* win, HttpRsp* rsp, UpdateCheck updateCheckType) {
    Str url = rsp->url;
    str::Builder* data = &rsp->data;
    UpdateInfoFetch kind = ClassifyUpdateInfoFetch(url, (int)rsp->error, (int)rsp->httpStatusCode, len(*data),
                                                   updateInfoURLs, dimof(updateInfoURLs));
    if (kind == UpdateInfoFetch::TransportError) {
        logf("MaybeStartUpdateDownload: http get of '%s' failed with %d\n", url, (int)rsp->error);
        return rsp->error;
    }
    if (kind == UpdateInfoFetch::BadStatus) {
        logf("MaybeStartUpdateDownload: http get of '%s' failed with code %d\n", url, (int)rsp->httpStatusCode);
        return (DWORD)rsp->httpStatusCode;
    }
    if (kind == UpdateInfoFetch::UnlistedUrl) {
        logf("MaybeStartUpdateDownload: '%s' is not a valid url\n", url);
        return 1;
    }
    if (kind == UpdateInfoFetch::EmptyBody) {
        logf("MaybeStartUpdateDownload: empty response from url '%s'\n", url);
        return 2;
    }

    UpdateInfo* updateInfo = ParseUpdateInfo(ToStr(*data));
    if (!updateInfo) {
        logf("MaybeStartUpdateDownload: ParseUpdateInfo() failed. URL: '%s'\n", url);
        return 3;
    }
    updateInfo->win = win;
#if OS_LINUX
    PickLinuxPackage(updateInfo, ToStr(*data));
#endif

    if (!ShouldDownloadUpdate(updateInfo)) {
        Str myVer = currentVersion;
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

#if OS_DARWIN
    // e.g. run from the disk image: the browser gets the .dmg instead
    if (!MacCanUpdateSelf()) {
        str::FreePtr(&updateInfo->dlURL);
    }
#endif

    if (len(updateInfo->dlURL) > 0 && !IsTrustedUpdateDlUrl(updateInfo->dlURL)) {
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        NotifySuspiciousUpdate(win, updateInfo->dlURL);
        delete updateInfo;
        return 0;
    }

    // ng: every build is dated like orig's pre-release ones, so an automatic
    // check doesn't download yet. A bottom-left notification's "Update" link
    // does the download + install, or opens the download page.
    if (updateCheckType == UpdateCheck::Automatic) {
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        delete gPendingUpdate;       // drop any update from a previous check
        gPendingUpdate = updateInfo; // take ownership (freed when installed/replaced)
        ShowUpdateAvailableNotification(win, updateInfo);
        return 0;
    }

    if (len(updateInfo->dlURL) == 0) {
        // currently for release builds we don't set this and redirect to a website instead
        logf("MaybeStartUpdateDownload: didn't find download url\n");
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        NotifyUserOfUpdate(updateInfo);
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
// A value from os-release, e.g. "ubuntu" for ID=ubuntu or ID="ubuntu", "" if missing.
static TempStr OsReleaseValueTemp(Str key) {
    FILE* f = fopen("/etc/os-release", "r");
    if (!f) {
        f = fopen("/usr/lib/os-release", "r");
    }
    if (!f) {
        return {};
    }
    TempStr prefix = str::JoinTemp(key, StrL("="));
    TempStr res{};
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        Str s(line);
        if (!str::StartsWith(s, prefix)) {
            continue;
        }
        // keep what is safe in a url; that drops the quotes and the newline
        str::Builder v;
        for (int i = len(prefix); i < len(s); i++) {
            char c = s.s[i];
            if (isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-') {
                v.AppendChar(c);
            }
        }
        if (len(v) > 0) {
            res = str::DupTemp(ToStr(v));
        }
        break;
    }
    fclose(f);
    return res;
}

// The distribution's ID from os-release, e.g. "ubuntu".
static TempStr OsNameTemp() {
    TempStr id = OsReleaseValueTemp(StrL("ID"));
    return len(id) > 0 ? id : StrL("linux");
}

// Opens this distro's package instead of DownloadPage (the list of all) when
// the update info has one, e.g. "Package-ubuntu24.04-x86_64: <.deb url>".
// A rolling release has no VERSION_ID: "Package-arch-x86_64".
static void PickLinuxPackage(UpdateInfo* updateInfo, Str data) {
    TempStr id = OsReleaseValueTemp(StrL("ID"));
    TempStr ver = OsReleaseValueTemp(StrL("VERSION_ID"));
    if (len(id) == 0) {
        return;
    }
#if defined(__aarch64__)
    Str arch = StrL("aarch64");
#else
    Str arch = StrL("x86_64");
#endif
    SquareTreeNode* root = ParseSquareTree(data);
    if (!root) {
        return;
    }
    AutoDelete delRoot(root);
    SquareTreeNode* node = root->GetChild(StrL("SumatraPDF"));
    TempStr key = fmt("Package-%s%s-%s", id, ver, arch);
    Str url = node ? node->GetValue(key) : Str{};
    logf("PickLinuxPackage: %s: '%s'\n", key, url);
    if (len(url) == 0 || !IsTrustedUpdateDlUrl(url)) {
        return;
    }
    str::ReplaceWithCopy(&updateInfo->downloadPage, url);
}
#elif !OS_WIN
static TempStr OsNameTemp() {
    return StrL("posix");
}
#endif

// Shared by update check and minidump upload: v, os, 64bit, arm, lang, store, simd.
void AppendClientInfoQuery(str::Builder& url) {
    url.Append(StrL("?v="));
    url.Append(currentVersion);
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
#if OS_DARWIN
        // tells installs apart in the update check stats
        url.Append(StrL("&id="));
        url.Append(MacMachineIdHashTemp());
#endif
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
    if (!ShouldCheckForUpdate(updateCheckType, gUpdateCheckInProgress)) {
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
    GetSystemTimeAsFileTime(&gSettings->timeOfLastUpdateCheck);
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

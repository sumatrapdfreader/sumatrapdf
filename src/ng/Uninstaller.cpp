/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's uninstaller window is win32 over the same painted frame as the
// installer's; here it is a gpui window with the same logo, message and single
// button. The previewer / search filter un-registration is gone with those
// features (step 17a).

#include "gui/GpuiBridge.h"

#include "base/File.h"
#include "base/Win.h"
#include "base/Timer.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "SumatraConfig.h"
#include "Flags.h"
#include "Version.h"
#include "AppTools.h"
#include "Translations.h"
#include "Installer.h"
#include "InstallerUtil.h"

#include "SumatraLog.h"

#if OS_WIN

// the installer's logo / message elements, shared as orig shares OnPaintFrame
gp::El* BuildInstallerLogo(gp::Ctx* cx);
gp::El* BuildInstallerMessage(gp::Ctx* cx);

struct UninstallerWnd {
    gp::App* app = nullptr;
    gp::Window* win = nullptr;
    bool uninstalling = false;
    bool finished = false;
    ThreadHandle hThread = nullptr;
};

static UninstallerWnd* gWnd = nullptr;
static bool gSuccess = false;
static volatile LONG gUninstallFinished = 0;
static Str gUninstallerLogPath;

static void RemoveInstalledFiles() {
    // can't use GetExistingInstallationDir() anymore because we
    // delete registry entries
    Str dir = gCli->installDir;
    if (len(dir) == 0) {
        log(StrL("RemoveInstalledFiles(): dir is empty\n"));
        return;
    }
    bool ok = dir::RemoveAll(dir);
    logf("RemoveInstalledFiles(): removed dir '%s', ok = %d\n", dir, (int)ok);
}

static void UninstallerThread() {
    log(StrL("UninstallerThread started\n"));
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    TempStr exePath = GetInstalledExePathTemp();
    TempStr ownPath = GetSelfExePathTemp();
    if (!path::IsSame(exePath, ownPath)) {
        KillProcessesWithModule(exePath, true);
    }

    // TODO: reconsider what is failure
    bool ok = RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    ok |= RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);

    if (!ok) {
        log(StrL("RemoveUninstallerRegistryInfo failed\n"));
        NotifyFailed(Tr("Failed to delete uninstaller registry keys"));
    }

    RemoveInstallRegistryKeys(HKEY_LOCAL_MACHINE);
    RemoveInstallRegistryKeys(HKEY_CURRENT_USER);
    RemoveAppShortcuts();

    RemoveInstallDirFromPath(gCli->allUsers, gCli->installDir);
    RemoveInstalledFiles();
    LoggedDeleteRegValue(HKEY_CURRENT_USER, StrL("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                         StrL("SumatraPDF-QuickLook"));

    // always succeed, even for partial uninstallations
    gSuccess = true;

    log(StrL("UninstallerThread finished\n"));
    InterlockedExchange(&gUninstallFinished, 1);
}

static void OnButtonUninstall(UninstallerWnd* wnd) {
    if (!CheckInstallUninstallPossible()) {
        return;
    }

    // disable the button during uninstallation
    wnd->uninstalling = true;
    SetMsg(Tr("Uninstallation in progress..."), kColorMsgInstallation);

    auto fn = MkFunc0Void(UninstallerThread);
    wnd->hThread = StartThread(fn, StrL("UninstallerThread"));
}

static void OnUninstallationFinished(UninstallerWnd* wnd) {
    wnd->uninstalling = false;
    wnd->finished = true;
    SetMsg(Tr("SumatraPDF has been uninstalled."), gMsgError ? kColorMsgFailed : kColorMsgOk);
    gMsgError = gFirstError;
    SafeCloseThreadHandle(&wnd->hThread);
}

struct UninstallerView {
    static gp::El* Render(UninstallerView* self, gp::Ctx* cx);
    static void OnTick(UninstallerView* self, gp::Ctx* cx, const gp::TickEvent*);
    static void OnUninstall(UninstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnExit(UninstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<UninstallerView> gUninstallerView;

void UninstallerView::OnTick(UninstallerView*, gp::Ctx* cx, const gp::TickEvent*) {
    bool needsRedraw = IsRevealingLettersAnimRunning();
    AnimStep();
    if (gWnd && gWnd->uninstalling && InterlockedCompareExchange(&gUninstallFinished, 0, 1) == 1) {
        OnUninstallationFinished(gWnd);
        needsRedraw = true;
    }
    if (needsRedraw) {
        gp::Notify(cx);
    }
}

void UninstallerView::OnUninstall(UninstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    OnButtonUninstall(gWnd);
    gp::Notify(cx);
}

void UninstallerView::OnExit(UninstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gp::AppQuit(cx->win);
}

gp::El* UninstallerView::Render(UninstallerView*, gp::Ctx* cx) {
    UninstallerWnd* wnd = gWnd;
    float margin = 8;

    gp::El* root = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->H(gp::kFill)->Bg(ToGpui(kInstallerWinBgColor));
    root->Child(gp::Div(cx->a)->W(gp::kFill)->PadT(18)->Child(BuildInstallerLogo(cx)));
    root->Child(gp::Div(cx->a)->Flex1()->W(gp::kFill)->FlexCol()->JustifyCenter()->Pad(margin)->Child(
        BuildInstallerMessage(cx)));

    gp::El* bottom = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8)->Pad(margin);
    bottom->Child(gp::Div(cx->a)->Flex1());
    if (wnd->finished) {
        bottom->Child(gpc::Button::New(cx, GStrL("uninst-close"))
                          ->Label(ToGpui(Tr("Close")))
                          ->Primary()
                          ->WithSize(gp::UiSize::Small)
                          ->OnClick(gp::ListenTo(gUninstallerView, &UninstallerView::OnExit))
                          ->IntoEl());
    } else {
        bottom->Child(gpc::Button::New(cx, GStrL("uninst-run"))
                          ->Label(ToGpui(Tr("Uninstall SumatraPDF")))
                          ->Primary()
                          ->Disabled(wnd->uninstalling)
                          ->WithSize(gp::UiSize::Small)
                          ->OnClick(gp::ListenTo(gUninstallerView, &UninstallerView::OnUninstall))
                          ->IntoEl());
    }
    root->Child(bottom);
    return root;
}

static bool CreateUninstallerWindow() {
    gWnd = new UninstallerWnd();
    gp::App* app = gp::AppNew();
    gpc::Init(app);
    gWnd->app = app;
    gUninstallerView = gp::EntityNew<UninstallerView>(app);

    TempStr title = fmt(Tr("SumatraPDF %s Uninstaller").s, currentVersion);
    int dx = GetInstallerWinDx();
    int dy = kInstallerWinDy;
    gWnd->win = gp::WindowOpenView(app, ToGpui(title), dx, dy, gUninstallerView.id, gp::WinOpts{});
    if (!gWnd->win) {
        return false;
    }
    gp::WindowSetInterval(gWnd->win, 33, gp::ListenTo(gUninstallerView, &UninstallerView::OnTick));
    SetDefaultMsg();
    RevealingLettersAnimStart();
    return true;
}

static void ShowUsage() {
    // Note: translation services aren't initialized at this point, so English only
    TempStr caption = str::JoinTemp(StrL(kAppName), StrL(" Uninstaller Usage"));
    TempStr msg = fmt(R"(SumatraPDF.exe -uninstall [-s][-d <path>]

-s	uninstalls %s silently (without user interaction).
-d	changes the directory from where %s will be uninstalled.)",
                      StrL(kAppName), StrL(kAppName));
    logf("%s\n%s\n", caption, msg);
}

int RunUninstaller(Flags* cli) {
    gCli = cli;
    gLogRegistryCalls = true;
    trans::SetCurrentLangByCode(trans::DetectUserLang());

    if (gCli->log) {
        // same as installer
        gUninstallerLogPath = GetInstallerLogPath();
        if (gUninstallerLogPath) {
            StartLogToFile(gUninstallerLogPath, false);
        }
        logf("------------- Starting SumatraPDF uninstallation\n");
    }

    // TODO: remove dependency on this in the uninstaller
    // dup from the perm arena: flag strings are never individually freed
    if (len(gCli->installDir) == 0) {
        gCli->installDir = str::Dup(GetPermArena(), GetExistingInstallationDirTemp());
    }
    Str instDir = gCli->installDir;
    TempStr exePath = GetSelfExePathTemp();
    logf("Running uninstaller '%s' for '%s'\n", exePath, instDir);

    int ret = 1;
    if (!file::Exists(exePath)) {
        log(StrL("Uninstaller executable doesn't exist\n"));
        // ng: orig shows a MessageBox; the port's MsgBox needs a MainWindow,
        // which the uninstaller has none of
        logf("%s: %s\n", Tr("Uninstallation failed"), Tr("SumatraPDF installation not found."));
        return ret;
    }

    if (gCli->showHelp) {
        ShowUsage();
        return 0;
    }

    RelaunchMaybeElevatedFromTempDirectory(gCli);

    gDefaultMsg = Tr("Are you sure you want to uninstall SumatraPDF?");

    if (gCli->silent) {
        UninstallerThread();
        return gSuccess ? 0 : 1;
    }

    if (!CreateUninstallerWindow()) {
        return ret;
    }
    ret = gp::AppRun(gWnd->app);

    InitSelfDelete();
    LaunchFileIfExists(gUninstallerLogPath);
    return ret;
}

#else

int RunUninstaller(Flags*) {
    log(StrL("-uninstall is not supported on this platform\n"));
    return 1;
}

#endif

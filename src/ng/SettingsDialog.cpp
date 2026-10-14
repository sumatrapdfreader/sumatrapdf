/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SettingsDialog.cpp is a WS_POPUPWINDOW with two DropDowns and five
// Checkboxes. Here it is a gpui Dialog with the same rows in the same order:
// the "View" section (Default Layout, Default Zoom, the two checkboxes), the
// "Advanced" section (Use tabs, check for updates, remember opened files) and,
// when TeX enhancements are on, the inverse search command line.

#include "gui/GpuiBridge.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "FileThumbnails.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

#include "SumatraLog.h"

struct SettingsDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    bool showInverseSearch = false;
    float scrollY = 0;
    Vec<float> zoomLevels;
    float startZoom = 0;
    DialogSelect ddLayout;
    DialogSelect ddZoom;
    DialogSelect ddInverse;
    gpui::InputState* editZoom = nullptr;
    gpui::InputState* editInverse = nullptr;
    bool showToc = false;
    bool rememberState = false;
    bool useTabs = false;
    bool checkUpdates = false;
    bool rememberOpened = false;
};

static SettingsDlg gSettingsDlg;

// orig's layout: View, Advanced and inverse search as sections under muted
// headings, hairlines between, the footer
constexpr float kOptDlgDx = 480;
// the frame's card scrolls past this
constexpr float kOptCardMaxDy = 520;

struct SettingsView {
    static void OnOk(SettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(SettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnToggle(SettingsView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t which);
    static void OnScroll(SettingsView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<SettingsView> gSettingsView;

// orig's window (modeless, as orig's), where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gSettingsTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsSettingsDialogVisible() {
    return gSettingsDlg.visible && !gSettingsTw;
}

void CloseSettingsDialog() {
    if (!gSettingsDlg.visible) {
        return;
    }
    gSettingsDlg.visible = false;
    DlgWindowClose(&gSettingsTw);
    MainWindow* win = gSettingsDlg.win;
    if (win && win->gpuiWin) {
        if (gSettingsDlg.editZoom) {
            gp::InputBlur(gSettingsDlg.editZoom, win->gpuiWin->app, win->gpuiWin);
        }
        if (gSettingsDlg.editInverse) {
            gp::InputBlur(gSettingsDlg.editInverse, win->gpuiWin->app, win->gpuiWin);
        }
    }
    delete gSettingsDlg.editZoom;
    gSettingsDlg.editZoom = nullptr;
    delete gSettingsDlg.editInverse;
    gSettingsDlg.editInverse = nullptr;
    gSettingsDlg.ddLayout.Free();
    gSettingsDlg.ddZoom.Free();
    gSettingsDlg.ddInverse.Free();
    VecReset(gSettingsDlg.zoomLevels);
    AppShellInvalidate(win);
}

// orig's SettingsWnd::FillLayout
static void FillLayout() {
    StrVec items;
    items.Append(Tr("Automatic"));
    items.Append(Tr("Single Page"));
    items.Append(Tr("Facing"));
    items.Append(Tr("Book View"));
    items.Append(Tr("Continuous"));
    items.Append(Tr("Continuous Facing"));
    items.Append(Tr("Continuous Book View"));
    items.Append(Tr("Page Aspect"));
    int sel = 0;
    if (IsPageAspectDisplayMode(gSettings->defaultDisplayMode)) {
        sel = len(items) - 1;
    } else {
        sel = (int)gSettings->defaultDisplayModeEnum - (int)DisplayMode::Automatic;
    }
    if (sel < 0 || sel >= len(items)) {
        sel = 0;
    }
    gSettingsDlg.ddLayout.SetItems(items, sel);
}

// orig's SettingsWnd::FillZoom
static void FillZoom() {
    gSettingsDlg.startZoom = gSettings->defaultZoomFloat;
    CollectZoomLevels(gSettingsDlg.zoomLevels, false);
    StrVec items;
    int sel = -1;
    for (int i = 0; i < len(gSettingsDlg.zoomLevels); i++) {
        float z = gSettingsDlg.zoomLevels[i];
        items.Append(ZoomLevelStrExact(z));
        if (z == gSettingsDlg.startZoom) {
            sel = i;
        }
    }
    gSettingsDlg.ddZoom.SetItems(items, sel);
    Str text = sel >= 0 ? items[sel] : Str(fmt("%.0f%%", gSettingsDlg.startZoom));
    gp::InputSetValue(gSettingsDlg.editZoom, ToGpui(text));
}

// orig's SettingsWnd::FillInverse
static void FillInverse() {
    StrVec items;
    Str cmdLine = gSettings->inverseSearchCmdLine;
    CollectInverseSearchCommands(items, cmdLine);
    if (len(cmdLine) == 0 && len(items) > 0) {
        cmdLine = items[0];
    }
    gSettingsDlg.ddInverse.SetItems(items, items.Find(cmdLine));
    gp::InputSetValue(gSettingsDlg.editInverse, ToGpui(cmdLine));
}

// orig's SettingsWnd::SelectedZoom: the list entry, or a typed number
static float SelectedZoom() {
    TempStr text = str::DupTemp(FromGpui(gp::InputValue(gSettingsDlg.editZoom)));
    for (int i = 0; i < len(gSettingsDlg.zoomLevels); i++) {
        if (str::EqI(text, ZoomLevelStrExact(gSettingsDlg.zoomLevels[i]))) {
            float z = gSettingsDlg.zoomLevels[i];
            return z == 0 ? gSettingsDlg.startZoom : z;
        }
    }
    if (len(text) == 0) {
        return gSettingsDlg.startZoom;
    }
    float zoom = (float)atof(CStrTemp(text));
    if (zoom == 0) {
        return gSettingsDlg.startZoom;
    }
    return limitValue(zoom, kZoomMin, kZoomMax);
}

static Str SettingsDlgTitle() {
    return Tr("Settings");
}

// orig's window has no owner and stays when its main window closes
static void SettingsDlgOnOwnerClosed(MainWindow* newOwner) {
    gSettingsDlg.win = newOwner;
}

void ShowSettingsDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences) || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    CloseSettingsDialog();
    gSettingsDlg.win = win;
    gSettingsDlg.showInverseSearch = gSettings->enableTeXEnhancements && CanAccessDisk();
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;

    auto* ez = new gp::InputState();
    ez->focus = gp::FocusHandleNew(app);
    gSettingsDlg.editZoom = ez;
    auto* ei = new gp::InputState();
    ei->focus = gp::FocusHandleNew(app);
    gSettingsDlg.editInverse = ei;

    gSettingsDlg.ddLayout.Init(app);
    gSettingsDlg.ddZoom.Init(app);
    gSettingsDlg.ddInverse.Init(app);
    FillLayout();
    FillZoom();
    FillInverse();

    gSettingsDlg.showToc = gSettings->showToc;
    gSettingsDlg.rememberState = gSettings->rememberStatePerDocument;
    gSettingsDlg.useTabs = gSettings->useTabs;
    gSettingsDlg.checkUpdates = gSettings->checkForUpdates;
    gSettingsDlg.rememberOpened = gSettings->rememberOpenedFiles;
    gSettingsDlg.scrollY = 0;
    gSettingsDlg.visible = true;
    DlgWindowSpec spec;
    spec.name = "options";
    spec.title = SettingsDlgTitle;
    spec.modal = false;
    spec.build = SettingsDialogBuild;
    spec.close = CloseSettingsDialog;
    spec.onOwnerClosed = SettingsDlgOnOwnerClosed;
    spec.clientDx = kOptDlgDx;
    gSettingsTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

// orig's SettingsWnd::OnOk
static void SettingsOk() {
    int layoutIdx = gSettingsDlg.ddLayout.sel;
    int nLayout = len(gSettingsDlg.ddLayout.titles);
    if (layoutIdx >= 0 && nLayout > 0 && layoutIdx == nLayout - 1) {
        str::ReplaceWithCopy(&gSettings->defaultDisplayMode, StrL("page aspect"));
        gSettings->defaultDisplayModeEnum = DisplayMode::Automatic;
    } else if (layoutIdx >= 0) {
        gSettings->defaultDisplayModeEnum = (DisplayMode)(layoutIdx + (int)DisplayMode::Automatic);
        str::ReplaceWithCopy(&gSettings->defaultDisplayMode, DisplayModeToString(gSettings->defaultDisplayModeEnum));
    }
    gSettings->defaultZoomFloat = SelectedZoom();
    gSettings->showToc = gSettingsDlg.showToc;
    gSettings->rememberStatePerDocument = gSettingsDlg.rememberState;
    gSettings->useTabs = gSettingsDlg.useTabs;
    gSettings->checkForUpdates = gSettingsDlg.checkUpdates;
    gSettings->rememberOpenedFiles = gSettingsDlg.rememberOpened;
    if (gSettingsDlg.showInverseSearch) {
        TempStr tmp = str::DupTemp(FromGpui(gp::InputValue(gSettingsDlg.editInverse)));
        str::ReplaceWithCopy(&gSettings->inverseSearchCmdLine, tmp);
    }
    logf("SettingsDialog: layout '%s', zoom %.2f, toc %d, tabs %d\n", gSettings->defaultDisplayMode,
         gSettings->defaultZoomFloat, (int)gSettings->showToc, (int)gSettings->useTabs);

    if (!SettingsRememberOpenedFiles()) {
        FileHistoryClear(true);
        EmptyThumbnailCacheDirectory();
    }
    UpdateDocumentColors();
    // note: ideally we would also update state for useTabs changes but that's complicated since
    // to do it right we would have to convert tabs to windows
    ApplySettingsToOpenWindows();
    ScheduleSaveSettings();
    MaybeRedrawHomePage();
    CloseSettingsDialog();
}

void SettingsView::OnOk(SettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    SettingsOk();
    gp::Notify(cx);
}

void SettingsView::OnCancel(SettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseSettingsDialog();
    gp::Notify(cx);
}

void SettingsView::OnToggle(SettingsView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t which) {
    switch (which) {
        case 0:
            gSettingsDlg.showToc = !gSettingsDlg.showToc;
            break;
        case 1:
            gSettingsDlg.rememberState = !gSettingsDlg.rememberState;
            break;
        case 2:
            gSettingsDlg.useTabs = !gSettingsDlg.useTabs;
            break;
        case 3:
            gSettingsDlg.checkUpdates = !gSettingsDlg.checkUpdates;
            break;
        case 4:
            gSettingsDlg.rememberOpened = !gSettingsDlg.rememberOpened;
            break;
    }
    gp::Notify(cx);
    AppShellInvalidate(gSettingsDlg.win);
}

void SettingsView::OnScroll(SettingsView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gSettingsDlg.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gSettingsDlg.win);
}

gp::El* SettingsDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gSettingsDlg.visible || gSettingsDlg.win != win) {
        return nullptr;
    }
    if (gSettingsTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gSettingsView.IsValid()) {
        gSettingsView = gp::EntityNewState<SettingsView>(cx->app);
    }
    gSettingsDlg.ddLayout.PollChanged(cx->app);
    gp::Listener onOk = gp::ListenTo(gSettingsView, &SettingsView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gSettingsView, &SettingsView::OnCancel);
    DlgSetDefault(cx, onOk, true);
    Color colMuted = ThemeWindowTextDisabledColor();

    auto check = [&](Str id, Str label, bool checked, bool disabled, intptr_t which) {
        return DlgFieldRow(cx)->Child(DlgCheck(cx, id, label, checked,
                                               gp::ListenTo(gSettingsView, &SettingsView::OnToggle, which),
                                               DlgCheckKind::Checkbox, disabled));
    };
    Str layoutLabel = Tr("Default &Layout:");
    Str zoomLabel = Tr("Default &Zoom:");
    float labelDx = std::max(DlgFieldLabelDx(cx, layoutLabel), DlgFieldLabelDx(cx, zoomLabel));
    auto row = [&](gp::Str label, gp::El* ctrl) {
        gp::El* l = gp::Div(cx->a)->W(labelDx)->Shrink0()->Child(
            DlgAccelText(cx, label)->Font(kToolDlgTextFontPx)->Fg(ToGpui(colMuted)));
        return DlgFieldRow(cx)->Child(l)->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(ctrl));
    };

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    gp::El* view = DlgSection(cx);
    view->Child(DlgSectionHeader(cx, Tr("View")));
    view->Child(row(DlgAccelSelect(cx, layoutLabel, &gSettingsDlg.ddLayout),
                    gSettingsDlg.ddLayout.Build(cx, StrL("opt-layout"), gp::kFill)));
    // orig's editable combo box of zoom levels
    view->Child(row(DlgAccelInput(cx, zoomLabel, gSettingsDlg.editZoom),
                    gSettingsDlg.ddZoom.BuildCombo(cx, StrL("opt-zoom-edit"), gSettingsDlg.editZoom, gp::kFill, false,
                                                   kDlgWinEditDy, kToolDlgTextFontPx)));
    view->Child(
        check(StrL("opt-showtoc"), Tr("Show the &bookmarks sidebar when available"), gSettingsDlg.showToc, false, 0));
    view->Child(check(StrL("opt-remember"), Tr("&Remember these settings for each document"),
                      gSettingsDlg.rememberState, !gSettingsDlg.rememberOpened, 1));
    col->Child(view);
    col->Child(DlgHairline(cx));

    gp::El* advanced = DlgSection(cx);
    advanced->Child(DlgSectionHeader(cx, Tr("Advanced")));
    advanced->Child(check(StrL("opt-tabs"), Tr("Use &tabs"), gSettingsDlg.useTabs, false, 2));
    advanced->Child(check(StrL("opt-updates"), Tr("Automatically check for &updates"), gSettingsDlg.checkUpdates,
                          !HasPermission(Perm::InternetAccess), 3));
    advanced->Child(check(StrL("opt-opened"), Tr("Remember &opened files"), gSettingsDlg.rememberOpened, false, 4));
    col->Child(advanced);

    if (gSettingsDlg.showInverseSearch) {
        col->Child(DlgHairline(cx));
        gp::El* inverse = DlgSection(cx);
        inverse->Child(DlgSectionHeader(cx, Tr("Set inverse search command line")));
        Str prompt = Tr("Enter the command line to invoke when you double-click on the PDF document:");
        inverse->Child(DlgFieldRow(cx)->Child(DlgFieldLabel(cx, prompt, 0)));
        inverse->Child(
            gp::Div(cx->a)
                ->W(gp::kFill)
                ->PadB(kToolDlgFilesPadDy)
                ->Shrink0()
                ->Child(gSettingsDlg.ddInverse.BuildCombo(cx, StrL("opt-inverse-edit"), gSettingsDlg.editInverse,
                                                          gp::kFill, false, kDlgWinEditDy, kToolDlgTextFontPx)));
        col->Child(inverse);
    }
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel));

    if (!DlgWindowIsHost(cx)) {
        // the frame may be shorter than the dialog
        col = gp::Div(cx->a)
                  ->Id(GStrL("opt-body"))
                  ->FlexCol()
                  ->W(gp::kFill)
                  ->MaxH(kOptCardMaxDy)
                  ->ScrollY(gSettingsDlg.scrollY)
                  ->ScrollFromPath()
                  ->OnScroll(gp::ListenTo(gSettingsView, &SettingsView::OnScroll))
                  ->Child(col);
    }
    return DlgToolContent(cx, col, Tr("Settings"), kOptDlgDx, onCancel);
}

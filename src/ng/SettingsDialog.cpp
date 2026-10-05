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

// orig's window at 96 dpi: a 480 wide client area, 4 / 8 around. "View" with
// 4 under it; the two labelled drop-downs, their labels sharing a column, 8
// between the columns and 4 between the rows; the checkboxes 8 or 4 under
// what is above them; "Advanced" with 12 above and 4 under it; the buttons
// with 4 above and below
constexpr float kOptWinDx = 480;
constexpr float kOptWinPadX = 8;
constexpr float kOptWinPadY = 4;
constexpr float kOptWinColGap = 8;
constexpr float kOptWinCheckGap = 8;
constexpr float kOptWinSectionGap = 12;

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
    spec.clientDx = kOptWinDx;
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

static gp::El* SectionLabel(gp::Ctx* cx, Str s) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    return gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(13)->Bold()->Fg(th.foreground);
}

static gp::El* LabeledRow(gp::Ctx* cx, gp::Str label, gp::El* ctrl) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* row = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8);
    row->Child(DlgAccelText(cx, label)->Font(13)->Fg(th.foreground)->W(140)->Shrink0());
    row->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(ctrl));
    return row;
}

static gp::El* CheckRow(gp::Ctx* cx, Str id, Str label, bool checked, bool disabled, intptr_t which) {
    gp::El* box = DlgAccelEl(cx, gpc::Checkbox::New(cx, GpuiDup(cx->a, id))->Checked(checked)->Disabled(disabled),
                             label, gp::ListenTo(gSettingsView, &SettingsView::OnToggle, which), disabled);
    // ng: gpui's Checkbox reports its indicator's 2 px top margin as content
    // past its own box, which made the body scroll by 2 px; a clipping
    // wrapper keeps that out of the body's content height
    return gp::Div(cx->a)->W(gp::kFill)->ClipY()->Child(box);
}

void SettingsView::OnScroll(SettingsView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gSettingsDlg.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gSettingsDlg.win);
}

static gp::El* SettingsWinCheck(gp::Ctx* cx, Str id, Str label, bool checked, bool disabled, intptr_t which,
                                float padT) {
    gp::El* box = DlgAccelEl(cx, gpc::Checkbox::New(cx, GpuiDup(cx->a, id))->Checked(checked)->Disabled(disabled),
                             label, gp::ListenTo(gSettingsView, &SettingsView::OnToggle, which), disabled);
    return DlgWinCheck(cx, box, padT);
}

static gp::El* SettingsWinBuild(gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kOptWinPadX)->PadY(kOptWinPadY);
    col->Child(DlgWinLabel(cx, ToGpui(Tr("View")), font, kOptWinPadY));

    Str layoutLabel = Tr("Default &Layout:");
    Str zoomLabel = Tr("Default &Zoom:");
    float labelDx = std::max(DlgWinTextDx(cx, layoutLabel), DlgWinTextDx(cx, zoomLabel));
    auto row = [&](gp::Str label, gp::El* ctrl, float padB) {
        gp::El* r =
            gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(kDlgWinEditDy + padB)->PadB(padB)->Shrink0();
        r->Child(gp::Div(cx->a)
                     ->W(labelDx + kOptWinColGap)
                     ->Shrink0()
                     ->Child(DlgAccelText(cx, label)->Font(font)->Fg(th.foreground)));
        r->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(ctrl));
        return r;
    };
    // gpui's Select has no height of its own to set: its box is the first child
    gp::El* layoutSel = gSettingsDlg.ddLayout.Build(cx, StrL("opt-layout"), gp::kFill)->H(kDlgWinEditDy);
    if (layoutSel->first) {
        layoutSel->first->H(kDlgWinEditDy);
    }
    col->Child(row(DlgAccelSelect(cx, layoutLabel, &gSettingsDlg.ddLayout), layoutSel, kOptWinPadY));
    col->Child(row(DlgAccelInput(cx, zoomLabel, gSettingsDlg.editZoom),
                   gSettingsDlg.ddZoom.BuildCombo(cx, StrL("opt-zoom-edit"), gSettingsDlg.editZoom, gp::kFill, false,
                                                  kDlgWinEditDy),
                   0));

    col->Child(SettingsWinCheck(cx, StrL("opt-showtoc"), Tr("Show the &bookmarks sidebar when available"),
                                gSettingsDlg.showToc, false, 0, kOptWinCheckGap));
    col->Child(SettingsWinCheck(cx, StrL("opt-remember"), Tr("&Remember these settings for each document"),
                                gSettingsDlg.rememberState, !gSettingsDlg.rememberOpened, 1, kOptWinPadY));

    auto section = [&](Str s) {
        return gp::Div(cx->a)->PadT(kOptWinSectionGap)->Shrink0()->Child(DlgWinLabel(cx, ToGpui(s), font, kOptWinPadY));
    };
    col->Child(section(Tr("Advanced")));
    col->Child(SettingsWinCheck(cx, StrL("opt-tabs"), Tr("Use &tabs"), gSettingsDlg.useTabs, false, 2, 0));
    col->Child(SettingsWinCheck(cx, StrL("opt-updates"), Tr("Automatically check for &updates"),
                                gSettingsDlg.checkUpdates, !HasPermission(Perm::InternetAccess), 3, kOptWinPadY));
    col->Child(SettingsWinCheck(cx, StrL("opt-opened"), Tr("Remember &opened files"), gSettingsDlg.rememberOpened,
                                false, 4, kOptWinPadY));

    if (gSettingsDlg.showInverseSearch) {
        col->Child(section(Tr("Set inverse search command line")));
        Str prompt = Tr("Enter the command line to invoke when you double-click on the PDF document:");
        col->Child(DlgWinLabel(cx, ToGpui(prompt), font, kOptWinPadY));
        col->Child(gSettingsDlg.ddInverse.BuildCombo(cx, StrL("opt-inverse-edit"), gSettingsDlg.editInverse, gp::kFill,
                                                     false, kDlgWinEditDy));
    }

    gp::Listener onOk = gp::ListenTo(gSettingsView, &SettingsView::OnOk);
    DlgSetDefault(cx, onOk, true);
    gp::El* buttons = DlgWinButtonRow(cx, kOptWinPadY);
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gSettingsView, &SettingsView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("OK"), onOk, true));
    col->Child(buttons);
    return DlgWinContent(cx, col);
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
    if (DlgWindowIsHost(cx)) {
        return SettingsWinBuild(cx);
    }

    gp::El* body = gp::Div(cx->a)
                       ->Id(GStrL("opt-body"))
                       ->FlexCol()
                       ->Gap(8)
                       ->MaxH(520)
                       ->ScrollY(gSettingsDlg.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gSettingsView, &SettingsView::OnScroll));
    body->Child(SectionLabel(cx, Tr("View")));
    body->Child(LabeledRow(cx, DlgAccelSelect(cx, Tr("Default &Layout:"), &gSettingsDlg.ddLayout),
                           gSettingsDlg.ddLayout.Build(cx, StrL("opt-layout"), gp::kFill)));

    // orig's editable combo box of zoom levels
    gp::El* zoomRow = gSettingsDlg.ddZoom.BuildCombo(cx, StrL("opt-zoom-edit"), gSettingsDlg.editZoom, gp::kFill);
    body->Child(LabeledRow(cx, DlgAccelInput(cx, Tr("Default &Zoom:"), gSettingsDlg.editZoom), zoomRow));

    body->Child(CheckRow(cx, StrL("opt-showtoc"), Tr("Show the &bookmarks sidebar when available"),
                         gSettingsDlg.showToc, false, 0));
    body->Child(CheckRow(cx, StrL("opt-remember"), Tr("&Remember these settings for each document"),
                         gSettingsDlg.rememberState, !gSettingsDlg.rememberOpened, 1));

    body->Child(SectionLabel(cx, Tr("Advanced")));
    body->Child(CheckRow(cx, StrL("opt-tabs"), Tr("Use &tabs"), gSettingsDlg.useTabs, false, 2));
    body->Child(CheckRow(cx, StrL("opt-updates"), Tr("Automatically check for &updates"), gSettingsDlg.checkUpdates,
                         !HasPermission(Perm::InternetAccess), 3));
    body->Child(CheckRow(cx, StrL("opt-opened"), Tr("Remember &opened files"), gSettingsDlg.rememberOpened, false, 4));

    if (gSettingsDlg.showInverseSearch) {
        body->Child(SectionLabel(cx, Tr("Set inverse search command line")));
        const gp::Theme& th = gp::ThemeNow(cx->app);
        Str prompt = Tr("Enter the command line to invoke when you double-click on the PDF document:");
        body->Child(gp::TextEl(cx->a, ToGpui(prompt))->Font(13)->Fg(th.mutedFg));
        body->Child(
            gSettingsDlg.ddInverse.BuildCombo(cx, StrL("opt-inverse-edit"), gSettingsDlg.editInverse, gp::kFill));
    }

    DlgSetDefault(cx, gp::ListenTo(gSettingsView, &SettingsView::OnOk), true);
    return DlgIntoEl(cx, gpc::Dialog::New(cx)
                             ->Open(true)
                             ->Title(ToGpui(Tr("Settings")))
                             ->Body(body)
                             ->W(560)
                             ->OkText(ToGpui(Tr("OK")))
                             ->CancelText(ToGpui(Tr("Cancel")))
                             ->ShowCancel(true)
                             ->OnOk(gp::ListenTo(gSettingsView, &SettingsView::OnOk))
                             ->OnCancel(gp::ListenTo(gSettingsView, &SettingsView::OnCancel))
                             ->OnClose(gp::ListenTo(gSettingsView, &SettingsView::OnCancel)));
}

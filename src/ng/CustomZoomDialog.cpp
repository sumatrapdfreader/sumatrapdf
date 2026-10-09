/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's CustomZoomDialog.cpp is a WS_POPUPWINDOW with an Edit and a
// VirtListBox. Here it is a gpui Dialog with the same rows: the
// "&Magnification:" label, the editable field, the list of zoom levels under
// it and Cancel / Zoom. Picking a row writes it into the field, typing into
// the field moves the list, and Up / Down step the list - orig's editable
// combo box behaviour.

#include "gui/GpuiBridge.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"

#include "SumatraLog.h"

static constexpr float kZoomRowDy = 22;
static constexpr float kZoomListMinDy = 120;
static constexpr float kZoomListMaxDy = 300;
// orig's window, at 96 dpi
static constexpr float kZoomWinDx = 240;
static constexpr float kZoomWinPadX = 8;
static constexpr float kZoomWinPadY = 4;

struct CustomZoomDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    bool forChm = false;
    float startZoom = 0;
    Vec<float> zoomLevels;
    int sel = -1;
    gpui::InputState* editZoom = nullptr;
    bool wantFocus = false;
    float scrollY = 0;
};

static CustomZoomDlg gCustomZoom;

struct CustomZoomView {
    static void OnOk(CustomZoomView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(CustomZoomView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(CustomZoomView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnRowClick(CustomZoomView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnScroll(CustomZoomView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<CustomZoomView> gCustomZoomView;

// orig's modal window, where the platform can have one (DlgWindowOpen);
// null: a dialog in the frame
static ToolWindow* gCustomZoomTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsCustomZoomDialogVisible() {
    return gCustomZoom.visible && !gCustomZoomTw;
}

void CloseCustomZoomDialog() {
    if (!gCustomZoom.visible) {
        return;
    }
    gCustomZoom.visible = false;
    MainWindow* win = gCustomZoom.win;
    if (gCustomZoomTw) {
        DlgWindowClose(&gCustomZoomTw);
    } else if (win && win->gpuiWin && gCustomZoom.editZoom) {
        gp::InputBlur(gCustomZoom.editZoom, win->gpuiWin->app, win->gpuiWin);
    }
    delete gCustomZoom.editZoom;
    gCustomZoom.editZoom = nullptr;
    VecReset(gCustomZoom.zoomLevels);
    AppShellInvalidate(win);
}

static TempStr EditTextTemp() {
    if (!gCustomZoom.editZoom) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(gCustomZoom.editZoom)));
}

// orig's CustomZoomWnd::SetEditFromSelection
static void SetEditFromSelection() {
    int idx = gCustomZoom.sel;
    if (!gCustomZoom.editZoom || idx < 0 || idx >= len(gCustomZoom.zoomLevels)) {
        return;
    }
    gp::InputSetValue(gCustomZoom.editZoom, ToGpui(ZoomLevelStrExact(gCustomZoom.zoomLevels[idx])));
}

// the tool window's rows are the shared dialog row; the in-frame card uses its own
static float ZoomListRowDy() {
    return gCustomZoomTw ? kDlgWinRowDy : kZoomRowDy;
}

// what orig's ListBox does when its selection changes
static void ScrollSelIntoView() {
    int n = len(gCustomZoom.zoomLevels);
    float rowDy = ZoomListRowDy();
    float viewDy = DialogListViewDy(n, rowDy, kZoomListMinDy, kZoomListMaxDy);
    gCustomZoom.scrollY = DialogScrollToRow(gCustomZoom.scrollY, gCustomZoom.sel, n, rowDy, viewDy);
}

// orig's SelectLevelFromEdit: typing a level's name puts the list on it
static void SelectLevelFromEdit() {
    TempStr text = EditTextTemp();
    int sel = -1;
    for (int i = 0; i < len(gCustomZoom.zoomLevels); i++) {
        if (str::EqI(text, ZoomLevelStrExact(gCustomZoom.zoomLevels[i]))) {
            sel = i;
            break;
        }
    }
    gCustomZoom.sel = sel;
    ScrollSelIntoView();
}

// orig's CustomZoomWnd::FillZoom
static void FillZoom() {
    CollectZoomLevels(gCustomZoom.zoomLevels, gCustomZoom.forChm);
    int sel = -1;
    for (int i = 0; i < len(gCustomZoom.zoomLevels); i++) {
        if (gCustomZoom.zoomLevels[i] == gCustomZoom.startZoom) {
            sel = i;
            break;
        }
    }
    gCustomZoom.sel = sel;
    gCustomZoom.scrollY = 0;
    ScrollSelIntoView();
    if (sel >= 0) {
        SetEditFromSelection();
    } else if (gCustomZoom.editZoom) {
        // a zoom that is none of them, e.g. one typed in here before
        gp::InputSetValue(gCustomZoom.editZoom, ToGpui(fmt("%.0f%%", gCustomZoom.startZoom)));
    }
}

// orig's CustomZoomWnd::SelectedZoom
static float SelectedZoom() {
    TempStr text = EditTextTemp();
    for (int i = 0; i < len(gCustomZoom.zoomLevels); i++) {
        if (str::EqI(text, ZoomLevelStrExact(gCustomZoom.zoomLevels[i]))) {
            return gCustomZoom.zoomLevels[i];
        }
    }
    if (len(text) == 0) {
        return gCustomZoom.startZoom;
    }
    float zoom = (float)atof(CStrTemp(text));
    if (zoom == 0) {
        return gCustomZoom.startZoom;
    }
    return limitValue(zoom, kZoomMin, kZoomMax);
}

// orig's MoveSelection: Up / Down move the list and the field follows; they
// stop at the ends rather than wrap
bool CustomZoomMoveSelection(int dir) {
    if (!gCustomZoom.visible) {
        return false;
    }
    int n = len(gCustomZoom.zoomLevels);
    if (n == 0) {
        return false;
    }
    int sel = gCustomZoom.sel;
    if (sel < 0) {
        sel = dir < 0 ? n - 1 : 0;
    } else if (dir < 0) {
        sel = std::max(sel - 1, 0);
    } else {
        sel = std::min(sel + 1, n - 1);
    }
    gCustomZoom.sel = sel;
    ScrollSelIntoView();
    SetEditFromSelection();
    AppShellInvalidate(gCustomZoom.win);
    return true;
}

void CustomZoomOk() {
    // Enter in the field reaches this twice: from the field and as the
    // default button's
    if (!gCustomZoom.visible) {
        return;
    }
    MainWindow* win = gCustomZoom.win;
    float zoom = SelectedZoom();
    CloseCustomZoomDialog();
    if (IsMainWindowValidAndNotClosing(win) && win->IsDocLoaded()) {
        logf("CustomZoomDialog: zoom %.2f\n", zoom);
        SmartZoom(win, zoom, nullptr, true);
    }
}

void CustomZoomView::OnOk(CustomZoomView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CustomZoomOk();
    gp::Notify(cx);
}

void CustomZoomView::OnCancel(CustomZoomView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseCustomZoomDialog();
    gp::Notify(cx);
}

void CustomZoomView::OnInput(CustomZoomView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind == gp::InputEventKind::PressEnter) {
        CustomZoomOk();
        gp::Notify(cx);
        return;
    }
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    SelectLevelFromEdit();
    gp::Notify(cx);
}

void CustomZoomView::OnRowClick(CustomZoomView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    gCustomZoom.sel = (int)idx;
    SetEditFromSelection();
    // orig's list is drawn, not a control: the edit keeps the focus
    if (gCustomZoom.editZoom) {
        gp::InputFocus(gCustomZoom.editZoom, cx->app, cx->win);
        gp::InputSelectAll(gCustomZoom.editZoom, cx->app, cx->win);
    }
    if (ev->clickCount >= 2) {
        CustomZoomOk();
    }
    gp::Notify(cx);
}

static Str CustomZoomTitle() {
    return Tr("Zoom");
}

// Up / Down move the list from anywhere in the dialog
static bool CustomZoomOnArrow(int dir, bool) {
    return CustomZoomMoveSelection(dir);
}

void ShowCustomZoomDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return;
    }
    CloseCustomZoomDialog();
    gCustomZoom.win = win;
    gCustomZoom.forChm = false;
    gCustomZoom.startZoom = 0;
    if (win->ctrl) {
        gCustomZoom.forChm = IsBrowserDocController(win->ctrl);
        gCustomZoom.startZoom = win->ctrl->GetZoomVirtual();
    }
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gCustomZoom.editZoom = s;
    FillZoom();
    gCustomZoom.visible = true;
    gCustomZoom.wantFocus = true;
    DlgWindowSpec spec;
    spec.name = "customzoom";
    spec.title = CustomZoomTitle;
    spec.build = CustomZoomDialogBuild;
    spec.close = CloseCustomZoomDialog;
    spec.onArrow = CustomZoomOnArrow;
    spec.clientDx = kZoomWinDx;
    gCustomZoomTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void CustomZoomView::OnScroll(CustomZoomView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gCustomZoom.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gCustomZoom.win);
}

// orig's window: a 240 wide client area, 4 / 8 around; the label with 4 under
// it, the edit, 4, every level as a row (nothing to scroll), then the buttons
// with 4 above and below
static gp::El* CustomZoomWinBuild(gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kZoomWinPadX)->PadY(kZoomWinPadY);
    col->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->H(kDlgWinLineDy + kZoomWinPadY)
                   ->PadB(kZoomWinPadY)
                   ->Shrink0()
                   ->Child(DlgAccelText(cx, DlgAccelInput(cx, Tr("&Magnification:"), gCustomZoom.editZoom))
                               ->Font(font)
                               ->Fg(th.foreground)));
    col->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->W(gp::kFill)
                   ->H(kDlgWinEditDy)
                   ->Shrink0()
                   ->Child(gpc::Input::New(cx, GStrL("customzoom-edit"), gCustomZoom.editZoom)
                               ->WithSize(gp::UiSize::Small)
                               ->W(gp::kFill)
                               ->IntoEl()
                               ->H(kDlgWinEditDy)));

    // more levels than the screen has room for: the list scrolls instead of
    // growing the dialog past the monitor (orig's tall-list check)
    int n = len(gCustomZoom.zoomLevels);
    float viewDy = DialogListViewDy(n, kDlgWinRowDy, kZoomListMinDy, kZoomListMaxDy);
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("customzoom-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(viewDy)
                       ->PadT(kZoomWinPadY)
                       ->Shrink0()
                       ->ScrollY(gCustomZoom.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnScroll));
    for (int i = 0; i < n; i++) {
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kDlgWinRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(4)
                          ->PathClick(GpuiDup(cx->a, fmt("customzoom-row-%d", i)))
                          ->OnClick(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnRowClick, (intptr_t)i));
        if (i == gCustomZoom.sel) {
            row->Bg(DlgWinListSelBg(false));
        }
        Str s = ZoomLevelStrExact(gCustomZoom.zoomLevels[i]);
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(font)->Fg(th.foreground));
        list->Child(row);
    }
    col->Child(list);

    gp::Listener onOk = gp::ListenTo(gCustomZoomView, &CustomZoomView::OnOk);
    DlgSetDefault(cx, onOk);
    gp::El* buttons = DlgWinButtonRow(cx, kZoomWinPadY);
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gCustomZoomView, &CustomZoomView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("Zoom"), onOk, true));
    col->Child(buttons);
    return DlgWinContent(cx, col);
}

gp::El* CustomZoomDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gCustomZoom.visible || gCustomZoom.win != win) {
        return nullptr;
    }
    if (gCustomZoomTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gCustomZoomView.IsValid()) {
        gCustomZoomView = gp::EntityNewState<CustomZoomView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gCustomZoom.editZoom->onChange = gp::ListenTo(gCustomZoomView, &CustomZoomView::OnInput);
    if (DlgWindowIsHost(cx)) {
        gp::El* content = CustomZoomWinBuild(cx);
        if (gCustomZoom.wantFocus) {
            gp::InputFocus(gCustomZoom.editZoom, cx->app, cx->win);
            gp::InputSelectAll(gCustomZoom.editZoom, cx->app, cx->win);
            gCustomZoom.wantFocus = cx->win->input != gCustomZoom.editZoom;
        }
        return content;
    }

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(6);
    body->Child(
        DlgAccelText(cx, DlgAccelInput(cx, Tr("&Magnification:"), gCustomZoom.editZoom))->Font(13)->Fg(th.foreground));
    body->Child(gpc::Input::New(cx, GStrL("customzoom-edit"), gCustomZoom.editZoom)
                    ->WithSize(gp::UiSize::Small)
                    ->W(gp::kFill)
                    ->IntoEl());

    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("customzoom-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->MinH(kZoomListMinDy)
                       ->MaxH(kZoomListMaxDy)
                       ->ScrollY(gCustomZoom.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnScroll))
                       ->Border(1, th.border);
    for (int i = 0; i < len(gCustomZoom.zoomLevels); i++) {
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kZoomRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(6)
                          ->PathClick(GpuiDup(cx->a, fmt("customzoom-row-%d", i)))
                          ->OnClick(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnRowClick, (intptr_t)i));
        if (i == gCustomZoom.sel) {
            row->Bg(th.selection);
        }
        Str s = ZoomLevelStrExact(gCustomZoom.zoomLevels[i]);
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(13)->Fg(th.foreground));
        list->Child(row);
    }
    body->Child(list);

    DlgSetDefault(cx, gp::ListenTo(gCustomZoomView, &CustomZoomView::OnOk));
    gp::El* dlg = DlgIntoEl(cx, gpc::Dialog::New(cx)
                                    ->Open(true)
                                    ->Title(ToGpui(Tr("Zoom")))
                                    ->Body(body)
                                    ->W(300)
                                    ->OkText(ToGpui(Tr("Zoom")))
                                    ->CancelText(ToGpui(Tr("Cancel")))
                                    ->ShowCancel(true)
                                    ->OnOk(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnOk))
                                    ->OnCancel(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnCancel))
                                    ->OnClose(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnCancel)));

    if (gCustomZoom.wantFocus) {
        gp::InputFocus(gCustomZoom.editZoom, cx->app, cx->win);
        gp::InputSelectAll(gCustomZoom.editZoom, cx->app, cx->win);
        gCustomZoom.wantFocus = cx->win->input != gCustomZoom.editZoom;
    }
    return dlg;
}

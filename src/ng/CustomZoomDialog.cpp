/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's CustomZoomDialog.cpp is a WS_POPUPWINDOW with an Edit and a
// VirtListBox. Here it is the same column of gpui elements: the
// "&Magnification:" line, the list of zoom levels under it and Cancel / Zoom. Picking a row writes it into the field,
// typing into the field moves the list, and Up / Down step the list - orig's editable combo box behaviour.

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
#include "shared/PdfToolsCommon.h"

#include "SumatraLog.h"

static constexpr float kZoomRowDy = 24;
static constexpr float kZoomListMinDy = 120;
// every level of the default set, as orig shows them
static constexpr float kZoomListMaxDy = 20 * kZoomRowDy;
static constexpr float kZoomDlgDx = 280;
// the rows' text lines up with the field's
static constexpr float kZoomListInsetDx = 8;
static constexpr float kZoomListRadius = 4;
static constexpr int kZoomListSelShift = 28;

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

// what orig's ListBox does when its selection changes
static void ScrollSelIntoView() {
    int n = len(gCustomZoom.zoomLevels);
    float viewDy = DialogListViewDy(n, kZoomRowDy, kZoomListMinDy, kZoomListMaxDy, 0);
    gCustomZoom.scrollY = DialogScrollToRow(gCustomZoom.scrollY, gCustomZoom.sel, n, kZoomRowDy, viewDy);
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
    spec.clientDx = kZoomDlgDx;
    gCustomZoomTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void CustomZoomView::OnScroll(CustomZoomView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gCustomZoom.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gCustomZoom.win);
}

// orig's layout: the zoom, the levels on a rounded selection, Cancel / Zoom
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
    gCustomZoom.editZoom->onChange = gp::ListenTo(gCustomZoomView, &CustomZoomView::OnInput);
    gp::Listener onOk = gp::ListenTo(gCustomZoomView, &CustomZoomView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gCustomZoomView, &CustomZoomView::OnCancel);
    DlgSetDefault(cx, onOk);
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgLine(cx, Tr("&Magnification:"), 0, StrL("customzoom-edit"), gCustomZoom.editZoom, DlgField::Plain));
    col->Child(DlgHairline(cx));

    // more levels than the screen has room for: the list scrolls instead of
    // growing the dialog past the monitor (orig's tall-list check)
    int n = len(gCustomZoom.zoomLevels);
    float viewDy = DialogListViewDy(n, kZoomRowDy, kZoomListMinDy, kZoomListMaxDy, 0);
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("customzoom-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(viewDy)
                       ->Shrink0()
                       ->ScrollY(gCustomZoom.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnScroll));
    for (int i = 0; i < n; i++) {
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kZoomRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(kZoomListInsetDx)
                          ->Radius(kZoomListRadius)
                          ->PathClick(GpuiDup(cx->a, fmt("customzoom-row-%d", i)))
                          ->OnClick(gp::ListenTo(gCustomZoomView, &CustomZoomView::OnRowClick, (intptr_t)i));
        if (i == gCustomZoom.sel) {
            row->Bg(ToGpui(AccentColor(colBg, kZoomListSelShift)));
        }
        Str s = ZoomLevelStrExact(gCustomZoom.zoomLevels[i]);
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(kToolDlgTextFontPx)->Fg(ToGpui(colTxt)));
        list->Child(row);
    }
    col->Child(gp::Div(cx->a)
                   ->W(gp::kFill)
                   ->Shrink0()
                   ->PadX(kToolDlgPadDx - kZoomListInsetDx)
                   ->PadY(kToolDlgFilesPadDy)
                   ->Child(list));
    col->Child(DlgFooter(cx, Tr("Zoom"), onOk, onCancel));
    gp::El* content = DlgToolContent(cx, col, CustomZoomTitle(), kZoomDlgDx, onCancel);

    if (gCustomZoom.wantFocus) {
        gp::InputFocus(gCustomZoom.editZoom, cx->app, cx->win);
        gp::InputSelectAll(gCustomZoom.editZoom, cx->app, cx->win);
        gCustomZoom.wantFocus = cx->win->input != gCustomZoom.editZoom;
    }
    return content;
}

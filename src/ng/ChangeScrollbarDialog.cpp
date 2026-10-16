/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's ChangeScrollbarDialog.cpp is a WS_POPUPWINDOW with a VirtListBox.
// Here it is a gpui Dialog with the same four modes in the same order and
// Cancel / OK.

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
#include "AppHelpersCommon.h"

#include "SumatraLog.h"

struct ChangeScrollbarDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    int sel = -1;
};

static ChangeScrollbarDlg gChangeScrollbar;

// orig's window at 96 dpi: a 260 wide client area, 4 / 8 around; the four
// rows of the list, which has the focus, and the buttons with 4 above and below
constexpr float kSbDlgDx = 280;

struct ChangeScrollbarView {
    static void OnOk(ChangeScrollbarView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(ChangeScrollbarView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRowClick(ChangeScrollbarView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
};

static gp::Entity<ChangeScrollbarView> gChangeScrollbarView;

// orig's modal window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gChangeScrollbarTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsChangeScrollbarDialogVisible() {
    return gChangeScrollbar.visible && !gChangeScrollbarTw;
}

void CloseChangeScrollbarDialog() {
    if (!gChangeScrollbar.visible) {
        return;
    }
    gChangeScrollbar.visible = false;
    DlgWindowClose(&gChangeScrollbarTw);
    AppShellInvalidate(gChangeScrollbar.win);
}

static Str ChangeScrollbarDlgTitle() {
    return Tr("Change Scrollbar");
}

// Up / Down move the list from anywhere in the dialog
static bool ChangeScrollbarDlgOnArrow(int dir, bool) {
    return ChangeScrollbarMoveSelection(dir);
}

void ShowChangeScrollbarDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences) || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    gChangeScrollbar.win = win;
    int curr = ScrollbarModeFromPrefs();
    gChangeScrollbar.sel = (curr >= 0 && curr <= kScrollbarHidden) ? curr : -1;
    gChangeScrollbar.visible = true;
    DlgWindowSpec spec;
    spec.name = "changescrollbar";
    spec.title = ChangeScrollbarDlgTitle;
    spec.modal = true;
    spec.build = ChangeScrollbarDialogBuild;
    spec.close = CloseChangeScrollbarDialog;
    spec.onArrow = ChangeScrollbarDlgOnArrow;
    spec.clientDx = kSbDlgDx;
    gChangeScrollbarTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

// orig's ChangeScrollbarWnd::OnOk
void ChangeScrollbarOk() {
    int idx = gChangeScrollbar.sel;
    CloseChangeScrollbarDialog();
    if (idx < kScrollbarSmart || idx > kScrollbarHidden) {
        return;
    }
    Str val = SeqStrByIndex(gScrollbarModeNames, idx);
    logf("ChangeScrollbarDialog: scrollbars = '%s'\n", val);
    str::ReplaceWithCopy(&gSettings->scrollbars, val);
    UpdateFixedPageScrollbarsVisibility();
    ScheduleSaveSettings();
}

bool ChangeScrollbarMoveSelection(int dir) {
    if (!gChangeScrollbar.visible) {
        return false;
    }
    // windows is not offered; the choices are smart, overlay, hidden
    int n = kScrollbarHidden - kScrollbarSmart + 1;
    int sel = gChangeScrollbar.sel;
    int pos = sel - kScrollbarSmart;
    if (pos < 0 || pos >= n) {
        pos = 0;
    }
    int step = dir < 0 ? n - 1 : 1;
    gChangeScrollbar.sel = kScrollbarSmart + (pos + step) % n;
    AppShellInvalidate(gChangeScrollbar.win);
    return true;
}

void ChangeScrollbarView::OnOk(ChangeScrollbarView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ChangeScrollbarOk();
    gp::Notify(cx);
}

void ChangeScrollbarView::OnCancel(ChangeScrollbarView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseChangeScrollbarDialog();
    gp::Notify(cx);
}

void ChangeScrollbarView::OnRowClick(ChangeScrollbarView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    gChangeScrollbar.sel = (int)idx;
    if (ev->clickCount >= 2) {
        ChangeScrollbarOk();
    }
    gp::Notify(cx);
}

// orig's layout: the modes, Cancel / OK
gp::El* ChangeScrollbarDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gChangeScrollbar.visible || gChangeScrollbar.win != win) {
        return nullptr;
    }
    if (gChangeScrollbarTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gChangeScrollbarView.IsValid()) {
        gChangeScrollbarView = gp::EntityNewState<ChangeScrollbarView>(cx->app);
    }
    gp::Listener onOk = gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnCancel);
    DlgSetDefault(cx, onOk);

    gp::El* list = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0();
    for (int i = kScrollbarSmart; i <= kScrollbarHidden; i++) {
        list->Child(DlgListRow(cx, fmt("scrollbar-row-%d", i), ScrollbarModeDisplayName(i), i == gChangeScrollbar.sel,
                               gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnRowClick, (intptr_t)i)));
    }
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgListBox(cx, list));
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel));
    return DlgToolContent(cx, col, ChangeScrollbarDlgTitle(), kSbDlgDx, onCancel);
}

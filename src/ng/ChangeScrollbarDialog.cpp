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

#include "SumatraLog.h"

struct ChangeScrollbarDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    int sel = -1;
};

static ChangeScrollbarDlg gChangeScrollbar;

// orig's window at 96 dpi: a 260 wide client area, 4 / 8 around; the four
// rows of the list, which has the focus, and the buttons with 4 above and below
constexpr float kSbWinDx = 260;
constexpr float kSbWinPadX = 8;
constexpr float kSbWinPadY = 4;

struct ChangeScrollbarView {
    static void OnOk(ChangeScrollbarView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(ChangeScrollbarView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRowClick(ChangeScrollbarView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
};

static gp::Entity<ChangeScrollbarView> gChangeScrollbarView;

// orig's ScrollbarModeDisplayName
static Str ScrollbarModeDisplayName(int idx) {
    if (idx == kScrollbarSmart) {
        return Tr("Smart Overlay");
    }
    if (idx == kScrollbarOverlay) {
        return Tr("Overlay");
    }
    if (idx == kScrollbarHidden) {
        return Tr("Hidden");
    }
    return Tr("Windows");
}

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
    spec.clientDx = kSbWinDx;
    gChangeScrollbarTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

// orig's ChangeScrollbarWnd::OnOk
void ChangeScrollbarOk() {
    int idx = gChangeScrollbar.sel;
    CloseChangeScrollbarDialog();
    if (idx < 0) {
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
    int n = kScrollbarHidden + 1;
    int sel = gChangeScrollbar.sel;
    sel = (sel < 0) ? (dir < 0 ? n - 1 : 0) : ((sel + dir + n) % n);
    gChangeScrollbar.sel = sel;
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

static gp::El* ChangeScrollbarWinBuild(gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kSbWinPadX)->PadY(kSbWinPadY);
    gp::El* list = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0();
    for (int i = 0; i <= kScrollbarHidden; i++) {
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kDlgWinRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(4)
                          ->PathClick(GpuiDup(cx->a, fmt("scrollbar-row-%d", i)))
                          ->OnClick(gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnRowClick, (intptr_t)i));
        if (i == gChangeScrollbar.sel) {
            row->Bg(DlgWinListSelBg(true));
        }
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, ScrollbarModeDisplayName(i)))->Font(font)->Fg(th.foreground));
        list->Child(row);
    }
    // orig's list has the focus: its dotted focus rectangle
    list->Child(gp::Div(cx->a)
                    ->Absolute()
                    ->Left(0)
                    ->Top(0)
                    ->W(gp::kFill)
                    ->H((kScrollbarHidden + 1) * kDlgWinRowDy)
                    ->Border(1, th.foreground)
                    ->Dashed());
    col->Child(list);

    gp::Listener onOk = gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnOk);
    DlgSetDefault(cx, onOk);
    gp::El* buttons = DlgWinButtonRow(cx, kSbWinPadY);
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("OK"), onOk, true));
    col->Child(buttons);
    return DlgWinContent(cx, col);
}

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
    const gp::Theme& th = gp::ThemeNow(cx->app);
    if (DlgWindowIsHost(cx)) {
        return ChangeScrollbarWinBuild(cx);
    }
    gp::El* list = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Border(1, th.border);
    for (int i = 0; i <= kScrollbarHidden; i++) {
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(24)
                          ->ItemsCenter()
                          ->PadX(6)
                          ->PathClick(GpuiDup(cx->a, fmt("scrollbar-row-%d", i)))
                          ->OnClick(gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnRowClick, (intptr_t)i));
        if (i == gChangeScrollbar.sel) {
            row->Bg(th.selection);
        }
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, ScrollbarModeDisplayName(i)))->Font(13)->Fg(th.foreground));
        list->Child(row);
    }

    DlgSetDefault(cx, gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnOk));
    return DlgIntoEl(cx, gpc::Dialog::New(cx)
                             ->Open(true)
                             ->Title(ToGpui(Tr("Change Scrollbar")))
                             ->Body(list)
                             ->W(320)
                             ->OkText(ToGpui(Tr("OK")))
                             ->CancelText(ToGpui(Tr("Cancel")))
                             ->ShowCancel(true)
                             ->OnOk(gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnOk))
                             ->OnCancel(gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnCancel))
                             ->OnClose(gp::ListenTo(gChangeScrollbarView, &ChangeScrollbarView::OnCancel)));
}

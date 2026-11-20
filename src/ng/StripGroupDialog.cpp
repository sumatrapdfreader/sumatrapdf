/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's StripGroupDialog.cpp is a modal tool window with a Name line and
// Cancel / Rename. Here it is the same column of gpui elements, in a window of
// its own where the platform has one, else in a card over the frame.

#include "gui/GpuiBridge.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/PlatformFont.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "StripGroups.h"

struct RenameStripGroupDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    int groupId = 0;
    gpui::InputState* editName = nullptr;
    bool wantFocus = false;
    // orig's modal window, where the platform can have one; null: a dialog in
    // the frame
    ToolWindow* tw = nullptr;
};

constexpr float kRenameDlgDx = 340;

static RenameStripGroupDlg gRename;

struct RenameStripGroupView {
    static void OnOk(RenameStripGroupView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(RenameStripGroupView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(RenameStripGroupView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<RenameStripGroupView> gRenameView;

// the dialog in the frame; a window of its own is not the frame's business
bool IsRenameStripGroupDialogVisible() {
    return gRename.visible && !gRename.tw;
}

void CloseRenameStripGroupDialog() {
    if (!gRename.visible) {
        return;
    }
    gRename.visible = false;
    MainWindow* win = gRename.win;
    if (gRename.tw) {
        DlgWindowClose(&gRename.tw);
    } else if (win && win->gpuiWin && gRename.editName) {
        InputBlurWhereFocused(gRename.editName, win->gpuiWin);
    }
    delete gRename.editName;
    gRename.editName = nullptr;
    AppShellInvalidate(win);
}

static void RenameOk() {
    MainWindow* win = gRename.win;
    if (IsMainWindowValidAndNotClosing(win) && gRename.editName) {
        StripGroupSetName(win, gRename.groupId, InputTextTemp(gRename.editName));
    }
    CloseRenameStripGroupDialog();
}

void RenameStripGroupView::OnOk(RenameStripGroupView*, gp::Ctx* cx, const gp::ClickEvent*) {
    RenameOk();
    gp::Notify(cx);
}

void RenameStripGroupView::OnCancel(RenameStripGroupView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseRenameStripGroupDialog();
    gp::Notify(cx);
}

void RenameStripGroupView::OnInput(RenameStripGroupView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    RenameOk();
    gp::Notify(cx);
}

static Str RenameTitle() {
    return Tr("Rename Group");
}

void ShowRenameStripGroupDialog(MainWindow* win, int groupId) {
    StripGroup* g = StripGroupFind(win, groupId);
    if (!IsMainWindowValidAndNotClosing(win) || !g) {
        return;
    }
    if (gRename.visible) {
        CloseRenameStripGroupDialog();
    }
    gRename.win = win;
    gRename.groupId = groupId;
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gRename.editName = new gp::InputState();
    gRename.editName->focus = gp::FocusHandleNew(app);
    gp::InputSetValue(gRename.editName, ToGpui(g->name));
    gRename.visible = true;
    gRename.wantFocus = true;
    DlgWindowSpec spec;
    spec.name = "renamestripgroup";
    spec.title = RenameTitle;
    spec.build = RenameStripGroupDialogBuild;
    spec.close = CloseRenameStripGroupDialog;
    spec.clientDx = kRenameDlgDx;
    gRename.tw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

gp::El* RenameStripGroupDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gRename.visible || gRename.win != win) {
        return nullptr;
    }
    if (gRename.tw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gRenameView.IsValid()) {
        gRenameView = gp::EntityNewState<RenameStripGroupView>(cx->app);
    }
    gp::Listener onOk = gp::ListenTo(gRenameView, &RenameStripGroupView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gRenameView, &RenameStripGroupView::OnCancel);
    DlgSetDefault(cx, onOk);

    gRename.editName->onChange = gp::ListenTo(gRenameView, &RenameStripGroupView::OnInput);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgLine(cx, Tr("&Name:"), 0, StrL("strip-group-name"), gRename.editName, DlgField::Plain));
    col->Child(DlgFooter(cx, Tr("Rename"), onOk, onCancel));
    gp::El* content = DlgToolContent(cx, col, RenameTitle(), kRenameDlgDx, onCancel);

    if (gRename.wantFocus) {
        gp::InputFocus(gRename.editName, cx->app, cx->win);
        gp::InputSelectAll(gRename.editName, cx->app, cx->win);
        gRename.wantFocus = cx->win->input != gRename.editName;
    }
    return content;
}

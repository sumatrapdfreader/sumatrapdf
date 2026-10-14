/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's InverseSearchDialog.cpp is a WS_POPUPWINDOW with an editable
// DropDown. gpui has no editable drop-down, so the command line is an Input and
// the detected editors are a Select next to it that writes into the Input. Same
// rows otherwise: the prompt, the command line, and Help / Cancel / OK.

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
#include "AppTools.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

#include "SumatraLog.h"

struct InverseSearchDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    gpui::InputState* editCmd = nullptr;
    DialogSelect ddCommands;
    bool wantFocus = false;
};

static InverseSearchDlg gInverseSearch;

// orig's window at 96 dpi: a 520 wide client area, 4 / 8 around; the prompt
// with 4 under it, the editable drop-down, then Help at the left and Cancel /
// OK at the right with 4 above and below
constexpr float kInvDlgDx = 520;
constexpr float kInvComboPadTop = 10;
constexpr float kInvComboPadBottom = 16;

struct InverseSearchView {
    static void OnOk(InverseSearchView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(InverseSearchView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnHelp(InverseSearchView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(InverseSearchView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<InverseSearchView> gInverseSearchView;

// orig's modal window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gInverseSearchTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsInverseSearchDialogVisible() {
    return gInverseSearch.visible && !gInverseSearchTw;
}

void CloseInverseSearchDialog() {
    if (!gInverseSearch.visible) {
        return;
    }
    gInverseSearch.visible = false;
    DlgWindowClose(&gInverseSearchTw);
    MainWindow* win = gInverseSearch.win;
    if (win && win->gpuiWin && gInverseSearch.editCmd) {
        gp::InputBlur(gInverseSearch.editCmd, win->gpuiWin->app, win->gpuiWin);
    }
    delete gInverseSearch.editCmd;
    gInverseSearch.editCmd = nullptr;
    gInverseSearch.ddCommands.Free();
    AppShellInvalidate(win);
}

// orig's InverseSearchWnd::FillCommands
static void FillCommands() {
    StrVec items;
    Str cmdLine = gSettings ? gSettings->inverseSearchCmdLine : Str{};
    CollectInverseSearchCommands(items, cmdLine);
    if (len(cmdLine) == 0 && len(items) > 0) {
        cmdLine = items[0];
    }
    int idx = items.Find(cmdLine);
    gInverseSearch.ddCommands.SetItems(items, idx);
    if (gInverseSearch.editCmd) {
        gp::InputSetValue(gInverseSearch.editCmd, ToGpui(cmdLine));
    }
}

static Str InverseSearchDlgTitle() {
    return Tr("Set inverse search command line");
}

void ShowInverseSearchDialog(MainWindow* win) {
    if (!CanAccessDisk() || !HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    CloseInverseSearchDialog();
    gInverseSearch.win = win;
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gInverseSearch.editCmd = s;
    gInverseSearch.ddCommands.Init(win->gpuiWin ? win->gpuiWin->app : nullptr);
    FillCommands();
    gInverseSearch.visible = true;
    DlgWindowSpec spec;
    spec.name = "inversesearch";
    spec.title = InverseSearchDlgTitle;
    spec.modal = true;
    spec.build = InverseSearchDialogBuild;
    spec.close = CloseInverseSearchDialog;
    spec.clientDx = kInvDlgDx;
    gInverseSearchTw = DlgWindowOpen(spec, win);
    gInverseSearch.wantFocus = true;
    AppShellInvalidate(win);
}

// orig's InverseSearchWnd::OnOk
static void InverseSearchOk() {
    TempStr cmd;
    if (gInverseSearch.editCmd) {
        cmd = str::DupTemp(FromGpui(gp::InputValue(gInverseSearch.editCmd)));
    }
    logf("InverseSearchDialog: InverseSearchCmdLine = '%s'\n", cmd);
    str::ReplaceWithCopy(&gSettings->inverseSearchCmdLine, cmd);
    gSettings->enableTeXEnhancements = true;
    ScheduleSaveSettings();
    CloseInverseSearchDialog();
}

void InverseSearchView::OnOk(InverseSearchView*, gp::Ctx* cx, const gp::ClickEvent*) {
    InverseSearchOk();
    gp::Notify(cx);
}

void InverseSearchView::OnCancel(InverseSearchView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseInverseSearchDialog();
    gp::Notify(cx);
}

void InverseSearchView::OnHelp(InverseSearchView*, gp::Ctx* cx, const gp::ClickEvent*) {
    LaunchDocumentation(StrL("LaTeX-integration"));
    gp::Notify(cx);
}

void InverseSearchView::OnInput(InverseSearchView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    InverseSearchOk();
    gp::Notify(cx);
}

// orig's layout: the prompt, the command line, Help, Cancel / OK
gp::El* InverseSearchDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gInverseSearch.visible || gInverseSearch.win != win) {
        return nullptr;
    }
    if (gInverseSearchTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gInverseSearchView.IsValid()) {
        gInverseSearchView = gp::EntityNewState<InverseSearchView>(cx->app);
    }
    gInverseSearch.editCmd->onChange = gp::ListenTo(gInverseSearchView, &InverseSearchView::OnInput);
    gp::Listener onOk = gp::ListenTo(gInverseSearchView, &InverseSearchView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gInverseSearchView, &InverseSearchView::OnCancel);
    DlgSetDefault(cx, onOk, true);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgCaption(cx, Tr("Enter the command line to invoke when you double-click on the PDF document:")));
    col->Child(gp::Div(cx->a)
                   ->W(gp::kFill)
                   ->Shrink0()
                   ->PadX(kToolDlgPadDx)
                   ->PadT(kInvComboPadTop)
                   ->PadB(kInvComboPadBottom)
                   ->Child(gInverseSearch.ddCommands.BuildCombo(cx, StrL("inverse-cmd"), gInverseSearch.editCmd,
                                                                gp::kFill, false, kDlgWinEditDy, kToolDlgTextFontPx)));
    gp::El* help = DlgGhostButton(cx, GStrL("inverse-help"), Tr("Help"),
                                  gp::ListenTo(gInverseSearchView, &InverseSearchView::OnHelp));
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel, help));
    gp::El* content = DlgToolContent(cx, col, InverseSearchDlgTitle(), kInvDlgDx, onCancel);

    if (gInverseSearch.wantFocus) {
        gp::InputFocus(gInverseSearch.editCmd, cx->app, cx->win);
        gp::InputSelectAll(gInverseSearch.editCmd, cx->app, cx->win);
        gInverseSearch.wantFocus = cx->win->input != gInverseSearch.editCmd;
    }
    return content;
}

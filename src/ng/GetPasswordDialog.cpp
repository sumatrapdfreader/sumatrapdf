/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's GetPasswordDialog.cpp is a modal WS_POPUPWINDOW that pumps its own
// message loop, because PasswordUI::GetPassword is called from the engine load
// and has to wait for the typed password. A gpui frame cannot block, so the
// dialog is asynchronous: it hands the password to `onDone` and the caller
// (LoadDocument) re-runs the load with it. Same column as orig: the file name,
// the password, "Show password", "Remember the password for this document"
// and Cancel / OK.

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

struct GetPasswordDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    Str fileName;
    bool canRemember = false;
    bool remember = false;
    gpui::InputState* editPwd = nullptr;
    bool wantFocus = false;
    // shown but not drawn yet: nobody can have answered it
    bool notDrawn = false;
    Func1<PasswordDialogResult*> onDone;
};

static GetPasswordDlg gGetPassword;

constexpr float kPwdDlgDx = 400;
// orig keeps the "show password" choice for the process, not per document
static bool gShowPassword = false;

struct GetPasswordView {
    static void OnOk(GetPasswordView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(GetPasswordView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(GetPasswordView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnShowPassword(GetPasswordView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRemember(GetPasswordView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<GetPasswordView> gGetPasswordView;

// orig's modal window, where the platform can have one (DlgWindowOpen);
// null: a dialog in the frame
static ToolWindow* gGetPasswordTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsGetPasswordDialogVisible() {
    return gGetPassword.visible && !gGetPasswordTw;
}

static Str GetPasswordDlgTitle() {
    return Tr("Enter password");
}

// orig's GetPasswordWnd::Finish: the caller is owed exactly one answer
static void FinishGetPassword(bool accepted) {
    if (!gGetPassword.visible) {
        return;
    }
    // the Enter that answered the prompt before this one is still being
    // dispatched when a wrong password brings up the next: in the frame it
    // reached the new prompt's default button and gave up with no password
    if (accepted && gGetPassword.notDrawn) {
        return;
    }
    gGetPassword.visible = false;
    DlgWindowClose(&gGetPasswordTw);
    MainWindow* win = gGetPassword.win;
    PasswordDialogResult res;
    res.accepted = accepted;
    res.rememberPassword = gGetPassword.remember;
    res.showPassword = gShowPassword;
    TempStr pwd;
    if (accepted && gGetPassword.editPwd) {
        pwd = str::DupTemp(FromGpui(gp::InputValue(gGetPassword.editPwd)));
        res.password = pwd;
    }
    if (win && win->gpuiWin && gGetPassword.editPwd) {
        gp::InputBlur(gGetPassword.editPwd, win->gpuiWin->app, win->gpuiWin);
    }
    Func1<PasswordDialogResult*> onDone = gGetPassword.onDone;
    gGetPassword.onDone = {};
    delete gGetPassword.editPwd;
    gGetPassword.editPwd = nullptr;
    str::Free(gGetPassword.fileName);
    gGetPassword.fileName = {};
    logf("GetPasswordDialog: accepted %d, remember %d\n", (int)accepted, (int)res.rememberPassword);
    onDone.Call(&res);
    AppShellInvalidate(win);
}

void CloseGetPasswordDialog() {
    FinishGetPassword(false);
}

void ShowGetPasswordDialog(MainWindow* win, Str fileName, bool canRemember, Func1<PasswordDialogResult*> onDone) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        PasswordDialogResult res;
        onDone.Call(&res);
        return;
    }
    CloseGetPasswordDialog();
    gGetPassword.win = win;
    gGetPassword.fileName = str::Dup(fileName);
    gGetPassword.canRemember = canRemember;
    gGetPassword.remember = false;
    gGetPassword.onDone = onDone;
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gGetPassword.editPwd = s;
    gGetPassword.visible = true;
    gGetPassword.wantFocus = true;
    gGetPassword.notDrawn = true;
    DlgWindowSpec spec;
    spec.name = "password";
    spec.title = GetPasswordDlgTitle;
    spec.build = GetPasswordDialogBuild;
    spec.close = CloseGetPasswordDialog;
    spec.clientDx = kPwdDlgDx;
    gGetPasswordTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void GetPasswordView::OnOk(GetPasswordView*, gp::Ctx* cx, const gp::ClickEvent*) {
    FinishGetPassword(true);
    gp::Notify(cx);
}

void GetPasswordView::OnCancel(GetPasswordView*, gp::Ctx* cx, const gp::ClickEvent*) {
    FinishGetPassword(false);
    gp::Notify(cx);
}

void GetPasswordView::OnInput(GetPasswordView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    FinishGetPassword(true);
    gp::Notify(cx);
}

void GetPasswordView::OnShowPassword(GetPasswordView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gShowPassword = !gShowPassword;
    gp::Notify(cx);
    AppShellInvalidate(gGetPassword.win);
}

void GetPasswordView::OnRemember(GetPasswordView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gGetPassword.remember = !gGetPassword.remember;
    gp::Notify(cx);
    AppShellInvalidate(gGetPassword.win);
}

constexpr float kPwdOptionsGap = 4;

// orig's layout: the prompt, the password, the options, Cancel / OK
gp::El* GetPasswordDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gGetPassword.visible || gGetPassword.win != win) {
        return nullptr;
    }
    if (gGetPasswordTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gGetPasswordView.IsValid()) {
        gGetPasswordView = gp::EntityNewState<GetPasswordView>(cx->app);
    }
    gGetPassword.notDrawn = false;
    gGetPassword.editPwd->onChange = gp::ListenTo(gGetPasswordView, &GetPasswordView::OnInput);
    gp::Listener onOk = gp::ListenTo(gGetPasswordView, &GetPasswordView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gGetPasswordView, &GetPasswordView::OnCancel);
    DlgSetDefault(cx, onOk);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgCaption(cx, fmt(Tr("Enter password for %s").s, gGetPassword.fileName)));
    DlgField kind = gShowPassword ? DlgField::Underlined : DlgField::Password;
    col->Child(DlgLine(cx, Tr("&Password:"), 0, StrL("password-edit"), gGetPassword.editPwd, kind));
    col->Child(DlgHairline(cx));

    // the options, a row each
    gp::El* options = gp::Div(cx->a)
                          ->FlexCol()
                          ->W(gp::kFill)
                          ->Shrink0()
                          ->PadX(kToolDlgPadDx)
                          ->PadY(kToolDlgFilesPadDy)
                          ->Gap(kPwdOptionsGap);
    auto option = [&](gp::El* box) {
        return gp::Div(cx->a)
            ->FlexRow()
            ->ItemsCenter()
            ->H(kToolDlgRowDy)
            ->Shrink0()
            ->Child(box->Font(kToolDlgTextFontPx));
    };
    options->Child(
        option(DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("password-show"))->Checked(gShowPassword),
                          Tr("&Show password"), gp::ListenTo(gGetPasswordView, &GetPasswordView::OnShowPassword))));
    if (gGetPassword.canRemember) {
        options->Child(
            option(DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("password-remember"))->Checked(gGetPassword.remember),
                              Tr("&Remember the password for this document"),
                              gp::ListenTo(gGetPasswordView, &GetPasswordView::OnRemember))));
    }
    col->Child(options);
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel));
    gp::El* content = DlgToolContent(cx, col, GetPasswordDlgTitle(), kPwdDlgDx, onCancel);

    if (gGetPassword.wantFocus) {
        gp::InputFocus(gGetPassword.editPwd, cx->app, cx->win);
        gp::InputSelectAll(gGetPassword.editPwd, cx->app, cx->win);
        gGetPassword.wantFocus = cx->win->input != gGetPassword.editPwd;
    }
    return content;
}

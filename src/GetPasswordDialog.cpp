/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/PlatformWindow.h"
#include "gui/PasswordDialog.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

//   Enter password for foo.pdf
//   Password:  ********
//   ---------------------------------------------- hairline
//   [ ] Show password
//   [ ] Remember the password for this document
//                                    [Cancel] [OK]
// Modal: engine load calls PasswordUI::GetPassword and waits for the password.
constexpr int kPasswordDlgDx = 400;
struct GetPasswordWnd : WindowBase {
    ~GetPasswordWnd() override;

    HWND hwndParent = nullptr;
    Str fileName;
    bool* remember = nullptr;
    bool* showPassword = nullptr;
    Edit* editPwd = nullptr;
    Checkbox* chkShow = nullptr;
    Checkbox* chkRemember = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;
    bool done = false;
    bool accepted = false;
    Str pwdOut;

    bool Create();
    void OnShowPasswordChanged();
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void Finish(bool ok);
};

GetPasswordWnd::~GetPasswordWnd() {
    str::Free(fileName);
    str::Free(pwdOut);
}

void GetPasswordWnd::Finish(bool ok) {
    if (done) {
        return;
    }
    if (ok && editPwd) {
        pwdOut = str::Dup(editPwd->GetTextTemp());
        if (remember && chkRemember) {
            *remember = chkRemember->IsChecked();
        }
    }
    accepted = ok;
    done = true;
    if (hwnd) {
        SetIsVisible(false);
        PostMessageW(hwnd, WM_NULL, 0, 0);
    }
}

void GetPasswordWnd::OnShowPasswordChanged() {
    bool show = chkShow && chkShow->IsChecked();
    if (showPassword) {
        *showPassword = show;
    }
    EditSetPasswordVisible(editPwd, show);
}

static void OnClose(WindowBase::CloseEvent* ev) {
    auto* wnd = (GetPasswordWnd*)ev->e->self;
    wnd->Finish(false);
}

void GetPasswordWnd::OnCancel(VirtMouseEvent*) {
    Finish(false);
}

void GetPasswordWnd::OnOk(VirtMouseEvent*) {
    Finish(true);
}

bool GetPasswordWnd::Create() {
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    PlatformFont* fontQuery = DlgUiFont(kToolDlgQueryFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.parent = hwndParent;
        args.title = Tr("Enter password");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = fontText;
        args.isRtl = isRtl;
        args.bgColor = colBg;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    SetColors(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(NewDlgCaption(fmt(Tr("Enter password for %s").s, fileName), fontText, colMuted));

    // the password, in the larger font
    {
        HBox* row = NewDlgRow();
        row->AddChild(NewDlgLabel(Tr("&Password:"), fontText, colMuted, kToolDlgLineDy, true));
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = fontQuery;
        args.withBorder = false;
        args.isPassword = true;
        args.selectAllOnFocus = true;
        args.isRtl = isRtl;
        editPwd = new Edit();
        editPwd->SetColors(colTxt, colBg);
        editPwd->Create(args);
        row->AddChild(NewDlgUnderlined(editPwd), 1);
        vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
    }
    vbox->AddChild(new VirtLine());

    // the options, a row each
    auto* options = new VBox();
    options->alignCross = CrossAxisAlign::Stretch;
    auto addCheck = [&](Str text, bool checked) {
        Checkbox::CreateArgs args;
        args.parent = hwnd;
        args.text = text;
        args.font = fontText;
        args.isRtl = isRtl;
        args.initialState = checked ? Checkbox::State::Checked : Checkbox::State::Unchecked;
        auto* c = new Checkbox();
        c->Create(args);
        c->SetColors(colTxt, colBg);
        HBox* row = NewDlgRow();
        row->AddChild(new Spacer(0, DpiScale(kToolDlgRowDy)));
        row->AddChild(c);
        options->AddChild(row);
        return c;
    };
    bool show = showPassword && *showPassword;
    chkShow = addCheck(Tr("&Show password"), show);
    chkShow->onStateChanged = MkMethod0<GetPasswordWnd, &GetPasswordWnd::OnShowPasswordChanged>(this);
    EditSetPasswordVisible(editPwd, show);
    if (remember) {
        chkRemember = addCheck(Tr("&Remember the password for this document"), false);
    }
    vbox->AddChild(new Padding(options, DpiScaledInsets(kToolDlgFilesPadDy, kToolDlgPadDx)));

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<GetPasswordWnd, VirtMouseEvent*, &GetPasswordWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<GetPasswordWnd, VirtMouseEvent*, &GetPasswordWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kPasswordDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, hwndParent);
    UpdateTheme();

    SetIsVisible(true);
    BringWindowToTop(hwnd);
    EditSetFocus(editPwd);
    return true;
}

// Modal: disable the parent and pump until OK / Cancel / close. Engine load
// (often on a worker thread) calls this and cannot continue without a result.
bool ShowPasswordDialog(const PasswordDialogArgs& args, PasswordDialogResult* result) {
    if (!result) {
        return false;
    }
    *result = {};
    bool rememberPassword = args.rememberPassword;
    bool showPassword = args.showPassword;
    auto* wnd = new GetPasswordWnd();
    wnd->hwndParent = args.parent && IsWindow(args.parent) ? args.parent : nullptr;
    wnd->fileName = str::Dup(args.fileName);
    wnd->remember = args.canRemember ? &rememberPassword : nullptr;
    wnd->showPassword = &showPassword;
    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create();
    if (!ok) {
        delete wnd;
        return false;
    }

    HWND parent = wnd->hwndParent;
    if (parent) {
        EnableWindow(parent, FALSE);
    }

    MSG msg;
    while (!wnd->done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_QUIT) {
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if (PreTranslateMessage(msg)) {
            continue;
        }
        if (IsDialogMessageW(wnd->hwnd, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (parent && IsWindow(parent)) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }

    result->accepted = wnd->accepted;
    result->rememberPassword = rememberPassword;
    result->showPassword = showPassword;
    if (wnd->accepted) {
        result->password = str::Dup(wnd->pwdOut);
    }
    delete wnd;
    return result->accepted;
}

Str ShowGetPasswordDialog(HWND hwndParent, Str fileName, bool* rememberPassword, bool* showPassword) {
    PasswordDialogArgs args;
    args.parent = hwndParent;
    args.fileName = fileName;
    args.canRemember = rememberPassword != nullptr;
    args.rememberPassword = rememberPassword && *rememberPassword;
    args.showPassword = showPassword && *showPassword;
    PasswordDialogResult result;
    ShowPasswordDialog(args, &result);
    if (rememberPassword && result.accepted) {
        *rememberPassword = result.rememberPassword;
    }
    if (showPassword) {
        *showPassword = result.showPassword;
    }
    return result.password;
}

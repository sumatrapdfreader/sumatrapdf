/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "GetPasswordDialog.h"

// Labels and buttons are VirtCtrl; the password field and checkboxes are HWNDs.
// Same WindowBase layout as Change Theme. The dialog itself stays modal: engine
// load calls PasswordUI::GetPassword and has to wait for the typed password.
struct GetPasswordWnd : WindowBase {
    ~GetPasswordWnd() override;

    HWND hwndParent = nullptr;
    Str fileName;
    bool* remember = nullptr;
    bool* showPassword = nullptr;
    VirtText* labelFile = nullptr;
    VirtText* labelPwd = nullptr;
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
    if (editPwd) {
        editPwd->SetPasswordVisible(show);
    }
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
    {
        CreateCustomArgs args;
        args.parent = hwndParent;
        args.title = _TRA("Enter password");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = GetFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    bool isRtl = IsUIRtl();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto* c = NewVirtText({
            .s = fmt(_TRA("Enter password for %s").s, fileName),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 0, 4, 0),
        });
        labelFile = c;
        vbox->AddChild(c);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        auto* lab = NewVirtText({
            .s = _TRA("&Password:"),
            .font = font,
            .isRtl = isRtl,
            .prefix = true,
        });
        labelPwd = lab;
        hbox->AddChild(lab);

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.withBorder = true;
        args.isPassword = true;
        args.selectAllOnFocus = true;
        args.isRtl = isRtl;
        auto* e = new Edit();
        e->SetInsetsPt(0, 8, 0, 0);
        e->Create(args);
        editPwd = e;
        hbox->AddChild(e, 1);
        vbox->AddChild(hbox);
    }

    {
        Checkbox::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("&Show password");
        args.isRtl = isRtl;
        if (showPassword && *showPassword) {
            args.initialState = Checkbox::State::Checked;
        }
        auto* c = new Checkbox();
        c->SetInsetsPt(8, 0, 0, 0);
        c->Create(args);
        c->onStateChanged = MkMethod0<GetPasswordWnd, &GetPasswordWnd::OnShowPasswordChanged>(this);
        chkShow = c;
        vbox->AddChild(c);
        if (showPassword && *showPassword) {
            editPwd->SetPasswordVisible(true);
        }
    }

    if (remember) {
        Checkbox::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("&Remember the password for this document");
        args.isRtl = isRtl;
        auto* c = new Checkbox();
        c->SetInsetsPt(4, 0, 0, 0);
        c->Create(args);
        chkRemember = c;
        vbox->AddChild(c);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<GetPasswordWnd, VirtMouseEvent*, &GetPasswordWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<GetPasswordWnd, VirtMouseEvent*, &GetPasswordWnd::OnOk>(this);
        hbox->AddChild(new Padding(btnOk, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(360);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, hwndParent);
    UpdateTheme();

    SetIsVisible(true);
    BringWindowToTop(hwnd);
    if (editPwd) {
        HwndSetFocus(editPwd->hwnd);
    }
    return true;
}

// Modal: disable the parent and pump until OK / Cancel / close. Engine load
// (often on a worker thread) calls this and cannot continue without a result.
Str ShowGetPasswordDialog(HWND hwndParent, Str fileName, bool* rememberPassword, bool* showPassword) {
    auto* wnd = new GetPasswordWnd();
    wnd->hwndParent = hwndParent && IsWindow(hwndParent) ? hwndParent : nullptr;
    wnd->fileName = str::Dup(fileName);
    wnd->remember = rememberPassword;
    wnd->showPassword = showPassword;
    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create();
    if (!ok) {
        delete wnd;
        return {};
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

    Str result = wnd->accepted ? str::Dup(wnd->pwdOut) : Str{};
    delete wnd;
    return result;
}

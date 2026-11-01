/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Http.h"
#include "base/UITask.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

// so_ + 10 char user id + _ + 32 char secret
constexpr int kOnlineApiKeyLen = 46;
constexpr DWORD kHttpStatusOk = 200;
constexpr DWORD kHttpStatusUnauthorized = 401;

// 0: nothing in flight. A finished request whose id does not match is dropped.
static u32 gOnlineReqSeq = 0;

struct OnlineVerifyResult {
    u32 id = 0;
    DWORD status = (DWORD)-1;
    DWORD error = 0;
    Str token;
    Str message;
    // set only when OnlineURLAllowed rejected the URL before WinHttpOpen
    bool urlRejected = false;
    ~OnlineVerifyResult() {
        str::Free(token);
        str::Free(message);
    }
};

struct OnlineVerifyReq {
    u32 id = 0;
    Str url;
    Str token;
    ~OnlineVerifyReq() {
        str::Free(url);
        str::Free(token);
    }
};

// ^so_[0-9A-Za-z]{10}_[0-9A-Za-z]{32}$
static bool IsOnlineApiKey(Str s) {
    if (len(s) != kOnlineApiKeyLen || !s.s || s.s[0] != 's' || s.s[1] != 'o' || s.s[2] != '_' || s.s[13] != '_') {
        return false;
    }
    for (int i = 3; i < 13; i++) {
        char c = s.s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
            return false;
        }
    }
    for (int i = 14; i < kOnlineApiKeyLen; i++) {
        char c = s.s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
            return false;
        }
    }
    return true;
}

// Empty onlineServer is production. Anything that is not one of the three
// origins is not used, even when it would pass a prefix check.
static Str OnlineOrigin() {
    Str server = (gSettings && len(gSettings->onlineServer) > 0) ? gSettings->onlineServer : Str{};
    bool known = str::Eq(server, StrL("https://online.sumatrapdfreader.org")) ||
                 str::Eq(server, StrL("http://localhost:9332")) || str::Eq(server, StrL("http://127.0.0.1:9332"));
    if (known && OnlineURLAllowed(server)) {
        return server;
    }
    return StrL("https://online.sumatrapdfreader.org");
}

struct SumatraOnlineWnd : WindowBase {
    ~SumatraOnlineWnd() override = default;

    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    VirtText* status = nullptr;
    VirtButton* btnLogin = nullptr;
    VirtButton* btnForget = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;
    bool inFlight = false;
    u32 reqId = 0;

    bool Create(MainWindow* win);
    void SetStatus(Str s);
    void SetOkEnabled(bool enabled);
    void SetForgetEnabled(bool enabled);
    void OnLogin(VirtMouseEvent* ev = nullptr);
    void OnForget(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void FinishVerify(OnlineVerifyResult* res);
};

static SumatraOnlineWnd* gSumatraOnlineWnd = nullptr;

static void ClearSumatraOnlineWnd() {
    gSumatraOnlineWnd = nullptr;
}

static VirtText* NewOnlineNote(Str s, PlatformFont* font, Color col) {
    VirtText* t = NewVirtText({.s = s, .font = font, .textColor = col, .isRtl = IsUIRtl()});
    int pad = DpiScale(kToolDlgPadDx);
    t->padding = Insets{DpiScale(6), pad, 0, pad};
    return t;
}

void SumatraOnlineWnd::SetStatus(Str s) {
    if (!status) {
        return;
    }
    status->SetText(s);
    status->Invalidate();
    if (hwnd) {
        HwndInvalidate(hwnd, false);
    }
}

void SumatraOnlineWnd::SetOkEnabled(bool enabled) {
    SetDlgButtonEnabled(btnOk, DlgButtonStyle::Filled, enabled);
}

void SumatraOnlineWnd::SetForgetEnabled(bool enabled) {
    SetDlgButtonEnabled(btnForget, DlgButtonStyle::Ghost, enabled);
}

void SumatraOnlineWnd::OnLogin(VirtMouseEvent*) {
    Str origin = OnlineOrigin();
    TempStr url = fmt("%s/desktop", origin);
    if (!OnlineURLAllowed(url)) {
        SetStatus(Tr("The server address is not allowed."));
        return;
    }
    SumatraLaunchBrowser(url);
}

void SumatraOnlineWnd::OnForget(VirtMouseEvent*) {
    if (!gSettings) {
        return;
    }
    // local only; the website's revoke button is what invalidates the key
    str::ReplaceWithCopy(&gSettings->onlineApiKey, Str{});
    ScheduleSaveSettings();
    reqId = 0;
    inFlight = false;
    SetOkEnabled(true);
    SetForgetEnabled(false);
    if (edit) {
        edit->SetText(Str{});
    }
    SetStatus(Tr("Key removed from this computer."));
}

void SumatraOnlineWnd::OnCancel(VirtMouseEvent*) {
    reqId = 0;
    inFlight = false;
    ScheduleDelete();
}

void SumatraOnlineWnd::FinishVerify(OnlineVerifyResult* res) {
    reqId = 0;
    inFlight = false;
    SetOkEnabled(true);
    if (!gSettings) {
        return;
    }
    if (res->status == kHttpStatusOk) {
        str::ReplaceWithCopy(&gSettings->onlineApiKey, res->token);
        ScheduleSaveSettings();
        if (edit) {
            edit->SetText(res->token);
        }
        SetForgetEnabled(true);
        SetStatus(Tr("Key saved."));
        return;
    }
    if (res->status == kHttpStatusUnauthorized) {
        SetStatus(Tr("That key was rejected. Create a new key on the website and paste it again."));
        return;
    }
    if (res->status >= 100 && res->status < 600) {
        SetStatus(fmt("HTTP %d", (int)res->status));
        return;
    }
    if (res->urlRejected) {
        SetStatus(Tr("The server address is not allowed."));
        return;
    }
    if (len(res->message) > 0) {
        SetStatus(res->message);
        return;
    }
    SetStatus(Tr("Could not reach Sumatra Online."));
}

static void VerifyOnlineKeyDone(OnlineVerifyResult* res) {
    AutoDelete del(res);
    SumatraOnlineWnd* wnd = gSumatraOnlineWnd;
    if (!wnd || wnd->reqId != res->id) {
        return;
    }
    wnd->FinishVerify(res);
}

static void VerifyOnlineKeyThread(OnlineVerifyReq* req) {
    AutoDelete del(req);
    HttpRsp rsp;
    TempStr hdr = fmt("Authorization: Bearer %s", req->token);
    HttpGetWithHeaders(req->url, hdr, &rsp);

    auto* res = new OnlineVerifyResult();
    res->id = req->id;
    res->status = rsp.httpStatusCode;
    res->error = rsp.error;
    res->token = str::Dup(req->token);
    // ERROR_INVALID_PARAMETER is also a WinHTTP failure after the session opens
    res->urlRejected = !OnlineURLAllowed(req->url);
    if (!res->urlRejected && rsp.httpStatusCode == (DWORD)-1 && rsp.error != 0) {
        DWORD err = rsp.error;
        TempStr msg = GetLastErrorStrTemp(err);
        str::TrimWSInPlace(msg, str::TrimOpt::Both);
        res->message = str::Dup(msg);
    }
    uitask::Post(MkFunc0(VerifyOnlineKeyDone, res), "OnlineKeyVerified");
}

void SumatraOnlineWnd::OnOk(VirtMouseEvent*) {
    if (inFlight || !edit) {
        return;
    }
    Str token = edit->GetTextTemp();
    if (!IsOnlineApiKey(token)) {
        SetStatus(Tr("That is not a Sumatra Online API key."));
        return;
    }
    Str origin = OnlineOrigin();
    TempStr url = fmt("%s/api/files", origin);
    if (!OnlineURLAllowed(url)) {
        SetStatus(Tr("The server address is not allowed."));
        return;
    }

    inFlight = true;
    reqId = ++gOnlineReqSeq;
    SetOkEnabled(false);

    auto* req = new OnlineVerifyReq();
    req->id = reqId;
    req->url = str::Dup(url);
    req->token = str::Dup(token);
    RunAsync(MkFunc0(VerifyOnlineKeyThread, req), StrL("SumatraOnlineKey"));
}

static void OnClose(WindowBase::CloseEvent*) {
    if (gSumatraOnlineWnd) {
        gSumatraOnlineWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent*) {
    if (gSumatraOnlineWnd) {
        gSumatraOnlineWnd->OnCancel();
    }
}

bool SumatraOnlineWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Link Sumatra Online");
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

    Str saved = (gSettings && len(gSettings->onlineApiKey) > 0) ? gSettings->onlineApiKey : Str{};

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(NewOnlineNote(Tr("Log in in the browser, then paste the API key here."), fontText, colMuted));
    vbox->AddChild(NewOnlineNote(Tr("The key is a password for the files on the account."), fontText, colMuted));
    vbox->AddChild(
        NewOnlineNote(Tr("Forget key removes it from this computer. To revoke it, use the button on the website."),
                      fontText, colMuted));

    edit = NewDlgEdit(hwnd, fontText, saved, kOnlineApiKeyLen);
    Insets fieldPad = DpiScaledInsets(8, kToolDlgPadDx, 4, kToolDlgPadDx);
    vbox->AddChild(new Padding(NewDlgUnderlined(edit), fieldPad));

    status = NewVirtText({.s = {}, .font = fontText, .textColor = colTxt, .isRtl = isRtl, .ellipsis = true});
    int pad = DpiScale(kToolDlgPadDx);
    status->padding = Insets{DpiScale(8), pad, DpiScale(4), pad};
    vbox->AddChild(status);

    btnLogin = NewDlgButton(hwnd, Tr("Log in..."), fontText, DlgButtonStyle::Ghost, false);
    btnLogin->onClick = MkMethod1<SumatraOnlineWnd, VirtMouseEvent*, &SumatraOnlineWnd::OnLogin>(this);
    btnForget = NewDlgButton(hwnd, Tr("Forget key"), fontText, DlgButtonStyle::Ghost, false);
    btnForget->onClick = MkMethod1<SumatraOnlineWnd, VirtMouseEvent*, &SumatraOnlineWnd::OnForget>(this);
    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<SumatraOnlineWnd, VirtMouseEvent*, &SumatraOnlineWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<SumatraOnlineWnd, VirtMouseEvent*, &SumatraOnlineWnd::OnOk>(this);
    SetForgetEnabled(len(saved) > 0);

    HBox* row = NewDlgRow();
    row->AddChild(btnLogin);
    row->AddChild(btnForget);
    row->AddChild(new Spacer(0, 0), 1);
    row->AddChild(btnCancel);
    row->AddChild(btnOk);
    auto* foot = new VBox();
    foot->alignCross = CrossAxisAlign::Stretch;
    foot->AddChild(new Padding(row, DpiScaledInsets(kToolDlgFootPadDy, kToolDlgPadDx)));
    vbox->AddChild(foot);
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(520), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    HwndSetFocus(edit->hwnd);
    return true;
}

void ShowSumatraOnlineDialog(MainWindow* win) {
    if (!CanAccessDisk() || !HasPermission(Perm::SavePreferences) || !HasPermission(Perm::InternetAccess)) {
        return;
    }
    if (gSumatraOnlineWnd) {
        HwndSetFocus(gSumatraOnlineWnd->hwnd);
        if (gSumatraOnlineWnd->edit) {
            HwndSetFocus(gSumatraOnlineWnd->edit->hwnd);
        }
        return;
    }
    auto* wnd = new SumatraOnlineWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearSumatraOnlineWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gSumatraOnlineWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}

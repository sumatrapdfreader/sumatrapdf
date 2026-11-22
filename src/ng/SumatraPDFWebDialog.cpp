/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SumatraPDFWebDialog.cpp is a modal popup: four notes, the API key
// field, the account's files, a status line, Upload... / Upload this document /
// Open, and Log in... / Forget key / Cancel / OK. Here it is the same column of
// gpui elements, in a window of its own where the platform has one, else in a
// card over the frame. The key check and the transfers are in
// SumatraPDFWebCommon.cpp, shared with orig.

#include "gui/GpuiBridge.h"

#include "base/File.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "EngineBase.h"
#include "DocController.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "PdfToolsCommon.h"
#include "SumatraPDFWebCommon.h"
#include "ServerFiles.h"

constexpr float kWebDlgDx = 520;
constexpr float kWebBtnGap = 8;

struct WebDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    bool wantFocus = false;
    gpui::InputState* editKey = nullptr;
    Str status;
    StrVec names;
    Vec<i64> sizes;
    int sel = -1;
    float scrollY = 0;
    // last /api/files status. 401 blocks another POST until a later response is not 401.
    DWORD lastHttpStatus = 0;
    bool inFlight = false;
    bool clearListStatus = false;
    u32 reqId = 0;
};

static WebDlg gWeb;

struct WebView {
    static void OnOk(WebView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(WebView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(WebView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnLogin(WebView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnForget(WebView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnUpload(WebView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnUploadDoc(WebView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnOpen(WebView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRowClick(WebView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnScroll(WebView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<WebView> gWebView;

// orig's modal window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gWebTw = nullptr;

bool IsSumatraPDFWebDialogVisible() {
    return gWeb.visible && !gWebTw;
}

static Str WebDlgTitle() {
    return Tr("Link SumatraPDF Web");
}

// The window the dialog was opened for, or nullptr once it's gone: on quit
// the dialog's own window closes after the main window was deleted.
static MainWindow* WebOwner() {
    return IsMainWindowValid(gWeb.win) ? gWeb.win : nullptr;
}

static void WebInvalidate() {
    if (gWebTw) {
        ToolWindowInvalidate(gWebTw);
    }
    AppShellInvalidate(WebOwner());
}

static void SetStatus(Str s) {
    str::ReplaceWithCopy(&gWeb.status, s);
    WebInvalidate();
}

static bool KeySaved() {
    return gSettings && IsOnlineApiKey(gSettings->onlineApiKey);
}

static void ClearFileList() {
    gWeb.names.Reset();
    VecReset(gWeb.sizes);
    gWeb.sel = -1;
    gWeb.scrollY = 0;
}

static bool CanUploadDoc() {
    MainWindow* win = WebOwner();
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    // Home and Favorites leave filePath empty. Do not invent a path.
    return tab && len(tab->filePath) > 0 && file::Exists(tab->filePath);
}

void CloseSumatraPDFWebDialog() {
    if (!gWeb.visible) {
        return;
    }
    gWeb.visible = false;
    gWeb.reqId = 0;
    gWeb.inFlight = false;
    MainWindow* win = WebOwner();
    // Blur the window that holds the field before freeing it. The tool
    // window's close otherwise blurs the freed input.
    gp::Window* host = gWebTw ? ToolWindowGpui(gWebTw) : (win ? win->gpuiWin : nullptr);
    if (host && gWeb.editKey) {
        gp::InputBlur(gWeb.editKey, host->app, host);
    }
    delete gWeb.editKey;
    gWeb.editKey = nullptr;
    str::Free(gWeb.status);
    gWeb.status = {};
    ClearFileList();
    DlgWindowClose(&gWebTw);
    AppShellInvalidate(win);
}

static void FinishVerify(OnlineVerifyResult* res) {
    gWeb.reqId = 0;
    gWeb.inFlight = false;
    if (!gSettings) {
        return;
    }
    if (res->status != kHttpStatusOk) {
        SetStatus(OnlineVerifyMsgTemp(res));
        return;
    }
    // A different key must not keep the previous account's names for the replace prompt.
    bool same = str::Eq(gSettings->onlineApiKey, res->token);
    str::ReplaceWithCopy(&gSettings->onlineApiKey, res->token);
    ScheduleSaveSettings();
    if (!same) {
        ClearFileList();
    }
    ServerFilesReset();
    gWeb.lastHttpStatus = kHttpStatusOk;
    if (gWeb.editKey) {
        gp::InputSetValue(gWeb.editKey, ToGpui(res->token));
    }
    // Verify does not draw the file list. Listing happens when the dialog opens with a saved key.
    SetStatus(Tr("Key saved."));
}

static void VerifyDone(OnlineVerifyResult* res) {
    if (!gWeb.visible || gWeb.reqId != res->id) {
        return;
    }
    FinishVerify(res);
}

static void FinishList(OnlineJobResult* res) {
    TempStr msg;
    if (OnlineJobFailed(res, &gWeb.lastHttpStatus, &msg)) {
        gWeb.clearListStatus = false;
        SetStatus(msg);
        return;
    }
    Vec<OnlineListedFile> parsed;
    if (!ParseOnlineFiles(res->body, parsed)) {
        gWeb.clearListStatus = false;
        SetStatus(Tr("Could not read the file list."));
        return;
    }
    ClearFileList();
    for (OnlineListedFile& f : parsed) {
        gWeb.names.Append(f.name);
        VecAppend(gWeb.sizes, f.size);
    }
    FreeOnlineFiles(parsed);
    if (gWeb.clearListStatus) {
        SetStatus({});
    }
    gWeb.clearListStatus = false;
    WebInvalidate();
}

static void OnlineJobDone(OnlineJobResult* res);

static void StartList() {
    if (gWeb.inFlight || !KeySaved()) {
        return;
    }
    bool hadStatus = len(gWeb.status) > 0;
    TempStr err;
    if (!StartOnlineList(gSettings->onlineApiKey, &gWeb.reqId, MkFunc1Void<OnlineJobResult*>(OnlineJobDone), &err)) {
        SetStatus(err);
        return;
    }
    gWeb.inFlight = true;
    gWeb.clearListStatus = !hadStatus;
    if (!hadStatus) {
        SetStatus(Tr("Contacting SumatraPDF Web..."));
    }
    WebInvalidate();
}

static void FinishUpload(OnlineJobResult* res) {
    RememberOnlineJobHash(res);
    TempStr msg;
    if (OnlineJobFailed(res, &gWeb.lastHttpStatus, &msg)) {
        SetStatus(msg);
        return;
    }
    SetStatus(Tr("Uploaded."));
    ServerFilesReset();
    StartList();
}

static void FinishDownload(OnlineJobResult* res) {
    RememberOnlineJobHash(res);
    TempStr msg;
    if (OnlineJobFailed(res, &gWeb.lastHttpStatus, &msg)) {
        SetStatus(msg);
        return;
    }
    WebInvalidate();
    if (len(res->cachePath) == 0 || !IsMainWindowValidAndNotClosing(gWeb.win)) {
        return;
    }
    // Cache path must not become a File / History entry. The tab title is the server name.
    if (!LoadDocument(gWeb.win, res->cachePath)) {
        return;
    }
    WindowTab* tab = gWeb.win->CurrentTab();
    if (!tab) {
        return;
    }
    tab->skipHistory = true;
    tab->SetDisplayName(res->displayName);
}

static void OnlineJobDone(OnlineJobResult* res) {
    if (!gWeb.visible || gWeb.reqId != res->id) {
        return;
    }
    gWeb.inFlight = false;
    gWeb.reqId = 0;
    if (res->job == OnlineJob::List) {
        FinishList(res);
    } else if (res->job == OnlineJob::Upload) {
        FinishUpload(res);
    } else {
        FinishDownload(res);
    }
    WebInvalidate();
}

static void OkClicked() {
    if (gWeb.inFlight || !gWeb.editKey) {
        return;
    }
    TempStr token = str::DupTemp(FromGpui(gp::InputValue(gWeb.editKey)));
    if (!IsOnlineApiKey(token)) {
        SetStatus(Tr("That is not a SumatraPDF Web API key."));
        return;
    }
    TempStr err;
    if (!StartOnlineVerify(token, &gWeb.reqId, MkFunc1Void<OnlineVerifyResult*>(VerifyDone), &err)) {
        SetStatus(err);
        return;
    }
    gWeb.inFlight = true;
    WebInvalidate();
}

void WebView::OnOk(WebView*, gp::Ctx* cx, const gp::ClickEvent*) {
    OkClicked();
    gp::Notify(cx);
}

void WebView::OnCancel(WebView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseSumatraPDFWebDialog();
    gp::Notify(cx);
}

void WebView::OnInput(WebView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    OkClicked();
    gp::Notify(cx);
}

void WebView::OnLogin(WebView*, gp::Ctx* cx, const gp::ClickEvent*) {
    TempStr url = OnlineLoginUrlTemp();
    if (len(url) == 0) {
        SetStatus(Tr("The server address is not allowed."));
    } else {
        SumatraLaunchBrowser(url);
    }
    gp::Notify(cx);
}

void WebView::OnForget(WebView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gSettings) {
        // local only; the website's revoke button is what invalidates the key
        str::ReplaceWithCopy(&gSettings->onlineApiKey, Str{});
        ScheduleSaveSettings();
        gWeb.reqId = 0;
        gWeb.inFlight = false;
        gWeb.lastHttpStatus = 0;
        gWeb.clearListStatus = false;
        ClearFileList();
        ServerFilesReset();
        if (gWeb.editKey) {
            gp::InputSetValue(gWeb.editKey, GStrL(""));
        }
        SetStatus(Tr("Key removed from this computer."));
    }
    gp::Notify(cx);
}

struct UploadAsk {
    Str path;
};

static void UploadAnswered(UploadAsk* ask, int answer) {
    Str path = ask->path;
    AutoFree<char> pathFree(path.s);
    delete ask;
    if (answer != MbRetYes || !gWeb.visible || gWeb.inFlight || !KeySaved()) {
        return;
    }
    if (gWeb.lastHttpStatus == kHttpStatusUnauthorized) {
        SetStatus(Tr("The server rejected the saved API key. It may have been revoked."));
        return;
    }
    TempStr err;
    if (!StartOnlineUpload(gSettings->onlineApiKey, path, &gWeb.reqId, MkFunc1Void<OnlineJobResult*>(OnlineJobDone),
                           &err)) {
        SetStatus(err);
        return;
    }
    gWeb.inFlight = true;
    WebInvalidate();
}

static void BeginUpload(Str localPath) {
    if (gWeb.inFlight) {
        return;
    }
    if (gWeb.lastHttpStatus == kHttpStatusUnauthorized) {
        SetStatus(Tr("The server rejected the saved API key. It may have been revoked."));
        return;
    }
    if (!KeySaved()) {
        SetStatus(Tr("That is not a SumatraPDF Web API key."));
        return;
    }
    TempStr why = CheckOnlineUploadTemp(localPath);
    if (len(why) > 0) {
        SetStatus(why);
        return;
    }
    Str base = path::GetBaseNameTemp(localPath);
    bool known = gWeb.names.Contains(base);
    TempStr question = known ? fmt(Tr("Replace %s on SumatraPDF Web?").s, base)
                             : fmt(Tr("Upload %s? If that name already exists on the server, it is replaced.").s, base);
    auto* ask = new UploadAsk();
    ask->path = str::Dup(localPath);
    MsgBox(gWeb.win, question, Tr("Link SumatraPDF Web"), MbYesNo | MbIconQuestion, MkFunc1(UploadAnswered, ask));
}

static void UploadPicked(Str path) {
    if (gWeb.visible && len(path) > 0) {
        BeginUpload(path);
    }
}

void WebView::OnUpload(WebView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (!gWeb.inFlight) {
        if (!AppShellPickFileAsync(gWeb.win, Tr("Open"), StrL("*.*"), MkFunc1Void<Str>(UploadPicked))) {
            UploadPicked(AppShellPromptForPathTemp(gWeb.win, Tr("Open"), StrL("*.*")));
        }
    }
    gp::Notify(cx);
}

void WebView::OnUploadDoc(WebView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (!gWeb.inFlight && CanUploadDoc()) {
        BeginUpload(gWeb.win->CurrentTab()->filePath);
    }
    gp::Notify(cx);
}

void WebView::OnOpen(WebView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gp::Notify(cx);
    if (gWeb.inFlight || !KeySaved()) {
        return;
    }
    if (gWeb.lastHttpStatus == kHttpStatusUnauthorized) {
        SetStatus(Tr("The server rejected the saved API key. It may have been revoked."));
        return;
    }
    int sel = gWeb.sel;
    if (sel < 0 || sel >= len(gWeb.names) || sel >= len(gWeb.sizes)) {
        return;
    }
    TempStr err;
    if (!StartOnlineDownload(gSettings->onlineApiKey, gWeb.names[sel], gWeb.sizes[sel], &gWeb.reqId,
                             MkFunc1Void<OnlineJobResult*>(OnlineJobDone), &err)) {
        SetStatus(err);
        return;
    }
    gWeb.inFlight = true;
    WebInvalidate();
}

void WebView::OnRowClick(WebView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    gWeb.sel = (int)idx;
    gp::Notify(cx);
}

void WebView::OnScroll(WebView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gWeb.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gWeb.win);
}

void ShowSumatraPDFWebDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (!CanAccessDisk() || !HasPermission(Perm::SavePreferences) || !HasPermission(Perm::InternetAccess)) {
        return;
    }
    if (gWeb.visible) {
        gWeb.wantFocus = true;
        if (gWebTw) {
            ToolWindowActivate(gWebTw);
        }
        AppShellInvalidate(gWeb.win);
        return;
    }
    gWeb.win = win;
    gWeb.lastHttpStatus = 0;
    gWeb.sel = -1;
    Str saved = (gSettings && len(gSettings->onlineApiKey) > 0) ? gSettings->onlineApiKey : Str{};
    auto* edit = new gp::InputState();
    edit->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gp::InputSetValue(edit, ToGpui(saved));
    gWeb.editKey = edit;
    gWeb.visible = true;
    DialogsTrackFrameDialog(IsSumatraPDFWebDialogVisible, [] { return gWeb.win; }, CloseSumatraPDFWebDialog);

    DlgWindowSpec spec;
    spec.name = "sumatrapdfweb";
    spec.title = WebDlgTitle;
    spec.modal = true;
    spec.build = SumatraPDFWebDialogBuild;
    spec.close = CloseSumatraPDFWebDialog;
    spec.clientDx = kWebDlgDx;
    gWebTw = DlgWindowOpen(spec, win);
    gWeb.wantFocus = true;
    StartList();
    AppShellInvalidate(win);
}

// orig's layout: four notes, the key over a hairline, the files, the status,
// Upload... / Upload this document / Open, then Log in... / Forget key and
// Cancel / OK
gp::El* SumatraPDFWebDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gWeb.visible || gWeb.win != win) {
        return nullptr;
    }
    if (gWebTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gWebView.IsValid()) {
        gWebView = gp::EntityNewState<WebView>(cx->app);
    }
    gWeb.editKey->onChange = gp::ListenTo(gWebView, &WebView::OnInput);
    gp::Listener onOk = gp::ListenTo(gWebView, &WebView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gWebView, &WebView::OnCancel);
    DlgSetDefault(cx, onOk);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgCaption(cx, Tr("Log in in the browser, then paste the API key here.")));
    col->Child(DlgCaption(cx, Tr("The key is a password for the files on the account.")));
    col->Child(
        DlgCaption(cx, Tr("Forget key removes it from this computer. To revoke it, use the button on the website.")));
    col->Child(DlgCaption(cx, Tr("This is a copy. Edits stay on this computer until you upload again. Save does not "
                                 "update SumatraPDF Web.")));
    col->Child(DlgLine(cx, {}, 0, StrL("web-key"), gWeb.editKey, DlgField::Underlined));

    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("web-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(kOnlineFileLines * kDlgListRowDy)
                       ->Shrink0()
                       ->ScrollY(gWeb.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gWebView, &WebView::OnScroll));
    for (int i = 0; i < len(gWeb.names); i++) {
        Str meta = i < len(gWeb.sizes) ? Str(str::FormatSizeShortTemp(gWeb.sizes[i])) : Str{};
        list->Child(DlgListRow(cx, fmt("web-row-%d", i), gWeb.names[i], i == gWeb.sel,
                               gp::ListenTo(gWebView, &WebView::OnRowClick, (intptr_t)i), meta));
    }
    col->Child(DlgListBox(cx, list));
    col->Child(DlgCaption(cx, gWeb.status));

    bool can = !gWeb.inFlight && KeySaved() && gWeb.lastHttpStatus != kHttpStatusUnauthorized;
    gp::El* fileRow = DlgFieldRow(cx)->Gap(kWebBtnGap);
    fileRow->Child(
        DlgGhostButton(cx, GStrL("web-upload"), Tr("Upload..."), gp::ListenTo(gWebView, &WebView::OnUpload), !can));
    fileRow->Child(DlgGhostButton(cx, GStrL("web-upload-doc"), Tr("Upload this document"),
                                  gp::ListenTo(gWebView, &WebView::OnUploadDoc), !(can && CanUploadDoc())));
    fileRow->Child(DlgGhostButton(cx, GStrL("web-open"), Tr("Open"), gp::ListenTo(gWebView, &WebView::OnOpen),
                                  !(can && gWeb.sel >= 0)));
    col->Child(fileRow);

    gp::El* extra = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(kWebBtnGap);
    extra->Child(DlgGhostButton(cx, GStrL("web-login"), Tr("Log in..."), gp::ListenTo(gWebView, &WebView::OnLogin)));
    extra->Child(DlgGhostButton(cx, GStrL("web-forget"), Tr("Forget key"), gp::ListenTo(gWebView, &WebView::OnForget),
                                !KeySaved()));
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel, extra, gWeb.inFlight));
    gp::El* content = DlgToolContent(cx, col, WebDlgTitle(), kWebDlgDx, onCancel);

    if (gWeb.wantFocus) {
        gp::InputFocus(gWeb.editKey, cx->app, cx->win);
        gWeb.wantFocus = cx->win->input != gWeb.editKey;
    }
    return content;
}

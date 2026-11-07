/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Http.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/JsonParser.h"
#include "base/AutoWin.h"
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
#include "AppTools.h"
#include "MainWindow.h"
#include "WindowTab.h"
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

// Desktop cap. The website still accepts 200 MiB; refusing here keeps a transfer
// inside the server's existing timeouts. Exactly 50 MiB is allowed.
constexpr i64 kOnlineMaxBytes = (i64)50 << 20;
constexpr int kOnlineFileLines = 6;

// 0: nothing in flight. A finished request whose id does not match is dropped.
static u32 gOnlineReqSeq = 0;

// The server's validUploadName: empty, "." and "..", / \ and control characters.
// The server stores files by content, so the name is only metadata; older servers
// also refused : " ; (": and " can't be in a Windows file name anyway).
static bool ValidUploadName(Str name) {
    if (len(name) == 0 || str::Eq(name, StrL(".")) || str::Eq(name, StrL(".."))) {
        return false;
    }
    for (int i = 0; i < len(name); i++) {
        u8 c = (u8)name.s[i];
        if (c == '/' || c == '\\' || c < 0x20 || c == 0x7f) {
            return false;
        }
    }
    return true;
}

enum class OnlineJob : int {
    List = 1,
    Upload,
    Download,
};

struct OnlineListedFile {
    Str name;
    i64 size = 0;
};

struct OnlineNameAcc {
    int idx = -1;
    Str name;
    i64 size = 0;
    bool haveName = false;
    bool haveSize = false;
};

static void FreeOnlineFiles(Vec<OnlineListedFile>& v) {
    for (OnlineListedFile& f : v) {
        str::Free(f.name);
        f.name = {};
    }
    VecReset(v);
}

static void PushOnlineName(Vec<OnlineListedFile>& out, OnlineNameAcc& cur) {
    if (cur.haveName && len(cur.name) > 0) {
        OnlineListedFile f;
        f.name = cur.name;
        f.size = cur.haveSize ? cur.size : 0;
        cur.name = {};
        VecAppend(out, f);
    } else {
        str::Free(cur.name);
    }
    cur = {};
}

// {"files":[{"name":"...","size":N}]} — name bytes are copied; the parser reuses its buffer.
static bool ParseOnlineFiles(Str body, Vec<OnlineListedFile>& out) {
    json::Reader r(body);
    OnlineNameAcc cur;
    for (json::Value& v : r) {
        bool isName = json::PathMatch(v.path, StrL("/files"), StrL("*"), StrL("/name"));
        bool isSize = json::PathMatch(v.path, StrL("/files"), StrL("*"), StrL("/size"));
        if (!isName && !isSize) {
            continue;
        }
        int idx = json::PathSegIndex(json::PathNth(v.path, 1));
        if (idx < 0) {
            continue;
        }
        if (cur.idx != idx) {
            PushOnlineName(out, cur);
            cur.idx = idx;
        }
        if (isName && v.type == json::Type::String) {
            str::Free(cur.name);
            cur.name = str::Dup(v.value);
            cur.haveName = true;
        } else if (isSize && v.type == json::Type::Number) {
            cur.size = ParseInt64(v.value);
            cur.haveSize = true;
        }
    }
    PushOnlineName(out, cur);
    if (!r.Ok()) {
        FreeOnlineFiles(out);
        return false;
    }
    return true;
}

struct OnlineJobResult {
    u32 id = 0;
    OnlineJob job = OnlineJob::List;
    DWORD status = (DWORD)-1;
    DWORD error = 0;
    Str message;
    Str body;
    Str cachePath;
    Str displayName;
    bool urlRejected = false;
    bool tooBig = false;
    ~OnlineJobResult() {
        str::Free(message);
        str::Free(body);
        str::Free(cachePath);
        str::Free(displayName);
    }
};

struct OnlineJobReq {
    u32 id = 0;
    OnlineJob job = OnlineJob::List;
    Str url;
    Str token;
    Str filePath;
    Str destPath;
    Str displayName;
    ~OnlineJobReq() {
        str::Free(url);
        str::Free(token);
        str::Free(filePath);
        str::Free(destPath);
        str::Free(displayName);
    }
};

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
    bool known = str::Eq(server, StrL("https://web.sumatrapdfreader.org")) ||
                 str::Eq(server, StrL("http://localhost:9332")) || str::Eq(server, StrL("http://127.0.0.1:9332"));
    if (known && OnlineURLAllowed(server)) {
        return server;
    }
    return StrL("https://web.sumatrapdfreader.org");
}

struct SumatraPDFWebWnd : WindowBase {
    ~SumatraPDFWebWnd() override = default;

    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    VirtText* status = nullptr;
    VirtButton* btnLogin = nullptr;
    VirtButton* btnForget = nullptr;
    VirtButton* btnUpload = nullptr;
    VirtButton* btnUploadDoc = nullptr;
    VirtButton* btnOpen = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;
    VirtListBox* listBox = nullptr;
    ListBoxModelStrings* fileModel = nullptr; // owned by listBox
    Vec<i64> fileSizes;
    // last /api/files status. 401 blocks another POST until a later response is not 401.
    DWORD lastHttpStatus = 0;
    bool inFlight = false;
    bool clearListStatus = false;
    u32 reqId = 0;

    bool Create(MainWindow* win);
    void SetStatus(Str s);
    void SetOkEnabled(bool enabled);
    void SetForgetEnabled(bool enabled);
    void UpdateFileButtons();
    bool CanUploadDoc() const;
    void ClearFileList();
    void DrawFileRow(VirtListBox::DrawItemEvent* ev);
    void OnFileSel();
    void OnLogin(VirtMouseEvent* ev = nullptr);
    void OnForget(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnUpload(VirtMouseEvent* ev = nullptr);
    void OnUploadDoc(VirtMouseEvent* ev = nullptr);
    void OnOpen(VirtMouseEvent* ev = nullptr);
    void BeginUpload(Str localPath);
    void StartJob(OnlineJobReq* req);
    void StartList();
    bool ShowTransportError(OnlineJobResult* res);
    void FinishVerify(OnlineVerifyResult* res);
    void FinishList(OnlineJobResult* res);
    void FinishUpload(OnlineJobResult* res);
    void FinishDownload(OnlineJobResult* res);
    void FinishJob(OnlineJobResult* res);
};

static SumatraPDFWebWnd* gSumatraPDFWebWnd = nullptr;

static void ClearSumatraPDFWebWnd() {
    gSumatraPDFWebWnd = nullptr;
}

static VirtText* NewOnlineNote(Str s, PlatformFont* font, Color col) {
    VirtText* t = NewVirtText({.s = s, .font = font, .textColor = col, .isRtl = IsUIRtl()});
    int pad = DpiScale(kToolDlgPadDx);
    t->padding = Insets{DpiScale(6), pad, 0, pad};
    return t;
}

void SumatraPDFWebWnd::SetStatus(Str s) {
    if (!status) {
        return;
    }
    status->SetText(s);
    status->Invalidate();
    if (hwnd) {
        HwndInvalidate(hwnd, false);
    }
}

void SumatraPDFWebWnd::SetOkEnabled(bool enabled) {
    SetDlgButtonEnabled(btnOk, DlgButtonStyle::Filled, enabled);
}

void SumatraPDFWebWnd::SetForgetEnabled(bool enabled) {
    SetDlgButtonEnabled(btnForget, DlgButtonStyle::Ghost, enabled);
}

bool SumatraPDFWebWnd::CanUploadDoc() const {
    if (!win) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    // Home and Favorites leave filePath empty. Do not invent a path.
    if (!tab || len(tab->filePath) == 0 || !file::Exists(tab->filePath)) {
        return false;
    }
    return true;
}

void SumatraPDFWebWnd::UpdateFileButtons() {
    bool busy = inFlight;
    SetOkEnabled(!busy);
    bool keyOk = gSettings && IsOnlineApiKey(gSettings->onlineApiKey);
    bool blocked = lastHttpStatus == kHttpStatusUnauthorized;
    bool can = !busy && keyOk && !blocked;
    if (btnUpload) {
        SetDlgButtonEnabled(btnUpload, DlgButtonStyle::Ghost, can);
    }
    if (btnUploadDoc) {
        SetDlgButtonEnabled(btnUploadDoc, DlgButtonStyle::Ghost, can && CanUploadDoc());
    }
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    if (btnOpen) {
        SetDlgButtonEnabled(btnOpen, DlgButtonStyle::Ghost, can && sel >= 0);
    }
}

void SumatraPDFWebWnd::ClearFileList() {
    VecReset(fileSizes);
    if (!fileModel) {
        return;
    }
    fileModel->strings.Reset();
    if (listBox) {
        listBox->SetModel(fileModel);
    }
}

void SumatraPDFWebWnd::DrawFileRow(VirtListBox::DrawItemEvent* ev) {
    Str name = ev->listBox->model ? ev->listBox->model->Item(ev->itemIndex) : Str{};
    Str meta;
    if (ev->itemIndex >= 0 && ev->itemIndex < len(fileSizes)) {
        meta = str::FormatSizeShortTemp(fileSizes[ev->itemIndex]);
    }
    DrawDlgListItem(ev->listBox, ev->gfx, ev->itemRect, ev->selected, name, meta);
}

void SumatraPDFWebWnd::OnFileSel() {
    UpdateFileButtons();
}

void SumatraPDFWebWnd::OnLogin(VirtMouseEvent*) {
    Str origin = OnlineOrigin();
    TempStr url = fmt("%s/desktop-link", origin);
    if (!OnlineURLAllowed(url)) {
        SetStatus(Tr("The server address is not allowed."));
        return;
    }
    SumatraLaunchBrowser(url);
}

void SumatraPDFWebWnd::OnForget(VirtMouseEvent*) {
    if (!gSettings) {
        return;
    }
    // local only; the website's revoke button is what invalidates the key
    str::ReplaceWithCopy(&gSettings->onlineApiKey, Str{});
    ScheduleSaveSettings();
    reqId = 0;
    inFlight = false;
    lastHttpStatus = 0;
    clearListStatus = false;
    ClearFileList();
    SetForgetEnabled(false);
    if (edit) {
        edit->SetText(Str{});
    }
    SetStatus(Tr("Key removed from this computer."));
    UpdateFileButtons();
}

void SumatraPDFWebWnd::OnCancel(VirtMouseEvent*) {
    reqId = 0;
    inFlight = false;
    ScheduleDelete();
}

void SumatraPDFWebWnd::FinishVerify(OnlineVerifyResult* res) {
    reqId = 0;
    inFlight = false;
    if (!gSettings) {
        UpdateFileButtons();
        return;
    }
    if (res->status == kHttpStatusOk) {
        // A different key must not keep the previous account's names for the replace prompt.
        bool same = str::Eq(gSettings->onlineApiKey, res->token);
        str::ReplaceWithCopy(&gSettings->onlineApiKey, res->token);
        ScheduleSaveSettings();
        if (!same) {
            ClearFileList();
        }
        lastHttpStatus = kHttpStatusOk;
        if (edit) {
            edit->SetText(res->token);
        }
        SetForgetEnabled(true);
        // Verify does not draw the file list. Listing happens when the dialog opens with a saved key.
        SetStatus(Tr("Key saved."));
        UpdateFileButtons();
        return;
    }
    if (res->status == kHttpStatusUnauthorized) {
        SetStatus(Tr("That key was rejected. Create a new key on the website and paste it again."));
        UpdateFileButtons();
        return;
    }
    if (res->status >= 100 && res->status < 600) {
        SetStatus(fmt("HTTP %d", (int)res->status));
        UpdateFileButtons();
        return;
    }
    if (res->urlRejected) {
        SetStatus(Tr("The server address is not allowed."));
        UpdateFileButtons();
        return;
    }
    if (len(res->message) > 0) {
        SetStatus(res->message);
        UpdateFileButtons();
        return;
    }
    SetStatus(Tr("Could not reach SumatraPDF Web."));
    UpdateFileButtons();
}

static void VerifyOnlineKeyDone(OnlineVerifyResult* res) {
    AutoDelete del(res);
    SumatraPDFWebWnd* wnd = gSumatraPDFWebWnd;
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

void SumatraPDFWebWnd::OnOk(VirtMouseEvent*) {
    if (inFlight || !edit) {
        return;
    }
    Str token = edit->GetTextTemp();
    if (!IsOnlineApiKey(token)) {
        SetStatus(Tr("That is not a SumatraPDF Web API key."));
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
    UpdateFileButtons();

    auto* req = new OnlineVerifyReq();
    req->id = reqId;
    req->url = str::Dup(url);
    req->token = str::Dup(token);
    RunAsync(MkFunc0(VerifyOnlineKeyThread, req), StrL("SumatraPDFWebKey"));
}

static void NoteHttp(OnlineJobResult* res, const HttpRsp& rsp) {
    res->status = rsp.httpStatusCode;
    res->error = rsp.error;
    if (res->status != (DWORD)-1 || rsp.error == 0 || rsp.error == ERROR_INVALID_PARAMETER ||
        rsp.error == ERROR_FILE_TOO_LARGE || rsp.error == ERROR_READ_FAULT) {
        return;
    }
    DWORD err = rsp.error;
    TempStr msg = GetLastErrorStrTemp(err);
    str::TrimWSInPlace(msg, str::TrimOpt::Both);
    res->message = str::Dup(msg);
}

static void OnlineJobDone(OnlineJobResult* res);

static void OnlineJobThread(OnlineJobReq* req) {
    AutoDelete del(req);
    auto* res = new OnlineJobResult();
    res->id = req->id;
    res->job = req->job;
    res->urlRejected = !OnlineURLAllowed(req->url);
    if (res->urlRejected) {
        res->error = ERROR_INVALID_PARAMETER;
        uitask::Post(MkFunc0(OnlineJobDone, res), "OnlineJobDone");
        return;
    }

    TempStr hdr = fmt("Authorization: Bearer %s", req->token);
    if (req->job == OnlineJob::Upload) {
        i64 sz = file::GetSize(req->filePath);
        if (sz < 0 || sz > kOnlineMaxBytes) {
            res->tooBig = sz > kOnlineMaxBytes;
            res->error = res->tooBig ? (DWORD)ERROR_FILE_TOO_LARGE : (DWORD)ERROR_READ_FAULT;
            uitask::Post(MkFunc0(OnlineJobDone, res), "OnlineJobDone");
            return;
        }
        Str body = file::ReadFile(req->filePath);
        if ((i64)len(body) > kOnlineMaxBytes) {
            str::Free(body);
            res->tooBig = true;
            res->error = ERROR_FILE_TOO_LARGE;
            uitask::Post(MkFunc0(OnlineJobDone, res), "OnlineJobDone");
            return;
        }
        if ((i64)len(body) != sz) {
            str::Free(body);
            res->error = ERROR_READ_FAULT;
            uitask::Post(MkFunc0(OnlineJobDone, res), "OnlineJobDone");
            return;
        }
        // Content the server already has (another name, another device) isn't sent
        // again: ask it to name that content. 404: it doesn't have it. A server from
        // before sha1 links refuses the empty body. Either way, upload; the server
        // checks the content against sha1.
        u8 digest[20];
        CalcSHA1Digest(body, digest);
        TempStr url = fmt("%s&sha1=%s", req->url, str::MemToHexTemp(Str((const char*)digest, dimofi(digest))));
        HttpRsp linkRsp;
        HttpPostUrlNoRedirect(url, StrL("application/octet-stream"), hdr, Str{}, &linkRsp);
        if (linkRsp.httpStatusCode == kHttpStatusOk || linkRsp.httpStatusCode == kHttpStatusUnauthorized) {
            str::Free(body);
            NoteHttp(res, linkRsp);
            uitask::Post(MkFunc0(OnlineJobDone, res), "OnlineJobDone");
            return;
        }
        HttpRsp rsp;
        HttpPostUrlNoRedirect(url, StrL("application/octet-stream"), hdr, body, &rsp);
        str::Free(body);
        NoteHttp(res, rsp);
        uitask::Post(MkFunc0(OnlineJobDone, res), "OnlineJobDone");
        return;
    }

    if (req->job == OnlineJob::Download) {
        res->cachePath = str::Dup(req->destPath);
        res->displayName = str::Dup(req->displayName);
        HttpRsp rsp;
        HttpDownloadWithHeaders(req->url, hdr, req->destPath, &rsp);
        NoteHttp(res, rsp);
        if (rsp.error == ERROR_FILE_TOO_LARGE) {
            res->tooBig = true;
        }
        uitask::Post(MkFunc0(OnlineJobDone, res), "OnlineJobDone");
        return;
    }

    HttpRsp rsp;
    HttpGetWithHeaders(req->url, hdr, &rsp);
    NoteHttp(res, rsp);
    if (rsp.httpStatusCode == kHttpStatusOk) {
        res->body = str::Dup(ToStr(rsp.data));
    }
    uitask::Post(MkFunc0(OnlineJobDone, res), "OnlineJobDone");
}

void SumatraPDFWebWnd::StartJob(OnlineJobReq* req) {
    inFlight = true;
    reqId = ++gOnlineReqSeq;
    req->id = reqId;
    UpdateFileButtons();
    RunAsync(MkFunc0(OnlineJobThread, req), StrL("SumatraPDFWebFile"));
}

void SumatraPDFWebWnd::StartList() {
    if (inFlight || !gSettings || !IsOnlineApiKey(gSettings->onlineApiKey)) {
        return;
    }
    TempStr url = fmt("%s/api/files", OnlineOrigin());
    if (!OnlineURLAllowed(url)) {
        SetStatus(Tr("The server address is not allowed."));
        return;
    }
    if (!status || len(status->s) == 0) {
        SetStatus(Tr("Contacting SumatraPDF Web..."));
        clearListStatus = true;
    } else {
        clearListStatus = false;
    }
    auto* req = new OnlineJobReq();
    req->job = OnlineJob::List;
    req->url = str::Dup(url);
    req->token = str::Dup(gSettings->onlineApiKey);
    StartJob(req);
}

// true when the caller should stop. A 401 keeps the saved key.
bool SumatraPDFWebWnd::ShowTransportError(OnlineJobResult* res) {
    if (res->tooBig || res->error == ERROR_FILE_TOO_LARGE) {
        if (res->status >= 100 && res->status < 600) {
            lastHttpStatus = res->status;
        }
        if (res->job == OnlineJob::Download) {
            SetStatus(Tr("This file is over 50 MiB. SumatraPDF will not download it."));
        } else {
            SetStatus(
                Tr("This file is over 50 MiB. SumatraPDF will not upload it. The website still accepts files up to 200 "
                   "MiB."));
        }
        return true;
    }
    if (res->status == kHttpStatusUnauthorized) {
        lastHttpStatus = kHttpStatusUnauthorized;
        SetStatus(Tr("The server rejected the saved API key. It may have been revoked."));
        return true;
    }
    if (res->status == kHttpStatusOk && res->error != ERROR_SUCCESS) {
        lastHttpStatus = kHttpStatusOk;
        SetStatus(res->job == OnlineJob::Download ? Tr("Could not save the download.")
                                                  : Tr("Could not reach SumatraPDF Web."));
        return true;
    }
    if (res->status == kHttpStatusOk) {
        lastHttpStatus = kHttpStatusOk;
        return false;
    }
    if (res->status >= 100 && res->status < 600) {
        lastHttpStatus = res->status;
        SetStatus(fmt("HTTP %d", (int)res->status));
        return true;
    }
    if (res->urlRejected) {
        SetStatus(Tr("The server address is not allowed."));
        return true;
    }
    if (res->error == ERROR_READ_FAULT) {
        SetStatus(Tr("Could not read the file."));
        return true;
    }
    if (len(res->message) > 0) {
        SetStatus(res->message);
        return true;
    }
    SetStatus(Tr("Could not reach SumatraPDF Web."));
    return true;
}

void SumatraPDFWebWnd::FinishList(OnlineJobResult* res) {
    if (ShowTransportError(res)) {
        clearListStatus = false;
        UpdateFileButtons();
        return;
    }
    Vec<OnlineListedFile> parsed;
    if (!ParseOnlineFiles(res->body, parsed)) {
        SetStatus(Tr("Could not read the file list."));
        clearListStatus = false;
        UpdateFileButtons();
        return;
    }
    VecReset(fileSizes);
    if (fileModel) {
        fileModel->strings.Reset();
        for (OnlineListedFile& f : parsed) {
            fileModel->strings.Append(f.name);
            VecAppend(fileSizes, f.size);
        }
    }
    FreeOnlineFiles(parsed);
    if (listBox && fileModel) {
        listBox->SetModel(fileModel);
    }
    if (clearListStatus) {
        SetStatus({});
    }
    clearListStatus = false;
    UpdateFileButtons();
}

void SumatraPDFWebWnd::FinishUpload(OnlineJobResult* res) {
    if (ShowTransportError(res)) {
        UpdateFileButtons();
        return;
    }
    SetStatus(Tr("Uploaded."));
    UpdateFileButtons();
    StartList();
}

void SumatraPDFWebWnd::FinishDownload(OnlineJobResult* res) {
    if (ShowTransportError(res)) {
        UpdateFileButtons();
        return;
    }
    UpdateFileButtons();
    if (len(res->cachePath) == 0) {
        return;
    }
    // Cache path must not become a File / History entry. The tab title is the server name.
    LoadArgs args(res->cachePath, win);
    args.skipHistory = true;
    args.SetDisplayName(res->displayName);
    StartLoadDocument(&args);
}

void SumatraPDFWebWnd::FinishJob(OnlineJobResult* res) {
    inFlight = false;
    reqId = 0;
    if (res->job == OnlineJob::List) {
        FinishList(res);
        return;
    }
    if (res->job == OnlineJob::Upload) {
        FinishUpload(res);
        return;
    }
    FinishDownload(res);
}

static void OnlineJobDone(OnlineJobResult* res) {
    AutoDelete del(res);
    SumatraPDFWebWnd* wnd = gSumatraPDFWebWnd;
    if (!wnd || wnd->reqId != res->id) {
        return;
    }
    wnd->FinishJob(res);
}

// One filesystem path. Not OpenFile: that picker is multi-select and loads the file.
static TempStr PickUploadPath(HWND owner) {
    AutoReleaseComPtr<IFileOpenDialog> dlg;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        logf("PickUploadPath: CoCreateInstance failed: 0x%x\n", (uint)hr);
        return {};
    }
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST;
    opts &= ~(DWORD)FOS_ALLOWMULTISELECT;
    dlg->SetOptions(opts);

    hr = dlg->Show(owner);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return {};
    }
    if (FAILED(hr)) {
        logf("PickUploadPath: Show failed: 0x%x\n", (uint)hr);
        return {};
    }

    AutoReleaseComPtr<IShellItem> item;
    hr = dlg->GetResult(&item);
    if (FAILED(hr) || !item) {
        return {};
    }
    PWSTR pathW = nullptr;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &pathW);
    if (FAILED(hr) || !pathW) {
        return {};
    }
    TempStr path = ToUtf8Temp(WStr(pathW));
    CoTaskMemFree(pathW);
    return path;
}

void SumatraPDFWebWnd::BeginUpload(Str localPath) {
    if (inFlight) {
        return;
    }
    if (lastHttpStatus == kHttpStatusUnauthorized) {
        SetStatus(Tr("The server rejected the saved API key. It may have been revoked."));
        return;
    }
    if (!gSettings || !IsOnlineApiKey(gSettings->onlineApiKey)) {
        SetStatus(Tr("That is not a SumatraPDF Web API key."));
        return;
    }
    // MsgBox pumps. localPath may be tab->filePath, freed when a load finishes.
    Str pathOwned = str::Dup(localPath);
    AutoFree<char> pathFree(pathOwned.s);
    Str baseOwned = str::Dup(path::GetBaseNameTemp(pathOwned));
    AutoFree<char> baseFree(baseOwned.s);
    if (!ValidUploadName(baseOwned)) {
        SetStatus(Tr("That file name cannot be uploaded."));
        return;
    }
    i64 sz = file::GetSize(pathOwned);
    if (sz < 0) {
        SetStatus(Tr("Could not read the file."));
        return;
    }
    if (sz > kOnlineMaxBytes) {
        SetStatus(Tr(
            "This file is over 50 MiB. SumatraPDF will not upload it. The website still accepts files up to 200 MiB."));
        return;
    }

    bool known = fileModel && fileModel->strings.Contains(baseOwned);
    TempStr question =
        known ? fmt(Tr("Replace %s on SumatraPDF Web?").s, baseOwned)
              : fmt(Tr("Upload %s? If that name already exists on the server, it is replaced.").s, baseOwned);
    int answer = MsgBox(hwnd, question, Tr("Link SumatraPDF Web"), MB_YESNO | MB_ICONQUESTION);
    if (answer != IDYES) {
        return;
    }
    if (inFlight || lastHttpStatus == kHttpStatusUnauthorized) {
        if (lastHttpStatus == kHttpStatusUnauthorized) {
            SetStatus(Tr("The server rejected the saved API key. It may have been revoked."));
        }
        return;
    }

    TempStr url = fmt("%s/api/files?name=%s", OnlineOrigin(), url::EncodeTemp(baseOwned));
    if (!OnlineURLAllowed(url)) {
        SetStatus(Tr("The server address is not allowed."));
        return;
    }
    auto* req = new OnlineJobReq();
    req->job = OnlineJob::Upload;
    req->url = str::Dup(url);
    req->token = str::Dup(gSettings->onlineApiKey);
    req->filePath = str::Dup(pathOwned);
    StartJob(req);
}

void SumatraPDFWebWnd::OnUpload(VirtMouseEvent*) {
    if (inFlight) {
        return;
    }
    TempStr path = PickUploadPath(hwnd);
    if (len(path) == 0) {
        return;
    }
    BeginUpload(path);
}

void SumatraPDFWebWnd::OnUploadDoc(VirtMouseEvent*) {
    if (inFlight || !CanUploadDoc()) {
        UpdateFileButtons();
        return;
    }
    BeginUpload(win->CurrentTab()->filePath);
}

void SumatraPDFWebWnd::OnOpen(VirtMouseEvent*) {
    if (inFlight || !listBox || !fileModel) {
        return;
    }
    if (lastHttpStatus == kHttpStatusUnauthorized) {
        SetStatus(Tr("The server rejected the saved API key. It may have been revoked."));
        return;
    }
    if (!gSettings || !IsOnlineApiKey(gSettings->onlineApiKey)) {
        return;
    }
    int sel = listBox->GetCurrentSelection();
    if (sel < 0 || sel >= fileModel->ItemsCount() || sel >= len(fileSizes)) {
        return;
    }
    Str name = fileModel->Item(sel);
    if (fileSizes[sel] > kOnlineMaxBytes) {
        SetStatus(Tr("This file is over 50 MiB. SumatraPDF will not download it."));
        return;
    }
    if (!ValidUploadName(name)) {
        SetStatus(Tr("That file name cannot be downloaded."));
        return;
    }
    // <appdata>/SumatraPDF-web/<base name>, only after the name check. The same name overwrites.
    TempStr dir = GetPathInAppDataDirTemp(StrL("SumatraPDF-web"));
    if (!dir::CreateAll(dir)) {
        SetStatus(Tr("Could not save the download."));
        return;
    }
    TempStr cachePath = path::JoinTemp(dir, name);
    TempStr url = fmt("%s/api/file?name=%s", OnlineOrigin(), url::EncodeTemp(name));
    if (!OnlineURLAllowed(url)) {
        SetStatus(Tr("The server address is not allowed."));
        return;
    }
    auto* req = new OnlineJobReq();
    req->job = OnlineJob::Download;
    req->url = str::Dup(url);
    req->token = str::Dup(gSettings->onlineApiKey);
    req->destPath = str::Dup(cachePath);
    req->displayName = str::Dup(name);
    StartJob(req);
}

static void CancelWebWnd() {
    if (gSumatraPDFWebWnd) {
        gSumatraPDFWebWnd->OnCancel();
    }
}

static void OnClose(WindowBase::CloseEvent*) {
    CancelWebWnd();
}

static void OnDestroy(WindowBase::DestroyEvent*) {
    CancelWebWnd();
}

bool SumatraPDFWebWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowDarkerTextColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Link SumatraPDF Web");
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
    vbox->AddChild(NewOnlineNote(
        Tr("This is a copy. Edits stay on this computer until you upload again. Save does not update SumatraPDF Web."),
        fontText, colMuted));

    edit = NewDlgEdit(hwnd, fontText, saved, kOnlineApiKeyLen);
    Insets fieldPad = DpiScaledInsets(8, kToolDlgPadDx, 4, kToolDlgPadDx);
    vbox->AddChild(new Padding(NewDlgUnderlined(edit), fieldPad));

    listBox = NewDlgList(GetDpi(), fontText);
    listBox->idealSizeLines = kOnlineFileLines;
    fileModel = new ListBoxModelStrings();
    listBox->SetModel(fileModel);
    listBox->onDrawItem =
        MkMethod1<SumatraPDFWebWnd, VirtListBox::DrawItemEvent*, &SumatraPDFWebWnd::DrawFileRow>(this);
    listBox->onSelectionChanged = MkMethod0<SumatraPDFWebWnd, &SumatraPDFWebWnd::OnFileSel>(this);
    vbox->AddChild(NewDlgListBox(listBox));

    status = NewVirtText({.s = {}, .font = fontText, .textColor = colTxt, .isRtl = isRtl, .ellipsis = true});
    int pad = DpiScale(kToolDlgPadDx);
    status->padding = Insets{DpiScale(8), pad, DpiScale(4), pad};
    vbox->AddChild(status);

    btnLogin = NewDlgButton(hwnd, Tr("Log in..."), fontText, DlgButtonStyle::Ghost, false);
    btnLogin->onClick = MkMethod1<SumatraPDFWebWnd, VirtMouseEvent*, &SumatraPDFWebWnd::OnLogin>(this);
    btnForget = NewDlgButton(hwnd, Tr("Forget key"), fontText, DlgButtonStyle::Ghost, false);
    btnForget->onClick = MkMethod1<SumatraPDFWebWnd, VirtMouseEvent*, &SumatraPDFWebWnd::OnForget>(this);
    btnUpload = NewDlgButton(hwnd, Tr("Upload..."), fontText, DlgButtonStyle::Ghost, false);
    btnUpload->onClick = MkMethod1<SumatraPDFWebWnd, VirtMouseEvent*, &SumatraPDFWebWnd::OnUpload>(this);
    btnUploadDoc = NewDlgButton(hwnd, Tr("Upload this document"), fontText, DlgButtonStyle::Ghost, false);
    btnUploadDoc->onClick = MkMethod1<SumatraPDFWebWnd, VirtMouseEvent*, &SumatraPDFWebWnd::OnUploadDoc>(this);
    btnOpen = NewDlgButton(hwnd, Tr("Open"), fontText, DlgButtonStyle::Ghost, false);
    btnOpen->onClick = MkMethod1<SumatraPDFWebWnd, VirtMouseEvent*, &SumatraPDFWebWnd::OnOpen>(this);
    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<SumatraPDFWebWnd, VirtMouseEvent*, &SumatraPDFWebWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<SumatraPDFWebWnd, VirtMouseEvent*, &SumatraPDFWebWnd::OnOk>(this);
    SetForgetEnabled(len(saved) > 0);

    HBox* fileRow = NewDlgRow();
    fileRow->AddChild(btnUpload);
    fileRow->AddChild(btnUploadDoc);
    fileRow->AddChild(btnOpen);
    vbox->AddChild(new Padding(fileRow, DpiScaledInsets(0, kToolDlgPadDx, 4, kToolDlgPadDx)));

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

    UpdateFileButtons();
    SetIsVisible(true);
    HwndSetFocus(edit->hwnd);
    return true;
}

void ShowSumatraPDFWebDialog(MainWindow* win) {
    if (!CanAccessDisk() || !HasPermission(Perm::SavePreferences) || !HasPermission(Perm::InternetAccess)) {
        return;
    }
    if (gSumatraPDFWebWnd) {
        HwndSetFocus(gSumatraPDFWebWnd->hwnd);
        if (gSumatraPDFWebWnd->edit) {
            HwndSetFocus(gSumatraPDFWebWnd->edit->hwnd);
        }
        return;
    }
    auto* wnd = new SumatraPDFWebWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearSumatraPDFWebWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gSumatraPDFWebWnd = wnd;
    wnd->StartList();
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}

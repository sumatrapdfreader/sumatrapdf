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
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "shared/SumatraPDFWebCommon.h"

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
    TempStr url = OnlineLoginUrlTemp();
    if (len(url) == 0) {
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
    SetStatus(OnlineVerifyMsgTemp(res));
    UpdateFileButtons();
}

static void VerifyOnlineKeyDone(OnlineVerifyResult* res) {
    SumatraPDFWebWnd* wnd = gSumatraPDFWebWnd;
    if (!wnd || wnd->reqId != res->id) {
        return;
    }
    wnd->FinishVerify(res);
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
    TempStr err;
    if (!StartOnlineVerify(token, &reqId, MkFunc1Void<OnlineVerifyResult*>(VerifyOnlineKeyDone), &err)) {
        SetStatus(err);
        return;
    }
    inFlight = true;
    UpdateFileButtons();
}

static void OnlineJobDone(OnlineJobResult* res);

void SumatraPDFWebWnd::StartList() {
    if (inFlight || !gSettings || !IsOnlineApiKey(gSettings->onlineApiKey)) {
        return;
    }
    bool hadStatus = status && len(status->s) > 0;
    TempStr err;
    if (!StartOnlineList(gSettings->onlineApiKey, &reqId, MkFunc1Void<OnlineJobResult*>(OnlineJobDone), &err)) {
        SetStatus(err);
        return;
    }
    inFlight = true;
    if (!hadStatus) {
        SetStatus(Tr("Contacting SumatraPDF Web..."));
        clearListStatus = true;
    } else {
        clearListStatus = false;
    }
    UpdateFileButtons();
}

// true when the caller should stop. A 401 keeps the saved key.
bool SumatraPDFWebWnd::ShowTransportError(OnlineJobResult* res) {
    TempStr msg;
    bool stop = OnlineJobFailed(res, &lastHttpStatus, &msg);
    if (len(msg) > 0) {
        SetStatus(msg);
    }
    return stop;
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
    TempStr why = CheckOnlineUploadTemp(pathOwned);
    if (len(why) > 0) {
        SetStatus(why);
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

    TempStr err;
    if (!StartOnlineUpload(gSettings->onlineApiKey, pathOwned, &reqId, MkFunc1Void<OnlineJobResult*>(OnlineJobDone),
                           &err)) {
        SetStatus(err);
        return;
    }
    inFlight = true;
    UpdateFileButtons();
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
    TempStr err;
    if (!StartOnlineDownload(gSettings->onlineApiKey, name, fileSizes[sel], &reqId,
                             MkFunc1Void<OnlineJobResult*>(OnlineJobDone), &err)) {
        SetStatus(err);
        return;
    }
    inFlight = true;
    UpdateFileButtons();
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
    if (!CreateToolDialog(this, win ? win->hwndFrame : nullptr, Tr("Link SumatraPDF Web"), fontText, colTxt, colBg)) {
        return false;
    }

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

    SizeToLayout(this, layout, DpiScale(520), 0, win ? win->hwndFrame : nullptr);
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

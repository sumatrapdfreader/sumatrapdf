/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/File.h"
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
#include "DocController.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "SumatraDialogs.h"

extern bool SaveAnnotationsToMaybeNewPdfFile(WindowTab*);

// Fills in a signature field with a certificate from a .pfx / .p12 file.
// Same WindowBase layout pattern as Add Favorite / Change Color.
struct SignDocumentWnd : WindowBase {
    ~SignDocumentWnd() override = default;

    MainWindow* win = nullptr;
    // unsigned signature fields the document already has; the placement
    // drop-down lists them first, then "new signature on the current page"
    StrVec fieldNames;
    Vec<int> fieldPages;
    int currPageNo = 1;

    Edit* editCert = nullptr;
    Edit* editPassword = nullptr;
    Edit* editReason = nullptr;
    Edit* editLocation = nullptr;
    DropDown* ddPlacement = nullptr;
    VirtButton* btnBrowse = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnSign = nullptr;

    bool Create(MainWindow* win);
    void CollectFields();
    void FillPlacement();
    bool BuildSignArgs(PdfSignArgs& args);

    void OnBrowse(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnSign(VirtMouseEvent* ev = nullptr);
};

static SignDocumentWnd* gSignDocumentWnd = nullptr;

static void ClearSignDocumentWnd() {
    gSignDocumentWnd = nullptr;
}

static EngineBase* GetPdfEngine(MainWindow* win) {
    if (!IsMainWindowValid(win) || !win->IsDocLoaded()) {
        return nullptr;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !EngineMupdfSupportsAnnotations(engine)) {
        return nullptr;
    }
    return engine;
}

// Bounding box of the selection on pageNo, so a signature can be placed by
// selecting where it should go. Empty if nothing is selected on that page.
static RectF SelectionRectOnPage(WindowTab* tab, int pageNo) {
    RectF res;
    if (!tab || !tab->selectionOnPage) {
        return res;
    }
    for (auto& sel : *tab->selectionOnPage) {
        if (sel.pageNo != pageNo || sel.rect.IsEmpty()) {
            continue;
        }
        res = res.IsEmpty() ? sel.rect : res.Union(sel.rect);
    }
    return res;
}

void SignDocumentWnd::CollectFields() {
    fieldNames.Reset();
    fieldPages.Reset();
    currPageNo = 1;
    EngineBase* engine = GetPdfEngine(win);
    if (!engine) {
        return;
    }
    if (win->ctrl) {
        currPageNo = win->ctrl->CurrentPageNo();
    }
    EngineMupdfGetUnsignedSignatureFields(engine, fieldNames, fieldPages);
}

void SignDocumentWnd::FillPlacement() {
    if (!ddPlacement) {
        return;
    }
    StrVec items;
    for (int i = 0; i < len(fieldNames); i++) {
        Str name = fieldNames[i];
        if (str::IsEmptyOrWhiteSpace(name)) {
            name = _TRA("Signature");
        }
        items.Append(fmt(_TRA("Empty signature field: %s (page %d)").s, name, fieldPages[i]));
    }
    items.Append(fmt(_TRA("New signature on page %d").s, currPageNo));
    ddPlacement->SetItems(items);
    ddPlacement->SetCurrentSelection(0);
}

// Turns the dialog state into what the engine needs; false if something the
// user has to fix is missing.
bool SignDocumentWnd::BuildSignArgs(PdfSignArgs& args) {
    TempStr certPath = editCert ? editCert->GetTextTemp() : Str{};
    str::TrimWSInPlace(certPath, str::TrimOpt::Both);
    if (len(certPath) == 0) {
        MessageBoxWarning(hwnd, _TRA("Please choose the certificate file to sign with."), _TRA("Sign Document"));
        return false;
    }
    if (!file::Exists(certPath)) {
        MessageBoxWarning(hwnd, fmt(_TRA("Certificate file %s doesn't exist.").s, certPath), _TRA("Sign Document"));
        return false;
    }
    args.certPath = certPath;
    args.certPassword = editPassword ? editPassword->GetTextTemp() : Str{};
    args.reason = editReason ? editReason->GetTextTemp() : Str{};
    args.location = editLocation ? editLocation->GetTextTemp() : Str{};
    str::TrimWSInPlace(args.reason, str::TrimOpt::Both);
    str::TrimWSInPlace(args.location, str::TrimOpt::Both);
    // an empty (but non-null) string would draw a "Reason:" label with nothing
    // after it in the signature
    if (len(args.reason) == 0) {
        args.reason = {};
    }
    if (len(args.location) == 0) {
        args.location = {};
    }

    int idx = ddPlacement ? ddPlacement->GetCurrentSelection() : -1;
    if (idx >= 0 && idx < len(fieldNames)) {
        args.fieldName = fieldNames[idx];
        args.pageNo = fieldPages[idx];
        return true;
    }
    // a new field on the current page, where the selection is if there is one
    args.pageNo = currPageNo;
    args.rect = SelectionRectOnPage(win ? win->CurrentTab() : nullptr, currPageNo);
    return true;
}

void SignDocumentWnd::OnBrowse(VirtMouseEvent*) {
    WCHAR fileName[MAX_PATH + 1]{};
    TempStr curr = editCert ? editCert->GetTextTemp() : Str{};
    if (len(curr) > 0 && len(curr) < MAX_PATH) {
        wstr::BufSet(WStr(fileName, dimof(fileName)), ToWStrTemp(curr));
    }

    str::Builder fileFilter(256);
    fileFilter.Append(_TRA("Certificate files"));
    fileFilter.Append("\1*.pfx;*.p12\1");
    fileFilter.Append(_TRA("All files"));
    fileFilter.Append("\1*.*\1");
    Str fileFilterStr = ToStr(fileFilter);
    str::TransCharsInPlace(fileFilterStr, StrL("\1"), StrL("\0"));

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = dimof(fileName);
    ofn.lpstrFilter = CWStrTemp(fileFilterStr);
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    str::Free(fileFilterStr);
    if (!GetOpenFileNameW(&ofn)) {
        return;
    }
    if (editCert) {
        editCert->SetText(ToUtf8Temp(WStr(fileName)));
    }
    if (editPassword) {
        HwndSetFocus(editPassword->hwnd);
    }
}

void SignDocumentWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

// mupdf reports a certificate it can't open as a raw Win32 failure
// ("PFXImportCertStore failed (gle=86)"). A mistyped password is by far the
// most likely cause of gle=86 (ERROR_INVALID_PASSWORD), so say that instead.
static TempStr SignErrorMessageTemp(Str err) {
    if (len(err) == 0) {
        return _TRA("Could not sign the document.");
    }
    if (str::Contains(err, StrL("PFXImportCertStore"))) {
        if (str::Contains(err, StrL("gle=86"))) {
            return _TRA("Wrong password for the certificate file.");
        }
        return fmt(_TRA("Could not read the certificate file: %s").s, err);
    }
    return str::DupTemp(err);
}

void SignDocumentWnd::OnSign(VirtMouseEvent*) {
    EngineBase* engine = GetPdfEngine(win);
    if (!engine) {
        ScheduleDelete();
        return;
    }
    PdfSignArgs args;
    if (!BuildSignArgs(args)) {
        return;
    }

    Str err;
    bool ok = EngineMupdfSignDocument(engine, args, &err);
    if (!ok) {
        MessageBoxWarning(hwnd, SignErrorMessageTemp(err), _TRA("Sign Document"));
        str::Free(err);
        return;
    }
    str::Free(err);

    // the signature is only computed while saving, so the document has to be
    // written out now; ask where, since signing rewrites the file
    WindowTab* tab = win->CurrentTab();
    ScheduleDelete();
    SaveAnnotationsToMaybeNewPdfFile(tab);
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gSignDocumentWnd) {
        gSignDocumentWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gSignDocumentWnd) {
        gSignDocumentWnd->ScheduleDelete();
    }
}

// label above a field, matching the spacing the other dialogs use
static VirtText* AddLabel(VBox* vbox, Str s, PlatformFont* font, bool isRtl, int padTop) {
    auto* c = NewVirtText({
        .s = s,
        .font = font,
        .isRtl = isRtl,
        .prefix = true,
        .padding = DpiScaledInsets(padTop, 0, 4, 0),
    });
    vbox->AddChild(c);
    return c;
}

static Edit* AddEdit(VBox* vbox, HWND hwnd, PlatformFont* font, bool isRtl, bool isPassword) {
    Edit::CreateArgs args;
    args.parent = hwnd;
    args.font = font;
    args.withBorder = true;
    args.isRtl = isRtl;
    args.isPassword = isPassword;
    auto* c = new Edit();
    c->Create(args);
    vbox->AddChild(c);
    return c;
}

bool SignDocumentWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    CollectFields();

    {
        CreateCustomArgs args;
        args.title = _TRA("Sign Document");
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
    PlatformFont* f = GetFont();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    AddLabel(vbox, _TRA("&Certificate file (.pfx, .p12):"), f, isRtl, 0);
    {
        auto* row = new HBox();
        row->alignMain = MainAxisAlign::MainStart;
        row->alignCross = CrossAxisAlign::CrossCenter;
        row->gap = f->averageCharWidth;

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = f;
        args.withBorder = true;
        args.isRtl = isRtl;
        args.idealWidthChars = 40;
        auto* e = new Edit();
        e->Create(args);
        editCert = e;
        row->AddChild(e);

        btnBrowse = NewThemedButton(hwnd, _TRA("&Browse..."), f, false);
        btnBrowse->onClick = MkMethod1<SignDocumentWnd, VirtMouseEvent*, &SignDocumentWnd::OnBrowse>(this);
        row->AddChild(btnBrowse);
        vbox->AddChild(row);
    }

    AddLabel(vbox, _TRA("&Password:"), f, isRtl, 8);
    editPassword = AddEdit(vbox, hwnd, f, isRtl, true);

    AddLabel(vbox, _TRA("&Reason (optional):"), f, isRtl, 8);
    editReason = AddEdit(vbox, hwnd, f, isRtl, false);

    AddLabel(vbox, _TRA("&Location (optional):"), f, isRtl, 8);
    editLocation = AddEdit(vbox, hwnd, f, isRtl, false);

    AddLabel(vbox, _TRA("&Where to sign:"), f, isRtl, 8);
    {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = f;
        args.isRtl = isRtl;
        auto* c = new DropDown();
        c->Create(args);
        ddPlacement = c;
        vbox->AddChild(c);
        FillPlacement();
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = f->averageCharWidth;
        auto pad = Insets{12, 0, 4, 0};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), f, false);
        btnCancel->onClick = MkMethod1<SignDocumentWnd, VirtMouseEvent*, &SignDocumentWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnSign = NewThemedButton(hwnd, _TRA("Sign"), f, true);
        btnSign->onClick = MkMethod1<SignDocumentWnd, VirtMouseEvent*, &SignDocumentWnd::OnSign>(this);
        hbox->AddChild(new Padding(btnSign, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(460);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (editCert) {
        HwndSetFocus(editCert->hwnd);
    }
    return true;
}

void ShowSignDocumentDialog(MainWindow* win) {
    if (!GetPdfEngine(win)) {
        return;
    }
    if (gSignDocumentWnd) {
        HwndSetFocus(gSignDocumentWnd->hwnd);
        return;
    }
    auto* wnd = new SignDocumentWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearSignDocumentWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gSignDocumentWnd = wnd;
}

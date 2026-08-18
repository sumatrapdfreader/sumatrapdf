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
#include "Notifications.h"
#include "DarkMode_win.h"
#include "SumatraDialogs.h"

extern bool SaveAnnotationsToMaybeNewPdfFile(WindowTab*);

// Fills in a signature field with a certificate from a .pfx / .p12 file.
// Same WindowBase layout pattern as Add Favorite / Change Color.
struct SignDocumentWnd : WindowBase {
    ~SignDocumentWnd() override { str::Free(preselectField); }

    MainWindow* win = nullptr;
    // unsigned signature fields the document already has; the placement
    // drop-down lists them first, then "new signature on the current page"
    StrVec fieldNames;
    Vec<int> fieldPages;
    int currPageNo = 1;
    // field the user clicked, so the drop-down opens on it rather than on the
    // first unsigned field in the document (issue #5964). Empty = no preference
    Str preselectField;
    bool hasPreselect = false;

    // CurrentUser\MY certs that can sign; the drop-down lists these, then
    // "Certificate file..." which uses editCert / editPassword instead
    StrVec certThumbs;
    DropDown* ddCert = nullptr;
    Edit* editCert = nullptr;
    Edit* editPassword = nullptr;
    Edit* editReason = nullptr;
    Edit* editLocation = nullptr;
    DropDown* ddPlacement = nullptr;
    VirtButton* btnBrowse = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnSign = nullptr;
    // appearance (issue #5963): which bits of the cert / labels to draw, and
    // an optional image that replaces the large name on the left
    Checkbox* cbShowLabels = nullptr;
    Checkbox* cbShowName = nullptr;
    Checkbox* cbShowDN = nullptr;
    Checkbox* cbShowDate = nullptr;
    Checkbox* cbShowGraphicName = nullptr;
    Edit* editImage = nullptr;
    VirtButton* btnBrowseImage = nullptr;

    bool Create(MainWindow* win);
    void CollectFields();
    void FillCertificates();
    void FillPlacement();
    void FillAppearance();
    bool UsingCertFile() const;
    void UpdateCertFileEnabled();
    void UpdateGraphicNameEnabled();
    bool BuildSignArgs(PdfSignArgs& args);
    void DoSign(const PdfSignArgs& args);

    void OnCertChanged();
    void OnImageChanged();
    void OnBrowse(VirtMouseEvent* ev = nullptr);
    void OnBrowseImage(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnSign(VirtMouseEvent* ev = nullptr);
};

// last appearance the user signed with, so the next Sign Document opens
// the same way (this process only; not written to settings)
static int gLastAppearanceFlags = -1;
static Str gLastImagePath;

static SignDocumentWnd* gSignDocumentWnd = nullptr;
// true while the Sign Document dialog is hidden and the next click / drag on
// the page will place a new signature (issue #5967)
static bool gPlacingSignature = false;
static Kind kNotifSignPlacement = "notifSignPlacement";
static void ClearSignaturePlacementNotif(MainWindow* win);

// Default size of a new signature when the user clicks rather than dragging
// a rectangle. 2" x 0.75" at 72 pt/in — enough for name, date and reason.
constexpr float kDefaultSignatureDx = 144;
constexpr float kDefaultSignatureDy = 54;

static void ClearSignDocumentWnd() {
    gPlacingSignature = false;
    gSignDocumentWnd = nullptr;
}

void CloseSignDocumentDialog(MainWindow* win) {
    if (!gSignDocumentWnd) {
        return;
    }
    if (win && gSignDocumentWnd->win != win) {
        return;
    }
    gPlacingSignature = false;
    ClearSignaturePlacementNotif(gSignDocumentWnd->win);
    gSignDocumentWnd->ScheduleDelete();
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

// Bounding box of the current selection. Sets pageNoOut to the page of the
// first non-empty piece. Empty if nothing is selected.
static RectF SelectionRect(WindowTab* tab, int* pageNoOut) {
    RectF res;
    if (!tab || !tab->selectionOnPage) {
        return res;
    }
    for (auto& sel : *tab->selectionOnPage) {
        if (sel.rect.IsEmpty()) {
            continue;
        }
        if (res.IsEmpty()) {
            if (pageNoOut) {
                *pageNoOut = sel.pageNo;
            }
            res = sel.rect;
        } else if (!pageNoOut || sel.pageNo == *pageNoOut) {
            res = res.Union(sel.rect);
        }
    }
    return res;
}

static void ClearSignaturePlacementNotif(MainWindow* win) {
    if (win && win->hwndCanvas) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifSignPlacement);
    }
}

static void ShowSignaturePlacementNotif(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = _TRA("Click or drag on the page to place the signature. Esc to cancel.");
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = kNotifSignPlacement;
    args.corner = NotifCorner::BottomBar;
    args.warning = true;
    args.tab = win->CurrentTab();
    ShowNotification(args);
}

bool IsPlacingSignature(MainWindow* win) {
    return gPlacingSignature && gSignDocumentWnd && gSignDocumentWnd->win == win;
}

// Leaves placement mode. Returns true if it was active. The hidden dialog is
// shown again so the user can change the certificate or cancel.
bool CancelPlacingSignature(MainWindow* win) {
    if (!IsPlacingSignature(win)) {
        if (gPlacingSignature) {
            gPlacingSignature = false;
        }
        return false;
    }
    gPlacingSignature = false;
    ClearSignaturePlacementNotif(win);
    if (gSignDocumentWnd) {
        gSignDocumentWnd->SetIsVisible(true);
        HwndSetFocus(gSignDocumentWnd->hwnd);
    }
    return true;
}

static void StartSignaturePlacement(SignDocumentWnd* wnd) {
    if (!wnd || !wnd->win) {
        return;
    }
    gPlacingSignature = true;
    wnd->SetIsVisible(false);
    DeleteOldSelectionInfo(wnd->win, true);
    ShowSignaturePlacementNotif(wnd->win);
    HwndSetFocus(wnd->win->hwndFrame);
}

static RectF ClampRectToPage(RectF r, RectF page) {
    if (page.IsEmpty()) {
        return r;
    }
    if (r.dx > page.dx) {
        r.dx = page.dx;
    }
    if (r.dy > page.dy) {
        r.dy = page.dy;
    }
    if (r.x < page.x) {
        r.x = page.x;
    }
    if (r.y < page.y) {
        r.y = page.y;
    }
    if (r.x + r.dx > page.x + page.dx) {
        r.x = page.x + page.dx - r.dx;
    }
    if (r.y + r.dy > page.y + page.dy) {
        r.y = page.y + page.dy - r.dy;
    }
    return r;
}

// A default-size box centered on the click, kept on the page.
static RectF DefaultSignatureRectAt(DisplayModel* dm, int pageNo, PointF pt) {
    RectF r(pt.x - kDefaultSignatureDx / 2, pt.y - kDefaultSignatureDy / 2, kDefaultSignatureDx, kDefaultSignatureDy);
    PageInfo* pi = dm ? dm->GetPageInfo(pageNo) : nullptr;
    if (!pi || !IsMediaBoxKnown(pi->mediaBox)) {
        return r;
    }
    return ClampRectToPage(r, pi->mediaBox);
}

// The click or drag that places a new signature. aborted is a click (no drag).
// Returns true if this press belonged to placement (even if we keep waiting).
bool FinishSignaturePlacement(MainWindow* win, int x, int y, bool aborted) {
    if (!IsPlacingSignature(win) || !gSignDocumentWnd) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        CancelPlacingSignature(win);
        return true;
    }

    int pageNo = gSignDocumentWnd->currPageNo;
    RectF rect;
    if (aborted) {
        Point pt(x, y);
        pageNo = dm->GetPageNoByPoint(pt);
        if (!dm->ValidPageNo(pageNo)) {
            return true; // click off the page: keep waiting
        }
        rect = DefaultSignatureRectAt(dm, pageNo, dm->CvtFromScreen(pt, pageNo));
    } else {
        rect = SelectionRect(win->CurrentTab(), &pageNo);
        if (rect.IsEmpty() || rect.dx < 8 || rect.dy < 8) {
            Point pt(x, y);
            int clickPage = dm->GetPageNoByPoint(pt);
            if (!dm->ValidPageNo(clickPage)) {
                return true;
            }
            pageNo = clickPage;
            rect = DefaultSignatureRectAt(dm, pageNo, dm->CvtFromScreen(pt, pageNo));
        }
    }
    if (rect.IsEmpty()) {
        return true;
    }

    PdfSignArgs args;
    if (!gSignDocumentWnd->BuildSignArgs(args)) {
        CancelPlacingSignature(win);
        return true;
    }
    args.pageNo = pageNo;
    args.rect = rect;
    args.fieldName = {};

    gPlacingSignature = false;
    ClearSignaturePlacementNotif(win);
    DeleteOldSelectionInfo(win, true);
    gSignDocumentWnd->DoSign(args);
    return true;
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

void SignDocumentWnd::FillCertificates() {
    if (!ddCert) {
        return;
    }
    certThumbs.Reset();
    StrVec labels;
    ListWindowsSigningCertificates(certThumbs, labels);
    StrVec items;
    for (int i = 0; i < len(labels); i++) {
        items.Append(labels[i]);
    }
    items.Append(_TRA("Certificate file..."));
    ddCert->SetItems(items);
    // a store cert if we have one; otherwise the file picker
    ddCert->SetCurrentSelection(len(certThumbs) > 0 ? 0 : len(items) - 1);
    UpdateCertFileEnabled();
}

bool SignDocumentWnd::UsingCertFile() const {
    int idx = ddCert ? ddCert->GetCurrentSelection() : -1;
    return idx < 0 || idx >= len(certThumbs);
}

void SignDocumentWnd::UpdateCertFileEnabled() {
    bool file = UsingCertFile();
    if (editCert) {
        editCert->SetIsEnabled(file);
    }
    if (btnBrowse) {
        btnBrowse->SetIsEnabled(file);
    }
    if (editPassword) {
        editPassword->SetIsEnabled(file);
    }
}

void SignDocumentWnd::OnCertChanged() {
    UpdateCertFileEnabled();
}

void SignDocumentWnd::UpdateGraphicNameEnabled() {
    bool hasImage = false;
    if (editImage) {
        TempStr path = editImage->GetTextTemp();
        str::TrimWSInPlace(path, str::TrimOpt::Both);
        hasImage = len(path) > 0;
    }
    if (cbShowGraphicName) {
        cbShowGraphicName->SetIsEnabled(!hasImage);
        if (hasImage) {
            cbShowGraphicName->SetIsChecked(false);
        }
    }
}

void SignDocumentWnd::OnImageChanged() {
    UpdateGraphicNameEnabled();
}

void SignDocumentWnd::FillAppearance() {
    int flags = gLastAppearanceFlags >= 0 ? gLastAppearanceFlags : kPdfSignDefaultAppearance;
    if (cbShowLabels) {
        cbShowLabels->SetIsChecked((flags & kPdfSignShowLabels) != 0);
    }
    if (cbShowName) {
        cbShowName->SetIsChecked((flags & kPdfSignShowTextName) != 0);
    }
    if (cbShowDN) {
        cbShowDN->SetIsChecked((flags & kPdfSignShowDN) != 0);
    }
    if (cbShowDate) {
        cbShowDate->SetIsChecked((flags & kPdfSignShowDate) != 0);
    }
    if (cbShowGraphicName) {
        cbShowGraphicName->SetIsChecked((flags & kPdfSignShowGraphicName) != 0);
    }
    if (editImage && gLastImagePath) {
        editImage->SetText(gLastImagePath);
    }
    UpdateGraphicNameEnabled();
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
    int sel = 0;
    if (hasPreselect) {
        for (int i = 0; i < len(fieldNames); i++) {
            if (str::Eq(fieldNames[i], preselectField)) {
                sel = i;
                break;
            }
        }
    }
    ddPlacement->SetCurrentSelection(sel);
}

// Turns the dialog state into what the engine needs; false if something the
// user has to fix is missing.
bool SignDocumentWnd::BuildSignArgs(PdfSignArgs& args) {
    if (UsingCertFile()) {
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
    } else {
        int idx = ddCert->GetCurrentSelection();
        args.certThumbprint = certThumbs[idx];
    }
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

    TempStr imagePath = editImage ? editImage->GetTextTemp() : Str{};
    str::TrimWSInPlace(imagePath, str::TrimOpt::Both);
    if (len(imagePath) > 0) {
        if (!file::Exists(imagePath)) {
            MessageBoxWarning(hwnd, fmt(_TRA("Image file %s doesn't exist.").s, imagePath), _TRA("Sign Document"));
            return false;
        }
        args.imagePath = imagePath;
    }

    int flags = 0;
    if (cbShowLabels && cbShowLabels->IsChecked()) {
        flags |= kPdfSignShowLabels;
    }
    if (cbShowName && cbShowName->IsChecked()) {
        flags |= kPdfSignShowTextName;
    }
    if (cbShowDN && cbShowDN->IsChecked()) {
        flags |= kPdfSignShowDN;
    }
    if (cbShowDate && cbShowDate->IsChecked()) {
        flags |= kPdfSignShowDate;
    }
    if (!args.imagePath && cbShowGraphicName && cbShowGraphicName->IsChecked()) {
        flags |= kPdfSignShowGraphicName;
    }
    // an empty appearance (no text bits and no reason/location) would draw a
    // blank box; keep the name so something is visible, like mupdf-gl
    if ((flags & (kPdfSignShowTextName | kPdfSignShowDN | kPdfSignShowDate)) == 0 && !args.reason && !args.location) {
        flags |= kPdfSignShowLabels | kPdfSignShowTextName;
    }
    args.appearanceFlags = flags;

    int idx = ddPlacement ? ddPlacement->GetCurrentSelection() : -1;
    if (idx >= 0 && idx < len(fieldNames)) {
        args.fieldName = fieldNames[idx];
        args.pageNo = fieldPages[idx];
        return true;
    }
    // a new field: use the selection if there is one, otherwise the caller
    // will ask the user to click or drag on the page (issue #5967)
    args.pageNo = currPageNo;
    int selPage = currPageNo;
    args.rect = SelectionRect(win ? win->CurrentTab() : nullptr, &selPage);
    if (!args.rect.IsEmpty()) {
        args.pageNo = selPage;
    }
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

void SignDocumentWnd::OnBrowseImage(VirtMouseEvent*) {
    WCHAR fileName[MAX_PATH + 1]{};
    TempStr curr = editImage ? editImage->GetTextTemp() : Str{};
    if (len(curr) > 0 && len(curr) < MAX_PATH) {
        wstr::BufSet(WStr(fileName, dimof(fileName)), ToWStrTemp(curr));
    }

    str::Builder fileFilter(256);
    fileFilter.Append(_TRA("Image files"));
    fileFilter.Append("\1*.png;*.jpg;*.jpeg\1");
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
    if (editImage) {
        editImage->SetText(ToUtf8Temp(WStr(fileName)));
    }
    UpdateGraphicNameEnabled();
}

void SignDocumentWnd::OnCancel(VirtMouseEvent*) {
    gPlacingSignature = false;
    ClearSignaturePlacementNotif(win);
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
    if (str::Contains(err, StrL("not found in the Windows certificate store")) ||
        str::Contains(err, StrL("invalid certificate thumbprint"))) {
        return _TRA("Could not use that certificate from the Windows certificate store.");
    }
    if (str::Contains(err, StrL("could not read signature image")) || str::Contains(err, StrL("cannot create image")) ||
        str::Contains(err, StrL("unknown image format"))) {
        return _TRA("Could not read the signature image.");
    }
    return str::DupTemp(err);
}

void SignDocumentWnd::DoSign(const PdfSignArgs& args) {
    EngineBase* engine = GetPdfEngine(win);
    if (!engine) {
        ScheduleDelete();
        return;
    }

    Str err;
    bool ok = EngineMupdfSignDocument(engine, args, &err);
    if (!ok) {
        SetIsVisible(true);
        HwndSetFocus(hwnd);
        MessageBoxWarning(hwnd, SignErrorMessageTemp(err), _TRA("Sign Document"));
        str::Free(err);
        return;
    }
    str::Free(err);

    gLastAppearanceFlags = args.appearanceFlags;
    str::ReplaceWithCopy(&gLastImagePath, args.imagePath);

    // the signature is only computed while saving, so the document has to be
    // written out now; ask where, since signing rewrites the file
    WindowTab* tab = win->CurrentTab();
    ScheduleDelete();
    SaveAnnotationsToMaybeNewPdfFile(tab);
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
    // a new field with nowhere to put it: hide this dialog and let the user
    // click or drag on the page (issue #5967)
    if (!args.fieldName && args.rect.IsEmpty()) {
        StartSignaturePlacement(this);
        return;
    }
    DoSign(args);
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

static Checkbox* AddCheckbox(VBox* vbox, HWND hwnd, Str text, bool isRtl, int padTop) {
    Checkbox::CreateArgs args;
    args.parent = hwnd;
    args.text = text;
    args.isRtl = isRtl;
    auto* c = new Checkbox();
    c->SetInsetsPt(padTop, 0, 0, 0);
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

    AddLabel(vbox, _TRA("&Certificate:"), f, isRtl, 0);
    {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = f;
        args.isRtl = isRtl;
        auto* c = new DropDown();
        c->Create(args);
        ddCert = c;
        c->onSelectionChanged = MkMethod0<SignDocumentWnd, &SignDocumentWnd::OnCertChanged>(this);
        vbox->AddChild(c);
        FillCertificates();
    }
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
        UpdateCertFileEnabled();
    }

    AddLabel(vbox, _TRA("&Password:"), f, isRtl, 8);
    editPassword = AddEdit(vbox, hwnd, f, isRtl, true);
    UpdateCertFileEnabled();

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

    AddLabel(vbox, _TRA("Appearance:"), f, isRtl, 8);
    cbShowLabels = AddCheckbox(vbox, hwnd, _TRA("Show &labels"), isRtl, 2);
    cbShowName = AddCheckbox(vbox, hwnd, _TRA("Show &name"), isRtl, 2);
    cbShowDN = AddCheckbox(vbox, hwnd, _TRA("Show &DN"), isRtl, 2);
    cbShowDate = AddCheckbox(vbox, hwnd, _TRA("Show da&te"), isRtl, 2);
    cbShowGraphicName = AddCheckbox(vbox, hwnd, _TRA("Show name as &graphic"), isRtl, 2);
    AddLabel(vbox, _TRA("&Image (optional):"), f, isRtl, 8);
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
        editImage = e;
        e->onTextChanged = MkMethod0<SignDocumentWnd, &SignDocumentWnd::OnImageChanged>(this);
        row->AddChild(e);

        btnBrowseImage = NewThemedButton(hwnd, _TRA("C&hoose..."), f, false);
        btnBrowseImage->onClick = MkMethod1<SignDocumentWnd, VirtMouseEvent*, &SignDocumentWnd::OnBrowseImage>(this);
        row->AddChild(btnBrowseImage);
        vbox->AddChild(row);
    }
    FillAppearance();

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
    if (UsingCertFile() && editCert) {
        HwndSetFocus(editCert->hwnd);
    } else if (ddCert) {
        HwndSetFocus(ddCert->hwnd);
    }
    return true;
}

// fieldName selects that signature field in the placement drop-down; pass
// hasField = false (the default) to leave the choice at the first unsigned
// field, as the Sign Document command does.
void ShowSignDocumentDialog(MainWindow* win, Str fieldName, bool hasField) {
    if (!GetPdfEngine(win)) {
        return;
    }
    if (gSignDocumentWnd) {
        if (gPlacingSignature) {
            CancelPlacingSignature(win);
        }
        if (hasField) {
            str::ReplaceWithCopy(&gSignDocumentWnd->preselectField, fieldName);
            gSignDocumentWnd->hasPreselect = true;
            gSignDocumentWnd->FillPlacement();
        }
        gSignDocumentWnd->SetIsVisible(true);
        HwndSetFocus(gSignDocumentWnd->hwnd);
        return;
    }
    auto* wnd = new SignDocumentWnd();
    if (hasField) {
        str::ReplaceWithCopy(&wnd->preselectField, fieldName);
        wnd->hasPreselect = true;
    }
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

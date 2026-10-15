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
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "SignDocumentDialogCommon.h"

extern bool SaveAnnotationsToMaybeNewPdfFile(WindowTab*);

// Fills in a signature field with a certificate from a .pfx / .p12 file.
//
//   Certificate:          [Krzysztof Kowalczyk          v]
//                         C:\certs\me.pfx________  Browse...
//   Password:             ********_____________________
//   ---------------------------------------------- hairline
//   Reason (optional):    _____________________________
//   Location (optional):  _____________________________
//   Where to sign:        [New signature on page 1     v]
//   Signature type:       [Approval signature          v]
//                         [ ] Lock form fields after signing
//   ---------------------------------------------- hairline
//   Appearance
//   [x] Show labels       [x] Show name
//   [x] Show DN           [x] Show date
//   [x] Show name as graphic
//   Image (optional):     ______________________  Choose...
//                                  [Cancel] [Sign]
constexpr int kSignDlgDx = 500;
struct SignDocumentWnd : WindowBase {
    ~SignDocumentWnd() override { str::Free(preselectField); }

    MainWindow* win = nullptr;
    HWND hwndOwner = nullptr;
    bool modalDisablesOwner = false;
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
    // approval or a certifying signature (discussion #6315); certifying is
    // only offered while the document has no signature yet
    DropDown* ddType = nullptr;
    Vec<int> typeCodes; // what each ddType row means (SignatureTypeItems)
    bool docSigned = false;
    int initialCertify = 0; // what the drop-down opens on ([CmdSignDocument certify])
    Checkbox* cbLockFields = nullptr;
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
    void FillSignatureType();
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
static void ClearSignaturePlacementNotif(MainWindow* win);

static void SetSignDocumentOwnerEnabled(SignDocumentWnd* wnd, bool enabled) {
    if (wnd && wnd->modalDisablesOwner && wnd->hwndOwner && IsWindow(wnd->hwndOwner)) {
        EnableWindow(wnd->hwndOwner, enabled);
    }
}

static void ClearSignDocumentWnd() {
    gPlacingSignature = false;
    SetSignDocumentOwnerEnabled(gSignDocumentWnd, true);
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

static void ClearSignaturePlacementNotif(MainWindow* win) {
    if (win && win->hwndCanvas) {
        RemoveNotificationsForGroup(win, kNotifSignPlacement);
    }
}

NotifCorner SignPlacementNotifCorner() {
    return NotifCorner::BottomBar;
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
        SetSignDocumentOwnerEnabled(gSignDocumentWnd, false);
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
    SetSignDocumentOwnerEnabled(wnd, true);
    DeleteOldSelectionInfo(wnd->win, true);
    ShowSignaturePlacementNotif(wnd->win);
    HwndSetFocus(wnd->win->hwndFrame);
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
    SetSignDocumentOwnerEnabled(gSignDocumentWnd, false);
    gSignDocumentWnd->DoSign(args);
    return true;
}

void SignDocumentWnd::CollectFields() {
    fieldNames.Reset();
    VecReset(fieldPages);
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
    items.Append(Tr("Certificate file..."));
    ddCert->SetItems(items);
    // a store cert if we have one; otherwise the file picker
    CbSetCurrentSelection(ddCert, len(certThumbs) > 0 ? 0 : len(items) - 1);
    UpdateCertFileEnabled();
}

bool SignDocumentWnd::UsingCertFile() const {
    int idx = CbGetCurrentSelection(ddCert);
    return idx < 0 || idx >= len(certThumbs);
}

void SignDocumentWnd::UpdateCertFileEnabled() {
    bool file = UsingCertFile();
    if (editCert) {
        editCert->SetIsEnabled(file);
    }
    if (btnBrowse) {
        SetDlgButtonEnabled(btnBrowse, DlgButtonStyle::Ghost, file);
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
            name = Tr("Signature");
        }
        items.Append(fmt(Tr("Empty signature field: %s (page %d)").s, name, fieldPages[i]));
    }
    items.Append(fmt(Tr("New signature on page %d").s, currPageNo));
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
    CbSetCurrentSelection(ddPlacement, sel);
}

void SignDocumentWnd::FillSignatureType() {
    if (!ddType) {
        return;
    }
    EngineBase* engine = GetPdfEngine(win);
    docSigned = engine && EngineMupdfCountSignedSignatures(engine) > 0;
    StrVec items;
    VecReset(typeCodes);
    SignatureTypeItems(items, typeCodes, docSigned);
    ddType->SetItems(items);
    int sel = VecFind(typeCodes, initialCertify);
    CbSetCurrentSelection(ddType, sel >= 0 ? sel : 0);
}

// Turns the dialog state into what the engine needs; false if something the
// user has to fix is missing.
bool SignDocumentWnd::BuildSignArgs(PdfSignArgs& args) {
    if (UsingCertFile()) {
        TempStr certPath = editCert ? editCert->GetTextTemp() : Str{};
        str::TrimWSInPlace(certPath, str::TrimOpt::Both);
        if (len(certPath) == 0) {
            MessageBoxWarning(hwnd, Tr("Please choose the certificate file to sign with."), Tr("Sign Document"));
            return false;
        }
        if (!file::Exists(certPath)) {
            MessageBoxWarning(hwnd, fmt(Tr("Certificate file %s doesn't exist.").s, certPath), Tr("Sign Document"));
            return false;
        }
        args.certPath = certPath;
        args.certPassword = editPassword ? editPassword->GetTextTemp() : Str{};
    } else {
        int idx = CbGetCurrentSelection(ddCert);
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
            MessageBoxWarning(hwnd, fmt(Tr("Image file %s doesn't exist.").s, imagePath), Tr("Sign Document"));
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
    if (len(args.imagePath) == 0 && cbShowGraphicName && cbShowGraphicName->IsChecked()) {
        flags |= kPdfSignShowGraphicName;
    }
    // an empty appearance (no text bits and no reason/location) would draw a
    // blank box; keep the name so something is visible, like mupdf-gl
    if ((flags & (kPdfSignShowTextName | kPdfSignShowDN | kPdfSignShowDate)) == 0 && len(args.reason) == 0 &&
        len(args.location) == 0) {
        flags |= kPdfSignShowLabels | kPdfSignShowTextName;
    }
    args.appearanceFlags = flags;
    int typeIdx = ddType ? CbGetCurrentSelection(ddType) : -1;
    ApplySignatureType(args, (typeIdx >= 0 && typeIdx < len(typeCodes)) ? typeCodes[typeIdx] : 0);
    args.lockFields = cbLockFields && cbLockFields->IsChecked();

    int idx = CbGetCurrentSelection(ddPlacement);
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

    str::Builder fileFilter;
    fileFilter.Reserve(256);
    fileFilter.Append(Tr("Certificate files"));
    fileFilter.Append(StrL("\1*.pfx;*.p12\1"));
    fileFilter.Append(Tr("All files"));
    fileFilter.Append(StrL("\1*.*\1"));
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
    if (!GetOpenFileNameW(&ofn)) {
        return;
    }
    if (editCert) {
        editCert->SetText(ToUtf8Temp(WStr(fileName)));
    }
    EditSetFocus(editPassword);
}

void SignDocumentWnd::OnBrowseImage(VirtMouseEvent*) {
    WCHAR fileName[MAX_PATH + 1]{};
    TempStr curr = editImage ? editImage->GetTextTemp() : Str{};
    if (len(curr) > 0 && len(curr) < MAX_PATH) {
        wstr::BufSet(WStr(fileName, dimof(fileName)), ToWStrTemp(curr));
    }

    str::Builder fileFilter;
    fileFilter.Reserve(256);
    fileFilter.Append(Tr("Image files"));
    fileFilter.Append(StrL("\1*.png;*.jpg;*.jpeg\1"));
    fileFilter.Append(Tr("All files"));
    fileFilter.Append(StrL("\1*.*\1"));
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

void SignDocumentWnd::DoSign(const PdfSignArgs& args) {
    EngineBase* engine = GetPdfEngine(win);
    if (!engine) {
        ScheduleDelete();
        return;
    }

    if (SignTypeLocksDocument(args)) {
        UINT flags = MB_ICONEXCLAMATION | MB_YESNO | MbRtlReadingMaybe();
        if (IDNO == MsgBox(win->hwndFrame, LockDocumentWarning(), Tr("Sign Document"), flags)) {
            SetIsVisible(true);
            HwndSetFocus(hwnd);
            return;
        }
    }

    Str err;
    bool ok = EngineMupdfSignDocument(engine, args, &err);
    if (!ok) {
        SetIsVisible(true);
        HwndSetFocus(hwnd);
        MessageBoxWarning(hwnd, SignErrorMessageTemp(err), Tr("Sign Document"));
        str::Free(err);
        return;
    }
    str::Free(err);

    gLastAppearanceFlags = args.appearanceFlags;
    str::ReplaceWithCopy(&gLastImagePath, args.imagePath);

    // the signature is only computed while saving, so the document has to be
    // written out now; ask where, since signing rewrites the file
    WindowTab* tab = win->CurrentTab();
    SetIsVisible(false);
    SetSignDocumentOwnerEnabled(this, true);
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
    if (len(args.fieldName) == 0 && args.rect.IsEmpty()) {
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
bool SignDocumentWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    hwndOwner = win ? win->hwndFrame : nullptr;
    CollectFields();
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    PlatformFont* fontMeta = DlgUiFont(kToolDlgMetaFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = hwndOwner;
        args.title = Tr("Sign Document");
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

    // every field after a muted label, the labels in one column
    Vec<VirtText*> labels;
    auto newSection = [&]() {
        auto* box = new VBox();
        box->alignCross = CrossAxisAlign::Stretch;
        return box;
    };
    auto addRow = [&](VBox* box, Str label, ILayout* ctrl) {
        HBox* row = NewDlgFieldRow();
        VirtText* t = NewDlgLabel(label, fontText, colMuted, kToolDlgRowDy, true);
        VecAppend(labels, t);
        row->AddChild(t);
        row->AddChild(ctrl, 1);
        box->AddChild(row);
        return row;
    };
    auto newDropDown = [&]() {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = fontText;
        args.isRtl = isRtl;
        auto* c = new DropDown();
        c->Create(args);
        return c;
    };
    auto newEdit = [&](bool isPassword) {
        Edit* e = NewDlgEdit(hwnd, fontText, {}, 0);
        if (isPassword) {
            HwndSetWindowStyle(e->hwnd, ES_PASSWORD, true);
        }
        return e;
    };
    auto newCheck = [&](Str text) { return NewDlgCheckbox(hwnd, text, fontText, false); };

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;

    // the certificate: one of the store's, or a file and its password
    VBox* cert = newSection();
    ddCert = newDropDown();
    ddCert->onSelectionChanged = MkMethod0<SignDocumentWnd, &SignDocumentWnd::OnCertChanged>(this);
    addRow(cert, Tr("&Certificate:"), ddCert);
    FillCertificates();
    {
        editCert = newEdit(false);
        btnBrowse = NewDlgButton(hwnd, Tr("&Browse..."), fontText, DlgButtonStyle::Ghost, false);
        btnBrowse->onClick = MkMethod1<SignDocumentWnd, VirtMouseEvent*, &SignDocumentWnd::OnBrowse>(this);
        HBox* file = NewDlgRow();
        file->AddChild(NewDlgUnderlined(editCert), 1);
        file->AddChild(btnBrowse);
        addRow(cert, {}, file);
    }
    editPassword = newEdit(true);
    addRow(cert, Tr("&Password:"), NewDlgUnderlined(editPassword));
    UpdateCertFileEnabled();
    vbox->AddChild(NewDlgSection(cert));
    vbox->AddChild(new VirtLine());

    // what the signature says and where it goes
    VBox* details = newSection();
    editReason = newEdit(false);
    addRow(details, Tr("&Reason (optional):"), NewDlgUnderlined(editReason));
    editLocation = newEdit(false);
    addRow(details, Tr("&Location (optional):"), NewDlgUnderlined(editLocation));
    ddPlacement = newDropDown();
    addRow(details, Tr("&Where to sign:"), ddPlacement);
    FillPlacement();
    ddType = newDropDown();
    addRow(details, Tr("Signature &type:"), ddType);
    FillSignatureType();
    cbLockFields = newCheck(Tr("Lock form &fields after signing"));
    addRow(details, {}, cbLockFields);
    vbox->AddChild(NewDlgSection(details));
    vbox->AddChild(new VirtLine());

    // what the signature shows, two checkboxes a row
    VBox* look = newSection();
    look->AddChild(NewDlgLabel(Tr("Appearance:"), fontMeta, colMuted, kToolDlgRowDy));
    auto addPair = [&](Checkbox* left, Checkbox* right) {
        HBox* row = NewDlgFieldRow();
        row->AddChild(left, 1);
        if (right) {
            row->AddChild(right, 1);
        } else {
            row->AddChild(new Spacer(0, 0), 1);
        }
        look->AddChild(row);
    };
    cbShowLabels = newCheck(Tr("Show &labels"));
    cbShowName = newCheck(Tr("Show &name"));
    addPair(cbShowLabels, cbShowName);
    cbShowDN = newCheck(Tr("Show &DN"));
    cbShowDate = newCheck(Tr("Show da&te"));
    addPair(cbShowDN, cbShowDate);
    cbShowGraphicName = newCheck(Tr("Show name as &graphic"));
    addPair(cbShowGraphicName, nullptr);
    {
        editImage = newEdit(false);
        editImage->onTextChanged = MkMethod0<SignDocumentWnd, &SignDocumentWnd::OnImageChanged>(this);
        btnBrowseImage = NewDlgButton(hwnd, Tr("C&hoose..."), fontText, DlgButtonStyle::Ghost, false);
        btnBrowseImage->onClick = MkMethod1<SignDocumentWnd, VirtMouseEvent*, &SignDocumentWnd::OnBrowseImage>(this);
        HBox* image = NewDlgRow();
        image->AddChild(NewDlgUnderlined(editImage), 1);
        image->AddChild(btnBrowseImage);
        addRow(look, Tr("&Image (optional):"), image);
    }
    FillAppearance();
    vbox->AddChild(NewDlgSection(look));
    AlignDlgLabels(labels);

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<SignDocumentWnd, VirtMouseEvent*, &SignDocumentWnd::OnCancel>(this);
    btnSign = NewDlgButton(hwnd, Tr("Sign"), fontText, DlgButtonStyle::Filled, true);
    btnSign->onClick = MkMethod1<SignDocumentWnd, VirtMouseEvent*, &SignDocumentWnd::OnSign>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnSign));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kSignDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (UsingCertFile() && editCert) {
        EditSetFocus(editCert);
    } else if (ddCert) {
        HwndSetFocus(ddCert->hwnd);
    }
    return true;
}

// fieldName selects that signature field in the placement drop-down; pass
// hasField = false (the default) to leave the choice at the first unsigned
// field, as the Sign Document command does.
void ShowSignDocumentDialog(MainWindow* win, Str fieldName, bool hasField, int certifyLevel) {
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
        if (certifyLevel > 0) {
            gSignDocumentWnd->initialCertify = certifyLevel;
            gSignDocumentWnd->FillSignatureType();
        }
        gSignDocumentWnd->SetIsVisible(true);
        HwndSetFocus(gSignDocumentWnd->hwnd);
        return;
    }
    auto* wnd = new SignDocumentWnd();
    wnd->initialCertify = certifyLevel;
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
    wnd->modalDisablesOwner = wnd->hwndOwner && IsWindowEnabled(wnd->hwndOwner);
    SetSignDocumentOwnerEnabled(wnd, false);
}

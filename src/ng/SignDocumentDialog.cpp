/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Fills in a signature field with a certificate from the Windows store or a
// .pfx / .p12 file. ng: orig is a WS_POPUPWINDOW of virtual controls; here it
// is a gpui dialog with the same rows in the same order (certificate, the
// file + browse, password, reason, location, where to sign, the appearance
// check boxes and the optional image), and the same "hide the dialog and let
// the user click or drag on the page" placement mode.
// Signing uses wincrypt on Windows; with a certificate file, the Security
// framework on macOS and OpenSSL on Linux.

#include "gui/GpuiBridge.h"
#include "base/File.h"

#include "gui/UIModels.h"

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
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "Menu.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "SignDocumentDialogCommon.h"

#include "SumatraLog.h"

struct SignDocumentDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    // hidden while the user places the signature on the page
    bool placing = false;

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
    DialogSelect ddCert;
    DialogSelect ddPlacement;
    // approval or a certifying signature (discussion #6315); certifying is
    // only offered while the document has no signature yet
    DialogSelect ddType;
    Vec<int> typeCodes; // what each ddType row means (SignatureTypeItems)
    bool docSigned = false;
    int initialCertify = 0; // what the drop-down opens on ([CmdSignDocument certify])
    bool lockFields = false;
    gpui::InputState* editCert = nullptr;
    gpui::InputState* editPassword = nullptr;
    gpui::InputState* editReason = nullptr;
    gpui::InputState* editLocation = nullptr;
    gpui::InputState* editImage = nullptr;

    // appearance (issue #5963): which bits of the cert / labels to draw
    bool showLabels = false;
    bool showName = false;
    bool showDN = false;
    bool showDate = false;
    bool showGraphicName = false;
};

static SignDocumentDlg gSign;

// last appearance the user signed with, so the next Sign Document opens
// the same way (this process only; not written to settings)
static int gLastAppearanceFlags = -1;
static Str gLastImagePath;

struct SignDocumentView {
    static void OnSign(SignDocumentView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(SignDocumentView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnBrowse(SignDocumentView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnBrowseImage(SignDocumentView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnToggle(SignDocumentView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t which);
};

static gp::Entity<SignDocumentView> gSignView;

// orig's window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame, or the window is away while a signature is placed
static ToolWindow* gSignTw = nullptr;

// orig's layout: the certificate, the signature's details, its appearance;
// hairlines between, the labels in one column
constexpr float kSignWinDx = 500;

static void ClearSignaturePlacementNotif(MainWindow* win) {
    if (win) {
        RemoveNotificationsForGroup(win, kNotifSignPlacement);
    }
}

// ng shows it where notifications go by default
NotifCorner SignPlacementNotifCorner() {
    return NotificationCreateArgs().corner;
}

// the dialog in the frame; a window of its own is not the frame's business
bool IsSignDocumentDialogVisible() {
    return gSign.visible && !gSign.placing && !gSignTw;
}

// The placement drop-down's current label. Tests read this; ng has no ComboBox.
TempStr SignDocumentPlacementTemp() {
    if (!gSign.visible) {
        return {};
    }
    return str::DupTemp(gSign.ddPlacement.SelText());
}

// Appearance checkboxes as "Show labels=1" lines. Empty while the dialog is down.
TempStr SignDocumentChecksTemp() {
    if (!gSign.visible || gSign.placing) {
        return {};
    }
    str::Builder out;
    out.Append(fmt("Show labels=%d\n", gSign.showLabels ? 1 : 0));
    out.Append(fmt("Show name=%d\n", gSign.showName ? 1 : 0));
    out.Append(fmt("Show DN=%d\n", gSign.showDN ? 1 : 0));
    out.Append(fmt("Show date=%d\n", gSign.showDate ? 1 : 0));
    out.Append(fmt("Show name as graphic=%d\n", gSign.showGraphicName ? 1 : 0));
    return ToStrTemp(out);
}

static Str SignDocumentDlgTitle() {
    return Tr("Sign Document");
}

static void SignDocumentDlgClose() {
    CloseSignDocumentDialog(nullptr);
}

// orig hides its window and enables the main one while a signature is
// placed on the page, and shows it and disables the main one again after;
// so does this (ToolWindowSetVisible). Called whenever the dialog's state
// may have changed
static void SignDocumentSyncWindow() {
    if (!gSign.visible) {
        DlgWindowClose(&gSignTw);
        return;
    }
    // orig's SetIsVisible() + EnableWindow(owner): hidden for the placement
    // with the main window usable, back where it was afterwards
    bool want = !gSign.placing;
    if (gSignTw && ToolWindowIsLive(gSignTw)) {
        ToolWindowSetVisible(gSignTw, want);
        return;
    }
    if (!want) {
        return;
    }
    DlgWindowSpec spec;
    spec.name = "signdocument";
    spec.title = SignDocumentDlgTitle;
    spec.build = SignDocumentDialogBuild;
    spec.close = SignDocumentDlgClose;
    spec.clientDx = kSignWinDx;
    gSignTw = DlgWindowOpen(spec, gSign.win);
}

bool IsPlacingSignature(MainWindow* win) {
    return gSign.placing && gSign.visible && gSign.win == win;
}

static void FreeSignDialogState() {
    MainWindow* win = gSign.win;
    if (win && win->gpuiWin) {
        gp::InputBlur(gSign.editCert, win->gpuiWin->app, win->gpuiWin);
    }
    delete gSign.editCert;
    delete gSign.editPassword;
    delete gSign.editReason;
    delete gSign.editLocation;
    delete gSign.editImage;
    gSign.editCert = nullptr;
    gSign.editPassword = nullptr;
    gSign.editReason = nullptr;
    gSign.editLocation = nullptr;
    gSign.editImage = nullptr;
    gSign.ddCert.Free();
    gSign.ddPlacement.Free();
    gSign.ddType.Free();
    gSign.fieldNames.Reset();
    VecReset(gSign.fieldPages);
    gSign.certThumbs.Reset();
    str::ReplaceWithCopy(&gSign.preselectField, {});
    gSign.hasPreselect = false;
}

void CloseSignDocumentDialog(MainWindow* win) {
    if (!gSign.visible) {
        return;
    }
    if (win && gSign.win != win) {
        return;
    }
    gSign.placing = false;
    ClearSignaturePlacementNotif(gSign.win);
    gSign.visible = false;
    DlgWindowClose(&gSignTw);
    MainWindow* w = gSign.win;
    FreeSignDialogState();
    AppShellInvalidate(w);
}

// Leaves placement mode. Returns true if it was active. The hidden dialog is
// shown again so the user can change the certificate or cancel.
bool CancelPlacingSignature(MainWindow* win) {
    if (!IsPlacingSignature(win)) {
        gSign.placing = false;
        return false;
    }
    gSign.placing = false;
    ClearSignaturePlacementNotif(win);
    SignDocumentSyncWindow();
    AppShellInvalidate(win);
    return true;
}

static void StartSignaturePlacement() {
    MainWindow* win = gSign.win;
    if (!win) {
        return;
    }
    gSign.placing = true;
    SignDocumentSyncWindow();
    DeleteOldSelectionInfo(win, true);
    ShowSignaturePlacementNotif(win);
    AppShellInvalidate(win);
}

static void CollectFields(MainWindow* win) {
    gSign.fieldNames.Reset();
    VecReset(gSign.fieldPages);
    gSign.currPageNo = 1;
    EngineBase* engine = GetPdfEngine(win);
    if (!engine) {
        return;
    }
    if (win->ctrl) {
        gSign.currPageNo = win->ctrl->CurrentPageNo();
    }
    EngineMupdfGetUnsignedSignatureFields(engine, gSign.fieldNames, gSign.fieldPages);
}

static bool UsingCertFile() {
    int idx = gSign.ddCert.sel;
    return idx < 0 || idx >= len(gSign.certThumbs);
}

static void FillCertificates() {
    gSign.certThumbs.Reset();
    StrVec labels;
#if OS_WIN
    ListWindowsSigningCertificates(gSign.certThumbs, labels);
#endif
    StrVec items;
    for (int i = 0; i < len(labels); i++) {
        items.Append(labels[i]);
    }
    items.Append(Tr("Certificate file..."));
    // a store cert if we have one; otherwise the file picker
    gSign.ddCert.SetItems(items, len(gSign.certThumbs) > 0 ? 0 : len(items) - 1);
}

static void FillPlacement() {
    StrVec items;
    for (int i = 0; i < len(gSign.fieldNames); i++) {
        Str name = gSign.fieldNames[i];
        if (str::IsEmptyOrWhiteSpace(name)) {
            name = Tr("Signature");
        }
        items.Append(fmt(Tr("Empty signature field: %s (page %d)").s, name, gSign.fieldPages[i]));
    }
    items.Append(fmt(Tr("New signature on page %d").s, gSign.currPageNo));
    int sel = 0;
    if (gSign.hasPreselect) {
        for (int i = 0; i < len(gSign.fieldNames); i++) {
            if (str::Eq(gSign.fieldNames[i], gSign.preselectField)) {
                sel = i;
                break;
            }
        }
    }
    gSign.ddPlacement.SetItems(items, sel);
}

static void FillSignatureType() {
    EngineBase* engine = GetPdfEngine(gSign.win);
    gSign.docSigned = engine && EngineMupdfCountSignedSignatures(engine) > 0;
    StrVec items;
    VecReset(gSign.typeCodes);
    SignatureTypeItems(items, gSign.typeCodes, gSign.docSigned);
    int sel = VecFind(gSign.typeCodes, gSign.initialCertify);
    gSign.ddType.SetItems(items, sel >= 0 ? sel : 0);
}

static void FillAppearance() {
    int flags = gLastAppearanceFlags >= 0 ? gLastAppearanceFlags : kPdfSignDefaultAppearance;
    gSign.showLabels = (flags & kPdfSignShowLabels) != 0;
    gSign.showName = (flags & kPdfSignShowTextName) != 0;
    gSign.showDN = (flags & kPdfSignShowDN) != 0;
    gSign.showDate = (flags & kPdfSignShowDate) != 0;
    gSign.showGraphicName = (flags & kPdfSignShowGraphicName) != 0;
    if (gSign.editImage && len(gLastImagePath) > 0) {
        gp::InputSetValue(gSign.editImage, ToGpui(gLastImagePath));
    }
}

static TempStr TrimmedValueTemp(gpui::InputState* s) {
    if (!s) {
        return {};
    }
    TempStr v = str::DupTemp(FromGpui(gp::InputValue(s)));
    str::TrimWSInPlace(v, str::TrimOpt::Both);
    return v;
}

// Turns the dialog state into what the engine needs; false if something the
// user has to fix is missing.
static bool BuildSignArgs(PdfSignArgs& args) {
    MainWindow* win = gSign.win;
    if (UsingCertFile()) {
        TempStr certPath = TrimmedValueTemp(gSign.editCert);
        if (len(certPath) == 0) {
            MessageBoxWarning(win, Tr("Please choose the certificate file to sign with."), Tr("Sign Document"));
            return false;
        }
        if (!file::Exists(certPath)) {
            MessageBoxWarning(win, fmt(Tr("Certificate file %s doesn't exist.").s, certPath), Tr("Sign Document"));
            return false;
        }
        args.certPath = certPath;
        args.certPassword = str::DupTemp(FromGpui(gp::InputValue(gSign.editPassword)));
    } else {
        args.certThumbprint = gSign.certThumbs[gSign.ddCert.sel];
    }
    args.reason = TrimmedValueTemp(gSign.editReason);
    args.location = TrimmedValueTemp(gSign.editLocation);
    // an empty (but non-null) string would draw a "Reason:" label with nothing
    // after it in the signature
    if (len(args.reason) == 0) {
        args.reason = {};
    }
    if (len(args.location) == 0) {
        args.location = {};
    }

    TempStr imagePath = TrimmedValueTemp(gSign.editImage);
    if (len(imagePath) > 0) {
        if (!file::Exists(imagePath)) {
            MessageBoxWarning(win, fmt(Tr("Image file %s doesn't exist.").s, imagePath), Tr("Sign Document"));
            return false;
        }
        args.imagePath = imagePath;
    }

    int flags = 0;
    if (gSign.showLabels) {
        flags |= kPdfSignShowLabels;
    }
    if (gSign.showName) {
        flags |= kPdfSignShowTextName;
    }
    if (gSign.showDN) {
        flags |= kPdfSignShowDN;
    }
    if (gSign.showDate) {
        flags |= kPdfSignShowDate;
    }
    if (len(args.imagePath) == 0 && gSign.showGraphicName) {
        flags |= kPdfSignShowGraphicName;
    }
    // an empty appearance (no text bits and no reason/location) would draw a
    // blank box; keep the name so something is visible, like mupdf-gl
    if ((flags & (kPdfSignShowTextName | kPdfSignShowDN | kPdfSignShowDate)) == 0 && len(args.reason) == 0 &&
        len(args.location) == 0) {
        flags |= kPdfSignShowLabels | kPdfSignShowTextName;
    }
    args.appearanceFlags = flags;
    int typeIdx = gSign.ddType.sel;
    ApplySignatureType(args, (typeIdx >= 0 && typeIdx < len(gSign.typeCodes)) ? gSign.typeCodes[typeIdx] : 0);
    args.lockFields = gSign.lockFields;

    int idx = gSign.ddPlacement.sel;
    if (idx >= 0 && idx < len(gSign.fieldNames)) {
        args.fieldName = gSign.fieldNames[idx];
        args.pageNo = gSign.fieldPages[idx];
        return true;
    }
    // a new field: use the selection if there is one, otherwise the caller
    // will ask the user to click or drag on the page (issue #5967)
    args.pageNo = gSign.currPageNo;
    int selPage = gSign.currPageNo;
    args.rect = SelectionRect(win ? win->CurrentTab() : nullptr, &selPage);
    if (!args.rect.IsEmpty()) {
        args.pageNo = selPage;
    }
    return true;
}

static void DoSign(const PdfSignArgs& args);

// ng: the "no changes allowed" warning is a message box with a callback, so
// the arguments wait here (their strings are temp-allocated by the caller)
static PdfSignArgs gPendingSign;
static bool gPendingSignConfirmed = false;

static void FreePendingSign() {
    str::FreePtr(&gPendingSign.certPath);
    str::FreePtr(&gPendingSign.certPassword);
    str::FreePtr(&gPendingSign.certThumbprint);
    str::FreePtr(&gPendingSign.reason);
    str::FreePtr(&gPendingSign.location);
    str::FreePtr(&gPendingSign.fieldName);
    str::FreePtr(&gPendingSign.imagePath);
    gPendingSign = PdfSignArgs{};
}

static void OnCertifyAnswer(int res) {
    if (res == MbRetYes) {
        gPendingSignConfirmed = true;
        DoSign(gPendingSign);
        gPendingSignConfirmed = false;
    } else {
        gSign.placing = false;
        SignDocumentSyncWindow();
        AppShellInvalidate(gSign.win);
    }
    FreePendingSign();
}

static void DoSign(const PdfSignArgs& args) {
    MainWindow* win = gSign.win;
    EngineBase* engine = GetPdfEngine(win);
    if (!engine) {
        CloseSignDocumentDialog(win);
        return;
    }
    if (SignTypeLocksDocument(args) && !gPendingSignConfirmed) {
        FreePendingSign();
        gPendingSign = args;
        gPendingSign.certPath = str::Dup(args.certPath);
        gPendingSign.certPassword = str::Dup(args.certPassword);
        gPendingSign.certThumbprint = str::Dup(args.certThumbprint);
        gPendingSign.reason = str::Dup(args.reason);
        gPendingSign.location = str::Dup(args.location);
        gPendingSign.fieldName = str::Dup(args.fieldName);
        gPendingSign.imagePath = str::Dup(args.imagePath);
        MsgBox(win, LockDocumentWarning(), Tr("Sign Document"), MbYesNo | MbIconWarning, MkFunc1Void(OnCertifyAnswer));
        return;
    }
    Str err;
    bool ok = false;
#if OS_WIN || OS_DARWIN || defined(SUMATRA_HAVE_OPENSSL)
    ok = EngineMupdfSignDocument(engine, args, &err);
#else
    (void)args;
    err = str::Dup(StrL("signing is not available on this platform"));
#endif
    if (!ok) {
        gSign.placing = false;
        SignDocumentSyncWindow();
        AppShellInvalidate(win);
        MessageBoxWarning(win, SignErrorMessageTemp(err), Tr("Sign Document"));
        str::Free(err);
        return;
    }
    str::Free(err);

    gLastAppearanceFlags = args.appearanceFlags;
    str::ReplaceWithCopy(&gLastImagePath, args.imagePath);

    // the signature is only computed while saving, so the document has to be
    // written out now; ask where, since signing rewrites the file
    WindowTab* tab = win->CurrentTab();
    CloseSignDocumentDialog(win);
    SaveAnnotationsToMaybeNewPdfFile(tab);
}

// The click or drag that places a new signature. aborted is a click (no drag).
// Returns true if this press belonged to placement (even if we keep waiting).
bool FinishSignaturePlacement(MainWindow* win, int x, int y, bool aborted) {
    if (!IsPlacingSignature(win)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        CancelPlacingSignature(win);
        return true;
    }

    int pageNo = gSign.currPageNo;
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
    if (!BuildSignArgs(args)) {
        CancelPlacingSignature(win);
        return true;
    }
    args.pageNo = pageNo;
    args.rect = rect;
    args.fieldName = {};

    gSign.placing = false;
    ClearSignaturePlacementNotif(win);
    DeleteOldSelectionInfo(win, true);
    DoSign(args);
    return true;
}

// --- events -----------------------------------------------------------------

void SignDocumentView::OnCancel(SignDocumentView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseSignDocumentDialog(gSign.win);
    gp::Notify(cx);
}

void SignDocumentView::OnSign(SignDocumentView*, gp::Ctx* cx, const gp::ClickEvent*) {
    MainWindow* win = gSign.win;
    gp::Notify(cx);
    if (!GetPdfEngine(win)) {
        CloseSignDocumentDialog(win);
        return;
    }
    PdfSignArgs args;
    if (!BuildSignArgs(args)) {
        return;
    }
    // a new field with nowhere to put it: hide this dialog and let the user
    // click or drag on the page (issue #5967)
    if (len(args.fieldName) == 0 && args.rect.IsEmpty()) {
        StartSignaturePlacement();
        return;
    }
    DoSign(args);
}

void SignDocumentView::OnToggle(SignDocumentView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t which) {
    switch (which) {
        case 0:
            gSign.showLabels = !gSign.showLabels;
            break;
        case 1:
            gSign.showName = !gSign.showName;
            break;
        case 2:
            gSign.showDN = !gSign.showDN;
            break;
        case 3:
            gSign.showDate = !gSign.showDate;
            break;
        case 4:
            gSign.showGraphicName = !gSign.showGraphicName;
            break;
        case 5:
            gSign.lockFields = !gSign.lockFields;
            break;
        default:
            break;
    }
    gp::Notify(cx);
    AppShellInvalidate(gSign.win);
}

// ng: for the platforms whose picker answers later (AppShellPickFileAsync)
static void OnCertPicked(MainWindow* win, Str path) {
    if (gSign.win == win && gSign.editCert) {
        gp::InputSetValue(gSign.editCert, ToGpui(path));
        AppShellInvalidate(win);
    }
}

static void OnSignImagePicked(MainWindow* win, Str path) {
    if (gSign.win == win && gSign.editImage) {
        gp::InputSetValue(gSign.editImage, ToGpui(path));
        AppShellInvalidate(win);
    }
}

void SignDocumentView::OnBrowse(SignDocumentView*, gp::Ctx* cx, const gp::ClickEvent*) {
    TempStr filter = fmt("%s\1*.pfx;*.p12\1%s\1*.*\1", Tr("Certificate files"), Tr("All files"));
    TempStr curr = gSign.editCert ? str::DupTemp(FromGpui(gp::InputValue(gSign.editCert))) : TempStr{};
    if (AppShellPickFileAsync(gSign.win, Tr("Certificate files"), filter, MkFunc1(OnCertPicked, gSign.win))) {
        gp::Notify(cx);
        return;
    }
    TempStr path = AppShellPromptForPathTemp(gSign.win, Tr("Certificate files"), filter, curr);
    if (len(path) > 0 && gSign.editCert) {
        gp::InputSetValue(gSign.editCert, ToGpui(path));
    }
    gp::Notify(cx);
    AppShellInvalidate(gSign.win);
}

void SignDocumentView::OnBrowseImage(SignDocumentView*, gp::Ctx* cx, const gp::ClickEvent*) {
    TempStr filter = fmt("%s\1*.png;*.jpg;*.jpeg\1%s\1*.*\1", Tr("Image files"), Tr("All files"));
    TempStr curr = gSign.editImage ? str::DupTemp(FromGpui(gp::InputValue(gSign.editImage))) : TempStr{};
    if (AppShellPickFileAsync(gSign.win, Tr("Image files"), filter, MkFunc1(OnSignImagePicked, gSign.win))) {
        gp::Notify(cx);
        return;
    }
    TempStr path = AppShellPromptForPathTemp(gSign.win, Tr("Image files"), filter, curr);
    if (len(path) > 0 && gSign.editImage) {
        gp::InputSetValue(gSign.editImage, ToGpui(path));
    }
    gp::Notify(cx);
    AppShellInvalidate(gSign.win);
}

// --- showing and building ---------------------------------------------------

static gpui::InputState* NewInput(MainWindow* win) {
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    return s;
}

// fieldName selects that signature field in the placement drop-down; pass
// hasField = false (the default) to leave the choice at the first unsigned
// field, as the Sign Document command does.
void ShowSignDocumentDialog(MainWindow* win, Str fieldName, bool hasField, int certifyLevel) {
    if (!GetPdfEngine(win)) {
        return;
    }
    if (gSign.visible && gSign.win == win) {
        if (gSign.placing) {
            CancelPlacingSignature(win);
        }
        if (hasField) {
            str::ReplaceWithCopy(&gSign.preselectField, fieldName);
            gSign.hasPreselect = true;
            FillPlacement();
        }
        if (certifyLevel > 0) {
            gSign.initialCertify = certifyLevel;
            FillSignatureType();
        }
        SignDocumentSyncWindow();
        AppShellInvalidate(win);
        return;
    }
    CloseSignDocumentDialog(nullptr);

    gSign.win = win;
    gSign.initialCertify = certifyLevel;
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gSign.editCert = NewInput(win);
    gSign.editPassword = NewInput(win);
    gSign.editReason = NewInput(win);
    gSign.editLocation = NewInput(win);
    gSign.editImage = NewInput(win);
    gSign.ddCert.Init(app);
    gSign.ddPlacement.Init(app);
    gSign.ddType.Init(app);
    if (hasField) {
        str::ReplaceWithCopy(&gSign.preselectField, fieldName);
        gSign.hasPreselect = true;
    }
    CollectFields(win);
    FillCertificates();
    FillPlacement();
    FillSignatureType();
    FillAppearance();
    gSign.visible = true;
    gSign.placing = false;
    logf("ShowSignDocumentDialog: %d unsigned fields, %d certificates\n", len(gSign.fieldNames), len(gSign.certThumbs));
    SignDocumentSyncWindow();
    AppShellInvalidate(win);
}

gp::El* SignDocumentDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gSign.visible || gSign.win != win || gSign.placing) {
        return nullptr;
    }
    if (gSignTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gSignView.IsValid()) {
        gSignView = gp::EntityNewState<SignDocumentView>(cx->app);
    }
    gSign.ddCert.PollChanged(cx->app);
    gSign.ddPlacement.PollChanged(cx->app);
    bool certFile = UsingCertFile();
    bool hasImage = len(TrimmedValueTemp(gSign.editImage)) > 0;
    gp::Listener onSign = gp::ListenTo(gSignView, &SignDocumentView::OnSign);
    gp::Listener onCancel = gp::ListenTo(gSignView, &SignDocumentView::OnCancel);
    DlgSetDefault(cx, onSign, true);
    Color colMuted = ThemeWindowTextDisabledColor();
    Color colEdge = ThemeEdgeColor();

    Str certLabel = Tr("&Certificate:"), pwdLabel = Tr("&Password:"), reasonLabel = Tr("&Reason (optional):");
    Str locLabel = Tr("&Location (optional):"), whereLabel = Tr("&Where to sign:"), typeLabel = Tr("Signature &type:");
    Str imageLabel = Tr("&Image (optional):");
    float labelDx = 0;
    for (Str l : {certLabel, pwdLabel, reasonLabel, locLabel, whereLabel, typeLabel, imageLabel}) {
        labelDx = std::max(labelDx, DlgFieldLabelDx(cx, l));
    }
    // `label` is what a DlgAccel*() call made of orig's label
    auto row = [&](gp::Str label, gp::El* ctrl) {
        gp::El* l = gp::Div(cx->a)->W(labelDx)->Shrink0();
        if (label.len > 0) {
            l->Child(DlgAccelText(cx, label)->Font(kToolDlgTextFontPx)->Fg(ToGpui(colMuted)));
        }
        return DlgFieldRow(cx)->Child(l)->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(ctrl));
    };
    auto edit = [&](gp::Str id, gp::InputState* st, bool masked, bool disabled) {
        return DlgUnderlineInput(cx, id, st, kToolDlgTextFontPx, colEdge, masked, disabled);
    };
    auto fileRow = [&](gp::El* field, gp::El* button) {
        return gp::Div(cx->a)
            ->FlexRow()
            ->ItemsCenter()
            ->W(gp::kFill)
            ->Gap(kToolDlgGapDx)
            ->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(field))
            ->Child(button);
    };
    auto check = [&](Str id, Str text, bool checked, intptr_t which, bool disabled) {
        return DlgCheck(cx, id, text, checked, gp::ListenTo(gSignView, &SignDocumentView::OnToggle, which),
                        DlgCheckKind::Checkbox, disabled);
    };

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);

    // the certificate: one of the store's, or a file and its password
    gp::El* cert = DlgSection(cx);
    cert->Child(
        row(DlgAccelSelect(cx, certLabel, &gSign.ddCert), gSign.ddCert.Build(cx, StrL("sign-cert"), gp::kFill)));
    cert->Child(row({}, fileRow(edit(GStrL("sign-certfile"), gSign.editCert, false, !certFile),
                                DlgGhostButton(cx, GStrL("sign-browse"), Tr("&Browse..."),
                                               gp::ListenTo(gSignView, &SignDocumentView::OnBrowse), !certFile))));
    cert->Child(row(DlgAccelInput(cx, pwdLabel, gSign.editPassword),
                    edit(GStrL("sign-password"), gSign.editPassword, true, !certFile)));
    col->Child(cert);
    col->Child(DlgHairline(cx));

    // what the signature says and where it goes
    gp::El* details = DlgSection(cx);
    details->Child(row(DlgAccelInput(cx, reasonLabel, gSign.editReason),
                       edit(GStrL("sign-reason"), gSign.editReason, false, false)));
    details->Child(row(DlgAccelInput(cx, locLabel, gSign.editLocation),
                       edit(GStrL("sign-location"), gSign.editLocation, false, false)));
    details->Child(row(DlgAccelSelect(cx, whereLabel, &gSign.ddPlacement),
                       gSign.ddPlacement.Build(cx, StrL("sign-placement"), gp::kFill)));
    details->Child(
        row(DlgAccelSelect(cx, typeLabel, &gSign.ddType), gSign.ddType.Build(cx, StrL("sign-type"), gp::kFill)));
    details->Child(
        row({}, check(StrL("sign-lock"), Tr("Lock form &fields after signing"), gSign.lockFields, 5, false)));
    col->Child(details);
    col->Child(DlgHairline(cx));

    // what the signature shows, two checkboxes a row
    gp::El* look = DlgSection(cx);
    look->Child(DlgSectionHeader(cx, FromGpui(DlgAccelNone(cx, Tr("Appearance:")))));
    auto pair = [&](gp::El* left, gp::El* right) {
        gp::El* r = DlgFieldRow(cx)->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(left));
        r->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(right ? right : gp::Div(cx->a)));
        return r;
    };
    look->Child(pair(check(StrL("sign-labels"), Tr("Show &labels"), gSign.showLabels, 0, false),
                     check(StrL("sign-name"), Tr("Show &name"), gSign.showName, 1, false)));
    look->Child(pair(check(StrL("sign-dn"), Tr("Show &DN"), gSign.showDN, 2, false),
                     check(StrL("sign-date"), Tr("Show da&te"), gSign.showDate, 3, false)));
    look->Child(
        pair(check(StrL("sign-graphic"), Tr("Show name as &graphic"), gSign.showGraphicName && !hasImage, 4, hasImage),
             nullptr));
    look->Child(row(DlgAccelInput(cx, imageLabel, gSign.editImage),
                    fileRow(edit(GStrL("sign-image"), gSign.editImage, false, false),
                            DlgGhostButton(cx, GStrL("sign-browseimg"), Tr("C&hoose..."),
                                           gp::ListenTo(gSignView, &SignDocumentView::OnBrowseImage), false))));
    col->Child(look);
    col->Child(DlgFooter(cx, Tr("Sign"), onSign, onCancel));
    return DlgToolContent(cx, col, Tr("Sign Document"), kSignWinDx, onCancel);
}

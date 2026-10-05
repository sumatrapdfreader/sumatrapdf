/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Fills in a signature field with a certificate from the Windows store or a
// .pfx / .p12 file. ng: orig is a WS_POPUPWINDOW of virtual controls; here it
// is a gpui dialog with the same rows in the same order (certificate, the
// file + browse, password, reason, location, where to sign, the appearance
// check boxes and the optional image), and the same "hide the dialog and let
// the user click or drag on the page" placement mode.
// Signing uses wincrypt on Windows and OpenSSL with a certificate file on Linux.

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

#include "SumatraLog.h"

// Default size of a new signature when the user clicks rather than dragging
// a rectangle. 2" x 0.75" at 72 pt/in - enough for name, date and reason.
constexpr float kDefaultSignatureDx = 144;
constexpr float kDefaultSignatureDy = 54;

static Kind kNotifSignPlacement = "notifSignPlacement";

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

// orig's window at 96 dpi: a 460 wide client area, 4 / 8 around. A label has
// 8 above (not the first) and 4 under it; the certificate file and the image
// rows are a 290 wide edit and a button 7 after it; the checkboxes are 2
// apart; the buttons have 12 above and 4 below
constexpr float kSignWinDx = 460;
constexpr float kSignWinPadX = 8;
constexpr float kSignWinPadY = 4;
constexpr float kSignWinLabelGap = 8;
constexpr float kSignWinFileEditDx = 290;
constexpr float kSignWinCheckGap = 2;
constexpr float kSignWinButtonsGap = 12;

static EngineBase* GetPdfEngine(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
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
    if (win) {
        RemoveNotificationsForGroup(win, kNotifSignPlacement);
    }
}

static void ShowSignaturePlacementNotif(MainWindow* win) {
    if (!win) {
        return;
    }
    NotificationCreateArgs args;
    args.win = win;
    args.msg = Tr("Click or drag on the page to place the signature. Esc to cancel.");
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = kNotifSignPlacement;
    args.warning = true;
    args.tab = win->CurrentTab();
    ShowNotification(args);
}

// the dialog in the frame; a window of its own is not the frame's business
bool IsSignDocumentDialogVisible() {
    return gSign.visible && !gSign.placing && !gSignTw;
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
    RectF r(pt.x - (kDefaultSignatureDx / 2), pt.y - (kDefaultSignatureDy / 2), kDefaultSignatureDx,
            kDefaultSignatureDy);
    PageInfo* pi = dm ? dm->GetPageInfo(pageNo) : nullptr;
    if (!pi || !IsMediaBoxKnown(pi->mediaBox)) {
        return r;
    }
    return ClampRectToPage(r, pi->mediaBox);
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
#if OS_WIN || defined(SUMATRA_HAVE_OPENSSL)
    EngineMupdfGetUnsignedSignatureFields(engine, gSign.fieldNames, gSign.fieldPages);
#endif
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

// mupdf reports a certificate it can't open as a raw Win32 failure
// ("PFXImportCertStore failed (gle=86)"). A mistyped password is by far the
// most likely cause of gle=86 (ERROR_INVALID_PASSWORD), so say that instead.
static TempStr SignErrorMessageTemp(Str err) {
    if (len(err) == 0) {
        return Tr("Could not sign the document.");
    }
    if (str::Contains(err, StrL("PFXImportCertStore"))) {
        if (str::Contains(err, StrL("gle=86"))) {
            return Tr("Wrong password for the certificate file.");
        }
        return fmt(Tr("Could not read the certificate file: %s").s, err);
    }
    if (str::Contains(err, StrL("not found in the Windows certificate store")) ||
        str::Contains(err, StrL("invalid certificate thumbprint"))) {
        return Tr("Could not use that certificate from the Windows certificate store.");
    }
    if (str::Contains(err, StrL("could not read signature image")) || str::Contains(err, StrL("cannot create image")) ||
        str::Contains(err, StrL("unknown image format"))) {
        return Tr("Could not read the signature image.");
    }
    return str::DupTemp(err);
}

static void DoSign(const PdfSignArgs& args) {
    MainWindow* win = gSign.win;
    EngineBase* engine = GetPdfEngine(win);
    if (!engine) {
        CloseSignDocumentDialog(win);
        return;
    }
    Str err;
    bool ok = false;
#if OS_WIN || defined(SUMATRA_HAVE_OPENSSL)
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
void ShowSignDocumentDialog(MainWindow* win, Str fieldName, bool hasField) {
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
        SignDocumentSyncWindow();
        AppShellInvalidate(win);
        return;
    }
    CloseSignDocumentDialog(nullptr);

    gSign.win = win;
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gSign.editCert = NewInput(win);
    gSign.editPassword = NewInput(win);
    gSign.editReason = NewInput(win);
    gSign.editLocation = NewInput(win);
    gSign.editImage = NewInput(win);
    gSign.ddCert.Init(app);
    gSign.ddPlacement.Init(app);
    if (hasField) {
        str::ReplaceWithCopy(&gSign.preselectField, fieldName);
        gSign.hasPreselect = true;
    }
    CollectFields(win);
    FillCertificates();
    FillPlacement();
    FillAppearance();
    gSign.visible = true;
    gSign.placing = false;
    logf("ShowSignDocumentDialog: %d unsigned fields, %d certificates\n", len(gSign.fieldNames), len(gSign.certThumbs));
    SignDocumentSyncWindow();
    AppShellInvalidate(win);
}

// `text` is what a DlgAccel*() call made of orig's label
static gp::El* LabelEl(gp::Ctx* cx, gp::Str text) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    return DlgAccelText(cx, text)->Font(12)->Fg(th.mutedFg);
}

static gp::El* CheckRow(gp::Ctx* cx, Str id, Str label, bool checked, intptr_t which, bool disabled) {
    return DlgAccelEl(cx, gpc::Checkbox::New(cx, GpuiDup(cx->a, id))->Checked(checked)->Disabled(disabled), label,
                      gp::ListenTo(gSignView, &SignDocumentView::OnToggle, which), disabled);
}

static gp::El* SignDocumentWinBuild(gp::Ctx* cx, bool certFile, bool hasImage) {
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kSignWinPadX)->PadY(kSignWinPadY);
    auto label = [&](gp::Str text, float padT) {
        return gp::Div(cx->a)->PadT(padT)->Shrink0()->Child(DlgWinLabel(cx, text, font, kSignWinPadY));
    };
    auto select = [&](DialogSelect& dd, Str id) {
        gp::El* sel = dd.Build(cx, id, gp::kFill)->H(kDlgWinEditDy);
        if (sel->first) {
            sel->first->H(kDlgWinEditDy);
        }
        return gp::Div(cx->a)->W(gp::kFill)->H(kDlgWinEditDy)->Shrink0()->Child(sel);
    };
    auto edit = [&](gp::Str id, gp::InputState* st, bool masked, bool disabled) {
        return gp::Div(cx->a)
            ->W(gp::kFill)
            ->H(kDlgWinEditDy)
            ->Shrink0()
            ->Child(gpc::Input::New(cx, id, st)
                        ->WithSize(gp::UiSize::Small)
                        ->Masked(masked)
                        ->Disabled(disabled)
                        ->W(gp::kFill)
                        ->IntoEl()
                        ->H(kDlgWinEditDy));
    };
    // an edit of orig's 40 characters with a button after it
    auto fileRow = [&](gp::Str id, gp::InputState* st, bool disabled, gp::El* button) {
        gp::El* row =
            gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(kDlgWinBtnDy)->Gap(kDlgWinBtnGap)->Shrink0();
        row->Child(gpc::Input::New(cx, id, st)
                       ->WithSize(gp::UiSize::Small)
                       ->Disabled(disabled)
                       ->W(kSignWinFileEditDx)
                       ->IntoEl()
                       ->H(kDlgWinEditDy)
                       ->Shrink0());
        row->Child(button);
        return row;
    };
    auto check = [&](Str id, Str text, bool checked, intptr_t which, bool disabled) {
        return DlgWinCheck(cx, CheckRow(cx, id, text, checked, which, disabled), kSignWinCheckGap);
    };

    col->Child(label(DlgAccelSelect(cx, Tr("&Certificate:"), &gSign.ddCert), 0));
    col->Child(select(gSign.ddCert, StrL("sign-cert")));
    col->Child(fileRow(GStrL("sign-certfile"), gSign.editCert, !certFile,
                       DlgWinButton(cx, GStrL("sign-browse"), Tr("&Browse..."),
                                    gp::ListenTo(gSignView, &SignDocumentView::OnBrowse), false, !certFile)));

    col->Child(label(DlgAccelInput(cx, Tr("&Password:"), gSign.editPassword), kSignWinLabelGap));
    col->Child(edit(GStrL("sign-password"), gSign.editPassword, true, !certFile));
    col->Child(label(DlgAccelInput(cx, Tr("&Reason (optional):"), gSign.editReason), kSignWinLabelGap));
    col->Child(edit(GStrL("sign-reason"), gSign.editReason, false, false));
    col->Child(label(DlgAccelInput(cx, Tr("&Location (optional):"), gSign.editLocation), kSignWinLabelGap));
    col->Child(edit(GStrL("sign-location"), gSign.editLocation, false, false));
    col->Child(label(DlgAccelSelect(cx, Tr("&Where to sign:"), &gSign.ddPlacement), kSignWinLabelGap));
    col->Child(select(gSign.ddPlacement, StrL("sign-placement")));

    col->Child(label(DlgAccelNone(cx, Tr("Appearance:")), kSignWinLabelGap));
    col->Child(check(StrL("sign-labels"), Tr("Show &labels"), gSign.showLabels, 0, false));
    col->Child(check(StrL("sign-name"), Tr("Show &name"), gSign.showName, 1, false));
    col->Child(check(StrL("sign-dn"), Tr("Show &DN"), gSign.showDN, 2, false));
    col->Child(check(StrL("sign-date"), Tr("Show da&te"), gSign.showDate, 3, false));
    col->Child(
        check(StrL("sign-graphic"), Tr("Show name as &graphic"), gSign.showGraphicName && !hasImage, 4, hasImage));

    col->Child(label(DlgAccelInput(cx, Tr("&Image (optional):"), gSign.editImage), kSignWinLabelGap));
    col->Child(fileRow(GStrL("sign-image"), gSign.editImage, false,
                       DlgWinButton(cx, GStrL("sign-browseimg"), Tr("C&hoose..."),
                                    gp::ListenTo(gSignView, &SignDocumentView::OnBrowseImage), false)));

    gp::Listener onSign = gp::ListenTo(gSignView, &SignDocumentView::OnSign);
    DlgSetDefault(cx, onSign, true);
    gp::El* buttons = gp::Div(cx->a)
                          ->FlexRow()
                          ->JustifyEnd()
                          ->ItemsCenter()
                          ->W(gp::kFill)
                          ->H(kDlgWinBtnDy + kSignWinButtonsGap + kSignWinPadY)
                          ->PadT(kSignWinButtonsGap)
                          ->PadB(kSignWinPadY)
                          ->Gap(kDlgWinBtnGap)
                          ->Shrink0();
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gSignView, &SignDocumentView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("Sign"), onSign, true));
    col->Child(buttons);
    return DlgWinContent(cx, col);
}

gp::El* SignDocumentDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gSign.visible || gSign.win != win || gSign.placing) {
        return nullptr;
    }
    if (!gSignView.IsValid()) {
        gSignView = gp::EntityNewState<SignDocumentView>(cx->app);
    }
    gSign.ddCert.PollChanged(cx->app);
    gSign.ddPlacement.PollChanged(cx->app);
    bool certFile = UsingCertFile();
    bool hasImage = len(TrimmedValueTemp(gSign.editImage)) > 0;
    if (gSignTw) {
        return DlgWindowIsHost(cx) ? SignDocumentWinBuild(cx, certFile, hasImage) : nullptr;
    }

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(6);
    body->Child(LabelEl(cx, DlgAccelSelect(cx, Tr("&Certificate:"), &gSign.ddCert)));
    body->Child(gSign.ddCert.Build(cx, StrL("sign-cert"), gp::kFill));

    gp::El* certRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8);
    certRow->Child(gp::Div(cx->a)->Flex1()->Child(gpc::Input::New(cx, GStrL("sign-certfile"), gSign.editCert)
                                                      ->WithSize(gp::UiSize::Small)
                                                      ->Disabled(!certFile)
                                                      ->W(gp::kFill)
                                                      ->IntoEl()));
    certRow->Child(
        DlgAccelEl(cx, gpc::Button::New(cx, GStrL("sign-browse"))->WithSize(gp::UiSize::Small)->Disabled(!certFile),
                   Tr("&Browse..."), gp::ListenTo(gSignView, &SignDocumentView::OnBrowse), !certFile));
    body->Child(certRow);

    body->Child(LabelEl(cx, DlgAccelInput(cx, Tr("&Password:"), gSign.editPassword)));
    body->Child(gpc::Input::New(cx, GStrL("sign-password"), gSign.editPassword)
                    ->WithSize(gp::UiSize::Small)
                    ->Masked(true)
                    ->Disabled(!certFile)
                    ->W(gp::kFill)
                    ->IntoEl());

    body->Child(LabelEl(cx, DlgAccelInput(cx, Tr("&Reason (optional):"), gSign.editReason)));
    body->Child(gpc::Input::New(cx, GStrL("sign-reason"), gSign.editReason)
                    ->WithSize(gp::UiSize::Small)
                    ->W(gp::kFill)
                    ->IntoEl());

    body->Child(LabelEl(cx, DlgAccelInput(cx, Tr("&Location (optional):"), gSign.editLocation)));
    body->Child(gpc::Input::New(cx, GStrL("sign-location"), gSign.editLocation)
                    ->WithSize(gp::UiSize::Small)
                    ->W(gp::kFill)
                    ->IntoEl());

    body->Child(LabelEl(cx, DlgAccelSelect(cx, Tr("&Where to sign:"), &gSign.ddPlacement)));
    body->Child(gSign.ddPlacement.Build(cx, StrL("sign-placement"), gp::kFill));

    body->Child(LabelEl(cx, DlgAccelNone(cx, Tr("Appearance:"))));
    body->Child(CheckRow(cx, StrL("sign-labels"), Tr("Show &labels"), gSign.showLabels, 0, false));
    body->Child(CheckRow(cx, StrL("sign-name"), Tr("Show &name"), gSign.showName, 1, false));
    body->Child(CheckRow(cx, StrL("sign-dn"), Tr("Show &DN"), gSign.showDN, 2, false));
    body->Child(CheckRow(cx, StrL("sign-date"), Tr("Show da&te"), gSign.showDate, 3, false));
    body->Child(CheckRow(cx, StrL("sign-graphic"), Tr("Show name as &graphic"), gSign.showGraphicName && !hasImage, 4,
                         hasImage));

    body->Child(LabelEl(cx, DlgAccelInput(cx, Tr("&Image (optional):"), gSign.editImage)));
    gp::El* imgRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8);
    imgRow->Child(gp::Div(cx->a)->Flex1()->Child(gpc::Input::New(cx, GStrL("sign-image"), gSign.editImage)
                                                     ->WithSize(gp::UiSize::Small)
                                                     ->W(gp::kFill)
                                                     ->IntoEl()));
    imgRow->Child(DlgAccelEl(cx, gpc::Button::New(cx, GStrL("sign-browseimg"))->WithSize(gp::UiSize::Small),
                             Tr("C&hoose..."), gp::ListenTo(gSignView, &SignDocumentView::OnBrowseImage)));
    body->Child(imgRow);

    gp::El* footer = DialogFooter(cx, nullptr, gSignView, Tr("Sign"), Tr("Cancel"), &SignDocumentView::OnSign,
                                  &SignDocumentView::OnCancel);

    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(Tr("Sign Document")))
        ->Body(body)
        ->Footer(footer)
        ->W(520)
        ->OnClose(gp::ListenTo(gSignView, &SignDocumentView::OnCancel))
        ->IntoEl(gp::WindowSize(cx->win));
}

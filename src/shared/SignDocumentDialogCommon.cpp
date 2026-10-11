/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
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
#include "SumatraDialogs.h"
#include "SignDocumentDialogCommon.h"

// Default size of a new signature when the user clicks rather than dragging
// a rectangle. 2" x 0.75" at 72 pt/in — enough for name, date and reason.
constexpr float kDefaultSignatureDx = 144;

constexpr float kDefaultSignatureDy = 54;

EngineBase* GetPdfEngine(MainWindow* win) {
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
RectF SelectionRect(WindowTab* tab, int* pageNoOut) {
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
RectF DefaultSignatureRectAt(DisplayModel* dm, int pageNo, PointF pt) {
    RectF r(pt.x - (kDefaultSignatureDx / 2), pt.y - (kDefaultSignatureDy / 2), kDefaultSignatureDx,
            kDefaultSignatureDy);
    PageInfo* pi = dm ? dm->GetPageInfo(pageNo) : nullptr;
    if (!pi || !IsMediaBoxKnown(pi->mediaBox)) {
        return r;
    }
    return ClampRectToPage(r, pi->mediaBox);
}

// mupdf reports a certificate it can't open as a raw Win32 failure
// ("PFXImportCertStore failed (gle=86)"). A mistyped password is by far the
// most likely cause of gle=86 (ERROR_INVALID_PASSWORD), so say that instead.
TempStr SignErrorMessageTemp(Str err) {
    if (len(err) == 0) {
        return Tr("Could not sign the document.");
    }
    if (str::Contains(err, StrL("PFXImportCertStore"))) {
        if (str::Contains(err, StrL("gle=86"))) {
            return Tr("Wrong password for the certificate file.");
        }
        return fmt(Tr("Could not read the certificate file: %s").s, err);
    }
    if (str::Contains(err, StrL("SecPKCS12Import"))) {
        if (str::Contains(err, StrL("wrong password"))) {
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

void ShowSignaturePlacementNotif(MainWindow* win) {
    if (!win) {
        return;
    }
    NotificationCreateArgs args;
    args.win = win;
    args.msg = Tr("Click or drag on the page to place the signature. Esc to cancel.");
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = kNotifSignPlacement;
    args.corner = SignPlacementNotifCorner();
    args.warning = true;
    args.tab = win->CurrentTab();
    ShowNotification(args);
}

// a certifying signature must be the document's first, so a signed document
// doesn't get those entries; locking the document is for the last signer,
// so it is always there
void SignatureTypeItems(StrVec& items, Vec<int>& codes, bool docSigned) {
    items.Append(Tr("Approval signature"));
    VecAppend(codes, 0);
    if (!docSigned) {
        items.Append(Tr("Certify: no changes allowed"));
        VecAppend(codes, kPdfCertifyNoChanges);
        items.Append(Tr("Certify: form filling and signing allowed"));
        VecAppend(codes, kPdfCertifyFormsAndSigning);
        items.Append(Tr("Certify: form filling, signing and annotations allowed"));
        VecAppend(codes, kPdfCertifyFormsSigningAndAnnots);
    }
    items.Append(Tr("Lock document after signing (last signer)"));
    VecAppend(codes, kSignTypeLockDocument);
}

void ApplySignatureType(PdfSignArgs& args, int code) {
    args.certifyLevel = 0;
    args.lockDocument = false;
    if (code == kSignTypeLockDocument) {
        args.lockDocument = true;
    } else if (code > 0) {
        args.certifyLevel = code;
    }
}

bool SignTypeLocksDocument(const PdfSignArgs& args) {
    return args.certifyLevel == kPdfCertifyNoChanges || args.lockDocument;
}

Str LockDocumentWarning() {
    return Tr("Signing this way locks the document: no further changes or signatures will be possible. Continue?");
}

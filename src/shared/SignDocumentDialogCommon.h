/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SignDocumentDialogCommon.cpp and each app's SignDocumentDialog.cpp ---

EngineBase* GetPdfEngine(MainWindow* win);
RectF SelectionRect(WindowTab* tab, int* pageNoOut);
RectF DefaultSignatureRectAt(DisplayModel* dm, int pageNo, PointF pt);

TempStr SignErrorMessageTemp(Str err);

// the "Signature type" drop-down. A code is PdfSignArgs::certifyLevel (0..3)
// or kSignTypeLockDocument; codes[i] is what items[i] means
constexpr int kSignTypeLockDocument = 4;
void SignatureTypeItems(StrVec& items, Vec<int>& codes, bool docSigned);
void ApplySignatureType(PdfSignArgs& args, int code);
// certifying with no changes allowed, or locking the document: the warning
// the dialogs confirm before signing
bool SignTypeLocksDocument(const PdfSignArgs& args);
Str LockDocumentWarning();

static Kind kNotifSignPlacement = "notifSignPlacement";
NotifCorner SignPlacementNotifCorner();

void ShowSignaturePlacementNotif(MainWindow* win);

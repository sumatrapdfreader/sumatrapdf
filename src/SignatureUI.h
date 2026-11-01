/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// What a signed PDF's signatures amount to, the way Acrobat shows it: a bar over
// the document, a sidebar view, a mark on each visible signature and a dialog.

struct SignaturesCtrl;
struct WindowTab;
struct MainWindow;
struct Gfx;
struct VirtCtrl;

bool TabHasSignatures(WindowTab*);
void CreateSignaturesCtrl(MainWindow*);
VirtCtrl* SignaturesView(MainWindow*);
void DeleteSignaturesCtrl(MainWindow*);
void UpdateSignaturesCtrl(MainWindow*);
void SignaturesCtrlColorsAndDpi(MainWindow*, int dpi);

bool SigBarWanted(MainWindow*);
int SigBarDy(MainWindow*);
HWND SigBarEnsure(MainWindow*);
void SigBarSetVisible(MainWindow*, bool);
void SigBarUpdate(MainWindow*);
void DeleteSigBar(MainWindow*);

void ShowSignatureDialog(MainWindow*, int entryIdx);

void PaintSignatureMarks(MainWindow*, Gfx*);
bool SigMarksOnLeftDown(MainWindow*, int x, int y);
bool SigMarksOnSetCursor(MainWindow*);

TempStr SignatureUiStateTemp(MainWindow*, Str action);
void ViewCertDer(HWND parent, Str der);

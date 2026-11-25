/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SignatureUI: the bar over the document, the mark on each visible
// signature and the Signature Properties dialog. The sidebar view is in
// gui/Sidebar.cpp, with the other panes.

namespace gpui {
struct Ctx;
struct El;
struct PaintCtx;
} // namespace gpui

struct MainWindow;

int SigBarDy(MainWindow*);
gpui::El* SigBarBuild(MainWindow*, gpui::Ctx*);

void CloseSignatureDialog();
bool IsSignatureDialogVisible();
gpui::El* SignatureDialogBuild(MainWindow*, gpui::Ctx*);

void PaintSignatureMarks(MainWindow*, gpui::PaintCtx*);
bool SigMarksOnSetCursor(MainWindow*, Point pt);

TempStr SignatureUiStateTemp(MainWindow*, Str action);

// gui/Sidebar.cpp's Signatures pane
void SidebarShowSignatures(MainWindow*);
void SidebarSelectSignature(MainWindow*, int row);
bool SidebarSignaturesFocused(MainWindow*);
#if OS_WIN
void ViewCertDer(HWND parent, Str der);
#endif

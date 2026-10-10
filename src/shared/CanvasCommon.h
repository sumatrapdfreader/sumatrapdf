/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by CanvasCommon.cpp, orig's Canvas.cpp and ng's gui/DocCanvas.cpp ---

extern bool gLaserPointer;
bool IsPointInSelection(MainWindow* win, Point pt);
int ScrollLineAmount(int configuredAmount);
bool IsLineEndpointHandle(ResizeHandle handle);
bool IsVertexHandle(ResizeHandle handle);
bool IsPolyVertexType(AnnotationType tp);
int GetPolyVertexAt(DisplayModel* dm, Point pt, Annotation* annot);
ResizeHandle GetResizeHandleAt(MainWindow* win, Point pt, Annotation* annot);
Annotation* AnnotationLockingMouse(MainWindow* win);
RectF CalculateResizedRect(MainWindow* win, int x, int y);
extern bool gShowImages;
extern bool gShowFitContentArea;
Color ColorForPdfPageBox(PdfPageBoxKind kind);
Point PdfPageBoxLabelPos(const Rect& r, PdfPageBoxKind kind);

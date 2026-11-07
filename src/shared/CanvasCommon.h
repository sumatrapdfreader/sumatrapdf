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
bool IsFullPageImage(DisplayModel* dm, IPageElement* el, int pageNo);
int WheelPageTurnDir(MainWindow* win, int delta, int turnDelta);

template <typename T>
void MakeFullScrollbar(T& si) {
    si.nPos = 0;
    si.nMin = 0;
    si.nMax = 99;
    si.nPage = 100;
}

// the canvas window's mouse capture, timers and coordinates; implemented by each app
void CanvasSetCapture(MainWindow* win, bool capture);
void CanvasStartTimer(MainWindow* win, int id, int ms);
void CanvasKillTimer(MainWindow* win, int id);
void CanvasTrackMouseLeave(MainWindow* win);
void CanvasRelayoutAutoScroll(MainWindow* win);
bool CanvasClientToScreen(MainWindow* win, Rect r, Rect& out);

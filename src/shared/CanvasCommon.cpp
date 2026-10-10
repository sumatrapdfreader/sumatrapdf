/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GuessFileType.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "AppSettings.h"
#include "TextSelection.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Selection.h"
#include "CanvasCommon.h"

#include "SumatraLog.h"

// Canvas logic that orig's Canvas.cpp and ng's gui/DocCanvas.cpp share: hit
// testing annotation resize handles and polygon vertices, the resized
// rectangle, the debug overlays' switches and PDF page box colors.

// A laser pointer is a session mode, not a setting: it's turned on to point
// things out during a presentation and off again afterwards, and an app that
// started up with the mouse cursor replaced by a red dot would look broken.
bool gLaserPointer = false;

bool IsLaserPointerActive() {
    return gLaserPointer;
}

bool IsPointInSelection(MainWindow* win, Point pt) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        Rect r = sel.GetRect(dm);
        if (r.Contains(pt)) {
            return true;
        }
    }
    return false;
}

// Size of resize handle hit area (in pixels)
constexpr int kResizeHandleSize = 8;

int ScrollLineAmount(int configuredAmount) {
    return configuredAmount > 0 ? configuredAmount : 16;
}

bool IsLineEndpointHandle(ResizeHandle handle) {
    return handle == ResizeHandle::LineStart || handle == ResizeHandle::LineEnd;
}

bool IsVertexHandle(ResizeHandle handle) {
    return handle == ResizeHandle::Vertex;
}

bool IsPolyVertexType(AnnotationType tp) {
    return tp == AnnotationType::PolyLine || tp == AnnotationType::Polygon;
}

// Line annotations: hit-test the two endpoints, not the bounding-box handles.
static ResizeHandle GetLineEndpointHandleAt(DisplayModel* dm, Point pt, Annotation* annot) {
    PointF start, end;
    if (!GetLinePoints(annot, start, end)) {
        return ResizeHandle::None;
    }
    Point startPt = dm->CvtToScreen(annot->pageNo, start);
    Point endPt = dm->CvtToScreen(annot->pageNo, end);
    int hs = kResizeHandleSize;
    auto dist = [&](Point p) { return std::max(abs(pt.x - p.x), abs(pt.y - p.y)); };
    int dStart = dist(startPt);
    int dEnd = dist(endPt);
    if (dStart <= hs && dStart <= dEnd) {
        return ResizeHandle::LineStart;
    }
    if (dEnd <= hs) {
        return ResizeHandle::LineEnd;
    }
    return ResizeHandle::None;
}

// PolyLine / Polygon: hit-test each vertex. Returns index, or -1.
int GetPolyVertexAt(DisplayModel* dm, Point pt, Annotation* annot) {
    if (!annot || !IsPolyVertexType(annot->type)) {
        return -1;
    }
    Vec<PointF> pts = GetVertices(annot);
    int n = len(pts);
    if (n == 0) {
        return -1;
    }
    int hs = kResizeHandleSize;
    int best = -1;
    int bestDist = hs + 1;
    for (int i = 0; i < n; i++) {
        Point p = dm->CvtToScreen(annot->pageNo, pts[i]);
        int d = std::max(abs(pt.x - p.x), abs(pt.y - p.y));
        if (d <= hs && d < bestDist) {
            best = i;
            bestDist = d;
        }
    }
    return best;
}

// Get the resize handle at the given point for the selected annotation
ResizeHandle GetResizeHandleAt(MainWindow* win, Point pt, Annotation* annot) {
    if (!annot) {
        return ResizeHandle::None;
    }

    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return ResizeHandle::None;
    }

    int pageNo = annot->pageNo;
    if (!dm->PageVisible(pageNo)) {
        return ResizeHandle::None;
    }

    if (annot->type == AnnotationType::Line) {
        return GetLineEndpointHandleAt(dm, pt, annot);
    }
    if (IsPolyVertexType(annot->type)) {
        return GetPolyVertexAt(dm, pt, annot) >= 0 ? ResizeHandle::Vertex : ResizeHandle::None;
    }
    if (annot->type == AnnotationType::Redact && len(GetQuadPointsAsRect(annot)) > 0) {
        // text-selection marks are a set of quads, not a stretchable rect
        return ResizeHandle::None;
    }

    Rect rect = dm->CvtToScreen(pageNo, GetRect(annot));
    int hs = kResizeHandleSize;

    bool nearLeft = pt.x >= rect.x - hs && pt.x <= rect.x + hs;
    bool nearRight = pt.x >= rect.x + rect.dx - hs && pt.x <= rect.x + rect.dx + hs;
    bool nearTop = pt.y >= rect.y - hs && pt.y <= rect.y + hs;
    bool nearBottom = pt.y >= rect.y + rect.dy - hs && pt.y <= rect.y + rect.dy + hs;
    bool betweenX = pt.x >= rect.x + hs && pt.x <= rect.x + rect.dx - hs;
    bool betweenY = pt.y >= rect.y + hs && pt.y <= rect.y + rect.dy - hs;

    // clang-format off
    // corners have priority over edges
    if (nearLeft  && nearTop)    return ResizeHandle::TopLeft;
    if (nearRight && nearTop)    return ResizeHandle::TopRight;
    if (nearRight && nearBottom) return ResizeHandle::BottomRight;
    if (nearLeft  && nearBottom) return ResizeHandle::BottomLeft;
    // edges
    if (betweenX  && nearTop)    return ResizeHandle::Top;
    if (nearRight && betweenY)   return ResizeHandle::Right;
    if (betweenX  && nearBottom) return ResizeHandle::Bottom;
    if (nearLeft  && betweenY)   return ResizeHandle::Left;
    // clang-format on

    return ResizeHandle::None;
}

// Edit PDF with an annotation selected (its toolbar is up): the mouse works only
// on that annotation, and a click anywhere else just deselects it
Annotation* AnnotationLockingMouse(MainWindow* win) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!win || !win->pdfAnnotationsToolbarEnabled || !AnnotationIsLive(annot)) {
        return nullptr;
    }
    return annot;
}

// Helper function to calculate new rectangle during resize
RectF CalculateResizedRect(MainWindow* win, int x, int y) {
    DisplayModel* dm = win->AsFixed();
    Annotation* annot = win->annotationBeingDragged;
    int pageNo = PageNo(annot);

    // Convert screen coordinates to page coordinates
    Rect screenPt{x, y, 1, 1};
    RectF pagePt = dm->CvtFromScreen(screenPt, pageNo);

    RectF orig = win->annotationOriginalRect;
    RectF r = orig;

    Point startPt = win->dragStart;
    Rect startScreen{startPt.x, startPt.y, 1, 1};
    RectF startPage = dm->CvtFromScreen(startScreen, pageNo);

    float deltaX = pagePt.x - startPage.x;
    float deltaY = pagePt.y - startPage.y;

    const float minSize = 10.0F;
    auto handle = (ResizeHandle)win->resizeHandle;

    bool moveLeft =
        handle == ResizeHandle::TopLeft || handle == ResizeHandle::Left || handle == ResizeHandle::BottomLeft;
    bool moveRight =
        handle == ResizeHandle::TopRight || handle == ResizeHandle::Right || handle == ResizeHandle::BottomRight;
    bool moveTop = handle == ResizeHandle::TopLeft || handle == ResizeHandle::Top || handle == ResizeHandle::TopRight;
    bool moveBottom =
        handle == ResizeHandle::BottomLeft || handle == ResizeHandle::Bottom || handle == ResizeHandle::BottomRight;

    if (moveLeft) {
        r.x = orig.x + deltaX;
        r.dx = orig.dx - deltaX;
        if (r.dx < minSize) {
            r.x = orig.x + orig.dx - minSize;
            r.dx = minSize;
        }
    }
    if (moveRight) {
        r.dx = orig.dx + deltaX;
        r.dx = std::max(r.dx, minSize);
    }
    if (moveTop) {
        r.y = orig.y + deltaY;
        r.dy = orig.dy - deltaY;
        if (r.dy < minSize) {
            r.y = orig.y + orig.dy - minSize;
            r.dy = minSize;
        }
    }
    if (moveBottom) {
        r.dy = orig.dy + deltaY;
        r.dy = std::max(r.dy, minSize);
    }

    float aspect = win->annotationResizeAspectRatio;
    if (aspect > 0) {
        bool widthDriven = moveLeft || moveRight;
        if (widthDriven && (moveTop || moveBottom)) {
            float widthChange = orig.dx > 0 ? fabsf(r.dx - orig.dx) / orig.dx : 0;
            float heightChange = orig.dy > 0 ? fabsf(r.dy - orig.dy) / orig.dy : 0;
            widthDriven = widthChange >= heightChange;
        }
        if (widthDriven) {
            r.dx = std::max(r.dx, minSize * aspect);
            r.dy = r.dx / aspect;
        } else {
            r.dy = std::max(r.dy, minSize);
            r.dx = r.dy * aspect;
        }

        if (moveLeft) {
            r.x = orig.x + orig.dx - r.dx;
        } else if (moveRight) {
            r.x = orig.x;
        } else {
            r.x = orig.x + ((orig.dx - r.dx) / 2);
        }
        if (moveTop) {
            r.y = orig.y + orig.dy - r.dy;
        } else if (moveBottom) {
            r.y = orig.y;
        } else {
            r.y = orig.y + ((orig.dy - r.dy) / 2);
        }
    }

    return r;
}

// CmdToggleImages. Like showLinks this is a debug aid (both live in the debug
// menu, so both are debug / pre-release only), and like it the outlines are
// only drawn, never saved - see CmdToggleImages in FrameOnCommand
bool gShowImages = false;

// CmdToggleImages: outline images the way showLinks outlines links (debug aid)
bool ShowImageOutlines() {
    return gShowImages;
}

void ToggleShowImageOutlines() {
    gShowImages = !gShowImages;
}

// CmdToggleTransparencyGrid: Acrobat-style checkerboard under the page so
// transparent PDFs (white art on a hole) are visible. Session-only, not saved.
static bool gShowTransparencyGrid = false;

bool ShowTransparencyGrid() {
    return gShowTransparencyGrid;
}

void ToggleTransparencyGrid() {
    gShowTransparencyGrid = !gShowTransparencyGrid;
}

// CmdDebugShowFitContentArea. Like gShowImages, a debug-only visualization that
// is drawn but never saved to settings
bool gShowFitContentArea = false;

void ToggleShowFitContentArea() {
    gShowFitContentArea = !gShowFitContentArea;
}

bool ShowFitContentArea() {
    return gShowFitContentArea;
}

Color ColorForPdfPageBox(PdfPageBoxKind kind) {
    switch (kind) {
        case PdfPageBoxKind::Media:
            return MkRgb(0x20, 0x20, 0x20);
        case PdfPageBoxKind::Crop:
            return MkRgb(0xc0, 0x20, 0x20);
        case PdfPageBoxKind::Bleed:
            return MkRgb(0x20, 0x40, 0xc0);
        case PdfPageBoxKind::Trim:
            return MkRgb(0x10, 0x90, 0x20);
        case PdfPageBoxKind::Art:
            return MkRgb(0xc0, 0x80, 0x00);
    }
    return kColBlack;
}

// Place the label so coincident boxes (crop == media, etc.) stay readable.
Point PdfPageBoxLabelPos(const Rect& r, PdfPageBoxKind kind) {
    constexpr int kPad = 3;
    switch (kind) {
        case PdfPageBoxKind::Media:
            return Point(r.x + kPad, r.y + kPad);
        case PdfPageBoxKind::Crop:
            return Point(r.x + r.dx - kPad, r.y + kPad);
        case PdfPageBoxKind::Bleed:
            return Point(r.x + kPad, r.y + r.dy - kPad);
        case PdfPageBoxKind::Trim:
            return Point(r.x + r.dx - kPad, r.y + r.dy - kPad);
        case PdfPageBoxKind::Art:
            return Point(r.x + (r.dx / 2), r.y + kPad);
    }
    return r.TL();
}

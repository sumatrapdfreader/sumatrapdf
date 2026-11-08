/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "TextSelection.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "AnnotEditToolbar.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "Selection.h"
#include "SelectTextKeyboard.h"
#include "Commands.h"
#include "Toolbar.h"
#include "Translations.h"
#include "AnnotPlacement.h"
#include "AnnotPlacementCommon.h"

Kind kNotifPointAnnotationPlacement = "notifTextAnnotationPlacement";

Kind kNotifLineAnnotationPlacement = "notifLineAnnotationPlacement";

Kind kNotifPolyLineAnnotationPlacement = "notifPolyLineAnnotationPlacement";

Kind kNotifShapeAnnotationPlacement = "notifShapeAnnotationPlacement";

Kind kNotifInkAnnotationPlacement = "notifInkAnnotationPlacement";

Kind kNotifHighlighterPlacement = "notifHighlighterPlacement";

// The same values the create path will use, so the preview shows what the
// click creates.
void FreeTextPlacementArgs(int cmdId, AnnotCreateArgs& args) {
    args.annotType = AnnotationType::FreeText;
    SetAnnotCreateArgs(args, FindCustomCommand(cmdId));
}

int FreeTextFontSize(const AnnotCreateArgs& args) {
    return args.textSize > 0 ? args.textSize : 12;
}

float FreeTextPadding(const AnnotCreateArgs& args) {
    return args.borderWidth > 0 ? (float)args.borderWidth * 2.f : 0.f;
}

Str FreeTextPlacementContent(const AnnotCreateArgs& args) {
    if (str::IsEmptyOrWhiteSpace(args.content)) {
        return StrL(kDefaultFreeTextContent);
    }
    return args.content;
}

void AnnotPlacement::Reset() {
    kind = AnnotPlacementKind::None;
    cmdId = 0;
    pageNo = -1;
    pos = {};
    start = {};
    end = {};
    rect = {};
    VecClear(points);
    VecClear(strokeCounts);
    circle = false;
    mouseDown = false;
    didDrag = false;
    constrain = false;
}

AnnotPlacementKind KindOf(MainWindow* win) {
    return win ? win->annotPlacement.kind : AnnotPlacementKind::None;
}

Kind NotifGroupForKind(AnnotPlacementKind kind) {
    switch (kind) {
        case AnnotPlacementKind::Text:
        case AnnotPlacementKind::FreeText:
        case AnnotPlacementKind::Stamp:
        case AnnotPlacementKind::Caret:
        case AnnotPlacementKind::FileAttachment:
            return kNotifPointAnnotationPlacement;
        case AnnotPlacementKind::Line:
            return kNotifLineAnnotationPlacement;
        case AnnotPlacementKind::PolyLine:
            return kNotifPolyLineAnnotationPlacement;
        case AnnotPlacementKind::Shape:
            return kNotifShapeAnnotationPlacement;
        case AnnotPlacementKind::Ink:
            return kNotifInkAnnotationPlacement;
        case AnnotPlacementKind::Highlighter:
            return kNotifHighlighterPlacement;
        default:
            return nullptr;
    }
}

int OrigCommandId(int cmdId) {
    CustomCommand* cmd = FindCustomCommand(cmdId);
    return cmd ? cmd->origId : cmdId;
}

AnnotPlacementKind PlacementKindFromCommand(int cmdId) {
    switch (OrigCommandId(cmdId)) {
        case CmdCreateAnnotText:
            return AnnotPlacementKind::Text;
        case CmdCreateAnnotFreeText:
            return AnnotPlacementKind::FreeText;
        case CmdCreateAnnotStamp:
            return AnnotPlacementKind::Stamp;
        case CmdCreateAnnotCaret:
            return AnnotPlacementKind::Caret;
        case CmdCreateAnnotFileAttachment:
            return AnnotPlacementKind::FileAttachment;
        case CmdCreateAnnotLine:
            return AnnotPlacementKind::Line;
        case CmdCreateAnnotPolyLine:
            return AnnotPlacementKind::PolyLine;
        case CmdCreateAnnotSquare:
        case CmdCreateAnnotCircle:
        case CmdCreateAnnotRedact:
            return AnnotPlacementKind::Shape;
        case CmdCreateAnnotInk:
            return AnnotPlacementKind::Ink;
        case CmdAnnotationHighlightBrush:
            return AnnotPlacementKind::Highlighter;
        default:
            return AnnotPlacementKind::None;
    }
}

bool CommandUsesPlacementMode(int cmdId) {
    return PlacementKindFromCommand(cmdId) != AnnotPlacementKind::None;
}

bool IsPlacingAnnotation(MainWindow* win) {
    return KindOf(win) != AnnotPlacementKind::None;
}

bool IsPointPlacementKind(AnnotPlacementKind kind) {
    return kind == AnnotPlacementKind::Text || kind == AnnotPlacementKind::FreeText ||
           kind == AnnotPlacementKind::Stamp || kind == AnnotPlacementKind::Caret ||
           kind == AnnotPlacementKind::FileAttachment;
}

bool IsPlacingPointAnnotation(MainWindow* win) {
    return IsPointPlacementKind(KindOf(win));
}

bool IsPlacingLineAnnotation(MainWindow* win) {
    return KindOf(win) == AnnotPlacementKind::Line;
}

bool IsPlacingPolyLineAnnotation(MainWindow* win) {
    return KindOf(win) == AnnotPlacementKind::PolyLine;
}

bool IsPlacingShapeAnnotation(MainWindow* win) {
    return KindOf(win) == AnnotPlacementKind::Shape;
}

bool IsPlacingInkAnnotation(MainWindow* win) {
    return KindOf(win) == AnnotPlacementKind::Ink;
}

bool IsPlacingHighlighterAnnotation(MainWindow* win) {
    return KindOf(win) == AnnotPlacementKind::Highlighter;
}

bool HasPreview(AnnotPlacementKind kind) {
    return kind == AnnotPlacementKind::FreeText || kind == AnnotPlacementKind::Stamp ||
           kind == AnnotPlacementKind::Caret || kind == AnnotPlacementKind::FileAttachment ||
           kind == AnnotPlacementKind::Line || kind == AnnotPlacementKind::PolyLine ||
           kind == AnnotPlacementKind::Shape || kind == AnnotPlacementKind::Ink;
}

Str PlacementNotification(AnnotPlacementKind kind, bool circle, int cmdId) {
    if (OrigCommandId(cmdId) == CmdCreateAnnotRedact) {
        return Tr("Mark content for redaction. Drag or click twice. **Esc** to cancel.");
    }
    switch (kind) {
        case AnnotPlacementKind::Stamp:
            return Tr("Place stamp annotation. **Esc** to cancel.");
        case AnnotPlacementKind::Caret:
            return Tr("Place caret annotation. **Esc** to cancel.");
        case AnnotPlacementKind::FileAttachment:
            return Tr("Place file attachment. **Esc** to cancel.");
        case AnnotPlacementKind::Text:
            return Tr("Place text annotation. **Esc** to cancel.");
        case AnnotPlacementKind::FreeText:
            return Tr("Place free text annotation. **Esc** to cancel.");
        case AnnotPlacementKind::Line:
            return Tr("Place line annotation. **Shift** to snap to multiples of 45 degrees. **Esc** to cancel.");
        case AnnotPlacementKind::PolyLine:
            return Tr(
                "Place polyline annotation. **Double-click**, **right-click**, **Space**, or **Enter** to finish, "
                "**Ctrl+click** to close it. **Shift** to snap to multiples of 45 degrees. **Esc** to cancel.");
        case AnnotPlacementKind::Shape:
            return circle
                       ? Tr("Place circle annotation. Drag or click twice. **Shift** for a circle. **Esc** to cancel.")
                       : Tr("Place rectangle annotation. Drag or click twice. **Shift** for a square. **Esc** to "
                            "cancel.");
        case AnnotPlacementKind::Ink:
            return Tr("Draw ink annotation. Release to finish. **Esc** to cancel.");
        case AnnotPlacementKind::Highlighter:
            return Tr("Select text to highlight it. **Esc** or **Enter** to finish.");
        default:
            return {};
    }
}

bool FinishPolyLineAnnotationPlacement(MainWindow* win) {
    if (!IsPlacingPolyLineAnnotation(win)) {
        return false;
    }
    return FinishAnnotationPlacement(win);
}

bool FinishInkAnnotationPlacement(MainWindow* win) {
    if (!IsPlacingInkAnnotation(win)) {
        return false;
    }
    return FinishAnnotationPlacement(win);
}

void EndCurrentPlacement(MainWindow* win) {
    if (IsPlacingInkAnnotation(win)) {
        FinishInkAnnotationPlacement(win);
        return;
    }
    CancelAnnotationPlacement(win);
}

Point ShapePlacementEnd(const AnnotPlacement& p, DisplayModel* dm) {
    Point start = dm->CvtToScreen(p.pageNo, p.start);
    Point end = p.end;
    if (!p.constrain) {
        return end;
    }
    int dx = end.x - start.x;
    int dy = end.y - start.y;
    int size = std::max(abs(dx), abs(dy));
    end.x = start.x + (dx < 0 ? -size : size);
    end.y = start.y + (dy < 0 ? -size : size);
    return end;
}

Rect ShapePlacementScreenRect(const AnnotPlacement& p, DisplayModel* dm) {
    Point start = dm->CvtToScreen(p.pageNo, p.start);
    Point end = ShapePlacementEnd(p, dm);
    return Rect::FromXY(start, end);
}

// Keep the cursor at the requested point while constraining only the line end.
Point SnapLineEndpoint(Point start, Point end) {
    int dx = end.x - start.x;
    int dy = end.y - start.y;
    if (dx == 0 && dy == 0) {
        return end;
    }

    constexpr float kSnapAngle = 0.785398163f; // pi / 4
    float angle = atan2f((float)dy, (float)dx);
    float distance = sqrtf((float)(dx * dx) + (float)(dy * dy));
    float snappedAngle = roundf(angle / kSnapAngle) * kSnapAngle;
    return {start.x + (int)roundf(distance * cosf(snappedAngle)), start.y + (int)roundf(distance * sinf(snappedAngle))};
}

// How many screen pixels one PDF point of the page covers at the current zoom.
float PxPerPagePt(DisplayModel* dm, int pageNo) {
    Point p0 = dm->CvtToScreen(pageNo, PointF(0, 0));
    Point p1 = dm->CvtToScreen(pageNo, PointF(0, 1));
    float px = (float)(p1.y - p0.y);
    return px < 0.01f ? 1.f : px;
}

// Screen rect of a preview box anchored at the cursor. The screen -> page ->
// screen round trip can lose a pixel (both conversions bias by 0.499 and then
// truncate), which shows as a preview sitting a pixel off the mouse, so shift
// the box by however much the round trip drifted.
Rect PlacementPreviewScreenRect(DisplayModel* dm, int pageNo, Point pt, PointF pagePt, RectF pageRect) {
    Rect r = dm->CvtToScreen(pageNo, pageRect);
    if (r.IsEmpty()) {
        return {};
    }
    Point anchor = dm->CvtToScreen(pageNo, pagePt);
    r.Offset(pt.x - anchor.x, pt.y - anchor.y);
    return r;
}

// Where the free text preview box currently is on screen; empty when the
// cursor isn't over a visible page.
Rect FreeTextPlacementScreenRect(MainWindow* win, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    if (!dm || p.rect.dx <= 0 || p.rect.dy <= 0) {
        return {};
    }
    int pageNo = dm->GetPageNoByPoint(p.pos);
    if (!dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo)) {
        return {};
    }
    PointF pagePt = dm->CvtFromScreen(p.pos, pageNo);
    return PlacementPreviewScreenRect(dm, pageNo, p.pos, pagePt, RectF{pagePt.x, pagePt.y, p.rect.dx, p.rect.dy});
}

bool AnnotationPlacementFillCreate(MainWindow* win, AnnotationType type, Point& pt, int& pageNo, PointF& ptOnPage,
                                   PointF& lineEndOnPage, AnnotCreateArgs& args) {
    if (!IsPlacingAnnotation(win)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    AnnotPlacement& p = win->annotPlacement;
    if (!dm) {
        return false;
    }
    switch (p.kind) {
        case AnnotPlacementKind::Ink:
            if (type != AnnotationType::Ink || len(p.points) == 0 || len(p.strokeCounts) == 0) {
                return false;
            }
            pageNo = p.pageNo;
            if (!dm->ValidPageNo(pageNo)) {
                return false;
            }
            ptOnPage = p.points[0];
            pt = dm->CvtToScreen(pageNo, VecLast(p.points));
            args.inkStrokeCounts = &p.strokeCounts;
            args.inkPoints = &p.points;
            return true;
        case AnnotPlacementKind::Shape: {
            bool validType =
                type == AnnotationType::Square || type == AnnotationType::Circle || type == AnnotationType::Redact;
            if (!validType) {
                return false;
            }
            pageNo = p.pageNo;
            if (!dm->ValidPageNo(pageNo) || p.rect.IsEmpty()) {
                return false;
            }
            ptOnPage = p.rect.TL();
            Rect screenRect = dm->CvtToScreen(pageNo, p.rect);
            pt = screenRect.BR();
            args.hasRect = true;
            args.rect = p.rect;
            return true;
        }
        case AnnotPlacementKind::Line:
            if (type != AnnotationType::Line) {
                return false;
            }
            pageNo = p.pageNo;
            if (!dm->ValidPageNo(pageNo)) {
                return false;
            }
            ptOnPage = p.start;
            lineEndOnPage = dm->CvtFromScreen(p.end, pageNo);
            pt = p.end;
            args.hasLineEnd = true;
            args.lineEnd = lineEndOnPage;
            return true;
        case AnnotPlacementKind::PolyLine:
            if (type != AnnotationType::PolyLine || len(p.points) < 2) {
                return false;
            }
            pageNo = p.pageNo;
            if (!dm->ValidPageNo(pageNo)) {
                return false;
            }
            ptOnPage = p.points[0];
            pt = dm->CvtToScreen(pageNo, VecLast(p.points));
            args.polyLinePoints = &p.points;
            return true;
        case AnnotPlacementKind::Text:
            if (type != AnnotationType::Text) {
                return false;
            }
            break;
        case AnnotPlacementKind::FreeText:
            if (type != AnnotationType::FreeText) {
                return false;
            }
            break;
        case AnnotPlacementKind::Stamp:
            if (type != AnnotationType::Stamp) {
                return false;
            }
            break;
        case AnnotPlacementKind::Caret:
            if (type != AnnotationType::Caret) {
                return false;
            }
            break;
        case AnnotPlacementKind::FileAttachment:
            if (type != AnnotationType::FileAttachment) {
                return false;
            }
            break;
        default:
            return false;
    }
    pt = p.pos;
    pageNo = dm->GetPageNoByPoint(pt);
    if (pageNo < 0) {
        return false;
    }
    ptOnPage = dm->CvtFromScreen(pt, pageNo);
    if (p.kind == AnnotPlacementKind::FreeText && p.rect.dx > 0 && p.rect.dy > 0) {
        // create the annotation exactly as big as the previewed box
        args.hasRect = true;
        args.rect = {ptOnPage.x, ptOnPage.y, p.rect.dx, p.rect.dy};
    }
    return dm->ValidPageNo(pageNo);
}

bool CommitShapePlacement(MainWindow* win) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    AnnotPlacement& p = win->annotPlacement;
    if (!IsPlacingShapeAnnotation(win) || !dm || !dm->ValidPageNo(p.pageNo)) {
        return false;
    }
    Rect screenRect = ShapePlacementScreenRect(p, dm);
    int minSize = std::max(DpiScale(4), 2);
    if (screenRect.dx < minSize || screenRect.dy < minSize) {
        return false;
    }
    RectF pageRect = dm->CvtFromScreen(screenRect, p.pageNo);
    if (pageRect.IsEmpty()) {
        return false;
    }

    p.rect = pageRect;
    Point pt = ShapePlacementEnd(p, dm);
    CommitPlacementCommand(win, pt);
    return true;
}

// A click outside every page is consumed but leaves the mode active. A valid
// click re-enters the command path with the original command id so custom
// color/openEdit arguments are retained.
bool PlacePointAnnotationAt(MainWindow* win, Point pt) {
    if (!IsPlacingPointAnnotation(win)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    if (!dm || !dm->ValidPageNo(pageNo)) {
        return true;
    }
    win->annotPlacement.pos = pt;
    CommitPlacementCommand(win, pt);
    return true;
}

bool AnnotationPlacementOnSetCursor(MainWindow* win) {
    if (!IsPlacingAnnotation(win) || KindOf(win) == AnnotPlacementKind::Highlighter) {
        return false;
    }
    SetPlacementCursor(win);
    return true;
}

constexpr int kInkEraserRadiusPx = 10;

bool AppendInkPoint(MainWindow* win, DisplayModel* dm, Point pt) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!dm || !dm->ValidPageNo(pageNo) || dm->GetPageNoByPoint(pt) != pageNo || len(p.strokeCounts) == 0) {
        return false;
    }
    if (VecLast(p.strokeCounts) > 0) {
        Point previous = dm->CvtToScreen(pageNo, VecLast(p.points));
        if (previous == pt) {
            return false;
        }
    }
    VecAppend(p.points, dm->CvtFromScreen(pt, pageNo));
    VecLast(p.strokeCounts)++;
    win->RedrawCanvas();
    return true;
}

bool AnnotationPlacementEraseAt(MainWindow* win, Point pt) {
    if (!IsPlacingInkAnnotation(win)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    if (!dm || !tab || !engine || !dm->ValidPageNo(pageNo)) {
        return true;
    }

    PointF pagePt = dm->CvtFromScreen(pt, pageNo);
    float radius = (float)DpiScale(kInkEraserRadiusPx) / PxPerPagePt(dm, pageNo);
    AnnotPlacement& p = win->annotPlacement;
    bool pendingChanged = false;
    if (p.pageNo == pageNo) {
        pendingChanged = EraseInkStrokes(p.strokeCounts, p.points, pagePt, radius);
        if (len(p.strokeCounts) == 0) {
            p.pageNo = -1;
        }
    }

    bool savedChanged = false;
    Vec<Annotation*> annots;
    EngineMupdfGetLoadedAnnotations(engine, annots);
    for (Annotation* annot : annots) {
        if (Type(annot) != AnnotationType::Ink || PageNo(annot) != pageNo) {
            continue;
        }
        InkEraseResult result = EraseAnnotationInk(annot, pagePt, radius);
        if (result == InkEraseResult::Empty) {
            DeleteAnnotationAndUpdateUI(tab, annot);
            savedChanged = true;
        } else if (result == InkEraseResult::Changed) {
            savedChanged = true;
        }
    }
    if (savedChanged) {
        RefreshAnnotationLists(tab);
        MainWindowRerender(win);
    } else if (pendingChanged) {
        win->RedrawCanvas();
    }
    return true;
}

bool CancelAnnotationPlacement(MainWindow* win) {
    if (!IsPlacingAnnotation(win)) {
        return false;
    }
    AnnotPlacement& p = win->annotPlacement;
    ReleasePlacementCapture(win);
    Kind group = NotifGroupForKind(p.kind);
    p.Reset();
    if (group) {
        RemoveNotificationsForGroup(win, group);
    }
    HideAnnotationHoverOverlay(win);
    ScheduleRepaint(win, 0);
    RestoreCanvasCursor(win);
    ToolbarUpdateStateForWindow(win, false);
    return true;
}

void CommitPlacementCommand(MainWindow* win, Point pt) {
    int cmdId = win->annotPlacement.cmdId;
    PlacementRunCreateCmd(win, cmdId, pt);
    CancelAnnotationPlacement(win);
}

// The first page click anchors the preview. A second click on that page
// executes the original command with both endpoints; a click anywhere else
// cancels the mode because a PDF line annotation cannot span pages.
static bool HandleLineClick(MainWindow* win, Point pt, bool isShift) {
    if (!IsPlacingLineAnnotation(win)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    bool started = p.pageNo > 0;
    if (!dm || !dm->ValidPageNo(pageNo) || (started && pageNo != p.pageNo)) {
        CancelAnnotationPlacement(win);
        return true;
    }
    if (!started) {
        p.pageNo = pageNo;
        p.start = dm->CvtFromScreen(pt, pageNo);
        p.end = pt;
        ScheduleRepaint(win, 0);
        return true;
    }
    p.end = isShift ? SnapLineEndpoint(dm->CvtToScreen(pageNo, p.start), pt) : pt;
    CommitPlacementCommand(win, pt);
    return true;
}

// Each page click commits a vertex and starts previewing the next segment.
// A click off that page cancels the whole path, matching line placement.
// Ctrl+click commits the vertex and then closes the shape, repeating the first
// point so the last segment runs back to it (issue #6119).
static bool HandlePolyLineClick(MainWindow* win, Point pt, bool isShift, bool isCtrl) {
    if (!IsPlacingPolyLineAnnotation(win)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    bool started = len(p.points) > 0;
    if (!dm || !dm->ValidPageNo(pageNo) || (started && pageNo != p.pageNo)) {
        CancelAnnotationPlacement(win);
        return true;
    }
    if (!started) {
        p.pageNo = pageNo;
    }
    if (started && isShift) {
        pt = SnapLineEndpoint(dm->CvtToScreen(pageNo, p.points[len(p.points) - 1]), pt);
    }
    VecAppend(p.points, dm->CvtFromScreen(pt, pageNo));
    p.end = pt;
    // one point plus this click is a single segment; closing it would just
    // double back on itself, so let it keep collecting vertices instead
    bool close = isCtrl && len(p.points) > 2;
    if (close) {
        // by value: VecAppend takes a reference, and growing the vec frees the
        // buffer that reference would point into
        // by value: VecAppend takes a reference, and growing the vec frees the
        // buffer that reference would point into
        PointF first = p.points[0];
        VecAppend(p.points, first);
        CommitPlacementCommand(win, dm->CvtToScreen(pageNo, first));
        return true;
    }
    ScheduleRepaint(win, 0);
    return true;
}

static bool HandleShapeDown(MainWindow* win, Point pt, bool isShift) {
    if (!IsPlacingShapeAnnotation(win)) {
        return false;
    }
    PlacementFocusFrame(win);
    DisplayModel* dm = win->AsFixed();
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    bool started = p.pageNo > 0;
    if (!dm || !dm->ValidPageNo(pageNo) || (started && pageNo != p.pageNo)) {
        CancelAnnotationPlacement(win);
        return true;
    }

    p.end = pt;
    p.constrain = isShift;
    if (started) {
        CommitShapePlacement(win);
        return true;
    }

    p.pageNo = pageNo;
    p.start = dm->CvtFromScreen(pt, pageNo);
    p.mouseDown = true;
    p.didDrag = false;
    PlacementCaptureMouse(win);
    ScheduleRepaint(win, 0);
    return true;
}

static bool HandleShapeUp(MainWindow* win, Point pt, bool isShift) {
    if (!IsPlacingShapeAnnotation(win) || !win->annotPlacement.mouseDown) {
        return false;
    }
    PlacementReleaseMouse(win);
    AnnotPlacement& p = win->annotPlacement;
    p.mouseDown = false;
    DisplayModel* dm = win->AsFixed();
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    if (!dm || pageNo != p.pageNo) {
        CancelAnnotationPlacement(win);
        return true;
    }

    p.end = pt;
    p.constrain = isShift;
    Point start = dm->CvtToScreen(pageNo, p.start);
    if (p.didDrag || IsDragDistance(pt.x, start.x, pt.y, start.y)) {
        CommitShapePlacement(win);
    } else {
        ScheduleRepaint(win, 0);
    }
    return true;
}

static bool HandleInkDown(MainWindow* win, Point pt) {
    if (!IsPlacingInkAnnotation(win)) {
        return false;
    }
    PlacementFocusFrame(win);
    DisplayModel* dm = win->AsFixed();
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    bool started = p.pageNo > 0;
    if (!dm || !dm->ValidPageNo(pageNo) || (started && pageNo != p.pageNo)) {
        CancelAnnotationPlacement(win);
        return true;
    }
    if (p.mouseDown) {
        return true;
    }
    if (!started) {
        p.pageNo = pageNo;
    }
    VecAppend(p.strokeCounts, 0);
    p.mouseDown = true;
    AppendInkPoint(win, dm, pt);
    PlacementCaptureMouse(win);
    return true;
}

static bool HandleInkUp(MainWindow* win, Point pt) {
    if (!IsPlacingInkAnnotation(win) || !win->annotPlacement.mouseDown) {
        return false;
    }
    AppendInkPoint(win, win->AsFixed(), pt);
    PlacementReleaseMouse(win);
    win->annotPlacement.mouseDown = false;
    int cmdId = win->annotPlacement.cmdId;
    FinishInkAnnotationPlacement(win);
    StartAnnotationPlacement(win, cmdId);
    return true;
}

bool AnnotationPlacementOnLeftDown(MainWindow* win, Point pt, bool isShift, bool isCtrl) {
    switch (KindOf(win)) {
        case AnnotPlacementKind::Ink:
            HandleInkDown(win, pt);
            return true;
        case AnnotPlacementKind::Shape:
            HandleShapeDown(win, pt, isShift);
            return true;
        case AnnotPlacementKind::Line:
            PlacementFocusFrame(win);
            HandleLineClick(win, pt, isShift);
            return true;
        case AnnotPlacementKind::PolyLine:
            PlacementFocusFrame(win);
            HandlePolyLineClick(win, pt, isShift, isCtrl);
            return true;
        case AnnotPlacementKind::Text:
        case AnnotPlacementKind::FreeText:
        case AnnotPlacementKind::Stamp:
        case AnnotPlacementKind::Caret:
        case AnnotPlacementKind::FileAttachment:
            PlacementFocusFrame(win);
            PlacePointAnnotationAt(win, pt);
            return true;
        default:
            return false;
    }
}

bool AnnotationPlacementOnLeftUp(MainWindow* win, Point pt, bool isShift) {
    if (HandleInkUp(win, pt)) {
        return true;
    }
    return HandleShapeUp(win, pt, isShift);
}

bool AnnotationPlacementOnLeftDblClk(MainWindow* win, Point pt) {
    if (IsPlacingInkAnnotation(win)) {
        HandleInkDown(win, pt);
        return true;
    }
    if (IsPlacingPolyLineAnnotation(win)) {
        PlacementFocusFrame(win);
        FinishPolyLineAnnotationPlacement(win);
        return true;
    }
    return false;
}

bool AnnotationPlacementOnRightDown(MainWindow* win) {
    if (!IsPlacingPolyLineAnnotation(win)) {
        return false;
    }
    PlacementFocusFrame(win);
    FinishPolyLineAnnotationPlacement(win);
    return true;
}

// The highlighter leaves the mouse to text selection, which it acts on when
// a selection is finished.
void AnnotationPlacementOnSelectionStop(MainWindow* win) {
    if (KindOf(win) != AnnotPlacementKind::Highlighter) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage || !win->showSelection) {
        return;
    }
    PlacementRunCreateCmd(win, win->annotPlacement.cmdId, Point{});
}

// Enter/Space finish polyline; Enter finishes ink. Starting another placement
// while ink has strokes also finishes it so the drawing isn't thrown away.
bool FinishAnnotationPlacement(MainWindow* win) {
    AnnotPlacementKind kind = KindOf(win);
    if (kind == AnnotPlacementKind::PolyLine) {
        AnnotPlacement& p = win->annotPlacement;
        if (len(p.points) < 2) {
            return true;
        }
        DisplayModel* dm = win->AsFixed();
        if (!dm || !dm->ValidPageNo(p.pageNo)) {
            CancelAnnotationPlacement(win);
            return true;
        }
        Point pt = dm->CvtToScreen(p.pageNo, VecLast(p.points));
        CommitPlacementCommand(win, pt);
        return true;
    }
    if (kind == AnnotPlacementKind::Ink) {
        AnnotPlacement& p = win->annotPlacement;
        if (len(p.points) == 0) {
            CancelAnnotationPlacement(win);
            return true;
        }
        DisplayModel* dm = win->AsFixed();
        if (!dm || !dm->ValidPageNo(p.pageNo)) {
            CancelAnnotationPlacement(win);
            return true;
        }
        ReleasePlacementCapture(win);
        p.mouseDown = false;
        Point pt = dm->CvtToScreen(p.pageNo, VecLast(p.points));
        CommitPlacementCommand(win, pt);
        return true;
    }
    return false;
}

TempStr PointPlacementDumpLineTemp(MainWindow* win, AnnotPlacementKind kind, Str key, bool svgCursor) {
    bool active = KindOf(win) == kind;
    if (!win) {
        if (svgCursor) {
            return fmt("%s active=0 notification=0 cursor=0 cmd=0 message=\n", key);
        }
        return fmt("%s active=0 notification=0 cursor=0 preview=0 cmd=0 message=\n", key);
    }
    NotificationWnd* notif = active ? GetNotificationForGroup(win, NotifGroupForKind(kind)) : nullptr;
    Str message = NotificationGetMessageTemp(notif);
    bool cursor = PlacementDumpCursor(win, kind, active);
    int cmdOut = active ? win->annotPlacement.cmdId : 0;
    if (svgCursor) {
        return fmt("%s active=%d notification=%d cursor=%d cmd=%d message=%s\n", key, active ? 1 : 0, notif ? 1 : 0,
                   cursor ? 1 : 0, cmdOut, message);
    }
    DisplayModel* dm = active ? win->AsFixed() : nullptr;
    Point pt = win->annotPlacement.pos;
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    bool preview = active && dm && dm->ValidPageNo(pageNo);
    return fmt("%s active=%d notification=%d cursor=%d preview=%d cmd=%d message=%s\n", key, active ? 1 : 0,
               notif ? 1 : 0, cursor ? 1 : 0, preview ? 1 : 0, cmdOut, message);
}

bool AnnotationPlacementOnMouseMove(MainWindow* win, Point pt, bool isShift, bool lButtonDown) {
    if (!IsPlacingAnnotation(win) || KindOf(win) == AnnotPlacementKind::Highlighter) {
        return false;
    }
    // a right-drag pan or a middle-click scroll under way keeps its moves
    if (win->mouseAction == MouseAction::Dragging || win->mouseAction == MouseAction::Scrolling) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return true;
    }
    if (win->annotationUnderCursor) {
        win->annotationUnderCursor = nullptr;
        if (IsPlacingPointAnnotation(win)) {
            ScheduleRepaint(win, 0);
        }
    }
    PlacementHideHover(win);
    SetPlacementCursor(win);

    AnnotPlacement& p = win->annotPlacement;
    switch (p.kind) {
        case AnnotPlacementKind::Ink:
            if (p.mouseDown && lButtonDown) {
                AppendInkPoint(win, dm, pt);
            }
            break;
        case AnnotPlacementKind::Shape:
            if (p.pageNo > 0) {
                if (p.mouseDown) {
                    Point start = dm->CvtToScreen(p.pageNo, p.start);
                    if (IsDragDistance(pt.x, start.x, pt.y, start.y)) {
                        p.didDrag = true;
                    }
                }
                if (pt != p.end || isShift != p.constrain) {
                    p.end = pt;
                    p.constrain = isShift;
                    ScheduleRepaint(win, 0);
                }
            }
            break;
        case AnnotPlacementKind::Line:
            if (p.pageNo > 0) {
                Point start = dm->CvtToScreen(p.pageNo, p.start);
                Point end = isShift ? SnapLineEndpoint(start, pt) : pt;
                // Compare the snapped point, not the pointer: Shift at the same
                // spot still has to move the preview, and releasing it has to put it back.
                if (end != p.end) {
                    p.end = end;
                    ScheduleRepaint(win, 0);
                }
            }
            break;
        case AnnotPlacementKind::PolyLine:
            if (len(p.points) > 0) {
                Point last = dm->CvtToScreen(p.pageNo, p.points[len(p.points) - 1]);
                Point end = isShift ? SnapLineEndpoint(last, pt) : pt;
                if (end != p.end) {
                    p.end = end;
                    ScheduleRepaint(win, 0);
                }
            }
            break;
        case AnnotPlacementKind::Text:
        case AnnotPlacementKind::FreeText:
        case AnnotPlacementKind::Stamp:
        case AnnotPlacementKind::Caret:
        case AnnotPlacementKind::FileAttachment: {
            bool previewMoved = HasPreview(p.kind) && pt != p.pos;
            p.pos = pt;
            if (previewMoved) {
                ScheduleRepaint(win, 0);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

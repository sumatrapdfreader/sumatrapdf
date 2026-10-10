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

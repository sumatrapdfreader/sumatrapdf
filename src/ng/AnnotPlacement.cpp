/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AnnotPlacement.cpp - the modes a CmdCreateAnnot* command enters
// when it has no point to place the annotation at: the cursor becomes a cross,
// a hint notification says what to do, and the next click (or drag, or set of
// strokes) says where. Same kinds, same keys, same previews. What differs is
// the cursor off Windows (gpui has no cursor from an image, so every mode uses
// Crosshair where orig draws the annotation's SVG icon into one) and that the
// preview is painted by the gpui canvas instead of into the canvas HDC.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "TextSelection.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "Commands.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Toolbar.h"
#include "Selection.h"
#include "SelectTextKeyboard.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "AnnotEditToolbar.h"
#include "AnnotTextPopup.h"
#include "AnnotPlacement.h"

#include "SumatraLog.h"

static Kind kNotifPointAnnotationPlacement = "notifTextAnnotationPlacement";
static Kind kNotifLineAnnotationPlacement = "notifLineAnnotationPlacement";
static Kind kNotifPolyLineAnnotationPlacement = "notifPolyLineAnnotationPlacement";
static Kind kNotifShapeAnnotationPlacement = "notifShapeAnnotationPlacement";
static Kind kNotifInkAnnotationPlacement = "notifInkAnnotationPlacement";
static Kind kNotifHighlighterPlacement = "notifHighlighterPlacement";

// MuPDF's default stamp is {12,12,12+190,12+50}; caret is {12,12,12+18,12+15}
// with the caret mark at the middle of the left edge; file attachment is
// {12,12,12+16,12+16}.
constexpr float kStampAnnotDefaultDx = 190.f;
constexpr float kStampAnnotDefaultDy = 50.f;
constexpr float kCaretAnnotDefaultDx = 18.f;
constexpr float kCaretAnnotDefaultDy = 15.f;
constexpr float kFileAttachmentAnnotDefaultDx = 16.f;
constexpr float kFileAttachmentAnnotDefaultDy = 16.f;

constexpr int kInkEraserRadiusPx = 10;

// 40% yellow, when Annotations.InkColor is not a color. How translucent a
// stroke is comes from its color's alpha.
constexpr Color kInkDefaultColor = 0x6600ffff;

// Free text is placed like a stamp: a preview box the size of the annotation
// follows the cursor and a click creates it there. MuPDF lays free text out
// with padding = 2 * border width and a 1.2 * font size line height
// (pdf_write_free_text_appearance), so a box that fits one line is that tall.
constexpr float kFreeTextLineHeight = 1.2f;
// ng: orig measures the placeholder with GDI+ Arial (Helvetica's metrics).
// There is no portable text measurement outside a paint, so the box is as wide
// as the average Helvetica advance makes it.
constexpr float kFreeTextAvgAdvance = 0.5f;
constexpr float kFreeTextWidthSlack = 1.02f;

// the preview and the selection markers, orig's colors
constexpr Color kPreviewBlue = MkRgb(0, 80, 200);
constexpr Color kPreviewWhite = 0xffffff;

// The same values the create path will use, so the preview shows what the
// click creates.
static void FreeTextPlacementArgs(int cmdId, AnnotCreateArgs& args) {
    args.annotType = AnnotationType::FreeText;
    SetAnnotCreateArgs(args, FindCustomCommand(cmdId));
}

static int FreeTextFontSize(const AnnotCreateArgs& args) {
    return args.textSize > 0 ? args.textSize : 12;
}

static float FreeTextPadding(const AnnotCreateArgs& args) {
    return args.borderWidth > 0 ? (float)args.borderWidth * 2.f : 0.f;
}

static Str FreeTextPlacementContent(const AnnotCreateArgs& args) {
    if (str::IsEmptyOrWhiteSpace(args.content)) {
        return StrL(kDefaultFreeTextContent);
    }
    return args.content;
}

// Size, in page units, of a box that fits the annotation's text.
SizeF FreeTextPlacementPageSize(const AnnotCreateArgs& args) {
    float fontSize = (float)FreeTextFontSize(args);
    float pad = FreeTextPadding(args);
    Str text = FreeTextPlacementContent(args);
    float dx = (float)len(text) * fontSize * kFreeTextAvgAdvance;
    dx = (dx * kFreeTextWidthSlack) + (2 * pad) + 2.f;
    int nLines = 1;
    for (int i = 0; i < len(text); i++) {
        nLines += text.s[i] == '\n' ? 1 : 0;
    }
    float dy = ((float)nLines * kFreeTextLineHeight * fontSize) + (2 * pad);
    return {dx, dy};
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

static AnnotPlacementKind KindOf(MainWindow* win) {
    return win ? win->annotPlacement.kind : AnnotPlacementKind::None;
}

// orig's SetPlacementCursor: the note and the ink tool have a cursor made from
// their SVG icon, every other mode a cross
// ng: the icon cursors exist on Windows only (see "gpui gaps": no cursor from
// an image); elsewhere all modes use the cross
static void SetPlacementCursor(MainWindow* win) {
    switch (KindOf(win)) {
        case AnnotPlacementKind::Text:
            if (CanvasSetNativeCursor(win, NativeCursor::TextAnnotationPlacement)) {
                return;
            }
            break;
        case AnnotPlacementKind::Ink:
            if (CanvasSetNativeCursor(win, NativeCursor::InkAnnotationPlacement)) {
                return;
            }
            break;
        default:
            break;
    }
    CanvasSetCursor(win, (int)gp::CursorKind::Crosshair);
}

static Kind NotifGroupForKind(AnnotPlacementKind kind) {
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

static int OrigCommandId(int cmdId) {
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

static bool IsPointPlacementKind(AnnotPlacementKind kind) {
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

static bool HasPreview(AnnotPlacementKind kind) {
    return kind == AnnotPlacementKind::FreeText || kind == AnnotPlacementKind::Stamp ||
           kind == AnnotPlacementKind::Caret || kind == AnnotPlacementKind::FileAttachment ||
           kind == AnnotPlacementKind::Line || kind == AnnotPlacementKind::PolyLine ||
           kind == AnnotPlacementKind::Shape || kind == AnnotPlacementKind::Ink;
}

static Str PlacementNotification(AnnotPlacementKind kind, bool circle, int cmdId) {
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

bool CancelAnnotationPlacement(MainWindow* win) {
    if (!IsPlacingAnnotation(win)) {
        return false;
    }
    AnnotPlacement& p = win->annotPlacement;
    Kind group = NotifGroupForKind(p.kind);
    p.Reset();
    if (group) {
        RemoveNotificationsForGroup(win, group);
    }
    CanvasSetCursor(win, (int)gp::CursorKind::Arrow);
    ToolbarUpdateStateForWindow(win, false);
    win->RedrawAll(true);
    return true;
}

// orig posts WM_COMMAND with kAnnotationPlacementCommandCode; here the command
// path takes the same two flags as arguments
static void CommitPlacementCommand(MainWindow* win, Point pt) {
    int cmdId = win->annotPlacement.cmdId;
    ExecuteAnnotCreateCmd(win, cmdId, true, pt);
    CancelAnnotationPlacement(win);
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
        DisplayModel* dm = win->AsFixed();
        if (len(p.points) == 0 || !dm || !dm->ValidPageNo(p.pageNo)) {
            CancelAnnotationPlacement(win);
            return true;
        }
        p.mouseDown = false;
        Point pt = dm->CvtToScreen(p.pageNo, VecLast(p.points));
        CommitPlacementCommand(win, pt);
        return true;
    }
    return false;
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

static void EndCurrentPlacement(MainWindow* win) {
    if (IsPlacingInkAnnotation(win)) {
        FinishInkAnnotationPlacement(win);
        return;
    }
    CancelAnnotationPlacement(win);
}

void StartAnnotationPlacement(MainWindow* win, int cmdId) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    AnnotPlacementKind kind = PlacementKindFromCommand(cmdId);
    if (!win || !tab || !engine || !EngineSupportsAnnotations(engine) || kind == AnnotPlacementKind::None) {
        return;
    }

    EndCurrentPlacement(win);

    AnnotPlacement& p = win->annotPlacement;
    p.Reset();
    p.kind = kind;
    p.cmdId = cmdId;
    p.circle = OrigCommandId(cmdId) == CmdCreateAnnotCircle;
    if (IsPointPlacementKind(kind)) {
        p.pos = win->dragPrevPos;
    }
    if (kind == AnnotPlacementKind::FreeText) {
        // rect holds the preview box size (page units); the position comes
        // from the cursor
        AnnotCreateArgs args;
        FreeTextPlacementArgs(cmdId, args);
        SizeF size = FreeTextPlacementPageSize(args);
        p.rect = {0, 0, size.dx, size.dy};
    }

    // the highlighter works on a keyboard selection; other modes take the pointer
    if (!(kind == AnnotPlacementKind::Highlighter && SelectTextWithKeyboardActive(win))) {
        StopSelectTextWithKeyboard(win);
        DeleteOldSelectionInfo(win, true);
    }
    if (tab->selectedAnnotation) {
        SetSelectedAnnotation(tab, nullptr);
    }
    win->annotationUnderCursor = nullptr;

    // edit PDF toolbar buttons are disabled for the duration of the mode
    ToolbarUpdateStateForWindow(win, false);

    NotificationCreateArgs args;
    args.win = win;
    args.msg = PlacementNotification(kind, p.circle, cmdId);
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = NotifGroupForKind(kind);
    args.warning = true;
    args.tab = tab;
    ShowNotification(args);

    SetPlacementCursor(win);
    logf("StartAnnotationPlacement: kind %d, cmd %d\n", (int)kind, cmdId);
    win->RedrawAll(true);
}

static Point ShapePlacementEnd(const AnnotPlacement& p, DisplayModel* dm) {
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

static Rect ShapePlacementScreenRect(const AnnotPlacement& p, DisplayModel* dm) {
    Point start = dm->CvtToScreen(p.pageNo, p.start);
    Point end = ShapePlacementEnd(p, dm);
    return Rect::FromXY(start, end);
}

static bool CommitShapePlacement(MainWindow* win) {
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
static bool PlacePointAnnotationAt(MainWindow* win, Point pt) {
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
        win->RedrawAll(true);
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
        PointF first = p.points[0];
        VecAppend(p.points, first);
        CommitPlacementCommand(win, dm->CvtToScreen(pageNo, first));
        return true;
    }
    win->RedrawAll(true);
    return true;
}

static bool HandleShapeDown(MainWindow* win, Point pt, bool isShift) {
    if (!IsPlacingShapeAnnotation(win)) {
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
    win->RedrawAll(true);
    return true;
}

static bool HandleShapeUp(MainWindow* win, Point pt, bool isShift) {
    if (!IsPlacingShapeAnnotation(win) || !win->annotPlacement.mouseDown) {
        return false;
    }
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
        win->RedrawAll(true);
    }
    return true;
}

static bool AppendInkPoint(MainWindow* win, DisplayModel* dm, Point pt) {
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
    win->RedrawAll(true);
    return true;
}

// How many screen pixels one PDF point of the page covers at the current zoom.
static float PxPerPagePt(DisplayModel* dm, int pageNo) {
    Point p0 = dm->CvtToScreen(pageNo, PointF(0, 0));
    Point p1 = dm->CvtToScreen(pageNo, PointF(0, 1));
    float px = (float)(p1.y - p0.y);
    return px < 0.01f ? 1.f : px;
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
        win->RedrawAll(true);
    }
    return true;
}

static bool HandleInkDown(MainWindow* win, Point pt) {
    if (!IsPlacingInkAnnotation(win)) {
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
    if (p.mouseDown) {
        return true;
    }
    if (!started) {
        p.pageNo = pageNo;
    }
    VecAppend(p.strokeCounts, 0);
    p.mouseDown = true;
    AppendInkPoint(win, dm, pt);
    return true;
}

static bool HandleInkUp(MainWindow* win, Point pt) {
    if (!IsPlacingInkAnnotation(win) || !win->annotPlacement.mouseDown) {
        return false;
    }
    AppendInkPoint(win, win->AsFixed(), pt);
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
            HandleLineClick(win, pt, isShift);
            return true;
        case AnnotPlacementKind::PolyLine:
            HandlePolyLineClick(win, pt, isShift, isCtrl);
            return true;
        case AnnotPlacementKind::Text:
        case AnnotPlacementKind::FreeText:
        case AnnotPlacementKind::Stamp:
        case AnnotPlacementKind::Caret:
        case AnnotPlacementKind::FileAttachment:
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
        FinishPolyLineAnnotationPlacement(win);
        return true;
    }
    return false;
}

bool AnnotationPlacementOnRightDown(MainWindow* win) {
    if (!IsPlacingPolyLineAnnotation(win)) {
        return false;
    }
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
    ExecuteAnnotCreateCmd(win, win->annotPlacement.cmdId, true, Point{});
}

bool AnnotationPlacementOnMouseMove(MainWindow* win, Point pt, bool isShift, bool lButtonDown) {
    if (!IsPlacingAnnotation(win) || KindOf(win) == AnnotPlacementKind::Highlighter) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return true;
    }
    if (win->annotationUnderCursor) {
        win->annotationUnderCursor = nullptr;
        if (IsPlacingPointAnnotation(win)) {
            win->RedrawAll(true);
        }
    }
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
                    win->RedrawAll(true);
                }
            }
            break;
        case AnnotPlacementKind::Line:
            if (p.pageNo > 0) {
                Point start = dm->CvtToScreen(p.pageNo, p.start);
                Point end = isShift ? SnapLineEndpoint(start, pt) : pt;
                // Compare the snapped point: toggling Shift without moving the
                // pointer must still update the preview.
                if (end != p.end) {
                    p.end = end;
                    win->RedrawAll(true);
                }
            }
            break;
        case AnnotPlacementKind::PolyLine:
            if (len(p.points) > 0) {
                Point last = dm->CvtToScreen(p.pageNo, p.points[len(p.points) - 1]);
                Point end = isShift ? SnapLineEndpoint(last, pt) : pt;
                if (end != p.end) {
                    p.end = end;
                    win->RedrawAll(true);
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
                win->RedrawAll(true);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

bool AnnotationPlacementOnSetCursor(MainWindow* win) {
    if (!IsPlacingAnnotation(win) || KindOf(win) == AnnotPlacementKind::Highlighter) {
        return false;
    }
    SetPlacementCursor(win);
    return true;
}

bool AnnotationPlacementOnKeyDown(MainWindow* win, int vkey) {
    if (!win || CanvasCtrlPressed() || CanvasShiftPressed()) {
        return false;
    }
    if (vkey == VK_RETURN && IsPlacingInkAnnotation(win)) {
        return FinishInkAnnotationPlacement(win);
    }
    if ((vkey == VK_SPACE || vkey == VK_RETURN) && IsPlacingPolyLineAnnotation(win)) {
        return FinishPolyLineAnnotationPlacement(win);
    }
    if (vkey == VK_RETURN && KindOf(win) == AnnotPlacementKind::Highlighter) {
        return CancelAnnotationPlacement(win);
    }
    return false;
}

// --- the previews -----------------------------------------------------------

// Screen rect of a preview box anchored at the cursor. The screen -> page ->
// screen round trip can lose a pixel, which shows as a preview sitting a pixel
// off the mouse, so shift the box by however much the round trip drifted.
static Rect PlacementPreviewScreenRect(DisplayModel* dm, int pageNo, Point pt, PointF pagePt, RectF pageRect) {
    Rect r = dm->CvtToScreen(pageNo, pageRect);
    if (r.IsEmpty()) {
        return {};
    }
    Point anchor = dm->CvtToScreen(pageNo, pagePt);
    r.Offset(pt.x - anchor.x, pt.y - anchor.y);
    return r;
}

static void PaintMarker(gp::PaintCtx* ctx, Point p) {
    int size = std::max(DpiScale(6), 4);
    int half = size / 2;
    Rect r{p.x - half, p.y - half, size, size};
    CanvasFillRects(ctx, &r, 1, kPreviewWhite, 0xff, 1);
    CanvasDrawRect(ctx, r, kPreviewBlue, 1.5f);
}

static void PaintPointPlacement(MainWindow* win, gp::PaintCtx* ctx, DisplayModel* dm) {
    AnnotPlacementKind kind = KindOf(win);
    bool preview = kind == AnnotPlacementKind::Stamp || kind == AnnotPlacementKind::Caret ||
                   kind == AnnotPlacementKind::FileAttachment;
    if (!preview || !dm) {
        return;
    }
    AnnotPlacement& p = win->annotPlacement;
    Point pt = p.pos;
    int pageNo = dm->GetPageNoByPoint(pt);
    if (!dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo)) {
        return;
    }
    PointF pagePt = dm->CvtFromScreen(pt, pageNo);
    RectF pageRect;
    if (kind == AnnotPlacementKind::Stamp) {
        pageRect = {pagePt.x, pagePt.y, kStampAnnotDefaultDx, kStampAnnotDefaultDy};
    } else if (kind == AnnotPlacementKind::Caret) {
        pageRect = {pagePt.x, pagePt.y - (kCaretAnnotDefaultDy / 2.f), kCaretAnnotDefaultDx, kCaretAnnotDefaultDy};
    } else {
        pageRect = {pagePt.x, pagePt.y, kFileAttachmentAnnotDefaultDx, kFileAttachmentAnnotDefaultDy};
    }
    Rect r = PlacementPreviewScreenRect(dm, pageNo, pt, pagePt, pageRect);
    if (r.IsEmpty()) {
        return;
    }

    if (kind == AnnotPlacementKind::Stamp) {
        Color red = 0xc82828; // 200, 40, 40
        CanvasFillRects(ctx, &r, 1, red, 40, 0);
        CanvasDrawRect(ctx, r, red, (float)std::max(DpiScale(2), 1));
        float fontDy = std::max((float)r.dy * 0.45f, 8.f);
        Size ts = CanvasMeasureText(ctx, StrL("DRAFT"), fontDy);
        Point at{r.x + ((r.dx - ts.dx) / 2), r.y + ((r.dy - ts.dy) / 2)};
        CanvasDrawText(ctx, StrL("DRAFT"), at, red, fontDy);
        return;
    }
    if (kind == AnnotPlacementKind::Caret) {
        Color blue = MkRgb(0, 80, 200);
        float w = (float)std::max(DpiScale(2), 1);
        int xMid = r.x + (r.dx / 2);
        CanvasDrawLine(ctx, Point{r.x, r.y + r.dy}, Point{xMid, r.y}, blue, w);
        CanvasDrawLine(ctx, Point{xMid, r.y}, Point{r.x + r.dx, r.y + r.dy}, blue, w);
        return;
    }
    // MuPDF's default FileAttachment is a 16x16 yellow PushPin icon
    Color yellow = 0xdcb414; // 220, 180, 20
    CanvasFillRects(ctx, &r, 1, yellow, 80, 0);
    CanvasDrawRect(ctx, r, yellow, 1);
    Color dark = 0x282828;
    int cx = r.x + (r.dx / 2);
    int head = std::max(r.dx / 5, 2);
    CanvasDrawEllipse(ctx, Rect{cx - head, r.y + head, head * 2, head * 2}, dark, 1);
    CanvasDrawLine(ctx, Point{cx, r.y + (head * 3)}, Point{cx, r.y + r.dy - head}, dark, 1);
}

// Where the free text preview box currently is on screen; empty when the
// cursor isn't over a visible page.
static Rect FreeTextPlacementScreenRect(MainWindow* win, DisplayModel* dm) {
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

// White "paper" with the text drawn on it, the size of the annotation that a
// click will create.
static void PaintFreeTextPlacement(MainWindow* win, gp::PaintCtx* ctx, DisplayModel* dm) {
    if (KindOf(win) != AnnotPlacementKind::FreeText) {
        return;
    }
    AnnotPlacement& p = win->annotPlacement;
    Rect r = FreeTextPlacementScreenRect(win, dm);
    if (r.IsEmpty()) {
        return;
    }

    AnnotCreateArgs args;
    FreeTextPlacementArgs(p.cmdId, args);
    float scale = (float)r.dy / p.rect.dy;
    float pad = FreeTextPadding(args) * scale;
    float fontDy = std::max((float)FreeTextFontSize(args) * scale, 4.f);
    Color textCol = args.col.parsedOk ? args.col.col : 0x000000;
    Color bgCol = args.bgCol.parsedOk ? args.bgCol.col : 0xffffff;

    CanvasFillRects(ctx, &r, 1, bgCol, 0xff, 0);
    float bw = args.borderWidth > 0 ? std::max((float)args.borderWidth * scale, 1.f) : 1.f;
    CanvasDrawRect(ctx, r, textCol, bw);

    Str content = FreeTextPlacementContent(args);
    Size ts = CanvasMeasureText(ctx, content, fontDy);
    int x = r.x + (int)pad;
    if (args.quadding == kQuaddingCenter) {
        x = r.x + ((r.dx - ts.dx) / 2);
    } else if (args.quadding == kQuaddingRight) {
        x = r.x + r.dx - (int)pad - ts.dx;
    }
    CanvasDrawText(ctx, content, Point{x, r.y + ((r.dy - ts.dy) / 2)}, textCol, fontDy);
}

static void PaintLinePlacement(MainWindow* win, gp::PaintCtx* ctx, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!IsPlacingLineAnnotation(win) || !dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo)) {
        return;
    }
    Point start = dm->CvtToScreen(pageNo, p.start);
    CanvasDrawLine(ctx, start, p.end, kPreviewBlue, (float)std::max(DpiScale(2), 1));
    PaintMarker(ctx, start);
}

static void PaintPolyLinePlacement(MainWindow* win, gp::PaintCtx* ctx, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!IsPlacingPolyLineAnnotation(win) || len(p.points) == 0 || !dm->ValidPageNo(pageNo) ||
        !dm->PageVisible(pageNo)) {
        return;
    }
    float w = (float)std::max(DpiScale(2), 1);
    Point previous = dm->CvtToScreen(pageNo, p.points[0]);
    for (int i = 1; i < len(p.points); i++) {
        Point current = dm->CvtToScreen(pageNo, p.points[i]);
        CanvasDrawLine(ctx, previous, current, kPreviewBlue, w);
        PaintMarker(ctx, previous);
        previous = current;
    }
    CanvasDrawLine(ctx, previous, p.end, kPreviewBlue, w);
    PaintMarker(ctx, previous);
}

static void PaintShapePlacement(MainWindow* win, gp::PaintCtx* ctx, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!IsPlacingShapeAnnotation(win) || !dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo)) {
        return;
    }
    Rect rect = ShapePlacementScreenRect(p, dm);
    if (rect.IsEmpty()) {
        return;
    }
    float w = (float)std::max(DpiScale(2), 1);
    if (p.circle) {
        CanvasDrawEllipse(ctx, rect, kPreviewBlue, w);
    } else {
        CanvasDrawRect(ctx, rect, kPreviewBlue, w);
    }
    PaintMarker(ctx, dm->CvtToScreen(pageNo, p.start));
}

static void PaintInkPlacement(MainWindow* win, gp::PaintCtx* ctx, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!IsPlacingInkAnnotation(win) || !dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo) || len(p.points) == 0) {
        return;
    }
    // the stroke the ink button's drop-down is set to make: its color at its
    // opacity, as wide as the saved stroke will be at this zoom
    Color col = GetParsedColor(gSettings->annotations.inkColor, kInkDefaultColor);
    float width = (float)std::max(DpiScale(2), 1);
    int bw = gSettings->annotations.inkBorderWidth;
    if (bw > 0) {
        width = std::max(1.f, (float)bw * PxPerPagePt(dm, pageNo));
    }
    int pointIdx = 0;
    for (int count : p.strokeCounts) {
        if (count <= 0 || pointIdx >= len(p.points)) {
            continue;
        }
        Point previous;
        for (int i = 0; i < count && pointIdx < len(p.points); i++) {
            Point pt = dm->CvtToScreen(pageNo, p.points[pointIdx++]);
            if (i > 0) {
                CanvasDrawLine(ctx, previous, pt, col, width);
            } else if (count == 1) {
                int dotSize = std::max(DpiScale(3), 2);
                Rect r{pt.x - dotSize / 2, pt.y - dotSize / 2, dotSize, dotSize};
                CanvasFillRects(ctx, &r, 1, col, GetAlpha(col) == 0 ? 0xff : GetAlpha(col), 0);
            }
            previous = pt;
        }
    }
}

void PaintAnnotationPlacement(MainWindow* win, gp::PaintCtx* ctx, DisplayModel* dm) {
    if (!win || !dm || !IsPlacingAnnotation(win)) {
        return;
    }
    PaintPointPlacement(win, ctx, dm);
    PaintFreeTextPlacement(win, ctx, dm);
    PaintLinePlacement(win, ctx, dm);
    PaintPolyLinePlacement(win, ctx, dm);
    PaintShapePlacement(win, ctx, dm);
    PaintInkPlacement(win, ctx, dm);
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

// orig compares GetCursor() to IDC_CROSS or the SVG cursor. ng records the
// cursor it asked for: the note and the ink tool are native cursors on
// Windows, everything else is the cross.
static bool PlacementDumpCursor(MainWindow* win, AnnotPlacementKind kind, bool active) {
    if (!win || !active) {
        return false;
    }
    if (kind == AnnotPlacementKind::Text && win->nativeCursor == NativeCursor::TextAnnotationPlacement) {
        return true;
    }
    if (kind == AnnotPlacementKind::Ink && win->nativeCursor == NativeCursor::InkAnnotationPlacement) {
        return true;
    }
    return win->canvasCursor == (int)gp::CursorKind::Crosshair;
}

static TempStr PointPlacementDumpLineTemp(MainWindow* win, AnnotPlacementKind kind, Str key, bool svgCursor) {
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

// Same lines as orig's AnnotationPlacementStateTemp. Tests match on the name.
TempStr AnnotationPlacementStateTemp(MainWindow* win) {
    str::Builder out;
    out.Append(PointPlacementDumpLineTemp(win, AnnotPlacementKind::Text, StrL("textPlacement"), true));
    out.Append(PointPlacementDumpLineTemp(win, AnnotPlacementKind::FreeText, StrL("freeTextPlacement"), false));
    out.Append(PointPlacementDumpLineTemp(win, AnnotPlacementKind::Stamp, StrL("stampPlacement"), false));
    out.Append(PointPlacementDumpLineTemp(win, AnnotPlacementKind::Caret, StrL("caretPlacement"), false));
    out.Append(
        PointPlacementDumpLineTemp(win, AnnotPlacementKind::FileAttachment, StrL("fileAttachmentPlacement"), false));

    if (!win) {
        out.Append(StrL("freeTextPreview rect=0,0,0,0\n"));
        out.Append(
            StrL("linePlacement active=0 notification=0 cursor=0 started=0 cmd=0 page=-1 start=0,0 end=0,0 "
                 "message=\n"));
        out.Append(
            StrL("polyLinePlacement active=0 notification=0 cursor=0 points=0 cmd=0 page=-1 end=0,0 "
                 "message=\n"));
        out.Append(
            StrL("shapePlacement active=0 notification=0 cursor=0 circle=0 mouseDown=0 dragged=0 constrain=0 "
                 "cmd=0 page=-1 preview=0,0,0,0 message=\n"));
        out.Append(
            StrL("inkPlacement active=0 notification=0 cursor=0 mouseDown=0 strokes=0 points=0 cmd=0 page=-1 "
                 "message=\n"));
        out.Append(StrL("highlighterPlacement active=0 notification=0 cmd=0 message=\n"));
        return ToStrTemp(out);
    }

    AnnotPlacement& p = win->annotPlacement;
    {
        // tests click the frame; the preview is stored in canvas pixels
        Rect preview;
        if (KindOf(win) == AnnotPlacementKind::FreeText) {
            float s = CanvasScale(win);
            if (s <= 0.f) {
                s = 1.f;
            }
            preview = FreeTextPlacementScreenRect(win, win->AsFixed());
            preview.x += (int)((float)win->canvasRc.x / s + 0.5f);
            preview.y += (int)((float)win->canvasRc.y / s + 0.5f);
        }
        out.Append(fmt("freeTextPreview rect=%d,%d,%d,%d\n", preview.x, preview.y, preview.dx, preview.dy));
    }
    bool line = IsPlacingLineAnnotation(win);
    bool poly = IsPlacingPolyLineAnnotation(win);
    bool shape = IsPlacingShapeAnnotation(win);
    bool ink = IsPlacingInkAnnotation(win);
    {
        NotificationWnd* notif = line ? GetNotificationForGroup(win, kNotifLineAnnotationPlacement) : nullptr;
        Str message = NotificationGetMessageTemp(notif);
        bool cursor = PlacementDumpCursor(win, AnnotPlacementKind::Line, line);
        bool started = line && p.pageNo > 0;
        PointF start = line ? p.start : PointF{};
        Point end = line ? p.end : Point{};
        out.Append(
            fmt("linePlacement active=%d notification=%d cursor=%d started=%d cmd=%d page=%d start=%g,%g "
                "end=%d,%d message=%s\n",
                line ? 1 : 0, notif ? 1 : 0, cursor ? 1 : 0, started ? 1 : 0, line ? p.cmdId : 0, line ? p.pageNo : -1,
                start.x, start.y, end.x, end.y, message));
    }
    {
        NotificationWnd* notif = poly ? GetNotificationForGroup(win, kNotifPolyLineAnnotationPlacement) : nullptr;
        Str message = NotificationGetMessageTemp(notif);
        bool cursor = PlacementDumpCursor(win, AnnotPlacementKind::PolyLine, poly);
        Point end = poly ? p.end : Point{};
        out.Append(
            fmt("polyLinePlacement active=%d notification=%d cursor=%d points=%d cmd=%d page=%d end=%d,%d "
                "message=%s\n",
                poly ? 1 : 0, notif ? 1 : 0, cursor ? 1 : 0, poly ? len(p.points) : 0, poly ? p.cmdId : 0,
                poly ? p.pageNo : -1, end.x, end.y, message));
    }
    {
        NotificationWnd* notif = shape ? GetNotificationForGroup(win, kNotifShapeAnnotationPlacement) : nullptr;
        Str message = NotificationGetMessageTemp(notif);
        bool cursor = PlacementDumpCursor(win, AnnotPlacementKind::Shape, shape);
        Rect preview;
        DisplayModel* dm = win->AsFixed();
        if (shape && dm && dm->ValidPageNo(p.pageNo)) {
            preview = ShapePlacementScreenRect(p, dm);
        }
        out.Append(
            fmt("shapePlacement active=%d notification=%d cursor=%d circle=%d mouseDown=%d dragged=%d "
                "constrain=%d cmd=%d page=%d preview=%d,%d,%d,%d message=%s\n",
                shape ? 1 : 0, notif ? 1 : 0, cursor ? 1 : 0, shape && p.circle ? 1 : 0, shape && p.mouseDown ? 1 : 0,
                shape && p.didDrag ? 1 : 0, shape && p.constrain ? 1 : 0, shape ? p.cmdId : 0, shape ? p.pageNo : -1,
                preview.x, preview.y, preview.dx, preview.dy, message));
    }
    {
        NotificationWnd* notif = ink ? GetNotificationForGroup(win, kNotifInkAnnotationPlacement) : nullptr;
        Str message = NotificationGetMessageTemp(notif);
        bool cursor = PlacementDumpCursor(win, AnnotPlacementKind::Ink, ink);
        out.Append(
            fmt("inkPlacement active=%d notification=%d cursor=%d mouseDown=%d strokes=%d points=%d cmd=%d "
                "page=%d message=%s\n",
                ink ? 1 : 0, notif ? 1 : 0, cursor ? 1 : 0, ink && p.mouseDown ? 1 : 0, ink ? len(p.strokeCounts) : 0,
                ink ? len(p.points) : 0, ink ? p.cmdId : 0, ink ? p.pageNo : -1, message));
    }
    {
        bool on = KindOf(win) == AnnotPlacementKind::Highlighter;
        NotificationWnd* notif = on ? GetNotificationForGroup(win, kNotifHighlighterPlacement) : nullptr;
        Str message = NotificationGetMessageTemp(notif);
        out.Append(fmt("highlighterPlacement active=%d notification=%d cmd=%d message=%s\n", on ? 1 : 0, notif ? 1 : 0,
                       on ? p.cmdId : 0, message));
    }
    return ToStrTemp(out);
}

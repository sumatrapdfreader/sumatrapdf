/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/BitManip.h"
#include "base/Pixmap.h"
#include "base/GdiPlusUtil.h"
#include "base/GuessFileType.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Gfx.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/win/WinGui.h"

#include "Settings.h"
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
#include "Canvas.h"
#include "Selection.h"
#include "SelectTextKeyboard.h"
#include "Toolbar.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "Commands.h"

#include "AnnotPlacement.h"

static Kind kNotifPointAnnotationPlacement = "notifTextAnnotationPlacement";
static Kind kNotifLineAnnotationPlacement = "notifLineAnnotationPlacement";
static Kind kNotifPolyLineAnnotationPlacement = "notifPolyLineAnnotationPlacement";
static Kind kNotifShapeAnnotationPlacement = "notifShapeAnnotationPlacement";
static Kind kNotifInkAnnotationPlacement = "notifInkAnnotationPlacement";

// MuPDF's default stamp is {12,12,12+190,12+50}; caret is {12,12,12+18,12+15}
// with the caret mark at the middle of the left edge; file attachment is
// {12,12,12+16,12+16}.
constexpr float kStampAnnotDefaultDx = 190.f;
constexpr float kStampAnnotDefaultDy = 50.f;
constexpr float kCaretAnnotDefaultDx = 18.f;
constexpr float kCaretAnnotDefaultDy = 15.f;
constexpr float kFileAttachmentAnnotDefaultDx = 16.f;
constexpr float kFileAttachmentAnnotDefaultDy = 16.f;

// Free text is placed like a stamp: a preview box the size of the annotation
// follows the cursor and a click creates it there. MuPDF lays free text out
// with padding = 2 * border width, a 1.2 * font size line height and a
// 0.8 * font size baseline (pdf_write_free_text_appearance), so a box that
// fits one line of the placeholder text is that tall.
constexpr float kFreeTextLineHeight = 1.2f;
// MeasureString already includes generous side bearings; a little more keeps
// MuPDF from wrapping the text we previewed on a single line
constexpr float kFreeTextWidthSlack = 1.02f;
// measure at a big size and scale down: GDI+ rounds a lot at 12 px
constexpr float kFreeTextMeasureSize = 96.f;

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

// Size, in page units, of a box that fits one line of the annotation's text.
static SizeF FreeTextPlacementPageSize(const AnnotCreateArgs& args) {
    float fontSize = (float)FreeTextFontSize(args);
    float pad = FreeTextPadding(args);
    float dx = 0;
    HDC hdc = GetDC(nullptr);
    {
        Gdiplus::Graphics gs(hdc);
        // Arial has Helvetica's metrics, which is what MuPDF's "Helv" is
        Gdiplus::Font font(L"Arial", kFreeTextMeasureSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        WStr text = ToWStrTemp(FreeTextPlacementContent(args));
        // MeasureString with no layout rect: no wrapping, and its generous
        // side bearings keep us from under-measuring vs MuPDF's Helvetica
        RectF measured = MeasureTextStandard(&gs, &font, text);
        dx = measured.dx * fontSize / kFreeTextMeasureSize;
    }
    ReleaseDC(nullptr, hdc);
    dx = (dx * kFreeTextWidthSlack) + (2 * pad) + 2.f;
    float dy = (kFreeTextLineHeight * fontSize) + (2 * pad);
    return {dx, dy};
}

static HCURSOR gCursorTextAnnotationPlacement = nullptr;
static int gCursorTextAnnotationPlacementDx = 0;
static int gCursorTextAnnotationPlacementDy = 0;
static Color gCursorTextAnnotationPlacementColor = 0;

static HCURSOR gCursorInkAnnotationPlacement = nullptr;
static int gCursorInkAnnotationPlacementDx = 0;
static int gCursorInkAnnotationPlacementDy = 0;
static Color gCursorInkAnnotationPlacementColor = 0;

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

static HCURSOR CreateSvgPlacementCursor(const char* icon, int dx, int dy, Color color, DWORD hotspotX, DWORD hotspotY) {
    Pixmap* px = GetCachedPixmapForSvg(Str(icon), dx, dy, color);
    if (!px || !px->hbmp) {
        return nullptr;
    }

    int maskBytesPerRow = ((dx + 15) / 16) * 2;
    u8* maskBits = AllocArray<u8>(maskBytesPerRow * dy);
    HBITMAP hbmpMask = CreateBitmap(dx, dy, 1, 1, maskBits);
    free(maskBits);
    if (!hbmpMask) {
        return nullptr;
    }

    ICONINFO ii{};
    ii.fIcon = FALSE;
    ii.xHotspot = hotspotX;
    ii.yHotspot = hotspotY;
    ii.hbmMask = hbmpMask;
    ii.hbmColor = px->hbmp;
    HCURSOR cursor = (HCURSOR)CreateIconIndirect(&ii);
    DeleteObject(hbmpMask);
    return cursor;
}

static HCURSOR GetTextAnnotationPlacementCursor() {
    int dx = std::max(ToolbarIconSize(), DpiGetSystemMetrics(SM_CXCURSOR));
    int dy = std::max(ToolbarIconSize(), DpiGetSystemMetrics(SM_CYCURSOR));
    Color color = ThemeWindowTextColor();
    if (gCursorTextAnnotationPlacement && dx == gCursorTextAnnotationPlacementDx &&
        dy == gCursorTextAnnotationPlacementDy && color == gCursorTextAnnotationPlacementColor) {
        return gCursorTextAnnotationPlacement;
    }
    HCURSOR cursor = CreateSvgPlacementCursor(gIconAnnotText, dx, dy, color, 0, 0);
    if (!cursor) {
        return gCursorTextAnnotationPlacement;
    }
    if (gCursorTextAnnotationPlacement) {
        DestroyCursor(gCursorTextAnnotationPlacement);
    }
    gCursorTextAnnotationPlacement = cursor;
    gCursorTextAnnotationPlacementDx = dx;
    gCursorTextAnnotationPlacementDy = dy;
    gCursorTextAnnotationPlacementColor = color;
    return gCursorTextAnnotationPlacement;
}

static HCURSOR GetInkAnnotationPlacementCursor() {
    int dx = std::max(ToolbarIconSize(), DpiGetSystemMetrics(SM_CXCURSOR));
    int dy = std::max(ToolbarIconSize(), DpiGetSystemMetrics(SM_CYCURSOR));
    Color color = ThemeWindowTextColor();
    if (gCursorInkAnnotationPlacement && dx == gCursorInkAnnotationPlacementDx &&
        dy == gCursorInkAnnotationPlacementDy && color == gCursorInkAnnotationPlacementColor) {
        return gCursorInkAnnotationPlacement;
    }
    DWORD hotspotX = (DWORD)((4 * dx) / 24);
    DWORD hotspotY = (DWORD)((20 * dy) / 24);
    HCURSOR cursor = CreateSvgPlacementCursor(gIconEditAnnotations, dx, dy, color, hotspotX, hotspotY);
    if (!cursor) {
        return gCursorInkAnnotationPlacement;
    }
    if (gCursorInkAnnotationPlacement) {
        DestroyCursor(gCursorInkAnnotationPlacement);
    }
    gCursorInkAnnotationPlacement = cursor;
    gCursorInkAnnotationPlacementDx = dx;
    gCursorInkAnnotationPlacementDy = dy;
    gCursorInkAnnotationPlacementColor = color;
    return cursor;
}

void DeleteAnnotationPlacementCursors() {
    if (gCursorTextAnnotationPlacement) {
        DestroyCursor(gCursorTextAnnotationPlacement);
    }
    gCursorTextAnnotationPlacement = nullptr;
    gCursorTextAnnotationPlacementDx = 0;
    gCursorTextAnnotationPlacementDy = 0;
    gCursorTextAnnotationPlacementColor = 0;

    if (gCursorInkAnnotationPlacement) {
        DestroyCursor(gCursorInkAnnotationPlacement);
    }
    gCursorInkAnnotationPlacement = nullptr;
    gCursorInkAnnotationPlacementDx = 0;
    gCursorInkAnnotationPlacementDy = 0;
    gCursorInkAnnotationPlacementColor = 0;
}

static void SetTextAnnotationPlacementCursor() {
    HCURSOR cursor = GetTextAnnotationPlacementCursor();
    if (cursor) {
        SetCursor(cursor);
    } else {
        SetCursorCached(IDC_CROSS);
    }
}

static void SetInkAnnotationPlacementCursor() {
    HCURSOR cursor = GetInkAnnotationPlacementCursor();
    if (cursor) {
        SetCursor(cursor);
    } else {
        SetCursorCached(IDC_CROSS);
    }
}

static void SetPlacementCursor(MainWindow* win) {
    switch (KindOf(win)) {
        case AnnotPlacementKind::Text:
            SetTextAnnotationPlacementCursor();
            break;
        case AnnotPlacementKind::Ink:
            SetInkAnnotationPlacementCursor();
            break;
        case AnnotPlacementKind::FreeText:
        case AnnotPlacementKind::Stamp:
        case AnnotPlacementKind::Caret:
        case AnnotPlacementKind::FileAttachment:
        case AnnotPlacementKind::Line:
        case AnnotPlacementKind::PolyLine:
        case AnnotPlacementKind::Shape:
            SetCursorCached(IDC_CROSS);
            break;
        default:
            break;
    }
}

static bool HasPreview(AnnotPlacementKind kind) {
    return kind == AnnotPlacementKind::FreeText || kind == AnnotPlacementKind::Stamp ||
           kind == AnnotPlacementKind::Caret || kind == AnnotPlacementKind::FileAttachment ||
           kind == AnnotPlacementKind::Line || kind == AnnotPlacementKind::PolyLine ||
           kind == AnnotPlacementKind::Shape || kind == AnnotPlacementKind::Ink;
}

static Str PlacementNotification(AnnotPlacementKind kind, bool circle, int cmdId) {
    if (OrigCommandId(cmdId) == CmdCreateAnnotRedact) {
        return _TRA("Mark content for redaction. Drag or click twice. **Esc** to cancel.");
    }
    switch (kind) {
        case AnnotPlacementKind::Stamp:
            return _TRA("Place stamp annotation. **Esc** to cancel.");
        case AnnotPlacementKind::Caret:
            return _TRA("Place caret annotation. **Esc** to cancel.");
        case AnnotPlacementKind::FileAttachment:
            return _TRA("Place file attachment. **Esc** to cancel.");
        case AnnotPlacementKind::Text:
            return _TRA("Place text annotation. **Esc** to cancel.");
        case AnnotPlacementKind::FreeText:
            return _TRA("Place free text annotation. **Esc** to cancel.");
        case AnnotPlacementKind::Line:
            return _TRA("Place line annotation. **Esc** to cancel.");
        case AnnotPlacementKind::PolyLine:
            return _TRA(
                "Place polyline annotation. **Double-click**, **right-click**, **Space**, or **Enter** to finish. "
                "**Esc** to cancel.");
        case AnnotPlacementKind::Shape:
            return circle
                       ? _TRA(
                             "Place circle annotation. Drag or click twice. **Shift** for a circle. **Esc** to cancel.")
                       : _TRA(
                             "Place rectangle annotation. Drag or click twice. **Shift** for a square. **Esc** to "
                             "cancel.");
        case AnnotPlacementKind::Ink:
            return _TRA("Draw ink annotation. **Enter** to finish. **Esc** to cancel.");
        default:
            return {};
    }
}

static void RestoreCanvasCursor(MainWindow* win) {
    if (win && win->hwndCanvas) {
        SendMessageW(win->hwndCanvas, WM_SETCURSOR, (WPARAM)win->hwndCanvas, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    }
}

static void ReleasePlacementCapture(MainWindow* win) {
    if (!win || !win->annotPlacement.mouseDown) {
        return;
    }
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
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
        RemoveNotificationsForGroup(win->hwndCanvas, group);
    }
    HideAnnotationHoverOverlay(win);
    ScheduleRepaint(win, 0);
    RestoreCanvasCursor(win);
    return true;
}

static void CommitPlacementCommand(MainWindow* win, Point pt) {
    int cmdId = win->annotPlacement.cmdId;
    WPARAM wp = MAKEWPARAM(cmdId, kAnnotationPlacementCommandCode);
    SendMessageW(win->hwndFrame, WM_COMMAND, wp, MAKELPARAM(pt.x, pt.y));
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
        Point pt = dm->CvtToScreen(p.pageNo, p.points.Last());
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
        Point pt = dm->CvtToScreen(p.pageNo, p.points.Last());
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
        p.pos = HwndGetCursorPos(win->hwndCanvas);
    }
    if (kind == AnnotPlacementKind::FreeText) {
        // rect holds the preview box size (page units); the position comes
        // from the cursor
        AnnotCreateArgs args;
        FreeTextPlacementArgs(cmdId, args);
        SizeF size = FreeTextPlacementPageSize(args);
        p.rect = {0, 0, size.dx, size.dy};
    }

    StopSelectTextWithKeyboard(win);
    DeleteOldSelectionInfo(win, true);
    if (tab->selectedAnnotation) {
        SetSelectedAnnotation(tab, nullptr);
    }
    win->annotationUnderCursor = nullptr;
    HideAnnotationHoverOverlay(win);
    ScheduleRepaint(win, 0);

    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = PlacementNotification(kind, p.circle, cmdId);
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = NotifGroupForKind(kind);
    args.corner = NotifCorner::BottomBar;
    args.warning = true;
    args.tab = tab;
    ShowNotification(args);

    HwndSetFocus(win->hwndFrame);
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    if (HwndClientRect(win->hwndCanvas).Contains(pt)) {
        SetPlacementCursor(win);
    }
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

// The first page click anchors the preview. A second click on that page
// executes the original command with both endpoints; a click anywhere else
// cancels the mode because a PDF line annotation cannot span pages.
static bool HandleLineClick(MainWindow* win, Point pt) {
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
    p.end = pt;
    CommitPlacementCommand(win, pt);
    return true;
}

// Each page click commits a vertex and starts previewing the next segment.
// A click off that page cancels the whole path, matching line placement.
static bool HandlePolyLineClick(MainWindow* win, Point pt) {
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
    p.points.Append(dm->CvtFromScreen(pt, pageNo));
    p.end = pt;
    ScheduleRepaint(win, 0);
    return true;
}

static bool HandleShapeDown(MainWindow* win, Point pt, WPARAM key) {
    if (!IsPlacingShapeAnnotation(win)) {
        return false;
    }
    HwndSetFocus(win->hwndFrame);
    DisplayModel* dm = win->AsFixed();
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = dm ? dm->GetPageNoByPoint(pt) : -1;
    bool started = p.pageNo > 0;
    if (!dm || !dm->ValidPageNo(pageNo) || (started && pageNo != p.pageNo)) {
        CancelAnnotationPlacement(win);
        return true;
    }

    p.end = pt;
    p.constrain = bit::IsMaskSet(key, (WPARAM)MK_SHIFT);
    if (started) {
        CommitShapePlacement(win);
        return true;
    }

    p.pageNo = pageNo;
    p.start = dm->CvtFromScreen(pt, pageNo);
    p.mouseDown = true;
    p.didDrag = false;
    SetCapture(win->hwndCanvas);
    ScheduleRepaint(win, 0);
    return true;
}

static bool HandleShapeUp(MainWindow* win, Point pt, WPARAM key) {
    if (!IsPlacingShapeAnnotation(win) || !win->annotPlacement.mouseDown) {
        return false;
    }
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
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
    p.constrain = bit::IsMaskSet(key, (WPARAM)MK_SHIFT);
    Point start = dm->CvtToScreen(pageNo, p.start);
    if (p.didDrag || IsDragDistance(pt.x, start.x, pt.y, start.y)) {
        CommitShapePlacement(win);
    } else {
        ScheduleRepaint(win, 0);
    }
    return true;
}

static bool AppendInkPoint(MainWindow* win, DisplayModel* dm, Point pt) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!dm || !dm->ValidPageNo(pageNo) || dm->GetPageNoByPoint(pt) != pageNo || len(p.strokeCounts) == 0) {
        return false;
    }
    if (p.strokeCounts.Last() > 0) {
        Point previous = dm->CvtToScreen(pageNo, p.points.Last());
        if (previous == pt) {
            return false;
        }
    }
    p.points.Append(dm->CvtFromScreen(pt, pageNo));
    p.strokeCounts.Last()++;
    HwndInvalidate(win->hwndCanvas);
    return true;
}

static bool HandleInkDown(MainWindow* win, Point pt) {
    if (!IsPlacingInkAnnotation(win)) {
        return false;
    }
    HwndSetFocus(win->hwndFrame);
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
    p.strokeCounts.Append(0);
    p.mouseDown = true;
    AppendInkPoint(win, dm, pt);
    SetCapture(win->hwndCanvas);
    return true;
}

static bool HandleInkUp(MainWindow* win, Point pt) {
    if (!IsPlacingInkAnnotation(win) || !win->annotPlacement.mouseDown) {
        return false;
    }
    AppendInkPoint(win, win->AsFixed(), pt);
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
    win->annotPlacement.mouseDown = false;
    HwndInvalidate(win->hwndCanvas);
    return true;
}

bool AnnotationPlacementOnLeftDown(MainWindow* win, Point pt, WPARAM key) {
    switch (KindOf(win)) {
        case AnnotPlacementKind::Ink:
            HandleInkDown(win, pt);
            return true;
        case AnnotPlacementKind::Shape:
            HandleShapeDown(win, pt, key);
            return true;
        case AnnotPlacementKind::Line:
            HwndSetFocus(win->hwndFrame);
            HandleLineClick(win, pt);
            return true;
        case AnnotPlacementKind::PolyLine:
            HwndSetFocus(win->hwndFrame);
            HandlePolyLineClick(win, pt);
            return true;
        case AnnotPlacementKind::Text:
        case AnnotPlacementKind::FreeText:
        case AnnotPlacementKind::Stamp:
        case AnnotPlacementKind::Caret:
        case AnnotPlacementKind::FileAttachment:
            HwndSetFocus(win->hwndFrame);
            PlacePointAnnotationAt(win, pt);
            return true;
        default:
            return false;
    }
}

bool AnnotationPlacementOnLeftUp(MainWindow* win, Point pt, WPARAM key) {
    if (HandleInkUp(win, pt)) {
        return true;
    }
    return HandleShapeUp(win, pt, key);
}

bool AnnotationPlacementOnLeftDblClk(MainWindow* win, Point pt) {
    if (IsPlacingInkAnnotation(win)) {
        HandleInkDown(win, pt);
        return true;
    }
    if (IsPlacingPolyLineAnnotation(win)) {
        HwndSetFocus(win->hwndFrame);
        FinishPolyLineAnnotationPlacement(win);
        return true;
    }
    return false;
}

bool AnnotationPlacementOnRightDown(MainWindow* win) {
    if (!IsPlacingPolyLineAnnotation(win)) {
        return false;
    }
    HwndSetFocus(win->hwndFrame);
    FinishPolyLineAnnotationPlacement(win);
    return true;
}

bool AnnotationPlacementOnMouseMove(MainWindow* win, Point pt, WPARAM key) {
    if (!IsPlacingAnnotation(win)) {
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
    HideAnnotationHoverOverlay(win);
    SetPlacementCursor(win);

    AnnotPlacement& p = win->annotPlacement;
    switch (p.kind) {
        case AnnotPlacementKind::Ink:
            if (p.mouseDown && bit::IsMaskSet(key, (WPARAM)MK_LBUTTON)) {
                AppendInkPoint(win, dm, pt);
            }
            break;
        case AnnotPlacementKind::Shape:
            if (p.pageNo > 0) {
                bool constrain = bit::IsMaskSet(key, (WPARAM)MK_SHIFT);
                if (p.mouseDown) {
                    Point start = dm->CvtToScreen(p.pageNo, p.start);
                    if (IsDragDistance(pt.x, start.x, pt.y, start.y)) {
                        p.didDrag = true;
                    }
                }
                if (pt != p.end || constrain != p.constrain) {
                    p.end = pt;
                    p.constrain = constrain;
                    ScheduleRepaint(win, 0);
                }
            }
            break;
        case AnnotPlacementKind::Line:
            if (p.pageNo > 0 && pt != p.end) {
                p.end = pt;
                ScheduleRepaint(win, 0);
            }
            break;
        case AnnotPlacementKind::PolyLine:
            if (len(p.points) > 0 && pt != p.end) {
                p.end = pt;
                ScheduleRepaint(win, 0);
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

bool AnnotationPlacementOnSetCursor(MainWindow* win) {
    if (!IsPlacingAnnotation(win)) {
        return false;
    }
    SetPlacementCursor(win);
    return true;
}

bool AnnotationPlacementOnKeyDown(MainWindow* win, WPARAM key) {
    if (!win || IsCtrlPressed() || IsShiftPressed() || IsAltPressed()) {
        return false;
    }
    if (key == VK_RETURN && IsPlacingInkAnnotation(win)) {
        return FinishInkAnnotationPlacement(win);
    }
    if ((key == VK_SPACE || key == VK_RETURN) && IsPlacingPolyLineAnnotation(win)) {
        return FinishPolyLineAnnotationPlacement(win);
    }
    return false;
}

bool AnnotationPlacementSkipAccelerator(MainWindow* win, WPARAM key) {
    if (!win || IsCtrlPressed() || IsShiftPressed() || IsAltPressed()) {
        return false;
    }
    if (key == VK_RETURN && IsPlacingInkAnnotation(win)) {
        return true;
    }
    if ((key == VK_SPACE || key == VK_RETURN) && IsPlacingPolyLineAnnotation(win)) {
        return true;
    }
    return false;
}

// Screen rect of a preview box anchored at the cursor. The screen -> page ->
// screen round trip can lose a pixel (both conversions bias by 0.499 and then
// truncate), which shows as a preview sitting a pixel off the mouse, so shift
// the box by however much the round trip drifted.
static Rect PlacementPreviewScreenRect(DisplayModel* dm, int pageNo, Point pt, PointF pagePt, RectF pageRect) {
    Rect r = dm->CvtToScreen(pageNo, pageRect);
    if (r.IsEmpty()) {
        return {};
    }
    Point anchor = dm->CvtToScreen(pageNo, pagePt);
    r.Offset(pt.x - anchor.x, pt.y - anchor.y);
    return r;
}

static void PaintPointPlacement(MainWindow* win, HDC hdc, DisplayModel* dm) {
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

    Gdiplus::Graphics gs(hdc);
    gs.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if (kind == AnnotPlacementKind::Stamp) {
        Gdiplus::Color red(180, 200, 40, 40);
        Gdiplus::Pen pen(red, (Gdiplus::REAL)std::max(DpiScale(2), 1));
        Gdiplus::SolidBrush fill(Gdiplus::Color(40, 200, 40, 40));
        gs.FillRectangle(&fill, r.x, r.y, r.dx, r.dy);
        gs.DrawRectangle(&pen, r.x, r.y, r.dx, r.dy);
        Gdiplus::REAL fontDy = (Gdiplus::REAL)std::max(r.dy * 0.45f, 8.f);
        Gdiplus::Font font(L"Arial", fontDy, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush text(red);
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF tr((Gdiplus::REAL)r.x, (Gdiplus::REAL)r.y, (Gdiplus::REAL)r.dx, (Gdiplus::REAL)r.dy);
        gs.DrawString(L"DRAFT", -1, &font, tr, &fmt, &text);
    } else if (kind == AnnotPlacementKind::Caret) {
        Gdiplus::Color blue(220, 0, 80, 200);
        Gdiplus::Pen pen(blue, (Gdiplus::REAL)std::max(DpiScale(2), 1));
        int x0 = r.x;
        int yTop = r.y;
        int xMid = r.x + (r.dx / 2);
        int x1 = r.x + r.dx;
        int yBot = r.y + r.dy;
        gs.DrawLine(&pen, x0, yBot, xMid, yTop);
        gs.DrawLine(&pen, xMid, yTop, x1, yBot);
    } else {
        // MuPDF's default FileAttachment is a 16x16 yellow PushPin icon.
        Gdiplus::Color yellow(220, 220, 180, 20);
        Gdiplus::Pen border(yellow, (Gdiplus::REAL)std::max(DpiScale(1), 1));
        Gdiplus::SolidBrush fill(Gdiplus::Color(80, 220, 180, 20));
        gs.FillRectangle(&fill, r.x, r.y, r.dx, r.dy);
        gs.DrawRectangle(&border, r.x, r.y, r.dx, r.dy);
        Gdiplus::Pen pin(Gdiplus::Color(220, 40, 40, 40), (Gdiplus::REAL)std::max(DpiScale(1), 1));
        int cx = r.x + r.dx / 2;
        int head = std::max(r.dx / 5, 2);
        gs.DrawEllipse(&pin, cx - head, r.y + head, head * 2, head * 2);
        gs.DrawLine(&pin, cx, r.y + head * 3, cx, r.y + r.dy - head);
    }
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
// click will create. An unset background is transparent in the PDF, but a box
// reads better than floating text while positioning it.
static void PaintFreeTextPlacement(MainWindow* win, HDC hdc, DisplayModel* dm) {
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
    Gdiplus::Color textCol = args.col.parsedOk ? GdiRgbFromColor(args.col.col) : Gdiplus::Color(255, 0, 0, 0);
    Gdiplus::Color bgCol = args.bgCol.parsedOk ? GdiRgbFromColor(args.bgCol.col) : Gdiplus::Color(255, 255, 255, 255);

    Gdiplus::Graphics gs(hdc);
    gs.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush bg(bgCol);
    gs.FillRectangle(&bg, r.x, r.y, r.dx, r.dy);
    if (args.borderWidth > 0) {
        float bw = std::max((float)args.borderWidth * scale, 1.f);
        Gdiplus::Pen pen(textCol, bw);
        gs.DrawRectangle(&pen, (Gdiplus::REAL)r.x + bw / 2, (Gdiplus::REAL)r.y + bw / 2, (Gdiplus::REAL)r.dx - bw,
                         (Gdiplus::REAL)r.dy - bw);
    } else {
        // no border on the annotation itself, so mark the extent faintly
        Gdiplus::Pen pen(Gdiplus::Color(120, 0, 0, 0), 1.f);
        pen.SetDashStyle(Gdiplus::DashStyleDash);
        gs.DrawRectangle(&pen, (Gdiplus::REAL)r.x, (Gdiplus::REAL)r.y, (Gdiplus::REAL)r.dx - 1,
                         (Gdiplus::REAL)r.dy - 1);
    }

    Gdiplus::Font font(L"Arial", fontDy, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush text(textCol);
    Gdiplus::StringFormat sf(Gdiplus::StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | Gdiplus::StringFormatFlagsNoWrap);
    if (args.quadding == kQuaddingCenter) {
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    } else if (args.quadding == kQuaddingRight) {
        sf.SetAlignment(Gdiplus::StringAlignmentFar);
    }
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF tr((Gdiplus::REAL)r.x + pad, (Gdiplus::REAL)r.y + pad, (Gdiplus::REAL)r.dx - (2 * pad),
                      (Gdiplus::REAL)r.dy - (2 * pad));
    WStr content = ToWStrTemp(FreeTextPlacementContent(args));
    gs.DrawString(content.s, content.len, &font, tr, &sf, &text);
}

static void PaintLinePlacement(MainWindow* win, HDC hdc, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!IsPlacingLineAnnotation(win) || !dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo)) {
        return;
    }
    Point start = dm->CvtToScreen(pageNo, p.start);
    Point end = p.end;

    Gdiplus::Graphics gs(hdc);
    gs.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Color blue(255, 0, 80, 200);
    Gdiplus::Pen pen(blue, (Gdiplus::REAL)std::max(DpiScale(2), 1));
    gs.DrawLine(&pen, start.x, start.y, end.x, end.y);

    int markerSize = std::max(DpiScale(6), 4);
    int markerHalf = markerSize / 2;
    Gdiplus::SolidBrush fill(Gdiplus::Color(255, 255, 255, 255));
    gs.FillEllipse(&fill, start.x - markerHalf, start.y - markerHalf, markerSize, markerSize);
    gs.DrawEllipse(&pen, start.x - markerHalf, start.y - markerHalf, markerSize, markerSize);
}

static void PaintPolyLinePlacement(MainWindow* win, HDC hdc, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!IsPlacingPolyLineAnnotation(win) || len(p.points) == 0 || !dm->ValidPageNo(pageNo) ||
        !dm->PageVisible(pageNo)) {
        return;
    }

    Gdiplus::Graphics gs(hdc);
    gs.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Color blue(255, 0, 80, 200);
    Gdiplus::Pen pen(blue, (Gdiplus::REAL)std::max(DpiScale(2), 1));
    int markerSize = std::max(DpiScale(6), 4);
    int markerHalf = markerSize / 2;
    Gdiplus::SolidBrush fill(Gdiplus::Color(255, 255, 255, 255));

    Point previous = dm->CvtToScreen(pageNo, p.points[0]);
    for (int i = 1; i < len(p.points); i++) {
        Point current = dm->CvtToScreen(pageNo, p.points[i]);
        gs.DrawLine(&pen, previous.x, previous.y, current.x, current.y);
        gs.FillEllipse(&fill, previous.x - markerHalf, previous.y - markerHalf, markerSize, markerSize);
        gs.DrawEllipse(&pen, previous.x - markerHalf, previous.y - markerHalf, markerSize, markerSize);
        previous = current;
    }
    Point end = p.end;
    gs.DrawLine(&pen, previous.x, previous.y, end.x, end.y);
    gs.FillEllipse(&fill, previous.x - markerHalf, previous.y - markerHalf, markerSize, markerSize);
    gs.DrawEllipse(&pen, previous.x - markerHalf, previous.y - markerHalf, markerSize, markerSize);
}

static void PaintShapePlacement(MainWindow* win, HDC hdc, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!IsPlacingShapeAnnotation(win) || !dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo)) {
        return;
    }
    Rect rect = ShapePlacementScreenRect(p, dm);
    if (rect.IsEmpty()) {
        return;
    }

    Gdiplus::Graphics gs(hdc);
    gs.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Color blue(255, 0, 80, 200);
    Gdiplus::Pen pen(blue, (Gdiplus::REAL)std::max(DpiScale(2), 1));
    if (p.circle) {
        gs.DrawEllipse(&pen, rect.x, rect.y, rect.dx, rect.dy);
    } else {
        gs.DrawRectangle(&pen, rect.x, rect.y, rect.dx, rect.dy);
    }

    Point start = dm->CvtToScreen(pageNo, p.start);
    int markerSize = std::max(DpiScale(6), 4);
    int markerHalf = markerSize / 2;
    Gdiplus::SolidBrush fill(Gdiplus::Color(255, 255, 255, 255));
    gs.FillEllipse(&fill, start.x - markerHalf, start.y - markerHalf, markerSize, markerSize);
    gs.DrawEllipse(&pen, start.x - markerHalf, start.y - markerHalf, markerSize, markerSize);
}

static void PaintInkPlacement(MainWindow* win, HDC hdc, DisplayModel* dm) {
    AnnotPlacement& p = win->annotPlacement;
    int pageNo = p.pageNo;
    if (!IsPlacingInkAnnotation(win) || !dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo) || len(p.points) == 0) {
        return;
    }

    Gdiplus::Graphics gs(hdc);
    gs.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Color blue(255, 0, 80, 200);
    Gdiplus::REAL width = (Gdiplus::REAL)std::max(DpiScale(2), 1);
    Gdiplus::Pen pen(blue, width);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);

    int pointIdx = 0;
    for (int count : p.strokeCounts) {
        if (count <= 0 || pointIdx >= len(p.points)) {
            continue;
        }
        Point previous = dm->CvtToScreen(pageNo, p.points[pointIdx++]);
        if (count == 1) {
            int dotSize = std::max(DpiScale(3), 2);
            int dotHalf = dotSize / 2;
            Gdiplus::SolidBrush brush(blue);
            gs.FillEllipse(&brush, previous.x - dotHalf, previous.y - dotHalf, dotSize, dotSize);
            continue;
        }
        for (int i = 1; i < count && pointIdx < len(p.points); i++) {
            Point current = dm->CvtToScreen(pageNo, p.points[pointIdx++]);
            gs.DrawLine(&pen, previous.x, previous.y, current.x, current.y);
            previous = current;
        }
    }
}

void PaintAnnotationPlacement(MainWindow* win, HDC hdc, DisplayModel* dm) {
    if (!win || !dm || !IsPlacingAnnotation(win)) {
        return;
    }
    PaintPointPlacement(win, hdc, dm);
    PaintFreeTextPlacement(win, hdc, dm);
    PaintLinePlacement(win, hdc, dm);
    PaintPolyLinePlacement(win, hdc, dm);
    PaintShapePlacement(win, hdc, dm);
    PaintInkPlacement(win, hdc, dm);
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
            pt = dm->CvtToScreen(pageNo, p.points.Last());
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
            pt = dm->CvtToScreen(pageNo, p.points.Last());
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

static bool PointDumpCursor(MainWindow* win, AnnotPlacementKind kind, bool active) {
    if (!active) {
        return false;
    }
    if (kind == AnnotPlacementKind::Text) {
        return gCursorTextAnnotationPlacement && GetCursor() == gCursorTextAnnotationPlacement;
    }
    return GetCursor() == GetCachedCursor(IDC_CROSS);
}

static TempStr PointPlacementDumpLineTemp(MainWindow* win, AnnotPlacementKind kind, Str key, bool svgCursor) {
    bool active = KindOf(win) == kind;
    if (!win || !win->hwndCanvas) {
        if (svgCursor) {
            return fmt("%s active=0 notification=0 cursor=0 cmd=0 message=\n", key);
        }
        return fmt("%s active=0 notification=0 cursor=0 preview=0 cmd=0 message=\n", key);
    }
    NotificationWnd* notif =
        active ? GetNotificationForGroup(win->hwndCanvas, kNotifPointAnnotationPlacement) : nullptr;
    Str message = NotificationGetMessageTemp(notif);
    bool cursor = PointDumpCursor(win, kind, active);
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

TempStr AnnotationPlacementStateTemp(MainWindow* win) {
    str::Builder out;
    out.Append(PointPlacementDumpLineTemp(win, AnnotPlacementKind::Text, StrL("textPlacement"), true));
    out.Append(PointPlacementDumpLineTemp(win, AnnotPlacementKind::FreeText, StrL("freeTextPlacement"), false));
    out.Append(PointPlacementDumpLineTemp(win, AnnotPlacementKind::Stamp, StrL("stampPlacement"), false));
    out.Append(PointPlacementDumpLineTemp(win, AnnotPlacementKind::Caret, StrL("caretPlacement"), false));
    out.Append(
        PointPlacementDumpLineTemp(win, AnnotPlacementKind::FileAttachment, StrL("fileAttachmentPlacement"), false));

    if (!win || !win->hwndCanvas) {
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
        return ToStrTemp(out);
    }

    AnnotPlacement& p = win->annotPlacement;
    {
        Rect preview;
        if (KindOf(win) == AnnotPlacementKind::FreeText) {
            preview = FreeTextPlacementScreenRect(win, win->AsFixed());
        }
        out.Append(fmt("freeTextPreview rect=%d,%d,%d,%d\n", preview.x, preview.y, preview.dx, preview.dy));
    }
    bool line = IsPlacingLineAnnotation(win);
    bool poly = IsPlacingPolyLineAnnotation(win);
    bool shape = IsPlacingShapeAnnotation(win);
    bool ink = IsPlacingInkAnnotation(win);
    {
        NotificationWnd* notif = GetNotificationForGroup(win->hwndCanvas, kNotifLineAnnotationPlacement);
        Str message = NotificationGetMessageTemp(notif);
        bool crossCursor = GetCursor() == GetCachedCursor(IDC_CROSS);
        bool started = line && p.pageNo > 0;
        PointF start = line ? p.start : PointF{};
        Point end = line ? p.end : Point{};
        out.Append(
            fmt("linePlacement active=%d notification=%d cursor=%d started=%d cmd=%d page=%d start=%g,%g end=%d,%d "
                "message=%s\n",
                line ? 1 : 0, notif ? 1 : 0, crossCursor ? 1 : 0, started ? 1 : 0, line ? p.cmdId : 0,
                line ? p.pageNo : -1, start.x, start.y, end.x, end.y, message));
    }
    {
        NotificationWnd* notif = GetNotificationForGroup(win->hwndCanvas, kNotifPolyLineAnnotationPlacement);
        Str message = NotificationGetMessageTemp(notif);
        bool crossCursor = GetCursor() == GetCachedCursor(IDC_CROSS);
        Point end = poly ? p.end : Point{};
        out.Append(
            fmt("polyLinePlacement active=%d notification=%d cursor=%d points=%d cmd=%d page=%d end=%d,%d "
                "message=%s\n",
                poly ? 1 : 0, notif ? 1 : 0, crossCursor ? 1 : 0, poly ? len(p.points) : 0, poly ? p.cmdId : 0,
                poly ? p.pageNo : -1, end.x, end.y, message));
    }
    {
        NotificationWnd* notif = GetNotificationForGroup(win->hwndCanvas, kNotifShapeAnnotationPlacement);
        Str message = NotificationGetMessageTemp(notif);
        bool crossCursor = GetCursor() == GetCachedCursor(IDC_CROSS);
        Rect preview;
        DisplayModel* dm = win->AsFixed();
        if (shape && dm && dm->ValidPageNo(p.pageNo)) {
            preview = ShapePlacementScreenRect(p, dm);
        }
        out.Append(
            fmt("shapePlacement active=%d notification=%d cursor=%d circle=%d mouseDown=%d dragged=%d constrain=%d "
                "cmd=%d page=%d preview=%d,%d,%d,%d message=%s\n",
                shape ? 1 : 0, notif ? 1 : 0, crossCursor ? 1 : 0, shape && p.circle ? 1 : 0,
                shape && p.mouseDown ? 1 : 0, shape && p.didDrag ? 1 : 0, shape && p.constrain ? 1 : 0,
                shape ? p.cmdId : 0, shape ? p.pageNo : -1, preview.x, preview.y, preview.dx, preview.dy, message));
    }
    {
        NotificationWnd* notif = GetNotificationForGroup(win->hwndCanvas, kNotifInkAnnotationPlacement);
        Str message = NotificationGetMessageTemp(notif);
        bool penCursor = gCursorInkAnnotationPlacement && GetCursor() == gCursorInkAnnotationPlacement;
        out.Append(fmt(
            "inkPlacement active=%d notification=%d cursor=%d mouseDown=%d strokes=%d points=%d cmd=%d page=%d "
            "message=%s\n",
            ink ? 1 : 0, notif ? 1 : 0, penCursor ? 1 : 0, ink && p.mouseDown ? 1 : 0, ink ? len(p.strokeCounts) : 0,
            ink ? len(p.points) : 0, ink ? p.cmdId : 0, ink ? p.pageNo : -1, message));
    }
    return ToStrTemp(out);
}

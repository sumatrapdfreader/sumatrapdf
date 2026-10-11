/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
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
#include "Canvas.h"
#include "Selection.h"
#include "SelectTextKeyboard.h"
#include "Commands.h"
#include "Toolbar.h"
#include "Translations.h"
#include "SvgIcons.h"

#include "AnnotPlacement.h"
#include "AnnotPlacementCommon.h"
#include "AnnotPlacementCursors.h"

// measure at a big size and scale down: GDI+ rounds a lot at 12 px
constexpr float kFreeTextMeasureSize = 96.f;

// Size, in page units, of a box that fits the annotation's text.
SizeF FreeTextPlacementPageSize(const AnnotCreateArgs& args) {
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
    // a text snippet can have several lines
    Str content = FreeTextPlacementContent(args);
    int nLines = 1;
    for (int i = 0; i < len(content); i++) {
        nLines += content.s[i] == '\n' ? 1 : 0;
    }
    float dy = ((float)nLines * kFreeTextLineHeight * fontSize) + (2 * pad);
    return {dx, dy};
}

HCURSOR CreateSvgPlacementCursor(const char* icon, int dx, int dy, Color color, DWORD hotspotX, DWORD hotspotY) {
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

void SetPlacementCursor(MainWindow* win) {
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

// what placement asks of the window system; ng has its own
void PlacementFocusFrame(MainWindow* win) {
    HwndSetFocus(win->hwndFrame);
}

void PlacementCaptureMouse(MainWindow* win) {
    SetCapture(win->hwndCanvas);
}

// runs the create-annotation command the placement was started for
void PlacementRunCreateCmd(MainWindow* win, int cmdId, Point pt) {
    WPARAM wp = MAKEWPARAM(cmdId, kAnnotationPlacementCommandCode);
    SendMessageW(win->hwndFrame, WM_COMMAND, wp, MAKELPARAM(pt.x, pt.y));
}

void PlacementReleaseMouse(MainWindow* win) {
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
}

void RestoreCanvasCursor(MainWindow* win) {
    if (win && win->hwndCanvas) {
        SendMessageW(win->hwndCanvas, WM_SETCURSOR, (WPARAM)win->hwndCanvas, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    }
}

void ReleasePlacementCapture(MainWindow* win) {
    if (!win || !win->annotPlacement.mouseDown) {
        return;
    }
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
}

static void OnPlacementNotifClosed(MainWindow* win, NotificationClosedEvent* ev) {
    RemoveNotification(ev->wnd);
    if (!win || !IsPlacingAnnotation(win)) {
        return;
    }
    if (IsPlacingInkAnnotation(win) || IsPlacingPolyLineAnnotation(win)) {
        FinishAnnotationPlacement(win);
        return;
    }
    CancelAnnotationPlacement(win);
}

bool CloseAnnotationPlacementHint(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return false;
    }
    Kind group = NotifGroupForKind(KindOf(win));
    if (!group) {
        return false;
    }
    NotificationWnd* notif = GetNotificationForGroup(win, group);
    if (!notif) {
        return false;
    }
    CloseNotification(notif);
    return true;
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

    // the highlighter works on a keyboard selection; other modes take the pointer
    if (!(kind == AnnotPlacementKind::Highlighter && SelectTextWithKeyboardActive(win))) {
        StopSelectTextWithKeyboard(win);
        DeleteOldSelectionInfo(win, true);
    }
    if (tab->selectedAnnotation) {
        SetSelectedAnnotation(tab, nullptr);
    }
    win->annotationUnderCursor = nullptr;
    HideAnnotationHoverOverlay(win);
    ScheduleRepaint(win, 0);

    // edit PDF toolbar buttons are disabled for the duration of the mode
    HideToolbarHoverDropdown(win);
    ToolbarUpdateStateForWindow(win, false);

    NotificationCreateArgs args;
    args.win = win;
    args.msg = PlacementNotification(kind, p.circle, cmdId);
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = NotifGroupForKind(kind);
    args.corner = NotifCorner::BottomBar;
    args.warning = true;
    args.tab = tab;
    args.onClosed = MkFunc1(OnPlacementNotifClosed, win);
    ShowNotification(args);

    PlacementFocusFrame(win);
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    if (HwndClientRect(win->hwndCanvas).Contains(pt)) {
        SetPlacementCursor(win);
    }
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
            ScheduleRepaint(win, 0);
        }
    }
    HideAnnotationHoverOverlay(win);
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
                bool constrain = isShift;
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
            if (p.pageNo > 0) {
                Point start = dm->CvtToScreen(p.pageNo, p.start);
                bool shift = isShift;
                Point end = shift ? SnapLineEndpoint(start, pt) : pt;
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
    if (key == VK_RETURN && KindOf(win) == AnnotPlacementKind::Highlighter) {
        return CancelAnnotationPlacement(win);
    }
    return false;
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
        Gdiplus::REAL fontDy = (Gdiplus::REAL)std::max((float)r.dy * 0.45f, 8.f);
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
        int cx = r.x + (r.dx / 2);
        int head = std::max(r.dx / 5, 2);
        gs.DrawEllipse(&pin, cx - head, r.y + head, head * 2, head * 2);
        gs.DrawLine(&pin, cx, r.y + (head * 3), cx, r.y + r.dy - head);
    }
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
        gs.DrawRectangle(&pen, (Gdiplus::REAL)r.x + (bw / 2), (Gdiplus::REAL)r.y + (bw / 2), (Gdiplus::REAL)r.dx - bw,
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
    // the stroke the ink button's drop-down is set to make: its color at its
    // opacity, as wide as the saved stroke will be at this zoom
    Color col = GetParsedColor(gSettings->annotations.inkColor, kInkDefaultColor);
    u8 r, g, b;
    UnpackColor(col, r, g, b);
    u8 a = GetAlpha(col);
    // no alpha written out is opaque
    Gdiplus::Color strokeCol(a == 0 ? 255 : a, r, g, b);
    Gdiplus::REAL width = (Gdiplus::REAL)std::max(DpiScale(2), 1);
    int bw = gSettings->annotations.inkBorderWidth;
    if (bw > 0) {
        width = (Gdiplus::REAL)std::max(1.f, (float)bw * PxPerPagePt(dm, pageNo));
    }
    Gdiplus::Pen pen(strokeCol, width);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    // one polyline per stroke: DrawLine per segment stacks round-cap alpha at
    // every vertex, so a 40% highlighter looks closer to solid while dragging
    pen.SetLineJoin(Gdiplus::LineJoinRound);

    Vec<Gdiplus::Point> pts;
    VecGrow(pts, len(p.points));
    int pointIdx = 0;
    for (int count : p.strokeCounts) {
        if (count <= 0 || pointIdx >= len(p.points)) {
            continue;
        }
        pts.len = 0;
        for (int i = 0; i < count && pointIdx < len(p.points); i++) {
            Point pt = dm->CvtToScreen(pageNo, p.points[pointIdx++]);
            VecAppend(pts, Gdiplus::Point(pt.x, pt.y));
        }
        if (len(pts) == 0) {
            continue;
        }
        if (len(pts) == 1) {
            int dotSize = std::max(DpiScale(3), 2);
            int dotHalf = dotSize / 2;
            Gdiplus::SolidBrush brush(strokeCol);
            gs.FillEllipse(&brush, pts[0].X - dotHalf, pts[0].Y - dotHalf, dotSize, dotSize);
            continue;
        }
        gs.DrawLines(&pen, pts.els, len(pts));
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

static bool PointDumpCursor(AnnotPlacementKind kind, bool active) {
    if (!active) {
        return false;
    }
    if (kind == AnnotPlacementKind::Text) {
        return gCursorTextAnnotationPlacement && GetCursor() == gCursorTextAnnotationPlacement;
    }
    return GetCursor() == GetCachedCursor(IDC_CROSS);
}

bool PlacementDumpCursor(MainWindow*, AnnotPlacementKind kind, bool active) {
    return PointDumpCursor(kind, active);
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
        out.Append(StrL("highlighterPlacement active=0 notification=0 cmd=0 message=\n"));
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
        NotificationWnd* notif = GetNotificationForGroup(win, kNotifLineAnnotationPlacement);
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
        NotificationWnd* notif = GetNotificationForGroup(win, kNotifPolyLineAnnotationPlacement);
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
        NotificationWnd* notif = GetNotificationForGroup(win, kNotifShapeAnnotationPlacement);
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
        NotificationWnd* notif = GetNotificationForGroup(win, kNotifInkAnnotationPlacement);
        Str message = NotificationGetMessageTemp(notif);
        bool penCursor = gCursorInkAnnotationPlacement && GetCursor() == gCursorInkAnnotationPlacement;
        out.Append(fmt(
            "inkPlacement active=%d notification=%d cursor=%d mouseDown=%d strokes=%d points=%d cmd=%d page=%d "
            "message=%s\n",
            ink ? 1 : 0, notif ? 1 : 0, penCursor ? 1 : 0, ink && p.mouseDown ? 1 : 0, ink ? len(p.strokeCounts) : 0,
            ink ? len(p.points) : 0, ink ? p.cmdId : 0, ink ? p.pageNo : -1, message));
    }
    {
        bool on = KindOf(win) == AnnotPlacementKind::Highlighter;
        NotificationWnd* notif = GetNotificationForGroup(win, kNotifHighlighterPlacement);
        Str message = NotificationGetMessageTemp(notif);
        out.Append(fmt("highlighterPlacement active=%d notification=%d cmd=%d message=%s\n", on ? 1 : 0, notif ? 1 : 0,
                       on ? p.cmdId : 0, message));
    }
    return ToStrTemp(out);
}

// the win32 mouse messages carry the modifier and button state in wParam
bool AnnotationPlacementOnLeftDown(MainWindow* win, Point pt, WPARAM key) {
    bool isShift = bit::IsMaskSet(key, (WPARAM)MK_SHIFT);
    bool isCtrl = bit::IsMaskSet(key, (WPARAM)MK_CONTROL);
    return AnnotationPlacementOnLeftDown(win, pt, isShift, isCtrl);
}

bool AnnotationPlacementOnLeftUp(MainWindow* win, Point pt, WPARAM key) {
    return AnnotationPlacementOnLeftUp(win, pt, bit::IsMaskSet(key, (WPARAM)MK_SHIFT));
}

bool AnnotationPlacementOnMouseMove(MainWindow* win, Point pt, WPARAM key) {
    bool isShift = bit::IsMaskSet(key, (WPARAM)MK_SHIFT);
    bool lButtonDown = bit::IsMaskSet(key, (WPARAM)MK_LBUTTON);
    return AnnotationPlacementOnMouseMove(win, pt, isShift, lButtonDown);
}

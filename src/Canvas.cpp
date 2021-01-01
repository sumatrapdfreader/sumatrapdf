/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/LogDbg.h"
#include "utils/Timer.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "AppColors.h"
#include "utils/ScopedWin.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"
#include "wingui/FrameRateWnd.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineCreate.h"
#include "Doc.h"

#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "EbookController.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraConfig.h"
#include "TabInfo.h"
#include "SumatraPDF.h"
#include "EditAnnotations.h"
#include "WindowInfo.h"
#include "resource.h"
#include "Commands.h"
#include "Canvas.h"
#include "Caption.h"
#include "Menu.h"
#include "uia/Provider.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "SumatraAbout.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "Translations.h"

// these can be global, as the mouse wheel can't affect more than one window at once
static int gDeltaPerLine = 0;
// set when WM_MOUSEWHEEL has been passed on (to prevent recursion)
static bool gWheelMsgRedirect = false;

void UpdateDeltaPerLine() {
    ULONG ulScrollLines;
    BOOL ok = SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);
    if (!ok) {
        return;
    }
    // ulScrollLines usually equals 3 or 0 (for no scrolling) or -1 (for page scrolling)
    // WHEEL_DELTA equals 120, so iDeltaPerLine will be 40
    gDeltaPerLine = 0;
    if (ulScrollLines == (ULONG)-1) {
        gDeltaPerLine = -1;
    } else if (ulScrollLines != 0) {
        gDeltaPerLine = WHEEL_DELTA / ulScrollLines;
    }
}

///// methods needed for FixedPageUI canvases with document loaded /////

static void OnVScroll(WindowInfo* win, WPARAM wp) {
    CrashIf(!win->AsFixed());

    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(win->hwndCanvas, SB_VERT, &si);

    int currPos = si.nPos;
    auto ctrl = win->ctrl;
    int lineHeight = DpiScale(win->hwndCanvas, 16);
    bool isFitPage = (ZOOM_FIT_PAGE == ctrl->GetZoomVirtual());
    if (!IsContinuous(ctrl->GetDisplayMode()) && isFitPage) {
        lineHeight = 1;
    }

    USHORT msg = LOWORD(wp);
    switch (msg) {
        case SB_TOP:
            si.nPos = si.nMin;
            break;
        case SB_BOTTOM:
            si.nPos = si.nMax;
            break;
        case SB_LINEUP:
            si.nPos -= lineHeight;
            break;
        case SB_LINEDOWN:
            si.nPos += lineHeight;
            break;
        case SB_HPAGEUP:
            si.nPos -= si.nPage / 2;
            break;
        case SB_HPAGEDOWN:
            si.nPos += si.nPage / 2;
            break;
        case SB_PAGEUP:
            si.nPos -= si.nPage;
            break;
        case SB_PAGEDOWN:
            si.nPos += si.nPage;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
    GetScrollInfo(win->hwndCanvas, SB_VERT, &si);

    // If the position has changed or we're dealing with a touchpad scroll event,
    // scroll the window and update it
    if (si.nPos != currPos || msg == SB_THUMBTRACK) {
        win->AsFixed()->ScrollYTo(si.nPos);
    }
}

static void OnHScroll(WindowInfo* win, WPARAM wp) {
    CrashIf(!win->AsFixed());

    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(win->hwndCanvas, SB_HORZ, &si);

    int currPos = si.nPos;
    USHORT msg = LOWORD(wp);
    switch (msg) {
        case SB_LEFT:
            si.nPos = si.nMin;
            break;
        case SB_RIGHT:
            si.nPos = si.nMax;
            break;
        case SB_LINELEFT:
            si.nPos -= DpiScale(win->hwndCanvas, 16);
            break;
        case SB_LINERIGHT:
            si.nPos += DpiScale(win->hwndCanvas, 16);
            break;
        case SB_PAGELEFT:
            si.nPos -= si.nPage;
            break;
        case SB_PAGERIGHT:
            si.nPos += si.nPage;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, TRUE);
    GetScrollInfo(win->hwndCanvas, SB_HORZ, &si);

    // If the position has changed or we're dealing with a touchpad scroll event,
    // scroll the window and update it
    if (si.nPos != currPos || msg == SB_THUMBTRACK) {
        win->AsFixed()->ScrollXTo(si.nPos);
    }
}

static void DrawMovePattern(WindowInfo* win, Point pt, Size size) {
    HWND hwnd = win->hwndCanvas;
    HDC hdc = GetDC(hwnd);
    auto [x, y] = pt;
    auto [dx, dy] = size;
    x += win->annotationBeingMovedOffset.x;
    y += win->annotationBeingMovedOffset.y;
    SetBrushOrgEx(hdc, x, y, 0);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, win->brMovePattern);
    PatBlt(hdc, x, y, dx, dy, PATINVERT);
    SelectObject(hdc, hbrushOld);
    ReleaseDC(hwnd, hdc);
}

static void StartMouseDrag(WindowInfo* win, int x, int y, bool right = false) {
    SetCapture(win->hwndCanvas);
    win->mouseAction = MouseAction::Dragging;
    win->dragRightClick = right;
    win->dragPrevPos = Point(x, y);
    if (GetCursor()) {
        SetCursor(gCursorDrag);
    }
}

// return true if this was annotation dragging
static bool StopDraggingAnnotation(WindowInfo* win, int x, int y, bool aborted) {
    Annotation* annot = win->annotationOnLastButtonDown;
    if (!annot) {
        return false;
    }
    DrawMovePattern(win, win->dragPrevPos, win->annotationBeingMovedSize);
    if (aborted) {
        delete annot;
        win->annotationOnLastButtonDown = nullptr;
        return true;
    }

    DisplayModel* dm = win->AsFixed();
    x += win->annotationBeingMovedOffset.x;
    y += win->annotationBeingMovedOffset.y;
    Point pt{x, y};
    int pageNo = dm->GetPageNoByPoint(pt);
    // we can only move annotation within the same page
    if (pageNo == annot->PageNo()) {
        Rect rScreen{x, y, 1, 1};
        RectF r = dm->CvtFromScreen(rScreen, pageNo);
        RectF ar = annot->Rect();
        r.dx = ar.dx;
        r.dy = ar.dy;
        // dbglogf("prev rect: x=%.2f, y=%.2f, dx=%.2f, dy=%.2f\n", ar.x, ar.y, ar.dx, ar.dy);
        // dbglogf(" new rect: x=%.2f, y=%.2f, dx=%.2f, dy=%.2f\n", r.x, r.y, r.dx, r.dy);
        annot->SetRect(r);
        WindowInfoRerender(win);
        StartEditAnnotations(win->currentTab, annot);
    } else {
        delete annot;
    }
    win->annotationOnLastButtonDown = nullptr;
    return true;
}

static void StopMouseDrag(WindowInfo* win, int x, int y, bool aborted) {
    if (GetCapture() != win->hwndCanvas) {
        return;
    }

    if (GetCursor()) {
        SetCursorCached(IDC_ARROW);
    }
    ReleaseCapture();

    if (StopDraggingAnnotation(win, x, y, aborted)) {
        return;
    }

    if (aborted) {
        return;
    }

    Size drag(x - win->dragPrevPos.x, y - win->dragPrevPos.y);
    win->MoveDocBy(drag.dx, -2 * drag.dy);
}

void CancelDrag(WindowInfo* win) {
    auto pt = win->dragPrevPos;
    auto [x, y] = pt;
    StopMouseDrag(win, x, y, true);
    win->mouseAction = MouseAction::Idle;
    delete win->linkOnLastButtonDown;
    win->linkOnLastButtonDown = nullptr;
    SetCursorCached(IDC_ARROW);
}

bool IsDrag(int x1, int x2, int y1, int y2) {
    int dx = abs(x1 - x2);
    int dragDx = GetSystemMetrics(SM_CXDRAG);
    if (dx > dragDx) {
        return true;
    }

    int dy = abs(y1 - y2);
    int dragDy = GetSystemMetrics(SM_CYDRAG);
    if (dy > dragDy) {
        return true;
    }
    return false;
}

static void OnMouseMove(WindowInfo* win, int x, int y, [[maybe_unused]] WPARAM flags) {
    CrashIf(!win->AsFixed());

    if (win->presentation != PM_DISABLED) {
        if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
            SetCursor((HCURSOR) nullptr);
            return;
        }
        // shortly display the cursor if the mouse has moved and the cursor is hidden
        if (Point(x, y) != win->dragPrevPos && !GetCursor()) {
            if (win->mouseAction == MouseAction::Idle) {
                SetCursorCached(IDC_ARROW);
            } else {
                SendMessageW(win->hwndCanvas, WM_SETCURSOR, 0, 0);
            }
            SetTimer(win->hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, nullptr);
        }
    }

    if (win->dragStartPending) {
        if (!IsDrag(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        win->dragStartPending = false;
        delete win->linkOnLastButtonDown;
        win->linkOnLastButtonDown = nullptr;
    }

    Point prevPos = win->dragPrevPos;
    Point pos{x, y};
    Annotation* annot = win->annotationOnLastButtonDown;
    switch (win->mouseAction) {
        case MouseAction::Scrolling:
            win->yScrollSpeed = (y - win->dragStart.y) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
            win->xScrollSpeed = (x - win->dragStart.x) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
            break;
        case MouseAction::SelectingText:
            if (GetCursor()) {
                SetCursorCached(IDC_IBEAM);
            }
        /* fall through */
        case MouseAction::Selecting:
            win->selectionRect.dx = x - win->selectionRect.x;
            win->selectionRect.dy = y - win->selectionRect.y;
            OnSelectionEdgeAutoscroll(win, x, y);
            RepaintAsync(win, 0);
            break;
        case MouseAction::Dragging:
            if (annot) {
                Size size = win->annotationBeingMovedSize;
                DrawMovePattern(win, prevPos, size);
                DrawMovePattern(win, pos, size);
            } else {
                win->MoveDocBy(win->dragPrevPos.x - x, win->dragPrevPos.y - y);
            }
            break;
    }
    win->dragPrevPos = pos;

    NotificationWnd* wnd = win->notifications->GetForGroup(NG_CURSOR_POS_HELPER);
    if (!wnd) {
        return;
    }

    if (MouseAction::Selecting == win->mouseAction) {
        win->selectionMeasure = win->AsFixed()->CvtFromScreen(win->selectionRect).Size();
    }
    UpdateCursorPositionHelper(win, pos, wnd);
}

// clang-format off
static AnnotationType moveableAnnotations[] = {
    AnnotationType::Text,
    AnnotationType::Link,
    AnnotationType::FreeText,
    AnnotationType::Line,
    AnnotationType::Square,
    AnnotationType::Circle,
    AnnotationType::Polygon,
    AnnotationType::PolyLine,
    //AnnotationType::Highlight,
    //AnnotationType::Underline,
    //AnnotationType::Squiggly,
    //AnnotationType::StrikeOut,
    //AnnotationType::Redact,
    AnnotationType::Stamp,
    AnnotationType::Caret,
    AnnotationType::Ink,
    AnnotationType::Popup,
    AnnotationType::FileAttachment,
    AnnotationType::Sound,
    AnnotationType::Movie,
    //AnnotationType::Widget, // TODO: maybe moveble?
    AnnotationType::Screen,
    AnnotationType::PrinterMark,
    AnnotationType::TrapNet,
    AnnotationType::Watermark,
    AnnotationType::ThreeD,
    AnnotationType::Unknown,
};
// clang-format on

static void SetObjectUnderMouse(WindowInfo* win, int x, int y) {
    CrashIf(win->linkOnLastButtonDown);
    CrashIf(win->annotationOnLastButtonDown);
    DisplayModel* dm = win->AsFixed();
    Point pt{x, y};

    Annotation* annot = dm->GetAnnotationAtPos(pt, moveableAnnotations);
    if (annot) {
        win->annotationOnLastButtonDown = annot;
        CreateMovePatternLazy(win);
        RectF r = annot->Rect();
        int pageNo = dm->GetPageNoByPoint(pt);
        Rect rScreen = dm->CvtToScreen(pageNo, r);
        win->annotationBeingMovedSize = {rScreen.dx, rScreen.dy};
        int offsetX = rScreen.x - pt.x;
        int offsetY = rScreen.y - pt.y;
        win->annotationBeingMovedOffset = Point{offsetX, offsetY};
        DrawMovePattern(win, pt, win->annotationBeingMovedSize);
    }

    IPageElement* pageEl = dm->GetElementAtPos(pt);
    if (pageEl) {
        if (pageEl->Is(kindPageElementDest)) {
            win->linkOnLastButtonDown = pageEl;
        } else {
            delete pageEl;
        }
    }
}

static void OnMouseLeftButtonDown(WindowInfo* win, int x, int y, WPARAM key) {
    // lf("Left button clicked on %d %d", x, y);
    if (IsRightDragging(win)) {
        return;
    }

    if (MouseAction::Scrolling == win->mouseAction) {
        win->mouseAction = MouseAction::Idle;
        return;
    }

    // happened e.g. in crash 50539
    DebugCrashIf(win->mouseAction != MouseAction::Idle);
    CrashIf(!win->AsFixed());

    SetFocus(win->hwndFrame);

    SetObjectUnderMouse(win, x, y);

    win->dragStartPending = true;
    Point pt{x, y};
    win->dragStart = pt;

    // - without modifiers, clicking on text starts a text selection
    //   and clicking somewhere else starts a drag
    // - pressing Shift forces dragging
    // - pressing Ctrl forces a rectangular selection
    // - pressing Ctrl+Shift forces text selection
    // - not having CopySelection permission forces dragging
    bool isShift = IsShiftPressed();
    bool isCtrl = IsCtrlPressed();
    bool canCopy = HasPermission(Perm_CopySelection);
    bool isOverText = win->AsFixed()->IsOverText(pt);
    Annotation* annot = win->annotationOnLastButtonDown;
    if (annot || !canCopy || (isShift || !isOverText) && !isCtrl) {
        StartMouseDrag(win, x, y);
    } else {
        OnSelectionStart(win, x, y, key);
    }
}

static void OnMouseLeftButtonUp(WindowInfo* win, int x, int y, WPARAM key) {
    DisplayModel* dm = win->AsFixed();
    CrashIf(!dm);
    auto ma = win->mouseAction;
    if (MouseAction::Idle == ma || IsRightDragging(win)) {
        return;
    }
    CrashIf(MouseAction::Selecting != ma && MouseAction::SelectingText != ma && MouseAction::Dragging != ma);

    // TODO: should IsDrag() ever be true here? We should get mouse move first
    bool didDragMouse = !win->dragStartPending || IsDrag(x, win->dragStart.x, y, win->dragStart.y);
    if (MouseAction::Dragging == ma) {
        StopMouseDrag(win, x, y, !didDragMouse);
    } else {
        OnSelectionStop(win, x, y, !didDragMouse);
        if (MouseAction::Selecting == ma && win->showSelection) {
            win->selectionMeasure = dm->CvtFromScreen(win->selectionRect).Size();
        }
    }

    win->mouseAction = MouseAction::Idle;

    PointF ptPage = dm->CvtFromScreen(Point(x, y));

    // TODO: win->linkHandler->GotoLink might spin the event loop
    IPageElement* link = win->linkOnLastButtonDown;
    win->linkOnLastButtonDown = nullptr;
    defer {
        delete link;
    };

    TabInfo* tab = win->currentTab;
    if (didDragMouse) {
        // no-op
        return;
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        /* return from white/black screens in presentation mode */
        win->ChangePresentationMode(PM_ENABLED);
        return;
    }

    if (link && link->GetRect().Contains(ptPage)) {
        /* follow an active link */
        PageDestination* dest = link->AsLink();
        // highlight the clicked link (as a reminder of the last action once the user returns)
        if (dest && (kindDestinationLaunchURL == dest->Kind() || kindDestinationLaunchFile == dest->Kind())) {
            DeleteOldSelectionInfo(win, true);
            tab->selectionOnPage =
                SelectionOnPage::FromRectangle(dm, dm->CvtToScreen(link->GetPageNo(), link->GetRect()));
            win->showSelection = tab->selectionOnPage != nullptr;
            RepaintAsync(win, 0);
        }
        SetCursorCached(IDC_ARROW);
        win->linkHandler->GotoLink(dest);
        return;
    }

    if (win->showSelection) {
        /* if we had a selection and this was just a click, hide the selection */
        ClearSearchResult(win);
        return;
    }

    if (win->fwdSearchMark.show && gGlobalPrefs->forwardSearch.highlightPermanent) {
        /* if there's a permanent forward search mark, hide it */
        win->fwdSearchMark.show = false;
        RepaintAsync(win, 0);
        return;
    }

    if (PM_ENABLED == win->presentation) {
        /* in presentation mode, change pages on left/right-clicks */
        if ((key & MK_SHIFT)) {
            tab->ctrl->GoToPrevPage();
        } else {
            tab->ctrl->GoToNextPage();
        }
        return;
    }
}

static void OnMouseLeftButtonDblClk(WindowInfo* win, int x, int y, WPARAM key) {
    // lf("Left button clicked on %d %d", x, y);
    if (win->presentation && !(key & ~MK_LBUTTON)) {
        // in presentation mode, left clicks turn the page,
        // make two quick left clicks (AKA one double-click) turn two pages
        OnMouseLeftButtonDown(win, x, y, key);
        return;
    }

    bool dontSelect = false;
    if (gGlobalPrefs->enableTeXEnhancements && !(key & ~MK_LBUTTON)) {
        dontSelect = OnInverseSearch(win, x, y);
    }
    if (dontSelect) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (dm->IsOverText(Point(x, y))) {
        int pageNo = dm->GetPageNoByPoint(Point(x, y));
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(Point(x, y), pageNo);
            dm->textSelection->SelectWordAt(pageNo, pt.x, pt.y);
            UpdateTextSelection(win, false);
            RepaintAsync(win, 0);
        }
        return;
    }

    IPageElement* pageEl = dm->GetElementAtPos(Point(x, y));
    if (pageEl && pageEl->Is(kindPageElementDest)) {
        // speed up navigation in a file where navigation links are in a fixed position
        OnMouseLeftButtonDown(win, x, y, key);
    } else if (pageEl && pageEl->Is(kindPageElementImage)) {
        // select an image that could be copied to the clipboard
        Rect rc = dm->CvtToScreen(pageEl->GetPageNo(), pageEl->GetRect());

        DeleteOldSelectionInfo(win, true);
        win->currentTab->selectionOnPage = SelectionOnPage::FromRectangle(dm, rc);
        win->showSelection = win->currentTab->selectionOnPage != nullptr;
        RepaintAsync(win, 0);
    }
    delete pageEl;
}

static void OnMouseMiddleButtonDown(WindowInfo* win, int x, int y, [[maybe_unused]] WPARAM key) {
    // Handle message by recording placement then moving document as mouse moves.

    switch (win->mouseAction) {
        case MouseAction::Idle:
            win->mouseAction = MouseAction::Scrolling;

            // record current mouse position, the farther the mouse is moved
            // from this position, the faster we scroll the document
            win->dragStart = Point(x, y);
            SetCursorCached(IDC_SIZEALL);
            break;

        case MouseAction::Scrolling:
            win->mouseAction = MouseAction::Idle;
            break;
    }
}

static void OnMouseRightButtonDown(WindowInfo* win, int x, int y) {
    // lf("Right button clicked on %d %d", x, y);
    if (MouseAction::Scrolling == win->mouseAction) {
        win->mouseAction = MouseAction::Idle;
    } else if (win->mouseAction != MouseAction::Idle) {
        return;
    }
    CrashIf(!win->AsFixed());

    SetFocus(win->hwndFrame);

    win->dragStartPending = true;
    win->dragStart = Point(x, y);

    StartMouseDrag(win, x, y, true);
}

static void OnMouseRightButtonUp(WindowInfo* win, int x, int y, WPARAM key) {
    CrashIf(!win->AsFixed());
    if (!IsRightDragging(win)) {
        return;
    }

    int isDragXOrY = IsDrag(x, win->dragStart.x, y, win->dragStart.y);
    bool didDragMouse = !win->dragStartPending || isDragXOrY;
    StopMouseDrag(win, x, y, !didDragMouse);

    win->mouseAction = MouseAction::Idle;

    if (didDragMouse) {
        /* pass */;
    } else if (PM_ENABLED == win->presentation) {
        if ((key & MK_CONTROL)) {
            OnWindowContextMenu(win, x, y);
        } else if ((key & MK_SHIFT)) {
            win->ctrl->GoToNextPage();
        } else {
            win->ctrl->GoToPrevPage();
        }
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        win->ChangePresentationMode(PM_ENABLED);
    } else {
        OnWindowContextMenu(win, x, y);
    }
}

static void OnMouseRightButtonDblClick(WindowInfo* win, int x, int y, WPARAM key) {
    if (win->presentation && !(key & ~MK_RBUTTON)) {
        // in presentation mode, right clicks turn the page,
        // make two quick right clicks (AKA one double-click) turn two pages
        OnMouseRightButtonDown(win, x, y);
        return;
    }
}

#ifdef DRAW_PAGE_SHADOWS
#define BORDER_SIZE 1
#define SHADOW_OFFSET 4
static void PaintPageFrameAndShadow(HDC hdc, Rect& bounds, Rect& pageRect, bool presentation) {
    // Frame info
    Rect frame = bounds;
    frame.Inflate(BORDER_SIZE, BORDER_SIZE);

    // Shadow info
    Rect shadow = frame;
    shadow.Offset(SHADOW_OFFSET, SHADOW_OFFSET);
    if (frame.x < 0) {
        // the left of the page isn't visible, so start the shadow at the left
        int diff = std::min(-pageRect.x, SHADOW_OFFSET);
        shadow.x -= diff;
        shadow.dx += diff;
    }
    if (frame.y < 0) {
        // the top of the page isn't visible, so start the shadow at the top
        int diff = std::min(-pageRect.y, SHADOW_OFFSET);
        shadow.y -= diff;
        shadow.dy += diff;
    }

    // Draw shadow
    if (!presentation) {
        ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(COL_PAGE_SHADOW));
        FillRect(hdc, &shadow.ToRECT(), brush);
    }

    // Draw frame
    ScopedGdiObj<HPEN> pe(CreatePen(PS_SOLID, 1, presentation ? TRANSPARENT : COL_PAGE_FRAME));
    ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(GetCurrentTheme()->mainWindow.backgroundColor));
    SelectObject(hdc, pe);
    SelectObject(hdc, brush);
    Rectangle(hdc, frame.x, frame.y, frame.x + frame.dx, frame.y + frame.dy);
}
#else
static void PaintPageFrameAndShadow(HDC hdc, Rect& bounds, [[maybe_unused]] Rect& pageRect,
                                    [[maybe_unused]] bool presentation) {
    AutoDeletePen pen(CreatePen(PS_NULL, 0, 0));
    auto col = GetAppColor(AppColor::MainWindowBg);
    AutoDeleteBrush brush(CreateSolidBrush(col));
    ScopedSelectPen restorePen(hdc, pen);
    ScopedSelectObject restoreBrush(hdc, brush);
    Rectangle(hdc, bounds.x, bounds.y, bounds.x + bounds.dx + 1, bounds.y + bounds.dy + 1);
}
#endif

/* debug code to visualize links (can block while rendering) */
static void DebugShowLinks(DisplayModel* dm, HDC hdc) {
    if (!gDebugShowLinks) {
        return;
    }

    Rect viewPortRect(Point(), dm->GetViewPort().Size());
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x00, 0xff, 0xff));
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pageInfo = dm->GetPageInfo(pageNo);
        if (!pageInfo || !pageInfo->shown || 0.0 == pageInfo->visibleRatio) {
            continue;
        }

        Vec<IPageElement*>* els = dm->GetEngine()->GetElements(pageNo);
        if (!els) {
            continue;
        }

        for (size_t i = 0; i < els->size(); i++) {
            if (els->at(i)->Is(kindPageElementImage)) {
                continue;
            }
            Rect rect = dm->CvtToScreen(pageNo, els->at(i)->GetRect());
            Rect isect = viewPortRect.Intersect(rect);
            if (!isect.IsEmpty()) {
                PaintRect(hdc, isect);
            }
        }
        DeleteVecMembers(*els);
        delete els;
    }

    DeletePen(SelectObject(hdc, oldPen));

    if (dm->GetZoomVirtual() == ZOOM_FIT_CONTENT) {
        // also display the content box when fitting content
        pen = CreatePen(PS_SOLID, 1, RGB(0xff, 0x00, 0xff));
        oldPen = SelectObject(hdc, pen);

        for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
            PageInfo* pageInfo = dm->GetPageInfo(pageNo);
            if (!pageInfo->shown || 0.0 == pageInfo->visibleRatio) {
                continue;
            }

            auto cbbox = dm->GetEngine()->PageContentBox(pageNo);
            Rect rect = dm->CvtToScreen(pageNo, cbbox);
            PaintRect(hdc, rect);
        }

        DeletePen(SelectObject(hdc, oldPen));
    }
}

// cf. http://forums.fofou.org/sumatrapdf/topic?id=3183580
static void GetGradientColor(COLORREF a, COLORREF b, float perc, TRIVERTEX* tv) {
    u8 ar, ag, ab;
    u8 br, bg, bb;
    UnpackRgb(a, ar, ag, ab);
    UnpackRgb(b, br, bg, bb);

    tv->Red = (COLOR16)((ar + perc * (br - ar)) * 256);
    tv->Green = (COLOR16)((ag + perc * (bg - ag)) * 256);
    tv->Blue = (COLOR16)((ab + perc * (bb - ab)) * 256);
}

static void DrawDocument(WindowInfo* win, HDC hdc, RECT* rcArea) {
    CrashIf(!win->AsFixed());
    if (!win->AsFixed()) {
        return;
    }
    DisplayModel* dm = win->AsFixed();

    bool isImage = dm->GetEngine()->IsImageCollection();
    // draw comic books and single images on a black background
    // (without frame and shadow)
    bool paintOnBlackWithoutShadow = win->presentation || isImage;

    auto gcols = gGlobalPrefs->fixedPageUI.gradientColors;
    auto nGCols = gcols->size();
    if (paintOnBlackWithoutShadow) {
        ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(WIN_COL_BLACK));
        FillRect(hdc, rcArea, brush);
    } else if (0 == nGCols) {
        auto col = GetAppColor(AppColor::NoDocBg);
        ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(col));
        FillRect(hdc, rcArea, brush);
    } else {
        COLORREF colors[3];
        colors[0] = gcols->at(0);
        if (nGCols == 1) {
            colors[1] = colors[2] = colors[0];
        } else if (nGCols == 2) {
            colors[2] = gcols->at(1);
            colors[1] =
                RGB((GetRed(colors[0]) + GetRed(colors[2])) / 2, (GetGreen(colors[0]) + GetGreen(colors[2])) / 2,
                    (GetBlue(colors[0]) + GetBlue(colors[2])) / 2);
        } else {
            colors[1] = gcols->at(1);
            colors[2] = gcols->at(2);
        }
        Size size = dm->GetCanvasSize();
        float percTop = 1.0f * dm->GetViewPort().y / size.dy;
        float percBot = 1.0f * dm->GetViewPort().BR().y / size.dy;
        if (!IsContinuous(dm->GetDisplayMode())) {
            percTop += dm->CurrentPageNo() - 1;
            percTop /= dm->PageCount();
            percBot += dm->CurrentPageNo() - 1;
            percBot /= dm->PageCount();
        }
        Size vp = dm->GetViewPort().Size();
        TRIVERTEX tv[4] = {{0, 0}, {vp.dx, vp.dy / 2}, {0, vp.dy / 2}, {vp.dx, vp.dy}};
        GRADIENT_RECT gr[2] = {{0, 1}, {2, 3}};

        COLORREF col0 = colors[0];
        COLORREF col1 = colors[1];
        COLORREF col2 = colors[2];
        if (percTop < 0.5f) {
            GetGradientColor(col0, col1, 2 * percTop, &tv[0]);
        } else {
            GetGradientColor(col1, col2, 2 * (percTop - 0.5f), &tv[0]);
        }

        if (percBot < 0.5f) {
            GetGradientColor(col0, col1, 2 * percBot, &tv[3]);
        } else {
            GetGradientColor(col1, col2, 2 * (percBot - 0.5f), &tv[3]);
        }

        bool needCenter = percTop < 0.5f && percBot > 0.5f;
        if (needCenter) {
            GetGradientColor(col1, col1, 0, &tv[1]);
            GetGradientColor(col1, col1, 0, &tv[2]);
            tv[1].y = tv[2].y = (LONG)((0.5f - percTop) / (percBot - percTop) * vp.dy);
        } else {
            gr[0].LowerRight = 3;
        }
        // TODO: disable for less than about two screen heights?
        ULONG nMesh = 1;
        if (needCenter) {
            nMesh = 2;
        }
        GradientFill(hdc, tv, dimof(tv), gr, nMesh, GRADIENT_FILL_RECT_V);
    }

    bool rendering = false;
    Rect screen(Point(), dm->GetViewPort().Size());

    for (int pageNo = 1; pageNo <= dm->PageCount(); ++pageNo) {
        PageInfo* pageInfo = dm->GetPageInfo(pageNo);
        if (!pageInfo || 0.0f == pageInfo->visibleRatio) {
            continue;
        }
        CrashIf(!pageInfo->shown);
        if (!pageInfo->shown) {
            continue;
        }

        Rect bounds = pageInfo->pageOnScreen.Intersect(screen);
        // don't paint the frame background for images
        if (!dm->GetEngine()->IsImageCollection()) {
            Rect r = pageInfo->pageOnScreen;
            auto presMode = win->presentation;
            PaintPageFrameAndShadow(hdc, bounds, r, presMode);
        }

        bool renderOutOfDateCue = false;
        int renderDelay = gRenderCache.Paint(hdc, bounds, dm, pageNo, pageInfo, &renderOutOfDateCue);

        if (renderDelay != 0) {
            AutoDeleteFont fontRightTxt(CreateSimpleFont(hdc, L"MS Shell Dlg", 14));
            HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt);
            auto col = GetAppColor(AppColor::MainWindowText);
            SetTextColor(hdc, col);
            if (renderDelay != RENDER_DELAY_FAILED) {
                if (renderDelay < REPAINT_MESSAGE_DELAY_IN_MS) {
                    RepaintAsync(win, REPAINT_MESSAGE_DELAY_IN_MS / 4);
                } else {
                    DrawCenteredText(hdc, bounds, _TR("Please wait - rendering..."), IsUIRightToLeft());
                }
                rendering = true;
            } else {
                DrawCenteredText(hdc, bounds, _TR("Couldn't render the page"), IsUIRightToLeft());
            }
            SelectObject(hdc, hPrevFont);
            continue;
        }

        if (!renderOutOfDateCue) {
            continue;
        }

        HDC bmpDC = CreateCompatibleDC(hdc);
        if (!bmpDC) {
            continue;
        }
        SelectObject(bmpDC, gBitmapReloadingCue);
        int size = DpiScale(win->hwndFrame, 16);
        int cx = std::min(bounds.dx, 2 * size);
        int cy = std::min(bounds.dy, 2 * size);
        int x = bounds.x + bounds.dx - std::min((cx + size) / 2, cx);
        int y = bounds.y + std::max((cy - size) / 2, 0);
        int dxDest = std::min(cx, size);
        int dyDest = std::min(cy, size);
        StretchBlt(hdc, x, y, dxDest, dyDest, bmpDC, 0, 0, 16, 16, SRCCOPY);
        DeleteDC(bmpDC);
    }

    if (win->showSelection) {
        PaintSelection(win, hdc);
    }

    if (win->fwdSearchMark.show) {
        PaintForwardSearchMark(win, hdc);
    }

    if (!rendering) {
        DebugShowLinks(dm, hdc);
    }
}

static void OnPaintDocument(WindowInfo* win) {
    auto t = TimeGet();
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);

    switch (win->presentation) {
        case PM_BLACK_SCREEN:
            FillRect(hdc, &ps.rcPaint, GetStockBrush(BLACK_BRUSH));
            break;
        case PM_WHITE_SCREEN:
            FillRect(hdc, &ps.rcPaint, GetStockBrush(WHITE_BRUSH));
            break;
        default:
            DrawDocument(win, win->buffer->GetDC(), &ps.rcPaint);
            win->buffer->Flush(hdc);
    }

    EndPaint(win->hwndCanvas, &ps);
    if (gShowFrameRate) {
        win->frameRateWnd->ShowFrameRateDur(TimeSinceInMs(t));
    }
}

static void SetTextOrArrorCursor(DisplayModel* dm, Point pt) {
    if (dm->IsOverText(pt)) {
        SetCursorCached(IDC_IBEAM);
    } else {
        SetCursorCached(IDC_ARROW);
    }
}

static LRESULT OnSetCursorMouseIdle(WindowInfo* win, HWND hwnd) {
    Point pt;
    DisplayModel* dm = win->AsFixed();
    if (!dm || !GetCursor() || !GetCursorPosInHwnd(hwnd, pt)) {
        win->HideToolTip();
        return FALSE;
    }
    if (win->notifications->GetForGroup(NG_CURSOR_POS_HELPER)) {
        SetCursorCached(IDC_CROSS);
        return TRUE;
    }

    IPageElement* pageEl = dm->GetElementAtPos(pt);
    if (!pageEl) {
        SetTextOrArrorCursor(dm, pt);
        win->HideToolTip();
        return TRUE;
    }
    WCHAR* text = pageEl->GetValue();
    int pageNo = pageEl->GetPageNo();
    auto r = pageEl->GetRect();
    Rect rc = dm->CvtToScreen(pageNo, r);
    win->ShowToolTip(text, rc, true);

    bool isLink = pageEl->Is(kindPageElementDest);
    delete pageEl;

    if (isLink) {
        SetCursorCached(IDC_HAND);
    } else {
        SetTextOrArrorCursor(dm, pt);
    }
    return TRUE;
}

static LRESULT OnSetCursor(WindowInfo* win, HWND hwnd) {
    CrashIf(win->hwndCanvas != hwnd);
    if (win->mouseAction != MouseAction::Idle) {
        win->HideToolTip();
    }

    switch (win->mouseAction) {
        case MouseAction::Dragging:
            SetCursor(gCursorDrag);
            return TRUE;
        case MouseAction::Scrolling:
            SetCursorCached(IDC_SIZEALL);
            return TRUE;
        case MouseAction::SelectingText:
            SetCursorCached(IDC_IBEAM);
            return TRUE;
        case MouseAction::Selecting:
            break;
        case MouseAction::Idle:
            return OnSetCursorMouseIdle(win, hwnd);
    }
    return win->presentation ? TRUE : FALSE;
}

static LRESULT CanvasOnMouseWheel(WindowInfo* win, UINT msg, WPARAM wp, LPARAM lp) {
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win->tocVisible && IsCursorOverWindow(win->tocTreeCtrl->hwnd) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessageW(win->tocTreeCtrl->hwnd, msg, wp, lp);
        gWheelMsgRedirect = false;
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wp);

    // Note: not all mouse drivers correctly report the Ctrl key's state
    if ((LOWORD(wp) & MK_CONTROL) || IsCtrlPressed() || (LOWORD(wp) & MK_RBUTTON)) {
        Point pt;
        GetCursorPosInHwnd(win->hwndCanvas, pt);

        float zoom = win->ctrl->GetNextZoomStep(delta < 0 ? ZOOM_MIN : ZOOM_MAX);
        win->ctrl->SetZoomVirtual(zoom, &pt);
        UpdateToolbarState(win);

        // don't show the context menu when zooming with the right mouse-button down
        if ((LOWORD(wp) & MK_RBUTTON)) {
            win->dragStartPending = false;
        }

        return 0;
    }

    // make sure to scroll whole pages in non-continuous Fit Content mode
    if (!IsContinuous(win->ctrl->GetDisplayMode()) && ZOOM_FIT_CONTENT == win->ctrl->GetZoomVirtual()) {
        if (delta > 0) {
            win->ctrl->GoToPrevPage();
        } else {
            win->ctrl->GoToNextPage();
        }
        return 0;
    }

    if (gDeltaPerLine == 0) {
        return 0;
    }

    bool horizontal = (LOWORD(wp) & MK_SHIFT) || IsShiftPressed();
    if (horizontal) {
        gSuppressAltKey = true;
    }

    if (gDeltaPerLine < 0 && win->AsFixed()) {
        // scroll by (fraction of a) page
        SCROLLINFO si = {0};
        si.cbSize = sizeof(si);
        si.fMask = SIF_PAGE;
        GetScrollInfo(win->hwndCanvas, horizontal ? SB_HORZ : SB_VERT, &si);
        int scrollBy = -MulDiv(si.nPage, delta, WHEEL_DELTA);
        if (horizontal) {
            win->AsFixed()->ScrollXBy(scrollBy);
        } else {
            win->AsFixed()->ScrollYBy(scrollBy, true);
        }
        return 0;
    }

    // alt while scrolling will scroll by half a page per tick
    // usefull for browsing long files
    if ((LOWORD(wp) & MK_ALT) || IsAltPressed()) {
        SendMessageW(win->hwndCanvas, WM_VSCROLL, (delta > 0) ? SB_HPAGEUP : SB_HPAGEDOWN, 0);
        return 0;
    }

    win->wheelAccumDelta += delta;
    int currentScrollPos = GetScrollPos(win->hwndCanvas, SB_VERT);

    while (win->wheelAccumDelta >= gDeltaPerLine) {
        if (horizontal) {
            SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_LINELEFT, 0);
        } else {
            SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        }
        win->wheelAccumDelta -= gDeltaPerLine;
    }
    while (win->wheelAccumDelta <= -gDeltaPerLine) {
        if (horizontal) {
            SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_LINERIGHT, 0);
        } else {
            SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        }
        win->wheelAccumDelta += gDeltaPerLine;
    }

    if (!horizontal && !IsContinuous(win->ctrl->GetDisplayMode()) &&
        GetScrollPos(win->hwndCanvas, SB_VERT) == currentScrollPos) {
        if (delta > 0) {
            win->ctrl->GoToPrevPage(true);
        } else {
            win->ctrl->GoToNextPage();
        }
    }

    return 0;
}

static LRESULT CanvasOnMouseHWheel(WindowInfo* win, UINT msg, WPARAM wp, LPARAM lp) {
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win->tocVisible && IsCursorOverWindow(win->tocTreeCtrl->hwnd) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEHWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessageW(win->tocTreeCtrl->hwnd, msg, wp, lp);
        gWheelMsgRedirect = false;
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wp);
    win->wheelAccumDelta += delta;

    while (win->wheelAccumDelta >= gDeltaPerLine) {
        SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_LINERIGHT, 0);
        win->wheelAccumDelta -= gDeltaPerLine;
    }
    while (win->wheelAccumDelta <= -gDeltaPerLine) {
        SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_LINELEFT, 0);
        win->wheelAccumDelta += gDeltaPerLine;
    }

    return TRUE;
}

static u32 LowerU64(ULONGLONG v) {
    u32 res = (u32)v;
    return res;
}

static LRESULT OnGesture(WindowInfo* win, UINT msg, WPARAM wp, LPARAM lp) {
    if (!touch::SupportsGestures()) {
        return DefWindowProc(win->hwndFrame, msg, wp, lp);
    }

    HGESTUREINFO hgi = (HGESTUREINFO)lp;
    GESTUREINFO gi = {0};
    gi.cbSize = sizeof(GESTUREINFO);

    BOOL ok = touch::GetGestureInfo(hgi, &gi);
    if (!ok) {
        touch::CloseGestureInfoHandle(hgi);
        return 0;
    }

    switch (gi.dwID) {
        case GID_ZOOM:
            if (gi.dwFlags != GF_BEGIN && win->AsFixed()) {
                float zoom = (float)LowerU64(gi.ullArguments) / (float)win->touchState.startArg;
                ZoomToSelection(win, zoom, false, true);
            }
            win->touchState.startArg = LowerU64(gi.ullArguments);
            break;

        case GID_PAN:
            // Flicking left or right changes the page,
            // panning moves the document in the scroll window
            if (gi.dwFlags == GF_BEGIN) {
                win->touchState.panStarted = true;
                win->touchState.panPos = gi.ptsLocation;
                win->touchState.panScrollOrigX = GetScrollPos(win->hwndCanvas, SB_HORZ);
            } else if (win->touchState.panStarted) {
                int deltaX = win->touchState.panPos.x - gi.ptsLocation.x;
                int deltaY = win->touchState.panPos.y - gi.ptsLocation.y;
                win->touchState.panPos = gi.ptsLocation;

                if ((!win->AsFixed() || !IsContinuous(win->AsFixed()->GetDisplayMode())) && (gi.dwFlags & GF_INERTIA) &&
                    abs(deltaX) > abs(deltaY)) {
                    // Switch pages once we hit inertia in a horizontal direction (only in
                    // non-continuous modes, cf. https://github.com/sumatrapdfreader/sumatrapdf/issues/9 )
                    if (deltaX < 0) {
                        win->ctrl->GoToPrevPage();
                    } else if (deltaX > 0) {
                        win->ctrl->GoToNextPage();
                    }
                    // When we switch pages, go back to the initial scroll position
                    // and prevent further pan movement caused by the inertia
                    if (win->AsFixed()) {
                        win->AsFixed()->ScrollXTo(win->touchState.panScrollOrigX);
                    }
                    win->touchState.panStarted = false;
                } else if (win->AsFixed()) {
                    // Pan/Scroll
                    win->MoveDocBy(deltaX, deltaY);
                }
            }
            break;

        case GID_ROTATE:
            // Rotate the PDF 90 degrees in one direction
            if (gi.dwFlags == GF_END && win->AsFixed()) {
                // This is in radians
                double rads = GID_ROTATE_ANGLE_FROM_ARGUMENT(LowerU64(gi.ullArguments));
                // The angle from the rotate is the opposite of the Sumatra rotate, thus the negative
                double degrees = -rads * 180 / M_PI;

                // Playing with the app, I found that I often didn't go quit a full 90 or 180
                // degrees. Allowing rotate without a full finger rotate seemed more natural.
                if (degrees < -120 || degrees > 120) {
                    win->AsFixed()->RotateBy(180);
                } else if (degrees < -45) {
                    win->AsFixed()->RotateBy(-90);
                } else if (degrees > 45) {
                    win->AsFixed()->RotateBy(90);
                }
            }
            break;

        case GID_TWOFINGERTAP:
            // Two-finger tap toggles fullscreen mode
            OnMenuViewFullscreen(win);
            break;

        case GID_PRESSANDTAP:
            // Toggle between Fit Page, Fit Width and Fit Content (same as 'z')
            if (gi.dwFlags == GF_BEGIN) {
                win->ToggleZoom();
            }
            break;

        default:
            // A gesture was not recognized
            break;
    }

    touch::CloseGestureInfoHandle(hgi);
    return 0;
}

static LRESULT WndProcCanvasFixedPageUI(WindowInfo* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // DbgLogMsg("canvas:", hwnd, msg, wp, lp);

    int x = GET_X_LPARAM(lp);
    int y = GET_Y_LPARAM(lp);
    switch (msg) {
        case WM_PAINT:
            OnPaintDocument(win);
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove(win, x, y, wp);
            return 0;

        case WM_LBUTTONDOWN:
            OnMouseLeftButtonDown(win, x, y, wp);
            return 0;

        case WM_LBUTTONUP:
            OnMouseLeftButtonUp(win, x, y, wp);
            return 0;

        case WM_LBUTTONDBLCLK:
            OnMouseLeftButtonDblClk(win, x, y, wp);
            return 0;

        case WM_MBUTTONDOWN:
            SetTimer(hwnd, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, nullptr);
            // TODO: Create window that shows location of initial click for reference
            OnMouseMiddleButtonDown(win, x, y, wp);
            return 0;

        case WM_RBUTTONDOWN:
            OnMouseRightButtonDown(win, x, y);
            return 0;

        case WM_RBUTTONUP:
            OnMouseRightButtonUp(win, x, y, wp);
            return 0;

        case WM_RBUTTONDBLCLK:
            OnMouseRightButtonDblClick(win, x, y, wp);
            return 0;

        case WM_VSCROLL:
            OnVScroll(win, wp);
            return 0;

        case WM_HSCROLL:
            OnHScroll(win, wp);
            return 0;

        case WM_MOUSEWHEEL:
            return CanvasOnMouseWheel(win, msg, wp, lp);

        case WM_MOUSEHWHEEL:
            return CanvasOnMouseHWheel(win, msg, wp, lp);

        case WM_SETCURSOR:
            if (OnSetCursor(win, hwnd)) {
                return TRUE;
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_CONTEXTMENU:
            OnWindowContextMenu(win, 0, 0);
            return 0;

        case WM_GESTURE:
            return OnGesture(win, msg, wp, lp);

        case WM_NCPAINT: {
            // check whether scrolling is required in the horizontal and/or vertical axes
            int requiredScrollAxes = -1;
            if (win->AsFixed()->NeedHScroll() && win->AsFixed()->NeedVScroll()) {
                requiredScrollAxes = SB_BOTH;
            } else if (win->AsFixed()->NeedHScroll()) {
                requiredScrollAxes = SB_HORZ;
            } else if (win->AsFixed()->NeedVScroll()) {
                requiredScrollAxes = SB_VERT;
            }

            if (requiredScrollAxes != -1) {
                ShowScrollBar(win->hwndCanvas, requiredScrollAxes, !gGlobalPrefs->fixedPageUI.hideScrollbars);
            }

            // allow default processing to continue
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

///// methods needed for ChmUI canvases (should be subclassed by HtmlHwnd) /////

static LRESULT WndProcCanvasChmUI(WindowInfo* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SETCURSOR:
            // TODO: make (re)loading a document always clear the infotip
            win->HideToolTip();
            return DefWindowProc(hwnd, msg, wp, lp);

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

///// methods needed for EbookUI canvases /////

// NO_INLINE to help in debugging https://github.com/sumatrapdfreader/sumatrapdf/issues/292
static NO_INLINE LRESULT CanvasOnMouseWheelEbook(WindowInfo* win, UINT msg, WPARAM wp, LPARAM lp) {
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win->tocVisible && IsCursorOverWindow(win->tocTreeCtrl->hwnd)) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        LRESULT res = 0;
        if (!gWheelMsgRedirect) {
            gWheelMsgRedirect = true;
            res = SendMessageW(win->tocTreeCtrl->hwnd, msg, wp, lp);
            gWheelMsgRedirect = false;
        }
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wp);
    if (delta > 0) {
        win->ctrl->GoToPrevPage();
    } else {
        win->ctrl->GoToNextPage();
    }
    return 0;
}

static LRESULT WndProcCanvasEbookUI(WindowInfo* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool wasHandled;
    LRESULT res = win->AsEbook()->HandleMessage(msg, wp, lp, wasHandled);
    if (wasHandled) {
        return res;
    }

    switch (msg) {
        case WM_SETCURSOR:
            // TODO: make (re)loading a document always clear the infotip
            win->HideToolTip();
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            return CanvasOnMouseWheelEbook(win, msg, wp, lp);

        case WM_GESTURE:
            return OnGesture(win, msg, wp, lp);

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

///// methods needed for FixedPageUI canvases with loading error /////

static void OnPaintError(WindowInfo* win) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);

    AutoDeleteFont fontRightTxt(CreateSimpleFont(hdc, L"MS Shell Dlg", 14));
    HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt);
    auto bgCol = GetAppColor(AppColor::NoDocBg);
    ScopedGdiObj<HBRUSH> bgBrush(CreateSolidBrush(bgCol));
    FillRect(hdc, &ps.rcPaint, bgBrush);
    // TODO: should this be "Error opening %s"?
    AutoFreeWstr msg(str::Format(_TR("Error loading %s"), win->currentTab->filePath.Get()));
    DrawCenteredText(hdc, ClientRect(win->hwndCanvas), msg, IsUIRightToLeft());
    SelectObject(hdc, hPrevFont);

    EndPaint(win->hwndCanvas, &ps);
}

static LRESULT WndProcCanvasLoadError(WindowInfo* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            OnPaintError(win);
            return 0;

        case WM_SETCURSOR:
            // TODO: make (re)loading a document always clear the infotip
            win->HideToolTip();
            return DefWindowProc(hwnd, msg, wp, lp);

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

///// methods needed for all types of canvas /////

void RepaintAsync(WindowInfo* win, int delayInMs) {
    // even though RepaintAsync is mostly called from the UI thread,
    // we depend on the repaint message to happen asynchronously
    uitask::Post([win, delayInMs] {
        if (!WindowInfoStillValid(win)) {
            return;
        }
        if (!delayInMs) {
            WndProcCanvas(win->hwndCanvas, WM_TIMER, REPAINT_TIMER_ID, 0);
        } else if (!win->delayedRepaintTimer) {
            win->delayedRepaintTimer = SetTimer(win->hwndCanvas, REPAINT_TIMER_ID, (uint)delayInMs, nullptr);
        }
    });
}

static void OnTimer(WindowInfo* win, HWND hwnd, WPARAM timerId) {
    Point pt;

    switch (timerId) {
        case REPAINT_TIMER_ID:
            win->delayedRepaintTimer = 0;
            KillTimer(hwnd, REPAINT_TIMER_ID);
            win->RedrawAllIncludingNonClient(true);
            break;

        case SMOOTHSCROLL_TIMER_ID:
            if (MouseAction::Scrolling == win->mouseAction) {
                win->MoveDocBy(win->xScrollSpeed, win->yScrollSpeed);
            } else if (MouseAction::Selecting == win->mouseAction || MouseAction::SelectingText == win->mouseAction) {
                GetCursorPosInHwnd(win->hwndCanvas, pt);
                if (NeedsSelectionEdgeAutoscroll(win, pt.x, pt.y)) {
                    OnMouseMove(win, pt.x, pt.y, MK_CONTROL);
                }
            } else {
                KillTimer(hwnd, SMOOTHSCROLL_TIMER_ID);
                win->yScrollSpeed = 0;
                win->xScrollSpeed = 0;
            }
            break;

        case HIDE_CURSOR_TIMER_ID:
            KillTimer(hwnd, HIDE_CURSOR_TIMER_ID);
            if (win->presentation) {
                SetCursor((HCURSOR) nullptr);
            }
            break;

        case HIDE_FWDSRCHMARK_TIMER_ID:
            win->fwdSearchMark.hideStep++;
            if (1 == win->fwdSearchMark.hideStep) {
                SetTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS, nullptr);
            } else if (win->fwdSearchMark.hideStep >= HIDE_FWDSRCHMARK_STEPS) {
                KillTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID);
                win->fwdSearchMark.show = false;
                RepaintAsync(win, 0);
            } else {
                RepaintAsync(win, 0);
            }
            break;

        case AUTO_RELOAD_TIMER_ID:
            KillTimer(hwnd, AUTO_RELOAD_TIMER_ID);
            if (win->currentTab && win->currentTab->reloadOnFocus) {
                ReloadDocument(win, true);
            }
            break;

        case EBOOK_LAYOUT_TIMER_ID:
            KillTimer(hwnd, EBOOK_LAYOUT_TIMER_ID);
            for (TabInfo* tab : win->tabs) {
                if (tab->AsEbook()) {
                    tab->AsEbook()->TriggerLayout();
                }
            }
            break;
    }
}

static void OnDropFiles(HDROP hDrop, bool dragFinish) {
    WCHAR filePath[MAX_PATH] = {0};
    int nFiles = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, 0, 0);

    bool isShift = IsShiftPressed();
    WindowInfo* win = nullptr;
    for (int i = 0; i < nFiles; i++) {
        DragQueryFile(hDrop, i, filePath, dimof(filePath));
        if (str::EndsWithI(filePath, L".lnk")) {
            AutoFreeWstr resolved(ResolveLnk(filePath));
            if (resolved) {
                str::BufSet(filePath, dimof(filePath), resolved);
            }
        }
        // The first dropped document may override the current window
        LoadArgs args(filePath, nullptr);
        if (isShift && !win) {
            win = CreateAndShowWindowInfo(nullptr);
            args.win = win;
        }
        LoadDocument(args);
    }
    if (dragFinish) {
        DragFinish(hDrop);
    }
}

LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // messages that don't require win

    switch (msg) {
        case WM_DROPFILES:
            CrashIf(lp != 0 && lp != 1);
            OnDropFiles((HDROP)wp, !lp);
            return 0;

        // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-erasebkgnd
        case WM_ERASEBKGND:
            // return non-zero to indicate we erased
            // helps to avoid flicker
            return 1;
    }

    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // messages that require win
    switch (msg) {
        case WM_TIMER:
            OnTimer(win, hwnd, wp);
            return 0;

        case WM_SIZE:
            if (!IsIconic(win->hwndFrame)) {
                win->UpdateCanvasSize();
            }
            return 0;

        case WM_GETOBJECT:
            // TODO: should we check for UiaRootObjectId, as in
            // http://msdn.microsoft.com/en-us/library/windows/desktop/ff625912.aspx ???
            // On the other hand
            // http://code.msdn.microsoft.com/windowsdesktop/UI-Automation-Clean-94993ac6/sourcecode?fileId=42883&pathId=2071281652
            // says that UiaReturnRawElementProvider() should be called regardless of lParam
            // Don't expose UIA automation in plugin mode yet. UIA is still too experimental
            if (gPluginMode) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            // disable UIAutomation in release builds until concurrency issues and
            // memory leaks have been figured out and fixed
            if (!gIsDebugBuild) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            if (!win->CreateUIAProvider()) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            // TODO: should win->uiaProvider->Release() as in
            // http://msdn.microsoft.com/en-us/library/windows/desktop/gg712214.aspx
            // and http://www.code-magazine.com/articleprint.aspx?quickid=0810112&printmode=true ?
            // Maybe instead of having a single provider per win, we should always create a new one
            // like in this sample:
            // http://code.msdn.microsoft.com/windowsdesktop/UI-Automation-Clean-94993ac6/sourcecode?fileId=42883&pathId=2071281652
            // currently win->uiaProvider refCount is really out of wack in WindowInfo::~WindowInfo
            // from logging it seems that UiaReturnRawElementProvider() increases refCount by 1
            // and since WM_GETOBJECT is called many times, it accumulates
            return UiaReturnRawElementProvider(hwnd, wp, lp, win->uiaProvider);

        default:
            // TODO: achieve this split through subclassing or different window classes
            if (win->AsFixed()) {
                return WndProcCanvasFixedPageUI(win, hwnd, msg, wp, lp);
            }

            if (win->AsChm()) {
                return WndProcCanvasChmUI(win, hwnd, msg, wp, lp);
            }

            if (win->AsEbook()) {
                return WndProcCanvasEbookUI(win, hwnd, msg, wp, lp);
            }

            if (win->IsAboutWindow()) {
                return WndProcCanvasAbout(win, hwnd, msg, wp, lp);
            }

            return WndProcCanvasLoadError(win, hwnd, msg, wp, lp);
    }
}

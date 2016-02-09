/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "WinDynCalls.h"
#include "Dpi.h"
#include "FileUtil.h"
#include "FrameRateWnd.h"
#include "Timer.h"
#include "UITask.h"
#include "WinUtil.h"
// rendering engines
#include "BaseEngine.h"
#include "EngineManager.h"
#include "Doc.h"
// layout controllers
#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "EbookController.h"
#include "GlobalPrefs.h"
#include "RenderCache.h"
#include "TextSelection.h"
#include "TextSearch.h"
// ui
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Canvas.h"
#include "Caption.h"
#include "Menu.h"
#include "Notifications.h"
#include "uia/Provider.h"
#include "Search.h"
#include "Selection.h"
#include "SumatraAbout.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "Translations.h"

// these can be global, as the mouse wheel can't affect more than one window at once
static int gDeltaPerLine = 0;
// set when WM_MOUSEWHEEL has been passed on (to prevent recursion)
static bool gWheelMsgRedirect = false;

void UpdateDeltaPerLine()
{
    ULONG ulScrollLines;
    BOOL ok = SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);
    if (!ok)
        return;
    // ulScrollLines usually equals 3 or 0 (for no scrolling) or -1 (for page scrolling)
    // WHEEL_DELTA equals 120, so iDeltaPerLine will be 40
    if (ulScrollLines == (ULONG)-1)
        gDeltaPerLine = -1;
    else if (ulScrollLines != 0)
        gDeltaPerLine = WHEEL_DELTA / ulScrollLines;
    else
        gDeltaPerLine = 0;
}

///// methods needed for FixedPageUI canvases with document loaded /////

static void OnVScroll(WindowInfo& win, WPARAM wParam)
{
    AssertCrash(win.AsFixed());

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(win.hwndCanvas, SB_VERT, &si);

    int iVertPos = si.nPos;
    int lineHeight = 16;
    if (!IsContinuous(win.ctrl->GetDisplayMode()) && ZOOM_FIT_PAGE == win.ctrl->GetZoomVirtual())
        lineHeight = 1;

    USHORT message = LOWORD(wParam);
    switch (message) {
    case SB_TOP:        si.nPos = si.nMin; break;
    case SB_BOTTOM:     si.nPos = si.nMax; break;
    case SB_LINEUP:     si.nPos -= lineHeight; break;
    case SB_LINEDOWN:   si.nPos += lineHeight; break;
    case SB_HPAGEUP:    si.nPos -= si.nPage / 2; break;
    case SB_HPAGEDOWN:  si.nPos += si.nPage / 2; break;
    case SB_PAGEUP:     si.nPos -= si.nPage; break;
    case SB_PAGEDOWN:   si.nPos += si.nPage; break;
    case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win.hwndCanvas, SB_VERT, &si, TRUE);
    GetScrollInfo(win.hwndCanvas, SB_VERT, &si);

    // If the position has changed or we're dealing with a touchpad scroll event, 
    // scroll the window and update it
    if (si.nPos != iVertPos || message == SB_THUMBTRACK)
        win.AsFixed()->ScrollYTo(si.nPos);
}

static void OnHScroll(WindowInfo& win, WPARAM wParam)
{
    AssertCrash(win.AsFixed());

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(win.hwndCanvas, SB_HORZ, &si);

    int iHorzPos = si.nPos;
    USHORT message = LOWORD(wParam);
    switch (message) {
    case SB_LEFT:       si.nPos = si.nMin; break;
    case SB_RIGHT:      si.nPos = si.nMax; break;
    case SB_LINELEFT:   si.nPos -= 16; break;
    case SB_LINERIGHT:  si.nPos += 16; break;
    case SB_PAGELEFT:   si.nPos -= si.nPage; break;
    case SB_PAGERIGHT:  si.nPos += si.nPage; break;
    case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win.hwndCanvas, SB_HORZ, &si, TRUE);
    GetScrollInfo(win.hwndCanvas, SB_HORZ, &si);

    // If the position has changed or we're dealing with a touchpad scroll event, 
    // scroll the window and update it
    if (si.nPos != iHorzPos || message == SB_THUMBTRACK)
        win.AsFixed()->ScrollXTo(si.nPos);
}

static void OnDraggingStart(WindowInfo& win, int x, int y, bool right=false)
{
    SetCapture(win.hwndCanvas);
    win.mouseAction = right ? MA_DRAGGING_RIGHT : MA_DRAGGING;
    win.dragPrevPos = PointI(x, y);
    if (GetCursor())
        SetCursor(gCursorDrag);
}

static void OnDraggingStop(WindowInfo& win, int x, int y, bool aborted)
{
    if (GetCapture() != win.hwndCanvas)
        return;

    if (GetCursor())
        SetCursor(IDC_ARROW);
    ReleaseCapture();

    if (aborted)
        return;

    SizeI drag(x - win.dragPrevPos.x, y - win.dragPrevPos.y);
    win.MoveDocBy(drag.dx, -2 * drag.dy);
}

static void OnMouseMove(WindowInfo& win, int x, int y, WPARAM flags)
{
    UNUSED(flags);
    AssertCrash(win.AsFixed());

    if (win.presentation) {
        if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation) {
            SetCursor((HCURSOR)nullptr);
            return;
        }
        // shortly display the cursor if the mouse has moved and the cursor is hidden
        if (PointI(x, y) != win.dragPrevPos && !GetCursor()) {
            if (win.mouseAction == MA_IDLE)
                SetCursor(IDC_ARROW);
            else
                SendMessage(win.hwndCanvas, WM_SETCURSOR, 0, 0);
            SetTimer(win.hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, nullptr);
        }
    }

    if (win.dragStartPending) {
        // have we already started a proper drag?
        if (abs(x - win.dragStart.x) <= GetSystemMetrics(SM_CXDRAG) &&
            abs(y - win.dragStart.y) <= GetSystemMetrics(SM_CYDRAG)) {
            return;
        }
        win.dragStartPending = false;
        delete win.linkOnLastButtonDown;
        win.linkOnLastButtonDown = nullptr;
    }

    switch (win.mouseAction) {
    case MA_SCROLLING:
        win.yScrollSpeed = (y - win.dragStart.y) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        win.xScrollSpeed = (x - win.dragStart.x) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        break;
    case MA_SELECTING_TEXT:
        if (GetCursor())
            SetCursor(IDC_IBEAM);
        /* fall through */
    case MA_SELECTING:
        win.selectionRect.dx = x - win.selectionRect.x;
        win.selectionRect.dy = y - win.selectionRect.y;
        OnSelectionEdgeAutoscroll(&win, x, y);
        win.RepaintAsync();
        break;
    case MA_DRAGGING:
    case MA_DRAGGING_RIGHT:
        win.MoveDocBy(win.dragPrevPos.x - x, win.dragPrevPos.y - y);
        break;
    }
    // needed also for detecting cursor movement in presentation mode
    win.dragPrevPos = PointI(x, y);

    NotificationWnd *wnd = win.notifications->GetForGroup(NG_CURSOR_POS_HELPER);
    if (wnd) {
        if (MA_SELECTING == win.mouseAction)
            win.selectionMeasure = win.AsFixed()->CvtFromScreen(win.selectionRect).Size();
        UpdateCursorPositionHelper(&win, PointI(x, y), wnd);
    }
}

static void OnMouseLeftButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    //lf("Left button clicked on %d %d", x, y);
    if (MA_DRAGGING_RIGHT == win.mouseAction)
        return;

    if (MA_SCROLLING == win.mouseAction) {
        win.mouseAction = MA_IDLE;
        return;
    }

    CrashIfDebugOnly(win.mouseAction != MA_IDLE); // happened e.g. in crash 50539
    CrashIf(!win.AsFixed());

    SetFocus(win.hwndFrame);

    AssertCrash(!win.linkOnLastButtonDown);
    DisplayModel *dm = win.AsFixed();
    PageElement *pageEl = dm->GetElementAtPos(PointI(x, y));
    if (pageEl && pageEl->GetType() == Element_Link)
        win.linkOnLastButtonDown = pageEl;
    else
        delete pageEl;
    win.dragStartPending = true;
    win.dragStart = PointI(x, y);

    // - without modifiers, clicking on text starts a text selection
    //   and clicking somewhere else starts a drag
    // - pressing Shift forces dragging
    // - pressing Ctrl forces a rectangular selection
    // - pressing Ctrl+Shift forces text selection
    // - not having CopySelection permission forces dragging
    bool isShift = IsShiftPressed();
    bool isCtrl = IsCtrlPressed();
    bool canCopy = HasPermission(Perm_CopySelection);
    bool isOverText = dm->IsOverText(PointI(x,y));
    if (!canCopy || (isShift || !isOverText) && !isCtrl) {
        OnDraggingStart(win, x, y);
    } else {
        OnSelectionStart(&win, x, y, key);
    }
}

static void OnMouseLeftButtonUp(WindowInfo& win, int x, int y, WPARAM key)
{
    AssertCrash(win.AsFixed());
    if (MA_IDLE == win.mouseAction || MA_DRAGGING_RIGHT == win.mouseAction)
        return;
    AssertCrash(MA_SELECTING == win.mouseAction || MA_SELECTING_TEXT == win.mouseAction || MA_DRAGGING == win.mouseAction);

    bool didDragMouse = !win.dragStartPending ||
        abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
    if (MA_DRAGGING == win.mouseAction)
        OnDraggingStop(win, x, y, !didDragMouse);
    else {
        OnSelectionStop(&win, x, y, !didDragMouse);
        if (MA_SELECTING == win.mouseAction && win.showSelection)
            win.selectionMeasure = win.AsFixed()->CvtFromScreen(win.selectionRect).Size();
    }

    DisplayModel *dm = win.AsFixed();
    PointD ptPage = dm->CvtFromScreen(PointI(x, y));
    // TODO: win.linkHandler->GotoLink might spin the event loop
    PageElement *link = win.linkOnLastButtonDown;
    win.linkOnLastButtonDown = nullptr;
    win.mouseAction = MA_IDLE;

    if (didDragMouse)
        /* pass */;
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation)
        win.ChangePresentationMode(PM_ENABLED);
    /* follow an active link */
    else if (link && link->GetRect().Contains(ptPage)) {
        PageDestination *dest = link->AsLink();
        // highlight the clicked link (as a reminder of the last action once the user returns)
        if (dest && (Dest_LaunchURL == dest->GetDestType() || Dest_LaunchFile == dest->GetDestType())) {
            DeleteOldSelectionInfo(&win, true);
            win.currentTab->selectionOnPage = SelectionOnPage::FromRectangle(dm, dm->CvtToScreen(link->GetPageNo(), link->GetRect()));
            win.showSelection = win.currentTab->selectionOnPage != nullptr;
            win.RepaintAsync();
        }
        SetCursor(IDC_ARROW);
        win.linkHandler->GotoLink(dest);
    }
    /* if we had a selection and this was just a click, hide the selection */
    else if (win.showSelection)
        ClearSearchResult(&win);
    /* if there's a permanent forward search mark, hide it */
    else if (win.fwdSearchMark.show && gGlobalPrefs->forwardSearch.highlightPermanent) {
        win.fwdSearchMark.show = false;
        win.RepaintAsync();
    }
    /* in presentation mode, change pages on left/right-clicks */
    else if (PM_ENABLED == win.presentation) {
        if ((key & MK_SHIFT))
            win.ctrl->GoToPrevPage();
        else
            win.ctrl->GoToNextPage();
    }

    delete link;
}

static void OnMouseLeftButtonDblClk(WindowInfo& win, int x, int y, WPARAM key)
{
    //lf("Left button clicked on %d %d", x, y);
    if (win.presentation && !(key & ~MK_LBUTTON)) {
        // in presentation mode, left clicks turn the page,
        // make two quick left clicks (AKA one double-click) turn two pages
        OnMouseLeftButtonDown(win, x, y, key);
        return;
    }

    bool dontSelect = false;
    if (gGlobalPrefs->enableTeXEnhancements && !(key & ~MK_LBUTTON))
        dontSelect = OnInverseSearch(&win, x, y);
    if (dontSelect)
        return;

    DisplayModel *dm = win.AsFixed();
    if (dm->IsOverText(PointI(x, y))) {
        int pageNo = dm->GetPageNoByPoint(PointI(x, y));
        if (win.ctrl->ValidPageNo(pageNo)) {
            PointD pt = dm->CvtFromScreen(PointI(x, y), pageNo);
            dm->textSelection->SelectWordAt(pageNo, pt.x, pt.y);
            UpdateTextSelection(&win, false);
            win.RepaintAsync();
        }
        return;
    }

    PageElement *pageEl = dm->GetElementAtPos(PointI(x, y));
    if (pageEl && pageEl->GetType() == Element_Link) {
        // speed up navigation in a file where navigation links are in a fixed position
        OnMouseLeftButtonDown(win, x, y, key);
    }
    else if (pageEl && pageEl->GetType() == Element_Image) {
        // select an image that could be copied to the clipboard
        RectI rc = dm->CvtToScreen(pageEl->GetPageNo(), pageEl->GetRect());

        DeleteOldSelectionInfo(&win, true);
        win.currentTab->selectionOnPage = SelectionOnPage::FromRectangle(dm, rc);
        win.showSelection = win.currentTab->selectionOnPage != nullptr;
        win.RepaintAsync();
    }
    delete pageEl;
}

static void OnMouseMiddleButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    // Handle message by recording placement then moving document as mouse moves.
    UNUSED(key);

    switch (win.mouseAction) {
    case MA_IDLE:
        win.mouseAction = MA_SCROLLING;

        // record current mouse position, the farther the mouse is moved
        // from this position, the faster we scroll the document
        win.dragStart = PointI(x, y);
        SetCursor(IDC_SIZEALL);
        break;

    case MA_SCROLLING:
        win.mouseAction = MA_IDLE;
        break;
    }
}

static void OnMouseRightButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    UNUSED(key);
    //lf("Right button clicked on %d %d", x, y);
    if (MA_SCROLLING == win.mouseAction)
        win.mouseAction = MA_IDLE;
    else if (win.mouseAction != MA_IDLE)
        return;
    AssertCrash(win.AsFixed());

    SetFocus(win.hwndFrame);

    win.dragStartPending = true;
    win.dragStart = PointI(x, y);

    OnDraggingStart(win, x, y, true);
}

static void OnMouseRightButtonUp(WindowInfo& win, int x, int y, WPARAM key)
{
    AssertCrash(win.AsFixed());
    if (MA_DRAGGING_RIGHT != win.mouseAction)
        return;

    bool didDragMouse = !win.dragStartPending ||
        abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
    OnDraggingStop(win, x, y, !didDragMouse);

    win.mouseAction = MA_IDLE;

    if (didDragMouse)
        /* pass */;
    else if (PM_ENABLED == win.presentation) {
        if ((key & MK_CONTROL))
            OnContextMenu(&win, x, y);
        else if ((key & MK_SHIFT))
            win.ctrl->GoToNextPage();
        else
            win.ctrl->GoToPrevPage();
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation)
        win.ChangePresentationMode(PM_ENABLED);
    else
        OnContextMenu(&win, x, y);
}

static void OnMouseRightButtonDblClick(WindowInfo& win, int x, int y, WPARAM key)
{
    if (win.presentation && !(key & ~MK_RBUTTON)) {
        // in presentation mode, right clicks turn the page,
        // make two quick right clicks (AKA one double-click) turn two pages
        OnMouseRightButtonDown(win, x, y, key);
        return;
    }
}

#ifdef DRAW_PAGE_SHADOWS
#define BORDER_SIZE   1
#define SHADOW_OFFSET 4
static void PaintPageFrameAndShadow(HDC hdc, RectI& bounds, RectI& pageRect, bool presentation)
{
    // Frame info
    RectI frame = bounds;
    frame.Inflate(BORDER_SIZE, BORDER_SIZE);

    // Shadow info
    RectI shadow = frame;
    shadow.Offset(SHADOW_OFFSET, SHADOW_OFFSET);
    if (frame.x < 0) {
        // the left of the page isn't visible, so start the shadow at the left
        int diff = std::min(-pageRect.x, SHADOW_OFFSET);
        shadow.x -= diff; shadow.dx += diff;
    }
    if (frame.y < 0) {
        // the top of the page isn't visible, so start the shadow at the top
        int diff = std::min(-pageRect.y, SHADOW_OFFSET);
        shadow.y -= diff; shadow.dy += diff;
    }

    // Draw shadow
    if (!presentation) {
        ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(COL_PAGE_SHADOW));
        FillRect(hdc, &shadow.ToRECT(), brush);
    }

    // Draw frame
    ScopedGdiObj<HPEN> pe(CreatePen(PS_SOLID, 1, presentation ? TRANSPARENT : COL_PAGE_FRAME));
    ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(gRenderCache.backgroundColor));
    SelectObject(hdc, pe);
    SelectObject(hdc, brush);
    Rectangle(hdc, frame.x, frame.y, frame.x + frame.dx, frame.y + frame.dy);
}
#else
static void PaintPageFrameAndShadow(HDC hdc, RectI& bounds, RectI& pageRect, bool presentation)
{
    UNUSED(pageRect);  UNUSED(presentation);
    ScopedPen pen(CreatePen(PS_NULL, 0, 0));
    ScopedBrush brush(CreateSolidBrush(gRenderCache.backgroundColor));
    ScopedHdcSelect restorePen(hdc, pen);
    ScopedHdcSelect restoreBrush(hdc, brush);
    Rectangle(hdc, bounds.x, bounds.y, bounds.x + bounds.dx + 1, bounds.y + bounds.dy + 1);
}
#endif

/* debug code to visualize links (can block while rendering) */
static void DebugShowLinks(DisplayModel& dm, HDC hdc)
{
    if (!gDebugShowLinks)
        return;

    RectI viewPortRect(PointI(), dm.GetViewPort().Size());
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x00, 0xff, 0xff));
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    for (int pageNo = dm.PageCount(); pageNo >= 1; --pageNo) {
        PageInfo *pageInfo = dm.GetPageInfo(pageNo);
        if (!pageInfo || !pageInfo->shown || 0.0 == pageInfo->visibleRatio)
            continue;

        Vec<PageElement *> *els = dm.GetEngine()->GetElements(pageNo);
        if (els) {
            for (size_t i = 0; i < els->Count(); i++) {
                if (els->At(i)->GetType() == Element_Image)
                    continue;
                RectI rect = dm.CvtToScreen(pageNo, els->At(i)->GetRect());
                RectI isect = viewPortRect.Intersect(rect);
                if (!isect.IsEmpty())
                    PaintRect(hdc, isect);
            }
            DeleteVecMembers(*els);
            delete els;
        }
    }

    DeletePen(SelectObject(hdc, oldPen));

    if (dm.GetZoomVirtual() == ZOOM_FIT_CONTENT) {
        // also display the content box when fitting content
        pen = CreatePen(PS_SOLID, 1, RGB(0xff, 0x00, 0xff));
        oldPen = SelectObject(hdc, pen);

        for (int pageNo = dm.PageCount(); pageNo >= 1; --pageNo) {
            PageInfo *pageInfo = dm.GetPageInfo(pageNo);
            if (!pageInfo->shown || 0.0 == pageInfo->visibleRatio)
                continue;

            RectI rect = dm.CvtToScreen(pageNo, dm.GetEngine()->PageContentBox(pageNo));
            PaintRect(hdc, rect);
        }

        DeletePen(SelectObject(hdc, oldPen));
    }
}

// cf. http://forums.fofou.org/sumatrapdf/topic?id=3183580
static void GetGradientColor(COLORREF a, COLORREF b, float perc, TRIVERTEX *tv)
{
    tv->Red = (COLOR16)((GetRValueSafe(a) + perc * (GetRValueSafe(b) - GetRValueSafe(a))) * 256);
    tv->Green = (COLOR16)((GetGValueSafe(a) + perc * (GetGValueSafe(b) - GetGValueSafe(a))) * 256);
    tv->Blue = (COLOR16)((GetBValueSafe(a) + perc * (GetBValueSafe(b) - GetBValueSafe(a))) * 256);
}

static void DrawDocument(WindowInfo& win, HDC hdc, RECT *rcArea)
{
    AssertCrash(win.AsFixed());
    if (!win.AsFixed()) return;
    DisplayModel* dm = win.AsFixed();

    bool paintOnBlackWithoutShadow = win.presentation ||
    // draw comic books and single images on a black background (without frame and shadow)
                                     dm->GetEngine()->IsImageCollection();
    if (paintOnBlackWithoutShadow) {
        ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(WIN_COL_BLACK));
        FillRect(hdc, rcArea, brush);
    }
    else if (0 == gGlobalPrefs->fixedPageUI.gradientColors->Count()) {
        ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(GetNoDocBgColor()));
        FillRect(hdc, rcArea, brush);
    }
    else {
        COLORREF colors[3];
        colors[0] = gGlobalPrefs->fixedPageUI.gradientColors->At(0);
        if (gGlobalPrefs->fixedPageUI.gradientColors->Count() == 1) {
            colors[1] = colors[2] = colors[0];
        }
        else if (gGlobalPrefs->fixedPageUI.gradientColors->Count() == 2) {
            colors[2] = gGlobalPrefs->fixedPageUI.gradientColors->At(1);
            colors[1] = RGB((GetRValueSafe(colors[0]) + GetRValueSafe(colors[2])) / 2,
                            (GetGValueSafe(colors[0]) + GetGValueSafe(colors[2])) / 2,
                            (GetBValueSafe(colors[0]) + GetBValueSafe(colors[2])) / 2);
        }
        else {
            colors[1] = gGlobalPrefs->fixedPageUI.gradientColors->At(1);
            colors[2] = gGlobalPrefs->fixedPageUI.gradientColors->At(2);
        }
        SizeI size = dm->GetCanvasSize();
        float percTop = 1.0f * dm->GetViewPort().y / size.dy;
        float percBot = 1.0f * dm->GetViewPort().BR().y / size.dy;
        if (!IsContinuous(dm->GetDisplayMode())) {
            percTop += dm->CurrentPageNo() - 1; percTop /= dm->PageCount();
            percBot += dm->CurrentPageNo() - 1; percBot /= dm->PageCount();
        }
        SizeI vp = dm->GetViewPort().Size();
        TRIVERTEX tv[4] = { { 0, 0 }, { vp.dx, vp.dy / 2 }, { 0, vp.dy / 2 }, { vp.dx, vp.dy } };
        GRADIENT_RECT gr[2] = { { 0, 1 }, { 2, 3 } };
        if (percTop < 0.5f)
            GetGradientColor(colors[0], colors[1], 2 * percTop, &tv[0]);
        else
            GetGradientColor(colors[1], colors[2], 2 * (percTop - 0.5f), &tv[0]);
        if (percBot < 0.5f)
            GetGradientColor(colors[0], colors[1], 2 * percBot, &tv[3]);
        else
            GetGradientColor(colors[1], colors[2], 2 * (percBot - 0.5f), &tv[3]);
        bool needCenter = percTop < 0.5f && percBot > 0.5f;
        if (needCenter) {
            GetGradientColor(colors[1], colors[1], 0, &tv[1]);
            GetGradientColor(colors[1], colors[1], 0, &tv[2]);
            tv[1].y = tv[2].y = (LONG)((0.5f - percTop) / (percBot - percTop) * vp.dy);
        }
        else
            gr[0].LowerRight = 3;
        // TODO: disable for less than about two screen heights?
        GradientFill(hdc, tv, dimof(tv), gr, needCenter ? 2 : 1, GRADIENT_FILL_RECT_V);
    }

    bool rendering = false;
    RectI screen(PointI(), dm->GetViewPort().Size());

    for (int pageNo = 1; pageNo <= dm->PageCount(); ++pageNo) {
        PageInfo *pageInfo = dm->GetPageInfo(pageNo);
        if (!pageInfo || 0.0f == pageInfo->visibleRatio)
            continue;
        AssertCrash(pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        RectI bounds = pageInfo->pageOnScreen.Intersect(screen);
        // don't paint the frame background for images
        if (!dm->GetEngine()->IsImageCollection())
            PaintPageFrameAndShadow(hdc, bounds, pageInfo->pageOnScreen, win.presentation);

        bool renderOutOfDateCue = false;
        UINT renderDelay = gRenderCache.Paint(hdc, bounds, dm, pageNo, pageInfo, &renderOutOfDateCue);

        if (renderDelay) {
            ScopedFont fontRightTxt(CreateSimpleFont(hdc, L"MS Shell Dlg", 14));
            HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt);
            SetTextColor(hdc, gRenderCache.textColor);
            if (renderDelay != RENDER_DELAY_FAILED) {
                if (renderDelay < REPAINT_MESSAGE_DELAY_IN_MS)
                    win.RepaintAsync(REPAINT_MESSAGE_DELAY_IN_MS / 4);
                else
                    DrawCenteredText(hdc, bounds, _TR("Please wait - rendering..."), IsUIRightToLeft());
                rendering = true;
            } else {
                DrawCenteredText(hdc, bounds, _TR("Couldn't render the page"), IsUIRightToLeft());
            }
            SelectObject(hdc, hPrevFont);
            continue;
        }

        if (!renderOutOfDateCue)
            continue;

        HDC bmpDC = CreateCompatibleDC(hdc);
        if (bmpDC) {
            SelectObject(bmpDC, gBitmapReloadingCue);
            int size = DpiScaleY(win.hwndFrame, 16);
            int cx = std::min(bounds.dx, 2 * size);
            int cy = std::min(bounds.dy, 2 * size);
            int x = bounds.x + bounds.dx - std::min((cx + size) / 2, cx);
            int y = bounds.y + std::max((cy - size) / 2, 0);
            int dxDest = std::min(cx, size);
            int dyDest = std::min(cy, size);
            StretchBlt(hdc, x, y, dxDest, dyDest, bmpDC, 0, 0, 16, 16, SRCCOPY);
            DeleteDC(bmpDC);
        }
    }

    if (win.showSelection)
        PaintSelection(&win, hdc);

    if (win.fwdSearchMark.show)
        PaintForwardSearchMark(&win, hdc);

    if (!rendering)
        DebugShowLinks(*dm, hdc);
}

static void OnPaintDocument(WindowInfo& win)
{
    Timer t;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win.hwndCanvas, &ps);

    switch (win.presentation) {
    case PM_BLACK_SCREEN:
        FillRect(hdc, &ps.rcPaint, GetStockBrush(BLACK_BRUSH));
        break;
    case PM_WHITE_SCREEN:
        FillRect(hdc, &ps.rcPaint, GetStockBrush(WHITE_BRUSH));
        break;
    default:
        DrawDocument(win, win.buffer->GetDC(), &ps.rcPaint);
        win.buffer->Flush(hdc);
    }

    EndPaint(win.hwndCanvas, &ps);
    if (gShowFrameRate) {
        ShowFrameRateDur(win.frameRateWnd, t.GetTimeInMs());
    }
}

static LRESULT OnSetCursor(WindowInfo& win, HWND hwnd)
{
    PointI pt;

    if (win.mouseAction != MA_IDLE)
        win.DeleteInfotip();

    switch (win.mouseAction) {
    case MA_DRAGGING:
    case MA_DRAGGING_RIGHT:
        SetCursor(gCursorDrag);
        return TRUE;
    case MA_SCROLLING:
        SetCursor(IDC_SIZEALL);
        return TRUE;
    case MA_SELECTING_TEXT:
        SetCursor(IDC_IBEAM);
        return TRUE;
    case MA_SELECTING:
        break;
    case MA_IDLE:
        if (GetCursor() && GetCursorPosInHwnd(hwnd, pt) && win.AsFixed()) {
            if (win.notifications->GetForGroup(NG_CURSOR_POS_HELPER)) {
                SetCursor(IDC_CROSS);
                return TRUE;
            }
            DisplayModel *dm = win.AsFixed();
            PageElement *pageEl = dm->GetElementAtPos(pt);
            if (pageEl) {
                ScopedMem<WCHAR> text(pageEl->GetValue());
                RectI rc = dm->CvtToScreen(pageEl->GetPageNo(), pageEl->GetRect());
                win.CreateInfotip(text, rc, true);

                bool isLink = pageEl->GetType() == Element_Link;
                delete pageEl;

                if (isLink) {
                    SetCursor(IDC_HAND);
                    return TRUE;
                }
            }
            else
                win.DeleteInfotip();
            if (dm->IsOverText(pt))
                SetCursor(IDC_IBEAM);
            else
                SetCursor(IDC_ARROW);
            return TRUE;
        }
        win.DeleteInfotip();
        break;
    }
    return win.presentation ? TRUE : FALSE;
}

static LRESULT CanvasOnMouseWheel(WindowInfo& win, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win.tocVisible && IsCursorOverWindow(win.hwndTocTree) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessage(win.hwndTocTree, message, wParam, lParam);
        gWheelMsgRedirect = false;
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wParam);

    // Note: not all mouse drivers correctly report the Ctrl key's state
    if ((LOWORD(wParam) & MK_CONTROL) || IsCtrlPressed() || (LOWORD(wParam) & MK_RBUTTON)) {
        PointI pt;
        GetCursorPosInHwnd(win.hwndCanvas, pt);

        float zoom = win.ctrl->GetNextZoomStep(delta < 0 ? ZOOM_MIN : ZOOM_MAX);
        win.ctrl->SetZoomVirtual(zoom, &pt);
        UpdateToolbarState(&win);

        // don't show the context menu when zooming with the right mouse-button down
        if ((LOWORD(wParam) & MK_RBUTTON))
            win.dragStartPending = false;

        return 0;
    }

    // make sure to scroll whole pages in non-continuous Fit Content mode
    if (!IsContinuous(win.ctrl->GetDisplayMode()) &&
        ZOOM_FIT_CONTENT == win.ctrl->GetZoomVirtual()) {
        if (delta > 0)
            win.ctrl->GoToPrevPage();
        else
            win.ctrl->GoToNextPage();
        return 0;
    }

    if (gDeltaPerLine == 0)
        return 0;

    bool horizontal = (LOWORD(wParam) & MK_ALT) || IsAltPressed();
    if (horizontal)
        gSuppressAltKey = true;

    if (gDeltaPerLine < 0 && win.AsFixed()) {
        // scroll by (fraction of a) page
        SCROLLINFO si = { 0 };
        si.cbSize = sizeof(si);
        si.fMask  = SIF_PAGE;
        GetScrollInfo(win.hwndCanvas, horizontal ? SB_HORZ : SB_VERT, &si);
        if (horizontal)
            win.AsFixed()->ScrollXBy(-MulDiv(si.nPage, delta, WHEEL_DELTA));
        else
            win.AsFixed()->ScrollYBy(-MulDiv(si.nPage, delta, WHEEL_DELTA), true);
        return 0;
    }
    
   // added: shift while scrolling will scroll by half a page per tick
   //        really usefull for browsing long files
	if ((LOWORD(wParam) & MK_SHIFT) || IsShiftPressed()) {
		SendMessage(win.hwndCanvas, WM_VSCROLL, (delta>0) ? SB_HPAGEUP : SB_HPAGEDOWN, 0);
		return 0;
	}

    win.wheelAccumDelta += delta;
    int currentScrollPos = GetScrollPos(win.hwndCanvas, SB_VERT);

    while (win.wheelAccumDelta >= gDeltaPerLine) {
        if (horizontal)
            SendMessage(win.hwndCanvas, WM_HSCROLL, SB_LINELEFT, 0);
        else
            SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        win.wheelAccumDelta -= gDeltaPerLine;
    }
    while (win.wheelAccumDelta <= -gDeltaPerLine) {
        if (horizontal)
            SendMessage(win.hwndCanvas, WM_HSCROLL, SB_LINERIGHT, 0);
        else
            SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        win.wheelAccumDelta += gDeltaPerLine;
    }

    if (!horizontal && !IsContinuous(win.ctrl->GetDisplayMode()) &&
        GetScrollPos(win.hwndCanvas, SB_VERT) == currentScrollPos) {
        if (delta > 0)
            win.ctrl->GoToPrevPage(true);
        else
            win.ctrl->GoToNextPage();
    }

    return 0;
}

static LRESULT CanvasOnMouseHWheel(WindowInfo& win, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win.tocVisible && IsCursorOverWindow(win.hwndTocTree) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEHWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessage(win.hwndTocTree, message, wParam, lParam);
        gWheelMsgRedirect = false;
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    win.wheelAccumDelta += delta;

    while (win.wheelAccumDelta >= gDeltaPerLine) {
        SendMessage(win.hwndCanvas, WM_HSCROLL, SB_LINERIGHT, 0);
        win.wheelAccumDelta -= gDeltaPerLine;
    }
    while (win.wheelAccumDelta <= -gDeltaPerLine) {
        SendMessage(win.hwndCanvas, WM_HSCROLL, SB_LINELEFT, 0);
        win.wheelAccumDelta += gDeltaPerLine;
    }

    return TRUE;
}

static LRESULT OnGesture(WindowInfo& win, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!touch::SupportsGestures())
        return DefWindowProc(win.hwndFrame, message, wParam, lParam);

    HGESTUREINFO hgi = (HGESTUREINFO)lParam;
    GESTUREINFO gi = { 0 };
    gi.cbSize = sizeof(GESTUREINFO);

    BOOL ok = touch::GetGestureInfo(hgi, &gi);
    if (!ok) {
        touch::CloseGestureInfoHandle(hgi);
        return 0;
    }

    switch (gi.dwID) {
        case GID_ZOOM:
            if (gi.dwFlags != GF_BEGIN && win.AsFixed()) {
                float zoom = (float)LODWORD(gi.ullArguments) / (float)win.touchState.startArg;
                ZoomToSelection(&win, zoom, false, true);
            }
            win.touchState.startArg = LODWORD(gi.ullArguments);
            break;

        case GID_PAN:
            // Flicking left or right changes the page,
            // panning moves the document in the scroll window
            if (gi.dwFlags == GF_BEGIN) {
                win.touchState.panStarted = true;
                win.touchState.panPos = gi.ptsLocation;
                win.touchState.panScrollOrigX = GetScrollPos(win.hwndCanvas, SB_HORZ);
            }
            else if (win.touchState.panStarted) {
                int deltaX = win.touchState.panPos.x - gi.ptsLocation.x;
                int deltaY = win.touchState.panPos.y - gi.ptsLocation.y;
                win.touchState.panPos = gi.ptsLocation;

                if ((!win.AsFixed() || !IsContinuous(win.AsFixed()->GetDisplayMode())) &&
                    (gi.dwFlags & GF_INERTIA) && abs(deltaX) > abs(deltaY)) {
                    // Switch pages once we hit inertia in a horizontal direction (only in
                    // non-continuous modes, cf. https://github.com/sumatrapdfreader/sumatrapdf/issues/9 )
                    if (deltaX < 0)
                        win.ctrl->GoToPrevPage();
                    else if (deltaX > 0)
                        win.ctrl->GoToNextPage();
                    // When we switch pages, go back to the initial scroll position
                    // and prevent further pan movement caused by the inertia
                    if (win.AsFixed())
                        win.AsFixed()->ScrollXTo(win.touchState.panScrollOrigX);
                    win.touchState.panStarted = false;
                }
                else if (win.AsFixed()) {
                    // Pan/Scroll
                    win.MoveDocBy(deltaX, deltaY);
                }
            }
            break;

        case GID_ROTATE:
            // Rotate the PDF 90 degrees in one direction
            if (gi.dwFlags == GF_END && win.AsFixed()) {
                // This is in radians
                double rads = GID_ROTATE_ANGLE_FROM_ARGUMENT(LODWORD(gi.ullArguments));
                // The angle from the rotate is the opposite of the Sumatra rotate, thus the negative
                double degrees = -rads * 180 / M_PI;

                // Playing with the app, I found that I often didn't go quit a full 90 or 180
                // degrees. Allowing rotate without a full finger rotate seemed more natural.
                if (degrees < -120 || degrees > 120)
                    win.AsFixed()->RotateBy(180);
                else if (degrees < -45)
                    win.AsFixed()->RotateBy(-90);
                else if (degrees > 45)
                    win.AsFixed()->RotateBy(90);
            }
            break;

        case GID_TWOFINGERTAP:
            // Two-finger tap toggles fullscreen mode
            OnMenuViewFullscreen(&win);
            break;

        case GID_PRESSANDTAP:
            // Toggle between Fit Page, Fit Width and Fit Content (same as 'z')
            if (gi.dwFlags == GF_BEGIN)
                win.ToggleZoom();
            break;

        default:
            // A gesture was not recognized
            break;
    }

    touch::CloseGestureInfoHandle(hgi);
    return 0;
}

static LRESULT WndProcCanvasFixedPageUI(WindowInfo& win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        OnPaintDocument(win);
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_LBUTTONDOWN:
        OnMouseLeftButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_LBUTTONUP:
        OnMouseLeftButtonUp(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_LBUTTONDBLCLK:
        OnMouseLeftButtonDblClk(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_MBUTTONDOWN:
        SetTimer(hwnd, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, nullptr);
        // TODO: Create window that shows location of initial click for reference
        OnMouseMiddleButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_RBUTTONDOWN:
        OnMouseRightButtonDown(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_RBUTTONUP:
        OnMouseRightButtonUp(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_RBUTTONDBLCLK:
        OnMouseRightButtonDblClick(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_VSCROLL:
        OnVScroll(win, wParam);
        return 0;

    case WM_HSCROLL:
        OnHScroll(win, wParam);
        return 0;

    case WM_MOUSEWHEEL:
        return CanvasOnMouseWheel(win, msg, wParam, lParam);

    case WM_MOUSEHWHEEL:
        return CanvasOnMouseHWheel(win, msg, wParam, lParam);

    case WM_SETCURSOR:
        if (OnSetCursor(win, hwnd))
            return TRUE;
        return DefWindowProc(hwnd, msg, wParam, lParam);

    case WM_CONTEXTMENU:
        OnContextMenu(&win, 0, 0);
        return 0;

    case WM_GESTURE:
        return OnGesture(win, msg, wParam, lParam);

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

///// methods needed for ChmUI canvases (should be subclassed by HtmlHwnd) /////

static LRESULT WndProcCanvasChmUI(WindowInfo& win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_SETCURSOR:
        // TODO: make (re)loading a document always clear the infotip
        win.DeleteInfotip();
        return DefWindowProc(hwnd, msg, wParam, lParam);

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

///// methods needed for EbookUI canvases /////

// NO_INLINE to help in debugging https://github.com/sumatrapdfreader/sumatrapdf/issues/292
static NO_INLINE LRESULT CanvasOnMouseWheelEbook(WindowInfo& win, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win.tocVisible && IsCursorOverWindow(win.hwndTocTree)) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        LRESULT res = 0;
        if (!gWheelMsgRedirect) {
            gWheelMsgRedirect = true;
            res = SendMessage(win.hwndTocTree, message, wParam, lParam);
            gWheelMsgRedirect = false;
        }
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (delta > 0)
        win.ctrl->GoToPrevPage();
    else
        win.ctrl->GoToNextPage();
    return 0;
}

static LRESULT WndProcCanvasEbookUI(WindowInfo& win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    bool wasHandled;
    LRESULT res = win.AsEbook()->HandleMessage(msg, wParam, lParam, wasHandled);
    if (wasHandled)
        return res;

    switch (msg) {
    case WM_SETCURSOR:
        // TODO: make (re)loading a document always clear the infotip
        win.DeleteInfotip();
        return DefWindowProc(hwnd, msg, wParam, lParam);

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return CanvasOnMouseWheelEbook(win, msg, wParam, lParam);

    case WM_GESTURE:
        return OnGesture(win, msg, wParam, lParam);

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

///// methods needed for the About/Start screen /////

static void OnPaintAbout(WindowInfo& win)
{
    Timer t;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win.hwndCanvas, &ps);

    if (HasPermission(Perm_SavePreferences | Perm_DiskAccess) && gGlobalPrefs->rememberOpenedFiles && gGlobalPrefs->showStartPage) {
        DrawStartPage(win, win.buffer->GetDC(), gFileHistory, gRenderCache.textColor, gRenderCache.backgroundColor);
    } else {
        DrawAboutPage(win, win.buffer->GetDC());
    }
    win.buffer->Flush(hdc);

    EndPaint(win.hwndCanvas, &ps);
    if (gShowFrameRate) {
        ShowFrameRateDur(win.frameRateWnd, t.GetTimeInMs());
    }
}

static void OnMouseLeftButtonDownAbout(WindowInfo& win, int x, int y, WPARAM key)
{
    UNUSED(key);
    //lf("Left button clicked on %d %d", x, y);

    // remember a link under so that on mouse up we only activate
    // link if mouse up is on the same link as mouse down
    win.url = GetStaticLink(win.staticLinks, x, y);
}

static void OnMouseLeftButtonUpAbout(WindowInfo& win, int x, int y, WPARAM key)
{
    UNUSED(key);
    SetFocus(win.hwndFrame);

    const WCHAR *url = GetStaticLink(win.staticLinks, x, y);
    if (url && url == win.url) {
        if (str::Eq(url, SLINK_OPEN_FILE))
            SendMessage(win.hwndFrame, WM_COMMAND, IDM_OPEN, 0);
        else if (str::Eq(url, SLINK_LIST_HIDE)) {
            gGlobalPrefs->showStartPage = false;
            win.RedrawAll(true);
        } else if (str::Eq(url, SLINK_LIST_SHOW)) {
            gGlobalPrefs->showStartPage = true;
            win.RedrawAll(true);
        } else if (!str::StartsWithI(url, L"http:") &&
                   !str::StartsWithI(url, L"https:") &&
                   !str::StartsWithI(url, L"mailto:"))
        {
            LoadArgs args(url, &win);
            LoadDocument(args);
        } else
            LaunchBrowser(url);
    }
    win.url = nullptr;
}

static void OnMouseRightButtonDownAbout(WindowInfo& win, int x, int y, WPARAM key)
{
    UNUSED(key);
    //lf("Right button clicked on %d %d", x, y);
    SetFocus(win.hwndFrame);
    win.dragStart = PointI(x, y);
}

static void OnMouseRightButtonUpAbout(WindowInfo& win, int x, int y, WPARAM key)
{
    UNUSED(key);
    bool didDragMouse =
        abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
    if (!didDragMouse)
        OnAboutContextMenu(&win, x, y);
}

static LRESULT OnSetCursorAbout(WindowInfo& win, HWND hwnd)
{
    PointI pt;
    if (GetCursorPosInHwnd(hwnd, pt)) {
        StaticLinkInfo linkInfo;
        if (GetStaticLink(win.staticLinks, pt.x, pt.y, &linkInfo)) {
            win.CreateInfotip(linkInfo.infotip, linkInfo.rect);
            SetCursor(IDC_HAND);
        }
        else {
            win.DeleteInfotip();
            SetCursor(IDC_ARROW);
        }
        return TRUE;
    }

    win.DeleteInfotip();
    return FALSE;
}

static LRESULT WndProcCanvasAbout(WindowInfo& win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_LBUTTONDOWN:
        OnMouseLeftButtonDownAbout(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_LBUTTONUP:
        OnMouseLeftButtonUpAbout(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_LBUTTONDBLCLK:
        OnMouseLeftButtonDownAbout(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_RBUTTONDOWN:
        OnMouseRightButtonDownAbout(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_RBUTTONUP:
        OnMouseRightButtonUpAbout(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_SETCURSOR:
        if (OnSetCursorAbout(win, hwnd))
            return TRUE;
        return DefWindowProc(hwnd, msg, wParam, lParam);

    case WM_CONTEXTMENU:
        OnAboutContextMenu(&win, 0, 0);
        return 0;

    case WM_PAINT:
        OnPaintAbout(win);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

///// methods needed for FixedPageUI canvases with loading error /////

static void OnPaintError(WindowInfo& win)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win.hwndCanvas, &ps);

    ScopedFont fontRightTxt(CreateSimpleFont(hdc, L"MS Shell Dlg", 14));
    HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt);
    ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(GetNoDocBgColor()));
    FillRect(hdc, &ps.rcPaint, brush);
    // TODO: should this be "Error opening %s"?
    ScopedMem<WCHAR> msg(str::Format(_TR("Error loading %s"), win.currentTab->filePath));
    DrawCenteredText(hdc, ClientRect(win.hwndCanvas), msg, IsUIRightToLeft());
    SelectObject(hdc, hPrevFont);

    EndPaint(win.hwndCanvas, &ps);
}

static LRESULT WndProcCanvasLoadError(WindowInfo& win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        OnPaintError(win);
        return 0;

    case WM_SETCURSOR:
        // TODO: make (re)loading a document always clear the infotip
        win.DeleteInfotip();
        return DefWindowProc(hwnd, msg, wParam, lParam);

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

///// methods needed for all types of canvas /////

void WindowInfo::RepaintAsync(UINT delay)
{
    // even though RepaintAsync is mostly called from the UI thread,
    // we depend on the repaint message to happen asynchronously
    uitask::Post([=]{
        if (!WindowInfoStillValid(this))
            return;
        if (!delay)
            WndProcCanvas(this->hwndCanvas, WM_TIMER, REPAINT_TIMER_ID, 0);
        else if (!this->delayedRepaintTimer)
            this->delayedRepaintTimer = SetTimer(this->hwndCanvas, REPAINT_TIMER_ID, delay, nullptr);
    });
}

static void OnTimer(WindowInfo& win, HWND hwnd, WPARAM timerId)
{
    PointI pt;

    switch (timerId) {
    case REPAINT_TIMER_ID:
        win.delayedRepaintTimer = 0;
        KillTimer(hwnd, REPAINT_TIMER_ID);
        win.RedrawAll();
        break;

    case SMOOTHSCROLL_TIMER_ID:
        if (MA_SCROLLING == win.mouseAction)
            win.MoveDocBy(win.xScrollSpeed, win.yScrollSpeed);
        else if (MA_SELECTING == win.mouseAction || MA_SELECTING_TEXT == win.mouseAction) {
            GetCursorPosInHwnd(win.hwndCanvas, pt);
            if (NeedsSelectionEdgeAutoscroll(&win, pt.x, pt.y))
                OnMouseMove(win, pt.x, pt.y, MK_CONTROL);
        }
        else {
            KillTimer(hwnd, SMOOTHSCROLL_TIMER_ID);
            win.yScrollSpeed = 0;
            win.xScrollSpeed = 0;
        }
        break;

    case HIDE_CURSOR_TIMER_ID:
        KillTimer(hwnd, HIDE_CURSOR_TIMER_ID);
        if (win.presentation)
            SetCursor((HCURSOR)nullptr);
        break;

    case HIDE_FWDSRCHMARK_TIMER_ID:
        win.fwdSearchMark.hideStep++;
        if (1 == win.fwdSearchMark.hideStep) {
            SetTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS, nullptr);
        }
        else if (win.fwdSearchMark.hideStep >= HIDE_FWDSRCHMARK_STEPS) {
            KillTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID);
            win.fwdSearchMark.show = false;
            win.RepaintAsync();
        }
        else
            win.RepaintAsync();
        break;

    case AUTO_RELOAD_TIMER_ID:
        KillTimer(hwnd, AUTO_RELOAD_TIMER_ID);
        if (win.currentTab && win.currentTab->reloadOnFocus)
            ReloadDocument(&win, true);
        break;

    case EBOOK_LAYOUT_TIMER_ID:
        KillTimer(hwnd, EBOOK_LAYOUT_TIMER_ID);
        for (TabInfo *tab : win.tabs) {
            if (tab->AsEbook())
                tab->AsEbook()->TriggerLayout();
        }
        break;
    }
}

static void OnDropFiles(HDROP hDrop, bool dragFinish)
{
    WCHAR filePath[MAX_PATH];
    const int count = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, 0, 0);

    for (int i = 0; i < count; i++) {
        DragQueryFile(hDrop, i, filePath, dimof(filePath));
        if (str::EndsWithI(filePath, L".lnk")) {
            ScopedMem<WCHAR> resolved(ResolveLnk(filePath));
            if (resolved)
                str::BufSet(filePath, dimof(filePath), resolved);
        }
        // The first dropped document may override the current window
        LoadArgs args(filePath);
        LoadDocument(args);
    }
    if (dragFinish)
        DragFinish(hDrop);
}

LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // messages that don't require win
    switch (msg) {
    case WM_DROPFILES:
        CrashIf(lParam != 0 && lParam != 1);
        OnDropFiles((HDROP)wParam, !lParam);
        return 0;

    case WM_ERASEBKGND:
        // do nothing, helps to avoid flicker
        return TRUE;
    }

    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    // messages that require win
    switch (msg) {
    case WM_TIMER:
        OnTimer(*win, hwnd, wParam);
        return 0;

    case WM_SIZE:
        if (!IsIconic(win->hwndFrame))
            win->UpdateCanvasSize();
        return 0;

    case WM_GETOBJECT:
        // TODO: should we check for UiaRootObjectId, as in http://msdn.microsoft.com/en-us/library/windows/desktop/ff625912.aspx ???
        // On the other hand http://code.msdn.microsoft.com/windowsdesktop/UI-Automation-Clean-94993ac6/sourcecode?fileId=42883&pathId=2071281652
        // says that UiaReturnRawElementProvider() should be called regardless of lParam
        // Don't expose UIA automation in plugin mode yet. UIA is still too experimental
        if (!gPluginMode) {
            // disable UIAutomation in release builds until concurrency issues and
            // memory leaks have been figured out and fixed
#ifdef DEBUG
            if (win->CreateUIAProvider()) {
                // TODO: should win->uia_provider->Release() as in http://msdn.microsoft.com/en-us/library/windows/desktop/gg712214.aspx
                // and http://www.code-magazine.com/articleprint.aspx?quickid=0810112&printmode=true ?
                // Maybe instead of having a single provider per win, we should always create a new one
                // like in this sample: http://code.msdn.microsoft.com/windowsdesktop/UI-Automation-Clean-94993ac6/sourcecode?fileId=42883&pathId=2071281652
                // currently win->uia_provider refCount is really out of wack in WindowInfo::~WindowInfo
                // from logging it seems that UiaReturnRawElementProvider() increases refCount by 1
                // and since WM_GETOBJECT is called many times, it accumulates
                return uia::ReturnRawElementProvider(hwnd, wParam, lParam, win->uia_provider);
            }
#endif
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);

    default:
        // TODO: achieve this split through subclassing or different window classes
        if (win->AsFixed())
            return WndProcCanvasFixedPageUI(*win, hwnd, msg, wParam, lParam);

        if (win->AsChm())
            return WndProcCanvasChmUI(*win, hwnd, msg, wParam, lParam);

        if (win->AsEbook())
            return WndProcCanvasEbookUI(*win, hwnd, msg, wParam, lParam);

        if (win->IsAboutWindow())
            return WndProcCanvasAbout(*win, hwnd, msg, wParam, lParam);

        return WndProcCanvasLoadError(*win, hwnd, msg, wParam, lParam);
    }
}

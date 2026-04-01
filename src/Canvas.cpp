/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/Timer.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"
#include "utils/ThreadUtil.h"
#include "utils/HttpUtil.h"
#include "utils/GuessFileType.h"
#include <algorithm>
#include <shlobj.h>

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "wingui/FrameRateWnd.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "AppColors.h"
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "DisplayModel.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraConfig.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "EditAnnotations.h"
#include "Notifications.h"
#include "OverlayScrollbar.h"
#include "MainWindow.h"
#include "resource.h"
#include "Commands.h"
#include "Canvas.h"
#include "Menu.h"
#include "uia/Provider.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "HomePage.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "Translations.h"

#include "utils/Log.h"

// if set instead of trying to render pages we don't have, we simply do nothing
// this reduces the flickering when going quickly through pages but creates
// impression of lag
bool gNoFlickerRender = true;

Kind kNotifAnnotation = "notifAnnotation";

// OLE drag-drop support for dragging selected text out of the window
class TextDropSource : public IDropSource {
    LONG refCount = 1;

  public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }
    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        if (fEscapePressed) {
            return DRAGDROP_S_CANCEL;
        }
        if (!(grfKeyState & MK_LBUTTON)) {
            return DRAGDROP_S_DROP;
        }
        return S_OK;
    }
    STDMETHODIMP GiveFeedback(__unused DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

class TextDataObject : public IDataObject {
    LONG refCount = 1;
    HGLOBAL hText = nullptr;

  public:
    explicit TextDataObject(const WCHAR* text) {
        size_t cb = (str::Len(text) + 1) * sizeof(WCHAR);
        hText = GlobalAlloc(GMEM_MOVEABLE, cb);
        if (hText) {
            void* p = GlobalLock(hText);
            memcpy(p, text, cb);
            GlobalUnlock(hText);
        }
    }
    ~TextDataObject() {
        if (hText) {
            GlobalFree(hText);
        }
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }

    STDMETHODIMP GetData(FORMATETC* pFE, STGMEDIUM* pMedium) override {
        if (!hText) {
            return E_UNEXPECTED;
        }
        if (pFE->cfFormat != CF_UNICODETEXT || !(pFE->tymed & TYMED_HGLOBAL)) {
            return DV_E_FORMATETC;
        }
        size_t cb = GlobalSize(hText);
        HGLOBAL hCopy = GlobalAlloc(GMEM_MOVEABLE, cb);
        if (!hCopy) {
            return E_OUTOFMEMORY;
        }
        void* src = GlobalLock(hText);
        void* dst = GlobalLock(hCopy);
        memcpy(dst, src, cb);
        GlobalUnlock(hCopy);
        GlobalUnlock(hText);
        pMedium->tymed = TYMED_HGLOBAL;
        pMedium->hGlobal = hCopy;
        pMedium->pUnkForRelease = nullptr;
        return S_OK;
    }
    STDMETHODIMP GetDataHere(__unused FORMATETC*, __unused STGMEDIUM*) override { return E_NOTIMPL; }
    STDMETHODIMP QueryGetData(FORMATETC* pFE) override {
        if (pFE->cfFormat == CF_UNICODETEXT && (pFE->tymed & TYMED_HGLOBAL)) {
            return S_OK;
        }
        return DV_E_FORMATETC;
    }
    STDMETHODIMP GetCanonicalFormatEtc(__unused FORMATETC*, FORMATETC* pOut) override {
        pOut->ptd = nullptr;
        return E_NOTIMPL;
    }
    STDMETHODIMP SetData(__unused FORMATETC*, __unused STGMEDIUM*, __unused BOOL) override { return E_NOTIMPL; }
    STDMETHODIMP EnumFormatEtc(__unused DWORD, __unused IEnumFORMATETC**) override { return E_NOTIMPL; }
    STDMETHODIMP DAdvise(__unused FORMATETC*, __unused DWORD, __unused IAdviseSink*, __unused DWORD*) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP DUnadvise(__unused DWORD) override { return E_NOTIMPL; }
    STDMETHODIMP EnumDAdvise(__unused IEnumSTATDATA**) override { return E_NOTIMPL; }
};

static bool IsPointInSelection(MainWindow* win, Point pt) {
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

static void StartTextDragDrop(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    bool isTextOnly = false;
    TempStr text = GetSelectedTextTemp(tab, "\r\n", isTextOnly);
    if (str::IsEmpty(text)) {
        return;
    }
    WCHAR* wtext = ToWStrTemp(text);
    TextDataObject* dataObj = new TextDataObject(wtext);
    TextDropSource* dropSrc = new TextDropSource();
    DWORD dwEffect = 0;
    DoDragDrop(dataObj, dropSrc, DROPEFFECT_COPY, &dwEffect);
    dropSrc->Release();
    dataObj->Release();
}

// Resize handle positions that used in resizing annotations
enum class ResizeHandle {
    None = 0,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

// Size of resize handle hit area (in pixels)
constexpr int kResizeHandleSize = 8;

// Smooth scrolling factor. This is a value between 0 and 1.
// Each step, we scroll the needed delta times this factor.
// Therefore, a higher factor makes smooth scrolling faster.
static const double gSmoothScrollingFactor = 0.2;

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
    // WHEEL_DELTA equals 120, so gDeltaPerLine will be 40
    gDeltaPerLine = 0;
    if (ulScrollLines == (ULONG)-1) {
        gDeltaPerLine = -1;
    } else if (ulScrollLines != 0) {
        gDeltaPerLine = WHEEL_DELTA / ulScrollLines;
    }
    // logf("SPI_GETWHEELSCROLLLINES: ulScrollLines=%d, gDeltaPerLine=%d\n", (int)ulScrollLines, gDeltaPerLine);
}

///// methods needed for FixedPageUI canvases with document loaded /////

const char* scrollMsgStr(USHORT msg) {
    switch (msg) {
        case SB_LINEDOWN:
            return "SB_LINEDOWN";
        case SB_LINEUP:
            return "SB_LINEUP";
        case SB_HALF_PAGEDOWN:
            return "SB_HALF_PAGEDOWN";
        case SB_HALF_PAGEUP:
            return "SB_HALF_PAGEUP";
        case SB_PAGEDOWN:
            return "SB_PAGEDOWN";
        case SB_PAGEUP:
            return "SB_PAGEUP";
    }
    return str::FormatTemp("%d", (int)msg);
}

static void OnVScroll(MainWindow* win, WPARAM wp) {
    ReportIf(!win->AsFixed());

    bool useOverlay = gGlobalPrefs->fixedPageUI.useOverlayScrollbar && IsOverlayScrollbarVisible(win->overlayScrollV);
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    if (useOverlay) {
        OverlayScrollbarGetInfo(win->overlayScrollV, &si);
    } else {
        GetScrollInfo(win->hwndCanvas, SB_VERT, &si);
    }

    USHORT msg = LOWORD(wp);
    auto* ctrl = win->ctrl;
    bool dmIsSinglePage = (ctrl->GetDisplayMode() == DisplayMode::SinglePage);
    // scrollbarInSinglePage is false by default
    // if true, we show scrollbar in single page mode and make its position correspond to page number, so user can
    // scroll through pages using scrollbar even in single page mode
    bool singlePageWithScrollbar = gGlobalPrefs->scrollbarInSinglePage && dmIsSinglePage;

    int lineHeight = DpiScale(win->hwndCanvas, 16);
    bool isFitPage = (kZoomFitPage == ctrl->GetZoomVirtual());
    if (!IsContinuous(ctrl->GetDisplayMode()) && isFitPage) {
        lineHeight = 1;
    }
    // logf("OnVscroll: msg=%s, min: %d, max: %d, nPage: %d, pos: %d, fit page: %d, lineHeight: %d,
    // singlePageWithScrollbar: %d\n", scrollMsgStr(msg), si.nMin,
    //      si.nMax, si.nPage, si.nPos, isFitPage ? 1 : 0, lineHeight, singlePageWithScrollbar);

    if (singlePageWithScrollbar) {
        // In SinglePage mode, scrollbar position directly corresponds to page number
        int targetPage = ctrl->CurrentPageNo();

        switch (msg) {
            case SB_TOP:
                targetPage = 1;
                break;
            case SB_BOTTOM:
                targetPage = ctrl->PageCount();
                break;
            case SB_LINEUP:
                targetPage = std::max(1, targetPage - 1);
                break;
            case SB_LINEDOWN:
                targetPage = std::min(ctrl->PageCount(), targetPage + 1);
                break;
            case SB_HALF_PAGEUP:
                targetPage = std::max(1, targetPage - 1);
                break;
            case SB_HALF_PAGEDOWN:
                targetPage = std::min(ctrl->PageCount(), targetPage + 1);
                break;
            case SB_PAGEUP:
                targetPage = std::max(1, targetPage - 1);
                break;
            case SB_PAGEDOWN:
                targetPage = std::min(ctrl->PageCount(), targetPage + 1);
                break;
            case SB_THUMBTRACK:
                targetPage = si.nTrackPos + 1;
                break;
        }

        // Navigate to the target page
        if (targetPage != ctrl->CurrentPageNo()) {
            ctrl->GoToPage(targetPage, true);
        }
        return;
    }

    // Original logic for other display modes

    int currPos = si.nPos;
    int halfPage = si.nPage / 2;
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
        case SB_HALF_PAGEUP:
            si.nPos -= halfPage;
            break;
        case SB_HALF_PAGEDOWN:
            si.nPos += halfPage;
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
    // logf("OnVScroll: nPos: %d\n", si.nPos);

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    bool showScrollbar = !gGlobalPrefs->fixedPageUI.hideScrollbars;
    BOOL showWinScrollbar = showScrollbar && !useOverlay;
    BOOL showOverScrollbar = showScrollbar && useOverlay;
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, showWinScrollbar);
    GetScrollInfo(win->hwndCanvas, SB_VERT, &si);
    OverlayScrollbarSetInfo(win->overlayScrollV, &si, showOverScrollbar);

    // If the position has changed or we're dealing with a touchpad scroll event,
    // scroll the window and update it
    if (si.nPos != currPos || msg == SB_THUMBTRACK) {
        if (gGlobalPrefs->smoothScroll) {
            win->scrollTargetY = si.nPos;
            SetTimer(win->hwndCanvas, kSmoothScrollTimerID, USER_TIMER_MINIMUM, nullptr);
        } else {
            win->AsFixed()->ScrollYTo(si.nPos);
        }
    }
}

static void OnHScroll(MainWindow* win, WPARAM wp) {
    ReportIf(!win->AsFixed());

    bool useOverlay = gGlobalPrefs->fixedPageUI.useOverlayScrollbar && IsOverlayScrollbarVisible(win->overlayScrollH);
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    if (useOverlay) {
        OverlayScrollbarGetInfo(win->overlayScrollH, &si);
    } else {
        GetScrollInfo(win->hwndCanvas, SB_HORZ, &si);
    }

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
    if (useOverlay) {
        OverlayScrollbarSetInfo(win->overlayScrollH, &si, TRUE);
    }

    // If the position has changed or we're dealing with a touchpad scroll event,
    // scroll the window and update it
    if (si.nPos != currPos || msg == SB_THUMBTRACK) {
        win->AsFixed()->ScrollXTo(si.nPos);
    }
}

static void DrawMovePattern(MainWindow* win, Point pt, Size size) {
    HWND hwnd = win->hwndCanvas;
    HDC hdc = GetDC(hwnd);
    auto [x, y] = pt;
    auto [dx, dy] = size;
    x += win->annotationBeingMovedOffset.x;
    y += win->annotationBeingMovedOffset.y;
    SetBrushOrgEx(hdc, x, y, nullptr);
    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, win->brMovePattern);
    PatBlt(hdc, x, y, dx, dy, PATINVERT);
    SelectObject(hdc, hbrushOld);
    ReleaseDC(hwnd, hdc);
}

static void StartMouseDrag(MainWindow* win, int x, int y, bool right = false) {
    SetCapture(win->hwndCanvas);
    win->mouseAction = MouseAction::Dragging;
    win->dragRightClick = right;
    win->dragPrevPos = Point(x, y);
    if (GetCursor()) {
        SetCursor(gCursorDrag);
    }
}

// Get the resize handle at the given point for the selected annotation
static ResizeHandle GetResizeHandleAt(MainWindow* win, Point pt, Annotation* annot) {
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

// Get the appropriate cursor for a resize handle
static LPWSTR GetCursorForResizeHandle(ResizeHandle handle) {
    switch (handle) {
        case ResizeHandle::TopLeft:
        case ResizeHandle::BottomRight:
            return IDC_SIZENWSE;
        case ResizeHandle::TopRight:
        case ResizeHandle::BottomLeft:
            return IDC_SIZENESW;
        case ResizeHandle::Top:
        case ResizeHandle::Bottom:
            return IDC_SIZENS;
        case ResizeHandle::Left:
        case ResizeHandle::Right:
            return IDC_SIZEWE;
        default:
            return IDC_ARROW;
    }
}

// return true if this was annotation dragging
static bool StopDraggingAnnotation(MainWindow* win, int x, int y, bool aborted) {
    Annotation* annot = win->annotationBeingDragged;
    if (!annot) {
        return false;
    }
    DrawMovePattern(win, win->dragPrevPos, win->annotationBeingMovedSize);

    win->annotationBeingDragged = nullptr;
    if (aborted) {
        return true;
    }

    DisplayModel* dm = win->AsFixed();
    x += win->annotationBeingMovedOffset.x;
    y += win->annotationBeingMovedOffset.y;
    Point pt{x, y};
    int pageNo = dm->GetPageNoByPoint(pt);
    // we can only move annotation within the same page
    if (pageNo == PageNo(annot)) {
        Rect rScreen{x, y, 1, 1};
        RectF r = dm->CvtFromScreen(rScreen, pageNo);
        RectF ar = GetRect(annot);
        r.dx = ar.dx;
        r.dy = ar.dy;
        // logf("prev rect: x=%.2f, y=%.2f, dx=%.2f, dy=%.2f\n", ar.x, ar.y, ar.dx, ar.dy);
        // logf(" new rect: x=%.2f, y=%.2f, dx=%.2f, dy=%.2f\n", r.x, r.y, r.dx, r.dy);
        SetRect(annot, r);
        NotifyAnnotationsChanged(win->CurrentTab()->editAnnotsWindow);
        MainWindowRerender(win);
        ToolbarUpdateStateForWindow(win, true);
    }
    return true;
}

static void StopMouseDrag(MainWindow* win, int x, int y, bool aborted) {
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

void CancelDrag(MainWindow* win) {
    auto pt = win->dragPrevPos;
    auto [x, y] = pt;
    StopMouseDrag(win, x, y, true);
    win->mouseAction = MouseAction::None;
    win->linkOnLastButtonDown = nullptr;
    win->annotationBeingDragged = nullptr;
    win->annotationBeingResized = false;
    SetCursorCached(IDC_ARROW);
}

bool IsDragDistance(int x1, int x2, int y1, int y2) {
    int dx = abs(x1 - x2);
    int dragDx = GetSystemMetrics(SM_CXDRAG);
    if (dx > dragDx) {
        return true;
    }

    int dy = abs(y1 - y2);
    int dragDy = GetSystemMetrics(SM_CYDRAG);
    return dy > dragDy;
}

static bool gShowAnnotationNotification = true;

// Forward declaration
static RectF CalculateResizedRect(MainWindow* win, int x, int y);

static void OnMouseMove(MainWindow* win, int x, int y, WPARAM) {
    DisplayModel* dm = win->AsFixed();
    // ReportIf(!dm); // can happen if reload fails, we delete DisplayModel
    if (!dm) return;

    if (win->InPresentation()) {
        if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
            // logf("OnMouseMove: hiding cursor because black screen or white screen\n");
            SetCursor((HCURSOR) nullptr);
            return;
        }

        bool showingCursor = (GetCursor() != nullptr);
        bool sameAsLastPos = win->dragPrevPos.Eq(x, y);
        // logf("OnMouseMove(): win->InPresentation() (%d, %d) showingCursor: %d, same as last pos: %d\n", x,
        // y,
        //     (int)showingCursor, (int)sameAsLastPos);
        if (!sameAsLastPos) {
            // shortly display the cursor if the mouse has moved and the cursor is hidden
            if (!showingCursor) {
                // logf("OnMouseMove: temporary showing cursor\n");
                if (win->mouseAction == MouseAction::None) {
                    SetCursorCached(IDC_ARROW);
                } else {
                    SendMessageW(win->hwndCanvas, WM_SETCURSOR, 0, 0);
                }
            }
            if (win->dragPrevPos.Eq(-2, -3)) {
                // hack: hide cursor immediately. see EnterFullScreen
                SetTimer(win->hwndCanvas, kHideCursorTimerID, 1, nullptr);
            } else {
                // logf("OnMouseMove: starting kHideCursorTimerID\n");
                SetTimer(win->hwndCanvas, kHideCursorTimerID, kHideCursorDelayInMs, nullptr);
            }
        }
    }

    Point pos{x, y};
    NotificationWnd* cursorPosNotif = GetNotificationForGroup(win->hwndCanvas, kNotifCursorPos);

    if (win->textDragPending) {
        if (!IsDragDistance(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        // threshold met: initiate OLE drag-drop of selected text
        win->textDragPending = false;
        win->dragStartPending = false;
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        StartTextDragDrop(win);
        return;
    }

    if (win->dragStartPending) {
        if (!IsDragDistance(x, win->dragStart.x, y, win->dragStart.y)) {
            return;
        }
        win->dragStartPending = false;
        win->linkOnLastButtonDown = nullptr;
    }

    Point prevPos = win->dragPrevPos;
    switch (win->mouseAction) {
        case MouseAction::None: {
            Annotation* annot = dm->GetAnnotationAtPos(pos, nullptr);
            Annotation* prev = win->annotationUnderCursor;
            if (annot != prev) {
#if 0
                TempStr name = annot ? AnnotationReadableNameTemp(annot->type) : (TempStr) "none";
                TempStr prevName = prev ? AnnotationReadableNameTemp(prev->type) : (TempStr) "none";
                logf("different annot under cursor. prev: %s, new: %s\n", prevName, name);
#endif
                if (gShowAnnotationNotification) {
                    if (annot) {
                        // auto r = annot->bounds;
                        // logf("new pos: %d-%d, size: %d-%d\n", (int)r.x, (int)r.y, (int)r.dx, (int)r.dy);
                        RemoveNotificationsForGroup(win->hwndCanvas, kNotifAnnotation);
                        NotificationCreateArgs args;
                        args.hwndParent = win->hwndCanvas;
                        args.groupId = kNotifAnnotation;
                        args.timeoutMs = 3000;
                        args.delayInMs = 1000;
                        args.noClose = true;
                        TempStr name = annot ? AnnotationReadableNameTemp(annot->type) : (TempStr) "none";
                        const char* fmt = _TRA("%s annotation. Ctrl+click to edit.");
                        args.msg = str::FormatTemp(fmt, name);
                        ShowNotification(args);
                    }
                }
            }
            if (!annot) {
                RemoveNotificationsForGroup(win->hwndCanvas, kNotifAnnotation);
            }
            win->annotationUnderCursor = annot;
            break;
        }

        case MouseAction::Scrolling: {
            win->annotationUnderCursor = nullptr;
            win->yScrollSpeed = (y - win->dragStart.y) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
            win->xScrollSpeed = (x - win->dragStart.x) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
            break;
        }
        case MouseAction::SelectingText:
            if (GetCursor()) {
                SetCursorCached(IDC_IBEAM);
            }
            [[fallthrough]];
        case MouseAction::Selecting: {
            win->annotationUnderCursor = nullptr;
            win->selectionRect.dx = x - win->selectionRect.x;
            win->selectionRect.dy = y - win->selectionRect.y;
            win->selectionMeasure = dm->CvtFromScreen(win->selectionRect).Size();
            OnSelectionEdgeAutoscroll(win, x, y);
            ScheduleRepaint(win, 0);
            break;
        }
        case MouseAction::Dragging: {
            Annotation* annot = win->annotationBeingDragged;
            if (annot) {
                if (win->annotationBeingResized) {
                    // During resize, calculate and apply new rectangle in real-time
                    win->dragPrevPos = pos;
                    // Keep the resize cursor active during resize
                    SetCursorCached(GetCursorForResizeHandle((ResizeHandle)win->resizeHandle));

                    // Calculate and apply the new rectangle based on current mouse position
                    RectF newRect = CalculateResizedRect(win, x, y);
                    SetRect(annot, newRect);

                    MainWindowRerender(win);
                } else {
                    Size size = win->annotationBeingMovedSize;
                    DrawMovePattern(win, prevPos, size);
                    DrawMovePattern(win, pos, size);
                }
            } else {
                win->MoveDocBy(win->dragPrevPos.x - x, win->dragPrevPos.y - y);
            }
            break;
        }
    }
    win->dragPrevPos = pos;

    if (cursorPosNotif) {
        UpdateCursorPositionHelper(win, pos, cursorPosNotif);
    }
}

static void StartAnnotationDrag(MainWindow* win, Annotation* annot, Point& pt) {
    win->annotationBeingDragged = annot;
    DisplayModel* dm = win->AsFixed();
    CreateMovePatternLazy(win);
    RectF r = GetRect(annot);
    int pageNo = PageNo(annot);
    Rect rScreen = dm->CvtToScreen(pageNo, r);
    win->annotationBeingMovedSize = {rScreen.dx, rScreen.dy};
    int offsetX = rScreen.x - pt.x;
    int offsetY = rScreen.y - pt.y;
    win->annotationBeingMovedOffset = Point{offsetX, offsetY};
    DrawMovePattern(win, pt, win->annotationBeingMovedSize);
}

// Helper function to calculate new rectangle during resize
static RectF CalculateResizedRect(MainWindow* win, int x, int y) {
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

    return r;
}

static void StartAnnotationResize(MainWindow* win, Annotation* annot, Point& pt, ResizeHandle handle) {
    win->annotationBeingDragged = annot;
    win->annotationBeingResized = true;
    win->resizeHandle = (int)handle;
    win->dragStart = pt;
    RectF r = GetRect(annot);
    win->annotationOriginalRect = r;
    SetCapture(win->hwndCanvas);
    win->mouseAction = MouseAction::Dragging;
    win->dragPrevPos = pt;
}

static bool StopAnnotationResize(MainWindow* win, int x, int y, bool aborted) {
    if (!win->annotationBeingResized) {
        return false;
    }

    Annotation* annot = win->annotationBeingDragged;
    win->annotationBeingResized = false;
    win->annotationBeingDragged = nullptr;

    // Release mouse capture and reset cursor
    if (GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }
    SetCursorCached(IDC_ARROW);

    if (aborted || !annot) {
        return true;
    }

    // The annotation has already been updated during mouse move,
    // just notify and update toolbar
    NotifyAnnotationsChanged(win->CurrentTab()->editAnnotsWindow);
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, true);

    return true;
}

static void OnMouseLeftButtonDown(MainWindow* win, int x, int y, WPARAM key) {
    // lf("Left button clicked on %d %d", x, y);
    if (IsRightDragging(win)) {
        return;
    }

    if (MouseAction::Scrolling == win->mouseAction) {
        win->mouseAction = MouseAction::None;
        return;
    }

    if (win->mouseAction != MouseAction::None) {
        // this can be MouseAction::SelectingText (4)
        // can't reproduce it so far
        logf("OnMouseLeftButtonDown: win->mouseAction=%d\n", (int)win->mouseAction);
        // ReportIf(win->mouseAction != MouseAction::Idle);
        win->mouseAction = MouseAction::None;
        return;
    }

    HwndSetFocus(win->hwndFrame);
    DisplayModel* dm = win->AsFixed();
    ReportIf(!dm);
    Point pt{x, y};

    WindowTab* tab = win->CurrentTab();
    Annotation* annot = dm->GetAnnotationAtPos(pt, tab->selectedAnnotation);
    bool isMoveableAnnot = annot && AnnotationCanBeMoved(annot->type);
    if (isMoveableAnnot) {
        if (annot == tab->selectedAnnotation) {
            // dragging the selected annotation. do nothing here, just start dragging in mouse move
        } else if (tab->editAnnotsWindow || tab->selectedAnnotation) {
            // clicking on a different annotation while edit annotations window is open. or
            // other annotation is selected, select the clicked annotation and start dragging yet
            SetSelectedAnnotation(tab, annot);
        } else {
            isMoveableAnnot = false;
        }
    }

    // Check if we're clicking on a resize handle of the selected annotation
    // must check selectedAnnotation directly (not annot) because resize handles
    // extend beyond annotation bounds and GetAnnotationAtPos() won't find them
    ResizeHandle resizeHandle = ResizeHandle::None;
    if (tab->selectedAnnotation && AnnotationCanBeResized(tab->selectedAnnotation->type)) {
        resizeHandle = GetResizeHandleAt(win, pt, tab->selectedAnnotation);
    }

    if (resizeHandle != ResizeHandle::None) {
        StartAnnotationResize(win, tab->selectedAnnotation, pt, resizeHandle);
    } else if (isMoveableAnnot) {
        StartAnnotationDrag(win, annot, pt);
    } else {
        ReportIf(win->linkOnLastButtonDown);
        IPageElement* pageEl = dm->GetElementAtPos(pt, nullptr);
        if (pageEl) {
            if (pageEl->Is(kindPageElementDest)) {
                win->linkOnLastButtonDown = pageEl;
            }
        }
    }

    win->dragStartPending = true;
    win->dragStart = pt;
    win->textDragPending = false;

    // - without modifiers, clicking on text starts a text selection
    //   and clicking somewhere else starts a drag
    // - pressing Shift forces dragging
    // - pressing Ctrl forces a rectangular selection
    // - pressing Ctrl+Shift forces text selection
    // - not having CopySelection permission forces dragging
    bool isShift = IsShiftPressed();
    bool isCtrl = IsCtrlPressed();
    bool canCopy = HasPermission(Perm::CopySelection);
    bool isOverText = win->AsFixed()->IsOverText(pt);

    // if clicking on already selected text, prepare for drag-out instead of new selection
    if (canCopy && !isShift && !isCtrl && isOverText && win->showSelection && IsPointInSelection(win, pt)) {
        win->textDragPending = true;
        win->linkOnLastButtonDown = nullptr;
        SetCapture(win->hwndCanvas);
        return;
    }

    if (resizeHandle != ResizeHandle::None || isMoveableAnnot || !canCopy || (isShift || !isOverText) && !isCtrl) {
        StartMouseDrag(win, x, y);
    } else {
        OnSelectionStart(win, x, y, key);
    }
}

static void OnMouseLeftButtonUp(MainWindow* win, int x, int y, WPARAM key) {
    DisplayModel* dm = win->AsFixed();
    ReportIf(!dm);

    // click on selected text without dragging: clear selection
    if (win->textDragPending) {
        win->textDragPending = false;
        win->dragStartPending = false;
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        DeleteOldSelectionInfo(win, true);
        return;
    }

    auto ma = win->mouseAction;
    if (MouseAction::None == ma || IsRightDragging(win)) {
        return;
    }

    if (MouseAction::Scrolling == ma) {
        win->mouseAction = MouseAction::None;
        // TODO: I'm seeing this in crash reports. Can we get button up without button down?
        // maybe when down happens on a different hwnd? How can I add more logging.
        // logfa("OnMouseLeftButtonUp: unexpected MouseAction::Scrolling (%d)\n", ma);
        // ReportIf(true);
        return;
    }

    // TODO: should IsDrag() ever be true here? We should get mouse move first
    bool didDragMouse = !win->dragStartPending || IsDragDistance(x, win->dragStart.x, y, win->dragStart.y);
    if (MouseAction::Dragging == ma) {
        if (win->annotationBeingResized) {
            StopAnnotationResize(win, x, y, !didDragMouse);
            // Trigger cursor update after resize
            SendMessageW(win->hwndCanvas, WM_SETCURSOR, 0, 0);
        } else {
            StopMouseDrag(win, x, y, !didDragMouse);
        }
    } else {
        OnSelectionStop(win, x, y, !didDragMouse);
        if (MouseAction::Selecting == ma && win->showSelection) {
            win->selectionMeasure = dm->CvtFromScreen(win->selectionRect).Size();
        }
    }

    win->mouseAction = MouseAction::None;

    Point pt(x, y);
    int pageNo = dm->GetPageNoByPoint(pt);
    PointF ptPage = dm->CvtFromScreen(pt, pageNo);

    // TODO: win->linkHandler->GotoLink might spin the event loop
    IPageElement* link = win->linkOnLastButtonDown;
    win->linkOnLastButtonDown = nullptr;

    WindowTab* tab = win->CurrentTab();
    if (didDragMouse) {
        // no-op
        return;
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        /* return from white/black screens in presentation mode */
        win->ChangePresentationMode(PM_ENABLED);
        return;
    }

    if (IsCtrlPressed() && win->annotationUnderCursor) {
        ShowEditAnnotationsWindow(tab, win->annotationUnderCursor);
        return;
    }

    if (win->annotationUnderCursor && (tab->selectedAnnotation || tab->editAnnotsWindow)) {
        SetSelectedAnnotation(tab, win->annotationUnderCursor);
        return;
    }

    if (link && link->GetRect().Contains(ptPage)) {
        /* follow an active link */
        IPageDestination* dest = link->AsLink();
        // highlight the clicked link (as a reminder of the last action once the user returns)
        Kind kind = nullptr;
        if (dest) {
            kind = dest->GetKind();
        }
        if ((kindDestinationLaunchURL == kind || kindDestinationLaunchFile == kind)) {
            DeleteOldSelectionInfo(win, true);
            tab->selectionOnPage = SelectionOnPage::FromRectangle(dm, dm->CvtToScreen(pageNo, link->GetRect()));
            win->showSelection = tab->selectionOnPage != nullptr;
            ScheduleRepaint(win, 0);
        }
        SetCursorCached(IDC_ARROW);

        // Ctrl+click on internal link: open in new tab and navigate there
        bool isInternal = (kindDestinationLaunchURL != kind && kindDestinationLaunchFile != kind);
        if (IsCtrlPressed() && dest && isInternal && tab->filePath) {
            LoadArgs args(tab->filePath, win);
            args.showWin = true;
            args.noPlaceWindow = true;
            args.forceReuse = false;
            MainWindow* newWin = LoadDocument(&args);
            if (newWin && newWin->IsDocLoaded()) {
                newWin->linkHandler->ScrollTo(dest);
            }
            return;
        }

        win->ctrl->HandleLink(dest, win->linkHandler);
        // win->linkHandler->GotoLink(dest);
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
        ScheduleRepaint(win, 0);
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

bool gDisableInteractiveInverseSearch = false;

static void OnMouseLeftButtonDblClk(MainWindow* win, int x, int y, WPARAM key) {
    // lf("Left button clicked on %d %d", x, y);
    auto isLeft = bit::IsMaskSet(key, (WPARAM)MK_LBUTTON);
    if (gGlobalPrefs->enableTeXEnhancements && !gDisableInteractiveInverseSearch && isLeft) {
        bool dontSelect = OnInverseSearch(win, x, y);
        if (dontSelect) {
            return;
        }
    }

    DisplayModel* dm = win->AsFixed();
    // note: before 3.5 double-click used to turn 2 pages
    // OnMouseLeftButtonDown(win, x, y, key);
    Point mousePos = Point(x, y);
    bool isOverText = dm->IsOverText(mousePos);

    if (isLeft && (win->presentation || win->isFullScreen)) {
        // in fullscreen we allow to exit by tapping in upper right corner
        constexpr int kCornerSize = 64;
        Rect r = ClientRect(win->hwndCanvas);
        if (!isOverText && (x >= (r.dx - kCornerSize)) && (y < kCornerSize)) {
            ExitFullScreen(win);
            return;
        }
    }

    int elementPageNo = -1;
    IPageElement* pageEl = dm->GetElementAtPos(mousePos, &elementPageNo);
    if (isOverText) {
        int pageNo = dm->GetPageNoByPoint(mousePos);
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(mousePos, pageNo);
            dm->textSelection->SelectWordAt(pageNo, pt.x, pt.y);
            UpdateTextSelection(win, false);
            ScheduleRepaint(win, 0);
        }
        return;
    }

    if (!pageEl) {
        return;
    }
    if (pageEl->Is(kindPageElementDest)) {
        // speed up navigation in a file where navigation links are in a fixed position
        OnMouseLeftButtonDown(win, x, y, key);
    } else if (pageEl->Is(kindPageElementImage)) {
        // select an image that could be copied to the clipboard
        Rect rc = dm->CvtToScreen(elementPageNo, pageEl->GetRect());

        DeleteOldSelectionInfo(win, true);
        win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(dm, rc);
        win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
        ScheduleRepaint(win, 0);
    }
}

static void OnMouseMiddleButtonDown(MainWindow* win, int x, int y, WPARAM) {
    // Handle message by recording placement then moving document as mouse moves.

    switch (win->mouseAction) {
        case MouseAction::None:
            win->mouseAction = MouseAction::Scrolling;

            win->dragStartPending = true;
            // record current mouse position, the farther the mouse is moved
            // from this position, the faster we scroll the document
            win->dragStart = Point(x, y);
            SetCursorCached(IDC_SIZEALL);
            break;

        case MouseAction::Scrolling:
            win->mouseAction = MouseAction::None;
            break;
    }
}

static void OnMouseMiddleButtonUp(MainWindow* win, int x, int y, WPARAM) {
    switch (win->mouseAction) {
        case MouseAction::Scrolling:
            if (!win->dragStartPending) {
                win->mouseAction = MouseAction::None;
                SetCursorCached(IDC_ARROW);
                break;
            }
    }
}

static void OnMouseRightButtonDown(MainWindow* win, int x, int y) {
    // lf("Right button clicked on %d %d", x, y);
    if (MouseAction::Scrolling == win->mouseAction) {
        win->mouseAction = MouseAction::None;
    } else if (win->mouseAction != MouseAction::None) {
        return;
    }
    ReportIf(!win->AsFixed());

    HwndSetFocus(win->hwndFrame);

    win->dragStartPending = true;
    win->dragStart = Point(x, y);

    StartMouseDrag(win, x, y, true);
}

static void OnMouseRightButtonUp(MainWindow* win, int x, int y, WPARAM key) {
    ReportIf(!win->AsFixed());
    if (!IsRightDragging(win)) {
        return;
    }

    int isDragXOrY = IsDragDistance(x, win->dragStart.x, y, win->dragStart.y);
    bool didDragMouse = !win->dragStartPending || isDragXOrY;
    StopMouseDrag(win, x, y, !didDragMouse);

    win->mouseAction = MouseAction::None;

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

static void OnMouseRightButtonDblClick(MainWindow* win, int x, int y, WPARAM key) {
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
        AutoDeleteBrush brush = CreateSolidBrush(COL_PAGE_SHADOW);
        FillRect(hdc, &shadow.ToRECT(), brush);
    }

    // Draw frame
    ScopedGdiObj<HPEN> pe(CreatePen(PS_SOLID, 1, presentation ? TRANSPARENT : COL_PAGE_FRAME));
    AutoDeleteBrush brush = CreateSolidBrush(gCurrentTheme->window.backgroundColor);
    SelectObject(hdc, pe);
    SelectObject(hdc, brush);
    Rectangle(hdc, frame.x, frame.y, frame.x + frame.dx, frame.y + frame.dy);
}
#else
static void PaintPageFrameAndShadow(HDC hdc, Rect& bounds, Rect&, bool) {
    AutoDeletePen pen(CreatePen(PS_NULL, 0, 0));
    COLORREF bgCol;
    ThemeDocumentColors(bgCol);
    AutoDeleteBrush brush(CreateSolidBrush(bgCol));
    ScopedSelectPen restorePen(hdc, pen);
    ScopedSelectObject restoreBrush(hdc, brush);
    Rectangle(hdc, bounds.x, bounds.y, bounds.x + bounds.dx + 1, bounds.y + bounds.dy + 1);
}
#endif

/* debug code to visualize links (can block while rendering) */
static void DebugShowLinks(DisplayModel* dm, HDC hdc) {
    if (!gGlobalPrefs->showLinks) {
        return;
    }

    Rect viewPortRect(Point(), dm->GetViewPort().Size());

    ScopedSelectObject autoPen(hdc, CreatePen(PS_SOLID, 1, RGB(0x00, 0x00, 0xff)), true);

    for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pageInfo = dm->GetPageInfo(pageNo);
        if (!pageInfo || !pageInfo->shown || 0.0 == pageInfo->visibleRatio) {
            continue;
        }

        Vec<IPageElement*> els = dm->GetEngine()->GetElements(pageNo);

        for (auto& el : els) {
            if (el->Is(kindPageElementImage)) {
                continue;
            }
            Rect rect = dm->CvtToScreen(pageNo, el->GetRect());
            Rect isect = viewPortRect.Intersect(rect);
            if (!isect.IsEmpty()) {
                isect.Inflate(2, 2);
                DrawRect(hdc, isect);
            }
        }
    }

    if (false && dm->GetZoomVirtual() == kZoomFitContent) {
        // also display the content box when fitting content
        for (int pageNo = dm->PageCount(); pageNo >= 1; --pageNo) {
            PageInfo* pageInfo = dm->GetPageInfo(pageNo);
            if (!pageInfo->shown || 0.0 == pageInfo->visibleRatio) {
                continue;
            }

            auto cbbox = dm->GetEngine()->PageContentBox(pageNo);
            Rect rect = dm->CvtToScreen(pageNo, cbbox);
            DrawRect(hdc, rect);
        }
    }
}

// cf. https://web.archive.org/web/20140201011540/http://forums.fofou.org/sumatrapdf/topic?id=3183580&comments=15
static void GetGradientColor(COLORREF a, COLORREF b, float perc, TRIVERTEX* tv) {
    u8 ar, ag, ab;
    u8 br, bg, bb;
    UnpackColor(a, ar, ag, ab);
    UnpackColor(b, br, bg, bb);

    tv->Red = (COLOR16)((ar + perc * (br - ar)) * 256);
    tv->Green = (COLOR16)((ag + perc * (bg - ag)) * 256);
    tv->Blue = (COLOR16)((ab + perc * (bb - ab)) * 256);
}

// Draw a border around selected annotation
static bool gDrawOldStyleAnnotationRect = false;

NO_INLINE static void PaintCurrentEditAnnotationMark(WindowTab* tab, HDC hdc, DisplayModel* dm) {
    if (!tab) {
        return;
    }
    Annotation* annot = tab->selectedAnnotation;
    if (!annot) {
        return;
    }
    int pageNo = annot->pageNo;
    if (!dm->PageVisible(pageNo)) {
        // CvtToScreen() might not work if page is not visible because
        // it might not have zoom etc. calculated yet
        return;
    }
    bool canResize = AnnotationCanBeResized(annot->type);

    Rect rect = dm->CvtToScreen(pageNo, GetRect(annot));
    if (!tab->didScrollToSelectedAnnotation) {
        dm->ScrollScreenToRect(pageNo, rect);
        tab->didScrollToSelectedAnnotation = true;
    }
    rect.Inflate(4, 4);

    Gdiplus::Graphics gs(hdc);

    if (gDrawOldStyleAnnotationRect) {
        Gdiplus::Color col = GdiRgbFromCOLORREF(0xff3333); // blue
        Gdiplus::Color colHatch2((Gdiplus::ARGB)Gdiplus::Color::Yellow);
        Gdiplus::HatchBrush br(Gdiplus::HatchStyleCross, colHatch2, col);
        Gdiplus::Pen pen(&br, 4);
        gs.DrawRectangle(&pen, rect.x, rect.y, rect.dx, rect.dy);
    } else {
        Gdiplus::Color blue(255, 0, 80, 200);
        Gdiplus::Pen pen(blue, 2);
        pen.SetDashStyle(Gdiplus::DashStyleDot);
        gs.DrawRectangle(&pen, rect.x, rect.y, rect.dx, rect.dy);
    }

    if (!canResize) {
        return;
    }

    // Draw resize handles
    Gdiplus::SolidBrush handleBrush(Gdiplus::Color(255, 255, 255, 255)); // White
    Gdiplus::Pen handlePen(Gdiplus::Color(255, 0, 0, 0), 1);             // Black
    int hs = 6;                                                          // handle size
    int hh = hs / 2;                                                     // half handle

    int left = rect.x - hh;
    int midX = rect.x + rect.dx / 2 - hh;
    int right = rect.x + rect.dx - hh;
    int top = rect.y - hh;
    int midY = rect.y + rect.dy / 2 - hh;
    int bottom = rect.y + rect.dy - hh;

    auto drawHandle = [&](int x, int y) {
        gs.FillRectangle(&handleBrush, x, y, hs, hs);
        gs.DrawRectangle(&handlePen, x, y, hs, hs);
    };

    // corners
    drawHandle(left, top);
    drawHandle(right, top);
    drawHandle(right, bottom);
    drawHandle(left, bottom);
    // edges
    drawHandle(midX, top);
    drawHandle(right, midY);
    drawHandle(midX, bottom);
    drawHandle(left, midY);
}

static bool DrawDocument(MainWindow* win, HDC hdc, RECT* rcArea) {
    ReportIf(!win->AsFixed());
    if (!win->AsFixed()) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    // logf("DrawDocument RenderCache:\n");

    bool isImage = dm->GetEngine()->IsImageCollection();
    // draw comic books and single images on a black background
    // (without frame and shadow)
    bool paintOnBlackWithoutShadow = win->presentation || isImage;
    COLORREF colDocBg;
    COLORREF colDocTxt = ThemeDocumentColors(colDocBg);
    if (isImage) {
        colDocBg = 0x0;
        colDocTxt = 0xffffff;
    }

    bool shouldPaint = false;
    auto* gcols = gGlobalPrefs->fixedPageUI.gradientColors;
    auto nGCols = gcols->size();
    if (paintOnBlackWithoutShadow) {
        AutoDeleteBrush brush = CreateSolidBrush(WIN_COL_BLACK);
        FillRect(hdc, rcArea, brush);
    } else if (0 == nGCols) {
        auto col = ThemeMainWindowBackgroundColor();
        AutoDeleteBrush brush = CreateSolidBrush(col);
        FillRect(hdc, rcArea, brush);
    } else {
        COLORREF colors[3];
        colors[0] = ParseColor(gcols->at(0), WIN_COL_WHITE);
        if (nGCols == 1) {
            colors[1] = colors[2] = colors[0];
        } else if (nGCols == 2) {
            colors[2] = ParseColor(gcols->at(1), WIN_COL_WHITE);
            colors[1] =
                RGB((GetRed(colors[0]) + GetRed(colors[2])) / 2, (GetGreen(colors[0]) + GetGreen(colors[2])) / 2,
                    (GetBlue(colors[0]) + GetBlue(colors[2])) / 2);
        } else {
            colors[1] = ParseColor(gcols->at(1), WIN_COL_WHITE);
            colors[2] = ParseColor(gcols->at(2), WIN_COL_WHITE);
        }
        Size size = dm->GetCanvasSize();
        float percTop = 1.0F * dm->GetViewPort().y / size.dy;
        float percBot = 1.0F * dm->GetViewPort().BR().y / size.dy;
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
        if (percTop < 0.5F) {
            GetGradientColor(col0, col1, 2 * percTop, &tv[0]);
        } else {
            GetGradientColor(col1, col2, 2 * (percTop - 0.5F), &tv[0]);
        }

        if (percBot < 0.5f) {
            GetGradientColor(col0, col1, 2 * percBot, &tv[3]);
        } else {
            GetGradientColor(col1, col2, 2 * (percBot - 0.5F), &tv[3]);
        }

        bool needCenter = percTop < 0.5F && percBot > 0.5F;
        if (needCenter) {
            GetGradientColor(col1, col1, 0, &tv[1]);
            GetGradientColor(col1, col1, 0, &tv[2]);
            tv[1].y = tv[2].y = (LONG)((0.5F - percTop) / (percBot - percTop) * vp.dy);
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

    bool isRtl = IsUIRtl();
    for (int pageNo = 1; pageNo <= dm->PageCount(); ++pageNo) {
        PageInfo* pageInfo = dm->GetPageInfo(pageNo);
        if (!pageInfo || 0.0F == pageInfo->visibleRatio) {
            continue;
        }
        ReportIf(!pageInfo->shown);
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
        int renderDelay = gRenderCache->Paint(hdc, bounds, dm, pageNo, pageInfo, &renderOutOfDateCue);
        if (renderDelay == 0 || renderDelay == RENDER_DELAY_FAILED) {
            shouldPaint = true;
        }
        if (renderDelay != 0) {
            HFONT fontRightTxt = CreateSimpleFont(hdc, "MS Shell Dlg", 14);
            HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt);
            if (renderDelay != RENDER_DELAY_FAILED) {
                if (renderDelay < REPAINT_MESSAGE_DELAY_IN_MS) {
                    ScheduleRepaint(win, REPAINT_MESSAGE_DELAY_IN_MS / 4);
                } else {
                    SetTextColor(hdc, colDocTxt);
                    DrawCenteredText(hdc, bounds, _TRA("Please wait - rendering..."), isRtl);
                }
                rendering = true;
            } else {
#if 0
                AutoDeletePen pen(CreatePen(PS_SOLID, 2, RGB(0xff, 0, 0)));
                ScopedSelectPen restorePen(hdc, pen);
                auto x = bounds.x;
                auto y = bounds.y;
                Rectangle(hdc, x, y, x + bounds.dx, y + bounds.dy);
#endif
                auto prevCol = SetTextColor(hdc, colDocTxt);
                DrawCenteredText(hdc, bounds, _TRA("Couldn't render the page"), isRtl);
                SetTextColor(hdc, prevCol);
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

    WindowTab* tab = win->CurrentTab();
    PaintCurrentEditAnnotationMark(tab, hdc, dm);

    if (win->showSelection) {
        PaintSelection(win, hdc);
    }

    if (win->fwdSearchMark.show) {
        PaintForwardSearchMark(win, hdc);
    }

    if (!rendering) {
        DebugShowLinks(dm, hdc);
    }
    return shouldPaint;
}

static void OnPaintDocument(MainWindow* win) {
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
            bool shouldPaint = DrawDocument(win, win->buffer->GetDC(), &ps.rcPaint);
            if (!gNoFlickerRender || shouldPaint) {
                win->buffer->Flush(hdc);
            }
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

// TODO: this gets called way too often
static LRESULT OnSetCursorMouseNone(MainWindow* win, HWND hwnd) {
    DisplayModel* dm = win->AsFixed();
    Point pt = HwndGetCursorPos(hwnd);
    if (!dm || !GetCursor() || pt.IsEmpty()) {
        win->DeleteToolTip();
        return FALSE;
    }
    if (GetNotificationForGroup(win->hwndCanvas, kNotifCursorPos)) {
        SetCursorCached(IDC_CROSS);
        return TRUE;
    }

    WindowTab* tab = win->CurrentTab();
    Annotation* selected = tab->selectedAnnotation;

    // Check if hovering over resize handle of selected annotation
    if (selected && AnnotationCanBeResized(selected->type)) {
        ResizeHandle handle = GetResizeHandleAt(win, pt, selected);
        if (handle != ResizeHandle::None) {
            SetCursorCached(GetCursorForResizeHandle(handle));
            return TRUE;
        }
    }

    Annotation* annot = dm->GetAnnotationAtPos(pt, selected);
    if (annot && (selected || tab->editAnnotsWindow)) {
        SetCursorCached(IDC_HAND);
        return TRUE;
    }

    int pageNo = 0;
    IPageElement* pageEl = dm->GetElementAtPos(pt, &pageNo);
    if (!pageEl) {
        SetTextOrArrorCursor(dm, pt);
        win->DeleteToolTip();
        return TRUE;
    }
    char* text = pageEl->GetValue();
    if (!dm->ValidPageNo(pageNo)) {
        const char* kind = pageEl->GetKind();
        logf("OnSetCursorMouseIdle: page element '%s' of kind '%s' on invalid page %d\n", text, kind, pageNo);
        ReportIf(true);
        return TRUE;
    }
    auto r = pageEl->GetRect();
    Rect rc = dm->CvtToScreen(pageNo, r);
    win->ShowToolTip(text, rc, true);

    bool isLink = pageEl->Is(kindPageElementDest);

    if (isLink) {
        SetCursorCached(IDC_HAND);
    } else {
        SetTextOrArrorCursor(dm, pt);
    }
    return TRUE;
}

static LRESULT OnSetCursor(MainWindow* win, HWND hwnd) {
    ReportIf(win->hwndCanvas != hwnd);
    if (win->mouseAction != MouseAction::None) {
        win->DeleteToolTip();
    }

    switch (win->mouseAction) {
        case MouseAction::Dragging:
            if (win->annotationBeingResized) {
                SetCursorCached(GetCursorForResizeHandle((ResizeHandle)win->resizeHandle));
            } else {
                SetCursor(gCursorDrag);
            }
            return TRUE;
        case MouseAction::Scrolling:
            SetCursorCached(IDC_SIZEALL);
            return TRUE;
        case MouseAction::SelectingText:
            SetCursorCached(IDC_IBEAM);
            return TRUE;
        case MouseAction::Selecting:
            break;
        case MouseAction::None:
            return OnSetCursorMouseNone(win, hwnd);
    }
    return win->presentation ? TRUE : FALSE;
}

float ScaleZoomBy(MainWindow* win, float factor) {
    auto zoomVirt = win->ctrl->GetZoomVirtual(true);
    return factor * zoomVirt;
}

static bool gWheelZoomRelative = true;

// we guess this is part of continous zoom action if WM_MOUSEWHEEL
bool IsFirstWheelMsg(LARGE_INTEGER& lastTime) {
    auto currTime = TimeGet();
    auto elapsedMs = TimeDiffMs(lastTime, currTime);
    // 150 ms is a heuristic based on looking at logs
    if (elapsedMs < 150.0) {
        // logf("IsFirstWheelMsg: no, elapsed: %.f\n", (float)elapsedMs);
        lastTime = currTime;
        return false;
    }
    // logf("IsFirstWheelMsg: yes, elapsed: %.f\n", (float)elapsedMs);
    lastTime = currTime;
    return true;
}

// this does zooming via mouse wheel (with ctrl or right mouse buttone)
static void ZoomByMouseWheel(MainWindow* win, WPARAM wp) {
    // don't show the context menu when zooming with the right mouse-button down
    win->dragStartPending = false;
    // Kill the smooth scroll timer when zooming
    // We don't want to move to the new updated y offset after zooming
    KillTimer(win->hwndCanvas, kSmoothScrollTimerID);

    short delta = GET_WHEEL_DELTA_WPARAM(wp);
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    float newZoom;
    float factor = 0;
    if (!gWheelZoomRelative) {
        // before 3.6 we were scrolling by steps
        newZoom = win->ctrl->GetNextZoomStep(delta < 0 ? kZoomMin : kZoomMax);
        bool smartZoom = false; // Note: if true will prioritze selection
        SmartZoom(win, newZoom, &pt, smartZoom);
        return;
    }

    static LARGE_INTEGER lastWheelMsgTime{};
    static int accumDelta = 0;
    static float initialZoomVritual = 0;

    if (IsFirstWheelMsg(lastWheelMsgTime)) {
        initialZoomVritual = win->ctrl->GetZoomVirtual(true);
        accumDelta = 0;
    }

    // special case the value coming from pinch gensture on thinkpad touchpad
    // WHEEL_DELTA is 120, which is too fast, so we slow down zooming
    // 10 is heuristic
    if (delta == WHEEL_DELTA) {
        delta = 10;
    } else if (delta == -WHEEL_DELTA) {
        delta = -10;
    }

    accumDelta += delta;
    // calc zooming factor as centered around 1.f (1 is no change, > 1 is zoom in, < 1 is zoom out)
    // from delta values that are centered around 0
    bool negative = accumDelta < 0;

    factor = (float)std::abs(accumDelta) / 100.F;
    factor = 1.F + factor;
    if (negative) {
        factor = 1.F / factor;
    }
    newZoom = initialZoomVritual * factor;
    bool smartZoom = false; // Note: if true will prioritze selection
    SmartZoom(win, newZoom, &pt, smartZoom);

    // logf("delta: %d, accumDelta: %d, factor: %f, newZoom: %f\n", delta, accumDelta, factor, newZoom);
}

static LRESULT CanvasOnMouseWheel(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp) {
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win->tocVisible && IsCursorOverWindow(win->tocTreeView->hwnd) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessageW(win->tocTreeView->hwnd, msg, wp, lp);
        gWheelMsgRedirect = false;
        return res;
    }

    DisplayModel* dm = win->AsFixed();

    // Note: not all mouse drivers correctly report the Ctrl key's state
    // isCtrl is also set if this is pinch gestore from touchpad (on thinkpad x1 at least).
    bool isCtrl = (LOWORD(wp) & MK_CONTROL) || IsCtrlPressed();
    bool isAlt = (LOWORD(wp) & MK_ALT) || IsAltPressed();
    bool isRightButton = (LOWORD(wp) & MK_RBUTTON);
    bool isZooming = isCtrl || isRightButton;
    if (isZooming) {
        ZoomByMouseWheel(win, wp);
        return 0;
    }

    bool hScroll = (LOWORD(wp) & MK_SHIFT) || IsShiftPressed();
    bool vScroll = !hScroll;
    bool isCont = !IsContinuous(win->ctrl->GetDisplayMode());

    // logf("delta: %d, accumDelta: %d, hscroll: %d, continuous: %d, gDeltaPerLine: %d\n", (int)delta,
    // win->wheelAccumDelta,
    //      (int)hScroll, (int)isCont, gDeltaPerLine);

    // Alt speeds up scrolling but also triggers showing menu
    // this will suppress next menu trigger to avoid accidental triggering of menu
    if (isAlt) {
        gSupressNextAltMenuTrigger = true;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wp);

    // fit content: always flip page on wheel, regardless of scrollbar state
    if (vScroll && dm && dm->GetZoomVirtual() == kZoomFitContent && IsSingle(dm->GetDisplayMode())) {
        win->wheelAccumDelta += delta;
        if (win->wheelAccumDelta >= WHEEL_DELTA) {
            win->ctrl->GoToPrevPage();
            win->wheelAccumDelta -= WHEEL_DELTA;
        } else if (win->wheelAccumDelta <= -WHEEL_DELTA) {
            win->ctrl->GoToNextPage();
            win->wheelAccumDelta += WHEEL_DELTA;
        }
        return 0;
    }

    // Handle page-by-page navigation for non-continuous modes and SinglePage mode
    bool isSinglePageMode =
        gGlobalPrefs->scrollbarInSinglePage && (win->ctrl->GetDisplayMode() == DisplayMode::SinglePage);

    // For SinglePage mode with content requiring scrolling, use continuous scrolling behavior
    if (isSinglePageMode && vScroll) {
        if (dm && dm->NeedVScroll()) {
            // Content is larger than viewport, use continuous scrolling
            // Fall through to the default scrolling behavior below
        } else {
            // Content fits in viewport, use page-by-page navigation
            int pageFlipDelta = WHEEL_DELTA; // One wheel click = one page
            win->wheelAccumDelta += delta;
            if (win->wheelAccumDelta >= pageFlipDelta) {
                win->ctrl->GoToPrevPage();
                win->wheelAccumDelta -= pageFlipDelta;
                return 0;
            }
            if (win->wheelAccumDelta <= -pageFlipDelta) {
                win->ctrl->GoToNextPage();
                win->wheelAccumDelta += pageFlipDelta;
                return 0;
            }
            return 0;
        }
    }

    // Handle page-by-page navigation for other non-continuous modes (but not SinglePage mode)
    if (vScroll && !isCont && !isSinglePageMode) {
        float zoomVirt = win->ctrl->GetZoomVirtual();
        // in fit content we might show vert scrollbar but we want to flip the whole page on mouse wheel
        bool flipPage = zoomVirt == kZoomFitContent;
        if (dm && !dm->NeedVScroll()) {
            // if page/pages fully fit in window, flip the whole page
            // logf("  flipping page because !dm->NeedVScroll()\n");
            flipPage = true;
        }
        // fit content/page: one wheel click = one page; otherwise 3 clicks
        int pageFlipDelta = flipPage ? WHEEL_DELTA : WHEEL_DELTA * 3;

        // int scrolLPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        //  Note: pre 3.6 didn't care about horizontallScroll and kZoomFitPage was handled below
        if (flipPage) {
            win->wheelAccumDelta += delta;
            if (win->wheelAccumDelta >= pageFlipDelta) {
                win->ctrl->GoToPrevPage();
                win->wheelAccumDelta -= pageFlipDelta;
                return 0;
            }
            if (win->wheelAccumDelta <= -pageFlipDelta) {
                win->ctrl->GoToNextPage();
                win->wheelAccumDelta += pageFlipDelta;
                return 0;
            }
            return 0;
        }
    }

    if (gDeltaPerLine == 0) {
        return 0;
    }

    // For SinglePage mode with zoomed content, use continuous scrolling with page transitions
    if (isSinglePageMode && vScroll && dm) {
        if (dm->NeedVScroll()) {
            // Use continuous scrolling that handles page transitions at boundaries
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_PAGE;
            GetScrollInfo(win->hwndCanvas, hScroll ? SB_HORZ : SB_VERT, &si);
            int scrollBy = -MulDiv(si.nPage, delta * 30, WHEEL_DELTA);
            // on sensitive touchpads delta can be very small
            if (scrollBy == 0) return 0;
            if (hScroll) {
                dm->ScrollXBy(scrollBy);
            } else {
                dm->ScrollYBy(scrollBy, true);
            }
            return 0;
        }
    }

    if (gDeltaPerLine < 0 && dm) {
        // scroll by (fraction of a) page
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_PAGE;
        GetScrollInfo(win->hwndCanvas, hScroll ? SB_HORZ : SB_VERT, &si);
        int scrollBy = -MulDiv(si.nPage, delta, WHEEL_DELTA);
        // on sensitive touchpads delta can be very small
        if (scrollBy == 0) return 0;
        if (hScroll) {
            dm->ScrollXBy(scrollBy);
        } else {
            dm->ScrollYBy(scrollBy, true);
        }
        return 0;
    }

    // alt while scrolling will scroll by half a page per tick
    // usefull for browsing long files
    if (isAlt) {
        wp = (delta > 0) ? SB_HALF_PAGEUP : SB_HALF_PAGEDOWN;
        SendMessageW(win->hwndCanvas, WM_VSCROLL, wp, 0);
        return 0;
    }

    if (gGlobalPrefs->fastScrollOverScrollbar) {
        // scroll faster if the cursor is over the scroll bar
        if (IsCursorOverWindow(win->hwndCanvas)) {
            Point pt = HwndGetCursorPos(win->hwndCanvas);
            if (pt.x > win->canvasRc.dx) {
                wp = (delta > 0) ? SB_HALF_PAGEUP : SB_HALF_PAGEDOWN;
                SendMessageW(win->hwndCanvas, WM_VSCROLL, wp, 0);
                return 0;
            }
        }
    }

    win->wheelAccumDelta += delta;
    int prevScrollPos = GetScrollPos(win->hwndCanvas, SB_VERT);

    UINT scrollMsg = hScroll ? WM_HSCROLL : WM_VSCROLL;
    bool didScrollByLine = false;
    if (win->wheelAccumDelta < 0) {
        WPARAM scrollWp = hScroll ? SB_LINERIGHT : SB_LINEDOWN;
        while (win->wheelAccumDelta <= -gDeltaPerLine) {
            SendMessageW(win->hwndCanvas, scrollMsg, scrollWp, 0);
            win->wheelAccumDelta += gDeltaPerLine;
            // logf("  line down\n");
            didScrollByLine = true;
        }
    } else {
        WPARAM scrollWp = hScroll ? SB_LINELEFT : SB_LINEUP;
        while (win->wheelAccumDelta >= gDeltaPerLine) {
            SendMessageW(win->hwndCanvas, scrollMsg, scrollWp, 0);
            win->wheelAccumDelta -= gDeltaPerLine;
            // logf("  line up\n");
            didScrollByLine = true;
        }
    }
    // in non-continuous mode flip page if necessary
    if (!vScroll || !isCont) {
        return 0;
    }
    if (!didScrollByLine) {
        // we haven't reached accumulated delta to scroll by line
        return 0;
    }

    int currScrollPos = GetScrollPos(win->hwndCanvas, SB_VERT);
    bool didScroll = (currScrollPos != prevScrollPos);
    if (didScroll) {
        // we don't flip a page if we did scroll by line
        return 0;
    }
    // logf("  flip page: delta: %d, accumDelta: %d\n", (int)delta, (int)win->wheelAccumDelta);
    if (delta > 0) {
        win->ctrl->GoToPrevPage(true);
    } else {
        win->ctrl->GoToNextPage();
    }

    return 0;
}

static LRESULT CanvasOnMouseHWheel(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp) {
    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win->tocVisible && IsCursorOverWindow(win->tocTreeView->hwnd) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEHWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessageW(win->tocTreeView->hwnd, msg, wp, lp);
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

const char* GiFlagsToStr(DWORD flags) {
    switch (flags) {
        case 0:
            return "";
        case GF_BEGIN:
            return "GF_BEGIN";
        case GF_INERTIA:
            return "GF_INERTIA";
        case GF_END:
            return "GF_END";
        case GF_INERTIA | GF_END:
            return "GF_INERTIA  | GF_END";
    }
    return "unknown";
}

static LRESULT OnGesture(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !touch::SupportsGestures()) {
        return DefWindowProc(win->hwndFrame, msg, wp, lp);
    }

    HGESTUREINFO hgi = (HGESTUREINFO)lp;
    GESTUREINFO gi{};
    gi.cbSize = sizeof(GESTUREINFO);
    TouchState& touchState = win->touchState;

    BOOL ok = touch::GetGestureInfo(hgi, &gi);
    if (!ok) {
        touch::CloseGestureInfoHandle(hgi);
        return 0;
    }

    switch (gi.dwID) {
        case GID_ZOOM: {
            auto curr = (float)LowerU64(gi.ullArguments);
            bool isBegin = gi.dwFlags & GF_BEGIN;
            if (!isBegin) {
                auto prev = (float)touchState.zoomIntermediate;
                float factor = curr / prev;
                Point pt{gi.ptsLocation.x, gi.ptsLocation.y};
                HwndScreenToClient(win->hwndCanvas, pt);
                float newZoom = ScaleZoomBy(win, factor);
                SmartZoom(win, newZoom, &pt, false);
            }
            touchState.zoomIntermediate = curr;
            break;
        }

        case GID_PAN:
            // Flicking left or right changes the page,
            // panning moves the document in the scroll window
            if (gi.dwFlags == GF_BEGIN) {
                touchState.panStarted = true;
                touchState.panPos = gi.ptsLocation;
                touchState.panScrollOrigX = GetScrollPos(win->hwndCanvas, SB_HORZ);
                // logf("OnGesture: GID_PAN, GF_BEGIN, scrollX: %d\n", touchState.panScrollOrigX);
            } else if (touchState.panStarted) {
                int deltaX = touchState.panPos.x - gi.ptsLocation.x;
                int deltaY = touchState.panPos.y - gi.ptsLocation.y;
                touchState.panPos = gi.ptsLocation;

                // on left / right flick, go to next / prev page
                // unless we can pan/scroll the document
                bool isFlickX = (gi.dwFlags & GF_INERTIA) && (abs(deltaX) > abs(deltaY)) && (abs(deltaX) > 26);
                // logf("OnGesture: GID_PAN, flags: %d (%s), dx: %d, dy: %d, isFlick: %d\n", gi.dwFlags,
                // GiFlagsToStr(gi.dwFlags), deltaX, deltaY, (int)isFlickX);
                bool flipPage = false;
                if (!dm->NeedHScroll()) {
                    // if the page is fully visible
                    flipPage = true;
                    // logf("flipPage becaues !dm->NeedHScroll()");
                }
                if (deltaX > 0 && !dm->CanScrollRight()) {
                    flipPage = true;
                    // logf("flipPage becaues deltaX > 0 && !dm->CanScrollRight()");
                }
                if (deltaX < 0 && !dm->CanScrollLeft()) {
                    flipPage = true;
                    // logf("flipPage becaues deltaX < 0 && !dm->CanScrollLeft()");
                }

                if (isFlickX && flipPage) {
                    if (deltaX < 0) {
                        win->ctrl->GoToPrevPage();
                        // TODO: scroll to show the right-hand part
                        int x = dm->canvasSize.dx - dm->viewPort.dx;
                        // logf("x: %d\n");
                        dm->ScrollXTo(x);
                    } else if (deltaX > 0) {
                        win->ctrl->GoToNextPage();
                        dm->ScrollXTo(0);
                    }
                    // When we switch pages prevent further pan movement
                    // caused by the inertia
                    touchState.panStarted = false;
                } else {
                    // pan / scroll
                    bool canScrollRightBefore = dm->CanScrollRight();
                    bool canScrollLeftBefore = dm->CanScrollLeft();
                    win->MoveDocBy(deltaX, deltaY);

                    // if pan to the rigth edge, we want to "sticK" to it
                    // and only flip page on the next flick motion
                    bool stopPanning = false;
                    if (canScrollRightBefore != dm->CanScrollRight()) {
                        stopPanning = true;
                        // logf("stopPanning because canScrollRightBefore != dm->CanScrollRight()\n");
                    }
                    if (canScrollLeftBefore != dm->CanScrollLeft()) {
                        stopPanning = true;
                        // logf("stopPanning because canScrollLeftBefore != dm->CanScrollLeft()\n");
                    }
                    if (stopPanning) {
                        touchState.panStarted = false;
                    }
                }
            }
            break;

        case GID_ROTATE:
            // Rotate the PDF 90 degrees in one direction
            if (gi.dwFlags == GF_END && dm) {
                // This is in radians
                double rads = GID_ROTATE_ANGLE_FROM_ARGUMENT(LowerU64(gi.ullArguments));
                // The angle from the rotate is the opposite of the Sumatra rotate, thus the negative
                double degrees = -rads * 180 / M_PI;

                // Playing with the app, I found that I often didn't go quit a full 90 or 180
                // degrees. Allowing rotate without a full finger rotate seemed more natural.
                if (degrees < -120 || degrees > 120) {
                    dm->RotateBy(180);
                } else if (degrees < -45) {
                    dm->RotateBy(-90);
                } else if (degrees > 45) {
                    dm->RotateBy(90);
                }
            }
            break;

        case GID_TWOFINGERTAP:
            // Two-finger tap toggles fullscreen mode
            ToggleFullScreen(win);
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

static LRESULT WndProcCanvasFixedPageUI(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // DbgLogMsg("canvas:", hwnd, msg, wp, lp);

    if (!IsMainWindowValid(win)) {
        bool hwndValid = IsWindow(hwnd);
        logf("WndProcCanvasFixedPageUI: MainWindow win: 0x%p is no longer valid, msg: %d, hwnd valid: %d\n", win,
             (int)msg, (int)hwndValid);
        ReportIfFast(true);
        return 0;
    }

    int x = GET_X_LPARAM(lp);
    int y = GET_Y_LPARAM(lp);
    switch (msg) {
        case WM_PAINT:
            if (gRedrawLog) {
                RECT urc;
                GetUpdateRect(hwnd, &urc, FALSE);
                logf("redraw: WM_PAINT hwnd=0x%p (canvas-fixed) rc=(%d,%d,%d,%d)\n", hwnd, urc.left, urc.top, urc.right,
                     urc.bottom);
            }
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

        case WM_MBUTTONUP:
            OnMouseMiddleButtonUp(win, x, y, wp);
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
            if (x == -1 || y == -1) {
                // if invoked with a keyboard (shift-F10) use current mouse position
                Point pt = HwndGetCursorPos(hwnd);
                x = pt.x;
                y = pt.y;
            }
            // super defensive
            if (x < 0) {
                x = 0;
            }
            if (y < 0) {
                y = 0;
            }
            OnWindowContextMenu(win, x, y);
            return 0;

        case WM_GESTURE:
            return OnGesture(win, msg, wp, lp);

        case WM_NCPAINT: {
            if (gGlobalPrefs->fixedPageUI.hideScrollbars || gGlobalPrefs->fixedPageUI.useOverlayScrollbar) {
                ShowScrollBar(win->hwndCanvas, SB_BOTH, false);
                goto def;
            }

            DisplayModel* dm = win->AsFixed();
            bool isSinglePage =
                gGlobalPrefs->scrollbarInSinglePage && (dm->GetDisplayMode() == DisplayMode::SinglePage);
            bool needH = dm->NeedHScroll();
            bool needV = dm->NeedVScroll() || isSinglePage;
            if (!needH && !needV) {
                ShowScrollBar(win->hwndCanvas, SB_BOTH, false);
                goto def;
            }

            // check whether scrolling is required in the horizontal and/or vertical axes
            int wBar = -1;
            if (needH && needV) {
                wBar = SB_BOTH;
            } else if (needH) {
                wBar = SB_HORZ;
            } else if (needV) {
                wBar = SB_VERT;
            }
            ShowScrollBar(win->hwndCanvas, wBar, true);
            // allow default processing to continue
        }
    }
def:
    return DefWindowProc(hwnd, msg, wp, lp);
}

///// methods needed for ChmUI canvases (should be subclassed by HtmlHwnd) /////

static LRESULT WndProcCanvasChmUI(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SETCURSOR:
            // TODO: make (re)loading a document always clear the infotip
            win->DeleteToolTip();
            return DefWindowProc(hwnd, msg, wp, lp);

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

///// methods needed for FixedPageUI canvases with loading error /////

static void OnPaintError(MainWindow* win) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);

    HFONT fontRightTxt = CreateSimpleFont(hdc, "MS Shell Dlg", 14);
    HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt);
    auto bgCol = ThemeMainWindowBackgroundColor();
    AutoDeleteBrush bgBrush = CreateSolidBrush(bgCol);
    FillRect(hdc, &ps.rcPaint, bgBrush);
    // TODO: should this be "Error opening %s"?
    auto tab = win->CurrentTab();
    const char* filePath = tab->filePath;
    if (filePath) {
        TempStr msg = str::FormatTemp(_TRA("Loading %s ..."), filePath);
        SetTextColor(hdc, ThemeWindowTextColor());
        DrawCenteredText(hdc, ClientRect(win->hwndCanvas), msg, IsUIRtl());
    }
    SelectObject(hdc, hPrevFont);

    EndPaint(win->hwndCanvas, &ps);
}

static LRESULT WndProcCanvasLoadError(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            if (gRedrawLog) {
                logf("redraw: WM_PAINT hwnd=0x%p (canvas-error)\n", hwnd);
            }
            OnPaintError(win);
            return 0;

        case WM_SETCURSOR:
            // TODO: make (re)loading a document always clear the infotip
            win->DeleteToolTip();
            return DefWindowProc(hwnd, msg, wp, lp);

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

///// methods needed for all types of canvas /////

struct RepaintTaskData {
    MainWindow* win = nullptr;
    int delayInMs = 0;
};

static void RepaintTask(RepaintTaskData* d) {
    AutoDelete delData(d);

    auto win = d->win;
    if (!IsMainWindowValid(win)) {
        return;
    }
    if (!d->delayInMs) {
        WndProcCanvas(win->hwndCanvas, WM_TIMER, REPAINT_TIMER_ID, 0);
    } else if (!win->delayedRepaintTimer) {
        win->delayedRepaintTimer = SetTimer(win->hwndCanvas, REPAINT_TIMER_ID, (uint)d->delayInMs, nullptr);
    }
}

void ScheduleRepaint(MainWindow* win, int delayInMs) {
    if (gRedrawLog) {
        logf("redraw: ScheduleRepaint delayMs=%d canvas=0x%p\n", delayInMs, win->hwndCanvas);
    }
    auto data = new RepaintTaskData;
    data->win = win;
    data->delayInMs = delayInMs;
    auto fn = MkFunc0<RepaintTaskData>(RepaintTask, data);
    // even though RepaintAsync is mostly called from the UI thread,
    // we depend on the repaint message to happen asynchronously
    uitask::Post(fn, nullptr);
}

static void OnTimer(MainWindow* win, HWND hwnd, WPARAM timerId) {
    Point pt;

    if (!win || !IsMainWindowValid(win) || win->isBeingClosed) {
        return;
    }

    switch (timerId) {
        case REPAINT_TIMER_ID:
            win->delayedRepaintTimer = 0;
            KillTimer(hwnd, REPAINT_TIMER_ID);
            win->RedrawAllIncludingNonClient();
            break;

        case SMOOTHSCROLL_TIMER_ID:
            if (MouseAction::Scrolling == win->mouseAction) {
                win->MoveDocBy(win->xScrollSpeed, win->yScrollSpeed);
            } else if (MouseAction::Selecting == win->mouseAction || MouseAction::SelectingText == win->mouseAction) {
                pt = HwndGetCursorPos(win->hwndCanvas);
                if (NeedsSelectionEdgeAutoscroll(win, pt.x, pt.y)) {
                    OnMouseMove(win, pt.x, pt.y, MK_CONTROL);
                }
            } else {
                KillTimer(hwnd, SMOOTHSCROLL_TIMER_ID);
                win->yScrollSpeed = 0;
                win->xScrollSpeed = 0;
            }
            break;

        case kHideCursorTimerID:
            // logf("got kHideCursorTimerID\n");
            KillTimer(hwnd, kHideCursorTimerID);
            if (win->InPresentation()) {
                // logf("hiding cursor because win->presentations\n");
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
                ScheduleRepaint(win, 0);
            } else {
                ScheduleRepaint(win, 0);
            }
            break;

        case AUTO_RELOAD_TIMER_ID: {
            KillTimer(hwnd, AUTO_RELOAD_TIMER_ID);
            auto tab = win->CurrentTab();
            if (tab && tab->reloadOnFocus) {
                if (tab->ignoreNextAutoReload) {
                    tab->ignoreNextAutoReload = false;
                } else {
                    ReloadDocument(win, true);
                }
            }
            break;
        }

        case kSmoothScrollTimerID:
            DisplayModel* dm = win->AsFixed();
            // window might have been closed while the timer was running
            if (!dm) {
                return;
            }

            int current = dm->yOffset();
            int target = win->scrollTargetY;
            int delta = target - current;

            if (delta == 0) {
                KillTimer(hwnd, kSmoothScrollTimerID);
            } else {
                // logf("Smooth scrolling from %d to %d (delta %d)\n", current, target, delta);

                double step = delta * gSmoothScrollingFactor;

                // Round away from zero
                int dy = step < 0 ? (int)floor(step) : (int)ceil(step);
                dm->ScrollYTo(current + dy);
            }
            break;
    }
}

static void GetDropFilesResolved(HDROP hDrop, bool dragFinish, StrVec& files) {
    int nFiles = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, nullptr, 0);
    WCHAR pathW[MAX_PATH]{};
    char* path = nullptr;
    for (int i = 0; i < nFiles; i++) {
        DragQueryFile(hDrop, i, pathW, dimof(pathW));
        path = ToUtf8Temp(pathW);
        if (str::EndsWithI(path, ".lnk")) {
            char* resolved = ResolveLnkTemp(path);
            if (resolved) {
                path = resolved;
            }
        }
        files.Append(path);
    }
    if (dragFinish) {
        DragFinish(hDrop);
    }
}

static void OnDropFiles(MainWindow* win, HDROP hDrop, bool dragFinish) {
    StrVec files;
    bool isShift = IsShiftPressed();

    GetDropFilesResolved(hDrop, dragFinish, files);
    for (char* path : files) {
        // The first dropped document may override the current window
        LoadArgs args(path, win);
        if (isShift && !win) {
            win = CreateAndShowMainWindow(nullptr);
            args.win = win;
        }
        StartLoadDocument(&args);
    }
}

// returns true if url looks like it could be an image URL
static bool IsImageUrl(const char* url) {
    // strip query string / fragment for extension check
    const char* q = str::FindChar(url, '?');
    const char* h = str::FindChar(url, '#');
    int len = str::Leni(url);
    if (q && (int)(q - url) < len) {
        len = (int)(q - url);
    }
    if (h && (int)(h - url) < len) {
        len = (int)(h - url);
    }
    // check for common image extensions
    const char* exts[] = {".png",  ".jpg",  ".jpeg", ".gif", ".bmp", ".tiff", ".tif",
                          ".webp", ".avif", ".heic", ".jxr", ".jp2", ".tga"};
    for (auto ext : exts) {
        int extLen = str::Leni(ext);
        if (len >= extLen) {
            TempStr ending = str::DupTemp(url + len - extLen, extLen);
            if (str::EqI(ending, ext)) {
                return true;
            }
        }
    }
    return false;
}

// Get the user's Downloads folder path
static TempStr GetDownloadsDirTemp() {
    WCHAR* pathW = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &pathW);
    if (FAILED(hr) || !pathW) {
        CoTaskMemFree(pathW);
        return nullptr;
    }
    TempStr res = ToUtf8Temp(pathW);
    CoTaskMemFree(pathW);
    return res;
}

// Extract a file name from a URL (last path component, without query/fragment)
static TempStr FileNameFromUrlTemp(const char* url) {
    // skip past scheme
    const char* s = str::FindChar(url, '/');
    if (s && s[1] == '/') {
        s += 2; // skip "//"
    }
    // find last '/' before any '?' or '#'
    const char* lastSlash = nullptr;
    const char* p = s ? s : url;
    while (*p && *p != '?' && *p != '#') {
        if (*p == '/') {
            lastSlash = p;
        }
        p++;
    }
    if (!lastSlash) {
        return nullptr;
    }
    int nameLen = (int)(p - lastSlash - 1);
    if (nameLen <= 0) {
        return nullptr;
    }
    return str::DupTemp(lastSlash + 1, nameLen);
}

struct DownloadAndOpenUrlData {
    char* url;
    HWND hwndCanvas;
};

static void DownloadAndOpenUrl(DownloadAndOpenUrlData* data) {
    TempStr url = data->url;
    HWND hwndCanvas = data->hwndCanvas;

    TempStr downloadsDir = GetDownloadsDirTemp();
    if (!downloadsDir) {
        logf("DownloadAndOpenUrl: failed to get Downloads folder\n");
        free(data->url);
        delete data;
        return;
    }

    TempStr fileName = FileNameFromUrlTemp(url);
    if (!fileName) {
        // generate a fallback name
        fileName = str::DupTemp("dropped_image.png");
    }

    TempStr destPath = path::JoinTemp(downloadsDir, fileName);

    // avoid overwriting: if file exists, add a numeric suffix
    if (file::Exists(destPath)) {
        TempStr ext = path::GetExtTemp(destPath);
        TempStr base = str::DupTemp(fileName, str::Leni(fileName) - str::Leni(ext));
        for (int i = 1; i < 1000; i++) {
            TempStr newName = str::FormatTemp("%s_%d%s", base, i, ext);
            destPath = path::JoinTemp(downloadsDir, newName);
            if (!file::Exists(destPath)) {
                break;
            }
        }
    }

    logf("DownloadAndOpenUrl: downloading '%s' to '%s'\n", url, destPath);

    Func1<HttpProgress*> emptyProgress;
    bool ok = HttpGetToFile(url, destPath, emptyProgress);
    if (!ok) {
        logf("DownloadAndOpenUrl: download failed for '%s'\n", url);
        free(data->url);
        delete data;
        return;
    }

    // verify the downloaded file is a supported image type
    Kind kind = GuessFileTypeFromContent(destPath);
    if (!IsEngineImageSupportedFileType(kind)) {
        logf("DownloadAndOpenUrl: downloaded file is not a supported image type: '%s'\n", destPath);
        file::Delete(destPath);
        free(data->url);
        delete data;
        return;
    }

    // ensure it has a good extension, some urls are like:
    // https://pbs.twimg.com/media/HEwit7bbQAAWiIO?format=jpg&name=large
    const char* ext = GetExtForKind(kind);
    if (!str::EndsWithI(destPath, ext)) {
        TempStr newDest = str::JoinTemp(destPath, ext);
        ok = file::Rename(newDest, destPath);
        if (ok) {
            destPath = newDest;
        }
    }

    // open the file on the UI thread
    char* pathDup = str::Dup(destPath);
    auto fn = MkFunc0<char>(
        [](char* path) {
            MainWindow* win = FindMainWindowByHwnd(GetForegroundWindow());
            if (!win && !gWindows.IsEmpty()) {
                win = gWindows.at(0);
            }
            if (win) {
                LoadArgs args(path, win);
                StartLoadDocument(&args);
            }
            free(path);
        },
        pathDup);
    uitask::Post(fn, "DownloadAndOpenUrl");

    free(data->url);
    delete data;
}

// Extract text from IDataObject (tries CF_UNICODETEXT, then CF_TEXT)
static TempStr GetTextFromDataObject(IDataObject* dataObj) {
    FORMATETC fmtUnicode = {CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    FORMATETC fmtAnsi = {CF_TEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    HRESULT hr = dataObj->GetData(&fmtUnicode, &medium);
    TempStr res;
    if (SUCCEEDED(hr) && medium.hGlobal) {
        WCHAR* w = (WCHAR*)GlobalLock(medium.hGlobal);
        res = w ? ToUtf8Temp(w) : nullptr;
        goto Cleanup;
    }
    hr = dataObj->GetData(&fmtAnsi, &medium);
    if (SUCCEEDED(hr) && medium.hGlobal) {
        char* s = (char*)GlobalLock(medium.hGlobal);
        res = s ? str::DupTemp(s) : nullptr;
        goto Cleanup;
    }
    return nullptr;
Cleanup:
    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);
    return res;
}

// Check if IDataObject contains a URL (registered format "UniformResourceLocatorW" or "UniformResourceLocator")
static TempStr GetUrlFromDataObject(IDataObject* dataObj) {
    // try wide URL format first
    static CLIPFORMAT cfUrlW = (CLIPFORMAT)RegisterClipboardFormatW(L"UniformResourceLocatorW");
    if (cfUrlW) {
        FORMATETC fmt = {cfUrlW, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmt, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            WCHAR* w = (WCHAR*)GlobalLock(medium.hGlobal);
            TempStr res = w ? ToUtf8Temp(w) : nullptr;
            GlobalUnlock(medium.hGlobal);
            ReleaseStgMedium(&medium);
            if (res && (str::StartsWithI(res, "http://") || str::StartsWithI(res, "https://"))) {
                return res;
            }
        }
    }
    // try ANSI URL format
    static CLIPFORMAT cfUrl = (CLIPFORMAT)RegisterClipboardFormatW(L"UniformResourceLocator");
    if (cfUrl) {
        FORMATETC fmt = {cfUrl, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmt, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            char* s = (char*)GlobalLock(medium.hGlobal);
            TempStr res = s ? str::DupTemp(s) : nullptr;
            GlobalUnlock(medium.hGlobal);
            ReleaseStgMedium(&medium);
            if (res && (str::StartsWithI(res, "http://") || str::StartsWithI(res, "https://"))) {
                return res;
            }
        }
    }
    return nullptr;
}

static bool DataObjectHasFiles(IDataObject* dataObj) {
    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return dataObj->QueryGetData(&fmt) == S_OK;
}

static bool DataObjectHasUrl(IDataObject* dataObj) {
    TempStr url = GetUrlFromDataObject(dataObj);
    if (url && IsImageUrl(url)) {
        return true;
    }
    // also check plain text that looks like an image URL
    TempStr text = GetTextFromDataObject(dataObj);
    if (text && (str::StartsWithI(text, "http://") || str::StartsWithI(text, "https://")) && IsImageUrl(text)) {
        return true;
    }
    return false;
}

class CanvasDropTarget : public IDropTarget {
    LONG refCount = 1;
    HWND hwnd = nullptr;

  public:
    explicit CanvasDropTarget(HWND h) : hwnd(h) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }

    STDMETHODIMP DragEnter(IDataObject* dataObj, __unused DWORD grfKeyState, __unused POINTL pt,
                           DWORD* pdwEffect) override {
        if (DataObjectHasFiles(dataObj) || DataObjectHasUrl(dataObj)) {
            *pdwEffect = DROPEFFECT_COPY;
        } else {
            *pdwEffect = DROPEFFECT_NONE;
        }
        return S_OK;
    }

    STDMETHODIMP DragOver(__unused DWORD grfKeyState, __unused POINTL pt, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    STDMETHODIMP DragLeave() override { return S_OK; }

    STDMETHODIMP Drop(IDataObject* dataObj, DWORD grfKeyState, __unused POINTL pt, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_COPY;

        // first try file drops (CF_HDROP)
        FORMATETC fmtHDrop = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmtHDrop, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            HDROP hDrop = (HDROP)medium.hGlobal;
            MainWindow* win = FindMainWindowByHwnd(hwnd);
            if (win) {
                OnDropFiles(win, hDrop, false);
            }
            ReleaseStgMedium(&medium);
            return S_OK;
        }

        // try URL drop
        TempStr url = GetUrlFromDataObject(dataObj);
        if (!url) {
            // fall back to plain text
            TempStr text = GetTextFromDataObject(dataObj);
            if (text && (str::StartsWithI(text, "http://") || str::StartsWithI(text, "https://"))) {
                url = text;
            }
        }

        if (url) {
            auto data = new DownloadAndOpenUrlData();
            data->url = str::Dup(url);
            data->hwndCanvas = hwnd;
            auto fn = MkFunc0<DownloadAndOpenUrlData>([](DownloadAndOpenUrlData* d) { DownloadAndOpenUrl(d); }, data);
            RunAsync(fn, "DownloadAndOpenUrl");
        }

        return S_OK;
    }
};

void RegisterCanvasDropTarget(HWND hwndCanvas) {
    auto* dt = new CanvasDropTarget(hwndCanvas);
    RegisterDragDrop(hwndCanvas, dt);
    dt->Release(); // RegisterDragDrop AddRef'd it
}

void RevokeCanvasDropTarget(HWND hwndCanvas) {
    RevokeDragDrop(hwndCanvas);
}

LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // messages that don't require win

    MainWindow* win = FindMainWindowByHwnd(hwnd);
    switch (msg) {
        case WM_DROPFILES:
            ReportIf(lp != 0 && lp != 1);
            OnDropFiles(win, (HDROP)wp, !lp);
            return 0;

            // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-erasebkgnd
        case WM_ERASEBKGND: {
            if (gRedrawLog) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                logf("redraw: WM_ERASEBKGND hwnd=0x%p (canvas) rc=(%d,%d,%d,%d)\n", hwnd, rc.left, rc.top, rc.right,
                     rc.bottom);
            }
            // don't paint here; old content stays until WM_PAINT covers it
            // (CS_HREDRAW|CS_VREDRAW removed so no transparent flash)
            return 1;
        }

        case WM_NCHITTEST: {
            // return HTTRANSPARENT near frame edges so the parent frame
            // can handle resize hit-testing beyond kFrameBorderSize
            if (win && win->tabsInTitlebar && !IsZoomed(GetParent(hwnd))) {
                int x = GET_X_LPARAM(lp);
                int y = GET_Y_LPARAM(lp);
                RECT wrc;
                GetWindowRect(GetParent(hwnd), &wrc);
                int b = kFrameResizeHitTest;
                if ((x - wrc.left) < b || (wrc.right - x) <= b || (y - wrc.top) < b || (wrc.bottom - y) <= b) {
                    return HTTRANSPARENT;
                }
            }
            break;
        }
    }

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
                if (gRedrawLog) {
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    logf("redraw: WM_SIZE hwnd=0x%p (canvas) size=(%d,%d)\n", hwnd, rc.right, rc.bottom);
                }
                win->UpdateCanvasSize();
                // fully invalidate since layout depends on size
                // (replaces CS_HREDRAW | CS_VREDRAW which caused transparent flash)
                InvalidateRect(hwnd, nullptr, FALSE);
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
            // currently win->uiaProvider refCount is really out of wack in MainWindow::~MainWindow
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

            if (win->IsCurrentTabAbout()) {
                return WndProcCanvasAbout(win, hwnd, msg, wp, lp);
            }

            return WndProcCanvasLoadError(win, hwnd, msg, wp, lp);
    }
}

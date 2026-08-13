/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "gui/Dpi.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHoverInternal.h"

static int gClassRegistered = 0;

static bool PopupClientToPagePt(RefHoverState* s, HWND hwnd, int clientX, int clientY, PointF& ptOut) {
    if (!s || !s->hitEngine || s->displayed.destPage <= 0) {
        return false;
    }
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f) {
        return false;
    }
    int border = DpiScale(kRefHoverBorder);
    // When a column-wrap continuation is stitched below displayed.region in
    // the bitmap (see RefHoverRender.cpp's StackPixmapsVertically), a click
    // there falls outside what displayed.region maps to — the formula below
    // would silently produce a page point in the wrong place. Reject clicks
    // past the primary crop's rendered height rather than mis-hit-test.
    float regionPixH = s->displayed.region.dy * zoom;
    if ((float)(clientY - border) > regionPixH) {
        return false;
    }
    ptOut.x = s->displayed.region.x + ((float)(clientX - border) / zoom);
    ptOut.y = s->displayed.region.y + ((float)(clientY - border) / zoom);
    return true;
}

static IPageDestination* LaunchLinkAtPagePt(RefHoverState* s, PointF pagePt) {
    if (!s || !s->hitEngine || s->displayed.destPage <= 0) {
        return nullptr;
    }
    IPageElement* el = s->hitEngine->GetElementAtPos(s->displayed.destPage, pagePt);
    if (!el || !el->Is(kindPageElementDest)) {
        return nullptr;
    }
    IPageDestination* dest = el->AsLink();
    if (!RefHoverIsLaunchLink(dest)) {
        return nullptr;
    }
    return dest;
}

static IPageDestination* LaunchLinkAtPopupPt(RefHoverState* s, HWND hwnd, int clientX, int clientY) {
    PointF pagePt;
    if (!PopupClientToPagePt(s, hwnd, clientX, clientY, pagePt)) {
        return nullptr;
    }
    return LaunchLinkAtPagePt(s, pagePt);
}

static LRESULT CALLBACK RefHoverWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SETCURSOR) {
        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        POINT p;
        if (s && GetCursorPos(&p)) {
            ScreenToClient(hwnd, &p);
            if (LaunchLinkAtPopupPt(s, hwnd, p.x, p.y)) {
                SetCursorCached(IDC_HAND);
                return TRUE;
            }
        }
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HBRUSH hbg = CreateSolidBrush(MkRgb(255, 252, 200));
        HdcFillRect(hdc, HwndClientRect(hwnd), hbg);
        DeleteObject(hbg);

        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (s && s->bmp) {
            Size bmpSize = Size(s->bmp->width, s->bmp->height);
            int border = DpiScale(kRefHoverBorder);
            HDC bmpDC = CreateCompatibleDC(hdc);
            HGDIOBJ oldBmp = bmpDC ? SelectObject(bmpDC, s->bmp->hbmp) : nullptr;
            if (oldBmp) {
                BitBlt(hdc, border, border, bmpSize.dx, bmpSize.dy, bmpDC, 0, 0, SRCCOPY);
                SelectObject(bmpDC, oldBmp);
            }
            if (bmpDC) {
                DeleteDC(bmpDC);
            }
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_LBUTTONDOWN) {
        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (s) {
            int cx = GET_X_LPARAM(lp);
            int cy = GET_Y_LPARAM(lp);
            IPageDestination* dest = LaunchLinkAtPopupPt(s, hwnd, cx, cy);
            if (dest) {
                RefHoverHandlePopupClick(s, dest);
            }
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static bool RegisterClassIfNeeded() {
    if (gClassRegistered != 0) {
        return gClassRegistered > 0;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = RefHoverWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REF_HOVER_CLASS;
    wc.hCursor = GetCachedCursor(IDC_ARROW);
    ATOM atom = RegisterClassW(&wc);
    gClassRegistered = atom ? 1 : -1;
    return gClassRegistered > 0;
}

bool RefHoverPopupCreate(RefHoverState* s, HWND hwndCanvas) {
    if (!RegisterClassIfNeeded()) {
        return false;
    }
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, REF_HOVER_CLASS, nullptr, WS_POPUP | WS_BORDER, 0, 0, 10, 10,
                                hwndCanvas, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
    s->hwndPopup = hwnd;
    s->hwndCanvas = hwndCanvas;
    return true;
}

void RefHoverShowPopup(RefHoverState* s, Point screenPt) {
    if (!s || !s->hwndPopup || !s->bmp) {
        return;
    }
    Size bmpSize = Size(s->bmp->width, s->bmp->height);
    int border = DpiScale(kRefHoverBorder);
    int popupW = bmpSize.dx + (2 * border);
    int popupH = bmpSize.dy + (2 * border);

    HMONITOR hmon = MonitorFromPoint({screenPt.x, screenPt.y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hmon, &mi);

    int leftBound = mi.rcWork.left;
    int rightBound = mi.rcWork.right;
    int topBound = mi.rcWork.top;
    int bottomBound = mi.rcWork.bottom;
    Rect pr = s->pending.pageScreenRect;
    if (pr.dy > 0) {
        topBound = std::max(pr.y, topBound);
        bottomBound = std::min(pr.y + pr.dy, bottomBound);
    }
    int boundW = rightBound - leftBound;
    int boundH = bottomBound - topBound;
    popupW = std::min(popupW, boundW);
    popupH = std::min(popupH, boundH);

    int pageCenterX = (pr.dx > 0) ? (pr.x + (pr.dx / 2)) : screenPt.x;
    int anchorX = pageCenterX;
    if (pr.dx > 0) {
        int colWidth = pr.dx / 2;
        if (popupW <= colWidth) {
            anchorX = (screenPt.x >= pageCenterX) ? (pr.x + (pr.dx * 3 / 4)) : (pr.x + (pr.dx / 4));
        }
    }
    int x = anchorX - (popupW / 2);
    int cursorPad = DpiScale(kRefHoverCursorPad);
    int spaceBelow = bottomBound - (screenPt.y + cursorPad);
    int spaceAbove = (screenPt.y - cursorPad) - topBound;
    int y;
    if (spaceBelow >= popupH) {
        y = screenPt.y + cursorPad;
    } else if (spaceAbove >= popupH) {
        y = screenPt.y - popupH - cursorPad;
    } else if (spaceBelow >= spaceAbove) {
        if (spaceBelow > 0) {
            popupH = spaceBelow;
        }
        y = screenPt.y + cursorPad;
    } else {
        if (spaceAbove > 0) {
            popupH = spaceAbove;
        }
        y = screenPt.y - popupH - cursorPad;
    }
    x = std::max(x, leftBound);
    if (x + popupW > rightBound) {
        x = rightBound - popupW;
    }
    y = std::max(y, topBound);
    if (y + popupH > bottomBound) {
        popupH = bottomBound - y;
    }

    if (popupW <= 0 || popupH <= 0) {
        ShowWindow(s->hwndPopup, SW_HIDE);
        return;
    }

    SetWindowPos(s->hwndPopup, HWND_TOPMOST, x, y, popupW, popupH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    HwndInvalidate(s->hwndPopup, true);
}

bool RefHoverRerenderDisplayedRegion(RefHoverState* s, EngineBase* engine, int page, RectF region) {
    if (!s || !engine || page <= 0) {
        return false;
    }
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f) {
        return false;
    }
    s->displayed.destPage = page;
    s->displayed.region = region;
    RefHoverState::RenderRequest req;
    req.pageNo = page;
    req.zoom = zoom;
    req.region = region;
    RefHoverRequestRender(s, engine, req);
    return true;
}

// Re-render the popup at adjusted zoom in response to a mouse-wheel event.
// Popup window keeps its initial size; only the rendered content scales.
// Positive delta zooms in, negative zooms out. Returns true if the zoom
// changed and a re-render happened.
bool RefHoverWheelZoom(RefHoverState* s, EngineBase* engine, int wheelDelta) {
    if (!s || !s->hwndPopup || s->displayed.destPage <= 0 || !engine) {
        return false;
    }
    float factor = (wheelDelta > 0) ? kRefHoverUserZoomStep : (1.f / kRefHoverUserZoomStep);
    float newZoom = s->displayed.userZoom * factor;
    if (newZoom < kRefHoverMinUserZoom) {
        newZoom = kRefHoverMinUserZoom;
    } else if (newZoom > kRefHoverMaxUserZoom) {
        newZoom = kRefHoverMaxUserZoom;
    }
    if (newZoom == s->displayed.userZoom) {
        return false;
    }
    s->displayed.userZoom = newZoom;

    Rect rc = HwndClientRect(s->hwndPopup);
    int border = DpiScale(kRefHoverBorder);
    float clientW = (float)(rc.dx - (2 * border));
    float clientH = (float)(rc.dy - (2 * border));
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f || clientW <= 0.f || clientH <= 0.f) {
        return false;
    }

    RectF mediabox = engine->PageMediabox(s->displayed.destPage);
    RectF region = s->displayed.region;
    region.dx = clientW / zoom;
    region.dy = clientH / zoom;
    if (region.x + region.dx > mediabox.dx) {
        region.dx = mediabox.dx - region.x;
    }
    if (region.y + region.dy > mediabox.dy) {
        region.dy = mediabox.dy - region.y;
    }
    if (region.dx <= 0.f || region.dy <= 0.f) {
        return false;
    }

    return RefHoverRerenderDisplayedRegion(s, engine, s->displayed.destPage, region);
}

// Scroll the popup's rendered region by a wheel notch. Positive delta scrolls
// toward earlier content (up); negative scrolls toward later content (down).
// Rolls over to the previous / next page when the viewport hits a page edge
// (continuous scrolling). Popup window keeps its initial size; only the
// rendered region's Y (and possibly page number) changes.
bool RefHoverWheelScroll(RefHoverState* s, EngineBase* engine, int wheelDelta) {
    if (!s || !s->hwndPopup || s->displayed.destPage <= 0 || !engine) {
        return false;
    }
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f) {
        return false;
    }
    int pageCount = engine->PageCount();
    int page = s->displayed.destPage;
    RectF region = s->displayed.region;
    RectF mediabox = engine->PageMediabox(page);
    if (mediabox.dx <= 0.f || mediabox.dy <= 0.f) {
        return false;
    }

    float scrollStep = (float)DpiScale(kRefHoverScrollStepPx);
    float scrollPt = scrollStep * ((float)wheelDelta / (float)WHEEL_DELTA) / zoom;
    float newY = region.y - scrollPt;

    if (newY < 0.f) {
        if (page > 1) {
            float overflow = -newY;
            page--;
            mediabox = engine->PageMediabox(page);
            newY = mediabox.dy - region.dy - overflow;
            newY = std::max(newY, 0.f);
        } else {
            newY = 0.f;
        }
    } else if (newY + region.dy > mediabox.dy) {
        if (page < pageCount) {
            float overflow = (newY + region.dy) - mediabox.dy;
            page++;
            mediabox = engine->PageMediabox(page);
            newY = overflow;
            if (newY + region.dy > mediabox.dy) {
                newY = mediabox.dy - region.dy;
            }
            newY = std::max(newY, 0.f);
        } else {
            newY = mediabox.dy - region.dy;
            newY = std::max(newY, 0.f);
        }
    }

    if (page == s->displayed.destPage && newY == region.y) {
        return false;
    }
    region.y = newY;
    region.dy = std::min(region.dy, mediabox.dy);
    if (region.x + region.dx > mediabox.dx) {
        region.x = mediabox.dx - region.dx;
        if (region.x < 0.f) {
            region.x = 0.f;
            region.dx = mediabox.dx;
        }
    }

    return RefHoverRerenderDisplayedRegion(s, engine, page, region);
}

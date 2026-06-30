/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Dpi.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

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
    int border = DpiScale(hwnd, kRefHoverBorder);
    ptOut.x = s->displayed.region.x + (float)(clientX - border) / zoom;
    ptOut.y = s->displayed.region.y + (float)(clientY - border) / zoom;
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
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH hbg = CreateSolidBrush(RGB(255, 252, 200));
        FillRect(hdc, &rc, hbg);
        DeleteObject(hbg);

        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (s && s->bmp) {
            Size bmpSize = Size(s->bmp->width, s->bmp->height);
            int border = DpiScale(hwnd, kRefHoverBorder);
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
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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
    int border = DpiScale(s->hwndPopup, kRefHoverBorder);
    int popupW = bmpSize.dx + 2 * border;
    int popupH = bmpSize.dy + 2 * border;

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
        if (pr.y > topBound) {
            topBound = pr.y;
        }
        if (pr.y + pr.dy < bottomBound) {
            bottomBound = pr.y + pr.dy;
        }
    }
    int boundW = rightBound - leftBound;
    int boundH = bottomBound - topBound;
    if (popupW > boundW) {
        popupW = boundW;
    }
    if (popupH > boundH) {
        popupH = boundH;
    }

    int pageCenterX = (pr.dx > 0) ? (pr.x + pr.dx / 2) : screenPt.x;
    int anchorX = pageCenterX;
    if (pr.dx > 0) {
        int colWidth = pr.dx / 2;
        if (popupW <= colWidth) {
            anchorX = (screenPt.x >= pageCenterX) ? (pr.x + pr.dx * 3 / 4) : (pr.x + pr.dx / 4);
        }
    }
    int x = anchorX - popupW / 2;
    int cursorPad = DpiScale(s->hwndPopup, kRefHoverCursorPad);
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
    if (x < leftBound) {
        x = leftBound;
    }
    if (x + popupW > rightBound) {
        x = rightBound - popupW;
    }
    if (y < topBound) {
        y = topBound;
    }
    if (y + popupH > bottomBound) {
        popupH = bottomBound - y;
    }

    if (popupW <= 0 || popupH <= 0) {
        ShowWindow(s->hwndPopup, SW_HIDE);
        return;
    }

    SetWindowPos(s->hwndPopup, HWND_TOPMOST, x, y, popupW, popupH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(s->hwndPopup, nullptr, TRUE);
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

    RECT rc;
    GetClientRect(s->hwndPopup, &rc);
    int border = DpiScale(s->hwndPopup, kRefHoverBorder);
    float clientW = (float)((rc.right - rc.left) - 2 * border);
    float clientH = (float)((rc.bottom - rc.top) - 2 * border);
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

    float scrollStep = (float)DpiScale(s->hwndPopup, kRefHoverScrollStepPx);
    float scrollPt = scrollStep * ((float)wheelDelta / (float)WHEEL_DELTA) / zoom;
    float newY = region.y - scrollPt;

    if (newY < 0.f) {
        if (page > 1) {
            float overflow = -newY;
            page--;
            mediabox = engine->PageMediabox(page);
            newY = mediabox.dy - region.dy - overflow;
            if (newY < 0.f) {
                newY = 0.f;
            }
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
            if (newY < 0.f) {
                newY = 0.f;
            }
        } else {
            newY = mediabox.dy - region.dy;
            if (newY < 0.f) {
                newY = 0.f;
            }
        }
    }

    if (page == s->displayed.destPage && newY == region.y) {
        return false;
    }
    region.y = newY;
    if (region.dy > mediabox.dy) {
        region.dy = mediabox.dy;
    }
    if (region.x + region.dx > mediabox.dx) {
        region.x = mediabox.dx - region.dx;
        if (region.x < 0.f) {
            region.x = 0.f;
            region.dx = mediabox.dx;
        }
    }

    return RefHoverRerenderDisplayedRegion(s, engine, page, region);
}
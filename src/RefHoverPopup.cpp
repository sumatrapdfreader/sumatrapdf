/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "gui/Dpi.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"
#include "RefHoverPopupCommon.h"
#include "Theme.h"

static int gClassRegistered = 0;

static LRESULT CALLBACK RefHoverWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SETCURSOR) {
        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        POINT p;
        if (s && GetCursorPos(&p)) {
            ScreenToClient(hwnd, &p);
            if (LaunchLinkAtPopupPt(s, p.x, p.y)) {
                SetCursorCached(IDC_HAND);
                return TRUE;
            }
        }
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        int border = DpiScale(kRefHoverBorder);

        // themed edge, then the frame inside it (WS_BORDER would be system gray)
        // clipped to outside the bitmap: painting it under the bitmap flickers (#6428)
        int savedDC = SaveDC(hdc);
        if (s && s->bmp) {
            ExcludeClipRect(hdc, border, border, border + s->bmp->width, border + s->bmp->height);
        }
        Rect rc = HwndClientRect(hwnd);
        int edge = DpiScale(1);
        HdcFillRect(hdc, rc, ThemeHoverPreviewEdgeColor());
        Rect inner = {rc.x + edge, rc.y + edge, rc.dx - (2 * edge), rc.dy - (2 * edge)};
        HdcFillRect(hdc, inner, ThemeHoverPreviewBackgroundColor());
        RestoreDC(hdc, savedDC);

        if (s && s->bmp) {
            Size bmpSize = Size(s->bmp->width, s->bmp->height);
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
    // the cursor moved onto the popup: the wheel is for it, no modifier needed
    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (s && s->hitEngine) {
            RefHoverOnWheel(s, s->hitEngine, msg, wp, lp, RefHoverWheelAt::Popup);
        }
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (s) {
            int cx = GET_X_LPARAM(lp);
            int cy = GET_Y_LPARAM(lp);
            IPageDestination* dest = LaunchLinkAtPopupPt(s, cx, cy);
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
    wc.lpszClassName = kRefHoverClass;
    wc.hCursor = GetCachedCursor(IDC_ARROW);
    ATOM atom = RegisterClassW(&wc);
    gClassRegistered = atom ? 1 : -1;
    return gClassRegistered > 0;
}

bool RefHoverPopupCreate(RefHoverState* s, HWND hwndCanvas) {
    if (!RegisterClassIfNeeded()) {
        return false;
    }
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kRefHoverClass, nullptr, WS_POPUP, 0, 0, 10, 10, hwndCanvas, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
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

bool RefHoverPopupShown(RefHoverState* s) {
    return s->hwndPopup != nullptr;
}

Size RefHoverPopupClientSize(RefHoverState* s) {
    return HwndClientRect(s->hwndPopup).Size();
}

// A wheel message meant for the popup: Ctrl + wheel zooms, around the cursor
// when it's on the popup. A horizontal wheel and Shift + wheel (right and down
// go right) scroll a zoomed-in popup sideways (issue #6429), else down / up:
// mouse software often sends Shift + wheel as a horizontal one (issue #6252).
// Any other wheel scrolls.
void RefHoverOnWheel(RefHoverState* s, EngineBase* engine, UINT msg, WPARAM wp, LPARAM lp, RefHoverWheelAt at) {
    int delta = GET_WHEEL_DELTA_WPARAM(wp);
    bool isCtrl = (LOWORD(wp) & MK_CONTROL) || IsCtrlPressed();
    bool isShift = (LOWORD(wp) & MK_SHIFT) || IsShiftPressed();
    if (msg == WM_MOUSEWHEEL && isCtrl) {
        Point anchor = kRefHoverNoAnchor;
        if (at == RefHoverWheelAt::Popup) {
            anchor = HwndScreenToClient(s->hwndPopup, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
        }
        RefHoverWheelZoom(s, engine, delta, anchor);
        return;
    }
    if (msg == WM_MOUSEWHEEL && !isShift) {
        RefHoverWheelScroll(s, engine, delta);
        return;
    }
    int right = (msg == WM_MOUSEHWHEEL) ? delta : -delta;
    if (at == RefHoverWheelAt::Popup && RefHoverWheelScrollX(s, engine, right)) {
        return;
    }
    RefHoverWheelScroll(s, engine, -right);
}

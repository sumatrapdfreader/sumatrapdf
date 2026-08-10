/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Timer.h"
#include "base/Win.h"

#include "wingui/FrameRateWnd.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Canvas.h"
#include "Menu.h"
#include "HomePage.h"
#include "Theme.h"
#include "FileHistory.h"
#include "AppSettings.h"

static void OnPaintAbout(MainWindow* win) {
    auto t = TimeGet();
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);
    if (!win->buffer) {
        EndPaint(win->hwndCanvas, &ps);
        return;
    }
    HDC bufDC = win->buffer->GetDC();
    GlobalPrefs* prefs = gGlobalPrefs;
    bool hasPerms = HasPermission(Perm::SavePreferences | Perm::DiskAccess);
    bool drawHome = hasPerms && prefs->rememberOpenedFiles && prefs->showStartPage;
    if (drawHome) {
        DrawHomePage(win, bufDC);
    } else {
        HomePageDestroySearch(win);
        // the about page has no virtual controls; leaving the home page's
        // around would keep them hit-testable over it
        HomePageDestroyChrome(win);
        DrawAboutPage(win, bufDC);
    }
    win->buffer->Flush(hdc);
    DrawCanvasKeyboardFocusIfNeeded(win, hdc);

    EndPaint(win->hwndCanvas, &ps);
    if (gShowFrameRate) {
        win->frameRateWnd->ShowFrameRateDur(TimeSinceInMs(t));
    }
}

static void OnMouseLeftButtonDownAbout(MainWindow* win, int x, int y, WPARAM /*key*/) {
    // lf("Left button clicked on %d %d", x, y);

    // remember a link under so that on mouse up we only activate
    // link if mouse up is on the same link as mouse down
    str::ReplaceWithCopy(&win->urlOnLastButtonDown, GetStaticLinkAtTemp(win->staticLinks, x, y, nullptr));
}

static bool IsLink(Str url) {
    if (str::StartsWithI(url, StrL("http:"))) {
        return true;
    }
    if (str::StartsWithI(url, StrL("https:"))) {
        return true;
    }
    if (str::StartsWithI(url, StrL("mailto:"))) {
        return true;
    }
    return false;
}

static void OnMouseMoveAbout(MainWindow* win, HWND hwnd, int x, int y) {
    TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT)};
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    TrackMouseEvent(&tme);
}

static void OnMouseLeftButtonUpAbout(MainWindow* win, int x, int y, WPARAM /*key*/) {
    TempStr url = GetStaticLinkAtTemp(win->staticLinks, x, y, nullptr);
    bool clickedURL = url && str::Eq(url, win->urlOnLastButtonDown);
    str::FreePtr(&win->urlOnLastButtonDown);
    if (!clickedURL) {
        return;
    }
    if (str::Eq(url, kLinkHideList)) {
        gGlobalPrefs->showStartPage = false;
        win->RedrawAll(true);
    } else if (str::Eq(url, kLinkShowList)) {
        gGlobalPrefs->showStartPage = true;
        win->RedrawAll(true);
    } else if (str::Eq(url, kLinkNextTip)) {
        PickAnotherRandomPromotion();
        win->RedrawAll(true);
    } else if (str::StartsWith(url, StrL("Cmd"))) {
        // may include args (e.g. "CmdFixDefaultApp .pdf")
        CustomCommand* custom = CreateCommandFromDefinition(url);
        if (custom) {
            HwndSendCommand(win->hwndFrame, custom->id);
        } else {
            int cmdId = GetCommandIdByName(url);
            if (cmdId > 0) {
                HwndSendCommand(win->hwndFrame, cmdId);
            }
        }
    } else if (IsLink(url)) {
        // documentation links open in the embedded manual browser
        if (!MaybeLaunchDocumentation(url)) {
            SumatraLaunchBrowser(url);
        }
    } else {
        // assume it's a thumbnail of a document
        auto path = url;
        ReportIf(!path);
        LoadArgs args(path, win);
        // ctrl forces always opening
        args.activateExisting = !IsCtrlPressed();
        args.activateExistingInWindow = true;
        StartLoadDocument(&args);
    }
    // HwndSetFocus(win->hwndFrame);
}

static void OnMouseRightButtonDownAbout(MainWindow* win, int x, int y, WPARAM /*key*/) {
    // lf("Right button clicked on %d %d", x, y);
    HwndSetFocus(win->hwndFrame);
    win->dragStart = Point(x, y);
}

static void OnMouseRightButtonUpAbout(MainWindow* win, int x, int y, WPARAM /*key*/) {
    int isDrag = IsDragDistance(x, win->dragStart.x, y, win->dragStart.y);
    if (isDrag) {
        return;
    }
    OnAboutContextMenu(win, x, y);
}

static LRESULT OnSetCursorAbout(MainWindow* win, HWND hwnd) {
    LRESULT res = 0;
    if (HomePageOnCanvasMessage(win, WM_SETCURSOR, 0, 0, res)) {
        return TRUE;
    }
    Point pt = HwndGetCursorPos(hwnd);
    if (!pt.IsEmpty()) {
        StaticLink* link = nullptr;
        if (GetStaticLinkAtTemp(win->staticLinks, pt.x, pt.y, &link)) {
            SetCursorCached(IDC_HAND);
            // File entries: selection/tip are driven only by real WM_MOUSEMOVE
            // (and keyboard). Do not call HomePageOnHover here — after arrow-key
            // selection the canvas invalidates and WM_SETCURSOR would snap the
            // active entry back under the stationary cursor.
            if (link && !path::IsAbsolute(link->target)) {
                // chrome links (open, tips, …) keep a simple hover tip
                win->ShowToolTip(LinkTooltipTemp(link), link->rect);
            }
        } else {
            // not on a link — hide tip; keyboard selection outline stays
            win->DeleteToolTip();
            SetCursorCached(IDC_ARROW);
        }
        return TRUE;
    }

    win->DeleteToolTip();
    return FALSE;
}

LRESULT WndProcCanvasAbout(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    int x = GET_X_LPARAM(lp);
    int y = GET_Y_LPARAM(lp);
    // the home page's virtual controls (header, view buttons, "Open a
    // document...", help button) handle their own hover / click / cursor
    switch (msg) {
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK: {
            LRESULT res = 0;
            if (HomePageOnCanvasMessage(win, msg, wp, lp, res)) {
                return res;
            }
            break;
        }
    }
    switch (msg) {
        case WM_CTLCOLOREDIT:
            if ((HWND)lp == win->hwndHomeSearch) {
                HDC hdcEdit = (HDC)wp;
                SetTextColor(hdcEdit, ThemeWindowTextColor());
                SetBkColor(hdcEdit, ThemeControlBackgroundColor());
                if (!win->brControlBgColor) {
                    win->brControlBgColor = CreateSolidBrush(ThemeControlBackgroundColor());
                }
                return (LRESULT)win->brControlBgColor;
            }
            break;

        case WM_COMMAND:
            if ((HWND)lp == win->hwndHomeSearch) {
                UINT notify = HIWORD(wp);
                if (notify == EN_CHANGE) {
                    win->homePageScrollY = 0;
                    // the filter changed the list, so select its first entry (#1136)
                    HomePageSelectFirst(win);
                    HwndInvalidate(win->hwndCanvas);
                    return 0;
                }
                // hide/show keyboard selection outline when focus enters/leaves search
                if (notify == EN_SETFOCUS || notify == EN_KILLFOCUS) {
                    HwndInvalidate(win->hwndCanvas);
                    return 0;
                }
            }
            break;

        case WM_KEYDOWN:
            // keyboard navigation of the file list (issue #1136). These keys are
            // routed here by MaybeTranslateAccelerator instead of scrolling
            switch (wp) {
                case VK_LEFT:
                    HomePageMoveSelection(win, -1, 0);
                    return 0;
                case VK_RIGHT:
                    HomePageMoveSelection(win, 1, 0);
                    return 0;
                case VK_UP:
                    HomePageMoveSelection(win, 0, -1);
                    return 0;
                case VK_DOWN:
                    HomePageMoveSelection(win, 0, 1);
                    return 0;
                case VK_RETURN: {
                    Str path = HomePageSelectedFilePathTemp(win);
                    if (!path) {
                        return 0;
                    }
                    LoadArgs args(path, win);
                    // ctrl forces always opening, as for a click
                    args.activateExisting = !IsCtrlPressed();
                    args.activateExistingInWindow = true;
                    StartLoadDocument(&args);
                    return 0;
                }
                case VK_DELETE: {
                    // remove the keyboard-selected entry from file history (not from disk)
                    Str path = HomePageSelectedFilePathTemp(win);
                    if (path) {
                        ForgetFileFromFrequentlyRead(win, path);
                    }
                    return 0;
                }
            }
            break;

        case WM_MOUSEMOVE:
            OnMouseMoveAbout(win, hwnd, x, y);
            return 0;

        case WM_MOUSELEAVE:
            HomePageClearActiveEntry(win);
            return 0;

        case WM_LBUTTONDOWN:
            OnMouseLeftButtonDownAbout(win, x, y, wp);
            return 0;

        case WM_LBUTTONUP:
            OnMouseLeftButtonUpAbout(win, x, y, wp);
            return 0;

        case WM_LBUTTONDBLCLK:
            OnMouseLeftButtonDownAbout(win, x, y, wp);
            return 0;

        case WM_RBUTTONDOWN:
            OnMouseRightButtonDownAbout(win, x, y, wp);
            return 0;

        case WM_RBUTTONUP:
            OnMouseRightButtonUpAbout(win, x, y, wp);
            return 0;

        case WM_SETCURSOR:
            if (OnSetCursorAbout(win, hwnd)) {
                return TRUE;
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_CONTEXTMENU:
            OnAboutContextMenu(win, 0, 0);
            return 0;

        case WM_PAINT:
            if (gRedrawLog) {
                logf("redraw: WM_PAINT hwnd=0x%p (canvas-about)\n", hwnd);
            }
            OnPaintAbout(win);
            return 0;

        case WM_VSCROLL:
            HomePageClearActiveEntry(win);
            HomePageOnVScroll(win, wp);
            return 0;

        case WM_MOUSEWHEEL: {
            HomePageClearActiveEntry(win);
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            HomePageOnMouseWheel(win, delta);
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

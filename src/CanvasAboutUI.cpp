/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"
#include "utils/Timer.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/FrameRateWnd.h"

#include "AppColors.h"
#include "Settings.h"
#include "DocController.h"
#include "GlobalPrefs.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "resource.h"
#include "Commands.h"
#include "Canvas.h"
#include "Menu.h"
#include "HomePage.h"
#include "Translations.h"
#include "Theme.h"

static void OnPaintAbout(MainWindow* win) {
    auto t = TimeGet();
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);

    auto txtCol = ThemeWindowTextColor();
    auto bgCol = GetMainWindowBackgroundColor();
    if (HasPermission(Perm::SavePreferences | Perm::DiskAccess) && gGlobalPrefs->rememberOpenedFiles &&
        gGlobalPrefs->showStartPage) {
        DrawHomePage(win, win->buffer->GetDC(), gFileHistory, txtCol, bgCol);
    } else {
        DrawAboutPage(win, win->buffer->GetDC());
    }
    win->buffer->Flush(hdc);

    EndPaint(win->hwndCanvas, &ps);
    if (gShowFrameRate) {
        win->frameRateWnd->ShowFrameRateDur(TimeSinceInMs(t));
    }
}

static void OnMouseLeftButtonDownAbout(MainWindow* win, int x, int y, WPARAM) {
    // lf("Left button clicked on %d %d", x, y);

    // remember a link under so that on mouse up we only activate
    // link if mouse up is on the same link as mouse down
    win->urlOnLastButtonDown.SetCopy(GetStaticLinkTemp(win->staticLinks, x, y, nullptr));
}

static bool IsLink(const char* url) {
    if (str::StartsWithI(url, "http:")) {
        return true;
    }
    if (str::StartsWithI(url, "https:")) {
        return true;
    }
    if (str::StartsWithI(url, "mailto:")) {
        return true;
    }
    return false;
}

static void OnMouseLeftButtonUpAbout(MainWindow* win, int x, int y, WPARAM) {
    char* url = GetStaticLinkTemp(win->staticLinks, x, y, nullptr);
    char* prevUrl = win->urlOnLastButtonDown;
    bool clickedURL = url && str::Eq(url, prevUrl);
    win->urlOnLastButtonDown.Set(nullptr);
    if (!clickedURL) {
        return;
    }
    if (str::Eq(url, kLinkOpenFile)) {
        HwndSendCommand(win->hwndFrame, CmdOpenFile);
    } else if (str::Eq(url, kLinkHideList)) {
        gGlobalPrefs->showStartPage = false;
        win->RedrawAll(true);
    } else if (str::Eq(url, kLinkShowList)) {
        gGlobalPrefs->showStartPage = true;
        win->RedrawAll(true);
    } else if (IsLink(url)) {
        SumatraLaunchBrowser(url);
    } else {
        // assume it's a thumbnail of a document
        auto path = url;
        CrashIf(!path);
        LoadArgs args(path, win);
        // ctrl forces always opening
        args.activateExisting = !IsCtrlPressed();
        LoadDocumentAsync(&args);
    }
    // SetFocus(win->hwndFrame);
}

static void OnMouseRightButtonDownAbout(MainWindow* win, int x, int y, WPARAM) {
    // lf("Right button clicked on %d %d", x, y);
    SetFocus(win->hwndFrame);
    win->dragStart = Point(x, y);
}

static void OnMouseRightButtonUpAbout(MainWindow* win, int x, int y, WPARAM) {
    int isDrag = IsDragDistance(x, win->dragStart.x, y, win->dragStart.y);
    if (isDrag) {
        return;
    }
    OnAboutContextMenu(win, x, y);
}

static LRESULT OnSetCursorAbout(MainWindow* win, HWND hwnd) {
    Point pt = HwndGetCursorPos(hwnd);
    if (!pt.IsEmpty()) {
        StaticLinkInfo* linkInfo;
        if (GetStaticLinkTemp(win->staticLinks, pt.x, pt.y, &linkInfo)) {
            win->ShowToolTip(linkInfo->infotip, linkInfo->rect);
            SetCursorCached(IDC_HAND);
        } else {
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
    switch (msg) {
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
            OnPaintAbout(win);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

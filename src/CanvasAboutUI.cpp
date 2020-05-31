/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
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
#include "EngineManager.h"
#include "Doc.h"

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
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
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

static void OnPaintAbout(WindowInfo* win) {
    auto t = TimeGet();
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);

    auto txtCol = GetAppColor(AppColor::MainWindowText);
    auto bgCol = GetAppColor(AppColor::MainWindowBg);
    if (HasPermission(Perm_SavePreferences | Perm_DiskAccess) && gGlobalPrefs->rememberOpenedFiles &&
        gGlobalPrefs->showStartPage) {
        DrawStartPage(win, win->buffer->GetDC(), gFileHistory, txtCol, bgCol);
    } else {
        DrawAboutPage(win, win->buffer->GetDC());
    }
    win->buffer->Flush(hdc);

    EndPaint(win->hwndCanvas, &ps);
    if (gShowFrameRate) {
        win->frameRateWnd->ShowFrameRateDur(TimeSinceInMs(t));
    }
}

static void OnMouseLeftButtonDownAbout(WindowInfo* win, int x, int y, WPARAM key) {
    UNUSED(key);
    // lf("Left button clicked on %d %d", x, y);

    // remember a link under so that on mouse up we only activate
    // link if mouse up is on the same link as mouse down
    win->url = GetStaticLink(win->staticLinks, x, y);
}

static void OnMouseLeftButtonUpAbout(WindowInfo* win, int x, int y, WPARAM key) {
    UNUSED(key);
    SetFocus(win->hwndFrame);

    const WCHAR* url = GetStaticLink(win->staticLinks, x, y);
    if (url && url == win->url) {
        if (str::Eq(url, SLINK_OPEN_FILE))
            SendMessage(win->hwndFrame, WM_COMMAND, IDM_OPEN, 0);
        else if (str::Eq(url, SLINK_LIST_HIDE)) {
            gGlobalPrefs->showStartPage = false;
            win->RedrawAll(true);
        } else if (str::Eq(url, SLINK_LIST_SHOW)) {
            gGlobalPrefs->showStartPage = true;
            win->RedrawAll(true);
        } else if (!str::StartsWithI(url, L"http:") && !str::StartsWithI(url, L"https:") &&
                   !str::StartsWithI(url, L"mailto:")) {
            LoadArgs args(url, win);
            LoadDocument(args);
        } else {
            SumatraLaunchBrowser(url);
        }
    }
    win->url = nullptr;
}

static void OnMouseRightButtonDownAbout(WindowInfo* win, int x, int y, WPARAM key) {
    UNUSED(key);
    // lf("Right button clicked on %d %d", x, y);
    SetFocus(win->hwndFrame);
    win->dragStart = Point(x, y);
}

static void OnMouseRightButtonUpAbout(WindowInfo* win, int x, int y, WPARAM key) {
    UNUSED(key);
    int isDragX = IsDragX(x, win->dragStart.x);
    int isDragY = IsDragY(y, win->dragStart.y);
    bool didDragMouse = isDragX || isDragY;
    if (!didDragMouse) {
        OnAboutContextMenu(win, x, y);
    }
}

static LRESULT OnSetCursorAbout(WindowInfo* win, HWND hwnd) {
    Point pt;
    if (GetCursorPosInHwnd(hwnd, pt)) {
        StaticLinkInfo linkInfo;
        if (GetStaticLink(win->staticLinks, pt.x, pt.y, &linkInfo)) {
            win->ShowToolTip(linkInfo.infotip, linkInfo.rect);
            SetCursor(IDC_HAND);
        } else {
            win->HideToolTip();
            SetCursor(IDC_ARROW);
        }
        return TRUE;
    }

    win->HideToolTip();
    return FALSE;
}

LRESULT WndProcCanvasAbout(WindowInfo* win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
            OnAboutContextMenu(win, 0, 0);
            return 0;

        case WM_PAINT:
            OnPaintAbout(win);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

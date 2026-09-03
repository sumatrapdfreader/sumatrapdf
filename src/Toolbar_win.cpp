/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// What the toolbar still needs Win32 for, now that VirtHost owns its window:
// the colors of the native page-number edit, dragging the frame by an empty
// part of the toolbar, eating the click that dismissed a drop-down menu, and
// reaching the frame and canvas windows (which are not VirtHosts yet).

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AnnotPlacement.h"
#include "WindowTab.h"
#include "Commands.h"
#include "AppTools.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/VirtHost.h"
#include "Theme.h"
#include "Toolbar.h"

//--- the frame and the canvas are still plain HWNDs

// canvas rectangle in frame-client coordinates
Rect ToolbarCanvasRectInFrame(MainWindow* win) {
    Rect rc = HwndWindowRect(win->hwndCanvas);
    Point tl = HwndScreenToClient(win->hwndFrame, rc.TL());
    return {tl, rc.Size()};
}

// a screen point in frame-client coordinates
Point ToolbarScreenToFrame(MainWindow* win, Point pt) {
    return HwndScreenToClient(win->hwndFrame, pt);
}

// repaint what the overlay toolbar was covering after it hides
void ToolbarRepaintUncovered(MainWindow* win, Rect rInFrame) {
    HwndInvalidate(win->hwndCanvas);
    HwndInvalidateRect(win->hwndFrame, rInFrame, false);
}

void ToolbarFocusFrame(MainWindow* win) {
    HwndSetFocus(win->hwndFrame);
}

bool ToolbarFrameIsVisible(MainWindow* win) {
    return HwndIsVisible(win->hwndFrame);
}

void ToolbarPostCommand(MainWindow* win, int cmdId) {
    LPARAM commandPoint = 0;
    if (cmdId >= CmdCreateAnnotFirst && cmdId <= CmdCreateAnnotLast && !CommandUsesPlacementMode(cmdId)) {
        Rect canvas = HwndClientRect(win->hwndCanvas);
        Point pt{canvas.dx / 2, canvas.dy / 2};
        commandPoint = MAKELPARAM(pt.x, pt.y);
    }
    HwndPostCommand(win->hwndFrame, cmdId, commandPoint);
}

void ToolbarSetHeight(MainWindow* win, int dy) {
    HWND hwnd = win ? win->hwndToolbar : nullptr;
    if (!hwnd || dy <= 0) {
        return;
    }
    Rect r = ChildPosWithinParent(hwnd);
    if (r.dy == dy) {
        return;
    }
    SetWindowPos(hwnd, nullptr, 0, 0, r.dx, dy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

//--- the native page-number edit

// Enter in either the page or chapter edit navigates; chaptered docs read
// both boxes and go by Location, single-chapter docs keep the page-label path
static void OnLocationEditChar(MainWindow* win, Edit::CharEvent* ev) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    switch ((Key)ev->c) {
        case Key::Enter: {
            DocController* ctrl = win->ctrl;
            if (ctrl->HasChapters()) {
                int chapter = win->chapterEdit ? ParseInt(win->chapterEdit->GetTextTemp()) : 1;
                int page = win->pageEdit ? ParseInt(win->pageEdit->GetTextTemp()) : 1;
                Location loc = ctrl->ClampLocation({chapter, page});
                ctrl->GoToLocation(loc, true);
            } else if (win->pageEdit) {
                TempStr s = win->pageEdit->GetTextTemp();
                int newPageNo = ctrl->GetPageByLabel(s);
                if (!ctrl->ValidPageNo(newPageNo)) {
                    ev->didHandle = true;
                    return;
                }
                ctrl->GoToPage(newPageNo, true);
            }
            HwndSetFocus(win->hwndFrame);
            // the overlay toolbar was kept up by the focus; now that
            // it's gone, let it hide again
            UpdateOverlayToolbarForMouse(win);
            ev->didHandle = true;
            return;
        }
        case Key::Escape:
            HwndSetFocus(win->hwndFrame);
            UpdateOverlayToolbarForMouse(win);
            ev->didHandle = true;
            return;
        case Key::Tab:
            AdvanceFocus(win);
            ev->didHandle = true;
            return;
        default:
            return;
    }
}

static int PageEditPadL() {
    return UiEdgeDx();
}

static int PageEditPadR() {
    return PageEditPadL() + DpiScale(4);
}

static Edit* ToolbarCreateLocationEdit(MainWindow* win, PlatformFont* font, int iconDy) {
    Edit::CreateArgs args;
    args.parent = win->hwndToolbar;
    args.font = font;
    args.isRtl = IsUIRtl();
    // no WS_EX_CLIENTEDGE: a themed edit draws a blue bottom accent (Win11)
    args.withFrame = true;
    args.noTheme = true;
    args.numbersOnly = true;
    args.alignRight = true;
    args.selectAllOnFocus = true;
    // the box is as tall as the icons, so without this the digits would sit at
    // its top instead of on the same line as "Page:" and "/ N"
    args.centerTextVert = true;
    args.marginLeft = PageEditPadL();
    args.marginRight = PageEditPadR();
    auto* e = new Edit();
    e->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    e->Create(args);
    // the toolbar tree arranges itself right-to-left (HBox.rtl), so its bounds
    // are offsets from the physical left; don't let the RTL host mirror them
    e->mapRtlX = true;
    // #5949: fixed width, or the box would resize to every page label while
    // scrolling a document with named pages, shifting the icons next to it.
    // ideal == max pins GetIdealSize() to this width
    e->SetIdealWidthChars(6);
    e->SetMaxWidthChars(6);
    e->idealDy = iconDy;
    e->onChar = MkFunc1(OnLocationEditChar, win);
    return e;
}

Edit* ToolbarCreatePageEdit(MainWindow* win, PlatformFont* font, int iconDy) {
    return ToolbarCreateLocationEdit(win, font, iconDy);
}

Edit* ToolbarCreateChapterEdit(MainWindow* win, PlatformFont* font, int iconDy) {
    return ToolbarCreateLocationEdit(win, font, iconDy);
}

// no document: the find edit does nothing, so don't offer a text cursor
void ToolbarUpdateFindEditCursor(MainWindow* win) {
    LPWSTR cursorId = win->IsDocLoaded() ? nullptr : IDC_ARROW;
    if (win->findEdit) {
        win->findEdit->SetCursorId(cursorId);
    }
}

//--- the messages VirtHost doesn't model

// the native edit control asks its parent what colors to draw itself in
static bool OnCtlColor(MainWindow* win, VirtHostNativeMsg* ev) {
    LRESULT reflected = TryReflectMessages(win->hwndToolbar, ev->msg, ev->wp, ev->lp);
    if (reflected) {
        ev->res = reflected;
        return true;
    }
    if (ev->msg == WM_COMMAND) {
        return false;
    }
    HDC hdc = (HDC)ev->wp;
    SetTextColor(hdc, TbTextColor());
    SetBkColor(hdc, ThemeWindowControlBackgroundColor());
    if (IsCurrentThemeDefault() && !ThemeColorizeControls() && !ThemeUsesHighContrastColors()) {
        ev->res = (LRESULT)GetStockObject(WHITE_BRUSH);
    } else {
        ev->res = (LRESULT)win->brControlBgColor;
    }
    return true;
}

// with the tabs in the title bar the toolbar is part of the caption, so
// dragging an empty part of it moves the window and a double click maximizes it
static bool OnCaptionDrag(MainWindow* win, VirtHostNativeMsg* ev) {
    HWND hwnd = win->hwndToolbar;
    Point pt = {GET_X_LPARAM(ev->lp), GET_Y_LPARAM(ev->lp)};
    HWND childAtPoint = ChildWindowFromPoint(hwnd, ToPOINT(pt));
    bool overChild = childAtPoint && childAtPoint != hwnd;
    // layout bounds are physical-left; WM_LBUTTONDOWN x is mirrored on RTL
    Point hitPt = pt;
    UnmirrorRtl(hwnd, hitPt);
    VirtCtrl* hit = ToolbarItemFromPoint(win, hitPt);
    if (overChild || (hit && hit->id != 0 && hit->id != PageInfoId)) {
        return false;
    }
    HWND hwndFrame = GetAncestor(hwnd, GA_ROOT);
    if (ev->msg == WM_LBUTTONDBLCLK) {
        WPARAM cmd = IsZoomed(hwndFrame) ? SC_RESTORE : SC_MAXIMIZE;
        PostMessageW(hwndFrame, WM_SYSCOMMAND, cmd, 0);
    } else {
        ReleaseCapture();
        SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
    ev->res = 0;
    return true;
}

static void OnToolbarNativeMsg(MainWindow* win, VirtHostNativeMsg* ev) {
    switch (ev->msg) {
        case WM_COMMAND:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            ev->didHandle = OnCtlColor(win, ev);
            return;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            if (win->tabsInTitlebar) {
                ev->didHandle = OnCaptionDrag(win, ev);
            }
            return;
    }
}

void ToolbarSetNativeHooks(MainWindow* win, VirtHost* host) {
    host->onNativeMsg = MkFunc1(OnToolbarNativeMsg, win);
}

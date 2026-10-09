/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the one win32 message path of the shell. gpui does not expose the native
// handle, and it has no hook for the messages a few services need
// (WM_HOTKEY for GlobalHotkeys, WM_COPYDATA for Explorer QuickLook,
// WM_ACTIVATE for the hotkey target window). We claim the gpui window of this
// thread by its class name and subclass it. OS file drops arrive as gpui
// ExternalPaths drops on the shell root.

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Pixmap.h"
#if OS_WIN
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#endif

#include "gui/UIModels.h"
#include "ProgressUpdateUI.h"
#include "EngineBase.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "GlobalHotkeys.h"
#include "ExplorerQuickLook.h"
#include "ReadAloud.h"
#include "SearchAndDDE.h"
#include "PdfTools.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "gui/OleDragDrop.h"
#include "gui/NativeCursors.h"
#include "gui/TouchGestures.h"
#include "HomePage.h"
#include "SumatraConfig.h"
#include "RefHover.h"
#include "gui/DocCanvas.h"
#include "AnnotEditToolbar.h"
#include "SelectTextKeyboard.h"

#if OS_WIN

#include <commctrl.h>

// gpui's win32 window class (ext/gpui/gpui.cpp)
static const WCHAR* kGpuiWndClass = L"GpuiSystemMonitor";

struct NativeWin {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    // orig's nonFullScreenWindowStyle / nonFullScreenFrameRect, kept here
    // because they are win32 state the portable MainWindow has no use for
    bool isFullScreen = false;
    long nonFullScreenWindowStyle = 0;
    Rect nonFullScreenFrameRect;
};

static Vec<NativeWin> gNativeWins;

HWND AppShellNativeHwnd(MainWindow* win) {
    for (const NativeWin& nw : gNativeWins) {
        if (nw.win == win) {
            return nw.hwnd;
        }
    }
    return nullptr;
}

MainWindow* AppShellWindowFromHwnd(HWND hwnd) {
    for (const NativeWin& nw : gNativeWins) {
        if (nw.hwnd == hwnd) {
            return nw.win;
        }
    }
    return nullptr;
}

static NativeWin* NativeWinOf(MainWindow* win) {
    for (NativeWin& nw : gNativeWins) {
        if (nw.win == win) {
            return &nw;
        }
    }
    return nullptr;
}

// orig's EnterFullScreen / ExitFullScreen window work: strip the caption and
// the sizing border, cover the monitor, and put the saved style and rect back
void AppShellSetFullScreen(MainWindow* win, bool fullScreen, bool restoreMaximized) {
    NativeWin* nw = NativeWinOf(win);
    if (!nw || nw->isFullScreen == fullScreen) {
        return;
    }
    HWND hwnd = nw->hwnd;
    if (fullScreen) {
        // ng: gpui's client title bar insets a zoomed frame by the sizing
        // border, which a fullscreen frame doesn't have
        AppShellSetClientTitleBar(win, false);
        long ws = GetWindowLong(hwnd, GWL_STYLE);
        nw->nonFullScreenWindowStyle = ws;
        nw->nonFullScreenFrameRect = HwndWindowRect(hwnd);
        nw->isFullScreen = true;
        ws &= ~(WS_CAPTION | WS_THICKFRAME);
        ws |= WS_MAXIMIZE;
        Rect rect = HwndGetFullscreenRect(hwnd);
        SetWindowLong(hwnd, GWL_STYLE, ws);
        uint flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER;
        SetWindowPos(hwnd, nullptr, rect.x, rect.y, rect.dx, rect.dy, flags);
        logf("AppShellSetFullScreen: on, %d,%d,%d,%d\n", rect.x, rect.y, rect.dx, rect.dy);
        return;
    }

    nw->isFullScreen = false;
    AppShellSetClientTitleBar(win, win->tabsInTitlebar);
    bool wasMaximized = restoreMaximized || (nw->nonFullScreenWindowStyle & WS_MAXIMIZE) != 0;
    long style = nw->nonFullScreenWindowStyle;
    if (wasMaximized) {
        // clear WS_MAXIMIZE so ShowWindow(SW_MAXIMIZE) applies cleanly
        style &= ~WS_MAXIMIZE;
    }
    SetWindowLong(hwnd, GWL_STYLE, style);
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, flags);
    if (wasMaximized) {
        ShowWindow(hwnd, SW_MAXIMIZE);
        logf("AppShellSetFullScreen: off, maximized\n");
        return;
    }
    // the display may have rotated or a monitor gone away while we were
    // fullscreen, so clamp the restore rect to the current work area
    Rect restore = nw->nonFullScreenFrameRect;
    Size limited = HwndLimitSizeToScreen(hwnd, {restore.dx, restore.dy});
    restore.dx = limited.dx;
    restore.dy = limited.dy;
    restore = ShiftRectToWorkArea(restore, nullptr, true);
    HwndMoveWindow(hwnd, &restore);
    logf("AppShellSetFullScreen: off, %d,%d,%d,%d\n", restore.x, restore.y, restore.dx, restore.dy);
}

Rect AppShellWindowScreenRect(MainWindow* win) {
    HWND hwnd = AppShellNativeHwnd(win);
    return hwnd ? HwndWindowRect(hwnd) : Rect{};
}

Rect AppShellCanvasScreenRect(MainWindow* win) {
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd || win->canvasRc.IsEmpty()) {
        return {};
    }
    // canvasRc is in dips of the client area
    int dpi = std::max((int)GetDpiForWindow(hwnd), 96);
    Rect c = win->canvasRc;
    POINT pt{MulDiv(c.x, dpi, 96), MulDiv(c.y, dpi, 96)};
    ClientToScreen(hwnd, &pt);
    return Rect{pt.x, pt.y, MulDiv(c.dx, dpi, 96), MulDiv(c.dy, dpi, 96)};
}

// ng: gpui opens a window at a size only and cannot move one (see "gpui
// gaps"), so orig's HwndMoveWindow / ShowWindow are applied to the frame gpui
// made. `r` is the outer rectangle in screen pixels; an empty one is left alone
bool AppShellPlaceWindow(MainWindow* win, Rect r, bool maximize) {
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd) {
        return false;
    }
    if (!r.IsEmpty()) {
        if (!maximize && IsZoomed(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }
        HwndMoveWindow(hwnd, &r);
    }
    if (maximize) {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }
    logf("AppShellPlaceWindow: %d,%d,%d,%d maximize %d\n", r.x, r.y, r.dx, r.dy, (int)maximize);
    return true;
}

// orig's RememberDefaultWindowPosition: the frame's rectangle, or the one it
// goes back to when it is maximized (so we know which monitor it is on)
static bool ReadNormalWindowRect(NativeWin* nw, Rect* out) {
    if (!nw || !IsWindow(nw->hwnd) || IsIconic(nw->hwnd) || nw->isFullScreen) {
        return false;
    }
    if (!IsZoomed(nw->hwnd)) {
        *out = HwndWindowRect(nw->hwnd);
        return !out->IsEmpty();
    }
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(nw->hwnd, &wp)) {
        return false;
    }
    *out = ToRect(wp.rcNormalPosition);
    return !out->IsEmpty();
}

// ng: the settings are also saved after gpui destroyed the frames (quitting
// closes every window first), so the last rectangle a frame had is kept on
// its MainWindow and answers when the HWND no longer can
bool AppShellNormalWindowRect(MainWindow* win, Rect* out) {
    if (!win) {
        return false;
    }
    Rect r;
    if (ReadNormalWindowRect(NativeWinOf(win), &r)) {
        win->normalWindowRc = r;
    }
    if (win->normalWindowRc.IsEmpty()) {
        return false;
    }
    *out = win->normalWindowRc;
    return true;
}

void AppShellFrameChanged(MainWindow* win) {
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd) {
        return;
    }
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, flags);
}

struct SystemMenuAt {
    HWND hwnd = nullptr;
    POINT pt{};
};

// ng: a win32 menu runs a message loop of its own, so it is opened from the
// ui task queue, once gpui is done with the click that asked for it
static void OpenSystemMenuNow(SystemMenuAt* at) {
    HWND hwnd = at->hwnd;
    POINT pt = at->pt;
    delete at;
    if (!IsWindow(hwnd)) {
        return;
    }
    HMENU menu = GetSystemMenu(hwnd, FALSE);
    if (!menu) {
        return;
    }
    // what DefWindowProc does before it shows this menu
    bool zoomed = IsZoomed(hwnd);
    EnableMenuItem(menu, SC_RESTORE, MF_BYCOMMAND | (zoomed ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, SC_MOVE, MF_BYCOMMAND | (zoomed ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(menu, SC_SIZE, MF_BYCOMMAND | (zoomed ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(menu, SC_MAXIMIZE, MF_BYCOMMAND | (zoomed ? MF_GRAYED : MF_ENABLED));
    SetMenuDefaultItem(menu, SC_CLOSE, FALSE);
    uint flags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD;
    int cmd = (int)TrackPopupMenu(menu, flags, pt.x, pt.y, 0, hwnd, nullptr);
    if (cmd != 0) {
        PostMessageW(hwnd, WM_SYSCOMMAND, (WPARAM)cmd, 0);
    }
}

void AppShellOpenSystemMenu(MainWindow* win, Rect below) {
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd) {
        return;
    }
    float scale = (float)DpiGetForHwnd(hwnd) / 96.f;
    auto* at = new SystemMenuAt();
    at->hwnd = hwnd;
    at->pt = POINT{(LONG)((float)below.x * scale), (LONG)((float)(below.y + below.dy) * scale)};
    ClientToScreen(hwnd, &at->pt);
    uitask::Post(MkFunc0<SystemMenuAt>(OpenSystemMenuNow, at), "OpenSystemMenu");
}

// orig draws GCLP_HICONSM into its system menu button
Pixmap* AppShellAppIconPixmap(MainWindow* win) {
    static Pixmap* gAppIcon = nullptr;
    static bool gTried = false;
    if (gTried) {
        return gAppIcon;
    }
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd) {
        return nullptr;
    }
    gTried = true;
    HICON icon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!icon) {
        icon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    }
    bool ownIcon = false;
    if (!icon) {
        int dx = GetSystemMetrics(SM_CXSMICON);
        int dy = GetSystemMetrics(SM_CYSMICON);
        icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, dx, dy, 0);
        ownIcon = icon != nullptr;
    }
    if (icon) {
        gAppIcon = PixmapFromHICON(icon);
    }
    if (ownIcon) {
        DestroyIcon(icon);
    }
    return gAppIcon;
}

// dpi of the monitor the frame is on
int AppShellWindowDpi(MainWindow* win) {
    HWND hwnd = AppShellNativeHwnd(win);
    return hwnd ? DpiGetForHwnd(hwnd) : 96;
}

Rect AppShellWorkArea(MainWindow* win) {
    HWND hwnd = win ? AppShellNativeHwnd(win) : nullptr;
    Rect r = hwnd ? HwndWindowRect(hwnd) : Rect{};
    return GetWorkAreaRect(r, hwnd);
}

Rect AppShellMonitorRect(MainWindow* win) {
    HWND hwnd = win ? AppShellNativeHwnd(win) : nullptr;
    if (!hwnd) {
        return {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    }
    return HwndGetFullscreenRect(hwnd);
}

Rect AppShellShiftToWorkArea(Rect rect, MainWindow* win, bool fully) {
    return ShiftRectToWorkArea(rect, win ? AppShellNativeHwnd(win) : nullptr, fully);
}

// ng: gpui's WM_SETCURSOR falls back to IDC_ARROW when it has no cursor, so
// there is no way to ask it for no cursor at all; ShowCursor is independent
// of it (see "gpui gaps")
void AppShellShowCursor(MainWindow*, bool show) {
    static bool gHidden = false;
    if (show == !gHidden) {
        return;
    }
    gHidden = !show;
    ShowCursor(show ? TRUE : FALSE);
}

void AppShellPreventSleep(bool on) {
    EXECUTION_STATE st = on ? (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) : ES_CONTINUOUS;
    SetThreadExecutionState(st);
}

static MainWindow* WinOf(HWND hwnd) {
    MainWindow* win = AppShellWindowFromHwnd(hwnd);
    return IsMainWindowValidAndNotClosing(win) ? win : nullptr;
}

struct DroppedFiles {
    MainWindow* win = nullptr;
    Vec<Str> paths;
    PointF pt;
    bool inClient = false;
    DropHost host = DropHost::Frame;
};

// what a drop does: the Merge PDF dialog takes the PDFs while it is up, the
// canvas opens the files
static void OpenDroppedFilesNow(DroppedFiles* d) {
    MainWindow* win = d->win;
    Vec<Str>& paths = d->paths;
    if (IsMainWindowValidAndNotClosing(win) && paths.len > 0 &&
        !PdfToolDialogOnDropFiles(win, paths.els, paths.len, d->inClient ? &d->pt : nullptr, d->host) &&
        d->host == DropHost::Frame) {
        OpenDroppedFiles(win, paths.els, paths.len);
    }
    for (Str s : paths) {
        str::Free(s);
    }
    delete d;
}

// orig's OnDropFiles. `ptDrop` is in client pixels. Returns false when the
// drop is refused. `dragFinish` is false for an HDROP that an IDataObject
// owns (OLE drop), and then the files are opened once Drop() returned, so
// the drag source isn't kept waiting.
bool AppShellOnDropFiles(HWND hwnd, HDROP hdrop, POINT ptDrop, bool inClient, bool dragFinish) {
    int n = (int)DragQueryFileW(hdrop, (UINT)-1, nullptr, 0);
    // client pixels; the Merge PDF grid wants window dips
    float scale = (float)DpiGetForHwnd(hwnd) / 96.f;
    PointF ptDips{(float)ptDrop.x / scale, (float)ptDrop.y / scale};
    Vec<Str> paths;
    for (int i = 0; i < n; i++) {
        WCHAR buf[1024]{};
        if (DragQueryFileW(hdrop, (UINT)i, buf, dimof(buf)) > 0) {
            // orig's GetDropFilesResolved: a dropped shortcut is its target
            Str path = ToUtf8Temp(buf);
            if (str::EndsWithI(path, StrL(".lnk"))) {
                TempStr resolved = ResolveLnkTemp(path);
                if (len(resolved) > 0) {
                    path = resolved;
                }
            }
            VecAppend(paths, str::Dup(path));
        }
    }
    if (dragFinish) {
        DragFinish(hdrop);
    }
    logf("AppShellOnDropFiles: %d file(s)\n", paths.len);
    // a tool window's drop (the Merge PDF window) is its main window's
    DropHost host = ToolWindowOwnsHwnd(hwnd) ? DropHost::ToolWindow : DropHost::Frame;
    MainWindow* win = host == DropHost::Frame ? WinOf(hwnd) : ToolWindowOwnerFromHwnd(hwnd);
    if (!IsMainWindowValidAndNotClosing(win)) {
        win = nullptr;
    }
    bool accept = win && paths.len > 0;
    if (accept && host == DropHost::Frame && !PdfToolDialogIsMerge(win)) {
        // orig registers only the canvas as a drop target: the menu, the tab
        // strip, the toolbar and the sidebar refuse a drop
        Rect rc = win->canvasRc;
        bool onCanvas = inClient && ptDips.x >= (float)rc.x && ptDips.x < (float)(rc.x + rc.dx) &&
                        ptDips.y >= (float)rc.y && ptDips.y < (float)(rc.y + rc.dy);
        if (!onCanvas) {
            logf("AppShellOnDropFiles: not on the canvas, ignored\n");
            accept = false;
        }
    }
    if (!accept) {
        for (Str s : paths) {
            str::Free(s);
        }
        return false;
    }
    auto* d = new DroppedFiles();
    d->win = win;
    for (Str s : paths) {
        VecAppend(d->paths, s);
    }
    d->pt = ptDips;
    d->inClient = inClient;
    d->host = host;
    if (dragFinish) {
        OpenDroppedFilesNow(d);
    } else {
        uitask::Post(MkFunc0<DroppedFiles>(OpenDroppedFilesNow, d), "OpenDroppedFiles");
    }
    return true;
}

static void OnDropFiles(HWND hwnd, HDROP hdrop) {
    POINT ptDrop{};
    bool inClient = DragQueryPoint(hdrop, &ptDrop);
    AppShellOnDropFiles(hwnd, hdrop, ptDrop, inClient, true);
}

// ng: gpui has no cursor from an image. When the canvas wants one of orig's
// bitmap cursors (the drag hand, the laser dot, a placement cursor) it asks
// gpui for a stand-in kind and this puts the real cursor in its place.
static HCURSOR NativeCursorWanted(HWND hwnd) {
    MainWindow* win = WinOf(hwnd);
    if (!win || !AppShellNativeCursorActive(win)) {
        return nullptr;
    }
    return NativeCursorGet(win->nativeCursor);
}

TempStr NativeCursorTestTemp(MainWindow* win, Str bmpPath) {
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd) {
        return StrL("ERR no-window");
    }
    HCURSOR cur = NativeCursorWanted(hwnd);
    LRESULT handled = SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    int dx = 0;
    int dy = 0;
    int hotX = 0;
    int hotY = 0;
    ICONINFO ii{};
    if (cur && GetIconInfo(cur, &ii)) {
        BITMAP bm{};
        GetObject(ii.hbmColor ? ii.hbmColor : ii.hbmMask, sizeof(bm), &bm);
        dx = bm.bmWidth;
        dy = ii.hbmColor ? bm.bmHeight : bm.bmHeight / 2;
        hotX = (int)ii.xHotspot;
        hotY = (int)ii.yHotspot;
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
    }
    if (cur && dx > 0 && dy > 0 && len(bmpPath) > 0) {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = dx;
        bmi.bmiHeader.biHeight = -dy;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC hdc = CreateCompatibleDC(nullptr);
        HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dib && bits) {
            HGDIOBJ prev = SelectObject(hdc, dib);
            // on a mid gray, so that black, white and translucent parts all show
            RECT rc{0, 0, dx, dy};
            HBRUSH br = CreateSolidBrush(RGB(0xa0, 0xa0, 0xa0));
            FillRect(hdc, &rc, br);
            DeleteObject(br);
            DrawIconEx(hdc, 0, 0, cur, dx, dy, 0, nullptr, DI_NORMAL);
            GdiFlush();
            BITMAPFILEHEADER fh{};
            fh.bfType = 0x4d42;
            fh.bfOffBits = sizeof(fh) + sizeof(BITMAPINFOHEADER);
            fh.bfSize = fh.bfOffBits + (DWORD)(dx * dy * 4);
            str::Builder data;
            data.Append(Str((char*)&fh, (int)sizeof(fh)));
            data.Append(Str((char*)&bmi.bmiHeader, (int)sizeof(BITMAPINFOHEADER)));
            data.Append(Str((char*)bits, dx * dy * 4));
            file::WriteFile(bmpPath, ToStrTemp(data));
            SelectObject(hdc, prev);
        }
        DeleteObject(dib);
        DeleteDC(hdc);
    }
    return fmt("OK native=%d active=%d handled=%d size=%dx%d hot=%d,%d", (int)win->nativeCursor, cur ? 1 : 0,
               (int)handled, dx, dy, hotX, hotY);
}

void AppShellApplyNativeCursor(MainWindow* win) {
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd || !AppShellNativeCursorActive(win)) {
        return;
    }
    POINT pt{};
    GetCursorPos(&pt);
    if (WindowFromPoint(pt) != hwnd) {
        return;
    }
    if (HCURSOR cur = NativeCursorGet(win->nativeCursor)) {
        SetCursor(cur);
    }
}

float CanvasScale(MainWindow* win);
bool FreeTextInPlaceSetText(MainWindow* win, const WCHAR* text);
bool CommandPaletteSetText(MainWindow* win, const WCHAR* text);
bool FreeTextInPlaceCommitOnChar(MainWindow* win, int ch);

// orig's SumatraPDF.h. Posted to the canvas; ng has no canvas child.
constexpr int kSbHalfPageUp = WM_USER + 102;
constexpr int kSbHalfPageDown = WM_USER + 103;

static bool ScrollMsgFromSb(bool vert, UINT code, ScrollMsg* out) {
    if (code == SB_THUMBPOSITION || code == SB_THUMBTRACK) {
        *out = ScrollMsg::ThumbTrack;
        return true;
    }
    if (code == SB_ENDSCROLL) {
        return false;
    }
    if (vert) {
        switch (code) {
            case SB_LINEUP:
                *out = ScrollMsg::LineUp;
                return true;
            case SB_LINEDOWN:
                *out = ScrollMsg::LineDown;
                return true;
            case SB_PAGEUP:
                *out = ScrollMsg::PageUp;
                return true;
            case SB_PAGEDOWN:
                *out = ScrollMsg::PageDown;
                return true;
            case SB_TOP:
                *out = ScrollMsg::Top;
                return true;
            case SB_BOTTOM:
                *out = ScrollMsg::Bottom;
                return true;
            case kSbHalfPageUp:
                *out = ScrollMsg::HalfPageUp;
                return true;
            case kSbHalfPageDown:
                *out = ScrollMsg::HalfPageDown;
                return true;
            default:
                return false;
        }
    }
    switch (code) {
        case SB_LINELEFT:
            *out = ScrollMsg::LineLeft;
            return true;
        case SB_LINERIGHT:
            *out = ScrollMsg::LineRight;
            return true;
        case SB_PAGELEFT:
            *out = ScrollMsg::PageLeft;
            return true;
        case SB_PAGERIGHT:
            *out = ScrollMsg::PageRight;
            return true;
        case SB_LEFT:
            *out = ScrollMsg::Left;
            return true;
        case SB_RIGHT:
            *out = ScrollMsg::Right;
            return true;
        default:
            return false;
    }
}

// One Present leaves PrintWindow on an older flip-swapchain buffer.
static void PresentScroll(MainWindow* win, HWND hwnd) {
    for (int i = 0; i < 3; i++) {
        AppShellInvalidate(win);
        UpdateWindow(hwnd);
    }
}

// A posted click carries MK_* in wParam. gpui asks GetKeyState, which
// SendMessage does not update, so Ctrl+click never entered Edit PDF.
struct PostedKeyBits {
    BYTE before[256]{};
    bool changed = false;
};

static PostedKeyBits ApplyPostedMouseMods(WPARAM wp) {
    PostedKeyBits bits;
    bool wantCtrl = (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) != 0 && !IsCtrlPressed();
    bool wantShift = (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) != 0 && !IsShiftPressed();
    if (!wantCtrl && !wantShift) {
        return bits;
    }
    if (!GetKeyboardState(bits.before)) {
        return bits;
    }
    BYTE state[256];
    memcpy(state, bits.before, sizeof(state));
    if (wantCtrl) {
        state[VK_CONTROL] |= 0x80;
        state[VK_LCONTROL] |= 0x80;
    }
    if (wantShift) {
        state[VK_SHIFT] |= 0x80;
        state[VK_LSHIFT] |= 0x80;
    }
    if (SetKeyboardState(state)) {
        bits.changed = true;
    }
    return bits;
}

static void RestorePostedMouseMods(PostedKeyBits& bits) {
    if (!bits.changed) {
        return;
    }
    SetKeyboardState(bits.before);
    bits.changed = false;
}

// GetPixel cannot read the frame: it is a flip swapchain created with
// WS_EX_NOREDIRECTIONBITMAP. Tests sample SUMATRA_PDF_CANVAS. This owned
// popup is a real GDI window, so the sample sees the page.
static const WCHAR* kPixelMirrorClass = L"SUMATRA_PDF_CANVAS";

struct PixelMirror {
    HWND frame = nullptr;
    HWND hwnd = nullptr;
    HBITMAP dib = nullptr;
};

static Vec<PixelMirror> gPixelMirrors;
static bool gPixelMirrorBusy = false;

static LRESULT CALLBACK PixelMirrorProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static PixelMirror* PixelMirrorForFrame(HWND frame) {
    for (PixelMirror& m : gPixelMirrors) {
        if (m.frame == frame) {
            return &m;
        }
    }
    return nullptr;
}

static PixelMirror* PixelMirrorForHwnd(HWND hwnd) {
    for (PixelMirror& m : gPixelMirrors) {
        if (m.hwnd == hwnd) {
            return &m;
        }
    }
    return nullptr;
}

static bool MirrorInputMsg(UINT msg) {
    switch (msg) {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_CONTEXTMENU:
        case WM_VSCROLL:
        case WM_HSCROLL:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_SYSCHAR:
            return true;
    }
    return false;
}

static void RegisterPixelMirrorClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = PixelMirrorProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kPixelMirrorClass;
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static void BlitDib(HWND hwnd, HBITMAP dib, int w, int h) {
    HDC dc = GetWindowDC(hwnd);
    if (!dc) {
        return;
    }
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, dib);
    BitBlt(dc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(hwnd, dc);
}

static void PlacePixelMirror(PixelMirror* m) {
    if (!m || !m->hwnd || !IsWindow(m->frame)) {
        return;
    }
    RECT rc{};
    GetClientRect(m->frame, &rc);
    POINT origin{0, 0};
    ClientToScreen(m->frame, &origin);
    // leave z-order alone: an owned popup already sits above its frame, and
    // raising it here would cover a dialog opened after it
    SetWindowPos(m->hwnd, nullptr, origin.x, origin.y, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
}

// PrintWindow of the frame, then into the popup. The popup is hidden for the
// capture so it is not what the frame's thumbnail shows.
static void UpdatePixelMirror(HWND frame) {
    if (!gForTesting || gPixelMirrorBusy) {
        return;
    }
    PixelMirror* m = PixelMirrorForFrame(frame);
    if (!m || !m->hwnd) {
        return;
    }
    RECT rc{};
    GetClientRect(frame, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        return;
    }

    gPixelMirrorBusy = true;
    PlacePixelMirror(m);
    SetWindowPos(m->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC screen = GetDC(frame);
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (dib) {
        HDC mem = CreateCompatibleDC(screen);
        HGDIOBJ old = SelectObject(mem, dib);
        ok = PrintWindow(frame, mem, PW_CLIENTONLY | PW_RENDERFULLCONTENT) != FALSE;
        SelectObject(mem, old);
        DeleteDC(mem);
    }
    ReleaseDC(frame, screen);

    if (!ok) {
        if (dib) {
            DeleteObject(dib);
        }
        PlacePixelMirror(m);
        gPixelMirrorBusy = false;
        return;
    }
    if (m->dib) {
        DeleteObject(m->dib);
    }
    m->dib = dib;
    PlacePixelMirror(m);
    BlitDib(m->hwnd, dib, w, h);
    gPixelMirrorBusy = false;
}

static void EnsurePixelMirror(HWND frame) {
    if (!gForTesting || !frame || PixelMirrorForFrame(frame)) {
        return;
    }
    RegisterPixelMirrorClass();
    PixelMirror m;
    m.frame = frame;
    m.hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_NOPARENTNOTIFY, kPixelMirrorClass, L"",
                             WS_POPUP, 0, 0, 0, 0, frame, nullptr, GetModuleHandleW(nullptr), frame);
    if (!m.hwnd) {
        return;
    }
    VecAppend(gPixelMirrors, m);
    UpdatePixelMirror(frame);
}

static void ForgetPixelMirror(HWND frame) {
    for (int i = 0; i < len(gPixelMirrors); i++) {
        if (gPixelMirrors[i].frame != frame) {
            continue;
        }
        HWND hwnd = gPixelMirrors[i].hwnd;
        HBITMAP dib = gPixelMirrors[i].dib;
        gPixelMirrors[i].hwnd = nullptr;
        gPixelMirrors[i].dib = nullptr;
        gPixelMirrors[i].frame = nullptr;
        if (dib) {
            DeleteObject(dib);
        }
        if (hwnd) {
            DestroyWindow(hwnd);
        }
        VecRemoveAt(gPixelMirrors, i);
        return;
    }
}

static LRESULT CALLBACK PixelMirrorProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    HWND frame = (HWND)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        PixelMirror* m = PixelMirrorForHwnd(hwnd);
        if (m && m->dib && dc) {
            BITMAP bm{};
            if (GetObject(m->dib, sizeof(bm), &bm) == sizeof(bm)) {
                HDC mem = CreateCompatibleDC(dc);
                HGDIOBJ old = SelectObject(mem, m->dib);
                BitBlt(dc, 0, 0, bm.bmWidth, bm.bmHeight, mem, 0, 0, SRCCOPY);
                SelectObject(mem, old);
                DeleteDC(mem);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_MOUSEACTIVATE) {
        return MA_NOACTIVATE;
    }
    if (msg == WM_SETFOCUS) {
        if (frame) {
            SetFocus(frame);
        }
        return 0;
    }
    if (msg == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    if (frame && MirrorInputMsg(msg)) {
        return SendMessageW(frame, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT DefSubclassWithPostedMods(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PostedKeyBits bits = ApplyPostedMouseMods(wp);
    LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
    RestorePostedMouseMods(bits);
    return res;
}

static LRESULT CALLBACK ShellSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_NCCALCSIZE:
            // SetScrollInfo adds scroll styles. Keep them out of the client size.
            if (wp) {
                LONG style = GetWindowLongW(hwnd, GWL_STYLE);
                if (style & (WS_VSCROLL | WS_HSCROLL)) {
                    SetWindowLongW(hwnd, GWL_STYLE, style & ~(WS_VSCROLL | WS_HSCROLL));
                }
            }
            break;
        case WM_VSCROLL:
        case WM_HSCROLL:
            // Tests scroll the canvas HWND. ng draws in the frame, so the
            // same SB_* codes land here.
            if (MainWindow* win = WinOf(hwnd)) {
                bool vert = msg == WM_VSCROLL;
                ScrollMsg sm;
                if (ScrollMsgFromSb(vert, LOWORD(wp), &sm)) {
                    int thumb = (int)(short)HIWORD(wp);
                    if (vert && win->IsCurrentTabAbout()) {
                        HomePageOnVScroll(win, sm, thumb);
                    } else if (vert) {
                        CanvasOnVScroll(win, sm, thumb);
                    } else {
                        CanvasOnHScroll(win, sm, thumb);
                    }
                    if (IsMainWindowValid(win)) {
                        PresentScroll(win, hwnd);
                    }
                }
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                if (HCURSOR cur = NativeCursorWanted(hwnd)) {
                    SetCursor(cur);
                    return TRUE;
                }
            }
            break;
        case WM_MOUSEMOVE: {
            // gpui sets the cursor itself from inside the mouse move when the
            // kind under the pointer changed
            LRESULT res = DefSubclassWithPostedMods(hwnd, msg, wp, lp);
            if (HCURSOR cur = NativeCursorWanted(hwnd)) {
                SetCursor(cur);
            }
            return res;
        }
        case WM_GESTURENOTIFY:
        case WM_GESTURE:
            // orig's OnGesture: pinch zoom, pan, flick, rotate, taps on a document
            if (MainWindow* win = WinOf(hwnd)) {
                if (TouchGesturesOnMessage(win, hwnd, msg, lp)) {
                    return 0;
                }
            }
            break;
        case WM_DROPFILES:
            OnDropFiles(hwnd, (HDROP)wp);
            return 0;
        case WM_HOTKEY:
            if (HandleGlobalHotkey((int)wp)) {
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            // the home page scrolls from a wheel posted at the frame, including
            // when the cursor is not over the thumbnails
            if (MainWindow* win = WinOf(hwnd)) {
                // CloseWindow is tearing the document down. Drop the wheel.
                if (win->isBeingClosed) {
                    return 0;
                }
                if (msg == WM_MOUSEWHEEL && win->IsCurrentTabAbout()) {
                    HomePageOnMouseWheel(win, GET_WHEEL_DELTA_WPARAM(wp));
                    return 0;
                }
                // posted wheels carry MK_* in the message. gpui reads GetKeyState,
                // which a test's PostMessage does not set.
                POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                ScreenToClient(hwnd, &pt);
                int delta = GET_WHEEL_DELTA_WPARAM(wp);
                bool isCtrl = ((GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) != 0) || IsCtrlPressed();
                bool isShift = ((GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) != 0) || IsShiftPressed();
                if (RefHoverTakePostedWheel(win, msg == WM_MOUSEHWHEEL, delta, isCtrl, isShift, pt.x, pt.y)) {
                    return 0;
                }
                if (ForwardBrowserMsg(win, msg, wp, lp)) {
                    return 0;
                }
                // lParam is screen coords. A message aimed at the window rather
                // than a point (tests post 0) is outside the client; orig's
                // canvas window still scrolls.
                RECT rc{};
                GetClientRect(hwnd, &rc);
                bool inside = pt.x >= 0 && pt.y >= 0 && pt.x < rc.right && pt.y < rc.bottom;
                if (!inside && msg == WM_MOUSEWHEEL && win->AsFixed()) {
                    DocCanvasPostedWheel(win, delta, isCtrl, isShift);
                    return 0;
                }
            }
            break;
        case WM_KEYDOWN:
            if (MainWindow* win = WinOf(hwnd)) {
                if (ForwardBrowserMsg(win, msg, wp, lp)) {
                    return 0;
                }
                LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
                // A handled key tells gpui to drop the WM_CHAR that follows.
                // End and the arrows never send one, so the next character
                // typed into a field would be dropped.
                if (IsMainWindowValid(win) && MapVirtualKeyW((UINT)wp, MAPVK_VK_TO_CHAR) == 0) {
                    AppShellClearEatChar(win);
                }
                return res;
            }
            break;
        case WM_KEYUP:
            if (MainWindow* win = WinOf(hwnd)) {
                if (ForwardBrowserMsg(win, msg, wp, lp)) {
                    return 0;
                }
            }
            break;
        case WM_SETTEXT:
            if (MainWindow* win = WinOf(hwnd)) {
                if (FreeTextInPlaceSetText(win, (const WCHAR*)lp)) {
                    return TRUE;
                }
                if (CommandPaletteSetText(win, (const WCHAR*)lp)) {
                    return TRUE;
                }
            }
            break;
        case WM_CHAR:
            if (MainWindow* win = WinOf(hwnd)) {
                if (FreeTextInPlaceCommitOnChar(win, (int)wp)) {
                    return 0;
                }
                // a posted WM_CHAR (the keyboard-selection test types 'v')
                // never becomes a gpui keydown. Handling it there as well
                // would toggle visual mode twice for a real key.
                if (SelectTextWithKeyboardOnChar(win, (int)wp)) {
                    return 0;
                }
            }
            break;
        case WM_COMMAND:
            // a posted command id (the test harness, and anything else that
            // used to send WM_COMMAND to the frame). Control notifications
            // have a non-zero HIWORD and are not commands.
            if (HIWORD(wp) == 0) {
                if (MainWindow* win = WinOf(hwnd)) {
                    int cmdId = (int)LOWORD(wp);
                    if (cmdId > 0) {
                        // lParam is a frame client point. ExecuteCmdAtPoint
                        // wants the document point ToDoc would produce.
                        if (lp != 0) {
                            int px = (int)(short)LOWORD(lp);
                            int py = (int)(short)HIWORD(lp);
                            float s = CanvasScale(win);
                            if (s <= 0.f) {
                                s = 1.f;
                            }
                            float dipX = (float)px * s;
                            float dipY = (float)py * s;
                            Point pt{(int)((dipX - (float)win->canvasRc.x) / s),
                                     (int)((dipY - (float)win->canvasRc.y) / s)};
                            ExecuteCmdAtPoint(win, cmdId, pt);
                        } else {
                            ExecuteCmd(win, cmdId);
                        }
                        // close deletes the window before ExecuteCmd returns
                        if (IsMainWindowValid(win)) {
                            AppShellInvalidate(win);
                        }
                        return 0;
                    }
                }
            }
            break;
        case WM_APPCOMMAND:
            // both keyboard and mouse drivers should produce WM_APPCOMMAND
            // messages for their special keys, so handle these here and return
            // TRUE so as to not make them bubble up further
            if (MainWindow* win = WinOf(hwnd)) {
                int cmdId = 0;
                switch (GET_APPCOMMAND_LPARAM(lp)) {
                    case APPCOMMAND_BROWSER_BACKWARD:
                        cmdId = CmdNavigateBack;
                        break;
                    case APPCOMMAND_BROWSER_FORWARD:
                        cmdId = CmdNavigateForward;
                        break;
                    case APPCOMMAND_BROWSER_REFRESH:
                        cmdId = CmdReloadDocument;
                        break;
                    case APPCOMMAND_BROWSER_SEARCH:
                        cmdId = CmdFindFirst;
                        break;
                    case APPCOMMAND_BROWSER_FAVORITES:
                        cmdId = CmdToggleBookmarks;
                        break;
                }
                if (cmdId) {
                    ExecuteCmd(win, cmdId);
                    AppShellInvalidate(win);
                    return TRUE;
                }
            }
            break;
        case WM_COPYDATA:
            // the DDE grammar, the reuse-instance open and Explorer QuickLook
            if (OnCopyData(hwnd, wp, lp)) {
                return TRUE;
            }
            break;
        case WM_DDE_INITIATE:
            return OnDDEInitiate(hwnd, wp, lp);
        case WM_DDE_EXECUTE:
            return OnDDExecute(hwnd, wp, lp);
        case WM_DDE_REQUEST:
            return OnDDERequest(hwnd, wp, lp);
        case WM_DDE_TERMINATE:
            return OnDDETerminate(hwnd, wp, lp);
        case WM_ACTIVATE:
            if (wp != WA_INACTIVE) {
                GlobalHotkeysOnActivate(hwnd);
            }
            break;
        case kWmTtsEvent:
            if (MainWindow* win = WinOf(hwnd)) {
                ReadAloudOnTtsEvent(win);
                return 0;
            }
            break;
        case WM_DPICHANGED:
            // gpui only moves the window to the suggested rect. The font sizes
            // follow frameDpi, which AppShellOnDpiChanged stores first.
            if (MainWindow* win = WinOf(hwnd)) {
                int dpi = RoundUp((int)LOWORD(wp), 4);
                if (dpi < 72) {
                    dpi = 96;
                }
                AppShellOnDpiChanged(win, dpi);
                return DefSubclassProc(hwnd, msg, wp, lp);
            }
            break;
        case WM_WINDOWPOSCHANGED:
            // moved, sized, maximized or restored: remember where it is
            if (MainWindow* win = WinOf(hwnd)) {
                Rect r;
                AppShellNormalWindowRect(win, &r);
                // orig's NotifyWindowMoved: the docked popups go along, once
                // gpui has laid the frame out for its new size
                LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
                ToolWindowsFollow(win);
                if (PixelMirror* m = PixelMirrorForFrame(hwnd)) {
                    PlacePixelMirror(m);
                }
                return res;
            }
            break;
        case WM_PAINT: {
            LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
            UpdatePixelMirror(hwnd);
            return res;
        }
        case WM_KILLFOCUS:
            // no new focus: the on-screen keyboard. Keep the contents editor.
            if ((HWND)wp == nullptr) {
                if (MainWindow* win = WinOf(hwnd)) {
                    if (IsEditingAnnotContents(win)) {
                        AnnotContentsKeepOnKillFocus(win);
                        return 0;
                    }
                }
            }
            break;
        case WM_DESTROY:
            ForgetPixelMirror(hwnd);
            GlobalHotkeysOnDestroy(hwnd);
            RevokeCanvasDropTarget(hwnd);
            break;
        case WM_CONTEXTMENU:
            if (MainWindow* win = WinOf(hwnd)) {
                DocCanvasOnWmContextMenu(win, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
            return DefSubclassWithPostedMods(hwnd, msg, wp, lp);
        default:
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static bool HwndIsClaimed(HWND hwnd) {
    if (ToolWindowOwnsHwnd(hwnd)) {
        return true;
    }
    for (const NativeWin& nw : gNativeWins) {
        if (nw.hwnd == hwnd) {
            return true;
        }
    }
    return false;
}

static BOOL CALLBACK FindGpuiWnd(HWND hwnd, LPARAM lp) {
    WCHAR cls[64]{};
    GetClassNameW(hwnd, cls, dimof(cls));
    if (!wstr::EqI(WStr(cls), WStr((WCHAR*)kGpuiWndClass))) {
        return TRUE;
    }
    if (HwndIsClaimed(hwnd)) {
        return TRUE; // another MainWindow already took this one
    }
    *(HWND*)lp = hwnd;
    return FALSE;
}

struct DpiToggleWindows {
    Vec<HWND>* hwnds;
};

static BOOL CALLBACK CollectDpiToggleWindows(HWND hwnd, LPARAM lp) {
    auto* ctx = (DpiToggleWindows*)lp;
    if (IsWindowVisible(hwnd)) {
        VecAppend(*ctx->hwnds, hwnd);
    }
    return TRUE;
}

// orig's ToggleDpiOverride: 0 -> 125% -> 150% -> 75% -> 0, then a
// WM_DPICHANGED whose suggested rect is scaled by the DPI ratio.
void AppShellToggleDpiOverride() {
    if (!gIsDebugBuild) {
        return;
    }
    int next = 125;
    if (gDpiOverride == 125) {
        next = 150;
    } else if (gDpiOverride == 150) {
        next = 75;
    } else if (gDpiOverride == 75) {
        next = 0;
    }

    Vec<HWND> hwnds;
    DpiToggleWindows ctx{&hwnds};
    EnumThreadWindows(GetCurrentThreadId(), CollectDpiToggleWindows, (LPARAM)&ctx);

    Vec<Rect> rects;
    Vec<int> dpis;
    for (HWND hwnd : hwnds) {
        VecAppend(rects, HwndWindowRect(hwnd));
        VecAppend(dpis, DpiGetForHwnd(hwnd));
    }

    gDpiOverride = next;
    int newDpi = len(hwnds) > 0 ? DpiGetForHwnd(hwnds[0]) : DpiGet();
    for (int i = 0; i < len(hwnds); i++) {
        int oldDpi = dpis[i] > 0 ? dpis[i] : 96;
        Rect r = rects[i];
        RECT suggested{r.x, r.y, r.x + MulDiv(r.dx, newDpi, oldDpi), r.y + MulDiv(r.dy, newDpi, oldDpi)};
        SendMessageW(hwnds[i], WM_DPICHANGED, MAKEWPARAM(newDpi, newDpi), (LPARAM)&suggested);
    }
}

void AppShellEnableFileDrop(MainWindow* win) {
    HWND hwnd = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), FindGpuiWnd, (LPARAM)&hwnd);
    if (!hwnd) {
        log(StrL("AppShellEnableFileDrop: no gpui window found\n"));
        return;
    }
    NativeWin nw{hwnd, win};
    VecAppend(gNativeWins, nw);
    // WM_DROPFILES, for when the OLE registration below fails
    DragAcceptFiles(hwnd, TRUE);
    // orig's CanvasDropTarget: files, image URLs, and where a dragged file is
    RegisterCanvasDropTarget(hwnd);
    SetWindowSubclass(hwnd, ShellSubclass, 1, 0);
    EnsurePixelMirror(hwnd);
    RegisterGlobalHotkeys(hwnd);
    // SAPI posts its word-boundary / end-of-stream events to a window
    TtsSetNotifyWindow(hwnd, kWmTtsEvent, 0, 0);
    logf("AppShellEnableFileDrop: hwnd 0x%p accepts files\n", (void*)hwnd);
}

// ng: orig looks for its own kFrameClassName; every gpui window shares one
// class, so a window of another instance is found by class + process id and
// confirmed by the process's exe path.
static TempStr ProcessPathTemp(DWORD procId) {
    AutoCloseHandle hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, procId);
    if (!hProc.IsValid()) {
        return {};
    }
    WCHAR pathW[MAX_PATH]{};
    DWORD pathLen = dimof(pathW);
    if (!QueryFullProcessImageNameW(hProc, 0, pathW, &pathLen)) {
        return {};
    }
    return ToUtf8Temp(pathW);
}

struct FindOtherWin {
    DWORD procId; // 0: any process running our exe
    Vec<HWND>* out;
};

static BOOL CALLBACK FindOtherInstanceWnd(HWND hwnd, LPARAM lp) {
    auto* d = (FindOtherWin*)lp;
    WCHAR cls[64]{};
    GetClassNameW(hwnd, cls, dimof(cls));
    if (!wstr::EqI(WStr(cls), WStr((WCHAR*)kGpuiWndClass))) {
        return TRUE;
    }
    DWORD procId = 0;
    GetWindowThreadProcessId(hwnd, &procId);
    if (procId == 0 || procId == GetCurrentProcessId()) {
        return TRUE;
    }
    if (d->procId != 0) {
        if (procId != d->procId) {
            return TRUE;
        }
    } else if (!path::IsSame(ProcessPathTemp(procId), GetSelfExePathTemp())) {
        return TRUE;
    }
    VecAppend(*d->out, hwnd);
    return TRUE;
}

void AppShellFindOtherInstanceWindows(DWORD procId, Vec<HWND>& out) {
    FindOtherWin d{procId, &out};
    EnumWindows(FindOtherInstanceWnd, (LPARAM)&d);
}

void AppShellForgetNativeHwnd(MainWindow* win) {
    for (int i = len(gNativeWins) - 1; i >= 0; i--) {
        if (gNativeWins[i].win != win) {
            continue;
        }
        HWND hwnd = gNativeWins[i].hwnd;
        VecRemoveAt(gNativeWins, i);
        GlobalHotkeysOnDestroy(hwnd);
        RevokeCanvasDropTarget(hwnd);
        TouchGesturesForget(win);
    }
}

#else

void AppShellEnableFileDrop(MainWindow*) {}

void AppShellApplyNativeCursor(MainWindow*) {}

void AppShellFrameChanged(MainWindow*) {}

void AppShellOpenSystemMenu(MainWindow*, Rect) {}

Pixmap* AppShellAppIconPixmap(MainWindow*) {
    return nullptr;
}

void AppShellForgetNativeHwnd(MainWindow*) {}

#endif

// x, y are window dips from the top left. Hover may omit paths. Leave clears
// the Merge PDF insertion bar. A drop on the canvas opens the files; while
// Merge PDF is up the dialog takes a drop anywhere on the frame.
bool AppShellAcceptDrop(MainWindow* win, float x, float y, const Str* paths, int n, FileDropPhase phase) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return false;
    }
    if (phase == FileDropPhase::Leave || !CanAccessDisk() || gPluginMode) {
        bool ignored = false;
        PdfToolDialogOnDragOver(win, nullptr, false, &ignored, DropHost::Frame);
        return false;
    }
    if (n < 0) {
        n = 0;
    }
    if (n > 0 && !paths) {
        return false;
    }

    bool hasPdf = n == 0 && phase == FileDropPhase::Hover;
    for (int i = 0; i < n; i++) {
        if (str::EndsWithI(paths[i], StrL(".pdf"))) {
            hasPdf = true;
            break;
        }
    }

    PointF pt{x, y};
    if (phase == FileDropPhase::Hover) {
        bool accept = false;
        if (PdfToolDialogOnDragOver(win, &pt, hasPdf, &accept, DropHost::Frame)) {
            return accept;
        }
    } else if (PdfToolDialogIsMerge(win)) {
        // orig accepts a drop anywhere on the frame while Merge PDF is up,
        // and the dialog consumes it instead of opening a tab
        if (n > 0) {
            PdfToolDialogOnDropFiles(win, paths, n, &pt, DropHost::Frame);
        }
        return n > 0;
    }

    Rect rc = win->canvasRc;
    bool onCanvas = x >= (float)rc.x && x < (float)(rc.x + rc.dx) && y >= (float)rc.y && y < (float)(rc.y + rc.dy);
    if (!onCanvas) {
        return false;
    }
    if (phase != FileDropPhase::Drop) {
        return true;
    }
    if (n <= 0) {
        return false;
    }
    logf("AppShellAcceptDrop: %d file(s)\n", n);
    OpenDroppedFiles(win, paths, n);
    return true;
}

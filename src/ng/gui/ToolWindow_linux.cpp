/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// X11 frames for tool windows. A second display connection moves the window
// gpui created; gpui keeps receiving its events.

#include <X11/Xatom.h>
#include <X11/Xlib.h>

typedef Window XWin;

#undef Window
#undef Bool
#undef Status
#undef None

#include "gui/GpuiBridge.h"

#include "gui/ToolWindowPlat.h"

static Display* Dpy() {
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
}

static XWin XOf(gp::Window* gw) {
    return (XWin)(uintptr_t)gp::PlatWindowHandle(gw);
}

static XWin RootWin(Display* dpy) {
    return DefaultRootWindow(dpy);
}

// left, right, top, bottom. Zero until the window manager has set them.
static void FrameExtents(Display* dpy, XWin xw, int* left, int* right, int* top, int* bottom) {
    *left = *right = *top = *bottom = 0;
    Atom prop = XInternAtom(dpy, "_NET_FRAME_EXTENTS", True);
    if (prop == 0 || xw == 0) {
        return;
    }
    Atom type = 0;
    int format = 0;
    unsigned long n = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dpy, xw, prop, 0, 4, False, XA_CARDINAL, &type, &format, &n, &after, &data) != Success) {
        return;
    }
    if (data && format == 32 && n >= 4) {
        auto* v = (long*)data;
        *left = (int)v[0];
        *right = (int)v[1];
        *top = (int)v[2];
        *bottom = (int)v[3];
    }
    if (data) {
        XFree(data);
    }
}

static bool ClientOrigin(Display* dpy, XWin xw, int* x, int* y, int* dx, int* dy) {
    XWindowAttributes attr{};
    if (!XGetWindowAttributes(dpy, xw, &attr)) {
        return false;
    }
    XWin child = 0;
    if (!XTranslateCoordinates(dpy, xw, RootWin(dpy), 0, 0, x, y, &child)) {
        return false;
    }
    *dx = attr.width;
    *dy = attr.height;
    return true;
}

static Rect WorkArea(Display* dpy) {
    XWin root = RootWin(dpy);
    int sx = DisplayWidth(dpy, DefaultScreen(dpy));
    int sy = DisplayHeight(dpy, DefaultScreen(dpy));
    Rect full{0, 0, sx, sy};
    Atom prop = XInternAtom(dpy, "_NET_WORKAREA", True);
    if (prop == 0) {
        return full;
    }
    Atom type = 0;
    int format = 0;
    unsigned long n = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dpy, root, prop, 0, 4, False, XA_CARDINAL, &type, &format, &n, &after, &data) != Success) {
        return full;
    }
    Rect r = full;
    if (data && format == 32 && n >= 4) {
        auto* v = (long*)data;
        if (v[2] > 0 && v[3] > 0) {
            r = Rect{(int)v[0], (int)v[1], (int)v[2], (int)v[3]};
        }
    }
    if (data) {
        XFree(data);
    }
    return r;
}

static bool ClientRectOf(gp::Window* gw, int* x, int* y, int* dx, int* dy) {
    Display* dpy = Dpy();
    XWin xw = XOf(gw);
    if (!dpy || !xw) {
        return false;
    }
    return ClientOrigin(dpy, xw, x, y, dx, dy);
}

Rect ToolWinNativeFrame(gp::Window* gw) {
    Display* dpy = Dpy();
    XWin xw = XOf(gw);
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
    if (!dpy || !ClientRectOf(gw, &x, &y, &dx, &dy)) {
        return {};
    }
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    FrameExtents(dpy, xw, &left, &right, &top, &bottom);
    return Rect{x - left, y - top, dx + left + right, dy + top + bottom};
}

// the X window is the client; the window manager's frame is outside it
Rect ToolWinNativeContentRect(gp::Window* gw) {
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
    if (!ClientRectOf(gw, &x, &y, &dx, &dy)) {
        return {};
    }
    return Rect{x, y, dx, dy};
}

Rect ToolWinNativeWorkArea(gp::Window*) {
    Display* dpy = Dpy();
    return dpy ? WorkArea(dpy) : Rect{};
}

Rect ToolWinNativeMonitor(gp::Window*) {
    Display* dpy = Dpy();
    if (!dpy) {
        return {};
    }
    return Rect{0, 0, DisplayWidth(dpy, DefaultScreen(dpy)), DisplayHeight(dpy, DefaultScreen(dpy))};
}

// what a decorated window adds when the window manager has not said yet
constexpr int kTitleGuess = 28;
constexpr int kBorderGuess = 1;

Size ToolWinNativeChrome(bool titled, bool, bool) {
    if (!titled) {
        return {};
    }
    return Size(2 * kBorderGuess, kTitleGuess + kBorderGuess);
}

void ToolWinNativeSetFrame(gp::Window* gw, Rect outer, bool titled) {
    Display* dpy = Dpy();
    XWin xw = XOf(gw);
    if (!dpy || !xw || outer.IsEmpty()) {
        return;
    }
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    if (titled) {
        FrameExtents(dpy, xw, &left, &right, &top, &bottom);
        if (top == 0 && left == 0) {
            top = kTitleGuess;
            bottom = kBorderGuess;
            left = right = kBorderGuess;
        }
    }
    int dx = std::max(outer.dx - left - right, 1);
    int dy = std::max(outer.dy - top - bottom, 1);
    XMoveResizeWindow(dpy, xw, outer.x + left, outer.y + top, (unsigned)dx, (unsigned)dy);
    XFlush(dpy);
}

void ToolWinNativeApplyStyle(gp::Window*, bool, bool, bool, bool) {}

void ToolWinNativeSetOwner(gp::Window* gw, gp::Window* owner, bool owned) {
    Display* dpy = Dpy();
    XWin xw = XOf(gw);
    XWin parent = XOf(owner);
    if (!dpy || !xw) {
        return;
    }
    if (owned && parent) {
        XSetTransientForHint(dpy, xw, parent);
    }
    XFlush(dpy);
}

void ToolWinNativeShow(gp::Window* gw, bool visible, bool activate) {
    Display* dpy = Dpy();
    XWin xw = XOf(gw);
    if (!dpy || !xw) {
        return;
    }
    if (!visible) {
        XUnmapWindow(dpy, xw);
    } else {
        XMapRaised(dpy, xw);
        if (activate) {
            XSetInputFocus(dpy, xw, RevertToParent, CurrentTime);
        }
    }
    XFlush(dpy);
}

void ToolWinNativeActivate(gp::Window* gw) {
    Display* dpy = Dpy();
    XWin xw = XOf(gw);
    if (!dpy || !xw) {
        return;
    }
    XRaiseWindow(dpy, xw);
    XSetInputFocus(dpy, xw, RevertToParent, CurrentTime);
    XFlush(dpy);
}

bool ToolWinNativeIsActive(gp::Window* gw) {
    Display* dpy = Dpy();
    XWin xw = XOf(gw);
    if (!dpy || !xw) {
        return false;
    }
    XWin focus = 0;
    int revert = 0;
    XGetInputFocus(dpy, &focus, &revert);
    return focus == xw;
}

void ToolWinNativeSetMinClient(gp::Window* gw, int dx, int dy) {
    Display* dpy = Dpy();
    XWin xw = XOf(gw);
    if (!dpy || !xw || dx <= 0 || dy <= 0) {
        return;
    }
    XSizeHints hints{};
    hints.flags = PMinSize;
    hints.min_width = dx;
    hints.min_height = dy;
    XSetWMNormalHints(dpy, xw, &hints);
    XFlush(dpy);
}

static XWin gGrabbed = 0;

void ToolWinNativeSetModal(gp::Window* gw, gp::Window*, bool on) {
    Display* dpy = Dpy();
    if (!dpy) {
        return;
    }
    if (gGrabbed) {
        XUngrabPointer(dpy, CurrentTime);
        XUngrabKeyboard(dpy, CurrentTime);
        gGrabbed = 0;
    }
    XWin xw = XOf(gw);
    if (!on || !xw) {
        XFlush(dpy);
        return;
    }
    unsigned mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    if (XGrabPointer(dpy, xw, True, mask, GrabModeAsync, GrabModeAsync, 0, 0, CurrentTime) == GrabSuccess) {
        XGrabKeyboard(dpy, xw, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        gGrabbed = xw;
    }
    XFlush(dpy);
}

bool ToolWinNativeMouseDown() {
    Display* dpy = Dpy();
    if (!dpy) {
        return false;
    }
    XWin root = 0;
    XWin child = 0;
    int rx = 0;
    int ry = 0;
    int wx = 0;
    int wy = 0;
    unsigned mask = 0;
    if (!XQueryPointer(dpy, RootWin(dpy), &root, &child, &rx, &ry, &wx, &wy, &mask)) {
        return false;
    }
    return (mask & (Button1Mask | Button2Mask | Button3Mask)) != 0;
}

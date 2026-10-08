/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// macOS frames for tool windows. gpui's window is an NSWindow; this places,
// styles and owns it. y grows down from the top of the primary screen.

#include "gui/GpuiBridge.h"

// AppKit's MacTypes and the `defer:` parameter collide with ours
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#pragma push_macro("defer")
#undef defer
#import <AppKit/AppKit.h>
#pragma pop_macro("defer")
#undef BOOL
#include "base/MacTypesShow.h"

#include "gui/ToolWindowPlat.h"

static NSWindow* NsWin(gp::Window* gw) {
    NSView* view = (__bridge NSView*)gp::PlatWindowHandle(gw);
    return view.window;
}

// the screen whose origin is (0, 0): its top is y = 0 in our coordinates
static CGFloat FlipHeight() {
    for (NSScreen* screen in [NSScreen screens]) {
        if (screen.frame.origin.x == 0 && screen.frame.origin.y == 0) {
            return screen.frame.size.height;
        }
    }
    NSScreen* screen = [NSScreen mainScreen];
    return screen ? screen.frame.size.height : 0;
}

static Rect FromMac(NSRect fr) {
    CGFloat h = FlipHeight();
    return Rect{(int)fr.origin.x, (int)(h - (fr.origin.y + fr.size.height)), (int)fr.size.width, (int)fr.size.height};
}

static NSRect ToMac(Rect r) {
    CGFloat h = FlipHeight();
    return NSMakeRect(r.x, h - (r.y + r.dy), r.dx, r.dy);
}

static NSScreen* ScreenOf(NSWindow* w) {
    if (w.screen) {
        return w.screen;
    }
    return [NSScreen mainScreen];
}

static NSWindowStyleMask StyleMask(bool titled, bool resizable, bool utility, bool borderless) {
    if (borderless || !titled) {
        return NSWindowStyleMaskBorderless;
    }
    NSWindowStyleMask mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;
    if (resizable) {
        mask |= NSWindowStyleMaskResizable;
    }
    if (utility) {
        mask |= NSWindowStyleMaskUtilityWindow;
    } else {
        mask |= NSWindowStyleMaskMiniaturizable;
    }
    return mask;
}

Rect ToolWinNativeFrame(gp::Window* gw) {
    NSWindow* w = NsWin(gw);
    if (!w) {
        return {};
    }
    return FromMac(w.frame);
}

Rect ToolWinNativeContentRect(gp::Window* gw) {
    NSWindow* w = NsWin(gw);
    if (!w) {
        return {};
    }
    return FromMac([w contentRectForFrameRect:w.frame]);
}

Rect ToolWinNativeWorkArea(gp::Window* gw) {
    NSScreen* screen = ScreenOf(NsWin(gw));
    if (!screen) {
        return {};
    }
    return FromMac(screen.visibleFrame);
}

Rect ToolWinNativeMonitor(gp::Window* gw) {
    NSScreen* screen = ScreenOf(NsWin(gw));
    if (!screen) {
        return {};
    }
    return FromMac(screen.frame);
}

Size ToolWinNativeChrome(bool titled, bool resizable, bool utility) {
    if (!titled) {
        return {};
    }
    NSRect content = NSMakeRect(0, 0, 200, 200);
    NSRect frame = [NSWindow frameRectForContentRect:content styleMask:StyleMask(true, resizable, utility, false)];
    return Size((int)(frame.size.width - content.size.width), (int)(frame.size.height - content.size.height));
}

void ToolWinNativeSetFrame(gp::Window* gw, Rect outer, bool) {
    NSWindow* w = NsWin(gw);
    if (!w || outer.IsEmpty()) {
        return;
    }
    [w setFrame:ToMac(outer) display:YES];
}

void ToolWinNativeApplyStyle(gp::Window* gw, bool titled, bool resizable, bool utility, bool borderless) {
    NSWindow* w = NsWin(gw);
    if (!w) {
        return;
    }
    [w setStyleMask:StyleMask(titled, resizable, utility, borderless)];
    if (borderless || !titled) {
        [w setHasShadow:YES];
        [w setMovableByWindowBackground:NO];
    }
}

void ToolWinNativeSetOwner(gp::Window* gw, gp::Window* owner, bool owned) {
    NSWindow* w = NsWin(gw);
    NSWindow* parent = NsWin(owner);
    if (!w) {
        return;
    }
    NSWindow* cur = w.parentWindow;
    if (cur && cur != parent) {
        [cur removeChildWindow:w];
    }
    if (owned && parent && w.parentWindow != parent) {
        [parent addChildWindow:w ordered:NSWindowAbove];
    }
}

void ToolWinNativeShow(gp::Window* gw, bool visible, bool activate) {
    NSWindow* w = NsWin(gw);
    if (!w) {
        return;
    }
    if (!visible) {
        [w orderOut:nil];
        return;
    }
    if (activate) {
        [w makeKeyAndOrderFront:nil];
    } else {
        [w orderFront:nil];
    }
}

void ToolWinNativeActivate(gp::Window* gw) {
    NSWindow* w = NsWin(gw);
    if (w) {
        [w makeKeyAndOrderFront:nil];
    }
}

bool ToolWinNativeIsActive(gp::Window* gw) {
    NSWindow* w = NsWin(gw);
    return w && w.keyWindow;
}

void ToolWinNativeSetMinClient(gp::Window* gw, int dx, int dy) {
    NSWindow* w = NsWin(gw);
    if (!w || dx <= 0 || dy <= 0) {
        return;
    }
    [w setContentMinSize:NSMakeSize(dx, dy)];
}

// clicks and keys aimed at the owner go to the dialog instead
static id gModalMonitor = nil;

void ToolWinNativeSetModal(gp::Window* gw, gp::Window* owner, bool on) {
    if (gModalMonitor) {
        [NSEvent removeMonitor:gModalMonitor];
        gModalMonitor = nil;
    }
    NSWindow* tool = NsWin(gw);
    NSWindow* parent = NsWin(owner);
    if (!on || !tool || !parent) {
        return;
    }
    gModalMonitor = [NSEvent
        addLocalMonitorForEventsMatchingMask:(NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown |
                                               NSEventMaskOtherMouseDown | NSEventMaskKeyDown)
                                      handler:^NSEvent*(NSEvent* event) {
                                        if (event.window == parent) {
                                            [tool makeKeyAndOrderFront:nil];
                                            return nil;
                                        }
                                        return event;
                                      }];
}

bool ToolWinNativeMouseDown() {
    return [NSEvent pressedMouseButtons] != 0;
}

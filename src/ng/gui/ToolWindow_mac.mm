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
#import <objc/runtime.h>

// A borderless NSWindow cannot become key. Replacing its isa drops AppKit's
// KVO subclass and throws on close. A flag plus these overrides is enough.
static char gWantsKey;
static BOOL (*gOrigCanBecomeKey)(id, SEL);
static BOOL (*gOrigCanBecomeMain)(id, SEL);

static BOOL ToolCanBecomeKey(id self, SEL sel) {
    if (objc_getAssociatedObject(self, &gWantsKey)) {
        return YES;
    }
    return gOrigCanBecomeKey(self, sel);
}

static BOOL ToolCanBecomeMain(id self, SEL sel) {
    if (objc_getAssociatedObject(self, &gWantsKey)) {
        return NO;
    }
    return gOrigCanBecomeMain(self, sel);
}

static void InstallKeyOverrides() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    Method key = class_getInstanceMethod([NSWindow class], @selector(canBecomeKeyWindow));
    Method main = class_getInstanceMethod([NSWindow class], @selector(canBecomeMainWindow));
    gOrigCanBecomeKey = (decltype(gOrigCanBecomeKey))method_getImplementation(key);
    gOrigCanBecomeMain = (decltype(gOrigCanBecomeMain))method_getImplementation(main);
    method_setImplementation(key, (IMP)ToolCanBecomeKey);
    method_setImplementation(main, (IMP)ToolCanBecomeMain);
}

static bool WindowWantsKey(NSWindow* w) {
    return w && objc_getAssociatedObject(w, &gWantsKey) != nil;
}

#pragma pop_macro("defer")
#undef BOOL
#include "base/MacTypesShow.h"

#include "gui/ToolWindowPlat.h"

static id gKeyMonitor = nil;
static bool gInKeyForward = false;

static NSEvent* KeyEventInWindow(NSEvent* event, NSWindow* window) {
    if (event.windowNumber == window.windowNumber) {
        return event;
    }
    return [NSEvent keyEventWithType:event.type
                            location:event.locationInWindow
                       modifierFlags:event.modifierFlags
                           timestamp:event.timestamp
                        windowNumber:window.windowNumber
                             context:nil
                          characters:event.characters ?: @""
         charactersIgnoringModifiers:event.charactersIgnoringModifiers ?: @""
                           isARepeat:event.isARepeat
                             keyCode:event.keyCode];
}

static void EnsureKeyMonitor() {
    if (gKeyMonitor) {
        return;
    }
    gKeyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyDown | NSEventMaskKeyUp)
                                                         handler:^NSEvent*(NSEvent* event) {
                                                           NSWindow* key = NSApp.keyWindow;
                                                           if (!WindowWantsKey(key) || gInKeyForward) {
                                                               return event;
                                                           }
                                                           // already on its way to the content view
                                                           if (event.window == key && key.firstResponder == key.contentView) {
                                                               return event;
                                                           }
                                                           // command shortcuts stay with the menu (paste, quit)
                                                           if (event.modifierFlags & NSEventModifierFlagCommand) {
                                                               return event;
                                                           }
                                                           NSView* view = key.contentView;
                                                           if (!view) {
                                                               return event;
                                                           }
                                                           NSEvent* rewritten = KeyEventInWindow(event, key);
                                                           gInKeyForward = true;
                                                           if (event.type == NSEventTypeKeyDown) {
                                                               [view keyDown:rewritten];
                                                           } else {
                                                               [view keyUp:rewritten];
                                                           }
                                                           gInKeyForward = false;
                                                           return nil;
                                                         }];
}

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

void ToolWinNativeApplyStyle(gp::Window* gw, bool titled, bool resizable, bool utility, bool borderless, bool wantsKey) {
    NSWindow* w = NsWin(gw);
    if (!w) {
        return;
    }
    [w setStyleMask:StyleMask(titled, resizable, utility, borderless)];
    if (borderless || !titled) {
        [w setHasShadow:YES];
        [w setMovableByWindowBackground:NO];
    }
    if (borderless && wantsKey) {
        InstallKeyOverrides();
        objc_setAssociatedObject(w, &gWantsKey, @YES, OBJC_ASSOCIATION_RETAIN);
        [w makeFirstResponder:w.contentView];
        EnsureKeyMonitor();
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

// -dbg-control. keyCode 0 is the letter a; anything else is a bare virtual key.
void ToolWinNativeInjectKey(gp::Window* gw, int keyCode) {
    NSWindow* w = NsWin(gw);
    if (!w) {
        return;
    }
    [w makeFirstResponder:w.contentView];
    NSString* chars = keyCode == 0 ? @"a" : @"";
    auto event = ^(NSEventType type, NSString* text) {
      return [NSEvent keyEventWithType:type
                              location:NSZeroPoint
                         modifierFlags:0
                             timestamp:[NSProcessInfo processInfo].systemUptime
                          windowNumber:w.windowNumber
                               context:nil
                            characters:text
           charactersIgnoringModifiers:chars
                             isARepeat:NO
                               keyCode:(unsigned short)keyCode];
    };
    [NSApp sendEvent:event(NSEventTypeKeyDown, chars)];
    [NSApp sendEvent:event(NSEventTypeKeyUp, @"")];
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
        if (WindowWantsKey(w)) {
            [w makeFirstResponder:w.contentView];
        }
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

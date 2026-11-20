/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"

// AppKit's MacTypes and the `defer:` parameter collide with ours
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#pragma push_macro("defer")
#undef defer
#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>
#pragma pop_macro("defer")
#undef BOOL
#include "base/MacTypesShow.h"

#include "base/UITask.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "gui/AppShell.h"

// Backing pixels per point. The canvas is drawn in points, so a tile rendered
// at GetZoomReal is upscaled on a Retina display.
float AppShellRenderScale(gpui::Window* win) {
    NSView* view = (__bridge NSView*)gp::PlatWindowHandle(win);
    if (!view || !view.window) {
        return 1;
    }
    float scale = (float)view.window.backingScaleFactor;
    if (!(scale > 0)) {
        return 1;
    }
    return scale;
}

static StrVec gOpenDocs;
static bool gOpenDocsReady = false;

// the window the user last worked in
static MainWindow* KeyMainWindow() {
    NSWindow* key = [NSApp keyWindow] ?: [NSApp mainWindow];
    for (MainWindow* win : gWindows) {
        NSView* view = (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin);
        if (key && view.window == key) {
            return win;
        }
    }
    return len(gWindows) > 0 ? gWindows[0] : CreateAndShowMainWindow(nullptr);
}

static void OpenQueuedDocs() {
    int n = len(gOpenDocs);
    MainWindow* win = n > 0 ? KeyMainWindow() : nullptr;
    if (!win) {
        return;
    }
    Vec<Str> paths;
    for (Str path : gOpenDocs) {
        VecAppend(paths, path);
    }
    OpenDroppedFiles(win, paths.els, n);
    gOpenDocs.Reset();
    AppShellActivateWindow(win);
}

static void OnOpenUrls(gp::App*, const gp::Str* urls, int n) {
    for (int i = 0; i < n; i++) {
        if (!gp::StrStartsWith(urls[i], "file://")) {
            continue;
        }
        gOpenDocs.Append(FromGpui(wry::PathFromFileUriTemp(urls[i])));
    }
    if (gOpenDocsReady) {
        uitask::Post(MkFunc0Void(OpenQueuedDocs), "OpenQueuedDocs");
    }
}

// Finder's "Open With" and `open -a` deliver documents through gpui's
// on_open_urls, not in argv. Those arriving before the first window exists
// wait in gOpenDocs.
void AppShellHandleOpenDocs(gp::App* app) {
    gp::AppOnOpenUrls(app, OnOpenUrls);
}

void AppShellOpenDocsReady() {
    gOpenDocsReady = true;
    OpenQueuedDocs();
}

// orig's HwndSetFocus(hwndFrame) from one of its tool windows: the frame gets
// the keys again. Only inside the app: it does not take the focus from another one
void AppShellMakeKeyWindow(MainWindow* win) {
    NSView* view = win ? (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin) : nil;
    if (view.window && NSApp.isActive && !view.window.isKeyWindow) {
        [view.window makeKeyWindow];
    }
}

//--- TestCaptureWindow: the app draws its own windows, so no Screen Recording permission is needed

constexpr double kWebSnapshotTimeoutSec = 5;

static void CollectWebViews(NSView* view, NSMutableArray<WKWebView*>* out) {
    for (NSView* sub in view.subviews) {
        if ([sub isKindOfClass:[WKWebView class]]) {
            [out addObject:(WKWebView*)sub];
            continue;
        }
        CollectWebViews(sub, out);
    }
}

// the view as it is on screen, at its backing scale; drawRect: runs into the
// bitmap. Its web views (and where they are on screen) go to `webs` / `webRects`
static NSBitmapImageRep* CaptureView(NSView* view, NSMutableArray<WKWebView*>* webs, NSMutableArray<NSValue*>* webRects) {
    NSRect b = view.bounds;
    if (b.size.width <= 0 || b.size.height <= 0) {
        return nil;
    }
    NSBitmapImageRep* rep = [view bitmapImageRepForCachingDisplayInRect:b];
    if (!rep) {
        return nil;
    }
    [view cacheDisplayInRect:b toBitmapImageRep:rep];
    NSMutableArray<WKWebView*>* found = [NSMutableArray array];
    CollectWebViews(view, found);
    for (WKWebView* web in found) {
        if (web.hiddenOrHasHiddenAncestor || web.bounds.size.width <= 0 || web.bounds.size.height <= 0) {
            continue;
        }
        [webs addObject:web];
        [webRects addObject:[NSValue valueWithRect:[web.window convertRectToScreen:[web convertRect:web.bounds
                                                                                               toView:nil]]]];
    }
    return rep;
}

static bool WritePng(NSBitmapImageRep* rep, NSString* dst) {
    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return png && [png writeToFile:dst atomically:YES];
}

// A WKWebView renders out of process and is blank in cacheDisplayInRect:, so
// its own snapshot is drawn over it. Snapshots arrive on the main queue after
// this request returns: the PNG is written once the last one is in.
static void DrawWebViewsThenWrite(NSBitmapImageRep* out, NSPoint outOrigin, NSArray<WKWebView*>* webs,
                                  NSArray<NSValue*>* webRects, NSString* dst) {
    __block NSUInteger left = webs.count;
    __block bool written = false;
    void (^finish)() = ^{
        if (!written) {
            written = true;
            WritePng(out, dst);
        }
    };
    for (NSUInteger i = 0; i < webs.count; i++) {
        NSRect r = webRects[i].rectValue;
        r.origin.x -= outOrigin.x;
        r.origin.y -= outOrigin.y;
        [webs[i] takeSnapshotWithConfiguration:nil
                             completionHandler:^(NSImage* img, NSError*) {
                                 if (img && !written) {
                                     NSGraphicsContext* gc = [NSGraphicsContext graphicsContextWithBitmapImageRep:out];
                                     [NSGraphicsContext saveGraphicsState];
                                     [NSGraphicsContext setCurrentContext:gc];
                                     [img drawInRect:r
                                            fromRect:NSZeroRect
                                           operation:NSCompositingOperationSourceOver
                                            fraction:1];
                                     [NSGraphicsContext restoreGraphicsState];
                                 }
                                 if (--left == 0) {
                                     finish();
                                 }
                             }];
    }
    // a snapshot that never comes still leaves a file
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kWebSnapshotTimeoutSec * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), finish);
}

// the windows to capture, back to front: `win`'s own, then with mode "all" the
// app's other visible windows (tool windows, the Measure window, ...)
static NSArray<NSWindow*>* WindowsToCapture(NSWindow* main, bool all) {
    if (!all) {
        return @[ main ];
    }
    NSMutableArray<NSWindow*>* out = [NSMutableArray array];
    for (NSWindow* w in [[NSApp orderedWindows] reverseObjectEnumerator]) {
        if (!w.isVisible || !w.contentView || w.isMiniaturized) {
            continue;
        }
        if (w != main && w.parentWindow != main && w.level < NSNormalWindowLevel) {
            continue;
        }
        [out addObject:w];
    }
    if (![out containsObject:main]) {
        [out insertObject:main atIndex:0];
    }
    return out;
}

// Writes a PNG of the content of `win` (mode "" or "main") or of it and the
// app's other windows composed where they are on screen (mode "all").
TempStr AppShellCaptureWindowTemp(MainWindow* win, Str path, Str mode) {
    NSView* mainView = win ? (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin) : nil;
    if (!mainView || !mainView.window) {
        return StrL("ERR no-window");
    }
    @autoreleasepool {
        bool all = str::Eq(mode, StrL("all"));
        NSArray<NSWindow*>* wins = WindowsToCapture(mainView.window, all);
        CGFloat scale = mainView.window.backingScaleFactor;
        if (!(scale > 0)) {
            scale = 1;
        }

        // each content view and where it is on screen (points, bottom-up)
        NSMutableArray<NSBitmapImageRep*>* reps = [NSMutableArray array];
        NSMutableArray<NSValue*>* rects = [NSMutableArray array];
        NSRect total = NSZeroRect;
        NSMutableArray<WKWebView*>* webs = [NSMutableArray array];
        NSMutableArray<NSValue*>* webRects = [NSMutableArray array];
        for (NSWindow* w in wins) {
            // another window with its title bar: the frame view draws it
            NSView* v = w == mainView.window ? mainView : (w.contentView.superview ?: w.contentView);
            NSBitmapImageRep* rep = CaptureView(v, webs, webRects);
            if (!rep) {
                continue;
            }
            NSRect r = [w convertRectToScreen:[v convertRect:v.bounds toView:nil]];
            [reps addObject:rep];
            [rects addObject:[NSValue valueWithRect:r]];
            total = reps.count == 1 ? r : NSUnionRect(total, r);
        }
        if (reps.count == 0) {
            return StrL("ERR empty");
        }

        NSBitmapImageRep* out = reps[0];
        if (reps.count > 1) {
            NSInteger pxW = (NSInteger)ceil(total.size.width * scale);
            NSInteger pxH = (NSInteger)ceil(total.size.height * scale);
            out = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                          pixelsWide:pxW
                                                          pixelsHigh:pxH
                                                       bitsPerSample:8
                                                     samplesPerPixel:4
                                                            hasAlpha:YES
                                                            isPlanar:NO
                                                      colorSpaceName:NSCalibratedRGBColorSpace
                                                         bytesPerRow:0
                                                        bitsPerPixel:0];
            out.size = total.size;
            NSGraphicsContext* gc = [NSGraphicsContext graphicsContextWithBitmapImageRep:out];
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:gc];
            for (NSUInteger i = 0; i < reps.count; i++) {
                NSRect r = rects[i].rectValue;
                r.origin.x -= total.origin.x;
                r.origin.y -= total.origin.y;
                [reps[i] drawInRect:r];
            }
            [NSGraphicsContext restoreGraphicsState];
        }

        NSString* dst = [NSString stringWithUTF8String:CStrTemp(path)];
        [[NSFileManager defaultManager] removeItemAtPath:dst error:nil];
        int nWeb = (int)webs.count;
        if (nWeb > 0) {
            NSPoint origin = reps.count > 1 ? total.origin : rects[0].rectValue.origin;
            DrawWebViewsThenWrite(out, origin, webs, webRects, dst);
        } else if (!WritePng(out, dst)) {
            return StrL("ERR write");
        }
        // with web views the file appears once their snapshots are in
        return fmt("OK w=%d h=%d scale=%g windows=%d webviews=%d pending=%d", (int)out.pixelsWide,
                   (int)out.pixelsHigh, (double)scale, (int)reps.count, nWeb, nWeb > 0 ? 1 : 0);
    }
}

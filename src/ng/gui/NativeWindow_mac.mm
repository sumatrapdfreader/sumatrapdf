/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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

#include "base/UITask.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "gui/AppShell.h"

// AppKit turns automatic termination back on at the end of launch when it
// still counts zero windows. Under lldb the app never becomes frontmost, so
// that quits the process with status 0.
void AppShellDisableAutoTermination() {
    NSProcessInfo* info = [NSProcessInfo processInfo];
    info.automaticTerminationSupportEnabled = NO;
    [info disableAutomaticTermination:@"SumatraPDF"];
}

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

@interface SumatraOpenDocs : NSObject
@end

@implementation SumatraOpenDocs
- (void)openDocs:(NSAppleEventDescriptor*)event withReply:(NSAppleEventDescriptor*)reply {
    (void)reply;
    NSAppleEventDescriptor* docs = [[event paramDescriptorForKeyword:keyDirectObject] coerceToDescriptorType:typeAEList];
    for (NSInteger i = 1; i <= docs.numberOfItems; i++) {
        NSString* path = [[docs descriptorAtIndex:i] fileURLValue].path;
        if (path) {
            gOpenDocs.Append(Str((char*)path.UTF8String));
        }
    }
    if (gOpenDocsReady) {
        uitask::Post(MkFunc0Void(OpenQueuedDocs), "OpenQueuedDocs");
    }
}
@end

// Finder's "Open With" and `open -a` deliver documents as an Apple event, not
// in argv. Those arriving before the first window exists wait in gOpenDocs.
void AppShellHandleOpenDocs() {
    static SumatraOpenDocs* handler = [[SumatraOpenDocs alloc] init];
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:handler
                                                       andSelector:@selector(openDocs:withReply:)
                                                     forEventClass:kCoreEventClass
                                                        andEventID:kAEOpenDocuments];
}

void AppShellOpenDocsReady() {
    gOpenDocsReady = true;
    OpenQueuedDocs();
}

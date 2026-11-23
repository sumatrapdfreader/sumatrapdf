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

// Tests pass HWND_TOPMOST. Floating level stays above ordinary windows.
// FullScreenPrimary excludes FullScreenAuxiliary, so clear it or the frame
// stays off the space a drop is tested against.
void AppShellRaiseAbove(MainWindow* win) {
    NSView* view = win && win->gpuiWin ? (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin) : nil;
    NSWindow* w = view.window;
    if (!w) {
        return;
    }
    NSWindowCollectionBehavior behavior = w.collectionBehavior;
    behavior &= ~NSWindowCollectionBehaviorFullScreenPrimary;
    behavior &= ~NSWindowCollectionBehaviorFullScreenNone;
    behavior &= ~NSWindowCollectionBehaviorPrimary;
    behavior &= ~NSWindowCollectionBehaviorAuxiliary;
    behavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
    behavior &= ~NSWindowCollectionBehaviorManaged;
    behavior &= ~NSWindowCollectionBehaviorTransient;
    behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces;
    behavior |= NSWindowCollectionBehaviorStationary;
    behavior |= NSWindowCollectionBehaviorIgnoresCycle;
    behavior |= NSWindowCollectionBehaviorFullScreenAuxiliary;
    behavior |= NSWindowCollectionBehaviorCanJoinAllApplications;
    w.collectionBehavior = behavior;
    w.level = NSFloatingWindowLevel;
    [w orderFrontRegardless];
}

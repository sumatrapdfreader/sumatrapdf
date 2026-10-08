/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"

#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#undef defer
#import <Cocoa/Cocoa.h>
#undef BOOL
#include "base/MacTypesShow.h"

#include "gui/AppShell.h"

float AppShellRenderScale(gp::Window* win) {
    NSView* view = (__bridge NSView*)gp::PlatWindowHandle(win);
    return view.window ? (float)view.window.backingScaleFactor : 1;
}

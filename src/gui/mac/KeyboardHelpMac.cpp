/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "KeyboardHelp.h"
#include "gui/mac/KeyboardHelpMacBridge.h"

// The early Mac app has no notification system yet. Commands.cpp uses this for
// malformed custom-command diagnostics, which the built-in help data never creates.
void MaybeDelayedWarningNotification(Str) {}

void MacToggleKeyboardHelp(void* parent, bool parentFullscreen) {
    KeyboardHelpArgs args;
    args.parent = parent;
    args.parentFullscreen = parentFullscreen;
    args.dataSource = GetDefaultKeyboardHelpDataSource();
    ToggleKeyboardHelp(args);
}

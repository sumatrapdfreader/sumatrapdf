/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/PlatformWindow.h"

#include "KeyboardHelp.h"

#include <gtk/gtk.h>

int main(int, char**) {
    gtk_init();
    KeyboardHelpArgs args;
    args.dataSource = GetDefaultKeyboardHelpDataSource();
    ToggleKeyboardHelp(args);
    while (IsKeyboardHelpVisible()) {
        g_main_context_iteration(nullptr, TRUE);
    }
    return 0;
}

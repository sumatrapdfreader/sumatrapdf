/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the Ctrl+Tab tab list. orig runs the command palette in smart-tab mode,
// and so does this: everything here forwards to src/CommandPalette.cpp, which
// owns the tab list, its TabsMru order and the Ctrl-release behaviour.

#include "base/Base.h"

#include "Commands.h"
#include "CommandPalette.h"
#include "gui/TabSwitcher.h"

void TabSwitcherStart(MainWindow* win, int advance) {
    RunCommandPalette(win, Str(kPalettePrefixTabs), advance);
}

WindowTab* TabSwitcherHighlighted(MainWindow* win) {
    return CommandPaletteHighlightedTab(win);
}

void TabSwitcherDelete(MainWindow* win) {
    if (CommandPaletteWindow() == win) {
        CloseCommandPalette();
    }
}

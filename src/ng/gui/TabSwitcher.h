/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Ctrl+Tab opens the command palette in "smart tab" mode
// (RunCommandPalette(win, kPalettePrefixTabs, advance)), and so does this:
// the three functions below forward to src/CommandPalette.cpp. Step 10a had a
// separate list here because the palette was not ported yet; step 11b made it
// the palette's tab half.

struct MainWindow;
struct WindowTab;

// Ctrl+Tab / Ctrl+Shift+Tab: open the list, or step through it when it is open
void TabSwitcherStart(MainWindow* win, int advance);
// the tab the list currently points at, so the strip can preview it (orig's
// TabsCtrl::SetHighlighted)
WindowTab* TabSwitcherHighlighted(MainWindow* win);
void TabSwitcherDelete(MainWindow* win);

/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the gpui tab strip. orig's is a win32 control (gui/win/TabsCtrl.cpp)
// with a TabInfo per tab; here the strip paints straight from the WindowTab
// list, so there is no second copy of the tab state. The behaviour is orig's:
// fixed-width tabs, hover, the ✕ on the selected tab, middle-click close,
// drag reorder and the tab context menu.

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct TabsUI;

// barDy: the strip is taller in the caption (orig gives it 2 px more there)
gpui::El* TabsUIBuild(MainWindow*, gpui::Ctx*, int barDy);
// the menu button alone, for a caption that has no tab strip
gpui::El* TabsUIMenuButton(MainWindow*, gpui::Ctx*);
void TabsUIDelete(MainWindow*);
// tabs were added, removed or reordered: drop the hover / drag state
void TabsUIOnTabsChanged(MainWindow*);

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

enum class SelToolbarShow {
    Now,
    Settled,
};
void ShowSelectionToolbar(MainWindow* win, SelToolbarShow when);
void SelectionToolbarOnShowTimer(MainWindow* win, int elapsedMs);
void UpdateSelectionToolbarPosition(MainWindow* win);
void HideSelectionToolbar(MainWindow* win);
void ResetSelectionToolbarDismissed(MainWindow* win);
void DeleteSelectionToolbar(MainWindow* win);
bool IsSelectionToolbarVisible(MainWindow* win);
// the floating card, absolutely positioned over the canvas; null when hidden
gpui::El* SelectionToolbarBuild(MainWindow* win, gpui::Ctx* cx);
TempStr SelectionToolbarLayoutDumpTemp(MainWindow* win);

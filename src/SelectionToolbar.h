/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

// Now: the gesture ended (mouse released); Settled: wait for the selection to
// stop changing first (keyboard nudges, repaints)
enum class SelToolbarShow {
    Now,
    Settled
};
void ShowSelectionToolbar(MainWindow* win, SelToolbarShow when);
void SelectionToolbarOnShowTimer(MainWindow* win);
void UpdateSelectionToolbarPosition(MainWindow* win);
void RepositionSelectionToolbar(MainWindow* win);
void HideSelectionToolbar(MainWindow* win);
void ResetSelectionToolbarDismissed(MainWindow* win);
void RefreshSelectionToolbarIcons(MainWindow* win);
void DeleteSelectionToolbar(MainWindow* win);
TempStr SelectionToolbarLayoutDumpTemp();
TempStr SelectionToolbarClickTemp(Str cmdName, int* exitCodeOut);

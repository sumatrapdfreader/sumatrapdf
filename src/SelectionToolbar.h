/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowSelectionToolbar(MainWindow* win);
void SelectionToolbarOnShowTimer(MainWindow* win);
void UpdateSelectionToolbarPosition(MainWindow* win);
void RepositionSelectionToolbar(MainWindow* win);
void HideSelectionToolbar(MainWindow* win);
void RefreshSelectionToolbarIcons(MainWindow* win);
void DeleteSelectionToolbar(MainWindow* win);
TempStr SelectionToolbarLayoutDumpTemp();
TempStr SelectionToolbarClickTemp(Str cmdName, int* exitCodeOut);

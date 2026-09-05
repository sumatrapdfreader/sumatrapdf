/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void DeleteAnnotFilterToolbar(MainWindow*);
void UpdateAnnotFilterToolbar(MainWindow*);
void RefreshAnnotFilterAnnotations(MainWindow*);
void ClearAnnotFilterAnnotations(MainWindow*);
void ToggleFloatingAnnotList(MainWindow*);
bool IsFloatingAnnotListVisible(MainWindow*);
TempStr AnnotFilterToolbarStateTemp(MainWindow*);
void ApplyAnnotFilterText(MainWindow* win, Str text);
void PaintAnnotFilterWindow(MainWindow* win);

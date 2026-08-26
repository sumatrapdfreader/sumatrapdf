/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void UpdateAnnotEditToolbar(MainWindow*);
void HideAnnotEditToolbar(MainWindow*);
void RepositionAnnotEditToolbar(MainWindow*);
void RefreshAnnotEditToolbar(MainWindow*);
void DeleteAnnotEditToolbar(MainWindow*);
TempStr AnnotEditToolbarStateTemp(MainWindow*);

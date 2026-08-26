/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct PlatformFont;
struct Edit;

Edit* CreateAnnotFilterEdit(MainWindow*, PlatformFont*, int iconDy);
void UnbindAnnotFilterEdit(MainWindow*);
void DeleteAnnotFilterToolbar(MainWindow*);
void HideAnnotFilterList(MainWindow*);
void UpdateAnnotFilterToolbar(MainWindow*);
void RefreshAnnotFilterAnnotations(MainWindow*);
void RepositionAnnotFilterList(MainWindow*);
void SetAnnotFilterEditVisible(MainWindow*, bool);
bool AnnotFilterListContainsScreenPoint(MainWindow*, Point);
TempStr AnnotFilterToolbarStateTemp(MainWindow*);

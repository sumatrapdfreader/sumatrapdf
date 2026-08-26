/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct PlatformFont;
struct Edit;
struct VirtIconButton;

Edit* CreateAnnotFilterEdit(MainWindow*, PlatformFont*, int iconDy);
VirtIconButton* CreateAnnotFilterFloatBtn(MainWindow*, int iconSize, int padY, int padX);
void UnbindAnnotFilterEdit(MainWindow*);
void DeleteAnnotFilterToolbar(MainWindow*);
void HideAnnotFilterList(MainWindow*);
void UpdateAnnotFilterToolbar(MainWindow*);
void RefreshAnnotFilterAnnotations(MainWindow*);
void RepositionAnnotFilterList(MainWindow*);
void SetAnnotFilterEditVisible(MainWindow*, bool);
void ToggleFloatingAnnotList(MainWindow*);
bool AnnotFilterListContainsScreenPoint(MainWindow*, Point);
TempStr AnnotFilterToolbarStateTemp(MainWindow*);

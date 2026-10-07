/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

void DeleteAnnotFilterToolbar(MainWindow*);
void UpdateAnnotFilterToolbar(MainWindow*);
void RefreshAnnotFilterAnnotations(MainWindow*);
void ClearAnnotFilterAnnotations(MainWindow*);
void ToggleFloatingAnnotList(MainWindow*);
bool IsFloatingAnnotListVisible(MainWindow*);
void ApplyAnnotFilterText(MainWindow* win, Str text);
void PaintAnnotFilterWindow(MainWindow* win);
bool AnnotFilterOnKeyDown(MainWindow*, int vk, bool ctrl, bool shift, bool alt);
void AnnotFilterOnMouseDown(MainWindow*, float x, float y);
bool AnnotFilterOnEscape(MainWindow*);
void AnnotFilterTestAction(MainWindow*, Str action, int arg, int mods);
void AnnotFilterTick(MainWindow*, int elapsedMs);
gpui::El* AnnotFilterListBuild(MainWindow*, gpui::Ctx*);
TempStr AnnotFilterToolbarStateTemp(MainWindow*);

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
// the keys the list takes while it has the keyboard
bool AnnotFilterOnKeyDown(MainWindow*, int vk, bool ctrl, bool shift, bool alt);
void AnnotFilterOnMouseDown(MainWindow*, float x, float y);
bool AnnotFilterOnEscape(MainWindow*);
void AnnotFilterTestAction(MainWindow*, Str action, int arg, int mods);
// ng: the debounced "the row the caret is on becomes the page's selection";
// orig runs it on a WM_TIMER
void AnnotFilterTick(MainWindow*, int elapsedMs);
gpui::El* AnnotFilterListBuild(MainWindow*, gpui::Ctx*);
TempStr AnnotFilterToolbarStateTemp(MainWindow*);

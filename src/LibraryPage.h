/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct MainWindow;

constexpr const char* kLinkLibraryPrefix = "<Library,";
constexpr int kLibraryServicePortDefault = 7863;

bool LibraryHomeEnabled();
void SetLibraryHomeEnabled(bool enabled);
bool LibraryHasBooks();

void DrawLibraryPage(MainWindow* win, HDC hdc);
bool LibraryOnLinkClicked(MainWindow* win, Str url);
bool LibraryOnRightClick(MainWindow* win, int x, int y);
bool LibraryOnLeftButtonDown(MainWindow* win, int x, int y);
bool LibraryOnMouseMove(MainWindow* win, int x, int y);
bool LibraryOnLeftButtonUp(MainWindow* win);
void LibraryOnCaptureLost(MainWindow* win);
void LibraryOnMouseWheel(MainWindow* win, int delta, int screenX, int screenY);
void LibraryOnVScroll(MainWindow* win, WPARAM wp);

void LibraryFreeCache();
void LibraryRefresh(MainWindow* win, bool rescan);

bool LibraryEnsureService();
int LibraryServicePort();

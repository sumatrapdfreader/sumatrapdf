/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the gpui side of orig's navigate-files-in-folder picker
// (NavFilesInFolder.cpp's window half): a panel at the right edge of the
// frame. The listing itself is the model half, in NavFilesInFolder.cpp.

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

enum class NavKeyResult {
    NotHandled,
    Handled,
    // the key types a character and the filter box was focused to receive it
    Typed,
};

bool IsNavFilesInFolderVisible();
void CloseNavFilesInFolder();
void NavFilesReapClosedWindow();
int NavFilesPanelDx(MainWindow* win);
// the keys the list answers while the picker has the keyboard
NavKeyResult NavFilesOnKeyDown(MainWindow* win, int vk, bool ctrl, bool alt);
void NavFilesOnMouseDown(MainWindow* win, float x, float y);
gpui::El* NavFilesUIBuild(MainWindow* win, gpui::Ctx* cx);

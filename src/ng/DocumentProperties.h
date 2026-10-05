/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct DocController;

// ng: orig's properties window is a top-level HWND per MainWindow, so its API
// is keyed by the parent HWND. Here it is a gpui Dialog inside the window.
void ShowProperties(MainWindow* win, DocController* ctrl);
void DeletePropertiesWindow(MainWindow* win);
bool IsPropertiesDialogVisible();
gpui::El* PropertiesDialogBuild(MainWindow* win, gpui::Ctx* cx);

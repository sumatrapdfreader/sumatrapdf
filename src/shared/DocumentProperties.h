/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct DocController;
struct PropertiesWnd;

void ShowProperties(HWND parent, DocController* ctrl);
void ShowProperties(MainWindow* win, DocController* ctrl);
void DeletePropertiesWindow(HWND hwndParent);
void DeletePropertiesWindow(MainWindow* win);
PropertiesWnd* FindPropertyWindowByHwnd(HWND hwnd);
bool IsHwndInPropertiesWindow(HWND hwnd);
bool IsPropertiesDialogVisible();
gpui::El* PropertiesDialogBuild(MainWindow* win, gpui::Ctx* cx);
TempStr PropertiesDialogButtonsTemp(int* exitCodeOut);
TempStr PropertiesDialogTextTemp(int* exitCodeOut);

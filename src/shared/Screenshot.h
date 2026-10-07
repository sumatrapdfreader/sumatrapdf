/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

void InitScreenshotHost();
void TakeScreenshots(MainWindow* win);
bool CopySelectionAsImage(MainWindow* win);
TempStr GetScreenshotSaveDirTemp();
void ShowSetScreenshotHotkeyDialog(HWND hwndOwner);
void ShowSetScreenshotHotkeyDialog(MainWindow* win);
void CloseSetScreenshotHotkeyDialog();
bool IsSetScreenshotHotkeyDialogVisible();
gpui::El* SetScreenshotHotkeyDialogBuild(MainWindow* win, gpui::Ctx* cx);
bool SetScreenshotHotkeyOnKey(MainWindow* win, int vk, bool ctrl, bool shift, bool alt);

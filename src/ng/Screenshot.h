/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

// ng: orig's CmdScreenshot captures every visible top-level window off the
// desktop and shows a picker overlay (ScreenshotCapture.cpp, a layered win32
// window). This port has no window to layer over the desktop, so the portable
// half is what it can do through the engines: render the rectangular selection
// - or, with none, the current page - to a PNG under the Screenshots folder
// and put it on the clipboard.
void TakeScreenshots(MainWindow* win);
// orig's CmdCopySelectionAsImage
bool CopySelectionAsImage(MainWindow* win);
TempStr GetScreenshotSaveDirTemp();

void ShowSetScreenshotHotkeyDialog(MainWindow* win);
void CloseSetScreenshotHotkeyDialog();
bool IsSetScreenshotHotkeyDialogVisible();
gpui::El* SetScreenshotHotkeyDialogBuild(MainWindow* win, gpui::Ctx* cx);
// the dialog turns every key into the hotkey it is capturing
bool SetScreenshotHotkeyOnKey(MainWindow* win, int vk, bool ctrl, bool shift, bool alt);

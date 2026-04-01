/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void TakeScreenshots();
void RegisterScreenshotHotkey(HWND hwnd);
void UnregisterScreenshotHotkey(HWND hwnd);

constexpr int kScreenshotHotkeyId = 0x5001;

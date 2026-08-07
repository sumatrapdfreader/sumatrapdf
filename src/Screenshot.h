/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void InitScreenshotHost();
void RegisterScreenshotHotkey(HWND hwnd);
void UnregisterScreenshotHotkey(HWND hwnd);
void ShowSetScreenshotHotkeyDialog(HWND hwndOwner);

constexpr int kScreenshotHotkeyId = 0x5001;

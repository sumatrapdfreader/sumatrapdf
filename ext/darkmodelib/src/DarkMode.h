// MIT license
// Copyright(c) 2024-2025 ozone10

// Parts of code based on the win32-darkmode project
// https://github.com/ysc3839/win32-darkmode
// which is licensed under the MIT License. Copyright (c) 2019 Richard Yu

#pragma once

#include <windows.h>

extern bool g_darkModeSupported;
extern bool g_darkModeEnabled;


[[nodiscard]] bool ShouldAppsUseDarkMode();
bool AllowDarkModeForWindow(HWND hWnd, bool allow);
[[nodiscard]] bool IsHighContrast();
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
void RefreshTitleBarThemeColor(HWND hWnd);
void SetTitleBarThemeColor(HWND hWnd, BOOL dark);
#endif
[[nodiscard]] bool IsColorSchemeChangeMessage(LPARAM lParam);
[[nodiscard]] bool IsColorSchemeChangeMessage(UINT uMsg, LPARAM lParam);
void AllowDarkModeForApp(bool allow);
void EnableDarkScrollBarForWindowAndChildren(HWND hWnd);
void InitDarkMode();
void SetDarkMode(bool useDarkMode, bool fixDarkScrollbar);
[[nodiscard]] bool IsWindows10();
[[nodiscard]] bool IsWindows11();
[[nodiscard]] DWORD GetWindowsBuildNumber();

void SetMySysColor(int nIndex, COLORREF clr);
bool HookSysColor();
void UnhookSysColor();

/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class CursorId;

LPWSTR ToWin32Cursor(CursorId);

using NativeWnd = HWND;

Rect PlatformWindowRect(NativeWnd);
Rect PlatformWindowWorkArea(NativeWnd);
bool PlatformWindowIsMaximized(NativeWnd);
void PlatformWindowActivateIfForeground(NativeWnd);

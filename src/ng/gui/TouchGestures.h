/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's WM_GESTURE handling (gui/TouchGestures_win.cpp). Windows only.

#if OS_WIN
// WM_GESTURENOTIFY / WM_GESTURE of the frame; true when the message was
// handled and must not go on to DefWindowProc
bool TouchGesturesOnMessage(MainWindow* win, HWND hwnd, UINT msg, LPARAM lp);
void TouchGesturesForget(MainWindow* win);
#endif

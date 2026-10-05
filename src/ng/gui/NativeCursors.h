/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's cursors that are bitmaps (gui/NativeCursors_win.cpp). Windows only.

#if OS_WIN
HCURSOR NativeCursorGet(NativeCursor cursor);
void NativeCursorsDelete();
// for the automation channel: what WM_SETCURSOR answers for `win` right now,
// and the cursor drawn into a 32-bit .bmp at `bmpPath` (a window capture has
// no cursor in it)
TempStr NativeCursorTestTemp(MainWindow* win, Str bmpPath);
#endif

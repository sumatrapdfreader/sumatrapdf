/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's cursors that are bitmaps (gui/NativeCursors_win.cpp). Windows only.

#if OS_WIN
HCURSOR NativeCursorGet(NativeCursor cursor);
void NativeCursorsDelete();
#endif

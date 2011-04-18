/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraStart_h
#define SumatraStart_h

#define THUMBNAILS_DIR_NAME _T("sumatrapdfcache")

LRESULT HandleWindowStartMsg(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled);
void    LoadThumbnails(FileHistory& fileHistory);
void    SaveThumbnail(DisplayState *state);

#endif

/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraStart_h
#define SumatraStart_h

#define THUMBNAILS_DIR_NAME _T("sumatrapdfcache")
// thumbnails are 150px high and have a ratio of sqrt(2) : 1
#define THUMBNAIL_DX        212
#define THUMBNAIL_DY        150

LRESULT HandleWindowStartMsg(WindowInfo *win, FileHistory& fileHistory, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled);
void    LoadThumbnails(FileHistory& fileHistory);
void    SaveThumbnail(DisplayState *state);

#endif

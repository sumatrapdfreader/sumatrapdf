/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ComicEngine_h
#define ComicEngine_h

#include "BaseUtil.h"

class WindowInfo;

bool        IsComicBook(const TCHAR *fileName);
WindowInfo *LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin);

#endif

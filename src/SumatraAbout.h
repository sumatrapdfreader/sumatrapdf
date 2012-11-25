/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraAbout_h
#define SumatraAbout_h

#include "DisplayState.h"
#include "FileHistory.h"
#include "WindowInfo.h" // for StaticLinkInfo

void OnMenuAbout();

void  DrawAboutPage(WindowInfo& win, HDC hdc);

const WCHAR *GetStaticLink(Vec<StaticLinkInfo>& linkInfo, int x, int y, StaticLinkInfo *info=NULL);

#define SLINK_OPEN_FILE L"<File,Open>"
#define SLINK_LIST_SHOW L"<View,ShowList>"
#define SLINK_LIST_HIDE L"<View,HideList>"

#define THUMBNAILS_DIR_NAME L"sumatrapdfcache"
// thumbnails are 150px high and have a ratio of sqrt(2) : 1
#define THUMBNAIL_DX        212
#define THUMBNAIL_DY        150

void    DrawStartPage(WindowInfo& win, HDC hdc, FileHistory& fileHistory, COLORREF colorRange[2]);
void    CleanUpThumbnailCache(FileHistory& fileHistory);
bool    HasThumbnail(DisplayState& ds);
void    SaveThumbnail(DisplayState& ds);
void    RemoveThumbnail(DisplayState& ds);

#endif

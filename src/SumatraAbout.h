/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraAbout_h
#define SumatraAbout_h

#define ABOUT_CLASS_NAME        _T("SUMATRA_PDF_ABOUT")

void OnMenuAbout();
LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void  DrawAboutPage(WindowInfo& win, HDC hdc);

const TCHAR *GetStaticLink(Vec<StaticLinkInfo>& linkInfo, int x, int y, StaticLinkInfo *info=NULL);

#define SLINK_OPEN_FILE _T("<File,Open>")
#define SLINK_LIST_SHOW _T("<View,ShowList>")
#define SLINK_LIST_HIDE _T("<View,HideList>")

#define THUMBNAILS_DIR_NAME _T("sumatrapdfcache")
// thumbnails are 150px high and have a ratio of sqrt(2) : 1
#define THUMBNAIL_DX        212
#define THUMBNAIL_DY        150

void    DrawStartPage(WindowInfo& win, HDC hdc, FileHistory& fileHistory, bool invertColors);
void    CleanUpThumbnailCache(FileHistory& fileHistory);
bool    HasThumbnail(DisplayState& ds);
void    SaveThumbnail(DisplayState& ds);

#endif

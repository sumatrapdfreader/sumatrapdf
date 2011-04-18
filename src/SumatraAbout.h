/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraAbout_h
#define SumatraAbout_h

// define the following to display a list of recently used files
// instead of the About screen, when no document is loaded
#define NEW_START_PAGE

#define ABOUT_CLASS_NAME        _T("SUMATRA_PDF_ABOUT")

LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT          HandleWindowAboutMsg(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled);

SizeI CalcSumatraVersionSize(HDC);
void  DrawSumatraVersion(HDC hdc, RectI rect);
RectI DrawBottomRightLink(HWND hwnd, HDC hdc, const TCHAR *txt);

void OnMenuAbout();

#endif

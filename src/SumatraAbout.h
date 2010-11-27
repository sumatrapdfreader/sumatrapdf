/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SUMATRA_PDF_ABOUT_H_
#define SUMATRA_PDF_ABOUT_H_

#define ABOUT_CLASS_NAME        _T("SUMATRA_PDF_ABOUT")

typedef struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const TCHAR *   leftTxt;
    const TCHAR *   rightTxt;
    const TCHAR *   url;

    /* data calculated by the layout */
    RectI           leftPos;
    RectI           rightPos;
} AboutLayoutInfoEl;

void DrawAbout(HWND hwnd, HDC hdc, RECT *rect);
void OnMenuAbout();
LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
const TCHAR *AboutGetLink(WindowInfo *win, int x, int y, AboutLayoutInfoEl **el_out=NULL);
void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, RECT * rect);

// in SumatraPDF.cpp
void LaunchBrowser(const TCHAR *url);

#endif

/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraAbout_h
#define SumatraAbout_h

#define ABOUT_CLASS_NAME        _T("SUMATRA_PDF_ABOUT")

LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT          HandleWindowAboutMsg(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled);

void OnMenuAbout();

// in SumatraPDF.cpp
void LaunchBrowser(const TCHAR *url);

#endif

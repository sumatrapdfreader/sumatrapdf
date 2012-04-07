/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ExternalPdfViewer_h
#define ExternalPdfViewer_h

class WindowInfo;

bool CanViewExternally(WindowInfo *win=NULL);

bool CanViewWithFoxit(WindowInfo *win=NULL);
bool ViewWithFoxit(WindowInfo *win, TCHAR *args=NULL);
bool CanViewWithPDFXChange(WindowInfo *win=NULL);
bool ViewWithPDFXChange(WindowInfo *win, TCHAR *args=NULL);
bool CanViewWithAcrobat(WindowInfo *win=NULL);
bool ViewWithAcrobat(WindowInfo *win, TCHAR *args=NULL);

bool CanViewWithXPSViewer(WindowInfo *win);
bool ViewWithXPSViewer(WindowInfo *win, TCHAR *args=NULL);

bool CanViewWithHtmlHelp(WindowInfo *win);
bool ViewWithHtmlHelp(WindowInfo *win, TCHAR *args=NULL);

#endif

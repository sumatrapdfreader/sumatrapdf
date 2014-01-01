/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ExternalPdfViewer_h
#define ExternalPdfViewer_h

class WindowInfo;

bool CanViewExternally(WindowInfo *win=NULL);

bool CanViewWithFoxit(WindowInfo *win=NULL);
bool ViewWithFoxit(WindowInfo *win, WCHAR *args=NULL);
bool CanViewWithPDFXChange(WindowInfo *win=NULL);
bool ViewWithPDFXChange(WindowInfo *win, WCHAR *args=NULL);
bool CanViewWithAcrobat(WindowInfo *win=NULL);
bool ViewWithAcrobat(WindowInfo *win, WCHAR *args=NULL);

bool CanViewWithXPSViewer(WindowInfo *win);
bool ViewWithXPSViewer(WindowInfo *win, WCHAR *args=NULL);

bool CanViewWithHtmlHelp(WindowInfo *win);
bool ViewWithHtmlHelp(WindowInfo *win, WCHAR *args=NULL);

bool ViewWithExternalViewer(size_t idx, const WCHAR *filePath, int pageNo=0);

#endif

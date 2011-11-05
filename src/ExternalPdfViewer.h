/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
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

#endif

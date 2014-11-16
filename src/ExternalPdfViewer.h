/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class WindowInfo;

bool CanViewExternally(WindowInfo *win=NULL);
bool CouldBePDFDoc(WindowInfo *win);

bool CanViewWithFoxit(WindowInfo *win=NULL);
bool ViewWithFoxit(WindowInfo *win, const WCHAR *args=NULL);
bool CanViewWithPDFXChange(WindowInfo *win=NULL);
bool ViewWithPDFXChange(WindowInfo *win, const WCHAR *args=NULL);
bool CanViewWithAcrobat(WindowInfo *win=NULL);
bool ViewWithAcrobat(WindowInfo *win, const WCHAR *args=NULL);

bool CanViewWithXPSViewer(WindowInfo *win);
bool ViewWithXPSViewer(WindowInfo *win, const WCHAR *args=NULL);

bool CanViewWithHtmlHelp(WindowInfo *win);
bool ViewWithHtmlHelp(WindowInfo *win, const WCHAR *args=NULL);

bool ViewWithExternalViewer(size_t idx, const WCHAR *filePath, int pageNo=0);

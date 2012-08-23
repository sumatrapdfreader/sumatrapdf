/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Print_h
#define Print_h

class WindowInfo;

bool PrintFile(const TCHAR *fileName, const TCHAR *printerName, bool displayErrors=true, const TCHAR *settings=NULL);
void OnMenuPrint(WindowInfo *win, bool waitForCompletion=false);
void AbortPrinting(WindowInfo *win);

#endif

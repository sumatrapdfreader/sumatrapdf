/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
#ifndef Print_h
#define Print_h

class WindowInfo;

bool PrintFile(const TCHAR *fileName, const TCHAR *printerName, bool displayErrors=true);
void OnMenuPrint(WindowInfo *win);
void AbortPrinting(WindowInfo *win);

#endif


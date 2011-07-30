/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Print_h
#define Print_h

// Undefine, if you prefer MuPDF/Fitz to render the whole page to a bitmap first
// at the expense of higher spooler requirements.
#define USE_GDI_FOR_PRINTING

class WindowInfo;

bool PrintFile(const TCHAR *fileName, const TCHAR *printerName, bool displayErrors=true);
void OnMenuPrint(WindowInfo *win);
void AbortPrinting(WindowInfo *win);

#endif

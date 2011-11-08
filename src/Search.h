/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Search_h
#define Search_h

#include "Vec.h"

#define PDFSYNC_DDE_SERVICE   _T("SUMATRA")
#define PDFSYNC_DDE_TOPIC     _T("control")

// forward-search command
//  format: [ForwardSearch(["<pdffilepath>",]"<sourcefilepath>",<line>,<column>[,<newwindow>, <setfocus>])]
//    if pdffilepath is provided, the file will be opened if no open window can be found for it
//    if newwindow = 1 then a new window is created even if the file is already open
//    if focus = 1 then the focus is set to the window
//  eg: [ForwardSearch("c:\file.pdf","c:\folder\source.tex",298,0)]
#define DDECOMMAND_SYNC       _T("ForwardSearch")

// open file command
//  format: [Open("<pdffilepath>"[,<newwindow>,<setfocus>,<forcerefresh>])]
//    if newwindow = 1 then a new window is created even if the file is already open
//    if focus = 1 then the focus is set to the window
//  eg: [Open("c:\file.pdf", 1, 1, 0)]
#define DDECOMMAND_OPEN       _T("Open")

// jump to named destination command
//  format: [GoToNamedDest("<pdffilepath>","<destination name>")]
//  eg: [GoToNamedDest("c:\file.pdf", "chapter.1")]. pdf file must be already opened
#define DDECOMMAND_GOTO       _T("GotoNamedDest")

// jump to page command
//  format: [GoToPage("<pdffilepath>",<page number>)]
//  eg: [GoToPage("c:\file.pdf", 37)]. pdf file must be already opened
#define DDECOMMAND_PAGE       _T("GotoPage")

// set view mode and zoom level
//  format: [SetView("<pdffilepath>", "<view mode>", <zoom level>[, <scrollX>, <scrollY>])]
//  eg: [SetView("c:\file.pdf", "book view", -2)]
//  note: use -1 for ZOOM_FIT_PAGE, -2 for ZOOM_FIT_WIDTH and -3 for ZOOM_FIT_CONTENT
#define DDECOMMAND_SETVIEW    _T("SetView")

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnDDExecute(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnDDETerminate(HWND hwnd, WPARAM wparam, LPARAM lparam);

#define HIDE_FWDSRCHMARK_TIMER_ID                4
#define HIDE_FWDSRCHMARK_DELAY_IN_MS             400
#define HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS     100
#define HIDE_FWDSRCHMARK_STEPS                   5

class WindowInfo;

bool NeedsFindUI(WindowInfo *win);
void ClearSearchResult(WindowInfo *win);
bool OnInverseSearch(WindowInfo *win, int x, int y);
void ShowForwardSearchResult(WindowInfo *win, const TCHAR *fileName, UINT line, UINT col, UINT ret, UINT page, Vec<RectI> &rects);
void PaintForwardSearchMark(WindowInfo *win, HDC hdc);
void OnMenuFindPrev(WindowInfo *win);
void OnMenuFindNext(WindowInfo *win);
void OnMenuFind(WindowInfo *win);
void OnMenuFindMatchCase(WindowInfo *win);
void AbortFinding(WindowInfo *win, bool hideMessage=false);

#endif

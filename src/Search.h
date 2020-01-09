/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define PDFSYNC_DDE_SERVICE L"SUMATRA"
#define PDFSYNC_DDE_TOPIC L"control"

// forward-search command
//  format: [ForwardSearch(["<pdffilepath>",]"<sourcefilepath>",<line>,<column>[,<newwindow>, <setfocus>])]
//    if pdffilepath is provided, the file will be opened if no open window can be found for it
//    if newwindow = 1 then a new window is created even if the file is already open
//    if focus = 1 then the focus is set to the window
//  eg: [ForwardSearch("c:\file.pdf","c:\folder\source.tex",298,0)]
#define DDECOMMAND_SYNC L"ForwardSearch"

// set view mode and zoom level
//  format: [SetView("<pdffilepath>", "<view mode>", <zoom level>[, <scrollX>, <scrollY>])]
//  eg: [SetView("c:\file.pdf", "book view", -2)]
//  note: use -1 for ZOOM_FIT_PAGE, -2 for ZOOM_FIT_WIDTH and -3 for ZOOM_FIT_CONTENT
#define DDECOMMAND_SETVIEW L"SetView"

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnDDExecute(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnDDETerminate(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnCopyData(HWND hwnd, WPARAM wparam, LPARAM lparam);

#define HIDE_FWDSRCHMARK_TIMER_ID 4
#define HIDE_FWDSRCHMARK_DELAY_IN_MS 400
#define HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS 100
#define HIDE_FWDSRCHMARK_STEPS 5

bool NeedsFindUI(WindowInfo* win);
void ClearSearchResult(WindowInfo* win);
bool OnInverseSearch(WindowInfo* win, int x, int y);
void ShowForwardSearchResult(WindowInfo* win, const WCHAR* fileName, UINT line, UINT col, UINT ret, UINT page,
                             Vec<RectI>& rects);
void PaintForwardSearchMark(WindowInfo* win, HDC hdc);
void OnMenuFindPrev(WindowInfo* win);
void OnMenuFindNext(WindowInfo* win);
void OnMenuFind(WindowInfo* win);
void OnMenuFindMatchCase(WindowInfo* win);
void OnMenuFindSel(WindowInfo* win, TextSearchDirection direction);
void AbortFinding(WindowInfo* win, bool hideMessage);
void FindTextOnThread(WindowInfo* win, TextSearchDirection direction, bool showProgress);

extern bool gIsStartup;
extern WStrVec gDdeOpenOnStartup;

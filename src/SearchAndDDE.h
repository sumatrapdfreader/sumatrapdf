/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define PDFSYNC_DDE_SERVICE L"SUMATRA"
#define PDFSYNC_DDE_TOPIC L"control"

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT OnDDExecute(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT OnDDETerminate(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT OnCopyData(HWND hwnd, WPARAM wp, LPARAM lp);

#define HIDE_FWDSRCHMARK_TIMER_ID 4
#define HIDE_FWDSRCHMARK_DELAY_IN_MS 400
#define HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS 100
#define HIDE_FWDSRCHMARK_STEPS 5

bool NeedsFindUI(WindowInfo* win);
void ClearSearchResult(WindowInfo* win);
bool OnInverseSearch(WindowInfo* win, int x, int y);
void ShowForwardSearchResult(WindowInfo* win, const WCHAR* fileName, uint line, uint col, uint ret, uint page,
                             Vec<Rect>& rects);
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

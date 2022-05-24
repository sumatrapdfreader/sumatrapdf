/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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

bool NeedsFindUI(MainWindow* win);
void ClearSearchResult(MainWindow* win);
bool OnInverseSearch(MainWindow* win, int x, int y);
void ShowForwardSearchResult(MainWindow* win, const char* fileName, uint line, uint col, uint ret, uint page,
                             Vec<Rect>& rects);
void PaintForwardSearchMark(MainWindow* win, HDC hdc);
void OnMenuFindPrev(MainWindow* win);
void OnMenuFindNext(MainWindow* win);
void OnMenuFind(MainWindow* win);
void OnMenuFindMatchCase(MainWindow* win);
void OnMenuFindSel(MainWindow* win, TextSearchDirection direction);
void AbortFinding(MainWindow* win, bool hideMessage);
void FindTextOnThread(MainWindow* win, TextSearchDirection direction, bool showProgress);
void FindTextOnThread(MainWindow* win, TextSearchDirection direction, const char* text, bool wasModified,
                      bool showProgress);
extern bool gIsStartup;
extern StrVec gDdeOpenOnStartup;

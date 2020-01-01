/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define SMOOTHSCROLL_TIMER_ID 2
#define SMOOTHSCROLL_DELAY_IN_MS 20
#define SMOOTHSCROLL_SLOW_DOWN_FACTOR 10

/* Represents selected area on given page */
struct SelectionOnPage {
    explicit SelectionOnPage(int pageNo = 0, RectD* rect = nullptr) : pageNo(pageNo), rect(rect ? *rect : RectD()) {
    }

    int pageNo; // page this selection is on
    RectD rect; // position of selection rectangle on page (in page coordinates)

    // position of selection rectangle in the view port
    RectI GetRect(DisplayModel* dm);

    static Vec<SelectionOnPage>* FromRectangle(DisplayModel* dm, RectI rect);
    static Vec<SelectionOnPage>* FromTextSelect(TextSel* textSel);
};

void DeleteOldSelectionInfo(WindowInfo* win, bool alsoTextSel = false);
void PaintTransparentRectangles(HDC hdc, RectI screenRc, Vec<RectI>& rects, COLORREF selectionColor, BYTE alpha = 0x5f,
                                int margin = 1);
void PaintSelection(WindowInfo* win, HDC hdc);
void UpdateTextSelection(WindowInfo* win, bool select = true);
void ZoomToSelection(WindowInfo* win, float factor, bool scrollToFit = true, bool relative = false);
void CopySelectionToClipboard(WindowInfo* win);
void OnSelectAll(WindowInfo* win, bool textOnly = false);
bool NeedsSelectionEdgeAutoscroll(WindowInfo* win, int x, int y);
void OnSelectionEdgeAutoscroll(WindowInfo* win, int x, int y);
void OnSelectionStart(WindowInfo* win, int x, int y, WPARAM key);
void OnSelectionStop(WindowInfo* win, int x, int y, bool aborted);

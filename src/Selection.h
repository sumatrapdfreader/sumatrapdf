/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define SMOOTHSCROLL_TIMER_ID 2
#define SMOOTHSCROLL_DELAY_IN_MS 20
#define SMOOTHSCROLL_SLOW_DOWN_FACTOR 10

/* Represents selected area on given page */
struct SelectionOnPage {
    explicit SelectionOnPage(int pageNo = 0, RectF* rect = nullptr);

    int pageNo; // page this selection is on
    RectF rect; // position of selection rectangle on page (in page coordinates)

    // position of selection rectangle in the view port
    Rect GetRect(DisplayModel* dm);

    static Vec<SelectionOnPage>* FromRectangle(DisplayModel* dm, Rect rect);
    static Vec<SelectionOnPage>* FromTextSelect(TextSel* textSel);
};

void DeleteOldSelectionInfo(WindowInfo* win, bool alsoTextSel = false);
void PaintTransparentRectangles(HDC hdc, Rect screenRc, Vec<Rect>& rects, COLORREF selectionColor, u8 alpha = 0x5f,
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
WCHAR* GetSelectedText(WindowInfo* win, const WCHAR* lineSep, bool& isTextOnlySelectionOut);

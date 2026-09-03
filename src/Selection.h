/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr int kSelectSmoothScrollTimerID = 2;
constexpr int kSelectSmoothScrollDelayInMs = 20;
constexpr int kSelectSmoothScrollSlowDownFactor = 10;

struct Gfx;
struct WindowTab;
struct DisplayModel;
struct RenderedBitmap;

/* Represents selected area on given page */
struct SelectionOnPage {
    explicit SelectionOnPage(int pageNo = 0, const RectF* rect = nullptr, const QuadF* quad = nullptr);

    int pageNo; // page this selection is on
    RectF rect; // axis-aligned box on the page (toolbar, copy-as-image)
    QuadF quad; // glyph corners; empty means paint rect

    SelectionOnPage(const SelectionOnPage&) = default;
    SelectionOnPage& operator=(const SelectionOnPage&) = default;

    bool HasQuad() const;
    // position of selection rectangle in the view port
    Rect GetRect(DisplayModel* dm) const;

    static Vec<SelectionOnPage>* FromRectangle(DisplayModel* dm, Rect rect);
    static Vec<SelectionOnPage>* FromTextSelect(TextSel* textSel);
};

RenderedBitmap* RenderSelectionsAsRenderedBitmap(DisplayModel* dm, const Vec<SelectionOnPage>& selections);

// default opacity of the selection rectangle when SelectionColor has no alpha
constexpr u8 kSelectionDefaultAlpha = 0x5f;

void DeleteOldSelectionInfo(MainWindow* win, bool alsoTextSel = false);
void RemapSelOnRenumber(MainWindow* win, DisplayModel* dm);
void RemapTextSelection(DisplayModel* dm);
void PaintTransparentRectangles(Gfx* gfx, Rect screenRc, Vec<Rect>& rects, Color selectionColor,
                                u8 alpha = kSelectionDefaultAlpha, int pad = 2, bool drawBorder = false);
void PaintSelection(MainWindow* win, Gfx* gfx);
void UpdateTextSelection(MainWindow* win, bool select = true);
void CopySelectionToClipboard(MainWindow* win);
void OnSelectAll(MainWindow* win, bool textOnly = false);
bool NeedsSelectionEdgeAutoscroll(MainWindow* win, int x, int y);
void OnSelectionEdgeAutoscroll(MainWindow* win, int x, int y);
void OnSelectionStart(MainWindow* win, int x, int y, WPARAM key, bool forceRect = false);
void OnSelectionStop(MainWindow* win, int x, int y, bool aborted);
TempStr GetSelectedTextTemp(WindowTab* tab, Str lineSep, bool& isTextOnlySelectionOut);

// Touch text selection handles (issue #538). Rects are in canvas coordinates.
// Returns false when there is no text selection to put handles on.
bool GetTouchSelHandleRects(MainWindow* win, Rect& startOut, Rect& endOut);
TouchSelHandle HitTestTouchSelHandle(MainWindow* win, int x, int y);
void HideTouchSelHandles(MainWindow* win);

bool IsRectangularSelection(MainWindow* win);
Rect GetRectangularSelectionScreenRect(MainWindow* win);
bool GetSelectionScreenRect(WindowTab* tab, Rect& out);
TempStr FormatSelectionPositionTemp(WindowTab* tab);
SelectionDragEdge HitTestRectangularSelection(MainWindow* win, int x, int y);
LPWSTR CursorIdForSelectionEdge(SelectionDragEdge edge);
bool StartRectangularSelectionEdit(MainWindow* win, int x, int y, SelectionDragEdge edge);
void UpdateRectangularSelectionEdit(MainWindow* win, int x, int y);

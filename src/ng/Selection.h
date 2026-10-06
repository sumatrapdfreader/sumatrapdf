/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct PaintCtx;
}

struct WindowTab;
struct DisplayModel;
struct Pixmap;

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
    static Vec<SelectionOnPage>* FromTextSelect(Vec<TextSel>* textSel);
};

// default opacity of the selection rectangle when SelectionColor has no alpha
constexpr u8 kSelectionDefaultAlpha = 0x5f;

void DeleteOldSelectionInfo(MainWindow* win, bool alsoTextSel = false);
void RemapSelOnRenumber(MainWindow* win, DisplayModel* dm);
void RemapTextSelection(DisplayModel* dm);
void PaintTransparentRectangles(gpui::PaintCtx* ctx, Rect screenRc, Vec<Rect>& rects, Color selectionColor,
                                u8 alpha = kSelectionDefaultAlpha, int pad = 2, bool drawBorder = false);
void PaintSelection(MainWindow* win, gpui::PaintCtx* ctx);
void UpdateTextSelection(MainWindow* win, bool select = true);
void CopySelectionToClipboard(MainWindow* win);
void OnSelectAll(MainWindow* win, bool textOnly = false);
void OnSelectCurrentPage(MainWindow* win);
bool NeedsSelectionEdgeAutoscroll(MainWindow* win, int x, int y);
void OnSelectionEdgeAutoscroll(MainWindow* win, int x, int y);
void OnSelectionStart(MainWindow* win, int x, int y, bool forceRect = false);
void OnSelectionStop(MainWindow* win, int x, int y, bool aborted);
TempStr GetSelectedTextTemp(WindowTab* tab, Str lineSep, bool& isTextOnlySelectionOut);
// ng: orig's RenderSelectionsAsRenderedBitmap returns a win32 RenderedBitmap;
// here the same pixels come back as a Pixmap. Caller frees with FreePixmap().
Pixmap* RenderSelectionsAsPixmap(DisplayModel* dm, const Vec<SelectionOnPage>& selections);

bool IsRectangularSelection(MainWindow* win);
Rect GetRectangularSelectionScreenRect(MainWindow* win);
bool GetSelectionScreenRect(WindowTab* tab, Rect& out);
TempStr FormatSelectionPositionTemp(WindowTab* tab);
SelectionDragEdge HitTestRectangularSelection(MainWindow* win, int x, int y);
int CursorIdForSelectionEdge(SelectionDragEdge edge);
bool StartRectangularSelectionEdit(MainWindow* win, int x, int y, SelectionDragEdge edge);
void UpdateRectangularSelectionEdit(MainWindow* win, int x, int y);

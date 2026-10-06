/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Selection.cpp. The model (SelectionOnPage, the text and
// rectangular selection state machine, select-all, copy) is orig's; what was
// win32 is gpui here: Gfx -> gpui::PaintCtx, SetCapture / KillTimer ->
// DocCanvas, the clipboard -> gpui::ClipboardSetText. Copy-as-image, the UIA
// notification and the touch selection handles are not ported; see
// docs/port-progress.md.

#include "gui/GpuiBridge.h"
#include "base/Pixmap.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "Notifications.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SelectionToolbar.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "Selection.h"

#include "SumatraLog.h"

SelectionOnPage::SelectionOnPage(int pageNo, const RectF* const rect, const QuadF* const quad) {
    this->pageNo = pageNo;
    if (rect) {
        this->rect = *rect;
    } else {
        this->rect = RectF();
    }
    if (quad) {
        this->quad = *quad;
    }
}

bool SelectionOnPage::HasQuad() const {
    return !quad.IsEmpty();
}

Rect SelectionOnPage::GetRect(DisplayModel* dm) const {
    // if the page is not visible, we return an empty rectangle
    PageInfo* pageInfo = dm->GetPageInfo(pageNo);
    if (!pageInfo || pageInfo->visibleRatio <= 0.0) {
        return {};
    }

    return dm->CvtToScreen(pageNo, rect);
}

Vec<SelectionOnPage>* SelectionOnPage::FromRectangle(DisplayModel* dm, Rect rect) {
    Vec<SelectionOnPage>* sel = new Vec<SelectionOnPage>();

    for (int pageNo = dm->GetEngine()->PageCount(); pageNo >= 1; --pageNo) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        ReportIf(!(!pi || 0.0 == pi->visibleRatio || pi->isShown));
        if (!pi || !pi->isShown) {
            continue;
        }

        Rect intersect = rect.Intersect(pi->pageOnScreen);
        if (intersect.IsEmpty()) {
            continue;
        }

        /* selection intersects with a page <pageNo> on the screen */
        RectF isectD = dm->CvtFromScreen(intersect, pageNo);
        VecAppend(*sel, SelectionOnPage(pageNo, &isectD));
    }
    VecReverse(*sel);

    if (len(*sel) == 0) {
        delete sel;
        return nullptr;
    }
    return sel;
}

Vec<SelectionOnPage>* SelectionOnPage::FromTextSelect(Vec<TextSel>* textSel) {
    if (len(*textSel) == 0) {
        return nullptr;
    }
    auto* sel = new Vec<SelectionOnPage>();
    VecReserve(*sel, len(*textSel));
    for (const TextSel& part : *textSel) {
        RectF rect = ToRectF(part.rect);
        VecAppend(*sel, SelectionOnPage(part.pageNo, &rect, &part.quad));
    }
    return sel;
}

void DeleteOldSelectionInfo(MainWindow* win, bool alsoTextSel) {
    HideSelectionToolbar(win);
    ResetSelectionToolbarDismissed(win);
    win->showSelection = false;
    win->selectionMeasure = SizeF();
    win->selectionDragEdge = SelectionDragEdge::None;
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }

    delete tab->selectionOnPage;
    tab->selectionOnPage = nullptr;
    if (alsoTextSel && tab->AsFixed()) {
        tab->AsFixed()->textSelection->Reset();
    }
}

// a chapter layout shifted dm's flat pageNo underneath tab->selectionOnPage;
// remap each entry via dm->RemapPageNo() instead of dropping the whole
// selection, and drop only the entries that no longer map to a page
void RemapSelOnRenumber(MainWindow* win, DisplayModel* dm) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!tab || !tab->selectionOnPage || !dm) {
        return;
    }
    for (int i = len(*tab->selectionOnPage) - 1; i >= 0; i--) {
        SelectionOnPage& sel = (*tab->selectionOnPage)[i];
        int newPageNo = dm->RemapPageNo(sel.pageNo);
        if (newPageNo < 1) {
            VecRemoveAt(*tab->selectionOnPage, i);
            continue;
        }
        sel.pageNo = newPageNo;
    }
    if (len(*tab->selectionOnPage) == 0) {
        delete tab->selectionOnPage;
        tab->selectionOnPage = nullptr;
        win->showSelection = false;
    }
}

// Remap selection pages after a chapter layout. Drop selections whose pages disappeared.
void RemapTextSelection(DisplayModel* dm) {
    if (!dm || !dm->textSelection) {
        return;
    }
    TextSelection* ts = dm->textSelection;
    Vec<TextSel>& result = ts->result;
    for (TextSel& part : result) {
        int newPageNo = dm->RemapPageNo(part.pageNo);
        if (newPageNo < 1) {
            ts->Reset();
            return;
        }
        part.pageNo = newPageNo;
    }
    ts->startPage = dm->RemapPageNo(ts->startPage);
    ts->endPage = dm->RemapPageNo(ts->endPage);
    ts->wordStartPage = dm->RemapPageNo(ts->wordStartPage);
    ts->wordEndPage = dm->RemapPageNo(ts->wordEndPage);
    if (len(result) > 0 && (ts->startPage < 1 || ts->endPage < 1)) {
        ts->Reset();
    }
}

// Rectangular (Ctrl+drag) selection: move/resize after it exists.
bool IsRectangularSelection(MainWindow* win) {
    if (!win || !win->showSelection) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage || len(*tab->selectionOnPage) == 0) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm || !dm->textSelection) {
        return false;
    }
    // text selection has glyphs; rectangular (Ctrl+drag) does not
    return len(dm->textSelection->result) == 0;
}

Rect GetRectangularSelectionScreenRect(MainWindow* win) {
    Rect bounds;
    if (!IsRectangularSelection(win)) {
        return bounds;
    }
    DisplayModel* dm = win->AsFixed();
    bool first = true;
    for (SelectionOnPage& sel : *win->CurrentTab()->selectionOnPage) {
        Rect r = sel.GetRect(dm);
        if (r.IsEmpty()) {
            continue;
        }
        if (first) {
            bounds = r;
            first = false;
        } else {
            bounds = bounds.Union(r);
        }
    }
    return bounds;
}

// Bounding box of the current selection.
// ng: orig returns screen coordinates (HwndClientToScreen on the canvas);
// gpui does not report a window's screen position, so these are canvas
// coordinates.
bool GetSelectionScreenRect(WindowTab* tab, Rect& out) {
    out = {};
    if (!tab || !tab->win || !tab->selectionOnPage) {
        return false;
    }
    MainWindow* win = tab->win;
    DisplayModel* dm = win->AsFixed();
    if (!dm || !win->showSelection) {
        return false;
    }
    Rect bounds;
    bool first = true;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        Rect r = sel.GetRect(dm);
        if (r.IsEmpty()) {
            continue;
        }
        if (first) {
            bounds = r;
            first = false;
        } else {
            bounds = bounds.Union(r);
        }
    }
    if (first) {
        return false;
    }
    out = bounds;
    return true;
}

TempStr FormatSelectionPositionTemp(WindowTab* tab) {
    Rect r;
    if (!GetSelectionScreenRect(tab, r)) {
        // StrL("") is empty but not null: ReplaceTemp treats a null
        // replacement as failure and would wipe the whole pattern
        return StrL("");
    }
    return fmt("%d,%d,%d,%d", r.x, r.y, r.dx, r.dy);
}

static Rect NormalizeScreenRect(Rect r) {
    if (r.dx < 0) {
        r.x += r.dx;
        r.dx = -r.dx;
    }
    if (r.dy < 0) {
        r.y += r.dy;
        r.dy = -r.dy;
    }
    return r;
}

// Apply edge/corner/move drag to an original normalized rect (screen coords).
static Rect ApplySelectionEdgeDrag(Rect orig, SelectionDragEdge edge, int dx, int dy) {
    int x = orig.x;
    int y = orig.y;
    int w = orig.dx;
    int h = orig.dy;

    if (edge == SelectionDragEdge::Move) {
        return {x + dx, y + dy, w, h};
    }

    if (edge == SelectionDragEdge::Left || edge == SelectionDragEdge::TopLeft ||
        edge == SelectionDragEdge::BottomLeft) {
        x = orig.x + dx;
        w = orig.dx - dx;
    }
    if (edge == SelectionDragEdge::Right || edge == SelectionDragEdge::TopRight ||
        edge == SelectionDragEdge::BottomRight) {
        w = orig.dx + dx;
    }
    if (edge == SelectionDragEdge::Top || edge == SelectionDragEdge::TopLeft || edge == SelectionDragEdge::TopRight) {
        y = orig.y + dy;
        h = orig.dy - dy;
    }
    if (edge == SelectionDragEdge::Bottom || edge == SelectionDragEdge::BottomLeft ||
        edge == SelectionDragEdge::BottomRight) {
        h = orig.dy + dy;
    }

    // minimum 1px (same idea as crop dialog)
    if (w < 1) {
        w = 1;
        if (edge == SelectionDragEdge::Left || edge == SelectionDragEdge::TopLeft ||
            edge == SelectionDragEdge::BottomLeft) {
            x = orig.x + orig.dx - 1;
        } else {
            x = orig.x;
        }
    }
    if (h < 1) {
        h = 1;
        if (edge == SelectionDragEdge::Top || edge == SelectionDragEdge::TopLeft ||
            edge == SelectionDragEdge::TopRight) {
            y = orig.y + orig.dy - 1;
        } else {
            y = orig.y;
        }
    }
    return {x, y, w, h};
}

SelectionDragEdge HitTestRectangularSelection(MainWindow* win, int mx, int my) {
    if (!IsRectangularSelection(win)) {
        return SelectionDragEdge::None;
    }
    Rect r = GetRectangularSelectionScreenRect(win);
    if (r.IsEmpty()) {
        return SelectionDragEdge::None;
    }
    int t = DpiScale(6);
    int left = r.x;
    int right = r.x + r.dx;
    int top = r.y;
    int bottom = r.y + r.dy;

    bool onLeft = (mx >= left - t && mx <= left + t);
    bool onRight = (mx >= right - t && mx <= right + t);
    bool onTop = (my >= top - t && my <= top + t);
    bool onBottom = (my >= bottom - t && my <= bottom + t);
    bool inVertRange = (my >= top - t && my <= bottom + t);
    bool inHorzRange = (mx >= left - t && mx <= right + t);

    if (onLeft && onTop) {
        return SelectionDragEdge::TopLeft;
    }
    if (onRight && onTop) {
        return SelectionDragEdge::TopRight;
    }
    if (onLeft && onBottom) {
        return SelectionDragEdge::BottomLeft;
    }
    if (onRight && onBottom) {
        return SelectionDragEdge::BottomRight;
    }
    if (onLeft && inVertRange) {
        return SelectionDragEdge::Left;
    }
    if (onRight && inVertRange) {
        return SelectionDragEdge::Right;
    }
    if (onTop && inHorzRange) {
        return SelectionDragEdge::Top;
    }
    if (onBottom && inHorzRange) {
        return SelectionDragEdge::Bottom;
    }
    if (mx > left + t && mx < right - t && my > top + t && my < bottom - t) {
        return SelectionDragEdge::Move;
    }
    return SelectionDragEdge::None;
}

// ng: orig answers a win32 IDC_* cursor; the portable canvas stores gpui's
// equivalent enum value as an int.
int CursorIdForSelectionEdge(SelectionDragEdge edge) {
    switch (edge) {
        case SelectionDragEdge::Left:
        case SelectionDragEdge::Right:
            return (int)gp::CursorKind::ColResize;
        case SelectionDragEdge::Top:
        case SelectionDragEdge::Bottom:
            return (int)gp::CursorKind::RowResize;
        case SelectionDragEdge::TopLeft:
        case SelectionDragEdge::BottomRight:
            return (int)gp::CursorKind::ResizeUpLeftDownRight;
        case SelectionDragEdge::TopRight:
        case SelectionDragEdge::BottomLeft:
            return (int)gp::CursorKind::ResizeUpRightDownLeft;
        case SelectionDragEdge::Move:
            return (int)gp::CursorKind::ClosedHand;
        default:
            return (int)gp::CursorKind::Arrow;
    }
}

bool StartRectangularSelectionEdit(MainWindow* win, int x, int y, SelectionDragEdge edge) {
    if (!win || edge == SelectionDragEdge::None || !IsRectangularSelection(win)) {
        return false;
    }
    Rect bounds = GetRectangularSelectionScreenRect(win);
    if (bounds.IsEmpty()) {
        return false;
    }
    win->selectionDragEdge = edge;
    win->selectionEditOrig = NormalizeScreenRect(bounds);
    win->selectionRect = win->selectionEditOrig;
    win->dragStart = Point(x, y);
    win->dragStartPending = true;
    win->showSelection = true;
    win->selectingByWord = false;
    win->mouseAction = MouseAction::Selecting;
    win->linkOnLastButtonDown = nullptr;
    win->textDragPending = false;
    win->imageDragPending = false;
    CanvasSetCapture(win, true);
    AppShellInvalidate(win);
    return true;
}

void UpdateRectangularSelectionEdit(MainWindow* win, int x, int y) {
    if (!win || win->selectionDragEdge == SelectionDragEdge::None) {
        return;
    }
    int dx = x - win->dragStart.x;
    int dy = y - win->dragStart.y;
    win->selectionRect = ApplySelectionEdgeDrag(win->selectionEditOrig, win->selectionDragEdge, dx, dy);
    win->selectionMeasure = win->AsFixed() ? win->AsFixed()->CvtFromScreen(win->selectionRect).Size() : SizeF();
}

void PaintTransparentRectangles(gp::PaintCtx* ctx, Rect screenRc, Vec<Rect>& rects, Color selectionColor, u8 alpha,
                                int pad, bool drawBorder) {
    Vec<Rect> paintedRects;
    // A bordered selection is the 3.6.1 look: font-height boxes as-is and a
    // 1px outline. Find highlights stay borderless and pad the box.
    int clipPad = drawBorder ? 1 : pad;
    screenRc.Inflate(clipPad, clipPad);
    for (int i = 0; i < len(rects); i++) {
        Rect rc = rects[i];
        if (!drawBorder && pad > 0) {
            rc.Inflate(pad, pad);
        }
        rc = rc.Intersect(screenRc);
        if (!rc.IsEmpty()) {
            VecAppend(paintedRects, rc);
        }
    }
    int outlineWidth = drawBorder ? 1 : 0;
    CanvasFillRects(ctx, paintedRects.els, len(paintedRects), selectionColor, alpha, outlineWidth);
}

static Rect QuadScreenBounds(const Point* pts) {
    int x0 = pts[0].x, y0 = pts[0].y, x1 = x0, y1 = y0;
    for (int i = 1; i < 4; i++) {
        x0 = std::min(x0, pts[i].x);
        y0 = std::min(y0, pts[i].y);
        x1 = std::max(x1, pts[i].x);
        y1 = std::max(y1, pts[i].y);
    }
    return Rect::FromXY(x0, y0, x1, y1);
}

static void PaintTransparentQuads(gp::PaintCtx* ctx, Rect screenRc, Vec<Point>& pts, Color selectionColor, u8 alpha,
                                  bool drawBorder) {
    int nQuads = len(pts) / 4;
    if (nQuads <= 0) {
        return;
    }
    screenRc.Inflate(1, 1);
    Vec<Point> painted;
    for (int i = 0; i < nQuads; i++) {
        Point* q = pts.els + (i * 4);
        if (QuadScreenBounds(q).Intersect(screenRc).IsEmpty()) {
            continue;
        }
        for (int k = 0; k < 4; k++) {
            VecAppend(painted, q[k]);
        }
    }
    int outlineWidth = drawBorder ? 1 : 0;
    CanvasFillQuads(ctx, painted.els, len(painted) / 4, selectionColor, alpha, outlineWidth);
}

void PaintSelection(MainWindow* win, gp::PaintCtx* ctx) {
    ReportIf(!win->AsFixed());

    Vec<Rect> rects;
    Vec<Point> quadPts;

    if (win->mouseAction == MouseAction::Selecting) {
        // during rectangle selection
        Rect selRect = win->selectionRect;
        if (selRect.dx < 0) {
            selRect.x += selRect.dx;
            selRect.dx *= -1;
        }
        if (selRect.dy < 0) {
            selRect.y += selRect.dy;
            selRect.dy *= -1;
        }

        VecAppend(rects, selRect);
    } else {
        // during text selection or after selection is done
        if (MouseAction::SelectingText == win->mouseAction) {
            // double/triple-click set the glyph range immediately; only extend
            // on repaint when the pointer has actually moved (issue #5712).
            int endX = win->selectionRect.x + win->selectionRect.dx;
            int endY = win->selectionRect.y + win->selectionRect.dy;
            bool dragged = IsDragDistance(win->selectionRect.x, endX, win->selectionRect.y, endY);
            UpdateTextSelection(win, dragged);
            if (!win->CurrentTab()->selectionOnPage) {
                // prevent the selection from disappearing while the
                // user is still at it (OnSelectionStop removes it
                // if it is still empty at the end)
                win->CurrentTab()->selectionOnPage = new Vec<SelectionOnPage>();
                win->showSelection = true;
            }
        }

        ReportDebugIf(!win->CurrentTab()->selectionOnPage);
        if (!win->CurrentTab()->selectionOnPage) {
            return;
        }

        for (SelectionOnPage& sel : *win->CurrentTab()->selectionOnPage) {
            if (sel.HasQuad()) {
                DisplayModel* dm = win->AsFixed();
                Point pts[4] = {
                    dm->CvtToScreen(sel.pageNo, sel.quad.ul),
                    dm->CvtToScreen(sel.pageNo, sel.quad.ur),
                    dm->CvtToScreen(sel.pageNo, sel.quad.lr),
                    dm->CvtToScreen(sel.pageNo, sel.quad.ll),
                };
                for (int k = 0; k < 4; k++) {
                    VecAppend(quadPts, pts[k]);
                }
            } else {
                VecAppend(rects, sel.GetRect(win->AsFixed()));
            }
        }
    }

    ParsedColor* parsedCol = GetPrefsColor(gSettings->fixedPageUI.selectionColor);
    // honor the alpha channel of SelectionColor (#aarrggbb): a smaller alpha makes
    // the overlay more transparent so the selected text stays crisp (issue #3209).
    // Fall back to the historical default when no alpha is given (e.g. #rrggbb).
    u8 alpha = GetAlpha(parsedCol->col);
    if (alpha == 0) {
        alpha = kSelectionDefaultAlpha;
    }
    Rect canvas(Point(), win->AsFixed()->GetViewPort().Size());
    if (len(quadPts) > 0) {
        PaintTransparentQuads(ctx, canvas, quadPts, parsedCol->col, alpha, /*drawBorder*/ true);
    }
    if (len(rects) > 0) {
        PaintTransparentRectangles(ctx, canvas, rects, parsedCol->col, alpha, 1, /*drawBorder*/ true);
    }
}

void UpdateTextSelection(MainWindow* win, bool select) {
    if (!win->AsFixed()) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (select) {
        int pageNo = dm->GetPageNoByPoint(win->selectionRect.BR());
        if (win->ctrl->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(win->selectionRect.BR(), pageNo);
            if (win->selectingByWord) {
                // double-click-drag: extend a whole word at a time (issue #4761)
                dm->textSelection->SelectWordsUpTo(pageNo, pt.x, pt.y);
            } else {
                dm->textSelection->SelectUpTo(pageNo, pt.x, pt.y);
            }
        }
    }

    DeleteOldSelectionInfo(win);
    win->CurrentTab()->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
}

// isTextSelectionOut is set to true if this is text-only selection (as opposed to
// rectangular selection)
TempStr GetSelectedTextTemp(WindowTab* tab, Str lineSep, bool& isTextOnlySelectionOut) {
    if (!tab || !tab->selectionOnPage) {
        return {};
    }
    if (len(*tab->selectionOnPage) == 0) {
        return {};
    }
    DisplayModel* dm = tab->AsFixed();
    ReportIf(!dm);
    if (!dm) {
        return {};
    }
    if (dm->GetEngine()->isImageCollection) {
        return {};
    }

    isTextOnlySelectionOut = len(dm->textSelection->result) > 0;
    if (isTextOnlySelectionOut) {
        return dm->textSelection->ExtractTextTemp(lineSep);
    }
    StrVec selections;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        // selection may reference pages that no longer exist after a reload
        if (!dm->ValidPageNo(sel.pageNo)) {
            continue;
        }
        Str text = dm->GetTextInRegion(sel.pageNo, sel.rect);
        if (text) {
            selections.Append(text);
        }
    }
    if (len(selections) == 0) {
        return {};
    }
    TempStr s = JoinTemp(&selections, lineSep);
    return s;
}

// orig's RenderSelectionsAsRenderedBitmap: each selection rectangle rendered
// out of the engine and stacked vertically into one image (Google Lens).
// ng: orig round-trips every piece through a RenderedBitmap / DIB; here the
// engine's Pixmap is normalized to 32bpp and used directly.
Pixmap* RenderSelectionsAsPixmap(DisplayModel* dm, const Vec<SelectionOnPage>& selections) {
    if (!dm || len(selections) == 0) {
        return nullptr;
    }

    constexpr i64 kMaxPixels = 24 * 1000 * 1000;
    Vec<Pixmap*> pixmaps;
    i64 totalHeight = 0;
    int maxWidth = 0;
    bool tooBig = false;

    for (const SelectionOnPage& selection : selections) {
        if (!dm->ValidPageNo(selection.pageNo)) {
            continue;
        }
        float zoom = dm->GetZoomReal(selection.pageNo);
        RectF rect = selection.rect;
        RenderPageArgs args(selection.pageNo, zoom, dm->GetRotation(), &rect, RenderTarget::Export);
        Pixmap* pixmap = PixmapToBgra(dm->GetEngine()->RenderPage(args));
        if (!pixmap) {
            continue;
        }
        i64 pixels = (i64)pixmap->width * pixmap->height;
        if (pixels <= 0 || pixels > kMaxPixels || totalHeight > kMaxPixels - pixels) {
            FreePixmap(pixmap);
            tooBig = true;
            break;
        }
        VecAppend(pixmaps, pixmap);
        totalHeight += pixmap->height;
        maxWidth = std::max(maxWidth, pixmap->width);
    }

    if (tooBig || len(pixmaps) == 0 || totalHeight > INT_MAX) {
        for (Pixmap* p : pixmaps) {
            FreePixmap(p);
        }
        return nullptr;
    }
    if (len(pixmaps) == 1) {
        return pixmaps[0];
    }

    Pixmap* combined = AllocPixmapDIB(maxWidth, (int)totalHeight);
    if (!combined) {
        for (Pixmap* p : pixmaps) {
            FreePixmap(p);
        }
        return nullptr;
    }
    for (int y = 0; y < combined->height; y++) {
        u8* row = combined->data + ((size_t)y * combined->stride);
        memset(row, 0xff, (size_t)combined->width * 4);
    }

    int y = 0;
    for (Pixmap* pixmap : pixmaps) {
        for (int row = 0; row < pixmap->height; row++) {
            memcpy(combined->data + ((size_t)(y + row) * combined->stride),
                   pixmap->data + ((size_t)row * pixmap->stride), (size_t)pixmap->width * 4);
        }
        y += pixmap->height;
        FreePixmap(pixmap);
    }
    return combined;
}

// ng: orig also puts the first selection rectangle on the clipboard as a
// bitmap (CF_BITMAP); gpui's clipboard is text only, so only the text goes.
void CopySelectionToClipboard(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    TempStr selText;
    bool isTextOnlySelectionOut = false;
    if (!gDisableDocumentRestrictions && (dm && !dm->GetEngine()->allowsCopyingText)) {
        ShowTemporaryNotification(win, Tr("Copying text was denied (copying as image only)"), kNotifDefaultTimeOut);
        return;
    }
    selText = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnlySelectionOut);
    logf("CopySelectionToClipboard: %d bytes, text-only selection: %d\n", len(selText), isTextOnlySelectionOut ? 1 : 0);
    if (len(selText) == 0) {
        return;
    }
    CanvasSetClipboardText(win, selText);
}

void OnSelectAll(MainWindow* win, bool textOnly) {
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }

    if (win->AsChm()) {
        win->AsChm()->SelectAll();
    } else if (win->AsMarkdown()) {
        win->AsMarkdown()->SelectAll();
        return;
    }
    if (!win->AsFixed()) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (textOnly) {
        int pageNo;
        for (pageNo = 1; !dm->PageShown(pageNo); pageNo++) {
            ;
        }
        dm->textSelection->StartAt(pageNo, 0);
        for (pageNo = win->ctrl->PageCount(); !dm->PageShown(pageNo); pageNo--) {
            ;
        }
        dm->textSelection->SelectUpTo(pageNo, -1);
        win->selectionRect = Rect::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        UpdateTextSelection(win);
    } else {
        DeleteOldSelectionInfo(win, true);
        win->selectionRect = Rect::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
        win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(dm, win->selectionRect);
    }

    win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
    ShowSelectionToolbar(win, SelToolbarShow::Settled);
    AppShellInvalidate(win);
}

// like Select All, but only the text of the current page
void OnSelectCurrentPage(MainWindow* win) {
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    int pageNo = dm->CurrentPageNo();
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return;
    }
    dm->textSelection->StartAt(pageNo, 0);
    dm->textSelection->SelectUpTo(pageNo, -1);
    win->selectionRect = Rect::FromXY(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
    UpdateTextSelection(win, false);
    AppShellInvalidate(win);
}

#define kSelectAutoscrollAreaWidth DpiScale(15)
#define kSelectAutoscrollStepLength DpiScale(10)

bool NeedsSelectionEdgeAutoscroll(MainWindow* win, int x, int y) {
    return x < kSelectAutoscrollAreaWidth || x > win->canvasRc.dx - kSelectAutoscrollAreaWidth ||
           y < kSelectAutoscrollAreaWidth || y > win->canvasRc.dy - kSelectAutoscrollAreaWidth;
}

// Horizontal auto-scroll while selecting text exists to reveal text the
// selection has reached. Once the selected text's leading edge is on screen
// there is nothing left to reveal, and scrolling on just pans the page out from
// under the user: at high zoom the cursor sits in the right margin long before
// the line ends, so the view runs away while the selection stays put (#5497).
// Vertical auto-scroll is untouched - there the next line really is off screen.
// Returns how much of `dx` is still needed, 0 once the selection is visible.
static int LimitTextSelectionAutoscrollDx(MainWindow* win, int dx) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSelection) {
        return dx;
    }
    Vec<TextSel>* sel = &dm->textSelection->result;
    if (len(*sel) == 0) {
        return dx;
    }
    int selLeft = INT_MAX;
    int selRight = INT_MIN;
    for (int i = 0; i < len(*sel); i++) {
        int pageNo = (*sel)[i].pageNo;
        if (!dm->PageVisible(pageNo)) {
            continue;
        }
        Rect rc = dm->CvtToScreen(pageNo, ToRectF((*sel)[i].rect));
        selLeft = std::min(selLeft, rc.x);
        selRight = std::max(selRight, rc.x + rc.dx);
    }
    if (selLeft > selRight) {
        return dx; // nothing selected on a visible page
    }
    int margin = kSelectAutoscrollAreaWidth;
    if (dx > 0) {
        int needed = selRight - (win->canvasRc.dx - margin);
        return limitValue(needed, 0, dx);
    }
    int needed = selLeft - margin;
    return limitValue(needed, dx, 0);
}

void OnSelectionEdgeAutoscroll(MainWindow* win, int x, int y) {
    int dx = 0, dy = 0;

    if (x < kSelectAutoscrollAreaWidth) {
        dx = -kSelectAutoscrollStepLength;
    } else if (x > win->canvasRc.dx - kSelectAutoscrollAreaWidth) {
        dx = kSelectAutoscrollStepLength;
    }
    if (y < kSelectAutoscrollAreaWidth) {
        dy = -kSelectAutoscrollStepLength;
    } else if (y > win->canvasRc.dy - kSelectAutoscrollAreaWidth) {
        dy = kSelectAutoscrollStepLength;
    }

    // clamping can legitimately leave dx at 0 while the cursor is still in the
    // auto-scroll strip
    if (dx != 0 && MouseAction::SelectingText == win->mouseAction) {
        dx = LimitTextSelectionAutoscrollDx(win, dx);
    }
    if (dx == 0 && dy == 0) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    Point oldOffset = dm->GetViewPort().TL();
    win->MoveDocBy(dx, dy);

    dx = dm->GetViewPort().x - oldOffset.x;
    dy = dm->GetViewPort().y - oldOffset.y;
    if (win->selectionDragEdge != SelectionDragEdge::None) {
        // move/resize: keep the selection fixed on the document as the view pans
        win->selectionEditOrig.x -= dx;
        win->selectionEditOrig.y -= dy;
        win->dragStart.x -= dx;
        win->dragStart.y -= dy;
        win->selectionRect.x -= dx;
        win->selectionRect.y -= dy;
    } else {
        // new selection: keep the start corner fixed on the document
        win->selectionRect.x -= dx;
        win->selectionRect.y -= dy;
        win->selectionRect.dx += dx;
        win->selectionRect.dy += dy;
    }
}

void OnSelectionStart(MainWindow* win, int x, int y, bool forceRect) {
    ReportIf(!win->AsFixed());
    DeleteOldSelectionInfo(win, true);

    win->selectionDragEdge = SelectionDragEdge::None;
    win->selectionRect = Rect(x, y, 0, 0);
    win->showSelection = true;
    win->selectingByWord = false;
    win->mouseAction = MouseAction::Selecting;

    bool isShift = CanvasShiftPressed();
    bool isCtrl = CanvasCtrlPressed();

    // Ctrl+drag (or forceRect) is a rectangular selection, not a text one
    if (!forceRect && (!isCtrl || isShift)) {
        DisplayModel* dm = win->AsFixed();
        int pageNo = dm->GetPageNoByPoint(Point(x, y));
        if (dm->ValidPageNo(pageNo)) {
            PointF pt = dm->CvtFromScreen(Point(x, y), pageNo);
            dm->textSelection->StartAt(pageNo, pt.x, pt.y);
            win->mouseAction = MouseAction::SelectingText;
        }
    }

    CanvasSetCapture(win, true);
    AppShellInvalidate(win);
}

void OnSelectionStop(MainWindow* win, int x, int y, bool aborted) {
    CanvasSetCapture(win, false);

    bool editingRect = win->selectionDragEdge != SelectionDragEdge::None && win->mouseAction == MouseAction::Selecting;

    // update the text selection before changing the selectionRect
    if (MouseAction::SelectingText == win->mouseAction) {
        // double/triple-click set the glyph range immediately; a tiny mouse jitter
        // while the button is held still updates selectionRect.dx/dy. Only extend
        // the selection on mouse-up when the pointer actually moved (issue #5712).
        bool dragged = IsDragDistance(win->selectionRect.x, x, win->selectionRect.y, y);
        UpdateTextSelection(win, dragged);
    }

    if (editingRect) {
        if (aborted) {
            // click without drag on a handle: keep previous selection
            win->selectionRect = win->selectionEditOrig;
        } else {
            UpdateRectangularSelectionEdit(win, x, y);
            win->selectionRect = NormalizeScreenRect(win->selectionRect);
        }
        delete win->CurrentTab()->selectionOnPage;
        win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(win->AsFixed(), win->selectionRect);
        win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
        if (win->showSelection) {
            win->selectionMeasure = win->AsFixed()->CvtFromScreen(win->selectionRect).Size();
        } else {
            win->selectionMeasure = SizeF();
        }
        win->selectionDragEdge = SelectionDragEdge::None;
    } else {
        win->selectionRect = Rect::FromXY(win->selectionRect.x, win->selectionRect.y, x, y);
        if (aborted || (MouseAction::Selecting == win->mouseAction ? win->selectionRect.IsEmpty()
                                                                   : !win->CurrentTab()->selectionOnPage)) {
            DeleteOldSelectionInfo(win, true);
        } else if (win->mouseAction == MouseAction::Selecting) {
            win->selectionRect = NormalizeScreenRect(win->selectionRect);
            win->CurrentTab()->selectionOnPage = SelectionOnPage::FromRectangle(win->AsFixed(), win->selectionRect);
            win->showSelection = win->CurrentTab()->selectionOnPage != nullptr;
        }
        win->selectionDragEdge = SelectionDragEdge::None;
    }
    win->selectingByWord = false;
    AppShellInvalidate(win);
    {
        DisplayModel* dmLog = win->AsFixed();
        WindowTab* tabLog = win->CurrentTab();
        int nSel = (tabLog && tabLog->selectionOnPage) ? len(*tabLog->selectionOnPage) : 0;
        logf("OnSelectionStop: aborted %d, %d rects, %d glyphs, rect %d,%d,%d,%d\n", aborted ? 1 : 0, nSel,
             dmLog ? len(dmLog->textSelection->result) : 0, win->selectionRect.x, win->selectionRect.y,
             win->selectionRect.dx, win->selectionRect.dy);
    }

    // show the floating selection toolbar for a finished text selection
    // (self-guards: needs a non-empty on-screen text selection)
    if (!aborted || editingRect) {
        ShowSelectionToolbar(win, SelToolbarShow::Now);
    }
}

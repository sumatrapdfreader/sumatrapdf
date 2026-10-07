/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"

constexpr int kDocumentLayoutInvalidPageNo = -1;

static bool PageIsSpread(const Vec<u8>& flags, int pageNo) {
    int i = pageNo - 1;
    if (i < 0 || i >= len(flags)) {
        return false;
    }
    return flags[i] != 0;
}

// Landscape spreads occupy a full row; book view keeps the first page alone.
void CollectFacingRows(Vec<FacingRow>& out, int pageCount, bool bookView, const Vec<u8>& spreadFlags) {
    VecReset(out);
    if (pageCount < 1) {
        return;
    }
    int page = 1;
    if (bookView) {
        VecAppend(out, {1, 1, PageIsSpread(spreadFlags, 1)});
        page = 2;
    }
    while (page <= pageCount) {
        FacingRow row{page, page, PageIsSpread(spreadFlags, page)};
        if (!row.isSpread && page + 1 <= pageCount && !PageIsSpread(spreadFlags, page + 1)) {
            row.lastPage++;
        }
        VecAppend(out, row);
        page = row.lastPage + 1;
    }
}

static const FacingRow* FindFacingRow(const Vec<FacingRow>& rows, int pageNo) {
    for (int i = 0; i < len(rows); i++) {
        if (rows[i].firstPage <= pageNo && pageNo <= rows[i].lastPage) {
            return &rows[i];
        }
    }
    return nullptr;
}

void DocumentLayout::Reset(int pageCount) {
    VecReset(pages);
    if (pageCount > 0) {
        VecResize(pages, pageCount);
    }
    canvasSize = {};
    viewPort = {};
    zoomReal = 1;
}

bool DocumentLayout::ValidPageNo(int pageNo) const {
    return pageNo >= 1 && pageNo <= pages.len;
}

void DocumentLayout::SetPageMediaBox(int pageNo, RectF mediaBox) {
    if (!ValidPageNo(pageNo)) {
        return;
    }
    pages[pageNo - 1].mediaBox = mediaBox;
}

DocumentLayoutPage* DocumentLayout::GetPage(int pageNo) {
    if (!ValidPageNo(pageNo)) {
        return nullptr;
    }
    return &pages[pageNo - 1];
}

const DocumentLayoutPage* DocumentLayout::GetPage(int pageNo) const {
    if (!ValidPageNo(pageNo)) {
        return nullptr;
    }
    return &pages[pageNo - 1];
}

static SizeF PageSizeAfterRotation(const DocumentLayoutPage* page, int rotation) {
    SizeF size = page ? page->mediaBox.Size() : SizeF();
    rotation = NormalizeRotation(rotation);
    if (rotation == 90 || rotation == 270) {
        std::swap(size.dx, size.dy);
    }
    return size;
}

// Match engine/tile rounding so zero page spacing leaves no background hairlines.
static Size PagePixelSize(SizeF pageSize, float zoom) {
    if (zoom <= 0 || pageSize.dx <= 0 || pageSize.dy <= 0) {
        return {};
    }
    return RectF(0, 0, pageSize.dx * zoom, pageSize.dy * zoom).Round().Size();
}

static float ZoomRealFromVirtualForPage(const DocumentLayout& layout, float zoomVirtual, int pageNo) {
    const DocumentLayoutParams& params = layout.params;
    if (zoomVirtual != kZoomFitWidth && zoomVirtual != kZoomFitHeight && zoomVirtual != kZoomFitPage) {
        return zoomVirtual * 0.01f * params.dpiFactor;
    }

    int nCols = IsSingle(params.displayMode) ? 1 : 2;
    SizeF row = PageSizeAfterRotation(layout.GetPage(pageNo), params.rotation);
    if (nCols > 1 && params.landscapeAsSpread && PageIsSpread(params.spreadFlags, pageNo)) {
        nCols = 1;
    }
    row.dx *= (float)nCols;
    row.dx += (float)((double)params.pageSpacing.dx * (double)(nCols - 1));

    if (RectF(PointF(), row).IsEmpty()) {
        return 0;
    }

    int areaForPagesDx = layout.viewPort.dx - params.windowMargin.left - params.windowMargin.right;
    int areaForPagesDy = layout.viewPort.dy - params.windowMargin.top - params.windowMargin.bottom;
    if (areaForPagesDx <= 0 || areaForPagesDy <= 0) {
        return 0;
    }

    float zoomX = (float)areaForPagesDx / row.dx;
    float zoomY = (float)areaForPagesDy / row.dy;
    if (zoomVirtual == kZoomFitWidth) {
        return zoomX;
    }
    if (zoomVirtual == kZoomFitHeight) {
        return zoomY;
    }
    return (zoomX < zoomY) ? zoomX : zoomY;
}

static void CalcZoomReal(DocumentLayout& layout, float zoomVirtual) {
    bool fitZoom = zoomVirtual == kZoomFitWidth || zoomVirtual == kZoomFitHeight || zoomVirtual == kZoomFitPage;
    if (!layout.params.usePageZooms && !fitZoom) {
        layout.zoomReal = zoomVirtual * 0.01f * layout.params.dpiFactor;
        for (DocumentLayoutPage& page : layout.pages) {
            page.zoomReal = layout.zoomReal;
        }
        return;
    }

    float minZoom = (float)HUGE_VAL;
    for (int pageNo = 1; pageNo <= len(layout.pages); pageNo++) {
        DocumentLayoutPage* page = layout.GetPage(pageNo);
        if (!page->isShown) {
            continue;
        }
        if (!layout.params.usePageZooms) {
            page->zoomReal = ZoomRealFromVirtualForPage(layout, zoomVirtual, pageNo);
        }
        minZoom = std::min(minZoom, page->zoomReal);
    }
    layout.zoomReal = minZoom == (float)HUGE_VAL ? 1 : minZoom;
}

Size FreePanSlack(Size viewPort) {
    return Size(viewPort.dx / 2, viewPort.dy / 2);
}

static void FinishRelayout(DocumentLayout& layout, int canvasDx, int canvasDy, bool isFitContent) {
    const DocumentLayoutParams& params = layout.params;
    Rect& viewPort = layout.viewPort;

    if (canvasDy < viewPort.dy) {
        int offY = params.windowMargin.top + ((viewPort.dy - canvasDy) / 2);
        for (DocumentLayoutPage& page : layout.pages) {
            if (page.isShown) {
                page.pos.y += offY;
            }
        }
    }

    // Clamp noncontinuous Fit Page to the viewport. Content fit needs margin scroll room.
    if (params.zoomVirtual == kZoomFitPage && !isFitContent && !IsContinuous(params.displayMode)) {
        canvasDy = std::min(canvasDy, viewPort.dy);
        canvasDx = std::min(canvasDx, viewPort.dx);
    }

    // Allow the last page's top to reach the viewport top in continuous mode.
    // PaddingAfterLastPage controls this extra scroll room.
    if (params.paddingAfterLastPage && IsContinuous(params.displayMode) && viewPort.dy > 0) {
        int lastPageTop = -1;
        for (int pageNo = layout.pages.len; pageNo >= 1; pageNo--) {
            DocumentLayoutPage* page = layout.GetPage(pageNo);
            if (page->isShown) {
                lastPageTop = page->pos.y;
                break;
            }
        }
        if (lastPageTop >= 0) {
            int minCanvasDy = lastPageTop + viewPort.dy;
            canvasDy = std::max(canvasDy, minCanvasDy);
        }
    }

    // Free pan adds scroll room, not window margins: fit zooms and navigation stay unchanged.
    if (params.freePan) {
        Size slack = FreePanSlack(viewPort.Size());
        for (DocumentLayoutPage& page : layout.pages) {
            if (page.isShown) {
                page.pos.Offset(slack.dx, slack.dy);
            }
        }
        canvasDx = std::max(canvasDx, viewPort.dx) + (2 * slack.dx);
        canvasDy = std::max(canvasDy, viewPort.dy) + (2 * slack.dy);
    }

    layout.canvasSize = Size(std::max(canvasDx, viewPort.dx), std::max(canvasDy, viewPort.dy));
    viewPort.x = std::min(viewPort.x, layout.canvasSize.dx - viewPort.dx);
    viewPort.y = std::min(viewPort.y, layout.canvasSize.dy - viewPort.dy);
    layout.RecalcVisibleParts();
}

static int CenterCanvasX(DocumentLayout& layout, int& canvasDx) {
    if (canvasDx >= layout.viewPort.dx) {
        return 0;
    }
    layout.viewPort.x = 0;
    int offX = (layout.viewPort.dx - canvasDx) / 2;
    canvasDx = layout.viewPort.dx;
    return offX;
}

static void SetPageDisplaySize(DocumentLayoutPage* page, int rotation, int currPosY) {
    Size px = PagePixelSize(PageSizeAfterRotation(page, rotation), page->zoomReal);
    page->pos.dx = px.dx;
    page->pos.dy = px.dy;
    page->pos.y = currPosY;
}

static void RelayoutRows(DocumentLayout& layout, bool isFitContent) {
    const DocumentLayoutParams& params = layout.params;
    const int pageCount = layout.pages.len;
    bool single = IsSingle(params.displayMode);
    Vec<FacingRow> rows;
    if (!single && !params.landscapeAsSpread && !IsContinuous(params.displayMode)) {
        int last = std::min(params.startPage + 1, pageCount);
        if (IsBookView(params.displayMode) && params.startPage == 1) {
            last = 1;
        }
        VecAppend(rows, {params.startPage, last, false});
    } else if (!single) {
        Vec<u8> noSpreads;
        CollectFacingRows(rows, pageCount, IsBookView(params.displayMode),
                          params.landscapeAsSpread ? params.spreadFlags : noSpreads);
    }

    int startFirst = params.startPage;
    int startLast = params.startPage;
    const FacingRow* startRow = FindFacingRow(rows, params.startPage);
    if (startRow) {
        startFirst = startRow->firstPage;
        startLast = startRow->lastPage;
    }

    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        auto& page = layout.pages[pageNo - 1];
        page.pos = {};
        page.isShown = IsContinuous(params.displayMode) || (startFirst <= pageNo && pageNo <= startLast);
    }

    int currPosY = params.windowMargin.top;
    CalcZoomReal(layout, params.zoomVirtual);

    int columnMaxWidth[2] = {0, 0};
    int maxFullRowWidth = 0;
    int nRows = single ? pageCount : len(rows);
    for (int ri = 0; ri < nRows; ri++) {
        FacingRow row = single ? FacingRow{ri + 1, ri + 1, false} : rows[ri];
        if (!layout.GetPage(row.firstPage)->isShown) {
            continue;
        }
        int rowMaxPageDy = 0;
        bool cover = row.firstPage == row.lastPage && IsBookView(params.displayMode) && row.firstPage == 1;
        for (int pageNo = row.firstPage; pageNo <= row.lastPage; pageNo++) {
            DocumentLayoutPage* page = layout.GetPage(pageNo);
            SetPageDisplaySize(page, params.rotation, currPosY);
            rowMaxPageDy = single ? page->pos.dy : std::max(rowMaxPageDy, page->pos.dy);
            if (single || row.isSpread) {
                maxFullRowWidth = std::max(maxFullRowWidth, page->pos.dx);
            } else {
                int col = pageNo - row.firstPage + (cover ? 1 : 0);
                ReportIf(col >= 2);
                columnMaxWidth[col] = std::max(columnMaxWidth[col], page->pos.dx);
            }
        }
        currPosY += rowMaxPageDy + params.pageSpacing.dy;
    }

    int canvasDy = currPosY + params.windowMargin.bottom - params.pageSpacing.dy;

    if (pageCount == 1) {
        if (IsBookView(params.displayMode)) {
            columnMaxWidth[0] = columnMaxWidth[1];
        } else {
            columnMaxWidth[1] = columnMaxWidth[0];
        }
    }

    int twoColDx = columnMaxWidth[0] + params.pageSpacing.dx + columnMaxWidth[1];
    int pagesDx = single ? maxFullRowWidth : twoColDx;
    if (params.landscapeAsSpread) {
        pagesDx = std::max(pagesDx, maxFullRowWidth);
    }
    int canvasDx = params.windowMargin.left + pagesDx + params.windowMargin.right;

    int offX = CenterCanvasX(layout, canvasDx);

    for (int ri = 0; ri < nRows; ri++) {
        FacingRow row = single ? FacingRow{ri + 1, ri + 1, false} : rows[ri];
        if (!layout.GetPage(row.firstPage)->isShown) {
            continue;
        }
        bool cover = row.firstPage == row.lastPage && IsBookView(params.displayMode) && row.firstPage == 1;
        int pageOffX = offX + params.windowMargin.left;
        for (int pageNo = row.firstPage; pageNo <= row.lastPage; pageNo++) {
            DocumentLayoutPage* page = layout.GetPage(pageNo);
            if (single || row.isSpread || (cover && !IsContinuous(params.displayMode))) {
                page->pos.x = pageOffX + ((pagesDx - page->pos.dx) / 2);
            } else if (!cover && pageNo == row.firstPage) {
                page->pos.x = pageOffX + columnMaxWidth[0] - page->pos.dx;
            } else {
                page->pos.x = pageOffX + (columnMaxWidth[0] + params.pageSpacing.dx);
            }
            if (!single && params.displayR2L) {
                page->pos.x = canvasDx - page->pos.x - page->pos.dx;
            }
        }
    }

    FinishRelayout(layout, canvasDx, canvasDy, isFitContent);
}

void DocumentLayout::Relayout(const DocumentLayoutParams& newParams) {
    if (len(pages) == 0) {
        Reset(0);
        return;
    }

    params = newParams;
    params.rotation = NormalizeRotation(params.rotation);
    params.startPage = limitValue(params.startPage, 1, pages.len);
    if (params.dpiFactor <= 0) {
        params.dpiFactor = 1;
    }
    if (params.zoomVirtual == kZoomFitByOrientation) {
        params.zoomVirtual = params.viewPortSize.dx > params.viewPortSize.dy ? kZoomFitWidth : kZoomFitPage;
    }
    // Content fit uses per-page zoom and must retain scroll room to hide margins.
    // ShrinkToFit stays within page fit and may use its canvas clamp.
    bool isFitContent = params.zoomVirtual == kZoomFitContent || params.zoomVirtual == kZoomFitVisible;
    if (isFitContent || params.zoomVirtual == kZoomShrinkToFit) {
        params.zoomVirtual = kZoomFitPage;
    }

    viewPort = Rect(params.viewPortOffset, params.viewPortSize);

    RelayoutRows(*this, isFitContent);
}

void DocumentLayout::RecalcVisibleParts() {
    for (DocumentLayoutPage& page : pages) {
        Rect pageRect = page.pos;
        Rect visiblePart = pageRect.Intersect(viewPort);
        page.visibleRatio = 0;
        if (!visiblePart.IsEmpty() && !pageRect.IsEmpty()) {
            page.visibleRatio =
                1.0f * (float)visiblePart.dx * (float)visiblePart.dy / ((float)pageRect.dx * (float)pageRect.dy);
        }
        page.pageOnScreen = pageRect;
        page.pageOnScreen.Offset(-viewPort.x, -viewPort.y);
    }
}

int DocumentLayout::CurrentPageNo() const {
    if (!IsContinuous(params.displayMode)) {
        return params.startPage;
    }
    int mostVisiblePage = 1;
    float ratio = 0;
    for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
        const DocumentLayoutPage* page = GetPage(pageNo);
        if (page->visibleRatio > ratio) {
            mostVisiblePage = pageNo;
            ratio = page->visibleRatio;
        }
    }
    if (ratio <= 0 && pages.len > 0) {
        // Horizontal scrolling may miss centered, narrow pages; choose by vertical band.
        mostVisiblePage = PageNoAtViewPortTop();
    }
    return mostVisiblePage;
}

// the page whose vertical band contains the top of the viewport (the last page
// when the viewport is past the end); ignores horizontal position
int DocumentLayout::PageNoAtViewPortTop() const {
    for (int pageNo = 1; pageNo <= len(pages); pageNo++) {
        const auto& page = pages[pageNo - 1];
        if (viewPort.y < page.pos.y + page.pos.dy) {
            return pageNo;
        }
    }
    return std::max(1, len(pages));
}

int DocumentLayout::FirstVisiblePageNo() const {
    for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
        const DocumentLayoutPage* page = GetPage(pageNo);
        if (page->visibleRatio > 0) {
            return pageNo;
        }
    }
    return kDocumentLayoutInvalidPageNo;
}

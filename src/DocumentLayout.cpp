/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"

constexpr int kDocumentLayoutInvalidPageNo = -1;

static int ColumnsFromDisplayMode(DisplayMode displayMode) {
    if (!IsSingle(displayMode)) {
        return 2;
    }
    return 1;
}

void DocumentLayout::Reset(int pageCount) {
    pages.Reset();
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

static float ZoomRealFromVirtualForPage(const DocumentLayout& layout, float zoomVirtual, int pageNo) {
    const DocumentLayoutParams& params = layout.params;
    if (zoomVirtual != kZoomFitWidth && zoomVirtual != kZoomFitHeight && zoomVirtual != kZoomFitPage) {
        return zoomVirtual * 0.01f * params.dpiFactor;
    }

    int columns = ColumnsFromDisplayMode(params.displayMode);
    SizeF row = PageSizeAfterRotation(layout.GetPage(pageNo), params.rotation);
    row.dx *= (float)columns;
    row.dx += (float)((double)params.pageSpacing.dx * (double)(columns - 1));

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
    const int pageCount = layout.pages.len;
    if (layout.params.usePageZooms) {
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
            DocumentLayoutPage* page = layout.GetPage(pageNo);
            if (!page->isShown) {
                continue;
            }
            minZoom = std::min(minZoom, page->zoomReal);
        }
        layout.zoomReal = minZoom == (float)HUGE_VAL ? 1 : minZoom;
        return;
    }

    if (zoomVirtual == kZoomFitWidth || zoomVirtual == kZoomFitHeight || zoomVirtual == kZoomFitPage) {
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
            DocumentLayoutPage* page = layout.GetPage(pageNo);
            if (!page->isShown) {
                continue;
            }
            float zoom = ZoomRealFromVirtualForPage(layout, zoomVirtual, pageNo);
            page->zoomReal = zoom;
            minZoom = std::min(minZoom, zoom);
        }
        layout.zoomReal = minZoom == (float)HUGE_VAL ? 1 : minZoom;
    } else {
        layout.zoomReal = zoomVirtual * 0.01f * layout.params.dpiFactor;
        for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
            layout.GetPage(pageNo)->zoomReal = layout.zoomReal;
        }
    }
}

void DocumentLayout::Relayout(const DocumentLayoutParams& newParams) {
    if (pages.len == 0) {
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
    // Fit Content lays out like Fit Page - the layout is built from media boxes
    // either way, the content fit lives in the per-page zoom - but it must not
    // take Fit Page's canvas clamp below: it zooms past the page fit and relies
    // on DisplayModel::GoToPage() scrolling the margins off-screen.
    // ShrinkToFit never zooms past the page fit, so the clamp is a no-op there.
    bool isFitContent = (params.zoomVirtual == kZoomFitContent);
    if (params.zoomVirtual == kZoomFitContent || params.zoomVirtual == kZoomShrinkToFit) {
        params.zoomVirtual = kZoomFitPage;
    }

    viewPort = Rect(params.viewPortOffset, params.viewPortSize);

    int columns = ColumnsFromDisplayMode(params.displayMode);
    int firstShown = params.startPage;
    if (IsBookView(params.displayMode) && firstShown == 1 && columns > 1) {
        firstShown--;
    }

    for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
        auto& page = pages[pageNo - 1];
        page.visibleRatio = 0;
        page.pageOnScreen = {};
        page.pos = {};
        page.isShown = IsContinuous(params.displayMode) || (firstShown <= pageNo && pageNo < firstShown + columns);
    }

    int currPosY = params.windowMargin.top;
    CalcZoomReal(*this, params.zoomVirtual);

    int columnMaxWidth[2] = {0, 0};
    int pageInARow = 0;
    int rowMaxPageDy = 0;
    for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
        DocumentLayoutPage* page = GetPage(pageNo);
        if (!page->isShown) {
            continue;
        }

        SizeF pageSize = PageSizeAfterRotation(page, params.rotation);
        Rect pos;
        float zoom = page->zoomReal;
        pos.dx = (int)((pageSize.dx * zoom) + 0.499f);
        pos.dy = (int)((pageSize.dy * zoom) + 0.499f);
        rowMaxPageDy = std::max(rowMaxPageDy, pos.dy);
        pos.y = currPosY;

        if (IsBookView(params.displayMode) && pageNo == 1 && columns - pageInARow > 1) {
            pageInARow++;
        }
        ReportIf(pageInARow >= dimofi(columnMaxWidth));
        columnMaxWidth[pageInARow] = std::max(columnMaxWidth[pageInARow], pos.dx);

        page->pos = pos;
        pageInARow++;
        ReportIf(pageInARow > columns);
        if (pageInARow == columns) {
            currPosY += rowMaxPageDy + params.pageSpacing.dy;
            rowMaxPageDy = 0;
            pageInARow = 0;
        }
    }

    if (pageInARow != 0) {
        currPosY += rowMaxPageDy + params.pageSpacing.dy;
    }
    int canvasDy = currPosY + params.windowMargin.bottom - params.pageSpacing.dy;

    if (columns == 2 && pages.len == 1) {
        if (IsBookView(params.displayMode)) {
            columnMaxWidth[0] = columnMaxWidth[1];
        } else {
            columnMaxWidth[1] = columnMaxWidth[0];
        }
    }

    int canvasDx = params.windowMargin.left + columnMaxWidth[0] +
                   (columns == 2 ? params.pageSpacing.dx + columnMaxWidth[1] : 0) + params.windowMargin.right;

    int offX = 0;
    if (canvasDx < viewPort.dx) {
        viewPort.x = 0;
        offX = (viewPort.dx - canvasDx) / 2;
        canvasDx = viewPort.dx;
    }

    pageInARow = 0;
    int pageOffX = offX + params.windowMargin.left;
    for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
        DocumentLayoutPage* page = GetPage(pageNo);
        if (!page->isShown) {
            continue;
        }

        if (IsBookView(params.displayMode) && pageNo == 1) {
            pageOffX += columnMaxWidth[pageInARow] + params.pageSpacing.dx;
            pageInARow++;
        }
        if (columns == 1) {
            page->pos.x = pageOffX + ((columnMaxWidth[0] - page->pos.dx) / 2);
        } else if (pageInARow == 0) {
            page->pos.x = pageOffX + columnMaxWidth[0] - page->pos.dx;
        } else {
            page->pos.x = pageOffX;
        }
        if (IsBookView(params.displayMode) && pageNo == 1 && !IsContinuous(params.displayMode)) {
            page->pos.x = offX + params.windowMargin.left +
                          ((columnMaxWidth[0] + params.pageSpacing.dx + columnMaxWidth[1] - page->pos.dx) / 2);
        }
        if (params.displayR2L && columns > 1) {
            page->pos.x = canvasDx - page->pos.x - page->pos.dx;
        }

        pageOffX += columnMaxWidth[pageInARow] + params.pageSpacing.dx;
        pageInARow++;
        if (pageInARow == columns) {
            pageOffX = offX + params.windowMargin.left;
            pageInARow = 0;
        }
    }

    if (canvasDy < viewPort.dy) {
        int offY = params.windowMargin.top + ((viewPort.dy - canvasDy) / 2);
        for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
            DocumentLayoutPage* page = GetPage(pageNo);
            if (page->isShown) {
                page->pos.y += offY;
            }
        }
    }

    // Fit Page never needs to scroll, so pin the canvas to the window and no
    // scrollbars appear. Not for Fit Content: clamping leaves it no scroll range,
    // so limitValue() in GoToPage() would drop the scroll to the content start
    // and the page would show its top/left margin with the content cut off at
    // the other end (it looked right only in continuous modes).
    if (params.zoomVirtual == kZoomFitPage && !isFitContent && !IsContinuous(params.displayMode)) {
        canvasDy = std::min(canvasDy, viewPort.dy);
        canvasDx = std::min(canvasDx, viewPort.dx);
    }

    // Continuous mode: extra space after the last page so it can be scrolled
    // up (e.g. last lines to the top of the window when the frame is partly
    // covered by another app) (issue #411). Need canvas tall enough that
    // max scroll (canvasDy - viewPort.dy) can place the last page's top at y=0.
    // Controlled by advanced setting PaddingAfterLastPage (default off).
    if (params.paddingAfterLastPage && IsContinuous(params.displayMode) && viewPort.dy > 0) {
        int lastPageTop = -1;
        for (int pageNo = pages.len; pageNo >= 1; pageNo--) {
            DocumentLayoutPage* page = GetPage(pageNo);
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

    canvasSize = Size(std::max(canvasDx, viewPort.dx), std::max(canvasDy, viewPort.dy));
    if (viewPort.x > canvasSize.dx - viewPort.dx) {
        viewPort.x = std::max(0, canvasSize.dx - viewPort.dx);
    }
    if (viewPort.y > canvasSize.dy - viewPort.dy) {
        viewPort.y = std::max(0, canvasSize.dy - viewPort.dy);
    }
    RecalcVisibleParts();
}

void DocumentLayout::RecalcVisibleParts() {
    for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
        DocumentLayoutPage* page = GetPage(pageNo);
        Rect pageRect = page->pos;
        Rect visiblePart = pageRect.Intersect(viewPort);
        page->visibleRatio = 0;
        if (!visiblePart.IsEmpty() && !pageRect.IsEmpty()) {
            page->visibleRatio =
                1.0f * (float)visiblePart.dx * (float)visiblePart.dy / ((float)pageRect.dx * (float)pageRect.dy);
        }
        page->pageOnScreen = pageRect;
        page->pageOnScreen.Offset(-viewPort.x, -viewPort.y);
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
        // No page overlaps the viewport at all. That is not only "before the
        // first page / after the last one": when one page is much wider than
        // the others the canvas is as wide as it and the narrow pages sit
        // centered in that canvas, so scrolled fully left the viewport misses
        // every page horizontally. Answering "the last page" there sent a
        // restored view to the end of the document (issue #1438), so go by the
        // vertical band the viewport is in, which is what "current page" means
        // in continuous mode
        mostVisiblePage = PageNoAtViewPortTop();
    }
    return mostVisiblePage;
}

// the page whose vertical band contains the top of the viewport (the last page
// when the viewport is past the end); ignores horizontal position
int DocumentLayout::PageNoAtViewPortTop() const {
    if (pages.len <= 0) {
        return 1;
    }
    for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
        const DocumentLayoutPage* page = GetPage(pageNo);
        if (page && viewPort.y < page->pos.y + page->pos.dy) {
            return pageNo;
        }
    }
    return pages.len;
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

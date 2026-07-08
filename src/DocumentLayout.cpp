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
        pages.SetSize(pageCount);
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
    if (zoomVirtual != kZoomFitWidth && zoomVirtual != kZoomFitPage) {
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

    float zoomX = areaForPagesDx / row.dx;
    float zoomY = areaForPagesDy / row.dy;
    return (zoomX < zoomY || zoomVirtual == kZoomFitWidth) ? zoomX : zoomY;
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
            if (minZoom > page->zoomReal) {
                minZoom = page->zoomReal;
            }
        }
        layout.zoomReal = minZoom == (float)HUGE_VAL ? 1 : minZoom;
        return;
    }

    if (zoomVirtual == kZoomFitWidth || zoomVirtual == kZoomFitPage) {
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
            DocumentLayoutPage* page = layout.GetPage(pageNo);
            if (!page->isShown) {
                continue;
            }
            float zoom = ZoomRealFromVirtualForPage(layout, zoomVirtual, pageNo);
            page->zoomReal = zoom;
            if (minZoom > zoom) {
                minZoom = zoom;
            }
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
    if (params.startPage < 1) {
        params.startPage = 1;
    }
    if (params.startPage > pages.len) {
        params.startPage = pages.len;
    }
    if (params.dpiFactor <= 0) {
        params.dpiFactor = 1;
    }
    if (params.zoomVirtual == kZoomFitByOrientation) {
        params.zoomVirtual = params.viewPortSize.dx > params.viewPortSize.dy ? kZoomFitWidth : kZoomFitPage;
    }
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
        pos.dx = (int)(pageSize.dx * zoom + 0.499f);
        pos.dy = (int)(pageSize.dy * zoom + 0.499f);
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
            page->pos.x = pageOffX + (columnMaxWidth[0] - page->pos.dx) / 2;
        } else if (pageInARow == 0) {
            page->pos.x = pageOffX + columnMaxWidth[0] - page->pos.dx;
        } else {
            page->pos.x = pageOffX;
        }
        if (IsBookView(params.displayMode) && pageNo == 1 && !IsContinuous(params.displayMode)) {
            page->pos.x = offX + params.windowMargin.left +
                          (columnMaxWidth[0] + params.pageSpacing.dx + columnMaxWidth[1] - page->pos.dx) / 2;
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
        int offY = params.windowMargin.top + (viewPort.dy - canvasDy) / 2;
        for (int pageNo = 1; pageNo <= pages.len; pageNo++) {
            DocumentLayoutPage* page = GetPage(pageNo);
            if (page->isShown) {
                page->pos.y += offY;
            }
        }
    }

    if (params.zoomVirtual == kZoomFitPage && !IsContinuous(params.displayMode)) {
        canvasDy = std::min(canvasDy, viewPort.dy);
        canvasDx = std::min(canvasDx, viewPort.dx);
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
            page->visibleRatio = 1.0f * visiblePart.dx * visiblePart.dy / ((float)pageRect.dx * pageRect.dy);
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
        const DocumentLayoutPage* first = GetPage(1);
        mostVisiblePage = (first && viewPort.y > first->pos.y + first->pos.dy) ? pages.len : 1;
    }
    return mostVisiblePage;
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

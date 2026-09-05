/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "DocProperties.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "PageRenderPolicy.h"
#include "PageRenderService.h"
#include "ReaderModel.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "gui/Gfx.h"
#include "gui/PlatformCanvas.h"
#include "gui/DocumentView.h"

struct DocumentViewData {
    ReaderModel* reader = nullptr;
    PageRenderService* renderer = nullptr;
    TextSelection* textSelection = nullptr;
    TextSearch* textSearch = nullptr;
    TocTree* toc = nullptr;
    Vec<TocItem*> tocItems;
    Vec<int> tocDepths;
    Props properties;
    bool propertiesLoaded = false;
    DocumentLayout layout;
    Size viewSize;
    Point viewOffset;
    float zoomVirtual = kZoomFitWidth;
    DisplayMode displayMode = DisplayMode::Continuous;
    int startPage = 1;
    int pendingPageNo = 0;
    int rotation = 0;
    bool isDragging = false;
    bool isSelectingText = false;
    IPageElement* pressedLink = nullptr;
    Point dragStart;
    Point dragOffset;
};

static DocumentViewData* ViewData(DocumentView* view) {
    return (DocumentViewData*)view->data;
}

static DocumentLayoutParams MakeLayoutParams(DocumentViewData* data) {
    DocumentLayoutParams p;
    p.displayMode = data->displayMode;
    p.startPage = data->startPage;
    p.viewPortSize = data->viewSize;
    p.viewPortOffset = data->viewOffset;
    p.zoomVirtual = data->zoomVirtual;
    p.dpiFactor = 72.0f / data->reader->FileDPI();
    p.rotation = data->rotation;
    p.windowMargin = {12, 12, 12, 12};
    p.pageSpacing = Size(0, 14);
    return p;
}

static void Relayout(DocumentView* view, Size viewSize) {
    auto* data = ViewData(view);
    if (!data->reader) {
        data->layout.Reset(0);
        data->viewSize = viewSize;
        data->viewOffset = {};
        return;
    }
    data->viewSize = viewSize;
    data->viewOffset.x = std::max(data->viewOffset.x, 0);
    data->viewOffset.y = std::max(data->viewOffset.y, 0);
    data->reader->Layout(MakeLayoutParams(data), &data->layout);
    data->viewOffset = {data->layout.viewPort.x, data->layout.viewPort.y};
}

static void Invalidate(DocumentView* view) {
    if (view->canvas) {
        view->canvas->Invalidate();
    }
}

static void NotifyStateChanged(DocumentView* view) {
    view->onStateChanged.Call();
}

static void OnPageReady(DocumentView* view) {
    Invalidate(view);
}

static void SetViewOffset(DocumentView* view, Point offset) {
    auto* data = ViewData(view);
    int oldPageNo = data->layout.CurrentPageNo();
    data->viewOffset = offset;
    Relayout(view, data->viewSize);
    Invalidate(view);
    if (oldPageNo != data->layout.CurrentPageNo()) {
        NotifyStateChanged(view);
    }
}

static int PageAtPoint(DocumentViewData* data, Point pt) {
    for (int pageNo = 1; pageNo <= len(data->layout.pages); pageNo++) {
        const DocumentLayoutPage* page = data->layout.GetPage(pageNo);
        if (page->isShown && page->pageOnScreen.Contains(pt)) {
            return pageNo;
        }
    }
    return data->layout.CurrentPageNo();
}

static bool PagePointAt(DocumentViewData* data, Point pt, int* pageNoOut, PointF* pagePointOut) {
    if (!data->reader) {
        return false;
    }
    int pageNo = PageAtPoint(data, pt);
    const DocumentLayoutPage* page = data->layout.GetPage(pageNo);
    if (!page || !page->isShown || !page->pageOnScreen.Contains(pt)) {
        return false;
    }
    PointF pagePoint((float)pt.x - 0.499f - (float)page->pageOnScreen.x,
                     (float)pt.y - 0.499f - (float)page->pageOnScreen.y);
    EngineBase* engine = data->reader->GetEngine();
    pagePoint = engine->Transform(pagePoint, pageNo, page->zoomReal, data->rotation, true);
    *pageNoOut = pageNo;
    *pagePointOut = pagePoint;
    return true;
}

static IPageElement* LinkAtPoint(DocumentViewData* data, Point pt) {
    int pageNo = 0;
    PointF pagePoint;
    if (!PagePointAt(data, pt, &pageNo, &pagePoint)) {
        return nullptr;
    }
    EngineBase* engine = data->reader->GetEngine();
    IPageElement* element = engine->GetElementAtPos(pageNo, pagePoint);
    return element && element->Is(kindPageElementDest) ? element : nullptr;
}

static bool TextAtPoint(DocumentViewData* data, Point pt, int* pageNoOut = nullptr, PointF* pagePointOut = nullptr) {
    int pageNo = 0;
    PointF pagePoint;
    if (!data->textSelection || !PagePointAt(data, pt, &pageNo, &pagePoint) ||
        !data->textSelection->IsOverGlyph(pageNo, pagePoint.x, pagePoint.y)) {
        return false;
    }
    if (pageNoOut) {
        *pageNoOut = pageNo;
    }
    if (pagePointOut) {
        *pagePointOut = pagePoint;
    }
    return true;
}

static CursorId CursorAtPoint(DocumentViewData* data, Point pt) {
    if (LinkAtPoint(data, pt)) {
        return CursorId::Hand;
    }
    return TextAtPoint(data, pt) ? CursorId::IBeam : CursorId::Arrow;
}

static void AppendTocItems(DocumentViewData* data, TocItem* item, int depth) {
    while (item) {
        data->tocItems.Append(item);
        data->tocDepths.Append(depth);
        AppendTocItems(data, item->child, depth + 1);
        item = item->next;
    }
}

static void LoadProperties(DocumentViewData* data) {
    if (data->propertiesLoaded || !data->reader) {
        return;
    }
    data->propertiesLoaded = true;
    Props engineProperties;
    data->reader->GetEngine()->GetProperties(engineProperties);
    for (const PropValue& value : engineProperties) {
        AddPropOwned(data->properties, value.prop, value.val);
    }
}

static void ScrollToDestination(DocumentView* view, int pageNo, RectF rect, float zoom) {
    auto* data = ViewData(view);
    if (!data->reader || pageNo < 1 || pageNo > data->reader->PageCount()) {
        return;
    }

    bool isVirtualZoom = zoom == kZoomFitPage || zoom == kZoomFitWidth || zoom == kZoomFitHeight ||
                         zoom == kZoomFitContent || zoom == kZoomShrinkToFit || zoom == kZoomFitByOrientation;
    if (isVirtualZoom) {
        view->SetZoom(zoom);
    } else if (zoom > 0) {
        view->SetZoom(zoom * 100.0f);
    }
    view->GoToPage(pageNo);

    DocumentLayoutPage* page = data->layout.GetPage(pageNo);
    if (!page) {
        return;
    }
    PointF destination = rect.TL();
    bool hasX = destination.x != kDestUseDefault;
    if (!hasX) {
        destination.x = 0;
    }
    if (destination.y == kDestUseDefault) {
        destination.y = 0;
    }
    PointF transformed = data->reader->GetEngine()->Transform(destination, pageNo, page->zoomReal, data->rotation);
    Point offset = data->viewOffset;
    if (hasX) {
        offset.x = page->pos.x + (int)transformed.x;
    }
    offset.y = page->pos.y + (int)transformed.y;
    SetViewOffset(view, offset);
}

struct DocumentViewLinkHandler : ILinkHandler {
    DocumentView* view = nullptr;

    explicit DocumentViewLinkHandler(DocumentView* view) : view(view) {}

    void GotoLink(IPageDestination* dest) override {
        if (!dest) {
            return;
        }
        Kind kind = dest->GetKind();
        if (kind == kindDestinationScrollTo) {
            ScrollTo(dest);
        } else if (kind == kindDestinationLaunchURL) {
            auto* urlDest = (PageDestinationURL*)dest;
            LaunchURL(urlDest->url);
        } else if (kind == kindDestinationLaunchFile) {
            auto* fileDest = (PageDestinationFile*)dest;
            LaunchFile(fileDest->path, dest);
        }
    }

    void GotoNamedDest(Str name) override {
        auto* data = ViewData(view);
        IPageDestination* dest = data->reader ? data->reader->GetEngine()->GetNamedDest(name) : nullptr;
        if (dest) {
            ScrollTo(dest);
        }
    }

    void GoToPage(int pageNo, bool) override { view->GoToPage(pageNo); }

    bool GoToNextPage() override {
        int oldPageNo = view->CurrentPageNo();
        view->GoToPage(oldPageNo + 1);
        return view->CurrentPageNo() != oldPageNo;
    }

    bool GoToPrevPage(bool) override {
        int oldPageNo = view->CurrentPageNo();
        view->GoToPage(oldPageNo - 1);
        return view->CurrentPageNo() != oldPageNo;
    }

    void ScrollTo(IPageDestination* dest) override {
        ScrollToDestination(view, PageDestGetPageNo(dest), PageDestGetRect(dest), PageDestGetZoom(dest));
    }

    void ScrollTo(int pageNo, RectF rect, float zoom) override { ScrollToDestination(view, pageNo, rect, zoom); }

    void LaunchURL(Str url) override {
        if (IsExternalUrl(url)) {
            view->onOpenUrl.Call(url);
        } else {
            view->onOpenFile.Call(url);
        }
    }

    void LaunchFile(Str path, IPageDestination*) override { view->onOpenFile.Call(path); }

    TocItem* FindTocItem(TocItem*, Str, bool) override { return nullptr; }
};

static void ActivateLink(DocumentView* view, IPageElement* element) {
    auto* data = ViewData(view);
    if (!data->reader || !element) {
        return;
    }
    IPageDestination* dest = element->AsLink();
    if (!dest) {
        return;
    }
    DocumentViewLinkHandler handler(view);
    data->reader->GetEngine()->HandleLink(dest, &handler);
}

static void OnPaint(DocumentView* view, PlatformCanvasPaintEvent* ev) {
    auto* data = ViewData(view);
    Size size = ev->clientRect.Size();
    if (size != data->viewSize) {
        float oldZoomReal = data->layout.zoomReal;
        Relayout(view, size);
        if (data->renderer && data->zoomVirtual < 0 && oldZoomReal != data->layout.zoomReal) {
            data->renderer->NewGeneration();
        }
        if (data->pendingPageNo > 0) {
            int pageNo = data->pendingPageNo;
            data->pendingPageNo = 0;
            view->GoToPage(pageNo);
        }
    }

    ev->gfx->FillRect(ev->clientRect, MkRgb(78, 81, 86));
    if (!data->reader) {
        return;
    }

    for (int pageNo = 1; pageNo <= len(data->layout.pages); pageNo++) {
        DocumentLayoutPage* page = data->layout.GetPage(pageNo);
        if (!page->isShown || page->visibleRatio <= 0) {
            continue;
        }

        Rect target = page->pageOnScreen;
        Rect shadow = target;
        shadow.Offset(3, 3);
        ev->gfx->FillRect(shadow, MkGray(40));
        ev->gfx->FillRect(target, kColWhite);

        PageRenderKey key{pageNo, page->zoomReal, data->rotation};
        if (!data->renderer->DrawPage(ev->gfx, key, target)) {
            data->renderer->Request(key, PageRenderPriority::Visible);
        }
        ev->gfx->DrawRect(target, MkGray(155));
    }

    if (data->textSelection && data->textSelection->result.len > 0) {
        Vec<Rect> selectionRects;
        TextSel& selection = data->textSelection->result;
        for (int i = 0; i < selection.len; i++) {
            int pageNo = selection.pages[i];
            DocumentLayoutPage* page = data->layout.GetPage(pageNo);
            if (!page || !page->isShown) {
                continue;
            }
            if (selection.quads && !selection.quads[i].IsEmpty()) {
                QuadF q = selection.quads[i];
                EngineBase* eng = data->reader->GetEngine();
                PointF corners[4] = {q.ul, q.ur, q.lr, q.ll};
                Point pts[4];
                for (int k = 0; k < 4; k++) {
                    PointF t = eng->Transform(corners[k], pageNo, page->zoomReal, data->rotation);
                    pts[k] = Point((int)t.x + page->pageOnScreen.x, (int)t.y + page->pageOnScreen.y);
                }
                ev->gfx->FillQuads(pts, 1, MkRgb(255, 225, 70), 115);
            } else {
                RectF transformed = data->reader->GetEngine()->Transform(ToRectF(selection.rects[i]), pageNo,
                                                                         page->zoomReal, data->rotation);
                Rect screenRect = transformed.Round();
                screenRect.Offset(page->pageOnScreen.x, page->pageOnScreen.y);
                selectionRects.Append(screenRect);
            }
        }
        if (len(selectionRects) > 0) {
            ev->gfx->FillRects(selectionRects.els, len(selectionRects), MkRgb(255, 225, 70), 115);
        }
    }

    int current = data->layout.CurrentPageNo();
    for (int distance = 1; distance <= 2; distance++) {
        PageRenderPriority priority = distance == 1 ? PageRenderPriority::Nearby : PageRenderPriority::Background;
        int candidates[] = {current - distance, current + distance};
        for (int pageNo : candidates) {
            DocumentLayoutPage* page = data->layout.GetPage(pageNo);
            if (page) {
                data->renderer->Request({pageNo, page->zoomReal, data->rotation}, priority);
            }
        }
    }
}

static void OnCanvasPaint(DocumentView* view, PlatformCanvasPaintEvent* ev) {
    OnPaint(view, ev);
}

static void ZoomBy(DocumentView* view, bool zoomIn, Point anchor) {
    auto* data = ViewData(view);
    if (!data->reader) {
        return;
    }
    float zoom = data->zoomVirtual;
    if (zoom <= 0) {
        float dpiFactor = 72.0f / data->reader->FileDPI();
        zoom = dpiFactor > 0 ? data->layout.zoomReal * 100.0f / dpiFactor : kZoomActualSize;
    }
    zoom *= zoomIn ? 1.2f : 1.0f / 1.2f;
    zoom = limitValue(zoom, kZoomMin, kZoomMax);
    view->SetZoom(zoom, &anchor);
}

static void OnCanvasScroll(DocumentView* view, PlatformCanvasScrollEvent* ev) {
    auto* data = ViewData(view);
    if (!data->reader) {
        return;
    }
    if (ev->isCtrl) {
        ZoomBy(view, ev->deltaY < 0, ev->pos);
    } else {
        Point offset = data->viewOffset;
        offset.x += (int)(ev->deltaX * 55.0);
        offset.y += (int)(ev->deltaY * 55.0);
        SetViewOffset(view, offset);
    }
    ev->didHandle = true;
}

static void OnCanvasPointer(DocumentView* view, PlatformCanvasPointerEvent* ev) {
    auto* data = ViewData(view);
    if (ev->type == PlatformCanvasPointerEventType::Down && ev->button == 1) {
        view->canvas->Focus();
        data->pressedLink = LinkAtPoint(data, ev->pos);
        data->isDragging = false;
        data->isSelectingText = false;
        int pageNo = 0;
        PointF pagePoint;
        if (!data->pressedLink && TextAtPoint(data, ev->pos, &pageNo, &pagePoint)) {
            data->textSelection->Reset();
            data->textSelection->StartAt(pageNo, pagePoint.x, pagePoint.y);
            data->textSelection->SelectUpTo(pageNo, pagePoint.x, pagePoint.y);
            data->isSelectingText = true;
            Invalidate(view);
        } else if (!data->pressedLink) {
            data->textSelection->Reset();
            data->isDragging = true;
            data->dragStart = ev->pos;
            data->dragOffset = data->viewOffset;
            Invalidate(view);
        }
        CursorId cursor = data->pressedLink ? CursorId::Hand : data->isSelectingText ? CursorId::IBeam : CursorId::Move;
        view->canvas->SetCursor(cursor);
        ev->didHandle = true;
    } else if (ev->type == PlatformCanvasPointerEventType::Move && data->isSelectingText) {
        int pageNo = 0;
        PointF pagePoint;
        if (PagePointAt(data, ev->pos, &pageNo, &pagePoint)) {
            data->textSelection->SelectUpTo(pageNo, pagePoint.x, pagePoint.y);
            Invalidate(view);
        }
        ev->didHandle = true;
    } else if (ev->type == PlatformCanvasPointerEventType::Move && data->isDragging) {
        Point delta(ev->pos.x - data->dragStart.x, ev->pos.y - data->dragStart.y);
        SetViewOffset(view, Point(data->dragOffset.x - delta.x, data->dragOffset.y - delta.y));
        ev->didHandle = true;
    } else if (ev->type == PlatformCanvasPointerEventType::Move) {
        view->canvas->SetCursor(CursorAtPoint(data, ev->pos));
    } else if (ev->type == PlatformCanvasPointerEventType::Leave && !data->isDragging && !data->isSelectingText) {
        view->canvas->SetCursor(CursorId::Arrow);
    } else if (ev->type == PlatformCanvasPointerEventType::Up && data->pressedLink) {
        IPageElement* releasedLink = LinkAtPoint(data, ev->pos);
        IPageElement* pressedLink = data->pressedLink;
        data->pressedLink = nullptr;
        if (releasedLink == pressedLink) {
            ActivateLink(view, pressedLink);
        }
        view->canvas->SetCursor(releasedLink ? CursorId::Hand : CursorId::Arrow);
        ev->didHandle = true;
    } else if (ev->type == PlatformCanvasPointerEventType::Up && data->isSelectingText) {
        int pageNo = 0;
        PointF pagePoint;
        if (PagePointAt(data, ev->pos, &pageNo, &pagePoint)) {
            data->textSelection->SelectUpTo(pageNo, pagePoint.x, pagePoint.y);
        }
        data->isSelectingText = false;
        view->canvas->SetCursor(CursorAtPoint(data, ev->pos));
        Invalidate(view);
        ev->didHandle = true;
    } else if (ev->type == PlatformCanvasPointerEventType::Up && data->isDragging) {
        data->isDragging = false;
        view->canvas->SetCursor(CursorAtPoint(data, ev->pos));
        ev->didHandle = true;
    }
}

static void ScrollBy(DocumentView* view, int dx, int dy) {
    auto* data = ViewData(view);
    SetViewOffset(view, Point(data->viewOffset.x + dx, data->viewOffset.y + dy));
}

static void ScrollByPage(DocumentView* view, int direction) {
    auto* data = ViewData(view);
    if (IsContinuous(data->displayMode)) {
        int pageStep = std::max(data->viewSize.dy - 60, 60);
        ScrollBy(view, 0, direction * pageStep);
    } else {
        view->GoToPage(data->startPage + direction);
    }
}

static void OnCanvasKey(DocumentView* view, PlatformCanvasKeyEvent* ev) {
    auto* data = ViewData(view);
    if (!data->reader) {
        return;
    }
    int pageStep = std::max(data->viewSize.dy - 60, 60);
    int horizontalPageStep = std::max(data->viewSize.dx - 60, 60);
    switch (ev->key) {
        case PlatformKey::Escape:
            if (view->HasTextSelection()) {
                data->textSelection->Reset();
                Invalidate(view);
                ev->didHandle = true;
            }
            return;
        case PlatformKey::Home:
            view->GoToPage(1);
            ev->didHandle = true;
            return;
        case PlatformKey::End:
            view->GoToPage(data->reader->PageCount());
            ev->didHandle = true;
            return;
        case PlatformKey::Enter:
            ScrollByPage(view, ev->isShift ? -1 : 1);
            ev->didHandle = true;
            return;
        case PlatformKey::PageUp:
            ScrollByPage(view, -1);
            ev->didHandle = true;
            return;
        case PlatformKey::PageDown:
            ScrollByPage(view, 1);
            ev->didHandle = true;
            return;
        case PlatformKey::Left:
            ScrollBy(view, ev->isShift ? -horizontalPageStep : -45, 0);
            ev->didHandle = true;
            return;
        case PlatformKey::Up:
            if (ev->isCtrl) {
                ScrollByPage(view, -1);
            } else {
                ScrollBy(view, 0, ev->isShift ? -(pageStep / 2) : -45);
            }
            ev->didHandle = true;
            return;
        case PlatformKey::Right:
            ScrollBy(view, ev->isShift ? horizontalPageStep : 45, 0);
            ev->didHandle = true;
            return;
        case PlatformKey::Down:
            if (ev->isCtrl) {
                ScrollByPage(view, 1);
            } else {
                ScrollBy(view, 0, ev->isShift ? pageStep / 2 : 45);
            }
            ev->didHandle = true;
            return;
        case PlatformKey::Plus:
            ZoomBy(view, true, Point(data->viewSize.dx / 2, data->viewSize.dy / 2));
            ev->didHandle = true;
            return;
        case PlatformKey::Minus:
            ZoomBy(view, false, Point(data->viewSize.dx / 2, data->viewSize.dy / 2));
            ev->didHandle = true;
            return;
        case PlatformKey::Zero:
            if (ev->isCtrl) {
                view->SetZoom(kZoomActualSize);
                ev->didHandle = true;
            }
            return;
        default:
            break;
    }

    int c = ev->codepoint;
    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
    }
    if (ev->isCtrl && c == 'c') {
        view->CopySelection();
    } else if (ev->isCtrl || ev->isAlt) {
        return;
    } else if (c == ' ') {
        ScrollByPage(view, ev->isShift ? -1 : 1);
    } else if (c == 'h') {
        ScrollBy(view, -45, 0);
    } else if (c == 'j') {
        ScrollBy(view, 0, 45);
    } else if (c == 'k') {
        ScrollBy(view, 0, -45);
    } else if (c == 'l') {
        ScrollBy(view, 45, 0);
    } else if (c == 'n') {
        view->GoToPage(data->startPage + 1);
    } else if (c == 'p') {
        view->GoToPage(data->startPage - 1);
    } else if (c == 'c') {
        view->SetContinuous(!view->IsContinuous());
    } else if (c == 'f') {
        view->SetZoom(kZoomFitPage);
    } else if (c == 'w') {
        view->SetZoom(kZoomFitWidth);
    } else if (c == '1') {
        view->SetZoom(kZoomActualSize);
    } else if (c == 'r') {
        view->RotateBy(ev->isShift ? -90 : 90);
    } else {
        return;
    }
    ev->didHandle = true;
}

DocumentView* DocumentView::Create() {
    auto* view = new DocumentView();
    view->data = new DocumentViewData();
    view->canvas = PlatformCanvas::Create(view);
    if (!view->canvas) {
        delete ViewData(view);
        delete view;
        return nullptr;
    }
    view->canvas->onPaint = MkFunc1(OnCanvasPaint, view);
    view->canvas->onPointer = MkFunc1(OnCanvasPointer, view);
    view->canvas->onScroll = MkFunc1(OnCanvasScroll, view);
    view->canvas->onKey = MkFunc1(OnCanvasKey, view);
    return view;
}

DocumentView::~DocumentView() {
    auto* viewData = ViewData(this);
    if (viewData) {
        FreeProps(viewData->properties);
        delete viewData->renderer;
        delete viewData->textSelection;
        delete viewData->textSearch;
        delete viewData->reader;
        delete viewData;
    }
    delete canvas;
    canvas = nullptr;
    data = nullptr;
}

bool DocumentView::Open(Str path, PasswordUI* pwdUI) {
    auto* viewData = ViewData(this);
    ReaderModel* reader = ReaderModel::Create(path, pwdUI);
    if (!reader) {
        return false;
    }
    PageRenderService* renderer = PageRenderService::Create(reader->GetEngine(), MkFunc0(OnPageReady, this));
    if (!renderer) {
        delete reader;
        return false;
    }
    auto* textSelection = new TextSelection(reader->GetEngine());
    auto* textSearch = new TextSearch(reader->GetEngine());
    TocTree* toc = reader->GetEngine()->GetToc();
    delete viewData->renderer;
    delete viewData->textSelection;
    delete viewData->textSearch;
    delete viewData->reader;
    viewData->reader = reader;
    viewData->renderer = renderer;
    viewData->textSelection = textSelection;
    viewData->textSearch = textSearch;
    viewData->toc = toc;
    viewData->tocItems.Reset();
    viewData->tocDepths.Reset();
    if (toc && toc->root) {
        AppendTocItems(viewData, toc->root->child, 0);
    }
    FreeProps(viewData->properties);
    viewData->propertiesLoaded = false;
    viewData->viewOffset = {};
    viewData->startPage = 1;
    viewData->pendingPageNo = 0;
    Relayout(this, canvas->ClientRect().Size());
    Invalidate(this);
    NotifyStateChanged(this);
    return true;
}

void* DocumentView::NativeWidget() const {
    return canvas ? canvas->NativeWidget() : nullptr;
}

void DocumentView::Focus() {
    if (canvas) {
        canvas->Focus();
    }
}

int DocumentView::PageCount() const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->reader ? viewData->reader->PageCount() : 0;
}

int DocumentView::CurrentPageNo() const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->reader ? viewData->layout.CurrentPageNo() : 0;
}

void DocumentView::GoToPage(int pageNo) {
    auto* viewData = ViewData(this);
    if (!viewData->reader) {
        return;
    }
    pageNo = limitValue(pageNo, 1, viewData->reader->PageCount());
    viewData->startPage = pageNo;
    if (viewData->viewSize.dx <= 0 || viewData->viewSize.dy <= 0) {
        viewData->pendingPageNo = pageNo;
        return;
    }
    if (::IsContinuous(viewData->displayMode)) {
        DocumentLayoutPage* page = viewData->layout.GetPage(pageNo);
        if (page) {
            viewData->viewOffset.y = page->pos.y;
        }
    } else {
        viewData->viewOffset = {};
        viewData->renderer->NewGeneration();
    }
    Relayout(this, viewData->viewSize);
    Invalidate(this);
    NotifyStateChanged(this);
}

void DocumentView::SetContinuous(bool continuous) {
    auto* viewData = ViewData(this);
    if (!viewData->reader || continuous == ::IsContinuous(viewData->displayMode)) {
        return;
    }
    viewData->startPage = viewData->layout.CurrentPageNo();
    viewData->displayMode = continuous ? DisplayMode::Continuous : DisplayMode::SinglePage;
    viewData->viewOffset = {};
    viewData->renderer->NewGeneration();
    Relayout(this, viewData->viewSize);
    Invalidate(this);
    NotifyStateChanged(this);
}

bool DocumentView::IsContinuous() const {
    return ::IsContinuous(ViewData((DocumentView*)this)->displayMode);
}

void DocumentView::SetZoom(float zoomVirtual, Point* anchor) {
    auto* viewData = ViewData(this);
    if (!viewData->reader || !IsValidZoom(zoomVirtual)) {
        return;
    }

    Point fix = anchor ? *anchor : Point(viewData->viewSize.dx / 2, viewData->viewSize.dy / 2);
    int pageNo = PageAtPoint(viewData, fix);
    DocumentLayoutPage* oldPage = viewData->layout.GetPage(pageNo);
    float relX = oldPage && oldPage->pageOnScreen.dx > 0
                     ? (float)(fix.x - oldPage->pageOnScreen.x) / oldPage->pageOnScreen.dx
                     : 0.5f;
    float relY = oldPage && oldPage->pageOnScreen.dy > 0
                     ? (float)(fix.y - oldPage->pageOnScreen.y) / oldPage->pageOnScreen.dy
                     : 0.5f;

    viewData->zoomVirtual = zoomVirtual;
    viewData->renderer->NewGeneration();
    Relayout(this, viewData->viewSize);
    DocumentLayoutPage* newPage = viewData->layout.GetPage(pageNo);
    if (anchor && newPage) {
        viewData->viewOffset.x = newPage->pos.x + (int)(relX * newPage->pos.dx) - fix.x;
        viewData->viewOffset.y = newPage->pos.y + (int)(relY * newPage->pos.dy) - fix.y;
        Relayout(this, viewData->viewSize);
    }
    Invalidate(this);
    NotifyStateChanged(this);
}

float DocumentView::Zoom() const {
    return ViewData((DocumentView*)this)->zoomVirtual;
}

void DocumentView::RotateBy(int degrees) {
    auto* viewData = ViewData(this);
    if (!viewData->reader) {
        return;
    }
    viewData->rotation = NormalizeRotation(viewData->rotation + degrees);
    viewData->renderer->NewGeneration();
    Relayout(this, viewData->viewSize);
    Invalidate(this);
    NotifyStateChanged(this);
}

int DocumentView::Rotation() const {
    return ViewData((DocumentView*)this)->rotation;
}

bool DocumentView::CanPrint() const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->reader && viewData->reader->GetEngine()->AllowsPrinting();
}

RectF DocumentView::PageMediabox(int pageNo) const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->reader ? viewData->reader->PageMediabox(pageNo) : RectF{};
}

float DocumentView::FileDPI() const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->reader ? viewData->reader->FileDPI() : 96.0f;
}

Pixmap* DocumentView::RenderPageForPrint(int pageNo, float zoom) const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->reader ? viewData->reader->RenderPageForPrint(pageNo, zoom, viewData->rotation) : nullptr;
}

bool DocumentView::HasTextSelection() const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->textSelection && viewData->textSelection->result.len > 0;
}

void DocumentView::CopySelection() {
    auto* viewData = ViewData(this);
    if (!HasTextSelection()) {
        return;
    }
    Str text = viewData->textSelection->ExtractText(StrL("\n"));
    if (text) {
        onCopyText.Call(text);
    }
    str::Free(text);
}

void DocumentView::SelectAll() {
    auto* viewData = ViewData(this);
    if (!viewData->textSelection || !viewData->reader) {
        return;
    }
    viewData->textSelection->Reset();
    viewData->textSelection->StartAt(1, 0);
    viewData->textSelection->SelectUpTo(viewData->reader->PageCount(), -1);
    Invalidate(this);
}

bool DocumentView::FindText(Str text, bool forward, bool restart) {
    auto* viewData = ViewData(this);
    if (!viewData->textSearch || !viewData->textSelection || len(text) == 0) {
        return false;
    }

    TextSearch* search = viewData->textSearch;
    bool newText = !str::Eq(search->lastText, text);
    search->SetDirection(forward ? TextSearch::Direction::Forward : TextSearch::Direction::Backward);
    TextSel* result = nullptr;
    if (restart || newText || len(search->findText) == 0) {
        result = search->FindFirst(CurrentPageNo(), text);
    } else {
        result = search->FindNext();
    }
    if (!result) {
        int wrapPage = forward ? search->RestrictFirst() : search->RestrictLast();
        result = search->FindFirst(wrapPage, text);
    }
    if (!result) {
        viewData->textSelection->Reset();
        Invalidate(this);
        return false;
    }

    viewData->textSelection->CopySelection(search);
    GoToPage(search->GetSearchHitStartPageNo());
    Invalidate(this);
    return true;
}

int DocumentView::TocItemCount() const {
    return len(ViewData((DocumentView*)this)->tocItems);
}

Str DocumentView::TocItemTitle(int index) const {
    auto* viewData = ViewData((DocumentView*)this);
    if (index < 0 || index >= len(viewData->tocItems)) {
        return {};
    }
    return viewData->tocItems[index]->title;
}

int DocumentView::TocItemDepth(int index) const {
    auto* viewData = ViewData((DocumentView*)this);
    if (index < 0 || index >= len(viewData->tocDepths)) {
        return 0;
    }
    return viewData->tocDepths[index];
}

bool DocumentView::GoToTocItem(int index) {
    auto* viewData = ViewData(this);
    if (!viewData->reader || index < 0 || index >= len(viewData->tocItems)) {
        return false;
    }
    TocItem* item = viewData->tocItems[index];
    IPageDestination* dest = item->GetPageDestination();
    if (dest) {
        DocumentViewLinkHandler handler(this);
        viewData->reader->GetEngine()->HandleLink(dest, &handler);
        return true;
    }
    if (item->pageNo >= 1 && item->pageNo <= viewData->reader->PageCount()) {
        GoToPage(item->pageNo);
        return true;
    }
    return false;
}

int DocumentView::PropertyCount() {
    auto* viewData = ViewData(this);
    LoadProperties(viewData);
    return len(viewData->properties);
}

Str DocumentView::PropertyName(int index) {
    auto* viewData = ViewData(this);
    LoadProperties(viewData);
    if (index < 0 || index >= len(viewData->properties)) {
        return {};
    }
    return PropNameTemp(viewData->properties[index].prop);
}

Str DocumentView::PropertyValue(int index) {
    auto* viewData = ViewData(this);
    LoadProperties(viewData);
    if (index < 0 || index >= len(viewData->properties)) {
        return {};
    }
    return viewData->properties[index].val;
}

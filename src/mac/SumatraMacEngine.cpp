#include "base/Base.h"
#include "base/Pixmap.h"

#include "Settings.h"
#include "Commands.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "DocProperties.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "PageRenderPolicy.h"
#include "PageRenderService.h"
#include "ProgressUpdateUI.h"
#include "ReaderModel.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "gui/CommandPaletteModel.h"
#include "mac/SumatraMacEngine.h"

void _uploadDebugReport(Str, Str, bool, bool) {}

void log(Str s) {
    if (!s) {
        return;
    }
    fwrite(s.s, 1, (size_t)s.len, stderr);
}

void loga(Str s) {
    log(s);
}

struct EBookUI;
EBookUI* GetEBookUI() {
    return nullptr;
}

struct FileEBookUI;
FileEBookUI* GetFileEBookUI(Str) {
    return nullptr;
}

static char* DupCString(Str s) {
    char* res = (char*)malloc((size_t)len(s) + 1);
    if (res) {
        memcpy(res, s.s, (size_t)len(s));
        res[len(s)] = 0;
    }
    return res;
}

static char* DupCString(const char* s) {
    return DupCString(Str((char*)s));
}

struct MacDocument {
    ReaderModel* model = nullptr;
    PageRenderService* renderer = nullptr;
    TextSelection* textSelection = nullptr;
    TextSearch* textSearch = nullptr;
    TocTree* toc = nullptr;
    Vec<TocItem*> tocItems;
    Vec<int> tocDepths;
    Props properties;
    bool propertiesLoaded = false;
    MacPageReadyCallback onPageReady = nullptr;
    void* callbackContext = nullptr;
};

static MacDocument* AsDocument(void* document) {
    return (MacDocument*)document;
}

static void OnPageReady(MacDocument* document) {
    if (document->onPageReady) {
        document->onPageReady(document->callbackContext);
    }
}

static void AppendTocItems(MacDocument* document, TocItem* item, int depth) {
    while (item) {
        document->tocItems.Append(item);
        document->tocDepths.Append(depth);
        AppendTocItems(document, item->child, depth + 1);
        item = item->next;
    }
}

static void LoadProperties(MacDocument* document) {
    if (document->propertiesLoaded) {
        return;
    }
    document->propertiesLoaded = true;
    document->model->GetEngine()->GetProperties(document->properties);
}

static PointF ToPagePoint(MacDocument* document, int pageNo, double x, double y, double zoom, int rotation) {
    PointF point((float)x, (float)y);
    return document->model->GetEngine()->Transform(point, pageNo, (float)zoom, rotation, true);
}

static int ResultRectCount(const TextSel& result, int pageNo) {
    int count = 0;
    for (int i = 0; i < result.len; i++) {
        if (result.pages[i] == pageNo) {
            count++;
        }
    }
    return count;
}

static bool TransformResultRect(MacDocument* document, const TextSel& result, int pageNo, int index, double zoom,
                                int rotation, MacDisplayRect* rect) {
    if (!rect || index < 0) {
        return false;
    }
    for (int i = 0; i < result.len; i++) {
        if (result.pages[i] != pageNo) {
            continue;
        }
        if (index-- != 0) {
            continue;
        }
        RectF transformed =
            document->model->GetEngine()->Transform(ToRectF(result.rects[i]), pageNo, (float)zoom, rotation);
        rect->x = transformed.x;
        rect->y = transformed.y;
        rect->width = transformed.dx;
        rect->height = transformed.dy;
        return true;
    }
    return false;
}

static bool CopyPixmap(Pixmap* pixmap, MacRenderedPage* page) {
    if (!pixmap || !pixmap->data) {
        return false;
    }

    if (pixmap->format == PixmapFormat::BGR8) {
        size_t stride = (((size_t)pixmap->width * 4) + 3) & ~(size_t)3;
        size_t nBytes = stride * (size_t)pixmap->height;
        auto* data = (unsigned char*)malloc(nBytes);
        if (!data) {
            return false;
        }
        for (int y = 0; y < pixmap->height; y++) {
            const unsigned char* src = pixmap->data + ((size_t)y * (size_t)pixmap->stride);
            unsigned char* dst = data + ((size_t)y * stride);
            for (int x = 0; x < pixmap->width; x++) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
                src += 3;
                dst += 4;
            }
        }
        page->width = pixmap->width;
        page->height = pixmap->height;
        page->stride = (int)stride;
        page->premultiplied = true;
        page->data = data;
        return true;
    }

    if (pixmap->format != PixmapFormat::BGRA8) {
        return false;
    }

    size_t nBytes = (size_t)pixmap->stride * (size_t)pixmap->height;
    auto* data = (unsigned char*)malloc(nBytes);
    if (!data) {
        return false;
    }

    memcpy(data, pixmap->data, nBytes);
    page->width = pixmap->width;
    page->height = pixmap->height;
    page->stride = pixmap->stride;
    page->premultiplied = pixmap->premultiplied;
    page->data = data;
    return true;
}

// Opens a document. Returns an opaque handle, or nullptr on failure; on failure
// *errorOut (if non-null) is set to a malloc'd message the caller must free().
void* MacOpenDocument(const char* path, MacPageReadyCallback onPageReady, void* callbackContext, char** errorOut) {
    if (!path || !path[0]) {
        if (errorOut) {
            *errorOut = DupCString("Pass a document path on the command line.");
        }
        return nullptr;
    }

    ReaderModel* model = ReaderModel::Create(Str((char*)path));
    if (!model) {
        if (errorOut) {
            *errorOut = DupCString("Could not open the document.");
        }
        return nullptr;
    }
    auto* document = new MacDocument();
    document->model = model;
    document->textSelection = new TextSelection(model->GetEngine());
    document->textSearch = new TextSearch(model->GetEngine());
    document->toc = model->GetEngine()->GetToc();
    if (document->toc && document->toc->root) {
        AppendTocItems(document, document->toc->root->child, 0);
    }
    document->onPageReady = onPageReady;
    document->callbackContext = callbackContext;
    document->renderer = PageRenderService::Create(model->GetEngine(), MkFunc0(OnPageReady, document));
    if (!document->renderer) {
        delete document->textSelection;
        delete document->textSearch;
        delete document->toc;
        delete model;
        delete document;
        if (errorOut) {
            *errorOut = DupCString("Could not start the page renderer.");
        }
        return nullptr;
    }
    return document;
}

// Number of pages, or 0 if the handle is invalid.
int MacPageCount(void* document) {
    if (!document) {
        return 0;
    }
    return AsDocument(document)->model->PageCount();
}

// Mediabox size of pageNo (1-based) in points. Returns false if invalid.
bool MacPageSize(void* document, int pageNo, double* widthOut, double* heightOut) {
    if (!document) {
        return false;
    }
    ReaderModel* model = AsDocument(document)->model;
    if (pageNo < 1 || pageNo > model->PageCount()) {
        return false;
    }
    RectF mb = model->PageMediabox(pageNo);
    if (widthOut) {
        *widthOut = mb.dx;
    }
    if (heightOut) {
        *heightOut = mb.dy;
    }
    return true;
}

double MacFileDPI(void* document) {
    if (!document) {
        return 96.0;
    }
    return AsDocument(document)->model->FileDPI();
}

bool MacLayoutDocument(void* document, const MacLayoutParams* params, MacDocumentLayout* layout) {
    if (!document || !params || !layout) {
        return false;
    }
    *layout = {};

    ReaderModel* model = AsDocument(document)->model;
    int pageCount = model->PageCount();
    if (pageCount <= 0) {
        return false;
    }

    DocumentLayoutParams p;
    p.displayMode = params->continuous ? DisplayMode::Continuous : DisplayMode::SinglePage;
    p.startPage = params->startPage;
    p.viewPortSize = Size(params->viewWidth, params->viewHeight);
    p.viewPortOffset = Point(params->viewX, params->viewY);
    p.zoomVirtual = (float)params->zoomVirtual;
    p.dpiFactor = 72.0f / model->FileDPI();
    p.rotation = params->rotation;
    p.windowMargin = {12, 12, 12, 12};
    p.pageSpacing = Size(0, 14);
    DocumentLayout docLayout;
    if (!model->Layout(p, &docLayout)) {
        return false;
    }

    auto* pages = (MacLayoutPage*)malloc(sizeof(MacLayoutPage) * (size_t)pageCount);
    if (!pages) {
        return false;
    }
    memset(pages, 0, sizeof(MacLayoutPage) * (size_t)pageCount);
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        const DocumentLayoutPage* page = docLayout.GetPage(pageNo);
        MacLayoutPage* dst = &pages[pageNo - 1];
        dst->pageNo = pageNo;
        dst->x = page->pos.x;
        dst->y = page->pos.y;
        dst->width = page->pos.dx;
        dst->height = page->pos.dy;
        dst->screenX = page->pageOnScreen.x;
        dst->screenY = page->pageOnScreen.y;
        dst->screenWidth = page->pageOnScreen.dx;
        dst->screenHeight = page->pageOnScreen.dy;
        dst->visibleRatio = page->visibleRatio;
        dst->layoutZoom = page->zoomReal;
        dst->renderZoom = page->zoomReal * params->backingScale;
        dst->shown = page->isShown;
    }

    layout->pageCount = pageCount;
    layout->currentPage = docLayout.CurrentPageNo();
    layout->canvasWidth = docLayout.canvasSize.dx;
    layout->canvasHeight = docLayout.canvasSize.dy;
    layout->pages = pages;
    return true;
}

// Renders pageNo (1-based) at the given zoom and rotation (0/90/180/270).
// Fills *page (caller frees with MacFreeRenderedPage); returns false on failure.
bool MacRenderPage(void* document, int pageNo, float zoom, int rotation, MacRenderedPage* page) {
    if (!page) {
        return false;
    }
    *page = {};
    if (!document) {
        return false;
    }
    ReaderModel* model = AsDocument(document)->model;
    if (pageNo < 1 || pageNo > model->PageCount()) {
        return false;
    }
    if (zoom <= 0) {
        zoom = 1.0f;
    }

    Pixmap* pixmap = model->RenderPage(pageNo, zoom, rotation);
    bool ok = CopyPixmap(pixmap, page);
    FreePixmap(pixmap);
    return ok;
}

void MacRequestPage(void* document, int pageNo, float zoom, int rotation, int priority) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->renderer || pageNo < 1 || pageNo > doc->model->PageCount()) {
        return;
    }
    PageRenderPriority renderPriority = PageRenderPriority::Background;
    if (priority <= 0) {
        renderPriority = PageRenderPriority::Visible;
    } else if (priority == 1) {
        renderPriority = PageRenderPriority::Nearby;
    }
    doc->renderer->Request({pageNo, zoom, rotation}, renderPriority);
}

bool MacCopyRenderedPage(void* document, int pageNo, float zoom, int rotation, MacRenderedPage* page) {
    if (!page) {
        return false;
    }
    *page = {};
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->renderer) {
        return false;
    }
    Pixmap* pixmap = doc->renderer->CopyPage({pageNo, zoom, rotation});
    bool ok = CopyPixmap(pixmap, page);
    FreePixmap(pixmap);
    return ok;
}

void MacResetRenderer(void* document) {
    MacDocument* doc = AsDocument(document);
    if (doc && doc->renderer) {
        doc->renderer->NewGeneration();
    }
}

bool MacFindText(void* document, int currentPage, const char* text, bool forward, bool restart) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSearch || !text || !text[0]) {
        return false;
    }
    TextSearch* search = doc->textSearch;
    Str query((char*)text);
    bool newText = !str::Eq(search->lastText, query);
    search->SetDirection(forward ? TextSearch::Direction::Forward : TextSearch::Direction::Backward);
    TextSel* result = nullptr;
    if (restart || newText || !search->findText) {
        result = search->FindFirst(currentPage, query);
    } else {
        result = search->FindNext();
    }
    if (!result) {
        int wrapPage = forward ? search->RestrictFirst() : search->RestrictLast();
        result = search->FindFirst(wrapPage, query);
    }
    if (!result) {
        search->Reset();
    }
    return result != nullptr;
}

int MacFindResultPage(void* document) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSearch || doc->textSearch->result.len == 0) {
        return 0;
    }
    return doc->textSearch->GetSearchHitStartPageNo();
}

int MacFindResultRectCount(void* document, int pageNo) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSearch) {
        return 0;
    }
    return ResultRectCount(doc->textSearch->result, pageNo);
}

bool MacFindResultRect(void* document, int pageNo, int index, double zoom, int rotation, MacDisplayRect* rect) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSearch || !rect || index < 0) {
        return false;
    }
    return TransformResultRect(doc, doc->textSearch->result, pageNo, index, zoom, rotation, rect);
}

bool MacTextAtPoint(void* document, int pageNo, double x, double y, double zoom, int rotation) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSelection || pageNo < 1 || pageNo > doc->model->PageCount()) {
        return false;
    }
    PointF point = ToPagePoint(doc, pageNo, x, y, zoom, rotation);
    return doc->textSelection->IsOverGlyph(pageNo, point.x, point.y);
}

bool MacStartSelection(void* document, int pageNo, double x, double y, double zoom, int rotation) {
    MacDocument* doc = AsDocument(document);
    if (!MacTextAtPoint(document, pageNo, x, y, zoom, rotation)) {
        return false;
    }
    PointF point = ToPagePoint(doc, pageNo, x, y, zoom, rotation);
    doc->textSelection->Reset();
    doc->textSelection->StartAt(pageNo, point.x, point.y);
    return true;
}

bool MacUpdateSelection(void* document, int pageNo, double x, double y, double zoom, int rotation) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSelection || doc->textSelection->startPage < 1 || pageNo < 1 ||
        pageNo > doc->model->PageCount()) {
        return false;
    }
    PointF point = ToPagePoint(doc, pageNo, x, y, zoom, rotation);
    doc->textSelection->SelectUpTo(pageNo, point.x, point.y);
    return true;
}

void MacSelectAll(void* document) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSelection) {
        return;
    }
    doc->textSelection->Reset();
    doc->textSelection->StartAt(1, 0);
    doc->textSelection->SelectUpTo(doc->model->PageCount(), -1);
}

bool MacHasSelection(void* document) {
    MacDocument* doc = AsDocument(document);
    return doc && doc->textSelection && doc->textSelection->result.len > 0;
}

int MacSelectionRectCount(void* document, int pageNo) {
    MacDocument* doc = AsDocument(document);
    return doc && doc->textSelection ? ResultRectCount(doc->textSelection->result, pageNo) : 0;
}

bool MacSelectionRect(void* document, int pageNo, int index, double zoom, int rotation, MacDisplayRect* rect) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSelection) {
        return false;
    }
    return TransformResultRect(doc, doc->textSelection->result, pageNo, index, zoom, rotation, rect);
}

char* MacCopySelectionText(void* document) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !doc->textSelection || doc->textSelection->result.len == 0) {
        return nullptr;
    }
    return DupCString(doc->textSelection->ExtractText(StrL("\n")));
}

bool MacLinkAtPoint(void* document, int pageNo, double x, double y, double zoom, int rotation, MacLink* link) {
    MacDocument* doc = AsDocument(document);
    if (!doc || !link || pageNo < 1 || pageNo > doc->model->PageCount()) {
        return false;
    }
    *link = {};
    PointF point = ToPagePoint(doc, pageNo, x, y, zoom, rotation);
    IPageElement* element = doc->model->GetEngine()->GetElementAtPos(pageNo, point);
    IPageDestination* dest = element ? element->AsLink() : nullptr;
    if (!dest) {
        return false;
    }
    int targetPage = PageDestGetPageNo(dest);
    if (targetPage >= 1 && targetPage <= doc->model->PageCount()) {
        link->kind = MacLinkKind::Page;
        link->pageNo = targetPage;
        return true;
    }
    Str value = PageDestGetValue(dest);
    if (!value) {
        return false;
    }
    Kind kind = dest->GetKind();
    link->kind = kind == kindDestinationLaunchFile ? MacLinkKind::File : MacLinkKind::Url;
    link->value = DupCString(value);
    return link->value != nullptr;
}

void MacFreeLink(MacLink* link) {
    if (!link) {
        return;
    }
    free(link->value);
    *link = {};
}

void* MacCreateCommandPalette() {
    const int commands[] = {
        CmdOpenFile,
        CmdCloseCurrentDocument,
        CmdReopenLastClosedFile,
        CmdNextTab,
        CmdPrevTab,
        CmdPrint,
        CmdShowInFolder,
        CmdProperties,
        CmdSinglePageView,
        CmdToggleContinuousView,
        CmdRotateLeft,
        CmdRotateRight,
        CmdToggleFullscreen,
        CmdCopySelection,
        CmdSelectAll,
        CmdGoToNextPage,
        CmdGoToPrevPage,
        CmdGoToFirstPage,
        CmdGoToLastPage,
        CmdGoToPage,
        CmdFindFirst,
        CmdFindNext,
        CmdFindPrev,
        CmdZoomFitPage,
        CmdZoomActualSize,
        CmdZoomFitWidth,
        CmdZoomIn,
        CmdZoomOut,
        CmdToggleBookmarks,
        CmdToggleKeyboardHelp,
    };
    auto* palette = new CommandPaletteModel();
    palette->SetCommands(commands, dimofi(commands));
    return palette;
}

void MacFilterCommandPalette(void* palette, const char* query) {
    if (!palette) {
        return;
    }
    ((CommandPaletteModel*)palette)->Filter(query ? Str((char*)query) : Str{});
}

int MacCommandPaletteCount(void* palette) {
    return palette ? ((CommandPaletteModel*)palette)->Count() : 0;
}

char* MacCopyCommandPaletteItem(void* palette, int index) {
    return palette ? DupCString(((CommandPaletteModel*)palette)->ItemText(index)) : nullptr;
}

int MacCommandPaletteItemCommand(void* palette, int index) {
    return palette ? ((CommandPaletteModel*)palette)->ItemCommandId(index) : 0;
}

MacCommandAction MacCommandPaletteAction(int commandId) {
    switch (commandId) {
        case CmdOpenFile:
            return MacCommandAction::Open;
        case CmdCloseCurrentDocument:
            return MacCommandAction::Close;
        case CmdReopenLastClosedFile:
            return MacCommandAction::ReopenClosed;
        case CmdNextTab:
            return MacCommandAction::NextTab;
        case CmdPrevTab:
            return MacCommandAction::PreviousTab;
        case CmdPrint:
            return MacCommandAction::Print;
        case CmdShowInFolder:
            return MacCommandAction::ShowInFolder;
        case CmdProperties:
            return MacCommandAction::Properties;
        case CmdSinglePageView:
            return MacCommandAction::SinglePage;
        case CmdToggleContinuousView:
            return MacCommandAction::ToggleContinuous;
        case CmdRotateLeft:
            return MacCommandAction::RotateLeft;
        case CmdRotateRight:
            return MacCommandAction::RotateRight;
        case CmdToggleFullscreen:
            return MacCommandAction::Fullscreen;
        case CmdCopySelection:
            return MacCommandAction::Copy;
        case CmdSelectAll:
            return MacCommandAction::SelectAll;
        case CmdGoToNextPage:
            return MacCommandAction::NextPage;
        case CmdGoToPrevPage:
            return MacCommandAction::PreviousPage;
        case CmdGoToFirstPage:
            return MacCommandAction::FirstPage;
        case CmdGoToLastPage:
            return MacCommandAction::LastPage;
        case CmdGoToPage:
            return MacCommandAction::GoToPage;
        case CmdFindFirst:
            return MacCommandAction::Find;
        case CmdFindNext:
            return MacCommandAction::FindNext;
        case CmdFindPrev:
            return MacCommandAction::FindPrevious;
        case CmdZoomFitPage:
            return MacCommandAction::FitPage;
        case CmdZoomActualSize:
            return MacCommandAction::ActualSize;
        case CmdZoomFitWidth:
            return MacCommandAction::FitWidth;
        case CmdZoomIn:
            return MacCommandAction::ZoomIn;
        case CmdZoomOut:
            return MacCommandAction::ZoomOut;
        case CmdToggleBookmarks:
            return MacCommandAction::Toc;
        case CmdToggleKeyboardHelp:
            return MacCommandAction::KeyboardHelp;
    }
    return MacCommandAction::None;
}

void MacDestroyCommandPalette(void* palette) {
    delete (CommandPaletteModel*)palette;
}

int MacTocItemCount(void* document) {
    MacDocument* doc = AsDocument(document);
    return doc ? len(doc->tocItems) : 0;
}

char* MacCopyTocItemTitle(void* document, int index) {
    MacDocument* doc = AsDocument(document);
    if (!doc || index < 0 || index >= len(doc->tocItems)) {
        return nullptr;
    }
    return DupCString(doc->tocItems[index]->title);
}

int MacTocItemDepth(void* document, int index) {
    MacDocument* doc = AsDocument(document);
    if (!doc || index < 0 || index >= len(doc->tocDepths)) {
        return 0;
    }
    return doc->tocDepths[index];
}

int MacTocItemPage(void* document, int index) {
    MacDocument* doc = AsDocument(document);
    if (!doc || index < 0 || index >= len(doc->tocItems)) {
        return 0;
    }
    TocItem* item = doc->tocItems[index];
    IPageDestination* dest = item->GetPageDestination();
    int pageNo = dest ? PageDestGetPageNo(dest) : item->pageNo;
    return pageNo >= 1 && pageNo <= doc->model->PageCount() ? pageNo : 0;
}

int MacPropertyCount(void* document) {
    MacDocument* doc = AsDocument(document);
    if (!doc) {
        return 0;
    }
    LoadProperties(doc);
    return len(doc->properties);
}

char* MacCopyPropertyName(void* document, int index) {
    MacDocument* doc = AsDocument(document);
    if (!doc) {
        return nullptr;
    }
    LoadProperties(doc);
    if (index < 0 || index >= len(doc->properties)) {
        return nullptr;
    }
    return DupCString(PropNameTemp(doc->properties[index].prop));
}

char* MacCopyPropertyValue(void* document, int index) {
    MacDocument* doc = AsDocument(document);
    if (!doc) {
        return nullptr;
    }
    LoadProperties(doc);
    if (index < 0 || index >= len(doc->properties)) {
        return nullptr;
    }
    return DupCString(doc->properties[index].val);
}

void MacFreeString(char* value) {
    free(value);
}

void MacFreeDocumentLayout(MacDocumentLayout* layout) {
    if (!layout) {
        return;
    }
    free(layout->pages);
    *layout = {};
}

void MacFreeRenderedPage(MacRenderedPage* page) {
    if (!page) {
        return;
    }
    free(page->data);
    page->data = nullptr;
}

void MacCloseDocument(void* document) {
    if (!document) {
        return;
    }
    MacDocument* doc = AsDocument(document);
    delete doc->renderer;
    delete doc->textSelection;
    delete doc->textSearch;
    delete doc->toc;
    FreeProps(doc->properties);
    delete doc->model;
    delete doc;
}

void MacShutdown() {
    DestroyTempArena();
}

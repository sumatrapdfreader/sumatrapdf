#include "base/Base.h"
#include "base/Pixmap.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "ReaderModel.h"
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

static char* DupCString(const char* s) {
    size_t len = strlen(s);
    char* res = (char*)malloc(len + 1);
    if (res) {
        memcpy(res, s, len + 1);
    }
    return res;
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
void* MacOpenDocument(const char* path, char** errorOut) {
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
    return model;
}

// Number of pages, or 0 if the handle is invalid.
int MacPageCount(void* document) {
    if (!document) {
        return 0;
    }
    return ((ReaderModel*)document)->PageCount();
}

// Mediabox size of pageNo (1-based) in points. Returns false if invalid.
bool MacPageSize(void* document, int pageNo, double* widthOut, double* heightOut) {
    if (!document) {
        return false;
    }
    auto* model = (ReaderModel*)document;
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
    return ((ReaderModel*)document)->FileDPI();
}

bool MacLayoutDocument(void* document, const MacLayoutParams* params, MacDocumentLayout* layout) {
    if (!document || !params || !layout) {
        return false;
    }
    *layout = {};

    auto* model = (ReaderModel*)document;
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
    auto* model = (ReaderModel*)document;
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
    delete (ReaderModel*)document;
}

void MacShutdown() {
    DestroyTempArena();
}

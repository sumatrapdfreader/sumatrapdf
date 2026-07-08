#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"

#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "mac/SumatraMacEngine.h"

void _uploadDebugReport(Str, Str, bool, bool) {
}

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

static EngineBase* CreateEngineForPath(Str path) {
    if (IsEngineImageDirSupportedFile(path)) {
        return CreateEngineImageDirFromFile(path);
    }

    FileType kind = GuessFileTypeFromName(path);
    if (IsEngineDjVuSupportedFileType(kind)) {
        return CreateEngineDjvuDecFromFile(path);
    }
    if (IsEngineImageSupportedFileType(kind)) {
        return CreateEngineImageFromFile(path);
    }
    if (IsEngineCbxSupportedFileType(kind)) {
        return CreateEngineCbxFromFile(path, nullptr, kind);
    }
    if (IsEngineMupdfSupportedFileType(kind)) {
        return CreateEngineMupdfFromFile(path, kind, 96, nullptr);
    }
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
    if (!pixmap || !pixmap->data || pixmap->format != PixmapFormat::BGRA8) {
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

void* MacOpenDocument(const char* path, char** errorOut) {
    if (!path || !path[0]) {
        if (errorOut) {
            *errorOut = DupCString("Pass a document path on the command line.");
        }
        return nullptr;
    }

    EngineBase* engine = CreateEngineForPath(Str((char*)path));
    if (!engine) {
        if (errorOut) {
            *errorOut = DupCString("Could not open the document.");
        }
        return nullptr;
    }
    if (engine->PageCount() < 1) {
        engine->Release();
        if (errorOut) {
            *errorOut = DupCString("Document has no pages.");
        }
        return nullptr;
    }
    return engine;
}

int MacPageCount(void* document) {
    if (!document) {
        return 0;
    }
    return ((EngineBase*)document)->PageCount();
}

bool MacPageSize(void* document, int pageNo, double* widthOut, double* heightOut) {
    if (!document) {
        return false;
    }
    auto* engine = (EngineBase*)document;
    if (pageNo < 1 || pageNo > engine->PageCount()) {
        return false;
    }
    RectF mb = engine->PageMediabox(pageNo);
    if (widthOut) {
        *widthOut = mb.dx;
    }
    if (heightOut) {
        *heightOut = mb.dy;
    }
    return true;
}

bool MacRenderPage(void* document, int pageNo, float zoom, int rotation, MacRenderedPage* page) {
    if (!page) {
        return false;
    }
    *page = {};
    if (!document) {
        return false;
    }
    auto* engine = (EngineBase*)document;
    if (pageNo < 1 || pageNo > engine->PageCount()) {
        return false;
    }
    if (zoom <= 0) {
        zoom = 1.0f;
    }

    RenderPageArgs renderArgs(pageNo, zoom, rotation);
    Pixmap* pixmap = engine->RenderPage(renderArgs);
    bool ok = CopyPixmap(pixmap, page);
    FreePixmap(pixmap);
    return ok;
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
    ((EngineBase*)document)->Release();
}

void MacShutdown() {
    DestroyTempArena();
}

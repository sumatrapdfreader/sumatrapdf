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
        page->error = DupCString("Could not render the first page.");
        return false;
    }

    size_t nBytes = (size_t)pixmap->stride * (size_t)pixmap->height;
    auto* data = (unsigned char*)malloc(nBytes);
    if (!data) {
        page->error = DupCString("Out of memory.");
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

bool MacOpenDocument(const char* path, MacRenderedPage* page) {
    *page = {};
    if (!path || !path[0]) {
        page->error = DupCString("Pass a document path on the command line.");
        return false;
    }

    EngineBase* engine = CreateEngineForPath(Str((char*)path));
    if (!engine) {
        page->error = DupCString("Could not open the document.");
        return false;
    }

    int pageCount = engine->PageCount();
    if (pageCount < 1) {
        engine->Release();
        page->error = DupCString("Document has no pages.");
        return false;
    }

    RenderPageArgs renderArgs(1, 1.0f, 0);
    Pixmap* pixmap = engine->RenderPage(renderArgs);
    bool ok = CopyPixmap(pixmap, page);
    FreePixmap(pixmap);
    if (!ok) {
        engine->Release();
        return false;
    }

    page->document = engine;
    return true;
}

void MacFreeRenderedPage(MacRenderedPage* page) {
    if (!page) {
        return;
    }
    free(page->data);
    free(page->error);
    page->data = nullptr;
    page->error = nullptr;
}

void MacCloseDocument(void* document) {
    if (!document) {
        return;
    }
    auto* engine = (EngineBase*)document;
    engine->Release();
}

void MacShutdown() {
    DestroyTempArena();
}

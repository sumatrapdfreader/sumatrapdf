/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraMacEngine_h
#define SumatraMacEngine_h

// Plain C bridge between the Cocoa app (SumatraMac.mm) and the C++ engine/base
// layer. Cocoa files must not include base/Base.h (Apple headers define names
// like Size that clash with Sumatra types), so all engine access goes through
// this header.

struct MacRenderedPage {
    int width;
    int height;
    int stride;
    bool premultiplied;
    unsigned char* data;
};

struct MacLayoutParams {
    bool continuous;
    int startPage;
    int viewX;
    int viewY;
    int viewWidth;
    int viewHeight;
    double zoomVirtual;
    double backingScale;
    int rotation;
};

struct MacLayoutPage {
    int pageNo;
    int x;
    int y;
    int width;
    int height;
    int screenX;
    int screenY;
    int screenWidth;
    int screenHeight;
    double visibleRatio;
    double renderZoom;
    bool shown;
};

struct MacDocumentLayout {
    int pageCount;
    int currentPage;
    int canvasWidth;
    int canvasHeight;
    MacLayoutPage* pages;
};

// Opens a document. Returns an opaque handle, or nullptr on failure; on failure
// *errorOut (if non-null) is set to a malloc'd message the caller must free().
void* MacOpenDocument(const char* path, char** errorOut);

// Number of pages, or 0 if the handle is invalid.
int MacPageCount(void* document);

// Mediabox size of pageNo (1-based) in points. Returns false if invalid.
bool MacPageSize(void* document, int pageNo, double* widthOut, double* heightOut);

double MacFileDPI(void* document);

// Renders pageNo (1-based) at the given zoom and rotation (0/90/180/270).
// Fills *page (caller frees with MacFreeRenderedPage); returns false on failure.
bool MacRenderPage(void* document, int pageNo, float zoom, int rotation, MacRenderedPage* page);

bool MacLayoutDocument(void* document, const MacLayoutParams* params, MacDocumentLayout* layout);
void MacFreeDocumentLayout(MacDocumentLayout* layout);

void MacFreeRenderedPage(MacRenderedPage* page);
void MacCloseDocument(void* document);
void MacShutdown();

#endif

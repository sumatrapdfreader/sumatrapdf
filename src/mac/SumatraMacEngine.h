/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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

void* MacOpenDocument(const char* path, char** errorOut);

int MacPageCount(void* document);

bool MacPageSize(void* document, int pageNo, double* widthOut, double* heightOut);

double MacFileDPI(void* document);

bool MacRenderPage(void* document, int pageNo, float zoom, int rotation, MacRenderedPage* page);

bool MacLayoutDocument(void* document, const MacLayoutParams* params, MacDocumentLayout* layout);
void MacFreeDocumentLayout(MacDocumentLayout* layout);

void MacFreeRenderedPage(MacRenderedPage* page);
void MacCloseDocument(void* document);
void MacShutdown();

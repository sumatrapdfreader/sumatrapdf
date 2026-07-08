/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraMacEngine_h
#define SumatraMacEngine_h

struct MacRenderedPage {
    int width;
    int height;
    int stride;
    bool premultiplied;
    unsigned char* data;
    char* error;
    void* document;
};

bool MacOpenDocument(const char* path, MacRenderedPage* page);
void MacFreeRenderedPage(MacRenderedPage* page);
void MacCloseDocument(void* document);
void MacShutdown();

#endif

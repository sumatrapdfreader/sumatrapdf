/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Splitter_h
#define Splitter_h

HWND CreateHSplitter(HWND parent);
HWND CreateVSplitter(HWND parent);

struct Splitter;

enum SplitterType {
    SplitterHoriz,
    SplitterVert
};

typedef bool (*onMouseMove)(void *ctx);

Splitter *  CreateSpliter(HWND parent, SplitterType *type, void *ctx, onMouseMove cb);
void        DeleteSplitter(Splitter *);

#endif


/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Splitter_h
#define Splitter_h

HWND CreateHSplitter(HWND parent);

struct Splitter;

enum SplitterType {
    SplitterHoriz,
    SplitterVert
};

// called on WM_MOUSEMOVE. the owner returns false if it doesn't allow
// resizing to current cursor position
typedef bool (*onMove)(void *ctx);
typedef void (*onMoveDone)(void *ctx);

void        RegisterSplitterWndClass();
Splitter *  CreateSplitter(HWND parent, SplitterType type, void *ctx, onMove cbMove, onMoveDone cbMoveDone);
void        DeleteSplitter(Splitter *);
HWND        GetSplitterHwnd(Splitter *);

#endif

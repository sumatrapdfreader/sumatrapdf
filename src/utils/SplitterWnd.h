/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SplitterWnd_h
#define SplitterWnd_h

struct SplitterWnd;

enum SplitterType {
    SplitterHoriz,
    SplitterVert
};

// called when user drags the splitter ('done' is false) and when drag is finished ('done' is
// true). the owner can constrain splitter by using current cursor
// position and returning false if it's not allowed to go there
typedef bool (*SplitterCallback)(void *ctx, bool done);

void           RegisterSplitterWndClass();
SplitterWnd *  CreateSplitter(HWND parent, SplitterType type, void *ctx, SplitterCallback cb);
HWND           GetHwnd(SplitterWnd *);
void           SetBgCol(SplitterWnd *, COLORREF);

#endif

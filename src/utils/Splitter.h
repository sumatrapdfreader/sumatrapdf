/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Splitter_h
#define Splitter_h

struct Splitter;

enum SplitterType {
    SplitterHoriz,
    SplitterVert
};

// called when user drags the splitter ('done' is false) and when drag is finished ('done' is
// true). the owner can constrain splitter by using current cursor
// position and returning false if it's not allowed to go there
typedef bool (*SplitterCallback)(void *ctx, bool done);

void        RegisterSplitterWndClass();
Splitter *  CreateSplitter(HWND parent, SplitterType type, void *ctx, SplitterCallback cb);
HWND        GetHwnd(Splitter *);
void        SetBgCol(Splitter *, COLORREF);

#endif

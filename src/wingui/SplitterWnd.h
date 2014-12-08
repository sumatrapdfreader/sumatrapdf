/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct SplitterWnd;

enum SplitterType {
    SplitterHoriz,
    SplitterVert,
};

// called when user drags the splitter ('done' is false) and when drag is finished ('done' is
// true). the owner can constrain splitter by using current cursor
// position and returning false if it's not allowed to go there
typedef bool (*SplitterCallback)(void *ctx, bool done);

void           RegisterSplitterWndClass();
SplitterWnd *  CreateSplitter(HWND parent, SplitterType type, void *ctx, SplitterCallback cb);
HWND           GetHwnd(SplitterWnd *);
void           SetBgCol(SplitterWnd *, COLORREF);

// call at the end of program
void           DeleteSplitterBrush();

// If a splitter is "live", the owner will re-layout windows in the callback,
// otherwise we'll draw an indicator of where the splitter will be
// at the end of resize and the owner only re-layouts when callback is called
// with 'done' set to true
void           SetSplitterLive(SplitterWnd *, bool live);

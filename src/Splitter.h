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

// called when user drags the splitter and when drag is finished (when done is
// true). the owner can constrain where splitter can go by using current cursor
// position and returning false if it's not allowed to go there
typedef bool (*SplitterCallback)(void *ctx, bool done);

void        RegisterSplitterWndClass();
Splitter *  CreateSplitter(HWND parent, SplitterType type, void *ctx, SplitterCallback cb);
HWND        GetSplitterHwnd(Splitter *);

#endif

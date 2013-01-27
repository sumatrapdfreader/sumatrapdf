/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiPainter_h
#error "dont include twice!"
#endif
#define MuiPainter_h

// Manages painting process of Control window and all its children.
// Automatically does double-buffering for less flicker.
// Create one object for each HWND-backed Control and keep it around.
class Painter
{
    HwndWrapper *wnd;
    // bitmap for double-buffering
    Bitmap *    cacheBmp;
    Size        sizeDuringLastPaint;

    void PaintBackground(Graphics *g, Rect r);

public:
    Painter(HwndWrapper *wnd);
    ~Painter();

    void Paint(HWND hwnd, bool isDirty);
};


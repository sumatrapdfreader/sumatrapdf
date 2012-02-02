/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiPainter_h
#define MuiPainter_h

// This is only meant to be included by Mui.h within mui namespace

class HwndWrapper;

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

    void Paint(HWND hwnd);
};


#endif

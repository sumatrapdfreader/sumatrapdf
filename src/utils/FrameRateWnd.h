/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef FrameRateWnd_h
#define FrameRateWnd_h

struct FrameRateWnd {
    HWND hwndAssociatedWith;

    HWND hwnd;
    SIZE maxSizeSoFar;
    int  frameRate;
};

FrameRateWnd *  AllocFrameRateWnd(HWND hwndAssociatedWith);
bool            CreateFrameRateWnd(FrameRateWnd *);
void            DeleteFrameRateWnd(FrameRateWnd *);
void            ShowFrameRate(FrameRateWnd *, int frameRate);
void            ShowFrameRateDur(FrameRateWnd *, double durMs);

int             FrameRateFromDuration(double durMs);

#endif


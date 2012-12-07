/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiBase_h
#error "dont include twice!"
#endif
#define MuiBase_h

void InitializeBase();
void DestroyBase();

void EnterMuiCriticalSection();
void LeaveMuiCriticalSection();

class ScopedMuiCritSec {
public:

    ScopedMuiCritSec() {
        EnterMuiCriticalSection();
    }

    ~ScopedMuiCritSec() {
        LeaveMuiCriticalSection();
    }
};

void        InitGraphicsMode(Graphics *g);
Font *      GetCachedFont(const WCHAR *name, float size, FontStyle style);
Graphics *  AllocGraphicsForMeasureText();
void        FreeGraphicsForMeasureText(Graphics *gfx);

int         CeilI(float n);


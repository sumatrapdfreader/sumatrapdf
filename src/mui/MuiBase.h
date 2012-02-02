/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiBase_h
#define MuiBase_h

// This is only meant to be included by Mui.h within mui namespace

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

Font *GetCachedFont(const WCHAR *name, float size, FontStyle style);

#endif

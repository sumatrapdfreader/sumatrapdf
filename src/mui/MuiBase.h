/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiBase_h
#define MuiBase_h

namespace mui {

using namespace Gdiplus;

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

}

#endif

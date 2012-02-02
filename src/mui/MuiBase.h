/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiBase_h
#define MuiBase_h

namespace mui {

void EnterMuiCriticalSection();
void LeaveMuiCriticalSection();
void InitMuiCriticalSection();
void DeleteMuiCriticalSection();

class ScopedMuiCritSec {
public:

    ScopedMuiCritSec() {
        EnterMuiCriticalSection();
    }

    ~ScopedMuiCritSec() {
        LeaveMuiCriticalSection();
    }
};

}

#endif

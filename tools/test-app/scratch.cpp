#include "stdafx.h"

#include "render-speed-test.h"

struct Buf {
    char *s;
    u64 sLen;
    bool owned;

    Buf() {
        Reset();
    }

    void Reset() {
        s = nullptr;
        sLen = 0;
        owned = false;
    }

    explicit Buf(char *sIn, u64 sLenIn = (u64) -1, bool ownedIn = false) {
        Set(sIn, sLenIn, ownedIn);
    }
    void Free() {
        if (owned)
            free(s);
        Reset();
    }

    void Set(char *sIn, u64 sLenIn = (u64) -1, bool ownedIn = false) {
        Free();
        s = sIn;
        if (sLenIn == (u64) -1) {
            sLen = (u64) strlen(s);
        }
        else {
            sLen = sLenIn;
        }
        owned = ownedIn;
    }

    void TakeOwnership(char *sIn, u64 sLenIn = (u64) -1) {
        char *tmp = _strdup(sIn);
        Set(tmp, sLenIn, true);
    }

    ~Buf() {
        if (owned) {
            free(s);
        }
    }
};

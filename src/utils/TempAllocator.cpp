/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

/*
Temp allocator is meant for allocating temporary values that don't need
to outlive this stack frame.
It's an alternative to using various AutoFree* classes.

It's a very fast bump allocator.

You must periodically call ResetTempArena()
to free memory used by allocator.
A good place to do it is at the beginning of window message loop.
*/

// GetTempArena()/ResetTempArena()/DestroyTempArena() live in common/arena.cpp

// Arena for allocations that live for the whole lifetime of the program (i.e.
// never freed until exit). Allocating them here avoids per-allocation frees and
// lets us track how much such memory we use (logged on exit). Never Reset().
Arena* gLifetimeArena = nullptr;

Arena* GetLifetimeArena() {
    if (!gLifetimeArena) {
        gLifetimeArena = ArenaNew();
    }
    return gLifetimeArena;
}

void DestroyLifetimeArena() {
    ArenaDelete(gLifetimeArena);
    gLifetimeArena = nullptr;
}

static Str WrapTempStr(char* s, size_t cb = (size_t)-1) {
    if (!s) {
        return {};
    }
    if (cb == (size_t)-1) {
        return Str(s);
    }
    return Str(s, (int)cb);
}

static WStr WrapTempWStr(WCHAR* s, size_t cch = (size_t)-1) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        return WStr(s);
    }
    return WStr(s, (int)cch);
}

namespace str {
TempStr DupTemp(Str s, size_t cb) {
    return Dup(GetTempArena(), s, cb);
}

TempWStr DupTemp(const WCHAR* s, size_t cch) {
    return WrapTempWStr(Dup(GetTempArena(), s, cch), cch);
}

TempStr JoinTemp(const char* s1, const char* s2, const char* s3) {
    return Join(GetTempArena(), Str(s1), Str(s2), Str(s3));
}

TempStr JoinTemp(const char* s1, const char* s2, const char* s3, const char* s4) {
    return Join(GetTempArena(), Str(s1), Str(s2), Str(s3), Str(s4));
}

TempStr JoinTemp(const char* s1, const char* s2, const char* s3, const char* s4, const char* s5) {
    return Join(GetTempArena(), Str(s1), Str(s2), Str(s3), Str(s4), Str(s5));
}

TempWStr JoinTemp(const WCHAR* s1, const WCHAR* s2, const WCHAR* s3) {
    return WrapTempWStr(Join(GetTempArena(), s1, s2, s3));
}

TempStr JoinTemp(Str s1, const char* s2, const char* s3) {
    return Join(GetTempArena(), s1, Str(s2), Str(s3));
}

TempStr JoinTemp(const char* s1, Str s2, const char* s3) {
    return Join(GetTempArena(), Str(s1), s2, Str(s3));
}

TempStr JoinTemp(Str s1, Str s2, const char* s3) {
    return Join(GetTempArena(), s1, s2, Str(s3));
}

TempWStr JoinTemp(WStr s1, const WCHAR* s2, const WCHAR* s3) {
    return JoinTemp(s1.s, s2, s3);
}

TempWStr JoinTemp(const WCHAR* s1, WStr s2, const WCHAR* s3) {
    return JoinTemp(s1, s2.s, s3);
}

TempWStr JoinTemp(WStr s1, WStr s2, const WCHAR* s3) {
    return JoinTemp(s1.s, s2.s, s3);
}

TempStr FormatTemp(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Str res = FmtVWithArena(GetTempArena(), fmt, args);
    va_end(args);
    return res;
}

TempStr ReplaceTemp(const char* s, const char* toReplace, const char* replaceWith) {
    if (!s || str::IsEmpty(toReplace) || !replaceWith) {
        return {};
    }

    const char* curr = s;
    Str end = str::Find(Str(curr), toReplace);
    if (!end) {
        // optimization: nothing to replace so do nothing
        return Str(s);
    }

    size_t findLen = str::Len(toReplace);
    size_t replLen = str::Len(replaceWith);
    size_t lenDiff = 0;
    if (replLen > findLen) {
        lenDiff = replLen - findLen;
    }
    // heuristic: allow 6 replacements without reallocating
    size_t capHint = str::Len(s) + 1 + (lenDiff * 6);
    StrBuilder result(capHint);
    bool ok;
    while (end) {
        ok = result.Append(curr, end.s - curr);
        if (!ok) {
            return {};
        }
        ok = result.Append(replaceWith, replLen);
        if (!ok) {
            return {};
        }
        curr = end.s + findLen;
        end = str::Find(Str(curr), toReplace);
    }
    ok = result.Append(curr);
    if (!ok) {
        return {};
    }
    return WrapTempStr(result.StealData(GetTempArena()));
}

TempStr ReplaceNoCaseTemp(const char* s, const char* toReplace, const char* replaceWith) {
    int n = str::Leni(toReplace);
    Str pos = str::FindI(Str(s), toReplace);
    if (!pos) {
        return Str(s);
    }
    if (!memeq(pos.s, toReplace, n)) {
        toReplace = str::DupTemp(pos, n).s;
    }
    TempStr res = str::ReplaceTemp(s, toReplace, replaceWith);
    return res;
}

} // namespace str

// handles embedded 0 in the string
TempWStr ToWStrTemp(const StrBuilder& str) {
    if (str.IsEmpty()) {
        return {};
    }
    return ToWStrTemp(Str(str.CStr(), str.Size()));
}
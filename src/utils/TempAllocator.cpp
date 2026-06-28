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

static Str WrapTempStr(char* s, size_t cb = (size_t)-1) { // str-port: owned heap
    if (!s) {
        return {};
    }
    if (cb == (size_t)-1) {
        return Str(s);
    }
    return Str(s, (int)cb);
}

static WStr WrapTempWStr(WCHAR* s, size_t cch = (size_t)-1) { // str-port: owned heap
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

TempWStr DupTemp(WStr s, size_t cch) {
    return WrapTempWStr(Dup(GetTempArena(), s.s, cch), cch);
}

TempStr JoinTemp(Str s1, Str s2, Str s3) {
    return Join(GetTempArena(), s1, s2, s3);
}

TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4) {
    return Join(GetTempArena(), s1, s2, s3, s4, Str{});
}

TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4, Str s5) {
    return Join(GetTempArena(), s1, s2, s3, s4, s5);
}

TempWStr JoinTemp(WStr s1, WStr s2, WStr s3) {
    return WrapTempWStr(Join(GetTempArena(), s1.s, s2.s, s3.s));
}

TempStr FormatTemp(Str fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Str res = FmtVWithArena(GetTempArena(), fmt, args);
    va_end(args);
    return res;
}

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith) {
    if (str::IsNull(s) || str::IsEmpty(toReplace) || str::IsNull(replaceWith)) {
        return {};
    }

    Str curr = s;
    Str end = str::Find(curr, toReplace);
    if (str::IsNull(end)) {
        // optimization: nothing to replace so do nothing
        return s;
    }

    size_t findLen = (size_t)toReplace.len;
    size_t replLen = (size_t)replaceWith.len;
    size_t lenDiff = 0;
    if (replLen > findLen) {
        lenDiff = replLen - findLen;
    }
    // heuristic: allow 6 replacements without reallocating
    size_t capHint = (size_t)s.len + 1 + (lenDiff * 6);
    StrBuilder result(capHint);
    bool ok;
    while (!str::IsNull(end)) {
        ok = result.Append(Str(curr.s, (int)(end.s - curr.s)));
        if (!ok) {
            return {};
        }
        ok = result.Append(Str(replaceWith.s, (int)replLen));
        if (!ok) {
            return {};
        }
        curr = Str(end.s + findLen, s.len - (int)(end.s + findLen - s.s));
        end = str::Find(curr, toReplace);
    }
    ok = result.Append(curr);
    if (!ok) {
        return {};
    }
    return WrapTempStr(result.StealData(GetTempArena()));
}

TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith) {
    int n = toReplace.len;
    Str pos = str::FindI(s, toReplace);
    if (!pos) {
        return s;
    }
    if (!memeq(pos.s, toReplace.s, n)) {
        toReplace = str::DupTemp(pos, n);
    }
    TempStr res = str::ReplaceTemp(s, toReplace, replaceWith);
    return res;
}

} // namespace str

// handles embedded 0 in the string
TempWStr ToWStrTempFromBuilder(const StrBuilder& str) {
    if (str.IsEmpty()) {
        return {};
    }
    return ToWStrTemp(str.CStr());
}

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dict.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

#define LETTERS "abcdefghijklmnopqrtswzABCDEFGHIJLMNOPQRTSWZ0123456789"

static inline char GenRandChar() {
    return LETTERS[rand() % dimof(LETTERS)];
}

static Str GenRandomString() {
    static char buf[256];
    int n = 1 + (rand() % (dimof(buf) - 4)); // 4 just in case, 2 should be precise value

    for (int i = 0; i < n; i++) {
        buf[i] = GenRandChar();
    }
    buf[n] = 0;
    return Str(buf, n);
}

void DictTestMapStrToInt() {
    dict::MapStrToInt d(4); // start small so that we can test resizing
    bool ok;
    int val;

    utassert(0 == d.Count());
    ok = d.Get(StrL("foo"), &val);
    utassert(!ok);
    ok = d.Remove(StrL("foo"), nullptr);
    utassert(!ok);

    ok = d.Insert(StrL("foo"), 5, nullptr);
    utassert(ok);
    utassert(1 == d.Count());
    ok = d.Get(StrL("foo"), &val);
    utassert(ok);
    utassert(val == 5);
    ok = d.Insert(StrL("foo"), 8, &val);
    utassert(!ok);
    utassert(val == 5);
    ok = d.Get(StrL("foo"), &val);
    utassert(ok);
    utassert(val == 5);
    ok = d.Get(StrL("bar"), &val);
    utassert(!ok);

    val = 0;
    ok = d.Remove(StrL("foo"), &val);
    utassert(ok);
    utassert(val == 5);
    utassert(0 == d.Count());

    srand((unsigned int)time(nullptr));
    StrVec toRemove;
    for (int i = 0; i < 1024; i++) {
        Str k = GenRandomString();
        ok = d.Insert(k, i, nullptr);
        // no guarantee that the string is unique, so Insert() doesn't always succeeds
        if (!ok) continue;
        toRemove.Append(str::Dup(k));
        utassert(len(toRemove) == d.Count());
        ok = d.Get(k, &val);
        ReportIf(!ok);
        ReportIf(i != val);
    }
    for (int i = 0; i < len(toRemove); i++) {
        Str k = toRemove[i];
        ok = d.Remove(k, nullptr);
        utassert(ok);
    }
}

void DictTest() {
    DictTestMapStrToInt();
}
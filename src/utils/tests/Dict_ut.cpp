/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dict.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

#define LETTERS "abcdefghijklmnopqrtswzABCDEFGHIJLMNOPQRTSWZ0123456789"

static inline char GenRandChar() {
    return LETTERS[rand() % dimof(LETTERS)];
}

static char* GenRandomString() {
    static char buf[256];
    int len = 1 + (rand() % (dimof(buf) - 4)); // 4 just in case, 2 should be precise value

    for (int i = 0; i < len; i++) {
        buf[i] = GenRandChar();
    }
    buf[len] = 0;
    return &buf[0];
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
    Vec<char*> toRemove;
    for (int i = 0; i < 1024; i++) {
        char* k = GenRandomString();
        ok = d.Insert(Str(k), i, nullptr);
        // no guarantee that the string is unique, so Insert() doesn't always succeeds
        if (!ok) continue;
        toRemove.Append(str::Dup(k));
        utassert(toRemove.size() == d.Count());
        ok = d.Get(Str(k), &val);
        ReportIf(!ok);
        ReportIf(i != val);
    }
    for (const char* k : toRemove) {
        ok = d.Remove(Str(k), nullptr);
        utassert(ok);
    }
    toRemove.FreeMembers();
}

void DictTest() {
    DictTestMapStrToInt();
}

/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Dict.h"

// must be last due to assert() over-write
#include "UtAssert.h"

#define LETTERS "abcdefghijklmnopqrtswzABCDEFGHIJLMNOPQRTSWZ0123456789"

static inline char GenRandChar()
{
    return LETTERS[rand() % dimof(LETTERS)];
}

static char *GenRandomString()
{
    static char buf[256];
    int len = 1 + (rand() % (dimof(buf) - 4)); // 4 just in case, 2 should be precise value

    for (int i=0; i<len; i++) {
        buf[i] = GenRandChar();
    }
    buf[len] = 0;
    return &buf[0];
}

void DictTestMapStrToInt()
{
    dict::MapStrToInt d(4); // start small so that we can test resizing
    bool ok;
    int val;

    utassert(0 == d.Count());
    ok = d.Get("foo", &val);
    utassert(!ok);
    ok = d.Remove("foo", NULL);
    utassert(!ok);

    ok = d.Insert("foo", 5, NULL);
    utassert(ok);
    utassert(1 == d.Count());
    ok = d.Get("foo", &val);
    utassert(ok);
    utassert(val == 5);
    ok = d.Insert("foo", 8, &val);
    utassert(!ok);
    utassert(val == 5);
    ok = d.Get("foo", &val);
    utassert(ok);
    utassert(val == 5);
    ok = d.Get("bar", &val);
    utassert(!ok);

    val = 0;
    ok = d.Remove("foo", &val);
    utassert(ok);
    utassert(val == 5);
    utassert(0 == d.Count());

    srand((unsigned int)time(NULL));
    Vec<char *> toRemove;
    for (int i=0; i < 1024; i++) {
        char *k = GenRandomString();
        ok = d.Insert(k, i, NULL);
        // no guarantee that the string is unique, so Insert() doesn't always succeeds
        if (!ok)
            continue;
        toRemove.Append(str::Dup(k));
        utassert(toRemove.Count() == d.Count());
        ok = d.Get(k, &val);
        CrashIf(!ok);
        CrashIf(i != val);
    }
    for (const char *k : toRemove) {
        ok = d.Remove(k, NULL);
        utassert(ok);
    }
    FreeVecMembers(toRemove);
}

void DictTest()
{
    DictTestMapStrToInt();
}

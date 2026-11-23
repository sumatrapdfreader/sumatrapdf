/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dict.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

#define kLetters "abcdefghijklmnopqrtswzABCDEFGHIJLMNOPQRTSWZ0123456789"

static inline char GenRandChar() {
    return kLetters[rand() % dimof(kLetters)];
}

static Str GenRandomString() {
    static char buf[256];
    int n = 1 + (rand() % (int)(dimof(buf) - 4)); // 4 just in case, 2 should be precise value

    for (int i = 0; i < n; i++) {
        buf[i] = GenRandChar();
    }
    buf[n] = 0;
    return Str(buf, n);
}

void DictTestStrToInt() {
    dict::StrToInt d(4); // start small so that we can test resizing
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

// removing from the middle of probe runs must keep every other key findable
static void DictTestRemoveKeepsOthers() {
    dict::StrToInt d(4);
    constexpr int kN = 3000;
    for (int i = 0; i < kN; i++) {
        utassert(d.Insert(fmt("key-%d", i), i));
    }
    utassert(d.Count() == kN);
    for (int i = 0; i < kN; i += 2) {
        int v = -1;
        utassert(d.Remove(fmt("key-%d", i), &v) && v == i);
    }
    utassert(d.Count() == kN / 2);
    for (int i = 0; i < kN; i++) {
        int v = -1;
        bool found = d.Get(fmt("key-%d", i), &v);
        utassert(found == (i % 2 == 1));
        utassert(!found || v == i);
    }
    // removed keys can come back, with new values
    for (int i = 0; i < kN; i += 2) {
        utassert(d.Insert(fmt("key-%d", i), i + kN));
    }
    int v = -1;
    utassert(d.Get(StrL("key-0"), &v) && v == kN);
    utassert(d.Count() == kN);
}

static TempStr RepeatedTemp(char c, int n) {
    char* d = AllocArrayTemp<char>(n + 1);
    memset(d, c, (size_t)n);
    return Str(d, n);
}

static void DictTestKeys() {
    dict::StrToInt d;
    int v = -1;
    // empty keys are refused
    utassert(!d.Insert(Str(), 1));
    utassert(!d.Get(Str(), &v));
    // a key needn't be NUL-terminated: only its length counts
    Str full = StrL("hello world");
    utassert(d.Insert(Str(full.s, 5), 7));
    utassert(d.Get(StrL("hello"), &v) && v == 7);
    utassert(!d.Get(full, &v));
    utassert(!d.Get(StrL("hell"), &v));
    // keys long enough for a multi-byte length
    TempStr longKey = RepeatedTemp('x', 300);
    utassert(d.Insert(longKey, 300));
    utassert(d.Get(longKey, &v) && v == 300);
    utassert(d.Insert(RepeatedTemp('x', 299), 299));
    utassert(d.Get(longKey, &v) && v == 300);
    int existing = -1;
    utassert(!d.Insert(longKey, 1, &existing) && existing == 300);
}

void DictTest() {
    DictTestStrToInt();
    DictTestRemoveKeepsOthers();
    DictTestKeys();
}
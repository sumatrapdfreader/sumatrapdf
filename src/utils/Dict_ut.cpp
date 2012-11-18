#include "BaseUtil.h"
#include "Dict.h"

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

void DictTest()
{
    dict::MapStrToInt d(4); // start small so that we can test resizing
    bool ok;
    int val;

    ok = d.GetValue("foo", &val);
    assert(!ok);

    ok = d.Insert("foo", 5, NULL);
    assert(ok);
    ok = d.GetValue("foo", &val);
    assert(val == 5);
    ok = d.Insert("foo", 8, &val);
    assert(!ok);
    assert(val == 5);
    ok = d.GetValue("foo", &val);
    assert(val == 5);
    ok = d.GetValue("bar", &val);
    assert(!ok);

    srand((unsigned int)time(NULL));
    for (int i=0; i < 1024; i++) {
        char *k = GenRandomString();
        // no guarantee that the string is unique, so Insert() doesn't always succeeds
        ok = d.Insert(k, i, NULL);
        if (!ok)
            continue;
        ok = d.GetValue(k, &val);
        CrashIf(!ok);
        CrashIf(i != val);
    }
}

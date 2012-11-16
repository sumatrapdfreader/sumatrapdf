#include "BaseUtil.h"
#include "Dict.h"

void DictTest()
{
    dict::MapStrToInt d(128);
    bool ok;
    int val;

    ok = d.GetValue("foo", &val);
    assert(!ok);

    ok = d.Insert("foo", 5);
    assert(ok);
    ok = d.GetValue("foo", &val);
    assert(val == 5);
    ok = d.Insert("foo", 8);
    assert(!ok);
    ok = d.GetValue("foo", &val);
    assert(val == 5);
    ok = d.GetValue("bar", &val);
    assert(!ok);

    // TODO: a stress test that adds a lot of random strings
}

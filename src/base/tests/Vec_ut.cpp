/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

// must be last due to assert() over-write
#include "base/UtAssert.h"

static size_t VecTestAppendFmt() {
    str::Builder v(256);
    i64 val = 1;
    for (int i = 0; i < 10000; i++) {
        v.Append(fmt("i%" PRId64 "e", val));
        val = (val * 3) / 2; // somewhat exponential growth
        val += 15;
    }
    int l = len(v);
    return l;
}

void VecTest() {
    Vec<int> ints;
    utassert(len(ints) == 0);
    ints.Append(1);
    ints.Append(2);
    ints.InsertAt(0, -1);
    utassert(len(ints) == 3);
    utassert(ints[0] == -1 && ints[1] == 1 && ints[2] == 2);
    utassert(ints[0] == -1 && ints.Last() == 2);
    int last = ints.Pop();
    utassert(last == 2);
    utassert(len(ints) == 2);
    ints.Append(3);
    ints.RemoveAt(0);
    utassert(len(ints) == 2);
    utassert(ints[0] == 1 && ints[1] == 3);
    ints.Reset();
    utassert(len(ints) == 0);

    for (int i = 0; i < 1000; i++) {
        ints.Append(i);
    }
    utassert(len(ints) == 1000 && ints[500] == 500);
    ints.Remove(500);
    utassert(len(ints) == 999 && ints[500] == 501);
    last = ints.Pop();
    utassert(last == 999);
    ints.Append(last);

    for (int& value : ints) {
        utassert(0 <= value && value < 1000);
    }

    {
        Vec<int> ints2(ints);
        utassert(len(ints2) == 999);
        utassert(ints.LendData() != ints2.LendData());
        ints.Remove(600);
        utassert(len(ints) < len(ints2));
        ints2 = ints;
        utassert(len(ints2) == 998);
    }

    {
        char buf[2] = {'a', '\0'};
        str::Builder v(0, nullptr);
        for (int i = 0; i < 7; i++) {
            v.Append(Str(buf, 1));
            buf[0] = buf[0] + 1;
        }
        Str s = ToStr(v);
        utassert(str::Eq("abcdefg", s));
        utassert(7 == len(v));
        v.Reset(StrL("helo"));
        utassert(4 == len(v));
        utassert(str::Eq("helo", ToStr(v)));
    }

    {
        str::Builder v(128);
        v.Append("boo");
        utassert(str::Eq("boo", ToStr(v)));
        utassert(len(v) == 3);
        v.Append("fop");
        utassert(str::Eq("boofop", ToStr(v)));
        utassert(len(v) == 6);
        v.RemoveAt(2, 3);
        utassert(len(v) == 3);
        utassert(str::Eq("bop", ToStr(v)));
        v.AppendChar('a');
        utassert(len(v) == 4);
        utassert(str::Eq("bopa", ToStr(v)));
        Str s = v.TakeStr();
        utassert(str::Eq("bopa", s));
        str::Free(s);
        utassert(len(v) == 0);
    }

    {
        str::Builder v(0, nullptr);
        for (size_t i = 0; i < 32; i++) {
            utassert(len(v) == i * 6);
            v.Append("lambd");
            if (i % 2 == 0)
                v.AppendChar('a');
            else
                v.AppendChar('a');
        }

        for (int i = 1; i <= 16; i++) {
            v.RemoveAt((16 - i) * 6, 6);
            utassert(len(v) == (32 - i) * 6);
        }

        v.RemoveAt(0, 6 * 15);
        utassert(len(v) == 6);
        Str s = ToStr(v);
        utassert(str::Eq(s, "lambda"));
        s = v.TakeStr();
        utassert(str::Eq(s, "lambda"));
        str::Free(s);
        utassert(len(v) == 0);

        v.Append("lambda");
        utassert(str::Eq(ToStr(v), "lambda"));
        char c = v.RemoveLast();
        utassert(c == 'a');
        utassert(str::Eq(ToStr(v), "lambd"));
    }

    VecTestAppendFmt();

    {
        Vec<Point*> v;
        srand((unsigned int)time(nullptr));
        for (int i = 0; i < 128; i++) {
            v.Append(new Point(i, i));
            int pos = rand() % len(v);
            v.InsertAt(pos, new Point(i, i));
        }
        utassert(len(v) == 128 * 2);

        while (len(v) > 64) {
            size_t pos = rand() % len(v);
            Point* f = v[(int)pos];
            v.Remove(f);
            delete f;
        }
        DeleteVecMembers(v);
    }

    {
        Vec<int> v;
        v.Append(2);
        for (int i = 0; i < 500; i++) v.Append(4);
        v[250] = 5;
        v.Reverse();
        utassert(len(v) == 501 && v[0] == 4 && v[249] == v[251] && v[250] == 5 && v[500] == 2);
        v.Remove(4);
        v.Reverse();
        utassert(len(v) == 500 && v[0] == 2 && v[249] == v[251] && v[250] == 5 && v[499] == 4);
    }

    {
        Vec<int> v;
        v.InsertAt(2, 2);
        auto size = len(v);
        utassert(size == 3);
        auto el0 = v[0];
        utassert(el0 == 0);
        auto el2 = v[2];
        utassert(el2 == 2);
    }

    {
        str::Builder v;
        v.Append("foo");
        utassert(len(v) == 3);
    }
}

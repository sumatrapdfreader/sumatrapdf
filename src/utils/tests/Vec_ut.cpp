/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <utils/VecSegmented.h>

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static size_t VecTestAppendFmt() {
    str::Str v(256);
    i64 val = 1;
    for (int i = 0; i < 10000; i++) {
        v.AppendFmt("i%" PRId64 "e", val);
        val = (val * 3) / 2; // somewhat exponential growth
        val += 15;
    }
    size_t l = v.size();
    return l;
}

struct Num {
    int n;
    int n2;
};

static void VecSegmentedTest() {
    VecSegmented<Num> vec;
    int nTotal = 1033;
    for (int i = 0; i < nTotal; i++) {
        vec.Append(Num{i, i + 1});
    }
    utassert(vec.Size() == (size_t)nTotal);
    int i = 0;
    for (Num* n : vec) {
        utassert(n->n == i);
        utassert(n->n2 == i + 1);
        ++i;
    }
}

void VecTest() {
    VecSegmentedTest();

    Vec<int> ints;
    utassert(ints.size() == 0);
    ints.Append(1);
    ints.Append(2);
    ints.InsertAt(0, -1);
    utassert(ints.size() == 3);
    utassert(ints.at(0) == -1 && ints.at(1) == 1 && ints.at(2) == 2);
    utassert(ints.at(0) == -1 && ints.Last() == 2);
    int last = ints.Pop();
    utassert(last == 2);
    utassert(ints.size() == 2);
    ints.Append(3);
    ints.RemoveAt(0);
    utassert(ints.size() == 2);
    utassert(ints.at(0) == 1 && ints.at(1) == 3);
    ints.Reset();
    utassert(ints.size() == 0);

    for (int i = 0; i < 1000; i++) {
        ints.Append(i);
    }
    utassert(ints.size() == 1000 && ints.at(500) == 500);
    ints.Remove(500);
    utassert(ints.size() == 999 && ints.at(500) == 501);
    last = ints.Pop();
    utassert(last == 999);
    ints.Append(last);

    for (int& value : ints) {
        utassert(0 <= value && value < 1000);
    }

    {
        Vec<int> ints2(ints);
        utassert(ints2.size() == 999);
        utassert(ints.LendData() != ints2.LendData());
        ints.Remove(600);
        utassert(ints.size() < ints2.size());
        ints2 = ints;
        utassert(ints2.size() == 998);
    }

    {
        char buf[2] = {'a', '\0'};
        str::Str v(0, nullptr);
        for (int i = 0; i < 7; i++) {
            v.Append(buf, 1);
            buf[0] = buf[0] + 1;
        }
        char* s = v.LendData();
        utassert(str::Eq("abcdefg", s));
        utassert(7 == v.size());
        v.Set("helo");
        utassert(4 == v.size());
        utassert(str::Eq("helo", v.LendData()));
    }

    {
        str::Str v(128);
        v.Append("boo", 3);
        utassert(str::Eq("boo", v.LendData()));
        utassert(v.size() == 3);
        v.Append("fop");
        utassert(str::Eq("boofop", v.LendData()));
        utassert(v.size() == 6);
        v.RemoveAt(2, 3);
        utassert(v.size() == 3);
        utassert(str::Eq("bop", v.LendData()));
        v.AppendChar('a');
        utassert(v.size() == 4);
        utassert(str::Eq("bopa", v.LendData()));
        char* s = v.StealData();
        utassert(str::Eq("bopa", s));
        free(s);
        utassert(v.size() == 0);
    }

    {
        str::Str v(0, nullptr);
        for (size_t i = 0; i < 32; i++) {
            utassert(v.size() == i * 6);
            v.Append("lambd", 5);
            if (i % 2 == 0)
                v.AppendChar('a');
            else
                v.AppendChar('a');
        }

        for (size_t i = 1; i <= 16; i++) {
            v.RemoveAt((16 - i) * 6, 6);
            utassert(v.size() == (32 - i) * 6);
        }

        v.RemoveAt(0, 6 * 15);
        utassert(v.size() == 6);
        char* s = v.LendData();
        utassert(str::Eq(s, "lambda"));
        s = v.StealData();
        utassert(str::Eq(s, "lambda"));
        free(s);
        utassert(v.size() == 0);

        v.Append("lambda");
        utassert(str::Eq(v.LendData(), "lambda"));
        char c = v.RemoveLast();
        utassert(c == 'a');
        utassert(str::Eq(v.LendData(), "lambd"));
    }

    VecTestAppendFmt();

    {
        Vec<Point*> v;
        srand((unsigned int)time(nullptr));
        for (int i = 0; i < 128; i++) {
            v.Append(new Point(i, i));
            size_t pos = rand() % v.size();
            v.InsertAt(pos, new Point(i, i));
        }
        utassert(v.size() == 128 * 2);

        while (v.size() > 64) {
            size_t pos = rand() % v.size();
            Point* f = v.at(pos);
            v.Remove(f);
            delete f;
        }
        DeleteVecMembers(v);
    }

    {
        Vec<int> v;
        v.Append(2);
        for (int i = 0; i < 500; i++)
            v.Append(4);
        v.at(250) = 5;
        v.Reverse();
        utassert(v.size() == 501 && v.at(0) == 4 && v.at(249) == v.at(251) && v.at(250) == 5 && v.at(500) == 2);
        v.Remove(4);
        v.Reverse();
        utassert(v.size() == 500 && v.at(0) == 2 && v.at(249) == v.at(251) && v.at(250) == 5 && v.at(499) == 4);
    }

    {
        Vec<int> v;
        v.InsertAt(2, 2);
        auto size = v.size();
        utassert(size == 3);
        auto el0 = v.at(0);
        utassert(el0 == 0);
        auto el2 = v.at(2);
        utassert(el2 == 2);
    }

    {
        str::Str v;
        v.Append("foo");
        utassert(v.size() == 3);
    }
}

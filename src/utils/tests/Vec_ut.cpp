/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

// must be last due to assert() over-write
#include "UtAssert.h"

static void WStrVecTest()
{
    WStrVec v;
    v.Append(str::Dup(L"foo"));
    v.Append(str::Dup(L"bar"));
    WCHAR *s = v.Join();
    utassert(v.Count() == 2);
    utassert(str::Eq(L"foobar", s));
    free(s);

    s = v.Join(L";");
    utassert(v.Count() == 2);
    utassert(str::Eq(L"foo;bar", s));
    free(s);

    v.Append(str::Dup(L"glee"));
    s = v.Join(L"_ _");
    utassert(v.Count() == 3);
    utassert(str::Eq(L"foo_ _bar_ _glee", s));
    free(s);

    v.Sort();
    s = v.Join();
    utassert(str::Eq(L"barfooglee", s));
    free(s);

    {
        WStrVec v2(v);
        utassert(str::Eq(v2.At(1), L"foo"));
        v2.Append(str::Dup(L"nobar"));
        utassert(str::Eq(v2.At(3), L"nobar"));
        v2 = v;
        utassert(v2.Count() == 3 && v2.At(0) != v.At(0));
        utassert(str::Eq(v2.At(1), L"foo"));
        utassert(&v2.At(2) == v2.AtPtr(2) && str::Eq(*v2.AtPtr(2), L"glee"));
    }

    {
        WStrVec v2;
        size_t count = v2.Split(L"a,b,,c,", L",");
        utassert(count == 5 && v2.Find(L"c") == 3);
        utassert(v2.Find(L"") == 2 && v2.Find(L"", 3) == 4 && v2.Find(L"", 5) == -1);
        utassert(v2.Find(L"B") == -1 && v2.FindI(L"B") == 1);
        ScopedMem<WCHAR> joined(v2.Join(L";"));
        utassert(str::Eq(joined, L"a;b;;c;"));
    }

    {
        WStrVec v2;
        size_t count = v2.Split(L"a,b,,c,", L",", true);
        utassert(count == 3 && v2.Find(L"c") == 2);
        ScopedMem<WCHAR> joined(v2.Join(L";"));
        utassert(str::Eq(joined, L"a;b;c"));
        ScopedMem<WCHAR> last(v2.Pop());
        utassert(v2.Count() == 2 && str::Eq(last, L"c"));
    }
}

static void StrListTest()
{
    WStrList l;
    utassert(l.Count() == 0);
    l.Append(str::Dup(L"one"));
    l.Append(str::Dup(L"two"));
    l.Append(str::Dup(L"One"));
    utassert(l.Count() == 3);
    utassert(str::Eq(l.At(0), L"one"));
    utassert(str::EqI(l.At(2), L"one"));
    utassert(l.Find(L"One") == 2);
    utassert(l.FindI(L"One") == 0);
    utassert(l.Find(L"Two") == -1);
}

static size_t VecTestAppendFmt()
{
    str::Str<char> v(256);
    int64_t val = 1;
    for (int i = 0; i < 10000; i++) {
        v.AppendFmt("i%" PRId64 "e", val);
        val = (val * 3) / 2; // somewhat exponential growth
        val += 15;
    }
    size_t l = v.Count();
    return l;
}

void VecTest()
{
    Vec<int> ints;
    utassert(ints.Count() == 0);
    ints.Append(1);
    ints.Push(2);
    ints.InsertAt(0, -1);
    utassert(ints.Count() == 3);
    utassert(ints.At(0) == -1 && ints.At(1) == 1 && ints.At(2) == 2);
    utassert(ints.At(0) == -1 && ints.Last() == 2);
    int last = ints.Pop();
    utassert(last == 2);
    utassert(ints.Count() == 2);
    ints.Push(3);
    ints.RemoveAt(0);
    utassert(ints.Count() == 2);
    utassert(ints.At(0) == 1 && ints.At(1) == 3);
    ints.Reset();
    utassert(ints.Count() == 0);

    for (int i = 0; i < 1000; i++) {
        ints.Push(i);
    }
    utassert(ints.Count() == 1000 && ints.At(500) == 500);
    ints.Remove(500);
    utassert(ints.Count() == 999 && ints.At(500) == 501);
    last = ints.Pop();
    utassert(last == 999);
    ints.Append(last);
    utassert(ints.AtPtr(501) == &ints.At(501));

    for (int& value : ints) {
        utassert(0 <= value && value < 1000);
    }
    utassert(ints.FindEl([](int& value) { return value == 999; }) == 999);
    utassert(ints.FindEl([](int& value) { return value == 500; }) == 0);

    {
        Vec<int> ints2(ints);
        utassert(ints2.Count() == 999);
        utassert(ints.LendData() != ints2.LendData());
        ints.Remove(600);
        utassert(ints.Count() < ints2.Count());
        ints2 = ints;
        utassert(ints2.Count() == 998);
    }

    {
        char buf[2] = {'a', '\0'};
        str::Str<char> v(0);
        for (int i = 0; i < 7; i++) {
            v.Append(buf, 1);
            buf[0] = buf[0] + 1;
        }
        char *s = v.LendData();
        utassert(str::Eq("abcdefg", s));
        utassert(7 == v.Count());
        v.Set("helo");
        utassert(4 == v.Count());
        utassert(str::Eq("helo", v.LendData()));
    }

    {
        str::Str<char> v(128);
        v.Append("boo", 3);
        utassert(str::Eq("boo", v.LendData()));
        utassert(v.Count() == 3);
        v.Append("fop");
        utassert(str::Eq("boofop", v.LendData()));
        utassert(v.Count() == 6);
        v.RemoveAt(2, 3);
        utassert(v.Count() == 3);
        utassert(str::Eq("bop", v.LendData()));
        v.Append('a');
        utassert(v.Count() == 4);
        utassert(str::Eq("bopa", v.LendData()));
        char *s = v.StealData();
        utassert(str::Eq("bopa", s));
        free(s);
        utassert(v.Count() == 0);
    }

    {
        str::Str<char> v(0);
        for (size_t i = 0; i < 32; i++) {
            utassert(v.Count() == i * 6);
            v.Append("lambd", 5);
            if (i % 2 == 0)
                v.Append('a');
            else
                v.Push('a');
        }

        for (size_t i=1; i<=16; i++) {
            v.RemoveAt((16 - i) * 6, 6);
            utassert(v.Count() == (32 - i) * 6);
        }

        v.RemoveAt(0, 6 * 15);
        utassert(v.Count() == 6);
        char *s = v.LendData();
        utassert(str::Eq(s, "lambda"));
        s = v.StealData();
        utassert(str::Eq(s, "lambda"));
        free(s);
        utassert(v.Count() == 0);

        v.Append("lambda");
        utassert(str::Eq(v.LendData(), "lambda"));
        char c = v.Pop();
        utassert(c == 'a');
        utassert(str::Eq(v.LendData(), "lambd"));
    }

    VecTestAppendFmt();

    {
        Vec<PointI *> v;
        srand((unsigned int)time(NULL));
        for (int i = 0; i < 128; i++) {
            v.Append(new PointI(i, i));
            size_t pos = rand() % v.Count();
            v.InsertAt(pos, new PointI(i, i));
        }
        utassert(v.Count() == 128 * 2);
        size_t idx = 0;
        for (PointI **p = v.IterStart(); p; p = v.IterNext()) {
            utassert(idx == v.IterIdx());
            ++idx;
        }

        while (v.Count() > 64) {
            size_t pos = rand() % v.Count();
            PointI *f = v.At(pos);
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
        v.At(250) = 5;
        v.Reverse();
        utassert(v.Count() == 501 && v.At(0) == 4 && v.At(249) == v.At(251) && v.At(250) == 5 && v.At(500) == 2);
        v.Remove(4);
        v.Reverse();
        utassert(v.Count() == 500 && v.At(0) == 2 && v.At(249) == v.At(251) && v.At(250) == 5 && v.At(499) == 4);
    }

    {
        Vec<int> v;
        v.InsertAt(2, 2);
        utassert(v.Count() == 3 && v.At(0) == 0 && v.At(2) == 2);
    }

    WStrVecTest();
    StrListTest();
}

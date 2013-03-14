/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

static void WStrVecTest()
{
    WStrVec v;
    v.Append(str::Dup(L"foo"));
    v.Append(str::Dup(L"bar"));
    WCHAR *s = v.Join();
    assert(v.Count() == 2);
    assert(str::Eq(L"foobar", s));
    free(s);

    s = v.Join(L";");
    assert(v.Count() == 2);
    assert(str::Eq(L"foo;bar", s));
    free(s);

    v.Append(str::Dup(L"glee"));
    s = v.Join(L"_ _");
    assert(v.Count() == 3);
    assert(str::Eq(L"foo_ _bar_ _glee", s));
    free(s);

    v.Sort();
    s = v.Join();
    assert(str::Eq(L"barfooglee", s));
    free(s);

    {
        WStrVec v2(v);
        assert(str::Eq(v2.At(1), L"foo"));
        v2.Append(str::Dup(L"nobar"));
        assert(str::Eq(v2.At(3), L"nobar"));
        v2 = v;
        assert(v2.Count() == 3 && v2.At(0) != v.At(0));
        assert(str::Eq(v2.At(1), L"foo"));
        assert(&v2.At(2) == v2.AtPtr(2) && str::Eq(*v2.AtPtr(2), L"glee"));
    }

    {
        WStrVec v2;
        size_t count = v2.Split(L"a,b,,c,", L",");
        assert(count == 5 && v2.Find(L"c") == 3);
        assert(v2.Find(L"") == 2 && v2.Find(L"", 3) == 4 && v2.Find(L"", 5) == MAX_SIZE_T);
        assert(v2.Find(L"B") == MAX_SIZE_T && v2.FindI(L"B") == 1);
        ScopedMem<WCHAR> joined(v2.Join(L";"));
        assert(str::Eq(joined, L"a;b;;c;"));
    }

    {
        WStrVec v2;
        size_t count = v2.Split(L"a,b,,c,", L",", true);
        assert(count == 3 && v2.Find(L"c") == 2);
        ScopedMem<WCHAR> joined(v2.Join(L";"));
        assert(str::Eq(joined, L"a;b;c"));
        ScopedMem<WCHAR> last(v2.Pop());
        assert(v2.Count() == 2 && str::Eq(last, L"c"));
    }
}

static void StrListTest()
{
    WStrList l;
    assert(l.Count() == 0);
    l.Append(str::Dup(L"one"));
    l.Append(str::Dup(L"two"));
    l.Append(str::Dup(L"One"));
    assert(l.Count() == 3);
    assert(str::Eq(l.At(0), L"one"));
    assert(str::EqI(l.At(2), L"one"));
    assert(l.Find(L"One") == 2);
    assert(l.FindI(L"One") == 0);
    assert(l.Find(L"Two") == MAX_SIZE_T);
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

static void VecTest()
{
    Vec<int> ints;
    assert(ints.Count() == 0);
    ints.Append(1);
    ints.Push(2);
    ints.InsertAt(0, -1);
    assert(ints.Count() == 3);
    assert(ints.At(0) == -1 && ints.At(1) == 1 && ints.At(2) == 2);
    assert(ints.At(0) == -1 && ints.Last() == 2);
    int last = ints.Pop();
    assert(last == 2);
    assert(ints.Count() == 2);
    ints.Push(3);
    ints.RemoveAt(0);
    assert(ints.Count() == 2);
    assert(ints.At(0) == 1 && ints.At(1) == 3);
    ints.Reset();
    assert(ints.Count() == 0);

    for (int i = 0; i < 1000; i++) {
        ints.Push(i);
    }
    assert(ints.Count() == 1000 && ints.At(500) == 500);
    ints.Remove(500);
    assert(ints.Count() == 999 && ints.At(500) == 501);
    last = ints.Pop();
    assert(last == 999);
    ints.Append(last);
    assert(ints.AtPtr(501) == &ints.At(501));

    {
        Vec<int> ints2(ints);
        assert(ints2.Count() == 999);
        assert(ints.LendData() != ints2.LendData());
        ints.Remove(600);
        assert(ints.Count() < ints2.Count());
        ints2 = ints;
        assert(ints2.Count() == 998);
    }

    {
        char buf[2] = {'a', '\0'};
        str::Str<char> v(0);
        for (int i = 0; i < 7; i++) {
            v.Append(buf, 1);
            buf[0] = buf[0] + 1;
        }
        char *s = v.LendData();
        assert(str::Eq("abcdefg", s));
        assert(7 == v.Count());
        v.Set("helo");
        assert(4 == v.Count());
        assert(str::Eq("helo", v.LendData()));
    }

    {
        str::Str<char> v(128);
        v.Append("boo", 3);
        assert(str::Eq("boo", v.LendData()));
        assert(v.Count() == 3);
        v.Append("fop");
        assert(str::Eq("boofop", v.LendData()));
        assert(v.Count() == 6);
        v.RemoveAt(2, 3);
        assert(v.Count() == 3);
        assert(str::Eq("bop", v.LendData()));
        v.Append('a');
        assert(v.Count() == 4);
        assert(str::Eq("bopa", v.LendData()));
        char *s = v.StealData();
        assert(str::Eq("bopa", s));
        free(s);
        assert(v.Count() == 0);
    }

    {
        str::Str<char> v(0);
        for (int i = 0; i < 32; i++) {
            assert(v.Count() == i * 6);
            v.Append("lambd", 5);
            if (i % 2 == 0)
                v.Append('a');
            else
                v.Push('a');
        }

        for (int i=1; i<=16; i++) {
            v.RemoveAt((16 - i) * 6, 6);
            assert(v.Count() == (32 - i) * 6);
        }

        v.RemoveAt(0, 6 * 15);
        assert(v.Count() == 6);
        char *s = v.LendData();
        assert(str::Eq(s, "lambda"));
        s = v.StealData();
        assert(str::Eq(s, "lambda"));
        free(s);
        assert(v.Count() == 0);

        v.Append("lambda");
        assert(str::Eq(v.LendData(), "lambda"));
        char c = v.Pop();
        assert(c == 'a');
        assert(str::Eq(v.LendData(), "lambd"));
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
        assert(v.Count() == 128 * 2);
        size_t idx = 0;
        for (PointI **p = v.IterStart(); p; p = v.IterNext()) {
            assert(idx == v.IterIdx());
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
        assert(v.Count() == 501 && v.At(0) == 4 && v.At(249) == v.At(251) && v.At(250) == 5 && v.At(500) == 2);
        v.Remove(4);
        v.Reverse();
        assert(v.Count() == 500 && v.At(0) == 2 && v.At(249) == v.At(251) && v.At(250) == 5 && v.At(499) == 4);
    }
}

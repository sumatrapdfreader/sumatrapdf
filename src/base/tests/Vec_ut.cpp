/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

// must be last due to assert() over-write
#include "base/UtAssert.h"

template <typename T>
concept CanNegateVec = requires(T v) {
    !v;
};

static_assert(!CanNegateVec<Vec<int>>);

// capacity, whether the storage is owned or borrowed. Was a Vec function, but
// only these tests look at the capacity.
template <typename T>
static int VecCap(const Vec<T>& v) {
    return v.cap < 0 ? -v.cap : v.cap;
}

static size_t VecTestAppendFmt() {
    str::Builder v;
    str::BuilderReserve(v, 256);
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
    VecAppend(ints, 1);
    utassert(VecCap(ints) == 4);
    VecAppend(ints, 2);
    VecInsertAt(ints, 0, -1);
    utassert(len(ints) == 3);
    utassert(ints[0] == -1 && ints[1] == 1 && ints[2] == 2);
    utassert(ints[0] == -1 && VecLast(ints) == 2);
    int last = VecPop(ints);
    utassert(last == 2);
    utassert(len(ints) == 2);
    VecAppend(ints, 3);
    VecRemoveAt(ints, 0);
    utassert(len(ints) == 2);
    utassert(ints[0] == 1 && ints[1] == 3);
    VecReset(ints);
    utassert(len(ints) == 0);

    for (int i = 0; i < 1000; i++) {
        VecAppend(ints, i);
    }
    utassert(len(ints) == 1000 && ints[500] == 500);
    VecRemove(ints, 500);
    utassert(len(ints) == 999 && ints[500] == 501);
    last = VecPop(ints);
    utassert(last == 999);
    VecAppend(ints, last);

    for (int& value : ints) {
        utassert(0 <= value && value < 1000);
    }

    {
        Vec<int> ints2(ints);
        utassert(len(ints2) == 999);
        utassert(VecData(ints) != VecData(ints2));
        VecRemove(ints, 600);
        utassert(len(ints) < len(ints2));
        ints2 = ints;
        utassert(len(ints2) == 998);
    }

    {
        int buf[4];
        Vec<int> v;
        VecUseExternalBuffer(v, buf);
        utassert(VecCap(v) == 4);
        utassert(v.els == buf);
        for (int i = 0; i < 4; i++) {
            VecAppend(v, i);
        }
        utassert(len(v) == 4);
        utassert(v.els == buf);
        utassert(v[0] == 0 && v[3] == 3);
        VecAppend(v, 4);
        utassert(len(v) == 5);
        utassert(v.els != buf);
        utassert(VecCap(v) >= 5);
        utassert(v[0] == 0 && v[4] == 4);
        VecReset(v);
        utassert(v.els == nullptr);
    }

    {
        char buf[2] = {'a', '\0'};
        str::Builder v;
        for (int i = 0; i < 7; i++) {
            v.Append(Str(buf, 1));
            buf[0] = (char)(buf[0] + 1);
        }
        Str s = ToStr(v);
        utassert(str::Eq(StrL("abcdefg"), s));
        utassert(7 == len(v));
        v.Reset(StrL("helo"));
        utassert(4 == len(v));
        utassert(str::Eq(StrL("helo"), ToStr(v)));
    }

    {
        str::Builder v;
        str::BuilderReserve(v, 128);
        v.Append(StrL("boo"));
        utassert(str::Eq(StrL("boo"), ToStr(v)));
        utassert(len(v) == 3);
        v.Append(StrL("fop"));
        utassert(str::Eq(StrL("boofop"), ToStr(v)));
        utassert(len(v) == 6);
        v.RemoveAt(2, 3);
        utassert(len(v) == 3);
        utassert(str::Eq(StrL("bop"), ToStr(v)));
        v.AppendChar('a');
        utassert(len(v) == 4);
        utassert(str::Eq(StrL("bopa"), ToStr(v)));
        Str s = v.TakeStr();
        utassert(str::Eq(StrL("bopa"), s));
        str::Free(s);
        utassert(len(v) == 0);
    }

    {
        str::Builder v;
        for (int i = 0; i < 32; i++) {
            utassert(len(v) == i * 6);
            v.Append(StrL("lambd"));
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
        utassert(str::Eq(s, StrL("lambda")));
        s = v.TakeStr();
        utassert(str::Eq(s, StrL("lambda")));
        str::Free(s);
        utassert(len(v) == 0);

        v.Append(StrL("lambda"));
        utassert(str::Eq(ToStr(v), StrL("lambda")));
        char c = v.RemoveLast();
        utassert(c == 'a');
        utassert(str::Eq(ToStr(v), StrL("lambd")));
    }

    VecTestAppendFmt();

    {
        Vec<Point*> v;
        srand((unsigned int)time(nullptr));
        for (int i = 0; i < 128; i++) {
            VecAppend(v, new Point(i, i));
            int pos = rand() % len(v);
            VecInsertAt(v, pos, new Point(i, i));
        }
        utassert(len(v) == 128 * 2);

        while (len(v) > 64) {
            size_t pos = rand() % len(v);
            Point* f = v[(int)pos];
            VecRemove(v, f);
            delete f;
        }
        DeleteVecMembers(v);
    }

    {
        Vec<int> v;
        VecAppend(v, 2);
        for (int i = 0; i < 500; i++) VecAppend(v, 4);
        v[250] = 5;
        VecReverse(v);
        utassert(len(v) == 501 && v[0] == 4 && v[249] == v[251] && v[250] == 5 && v[500] == 2);
        VecRemove(v, 4);
        VecReverse(v);
        utassert(len(v) == 500 && v[0] == 2 && v[249] == v[251] && v[250] == 5 && v[499] == 4);
    }

    {
        Vec<int> v;
        VecInsertAt(v, 2, 2);
        auto size = len(v);
        utassert(size == 3);
        auto el0 = v[0];
        utassert(el0 == 0);
        auto el2 = v[2];
        utassert(el2 == 2);
    }

    {
        str::Builder v;
        v.Append(StrL("foo"));
        utassert(len(v) == 3);
    }
}

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/StrFormat.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void check(Str got, Str expected) {
    utassert(str::Eq(got, expected));
}

void StrFormatTest() {
    Str s;
    AutoFreeStr s2;

    {
        s = strfmt::Format("c: {0}, i: {1}", 'x', -18);
        check(s, "c: x, i: -18");
        str::Free(s);
    }

    s = strfmt::FormatTemp("%04d", 34);
    s2 = str::Dup(fmt("%04d", 34)).s;
    check(s, Str(s2.Get()));

    {
        s = strfmt::Format("int: %d, s: %s", 5, "foo");
        check(s, "int: 5, s: foo");
        str::Free(s);
    }
    {
        s = strfmt::Format("be{0}-af", 888723);
        check(s, "be888723-af");
        str::Free(s);
    }
    {
        s = strfmt::Format("\\{03foo{0}", 255);
        check(s, "{03foo255");
        str::Free(s);
    }

    {
        s = strfmt::Format("int: {1}, s: {0}", L"hello", -1);
        check(s, "int: -1, s: hello");
        str::Free(s);
    }
    {
        s = strfmt::Format(" {0}  ", 99);
        check(s, " 99  ");
        str::Free(s);
    }
    {
        s = strfmt::Format("{0}goal", "ah");
        check(s, "ahgoal");
        str::Free(s);
    }
    {
        s = strfmt::Format("{1}-{0}", "so", L"r");
        check(s, "r-so");
        str::Free(s);
    }
    {
        s = strfmt::Format("{1}-{0}", "so", L"r");
        check(s, "r-so");
        str::Free(s);
    }
    {
        s = strfmt::Format("{0}", L"1");
        check(s, "1");
        str::Free(s);
    }

    {
        s = strfmt::Format(
            "c: %c, i: %d, f: %f, d: %f, s: %s, ws: %s, c: {0}, i: {1}, f: {2}, d: {3}, s: {4}, ws: {5}, i: {1}", 'x',
            -18, 3.45f, -18.38f, "str", L"wstr");
        check(s,
              "c: x, i: -18, f: 3.45, d: -18.38, s: str, ws: wstr, c: x, i: -18, f: 3.45, d: -18.38, s: str, ws: wstr, "
              "i: "
              "-18");
        str::Free(s);
    }

#if 0
#endif

    {
        s = strfmt::Format("foo %s bar %d %s", "sa", 5, L"sab");
        check(s, "foo sa bar 5 sab");
        str::Free(s);
    }

    s = strfmt::FormatTemp("foo %d", -23);
    check(s, "foo -23");

    s = strfmt::FormatTemp("foo {0}", -23);
    check(s, "foo -23");

    s = strfmt::FormatTemp("foo %v", -23);
    check(s, "foo -23");
}

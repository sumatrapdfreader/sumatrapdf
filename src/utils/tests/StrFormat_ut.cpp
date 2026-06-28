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

    s = strfmt::FormatTemp("c: {0}, i: {1}", 'x', -18);
    check(s, "c: x, i: -18");

    s = strfmt::FormatTemp("%04d", 34);
    Str s2 = fmt("%04d", 34);
    check(s, s2);

    s = strfmt::FormatTemp("int: %d, s: %s", 5, "foo");
    check(s, "int: 5, s: foo");

    s = strfmt::FormatTemp("be{0}-af", 888723);
    check(s, "be888723-af");

    s = strfmt::FormatTemp("\\{03foo{0}", 255);
    check(s, "{03foo255");

    s = strfmt::FormatTemp("int: {1}, s: {0}", L"hello", -1);
    check(s, "int: -1, s: hello");

    s = strfmt::FormatTemp(" {0}  ", 99);
    check(s, " 99  ");

    s = strfmt::FormatTemp("{0}goal", "ah");
    check(s, "ahgoal");

    s = strfmt::FormatTemp("{1}-{0}", "so", L"r");
    check(s, "r-so");

    s = strfmt::FormatTemp("{1}-{0}", "so", L"r");
    check(s, "r-so");

    s = strfmt::FormatTemp("{0}", L"1");
    check(s, "1");

    s = strfmt::FormatTemp(
        "c: %c, i: %d, f: %f, d: %f, s: %s, ws: %s, c: {0}, i: {1}, f: {2}, d: {3}, s: {4}, ws: {5}, i: {1}", 'x', -18,
        3.45f, -18.38f, "str", L"wstr");
    check(s,
          "c: x, i: -18, f: 3.45, d: -18.38, s: str, ws: wstr, c: x, i: -18, f: 3.45, d: -18.38, s: str, ws: wstr, "
          "i: "
          "-18");

    s = strfmt::FormatTemp("foo %s bar %d %s", "sa", 5, L"sab");
    check(s, "foo sa bar 5 sab");

    s = strfmt::FormatTemp("foo %d", -23);
    check(s, "foo -23");

    s = strfmt::FormatTemp("foo {0}", -23);
    check(s, "foo -23");

    s = strfmt::FormatTemp("foo %v", -23);
    check(s, "foo -23");
}

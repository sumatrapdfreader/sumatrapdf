/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/StrFormat.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

#define check(got, expected) utassert(str::Eq(got, expected))

void StrFormatTest() {
    fmt::Fmt f("int: %d, s: %s");
    char* s = f.i(5).s("foo").Get();
    check(s, "int: 5, s: foo");

    check(f.ParseFormat("be{0}af").i(-1).Get(), "be-1af");
    check(f.ParseFormat("\\{03foo{0}").i(255).Get(), "{03foo255");
    check(f.ParseFormat("int: {1}, s: {0}").s(L"hello").i(-1).Get(), "int: -1, s: hello");
    check(f.ParseFormat("{0}").i(1).Get(), "1");
    check(f.ParseFormat("{0}goal").s("ah").Get(), "ahgoal");
    check(f.ParseFormat("{1}-{0}").s("so").s(L"r").Get(), "r-so");
    check(f.ParseFormat("{0}").s(L"1").Get(), "1");
    s = f.ParseFormat(
             "c: %c, i: %d, f: %f, d: %f, s: %s, ws: %s, c: {0}, i: {1}, f: {2}, d: {3}, s: {4}, ws: {5}, i: {1}")
            .c('x')
            .i(-18)
            .f(3.45)
            .f(-18.38)
            .s("str")
            .s(L"wstr")
            .GetDup();
    check(s,
          "c: x, i: -18, f: 3.45, d: -18.38, s: str, ws: wstr, c: x, i: -18, f: 3.45, d: -18.38, s: str, ws: wstr, i: "
          "-18");
    free(s);
}

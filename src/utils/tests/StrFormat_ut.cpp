/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/StrFormat.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void check(const char* got, const char* expected) {
    utassert(str::Eq(got, expected));
}

void check(std::string_view got, const char* expected) {
    utassert(str::Eq(got, expected));
}

void StrFormatTest() {
    {
        auto s = fmt::Format("int: %d, s: %s", 5, "foo");
        check(s, "int: 5, s: foo");
        str::Free(s);
    }
    {
        auto s = fmt::Format("be{0}-af", 888723);
        check(s, "be888723-af");
        str::Free(s);
    }
    {
        auto s = fmt::Format("\\{03foo{0}", 255);
        check(s, "{03foo255");
        str::Free(s);
    }

    {
        auto s = fmt::Format("int: {1}, s: {0}", L"hello", -1);
        check(s, "int: -1, s: hello");
        str::Free(s);
    }
    {
        auto s = fmt::Format(" {0}  ", 99);
        check(s, " 99  ");
        str::Free(s);
    }
    {
        auto s = fmt::Format("{0}goal", "ah");
        check(s, "ahgoal");
        str::Free(s);
    }
    {
        auto s = fmt::Format("{1}-{0}", "so", L"r");
        check(s, "r-so");
        str::Free(s);
    }
    {
        auto s = fmt::Format("{1}-{0}", "so", L"r");
        check(s, "r-so");
        str::Free(s);
    }
    {
        auto s = fmt::Format("{0}", L"1");
        check(s, "1");
        str::Free(s);
    }
    {
        auto s = fmt::Format(
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
        auto s = fmt::Format("foo %s bar %d %s", "sa", 5, L"sab").data();
        check(s, "foo sa bar 5 sab");
        str::Free(s);
    }
}

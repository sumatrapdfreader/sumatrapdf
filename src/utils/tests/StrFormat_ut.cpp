/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

// must be last due to assert() over-write
#include "UtAssert.h"

static void StrFormatCheck(const char *s, const char *expected)
{
    utassert(str::Eq(s, expected));
}

static void StrFormatCheck(const WCHAR *s, const WCHAR *expected)
{
    utassert(str::Eq(s, expected));
}

void StrFormatTest()
{
    fmt::Fmt f("int: %d, s: %s");
    const char *s = f.i(5).s("foo").Get();
    StrFormatCheck(s, "int: 5, s: foo");

    s = f.ParseFormat("int: {1}, s: {0}").s(L"hello").i(-1).Get();
    StrFormatCheck(s, "int: -1, s: hello"));
    StrFormatCheck(f.ParseFormat("{0}").i(1).Get(), "1");
    StrFormatCheck(f.ParseFormat("{03foo{0}").i(255).Get(), "{03foo255");
    StrFormatCheck(f.ParseFormat("be{0}af").i(-1).Get(), "be-1af");
    StrFormatCheck(f.ParseFormat("{0}goal").s("ah").Get(), "ahgoal");
    StrFormatCheck(f.ParseFormat("{1}-{0}").s("so").s(L"r").Get(), "r-so");
    StrFormatCheck(f.ParseFormat("{0}").s(L"1"), L"1");

#if 0
    StrFormatCheck(str::Fmt(L"{03foo{0}", 255), L"{03foo255");
    StrFormatCheck(str::Fmt(L"be{0}af", -1), L"be-1af");
    StrFormatCheck(str::Fmt(L"{0}goal", str::Arg("ah")), L"ahgoal");
    StrFormatCheck(str::Fmt(L"{1}-{0}", str::Arg("so"), str::Arg(L"r")), L"r-so");
#endif
}

#endif

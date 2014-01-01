/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

// must be last due to assert() over-write
#include "UtAssert.h"

static void StrFormatCheck(const char *s, const char *expected)
{
    utassert(str::Eq(s, expected));
    free((void*)s);
}

static void StrFormatCheck(const WCHAR *s, const WCHAR *expected)
{
    utassert(str::Eq(s, expected));
    free((void*)s);
}

void StrFormatTest()
{
    StrFormatCheck(str::Fmt("{0}", 1), "1");
    StrFormatCheck(str::Fmt("{03foo{0}", 255), "{03foo255");
    StrFormatCheck(str::Fmt("be{0}af", -1), "be-1af");
    StrFormatCheck(str::Fmt("{0}goal", str::Arg("ah")), "ahgoal");
    StrFormatCheck(str::Fmt("{1}-{0}", str::Arg("so"), str::Arg(L"r")), "r-so");

    StrFormatCheck(str::Fmt(L"{0}", 1), L"1");
    StrFormatCheck(str::Fmt(L"{03foo{0}", 255), L"{03foo255");
    StrFormatCheck(str::Fmt(L"be{0}af", -1), L"be-1af");
    StrFormatCheck(str::Fmt(L"{0}goal", str::Arg("ah")), L"ahgoal");
    StrFormatCheck(str::Fmt(L"{1}-{0}", str::Arg("so"), str::Arg(L"r")), L"r-so");

}

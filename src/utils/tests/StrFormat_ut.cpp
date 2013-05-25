/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "StrFormat.h"

static void StrFormatCheck(char *s, char *expected)
{
    assert(str::Eq(s, expected));
    free(s);
}

static void StrFormatTest()
{
    StrFormatCheck(str::Fmt("{0}", 1), "1");
    StrFormatCheck(str::Fmt("{03foo{0}", 255), "{03foo255");
    StrFormatCheck(str::Fmt("be{0}af", -1), "be-1af");
    //StrFormatCheck(str::Fmt("{1}-{0}", "so", L"r"), "r-so");
}

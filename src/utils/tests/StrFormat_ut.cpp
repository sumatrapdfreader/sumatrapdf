/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

// must be last due to assert() over-write
#include "UtAssert.h"

static void StrFormatCheck(char *s, char *expected)
{
    utassert(str::Eq(s, expected));
    free(s);
}

void StrFormatTest()
{
    StrFormatCheck(str::Fmt("{0}", 1), "1");
    StrFormatCheck(str::Fmt("{03foo{0}", 255), "{03foo255");
    StrFormatCheck(str::Fmt("be{0}af", -1), "be-1af");
    // TODO: don't understand why have to use str::Arg() to resolve compiler's
    // ambiguity about which of 2 str::Fmt() calls I'm trying to use. Should be
    // clear from 
    StrFormatCheck(str::Fmt("{1}-{0}", str::Arg("so"), str::Arg(L"r")), "r-so");
}

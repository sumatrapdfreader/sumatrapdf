/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "CmdLineParser.h"

// must be last due to assert() over-write
#include "UtAssert.h"

void CmdLineParserTest()
{
    WStrVec args;

    ParseCmdLine(L"test.exe -arg foo.pdf", args);
    utassert(3 == args.Count());
    utassert(str::Eq(args.At(0), L"test.exe"));
    utassert(str::Eq(args.At(1), L"-arg"));
    utassert(str::Eq(args.At(2), L"foo.pdf"));
    args.Reset();

    ParseCmdLine(L"test.exe \"foo \\\" bar \\\\.pdf\" un\\\"quoted.pdf", args);
    utassert(3 == args.Count());
    utassert(str::Eq(args.At(0), L"test.exe"));
    utassert(str::Eq(args.At(1), L"foo \" bar \\\\.pdf"));
    utassert(str::Eq(args.At(2), L"un\"quoted.pdf"));
    args.Reset();

    ParseCmdLine(L"test.exe \"foo\".pdf foo\" bar.pdf ", args);
    utassert(3 == args.Count());
    utassert(str::Eq(args.At(0), L"test.exe"));
    utassert(str::Eq(args.At(1), L"foo.pdf"));
    utassert(str::Eq(args.At(2), L"foo bar.pdf "));
    args.Reset();

    ParseCmdLine(L"test.exe -arg \"%1\" -more", args, 2);
    utassert(2 == args.Count());
    utassert(str::Eq(args.At(0), L"test.exe"));
    utassert(str::Eq(args.At(1), L"-arg \"%1\" -more"));
    args.Reset();
}

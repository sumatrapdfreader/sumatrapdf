/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/CmdLineParser.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void CmdLineParserTest() {
    WStrVec args;

    ParseCmdLine(L"test.exe -arg foo.pdf", args);
    utassert(3 == args.size());
    utassert(str::Eq(args.at(0), L"test.exe"));
    utassert(str::Eq(args.at(1), L"-arg"));
    utassert(str::Eq(args.at(2), L"foo.pdf"));
    args.Reset();

    ParseCmdLine(L"test.exe \"foo \\\" bar \\\\.pdf\" un\\\"quoted.pdf", args);
    utassert(3 == args.size());
    utassert(str::Eq(args.at(0), L"test.exe"));
    utassert(str::Eq(args.at(1), L"foo \" bar \\\\.pdf"));
    utassert(str::Eq(args.at(2), L"un\"quoted.pdf"));
    args.Reset();

    ParseCmdLine(L"test.exe \"foo\".pdf foo\" bar.pdf ", args);
    utassert(3 == args.size());
    utassert(str::Eq(args.at(0), L"test.exe"));
    utassert(str::Eq(args.at(1), L"foo.pdf"));
    utassert(str::Eq(args.at(2), L"foo bar.pdf "));
    args.Reset();

    ParseCmdLine(L"test.exe -arg \"%1\" -more", args, 2);
    utassert(2 == args.size());
    utassert(str::Eq(args.at(0), L"test.exe"));
    utassert(str::Eq(args.at(1), L"-arg \"%1\" -more"));
    args.Reset();
}

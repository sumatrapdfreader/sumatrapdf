/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"

#include "base/CmdLineArgsIter.h"

#define REMOVE_FIRST_ARG

#if defined(REMOVE_FIRST_ARG)
void ParseCmdLine(WStr cmdLine, StrVec& argsOut) {
    int nArgs;
    WCHAR** argsArr = CommandLineToArgvW(CWStrTemp(cmdLine), &nArgs);
    for (int i = 0; i < nArgs; i++) {
        TempStr arg = ToUtf8Temp(argsArr[i]);
        if (len(arg) == 0) {
            continue;
        }
        argsOut.Append(arg);
    }
    LocalFree(argsArr);
}
#else
void ParseCmdLine(WStr cmdLine, StrVec& argsOut) {
    int nArgs;
    WCHAR** argsArr = CommandLineToArgvW(CWStrTemp(cmdLine), &nArgs);
    TempStr exePath = GetSelfExePathTemp();
    for (int i = 0; i < nArgs; i++) {
        TempStr arg = ToUtf8Temp(argsArr[i]);
        if (i == 0 && path::IsSame(arg, exePath)) {
            continue;
        }
        if (len(arg) == 0) {
            continue;
        }
        argsOut.Append(arg);
    }
    LocalFree(argsArr);
}
#endif

void ParseCmdLine(Str cmdLine, StrVec& argsOut) {
    TempWStr s = ToWStrTemp(cmdLine);
    ParseCmdLine(s, argsOut);
}

CmdLineArgsIter::CmdLineArgsIter(WStr cmdLine) {
    ParseCmdLine(cmdLine, args);
    nArgs = len(args);
#if defined(REMOVE_FIRST_ARG)
    curr = 1;
#endif
}

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "utils/CmdLineArgsIter.h"

#define REMOVE_FIRST_ARG

// TODO: quote '"' etc as per:
// https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?view=msvc-170&redirectedfrom=MSDN
TempStr QuoteCmdLineArgTemp(char* arg) {
    if (!arg) {
        return nullptr;
    }
    int n = (int)str::Len(arg);
    if (n < 2) {
        return arg;
    }
    if (*arg == '"' && arg[n - 1] == '"') {
        // already quoted, we assume correctly
        return arg;
    }
    bool needsQuote = false;
    char* s = arg;
    char c = *s++;
    while (c) {
        if (c == ' ' || c == '"') {
            needsQuote = true;
            break;
        }
        c = *s++;
    }
    if (!needsQuote) {
        return arg;
    }
    str::Str res;
    // TODO: can't do it because PoolAllocator doesn't support Realloc()
    // res.allocator = GetTempAllocator();
    res.AppendChar('"');
    s = arg;
    c = *s++;
    while (c) {
        // TODO: quote '"' ?
        res.AppendChar(c);
        c = *s++;
    }
    res.AppendChar('"');
    return res.StealData();
}

#if defined(REMOVE_FIRST_ARG)
void ParseCmdLine(const WCHAR* cmdLine, StrVec& argsOut) {
    int nArgs;
    WCHAR** argsArr = CommandLineToArgvW(cmdLine, &nArgs);
    for (int i = 0; i < nArgs; i++) {
        char* arg = ToUtf8Temp(argsArr[i]);
        // ignore empty quoted strings ("")
        if (str::IsEmpty(arg)) {
            continue;
        }
        argsOut.Append(arg);
    }
    LocalFree(argsArr);
}
#else
void ParseCmdLine(const WCHAR* cmdLine, StrVec& argsOut) {
    int nArgs;
    WCHAR** argsArr = CommandLineToArgvW(cmdLine, &nArgs);
    TempStr exePath = GetSelfExePathTemp();
    for (int i = 0; i < nArgs; i++) {
        char* arg = ToUtf8Temp(argsArr[i]);
        // sometimes cmd-line args have exe as first argument, sometimes not
        // to handle both possibilities we filter out first arg if it is path
        // of our executable, case-insensitive because not all filesystems
        if (i == 0 && path::IsSame(arg, exePath)) {
            continue;
        }
        // ignore empty quoted strings ("")
        if (str::IsEmpty(arg)) {
            continue;
        }
        argsOut.Append(arg);
    }
    LocalFree(argsArr);
}
#endif

void ParseCmdLine(const char* cmdLine, StrVec& argsOut) {
    TempWStr s = ToWStrTemp(cmdLine);
    ParseCmdLine(s, argsOut);
}

bool CouldBeArg(const char* s) {
    char c = *s;
    return (c == L'-') || (c == L'/');
}

CmdLineArgsIter::CmdLineArgsIter(const WCHAR* cmdLine) {
    ParseCmdLine(cmdLine, args);
    nArgs = args.Size();
#if defined(REMOVE_FIRST_ARG)
    // first arg is executable name by convention, we skip it
    curr = 1;
#endif
}

const char* CmdLineArgsIter::NextArg() {
    if (curr >= nArgs) {
        return nullptr;
    }
    currArg = args[curr++];
    return currArg;
}

const char* CmdLineArgsIter::EatParam() {
    // doesn't change currArg
    if (curr >= nArgs) {
        return nullptr;
    }
    return args[curr++];
}

void CmdLineArgsIter::RewindParam() {
    // undo EatParam()
    --curr;
    ReportIf(curr < 1);
}

// additional param is one in addition to the default first param
// they start at 1
// returns nullptr if no additional param
const char* CmdLineArgsIter::AdditionalParam(int n) const {
    ReportIf(n < 1);
    if (curr + n - 1 >= nArgs) {
        return nullptr;
    }

    // we assume that param cannot be args (i.e. start with - or /
    for (int i = 0; i < n; i++) {
        const char* s = args[curr + i];
        if (CouldBeArg(s)) {
            return nullptr;
        }
    }
    return args[curr + n - 1];
}

char* CmdLineArgsIter::at(int n) const {
    return args[n];
}

// returns just the params i.e. everything but the first
// arg (which is the name of the command)
// returns nullptr if no args
char* CmdLineArgsIter::ParamsTemp() {
    if (nArgs < 2) {
        return nullptr;
    }
    if (nArgs == 2) {
        return args[1];
    }
    // must concat all the
    char* s = args[1];
    for (int i = 2; i < nArgs; i++) {
        s = str::JoinTemp(s, " ", args[i]);
    }
    return s;
}

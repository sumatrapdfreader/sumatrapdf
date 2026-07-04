/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"

#include "base/CmdLineArgsIter.h"

#define REMOVE_FIRST_ARG

// TODO: quote '"' etc as per:
// https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?view=msvc-170&redirectedfrom=MSDN
TempStr QuoteCmdLineArgTemp(Str arg) {
    if (!arg) {
        return {};
    }
    int n = arg.len;
    if (n < 2) {
        return arg;
    }
    if (arg.s[0] == '"' && arg.s[n - 1] == '"') {
        // already quoted, we assume correctly
        return arg;
    }
    bool needsQuote = false;
    for (int i = 0; i < n; i++) {
        char c = arg.s[i];
        if (c == ' ' || c == '"') {
            needsQuote = true;
            break;
        }
    }
    if (!needsQuote) {
        return arg;
    }
    str::Builder res;
    res.AppendChar('"');
    for (int i = 0; i < n; i++) {
        res.AppendChar(arg.s[i]);
    }
    res.AppendChar('"');
    return ToStrTemp(res);
}

#if defined(REMOVE_FIRST_ARG)
void ParseCmdLine(WStr cmdLine, StrVec& argsOut) {
    int nArgs;
    // CommandLineToArgvW reads a NUL-terminated string; cmdLine (a WStr) may be
    // a non-terminated view, so use a terminated copy
    WCHAR** argsArr = CommandLineToArgvW(CWStrTemp(cmdLine), &nArgs);
    for (int i = 0; i < nArgs; i++) {
        TempStr arg = ToUtf8Temp(argsArr[i]);
        // ignore empty quoted strings ("")
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
    // CommandLineToArgvW reads a NUL-terminated string; cmdLine (a WStr) may be
    // a non-terminated view, so use a terminated copy
    WCHAR** argsArr = CommandLineToArgvW(CWStrTemp(cmdLine), &nArgs);
    TempStr exePath = GetSelfExePathTemp();
    for (int i = 0; i < nArgs; i++) {
        TempStr arg = ToUtf8Temp(argsArr[i]);
        // sometimes cmd-line args have exe as first argument, sometimes not
        // to handle both possibilities we filter out first arg if it is path
        // of our executable, case-insensitive because not all filesystems
        if (i == 0 && path::IsSame(arg, exePath)) {
            continue;
        }
        // ignore empty quoted strings ("")
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

bool CouldBeArg(Str s) {
    if (!s) {
        return false;
    }
    char c = *s.s;
    return (c == '-') || (c == '/');
}

CmdLineArgsIter::CmdLineArgsIter(WStr cmdLine) {
    ParseCmdLine(cmdLine, args);
    nArgs = len(args);
#if defined(REMOVE_FIRST_ARG)
    curr = 1;
#endif
}

Str CmdLineArgsIter::NextArg() {
    if (curr >= nArgs) {
        return {};
    }
    currArg = args[curr++];
    return currArg;
}

Str CmdLineArgsIter::EatParam() {
    // doesn't change currArg
    if (curr >= nArgs) {
        return {};
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
Str CmdLineArgsIter::AdditionalParam(int n) const {
    ReportIf(n < 1);
    if (curr + n - 1 >= nArgs) {
        return {};
    }

    // we assume that param cannot be args (i.e. start with - or /
    for (int i = 0; i < n; i++) {
        Str s = args[curr + i];
        if (CouldBeArg(s)) {
            return {};
        }
    }
    return args[curr + n - 1];
}

Str CmdLineArgsIter::at(int n) const {
    return args[n];
}

// returns just the params i.e. everything but the first
// arg (which is the name of the command)
// returns nullptr if no args
TempStr CmdLineArgsIter::ParamsTemp() {
    if (nArgs < 2) {
        return {};
    }
    if (nArgs == 2) {
        return Str(args[1]);
    }
    // must concat all the
    TempStr s = Str(args[1]);
    for (int i = 2; i < nArgs; i++) {
        s = str::JoinTemp(s, StrL(" "), args[i]);
    }
    return s;
}

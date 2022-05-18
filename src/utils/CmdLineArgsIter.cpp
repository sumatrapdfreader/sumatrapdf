/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/CmdLineArgsIter.h"

bool CouldBeArg(const char* s) {
    char c = *s;
    return (c == L'-') || (c == L'/');
}

CmdLineArgsIter::~CmdLineArgsIter() {
}

CmdLineArgsIter::CmdLineArgsIter(const WCHAR* cmdLine) {
    WCHAR** argsArr = CommandLineToArgvW(cmdLine, &nArgs);
    for (int i = 0; i < nArgs; i++) {
        char* arg = ToUtf8Temp(argsArr[i]);
        args.Append(arg);
    }
    LocalFree(argsArr);
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

/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/CmdLineParser.h"

bool CouldBeArg(const WCHAR* s) {
    WCHAR c = *s;
    return (c == L'-') || (c == L'/');
}

ArgsIter::~ArgsIter() {
    LocalFree(args);
}

ArgsIter::ArgsIter(const WCHAR* cmdLine) {
    args = CommandLineToArgvW(cmdLine, &nArgs);
}

const WCHAR* ArgsIter::NextArg() {
    if (curr >= nArgs) {
        return nullptr;
    }
    currArg = args[curr++];
    return currArg;
}

const WCHAR* ArgsIter::EatParam() {
    // doesn't change currArg
    if (curr >= nArgs) {
        return nullptr;
    }
    return args[curr++];
}

void ArgsIter::RewindParam() {
    // undo EatParam()
    --curr;
    ReportIf(curr < 1);
}

// additional param is one in addition to the default first param
// they start at 1
// returns nullptr if no additional param
const WCHAR* ArgsIter::AdditionalParam(int n) const {
    ReportIf(n < 1);
    if (curr + n - 1 >= nArgs) {
        return nullptr;
    }

    // we assume that param cannot be args (i.e. start with - or /
    for (int i = 0; i < n; i++) {
        const WCHAR* s = args[curr + i];
        if (CouldBeArg(s)) {
            return nullptr;
        }
    }
    return args[curr + n - 1];
}

WCHAR* ArgsIter::at(int n) const {
    return args[n];
}

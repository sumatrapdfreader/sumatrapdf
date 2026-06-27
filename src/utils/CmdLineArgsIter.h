/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool CouldBeArg(Str s);

void ParseCmdLine(const WCHAR* cmdLine, StrVec& argsOut);
void ParseCmdLine(Str cmdLine, StrVec& argsOut);
TempStr QuoteCmdLineArgTemp(char* arg);

struct CmdLineArgsIter {
    StrVec args;
    int curr = 0;
    int nArgs = 0;
    Str currArg = {};

    explicit CmdLineArgsIter(const WCHAR* cmdLine);
    ~CmdLineArgsIter() = default;

    Str NextArg();
    Str EatParam();
    void RewindParam();
    Str AdditionalParam(int n) const;

    char* at(int) const;
    TempStr ParamsTemp();
};
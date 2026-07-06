/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool CouldBeArg(Str s);

#if OS_WIN
void ParseCmdLine(WStr cmdLine, StrVec& argsOut);
void ParseCmdLine(Str cmdLine, StrVec& argsOut);
#endif
void BuildCmdLineArgs(int argc, char** argv, StrVec& argsOut);
TempStr QuoteCmdLineArgTemp(Str arg);

struct CmdLineArgsIter {
    StrVec args;
    int curr = 0;
    int nArgs = 0;
    Str currArg = {};

#if OS_WIN
    explicit CmdLineArgsIter(WStr cmdLine);
#endif
    CmdLineArgsIter(int argc, char** argv);
    ~CmdLineArgsIter() = default;

    Str NextArg();
    Str EatParam();
    void RewindParam();
    Str AdditionalParam(int n) const;

    Str at(int) const;
    TempStr ParamsTemp();
};

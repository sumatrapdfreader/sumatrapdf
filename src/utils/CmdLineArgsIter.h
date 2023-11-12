/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool CouldBeArg(const char*);

int ParseCmdLine(const WCHAR* cmdLine, StrVec& argsOut);
TempStr QuoteCmdLineArgTemp(char* arg);

struct CmdLineArgsIter {
    StrVec args;
    int curr = 1; // first argument is exe path, which we skip
    int nArgs = 0;
    const char* currArg = nullptr;

    explicit CmdLineArgsIter(const WCHAR* cmdLine);
    ~CmdLineArgsIter();

    const char* NextArg();
    const char* EatParam();
    void RewindParam();
    const char* AdditionalParam(int n) const;

    char* at(int) const;
    char* ParamsTemp();
};

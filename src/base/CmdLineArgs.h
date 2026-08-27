/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool CouldBeArg(Str s);

#if OS_WIN
StrNode* ParseCmdLine(WStr cmdLine);
StrNode* ParseCmdLine(Str cmdLine);
#endif
TempStr QuoteCmdLineArgTemp(Str arg);

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool CouldBeArg(Str s);

StrNode* ParseCmdLine(WStr cmdLine);
StrNode* ParseCmdLine(Str cmdLine);
TempStr QuoteCmdLineArgTemp(Str arg);

/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"

#include "base/CmdLineArgs.h"

StrNode* ParseCmdLine(WStr cmdLine) {
    StrNode* root = nullptr;
    StrNode* tail = nullptr;
    int nArgs;
    WCHAR** argsArr = CommandLineToArgvW(CWStrTemp(cmdLine), &nArgs);
    for (int i = 0; i < nArgs; i++) {
        TempStr arg = ToUtf8Temp(argsArr[i]);
        if (len(arg) == 0) {
            continue;
        }
        StrNode* node = AllocStrNode(nullptr, arg);
        if (!root) {
            root = node;
        } else {
            tail->next = node;
        }
        tail = node;
    }
    LocalFree((void*)argsArr);
    return root;
}

StrNode* ParseCmdLine(Str cmdLine) {
    TempWStr s = ToWStrTemp(cmdLine);
    return ParseCmdLine(s);
}

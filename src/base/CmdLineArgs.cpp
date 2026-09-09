/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#if OS_WIN
#include "base/File.h"
#endif

#include "base/CmdLineArgs.h"

TempStr QuoteCmdLineArgTemp(Str arg) {
    if (!arg.s) {
        return {};
    }

    char resScratch[1024]{};
    str::Builder res;
    str::BuilderUseExternalBuffer(res, Str(resScratch, sizeof(resScratch)));
    res.AppendChar('"');
    int n = arg.len;
    int i = 0;
    while (i < n) {
        int nBackslashes = 0;
        while (i < n && arg.s[i] == '\\') {
            nBackslashes++;
            i++;
        }
        if (i >= n) {
            for (int k = 0; k < nBackslashes * 2; k++) {
                res.AppendChar('\\');
            }
            break;
        }
        if (arg.s[i] == '"') {
            for (int k = 0; k < (nBackslashes * 2) + 1; k++) {
                res.AppendChar('\\');
            }
            res.AppendChar('"');
            i++;
        } else {
            for (int k = 0; k < nBackslashes; k++) {
                res.AppendChar('\\');
            }
            res.AppendChar(arg.s[i]);
            i++;
        }
    }
    res.AppendChar('"');
    return ToStrTemp(res);
}

bool CouldBeArg(Str s) {
    if (len(s) == 0) {
        return false;
    }
    char c = *s.s;
    return (c == '-') || (c == '/');
}

#if OS_WIN

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

#endif

/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#ifndef DbgHelpDyn_h
#define DbgHelpDyn_h

#include "BaseUtil.h"
#include <dbghelp.h>
#include <tlhelp32.h>
#include "Vec.h"

// We're not using DebugLog.[h|cpp] here to make sure logging doesn't allocate
// memory. We use plog because it's similar to plogf() but we don't want to lie
// by claiming we support formatted strings.
// We always log those because they only kick in on error code paths
#define plog(msg) \
    OutputDebugStringA(msg)

#if 0 // 1 for more detailed debugging of crash handler progress
#define logdetail(msg) \
    OutputDebugStringA(msg)
#else
#define logdetail(msg) NoOp()
#endif

namespace dbghelp 
{

bool Load();
bool Initialize(const WCHAR *symPathW, const char *symPathA);
bool HasSymbols();
void WriteMiniDump(const TCHAR *crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump);
void GetCallstack(str::Str<char>& s, CONTEXT& ctx, HANDLE hThread);
void SymCleanup();
void GetExceptionInfo(str::Str<char>& s, EXCEPTION_POINTERS *excPointers);

}

#endif

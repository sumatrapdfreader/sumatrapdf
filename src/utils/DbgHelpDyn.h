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

namespace dbghelp 
{

bool Load();
bool Initialize(const WCHAR *symPathW, bool force = false);
bool HasSymbols();
void WriteMiniDump(const TCHAR *crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump);
void GetThreadCallstack(str::Str<char>& s, DWORD threadId);
bool GetCurrentThreadCallstack(str::Str<char>& s);
void LogCallstack();
void GetAllThreadsCallstacks(str::Str<char>& s);
void GetExceptionInfo(str::Str<char>& s, EXCEPTION_POINTERS *excPointers);

}

#endif

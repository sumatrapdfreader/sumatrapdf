/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

// dbghelp.h is included here so that warning C4091 can be disabled in a single location
#pragma warning(push)
#pragma warning(disable: 4091) // VS2015: 'typedef ': ignored on left of '' when no variable is declared
#include <dbghelp.h>
#pragma warning(pop)

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
void WriteMiniDump(const WCHAR *crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump);
void GetThreadCallstack(str::Str<char>& s, DWORD threadId);
bool GetCurrentThreadCallstack(str::Str<char>& s);
void LogCallstack();
void RememberCallstackLogs();
void FreeCallstackLogs();
char *GetCallstacks();
void GetAllThreadsCallstacks(str::Str<char>& s);
void GetExceptionInfo(str::Str<char>& s, EXCEPTION_POINTERS *excPointers);

}

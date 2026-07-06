/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

namespace dbghelp {

bool Initialize(WStr symPath, bool force);
bool HasSymbols();
void GetAddressInfo(str::Builder& s, DWORD64 addr, bool compact);
void WriteMiniDump(WStr crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump);
void GetThreadCallstack(str::Builder& s, ThreadId threadId);
bool GetCurrentThreadCallstack(str::Builder& s);
void LogCallstack();
void RememberCallstackLogs();
TempStr GetCurrentThreadCallstackTemp();
void FreeCallstackLogs();
Str GetCallstacks();
void GetAllThreadsCallstacks(str::Builder& s);
void GetAllThreadsCallstacksExcept(str::Builder& s, ThreadId skipThreadId);
void GetExceptionInfo(str::Builder& s, EXCEPTION_POINTERS* excPointers);

} // namespace dbghelp

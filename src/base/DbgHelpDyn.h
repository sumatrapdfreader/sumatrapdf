/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

namespace dbghelp {

bool Initialize(WStr symPath, bool force);
bool HasSymbols();
void GetAddressInfo(Arena* a, str::Builder& s, DWORD64 addr, bool compact);
void WriteMiniDump(WStr crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump);
void GetThreadCallstack(Arena* a, str::Builder& s, ThreadId threadId);
int GetSuspendedThreadCallstackAddrs(ThreadHandle hThread, u64* addrs, int maxAddrs);
bool GetCurrentThreadCallstack(Arena* a, str::Builder& s);
void LogCallstack();
void RememberCallstackLogs();
TempStr GetCurrentThreadCallstackTemp();
void FreeCallstackLogs();
Str GetCallstacks();
void GetAllThreadsCallstacks(Arena* a, str::Builder& s);
void GetAllThreadsCallstacksExcept(Arena* a, str::Builder& s, ThreadId skipThreadId);
void GetExceptionInfo(Arena* a, str::Builder& s, EXCEPTION_POINTERS* excPointers);

} // namespace dbghelp

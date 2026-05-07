/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

namespace dbghelp {

bool Initialize(const WCHAR* symPath, bool force);
bool HasSymbols();
void GetAddressInfo(StrBuilder& s, DWORD64 addr, bool compact);
void WriteMiniDump(const WCHAR* crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump);
void GetThreadCallstack(StrBuilder& s, DWORD threadId);
bool GetCurrentThreadCallstack(StrBuilder& s);
void LogCallstack();
void RememberCallstackLogs();
TempStr GetCurrentThreadCallstackTemp();
void FreeCallstackLogs();
ByteSlice GetCallstacks();
void GetAllThreadsCallstacks(StrBuilder& s);
void GetExceptionInfo(StrBuilder& s, EXCEPTION_POINTERS* excPointers);

} // namespace dbghelp

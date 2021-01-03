/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

namespace dbghelp {

bool Initialize(const WCHAR* symPath, bool force);
bool HasSymbols();
void GetAddressInfo(str::Str& s, DWORD64 addr, bool compact);
void WriteMiniDump(const WCHAR* crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump);
void GetThreadCallstack(str::Str& s, DWORD threadId);
bool GetCurrentThreadCallstack(str::Str& s);
void LogCallstack();
void RememberCallstackLogs();
void FreeCallstackLogs();
std::span<u8> GetCallstacks();
void GetAllThreadsCallstacks(str::Str& s);
void GetExceptionInfo(str::Str& s, EXCEPTION_POINTERS* excPointers);

} // namespace dbghelp

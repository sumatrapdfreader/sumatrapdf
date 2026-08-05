/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "base/Base.h"
#include "base/DbgHelpDyn.h"

namespace dbghelp {

bool Initialize(WStr /*symPath*/, bool /*force*/) {
    return true;
}

bool HasSymbols() {
    return false;
}

void GetAddressInfo(str::Builder& s, DWORD64 addr, bool /*compact*/) {
    void* p = reinterpret_cast<void*>((uintptr_t)addr);
    s.Append(fmt("%p\n", p));
}

void WriteMiniDump(WStr /*crashDumpFilePath*/, MINIDUMP_EXCEPTION_INFORMATION* /*mei*/, bool /*fullDump*/) {}

void GetThreadCallstack(str::Builder& /*s*/, ThreadId /*threadId*/) {}

int GetSuspendedThreadCallstackAddrs(ThreadHandle /*hThread*/, u64* /*addrs*/, int /*maxAddrs*/) {
    return 0;
}

bool GetCurrentThreadCallstack(str::Builder& /*s*/) {
    return false;
}

void LogCallstack() {}

void RememberCallstackLogs() {}

TempStr GetCurrentThreadCallstackTemp() {
    return "";
}

void FreeCallstackLogs() {}

Str GetCallstacks() {
    return {};
}

void GetAllThreadsCallstacks(str::Builder& /*s*/) {}

void GetAllThreadsCallstacksExcept(str::Builder& /*s*/, ThreadId /*skipThreadId*/) {}

void GetExceptionInfo(str::Builder& /*s*/, EXCEPTION_POINTERS* /*excPointers*/) {}

} // namespace dbghelp

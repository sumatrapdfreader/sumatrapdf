/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "base/Base.h"
#include "base/DbgHelpDyn.h"

namespace dbghelp {

bool Initialize(WStr, bool) {
    return true;
}

bool HasSymbols() {
    return false;
}

void GetAddressInfo(str::Builder& s, DWORD64 addr, bool) {
    void* p = reinterpret_cast<void*>((uintptr_t)addr);
    s.Append(fmt("%p\n", p));
}

void WriteMiniDump(WStr, MINIDUMP_EXCEPTION_INFORMATION*, bool) {}

void GetThreadCallstack(str::Builder&, ThreadId) {}

bool GetCurrentThreadCallstack(str::Builder&) {
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

void GetAllThreadsCallstacks(str::Builder&) {}

void GetAllThreadsCallstacksExcept(str::Builder&, ThreadId) {}

void GetExceptionInfo(str::Builder&, EXCEPTION_POINTERS*) {}

} // namespace dbghelp

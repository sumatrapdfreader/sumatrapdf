/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

/* Wrappers around dbghelp.dll that load it on demand and provide
   utility functions related to debugging like getting callstacs etc.
   This module is carefully written to not allocate memory as it
   can be used from crash handler.
*/

#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/DbgHelpDyn.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

/* Hard won wisdom: changing symbol path with SymSetSearchPath() after modules
   have been loaded (invideProcess=TRUE in SymInitialize() or SymRefreshModuleList())
   doesn't work.
   I had to provide symbol path in SymInitialize() (and either invideProcess=TRUE
   or invideProcess=FALSE and call SymRefreshModuleList()). There's probably
   a way to force it, but I'm happy I found a way that works.
*/

namespace dbghelp {

static char* ExceptionNameFromCode(DWORD excCode) {
#define EXC(x)          \
    case EXCEPTION_##x: \
        return #x;

    switch (excCode) {
        EXC(ACCESS_VIOLATION)
        EXC(DATATYPE_MISALIGNMENT)
        EXC(BREAKPOINT)
        EXC(SINGLE_STEP)
        EXC(ARRAY_BOUNDS_EXCEEDED)
        EXC(FLT_DENORMAL_OPERAND)
        EXC(FLT_DIVIDE_BY_ZERO)
        EXC(FLT_INEXACT_RESULT)
        EXC(FLT_INVALID_OPERATION)
        EXC(FLT_OVERFLOW)
        EXC(FLT_STACK_CHECK)
        EXC(FLT_UNDERFLOW)
        EXC(INT_DIVIDE_BY_ZERO)
        EXC(INT_OVERFLOW)
        EXC(PRIV_INSTRUCTION)
        EXC(IN_PAGE_ERROR)
        EXC(ILLEGAL_INSTRUCTION)
        EXC(NONCONTINUABLE_EXCEPTION)
        EXC(STACK_OVERFLOW)
        EXC(INVALID_DISPOSITION)
        EXC(GUARD_PAGE)
        EXC(INVALID_HANDLE)
    }
#undef EXC

    static char buf[512] = {0};

    FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE, GetModuleHandleA("ntdll.dll"), excCode,
                   0, buf, sizeof(buf), 0);

    return buf;
}

#if 0
static bool SetupSymbolPath()
{
    if (!DynSymSetSearchPathW && !DynSymSetSearchPath) {
        plog("SetupSymbolPath(): DynSymSetSearchPathW and DynSymSetSearchPath missing");
        return false;
    }

    AutoFreeWstr path(GetSymbolPath());
    if (!path) {
        plog("SetupSymbolPath(): GetSymbolPath() returned nullptr");
        return false;
    }

    BOOL ok = FALSE;
    AutoFreeWstr tpath(strconv::FromWStr(path));
    if (DynSymSetSearchPathW) {
        ok = DynSymSetSearchPathW(GetCurrentProcess(), path);
        if (!ok)
            plog("DynSymSetSearchPathW() failed");
    } else {
        AutoFree tmp(strconv::ToAnsi(tpath));
        ok = DynSymSetSearchPath(GetCurrentProcess(), tmp);
        if (!ok)
            plog("DynSymSetSearchPath() failed");
    }

    DynSymRefreshModuleList(GetCurrentProcess());
    return ok;
}
#endif

static BOOL gSymInitializeOk = FALSE;

static bool CanStackWalk() {
    bool ok = DynSymCleanup && DynSymGetOptions && DynSymSetOptions && DynStackWalk64 && DynSymFunctionTableAccess64 &&
              DynSymGetModuleBase64 && DynSymFromAddr;
    // if (!ok)
    //    plog("dbghelp::CanStackWalk(): no");
    return ok;
}

// check if has access to valid .pdb symbols file by trying to resolve a symbol
__declspec(noinline) bool CanSymbolizeAddress(DWORD64 addr) {
    static const int MAX_SYM_LEN = 512;

    char buf[sizeof(SYMBOL_INFO) + MAX_SYM_LEN * sizeof(char)];
    SYMBOL_INFO* symInfo = (SYMBOL_INFO*)buf;

    memset(buf, 0, sizeof(buf));
    symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symInfo->MaxNameLen = MAX_SYM_LEN;

    DWORD64 symDisp = 0;
    BOOL ok = DynSymFromAddr(GetCurrentProcess(), addr, &symDisp, symInfo);
    return ok && symInfo->Name[0];
}

// a heuristic to test if we have symbols for our own binaries by testing if
// we can get symbol for any of our functions.
bool HasSymbols() {
    DWORD64 addr = (DWORD64)&CanSymbolizeAddress;
    return CanSymbolizeAddress(addr);
}

// Load and initialize dbghelp.dll. Returns false if failed.
// To simplify callers, it can be called multiple times - it only does the
// work the first time, unless force is true, in which case we re-initialize
// the library (needed in crash dump where we re-initialize dbghelp.dll after
// downloading symbols)
bool Initialize(const WCHAR* symPathW, bool force) {
    if (gSymInitializeOk && !force)
        return true;

    bool needsCleanup = gSymInitializeOk;

    if (!DynSymInitializeW && !DynSymInitialize) {
        // plog("dbghelp::Initialize(): SymInitializeW() and SymInitialize() not present in dbghelp.dll");
        return false;
    }

    if (needsCleanup)
        DynSymCleanup(GetCurrentProcess());

    if (DynSymInitializeW) {
        gSymInitializeOk = DynSymInitializeW(GetCurrentProcess(), symPathW, TRUE);
    } else {
        // SymInitializeW() is not present on some XP systems
        char symPathA[MAX_PATH];
        if (0 != strconv::ToCodePageBuf(symPathA, dimof(symPathA), symPathW, CP_ACP))
            gSymInitializeOk = DynSymInitialize(GetCurrentProcess(), symPathA, TRUE);
    }

    if (!gSymInitializeOk) {
        // plog("dbghelp::Initialize(): _SymInitialize() failed");
        return false;
    }

    DWORD symOptions = DynSymGetOptions();
    symOptions |= (SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS; // don't show system msg box on errors
    DynSymSetOptions(symOptions);

    // SetupSymbolPath();
    return true;
}

static BOOL CALLBACK OpenMiniDumpCallback(void* param, PMINIDUMP_CALLBACK_INPUT input,
                                          PMINIDUMP_CALLBACK_OUTPUT output) {
    UNUSED(param);
    if (!input || !output)
        return FALSE;

    switch (input->CallbackType) {
        case ModuleCallback:
            if (!(output->ModuleWriteFlags & ModuleReferencedByMemory))
                output->ModuleWriteFlags &= ~ModuleWriteModule;
            return TRUE;
        case IncludeModuleCallback:
        case IncludeThreadCallback:
        case ThreadCallback:
        case ThreadExCallback:
            return TRUE;
        default:
            return FALSE;
    }
}

void WriteMiniDump(const WCHAR* crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump) {
    if (!Initialize(nullptr, false) || !DynMiniDumpWriteDump)
        return;

    HANDLE hFile = CreateFile(crashDumpFilePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (INVALID_HANDLE_VALUE == hFile)
        return;

    MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory);
    if (fullDump)
        type =
            (MINIDUMP_TYPE)(type | MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithPrivateReadWriteMemory);
    MINIDUMP_CALLBACK_INFORMATION mci = {OpenMiniDumpCallback, nullptr};

    DynMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, type, mei, nullptr, &mci);

    CloseHandle(hFile);
}

static bool GetAddrInfo(void* addr, char* module, DWORD moduleLen, DWORD& sectionOut, DWORD_PTR& offsetOut) {
    MEMORY_BASIC_INFORMATION mbi;
    if (0 == VirtualQuery(addr, &mbi, sizeof(mbi)))
        return false;

    HMODULE hMod = (HMODULE)mbi.AllocationBase;
    if (0 == hMod)
        return false;

    if (!GetModuleFileNameA(hMod, module, moduleLen))
        return false;
    module[moduleLen - 1] = '\0';

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)mbi.AllocationBase;
    PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((BYTE*)dosHeader + dosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(pNtHeader);

    DWORD_PTR lAddr = (DWORD_PTR)addr - (DWORD_PTR)hMod;
    for (unsigned int i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++) {
        DWORD_PTR startAddr = section->VirtualAddress;
        DWORD_PTR endAddr = startAddr;
        if (section->SizeOfRawData > section->Misc.VirtualSize)
            endAddr += section->SizeOfRawData;
        else
            section->Misc.VirtualSize;

        if (lAddr >= startAddr && lAddr <= endAddr) {
            sectionOut = i + 1;
            offsetOut = lAddr - startAddr;
            return true;
        }
        section++;
    }
    return false;
}

static void AppendAddress(str::Str& s, DWORD64 addr) {
    void* p = reinterpret_cast<void*>(addr);
    s.AppendFmt("%p", p);
}

static void GetAddressInfo(str::Str& s, DWORD64 addr) {
    static const int MAX_SYM_LEN = 512;

    char buf[sizeof(SYMBOL_INFO) + MAX_SYM_LEN * sizeof(char)];
    SYMBOL_INFO* symInfo = (SYMBOL_INFO*)buf;

    memset(buf, 0, sizeof(buf));
    symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symInfo->MaxNameLen = MAX_SYM_LEN;

    DWORD64 symDisp = 0;
    char* symName = nullptr;
    BOOL ok = DynSymFromAddr(GetCurrentProcess(), addr, &symDisp, symInfo);
    if (ok)
        symName = &(symInfo->Name[0]);

    char module[MAX_PATH] = {0};
    DWORD section;
    DWORD_PTR offset;
    if (GetAddrInfo((void*)addr, module, sizeof(module), section, offset)) {
        str::ToLowerInPlace(module);
        const char* moduleShort = path::GetBaseNameNoFree(module);
        AppendAddress(s, addr);
        s.AppendFmt(" %02X:", section);
        AppendAddress(s, offset);
        s.AppendFmt(" %s", moduleShort);

        if (symName) {
            s.AppendFmt("!%s+0x%x", symName, (int)symDisp);
        } else if (symDisp != 0) {
            s.AppendFmt("+0x%x", (int)symDisp);
        }
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD disp;
        if (DynSymGetLineFromAddr64(GetCurrentProcess(), addr, &disp, &line)) {
            s.AppendFmt(" %s+%d", line.FileName, line.LineNumber);
        }
    } else {
        AppendAddress(s, addr);
    }
    s.Append("\r\n");
}

static bool GetStackFrameInfo(str::Str& s, STACKFRAME64* stackFrame, CONTEXT* ctx, HANDLE hThread) {
#if defined(_WIN64)
    int machineType = IMAGE_FILE_MACHINE_AMD64;
#else
    int machineType = IMAGE_FILE_MACHINE_I386;
#endif
    BOOL ok = DynStackWalk64(machineType, GetCurrentProcess(), hThread, stackFrame, ctx, nullptr,
                             DynSymFunctionTableAccess64, DynSymGetModuleBase64, nullptr);
    if (!ok)
        return false;

    DWORD64 addr = stackFrame->AddrPC.Offset;
    if (0 == addr)
        return true;
    if (addr == stackFrame->AddrReturn.Offset) {
        s.Append("GetStackFrameInfo(): addr == stackFrame->AddrReturn.Offset");
        return false;
    }

    GetAddressInfo(s, addr);
    return true;
}

static bool GetCallstack(str::Str& s, CONTEXT& ctx, HANDLE hThread) {
    if (!CanStackWalk()) {
        s.Append("GetCallstack(): CanStackWalk() returned false");
        return false;
    }

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(stackFrame));
#ifdef _WIN64
    stackFrame.AddrPC.Offset = ctx.Rip;
    stackFrame.AddrFrame.Offset = ctx.Rbp;
    stackFrame.AddrStack.Offset = ctx.Rsp;
#else
    stackFrame.AddrPC.Offset = ctx.Eip;
    stackFrame.AddrFrame.Offset = ctx.Ebp;
    stackFrame.AddrStack.Offset = ctx.Esp;
#endif
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    int framesCount = 0;
    static const int maxFrames = 32;
    while (framesCount < maxFrames) {
        if (!GetStackFrameInfo(s, &stackFrame, &ctx, hThread))
            break;
        framesCount++;
    }
    if (0 == framesCount) {
        s.Append("StackWalk64() couldn't get even the first stack frame info");
        return false;
    }
    return true;
}

void GetThreadCallstack(str::Str& s, DWORD threadId) {
    if (threadId == GetCurrentThreadId())
        return;

    s.AppendFmt("\r\nThread: %x\r\n", threadId);

    DWORD access = THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME;
    HANDLE hThread = OpenThread(access, false, threadId);
    if (!hThread) {
        s.Append("Failed to OpenThread()\r\n");
        return;
    }

    DWORD res = SuspendThread(hThread);
    if (-1 == res) {
        s.Append("Failed to SuspendThread()\r\n");
    } else {
        CONTEXT ctx = {0};
        ctx.ContextFlags = CONTEXT_FULL;
        BOOL ok = GetThreadContext(hThread, &ctx);
        if (ok)
            GetCallstack(s, ctx, hThread);
        else
            s.Append("Failed to GetThreadContext()\r\n");

        ResumeThread(hThread);
    }
    CloseHandle(hThread);
}

// we disable optimizations for this function as it calls RtlCaptureContext()
// which cannot deal with Omit Frame Pointers optimization (/Oy explicitly, turned
// implicitly by e.g. /O2)
// http://www.bytetalk.net/2011/06/why-rtlcapturecontext-crashes-on.html
#pragma optimize("", off)
// we also need to disable warning 4748 "/GS can not protect parameters and local variables
// from local buffer overrun because optimizations are disabled in function)"
#pragma warning(push)
#pragma warning(disable : 4748)
__declspec(noinline) bool GetCurrentThreadCallstack(str::Str& s) {
    // not available under Win2000
    if (!DynRtlCaptureContext)
        return false;

    if (!Initialize(nullptr, false))
        return false;

    CONTEXT ctx;
    DynRtlCaptureContext(&ctx);
    return GetCallstack(s, ctx, GetCurrentThread());
}
#pragma optimize("", off)

str::Str* gCallstackLogs = nullptr;

// start remembering callstack logs done with LogCallstack()
void RememberCallstackLogs() {
    CrashIf(gCallstackLogs);
    gCallstackLogs = new str::Str();
}

void FreeCallstackLogs() {
    delete gCallstackLogs;
    gCallstackLogs = nullptr;
}

char* GetCallstacks() {
    if (!gCallstackLogs)
        return nullptr;
    return str::Dup(gCallstackLogs->Get());
}

void LogCallstack() {
    str::Str s(2048);
    if (!GetCurrentThreadCallstack(s)) {
        return;
    }

    s.Append("\n");
    if (gCallstackLogs) {
        gCallstackLogs->Append(s.Get());
    }
}

void GetAllThreadsCallstacks(str::Str& s) {
    HANDLE threadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (threadSnap == INVALID_HANDLE_VALUE)
        return;

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    DWORD pid = GetCurrentProcessId();
    BOOL ok = Thread32First(threadSnap, &te32);
    while (ok) {
        if (te32.th32OwnerProcessID == pid)
            GetThreadCallstack(s, te32.th32ThreadID);
        ok = Thread32Next(threadSnap, &te32);
    }

    CloseHandle(threadSnap);
}
#pragma warning(pop)

void GetExceptionInfo(str::Str& s, EXCEPTION_POINTERS* excPointers) {
    if (!excPointers)
        return;
    EXCEPTION_RECORD* excRecord = excPointers->ExceptionRecord;
    DWORD excCode = excRecord->ExceptionCode;
    s.AppendFmt("Exception: %08X %s\r\n", (int)excCode, ExceptionNameFromCode(excCode));

    s.AppendFmt("Faulting IP: ");
    GetAddressInfo(s, (DWORD64)excRecord->ExceptionAddress);
    if ((EXCEPTION_ACCESS_VIOLATION == excCode) || (EXCEPTION_IN_PAGE_ERROR == excCode)) {
        int readWriteFlag = (int)excRecord->ExceptionInformation[0];
        DWORD64 dataVirtAddr = (DWORD64)excRecord->ExceptionInformation[1];
        if (0 == readWriteFlag) {
            s.Append("Fault reading address ");
            AppendAddress(s, dataVirtAddr);
        } else if (1 == readWriteFlag) {
            s.Append("Fault writing address ");
            AppendAddress(s, dataVirtAddr);
        } else if (8 == readWriteFlag) {
            s.Append("DEP violation at address ");
            AppendAddress(s, dataVirtAddr);
        } else {
            s.Append("unknown readWriteFlag: %d", readWriteFlag);
        }
        s.Append("\r\n");
    }

    PCONTEXT ctx = excPointers->ContextRecord;
    s.AppendFmt("\r\nRegisters:\r\n");

#ifdef _WIN64
    s.AppendFmt(
        "RAX:%016I64X  RBX:%016I64X  RCX:%016I64X\r\nRDX:%016I64X  RSI:%016I64X  RDI:%016I64X\r\n"
        "R8: %016I64X\r\nR9: "
        "%016I64X\r\nR10:%016I64X\r\nR11:%016I64X\r\nR12:%016I64X\r\nR13:%016I64X\r\nR14:%016I64X\r\nR15:%016I64X\r\n",
        ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx, ctx->Rsi, ctx->Rdi, ctx->R9, ctx->R10, ctx->R11, ctx->R12, ctx->R13,
        ctx->R14, ctx->R15);
    s.AppendFmt("CS:RIP:%04X:%016I64X\r\n", ctx->SegCs, ctx->Rip);
    s.AppendFmt("SS:RSP:%04X:%016X  RBP:%08X\r\n", ctx->SegSs, ctx->Rsp, ctx->Rbp);
    s.AppendFmt("DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n", ctx->SegDs, ctx->SegEs, ctx->SegFs, ctx->SegGs);
    s.AppendFmt("Flags:%08X\r\n", ctx->EFlags);
#else
    s.AppendFmt("EAX:%08X  EBX:%08X  ECX:%08X\r\nEDX:%08X  ESI:%08X  EDI:%08X\r\n", ctx->Eax, ctx->Ebx, ctx->Ecx,
                ctx->Edx, ctx->Esi, ctx->Edi);
    s.AppendFmt("CS:EIP:%04X:%08X\r\n", ctx->SegCs, ctx->Eip);
    s.AppendFmt("SS:ESP:%04X:%08X  EBP:%08X\r\n", ctx->SegSs, ctx->Esp, ctx->Ebp);
    s.AppendFmt("DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n", ctx->SegDs, ctx->SegEs, ctx->SegFs, ctx->SegGs);
    s.AppendFmt("Flags:%08X\r\n", ctx->EFlags);
#endif

    s.Append("\r\nCrashed thread:\r\n");
    // it's not really for current thread, but it seems to work
    GetCallstack(s, *ctx, GetCurrentThread());
}

} // namespace dbghelp

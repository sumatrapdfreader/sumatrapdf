/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

/* Wrappers around dbghelp.dll that load it on demand and provide
   utility functions related to debugging like getting callstacs etc. */

#include "DbgHelpDyn.h"
#include "FileUtil.h"
#include "WinUtil.h"

/* Hard won wisdom: changing symbol path with SymSetSearchPath() after modules
have been loaded (invideProcess=TRUE in SymInitialize() or SymRefreshModuleList())
doesn't work.
I had to provide symbol path in SymInitialize() (and either invideProcess=TRUE
or invideProcess=FALSE and call SymRefreshModuleList()). There's probably
a way to force it, but I'm happy I found a way that works. */

typedef BOOL WINAPI MiniDumpWriteDumpProc(
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    LONG DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

typedef BOOL _stdcall SymInitializeWProc(
    HANDLE hProcess,
    PCWSTR UserSearchPath,
    BOOL fInvadeProcess);

typedef BOOL _stdcall SymInitializeProc(
    HANDLE hProcess,
    PCSTR UserSearchPath,
    BOOL fInvadeProcess);

typedef BOOL _stdcall SymCleanupProc(
  HANDLE hProcess);

typedef DWORD _stdcall SymGetOptionsProc();
typedef DWORD _stdcall SymSetOptionsProc(DWORD SymOptions);

typedef BOOL _stdcall StackWalk64Proc(
    DWORD MachineType,
    HANDLE hProcess,
    HANDLE hThread,
    LPSTACKFRAME64 StackFrame,
    PVOID ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);

typedef BOOL _stdcall SymFromAddrProc(
    HANDLE hProcess,
    DWORD64 Address,
    PDWORD64 Displacement,
    PSYMBOL_INFO Symbol);

typedef PVOID _stdcall SymFunctionTableAccess64Proc(
    HANDLE hProcess,
    DWORD64 AddrBase);

typedef DWORD64 _stdcall SymGetModuleBase64Proc(
    HANDLE hProcess,
    DWORD64 qwAddr);

typedef BOOL _stdcall SymGetSearchPathWProc(
    HANDLE hProcess,
    PWSTR SearchPath,
    DWORD SearchPathLength);

typedef BOOL _stdcall SymSetSearchPathWProc(
    HANDLE hProcess,
    PCWSTR SearchPath);

typedef BOOL _stdcall SymSetSearchPathProc(
    HANDLE hProcess,
    PCSTR SearchPath);

typedef BOOL _stdcall SymRefreshModuleListProc(
  HANDLE hProcess);

typedef BOOL _stdcall SymGetLineFromAddr64Proc(
    HANDLE hProcess,
    DWORD64 dwAddr,
    PDWORD pdwDisplacement,
    PIMAGEHLP_LINE64 Line);

namespace dbghelp {

static MiniDumpWriteDumpProc *          _MiniDumpWriteDump;
static SymInitializeWProc *             _SymInitializeW;
static SymInitializeProc *              _SymInitialize;
static SymCleanupProc *                 _SymCleanup;
static SymGetOptionsProc *              _SymGetOptions;
static SymSetOptionsProc *              _SymSetOptions;
static SymGetSearchPathWProc *          _SymGetSearchPathW;
static SymSetSearchPathWProc *          _SymSetSearchPathW;
static SymSetSearchPathProc *           _SymSetSearchPath;
static StackWalk64Proc   *              _StackWalk64;
static SymFunctionTableAccess64Proc *   _SymFunctionTableAccess64;
static SymGetModuleBase64Proc *         _SymGetModuleBase64;
static SymFromAddrProc *                _SymFromAddr;
static SymRefreshModuleListProc *       _SymRefreshModuleList;
static SymGetLineFromAddr64Proc *       _SymGetLineFromAddr64;
static BOOL     						gSymInitializeOk = FALSE;

static char *ExceptionNameFromCode(DWORD excCode)
{
#define EXC(x) case EXCEPTION_##x: return #x;

    switch (excCode)
    {
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

    static char buf[512] = { 0 };

    FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
        GetModuleHandleA("ntdll.dll"),
        excCode, 0, buf, sizeof(buf), 0);

    return buf;
}

// It only loads dbghelp.dll and gets its functions.
// It can (but doesn't have to) be called before Initialize().
bool Load()
{
    if (_MiniDumpWriteDump)
        return true;
#if 0
    TCHAR *dbghelpPath = _T("C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Team Tools\\Performance Tools\\dbghelp.dll");
    HMODULE h = LoadLibrary(dbghelpPath);
#else
    HMODULE h = SafeLoadLibrary(_T("DBGHELP.DLL"));
#endif
    if (!h)
        return false;

#define Load(func) _ ## func = (func ## Proc *)GetProcAddress(h, #func)
    Load(MiniDumpWriteDump);
    Load(SymInitializeW);
    Load(SymInitialize);
    Load(SymCleanup);
    Load(SymGetOptions);
    Load(SymSetOptions);
    Load(SymGetSearchPathW);
    Load(SymSetSearchPathW);
    Load(SymSetSearchPath);
    Load(StackWalk64);
    Load(SymFunctionTableAccess64);
    Load(SymGetModuleBase64);
    Load(SymFromAddr);
    Load(SymRefreshModuleList);
    Load(SymGetLineFromAddr64);
#undef Load

    return _StackWalk64 != NULL;
}

#if 0
static bool SetupSymbolPath()
{
    if (!_SymSetSearchPathW && !_SymSetSearchPath) {
        plog("SetupSymbolPath(): _SymSetSearchPathW and _SymSetSearchPath missing");
        return false;
    }

    ScopedMem<WCHAR> path(GetSymbolPath());
    if (!path) {
        plog("SetupSymbolPath(): GetSymbolPath() returned NULL");
        return false;
    }

    BOOL ok = FALSE;
    ScopedMem<TCHAR> tpath(str::conv::FromWStr(path));
    if (_SymSetSearchPathW) {
        ok = _SymSetSearchPathW(GetCurrentProcess(), path);
        if (!ok)
            plog("_SymSetSearchPathW() failed");
    } else {
        ScopedMem<char> tmp(str::conv::ToAnsi(tpath));
        ok = _SymSetSearchPath(GetCurrentProcess(), tmp);
        if (!ok)
            plog("_SymSetSearchPath() failed");
    }

    _SymRefreshModuleList(GetCurrentProcess());
    free((void*)path);
    return ok;
}
#endif

static bool CanStackWalk()
{
    return gSymInitializeOk && _SymCleanup && _SymGetOptions
        && _SymSetOptions&& _StackWalk64 && _SymFunctionTableAccess64
        && _SymGetModuleBase64 && _SymFromAddr;
}

static bool CanSymbolizeAddress(DWORD64 addr)
{
    static const int MAX_SYM_LEN = 512;

    char buf[sizeof(SYMBOL_INFO) + MAX_SYM_LEN * sizeof(char)];
    SYMBOL_INFO *symInfo = (SYMBOL_INFO*)buf;

    memset(buf, 0, sizeof(buf));
    symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symInfo->MaxNameLen = MAX_SYM_LEN;

    DWORD64 symDisp = 0;
    BOOL ok = _SymFromAddr(GetCurrentProcess(), addr, &symDisp, symInfo);
    return ok && symInfo->Name[0];
}

// a heuristic to test if we have symbols for our own binaries by testing if
// we can get symbol for any of our functions.
bool HasSymbols()
{
    DWORD64 addr = (DWORD64)&CanSymbolizeAddress;
    return CanSymbolizeAddress(addr);
}

// Load and initialize dbghelp.dll. Returns false if failed.
// To simplify callers, it can be called multiple times - it only does the
// work the first time.
// We need both symPathW and symPathA because on some XP version I've
// seen dbghelp.dll not have SymInitializeW().
// TODO: symPathW and symPathA are the same location so we should
// build symPathA out of symPathW, to simplify callers
// TODO: if symPathW/A are NULL, as a fallback we should use exe's directory
// (which is the right place when running from IDE during development)
bool Initialize(const WCHAR *symPathW, const char *symPathA)
{
    if (gSymInitializeOk)
        return true;

    if (!Load()) {
        plog("dbghelp::Initialize(): LoadDbgHelpFuncs() failed");
        return false;
    }

    if (!_SymInitializeW && !_SymInitialize) {
        plog("dbghelp::Initialize(): SymInitializeW() or SymInitializeA() not present in dbghelp.dll");
        return false;
    }

    if (_SymInitializeW) {
        gSymInitializeOk = _SymInitializeW(GetCurrentProcess(), symPathW, TRUE);
    } else if (symPathA) {
        gSymInitializeOk = _SymInitialize(GetCurrentProcess(), symPathA, TRUE);
    }

    if (!gSymInitializeOk) {
        plog("dbghelp::Initialize(): _SymInitialize() failed");
        return false;
    }

    DWORD symOptions = _SymGetOptions();
    symOptions = (SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS; // don't show system msg box on errors
    _SymSetOptions(symOptions);

    //SetupSymbolPath();
    return true;
}

static BOOL CALLBACK OpenMiniDumpCallback(void* /*param*/, PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output)
{
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

void WriteMiniDump(const TCHAR *crashDumpFilePath, MINIDUMP_EXCEPTION_INFORMATION* mei, bool fullDump)
{
    if (!Initialize(NULL, NULL) || !_MiniDumpWriteDump)
        return;

    HANDLE hFile = CreateFile(crashDumpFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (INVALID_HANDLE_VALUE == hFile)
        return;

    MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory);
    if (fullDump)
        type = (MINIDUMP_TYPE)(type | MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithPrivateReadWriteMemory);
    MINIDUMP_CALLBACK_INFORMATION mci = { OpenMiniDumpCallback, NULL };

    _MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, type, mei, NULL, &mci);

    CloseHandle(hFile);
}

// TODO: should offsetOut be DWORD_PTR for 64-bit compat?
static bool GetAddrInfo(void *addr, char *module, DWORD moduleLen, DWORD& sectionOut, DWORD& offsetOut)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(addr, &mbi, sizeof(mbi)))
        return false;

    DWORD hMod = (DWORD)mbi.AllocationBase;
    if (!GetModuleFileNameA((HMODULE)hMod, module, moduleLen))
        return false;

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hMod;
    PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)(hMod + dosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(pNtHeader);

    DWORD lAddr = (DWORD)addr - hMod;
    for (unsigned int i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++) {
        DWORD startAddr = section->VirtualAddress;
        DWORD endAddr = startAddr;
        if (section->SizeOfRawData > section->Misc.VirtualSize)
            endAddr += section->SizeOfRawData;
        else
            section->Misc.VirtualSize;

        if (lAddr >= startAddr && lAddr <= endAddr) {
            sectionOut = i+1;
            offsetOut = lAddr - startAddr;
            return true;
        }
        section++;
    }
    return false;
}

static void AppendAddress(str::Str<char>& s, DWORD64 addr)
{
#ifdef _WIN64
    s.AppendFmt("%016I64X", addr);
#else
    s.AppendFmt("%08X", (DWORD)addr);
#endif
}

static void GetAddressInfo(str::Str<char>& s, DWORD64 addr)
{
    static const int MAX_SYM_LEN = 512;

    char buf[sizeof(SYMBOL_INFO) + MAX_SYM_LEN * sizeof(char)];
    SYMBOL_INFO *symInfo = (SYMBOL_INFO*)buf;

    memset(buf, 0, sizeof(buf));
    symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symInfo->MaxNameLen = MAX_SYM_LEN;

    DWORD64 symDisp = 0;
    char *symName = NULL;
    BOOL ok = _SymFromAddr(GetCurrentProcess(), addr, &symDisp, symInfo);
    if (ok)
        symName = &(symInfo->Name[0]);

    char module[MAX_PATH] = { 0 };
    DWORD section, offset;
    if (GetAddrInfo((void*)addr, module, sizeof(module), section, offset)) {
        str::ToLower(module);
        const char *moduleShort = path::GetBaseName(module);
        AppendAddress(s, addr);
        s.AppendFmt(" %02X:", section);
        AppendAddress(s, offset);
        s.AppendFmt(" %s", moduleShort);

        if (symName)
            s.AppendFmt("!%s+0x%x", symName, (int)symDisp);
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD disp;
        if (_SymGetLineFromAddr64(GetCurrentProcess(), addr, &disp, &line)) {
            s.AppendFmt(" %s+%d", line.FileName, line.LineNumber);
        }
    } else {
        AppendAddress(s, addr);
    }
    s.Append("\r\n");
}

static bool GetStackFrameInfo(str::Str<char>& s, STACKFRAME64 *stackFrame,
                              CONTEXT *ctx, HANDLE hThread)
{
#if defined(_WIN64)
    int machineType = IMAGE_FILE_MACHINE_AMD64;
#else
    int machineType = IMAGE_FILE_MACHINE_I386;
#endif
    BOOL ok = _StackWalk64(machineType, GetCurrentProcess(), hThread,
        stackFrame, ctx, NULL, _SymFunctionTableAccess64,
        _SymGetModuleBase64, NULL);
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

void GetCallstack(str::Str<char>& s, CONTEXT& ctx, HANDLE hThread)
{
    if (!CanStackWalk()) {
        s.Append("GetCallstack(): CanStackWalk() returned false");
        return;
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
    while (framesCount < maxFrames)
    {
        if (!GetStackFrameInfo(s, &stackFrame, &ctx, hThread))
            break;
        framesCount++;
    }
    if (0 == framesCount)
        s.Append("StackWalk64() couldn't get even the first stack frame info");
}

void SymCleanup()
{
    _SymCleanup(GetCurrentProcess());
}

void GetExceptionInfo(str::Str<char>& s, EXCEPTION_POINTERS *excPointers)
{
    if (!excPointers)
        return;
    EXCEPTION_RECORD *excRecord = excPointers->ExceptionRecord;
    DWORD excCode = excRecord->ExceptionCode;
    s.AppendFmt("Exception: %08X %s\r\n", (int)excCode, ExceptionNameFromCode(excCode));

    s.AppendFmt("Faulting IP: ");
    GetAddressInfo(s, (DWORD64)excRecord->ExceptionAddress);
    if ((EXCEPTION_ACCESS_VIOLATION == excCode) ||
        (EXCEPTION_IN_PAGE_ERROR == excCode))
    {
        int readWriteFlag = (int)excRecord->ExceptionInformation[0];
        DWORD64 dataVirtAddr = (DWORD64)excRecord->ExceptionInformation[1];
        if (0 == readWriteFlag) {
            s.Append("Fault reading address "); AppendAddress(s, dataVirtAddr);
        } else if (1 == readWriteFlag) {
            s.Append("Fault writing address "); AppendAddress(s, dataVirtAddr);
        } else if (8 == readWriteFlag) {
            s.Append("DEP violation at address "); AppendAddress(s, dataVirtAddr);
        } else {
            s.Append("unknown readWriteFlag: %d", readWriteFlag);
        }
        s.Append("\r\n");
    }

    PCONTEXT ctx = excPointers->ContextRecord;
    s.AppendFmt("\r\nRegisters:\r\n");

#ifdef _WIN64
    s.AppendFmt("RAX:%016I64X  RBX:%016I64X  RCX:%016I64X\r\nRDX:%016I64X  RSI:%016I64X  RDI:%016I64X\r\n"
        "R8: %016I64X\r\nR9: %016I64X\r\nR10:%016I64X\r\nR11:%016I64X\r\nR12:%016I64X\r\nR13:%016I64X\r\nR14:%016I64X\r\nR15:%016I64X\r\n",
        ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx, ctx->Rsi, ctx->Rdi,
        ctx->R9,ctx->R10,ctx->R11,ctx->R12,ctx->R13,ctx->R14,ctx->R15);
    s.AppendFmt("CS:RIP:%04X:%016I64X\r\n", ctx->SegCs, ctx->Rip);
    s.AppendFmt("SS:RSP:%04X:%016X  RBP:%08X\r\n", ctx->SegSs, ctx->Rsp, ctx->Rbp);
    s.AppendFmt("DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n", ctx->SegDs, ctx->SegEs, ctx->SegFs, ctx->SegGs);
    s.AppendFmt("Flags:%08X\r\n", ctx->EFlags);
#else
    s.AppendFmt("EAX:%08X  EBX:%08X  ECX:%08X\r\nEDX:%08X  ESI:%08X  EDI:%08X\r\n",
        ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx, ctx->Esi, ctx->Edi);
    s.AppendFmt("CS:EIP:%04X:%08X\r\n", ctx->SegCs, ctx->Eip);
    s.AppendFmt("SS:ESP:%04X:%08X  EBP:%08X\r\n", ctx->SegSs, ctx->Esp, ctx->Ebp);
    s.AppendFmt("DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n", ctx->SegDs, ctx->SegEs, ctx->SegFs, ctx->SegGs);
    s.AppendFmt("Flags:%08X\r\n", ctx->EFlags);
#endif

    s.Append("\r\nCrashed thread:\r\n");
    // it's not really for current thread, but it seems to work
    GetCallstack(s, *ctx, GetCurrentThread());
}

}

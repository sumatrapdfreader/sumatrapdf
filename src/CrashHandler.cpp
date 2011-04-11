/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include "CrashHandler.h"

#include "Version.h"

#include "BaseUtil.h"
#include "StrUtil.h"
#include "Vec.h"
#include "WinUtil.h"
#include "FileUtil.h"
#include "AppTools.h"
#include "Http.h"
#include "ZipUtil.h"

#include "translations.h"

//#define DEBUG_CRASH_INFO

typedef BOOL WINAPI MiniDumpWriteProc(
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    LONG DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

typedef BOOL _stdcall SymInitializeProc(
    HANDLE hProcess,
    PCSTR UserSearchPath,
    BOOL fInvadeProcess);

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

typedef BOOL _stdcall SymSetSearchPathWProc(
    HANDLE hProcess,
    PCWSTR SearchPath);

typedef BOOL _stdcall SymSetSearchPathProc(
    HANDLE hProcess,
    PCSTR SearchPath);

typedef BOOL __stdcall SymGetLineFromAddr64Proc(
    HANDLE hProcess,
    DWORD64 dwAddr,
    PDWORD pdwDisplacement,
    PIMAGEHLP_LINE64 Line);

static MiniDumpWriteProc *              _MiniDumpWriteDump;
static SymInitializeProc *              _SymInitialize;
static SymGetOptionsProc *              _SymGetOptions;
static SymSetOptionsProc *              _SymSetOptions;
static SymSetSearchPathWProc *          _SymSetSearchPathW;
static SymSetSearchPathProc *           _SymSetSearchPath;
static StackWalk64Proc   *              _StackWalk64;
static SymFunctionTableAccess64Proc *   _SymFunctionTableAccess64;
static SymGetModuleBase64Proc *         _SymGetModuleBase64;
static SymFromAddrProc *                _SymFromAddr;
static SymGetLineFromAddr64Proc *       _SymGetLineFromAddr64;

static ScopedMem<TCHAR> gCrashDumpPath(NULL);
static HANDLE gDumpEvent = NULL;
static HANDLE gDumpThread = NULL;
static MINIDUMP_EXCEPTION_INFORMATION gMei = { 0 };
static BOOL gSymInitializeOk = FALSE;

#define F(X) \
    { #X, (void**)&_ ## X },

FuncNameAddr gDbgHelpFuncs[] = {
    F(MiniDumpWriteDump)
    F(SymInitialize)
    F(SymGetOptions)
    F(SymSetOptions)
    F(SymSetSearchPathW)
    F(SymSetSearchPath)
    F(StackWalk64)
    F(SymFunctionTableAccess64)
    F(SymGetModuleBase64)
    F(SymFromAddr)
    F(SymGetLineFromAddr64)
    { NULL, NULL }
};
#undef F

static bool LoadDbgHelpFuncs()
{
    if (_MiniDumpWriteDump)
        return true;
#if 0
    TCHAR *dbghelpPath = _T("C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Team Tools\\Performance Tools\\dbghelp.dll");
    if (File::Exists(dbghelpPath)) {
        HMODULE h = LoadLibrary(dbghelpPath);
        if (h) {
            LoadDllFuncs(h, gDbgHelpFuncs);
            return;
        }
    }
#endif
    LoadDllFuncs(_T("DBGHELP.DLL"), gDbgHelpFuncs);
    return _MiniDumpWriteDump != 0;
}

static bool GetEnvOk(DWORD ret, DWORD cchBufSize)
{
    return cchBufSize == ret;
}

static TCHAR *GetCrashDumpDir()
{
    TCHAR *dir = AppGenDataDir();
    if (!dir) return NULL;
    TCHAR *symDir = Path::Join(dir, _T("symbols"));
    free(dir);
    if (!Dir::Create(symDir)) {
        free(symDir);
        return NULL;
    }
    return symDir;
}

/* Setting symbol path:
add GetEnvironmentVariableA("_NT_SYMBOL_PATH", ..., ...)
add GetEnvironmentVariableA("_NT_ALTERNATE_SYMBOL_PATH", ..., ...)
add: "srv*c:\\symbols*http://msdl.microsoft.com/download/symbols;cache*c:\\symbols"
(except a better directory than c:\\symbols
*/

static bool SetupSymbolPath()
{
    Str::Str<WCHAR> path(1024);

    if (!_SymSetSearchPathW && !_SymSetSearchPath)
        return false;

    WCHAR buf[512];
    DWORD cchBuf = dimof(buf);
    DWORD res = GetEnvironmentVariableW(L"_NT_SYMBOL_PATH", buf, cchBuf);
    if (GetEnvOk(res, cchBuf)) {
        path.Append(buf);
        path.Append(L";");
    }
    res = GetEnvironmentVariableW(L"_NT_ALTERNATE_SYMBOL_PATH", buf, cchBuf);
    if (GetEnvOk(res, cchBuf)) {
        path.Append(buf);
        path.Append(L";");
    }
    ScopedMem<TCHAR> symDir(GetCrashDumpDir());
    if (symDir) {
        path.Append(symDir);
        path.Append(_T(";"));
    }
#if 0
    // this probably wouldn't work anyway because it requires symsrv.dll in the same directory
    // as dbghelp.dll and it's not present with the os-provided dbghelp.dll
    path.Append(L"srv*");
    path.Append(symDir);
    path.Append(L"*http://msdl.microsoft.com/download/symbols;cache*");
    path.Append(symDir);
#endif
    // when running local builds, *.pdb is in the same dir as *.exe 
    ScopedMem<TCHAR> exePath(GetExePath());
    ScopedMem<TCHAR> exeDir(Path::GetDir(exePath));
    path.AppendFmt(_T("%s"), exeDir);
    BOOL ok = FALSE;
    if (_SymSetSearchPathW) {
        ok = _SymSetSearchPathW(GetCurrentProcess(), path.Get());
    } else {
        char *tmp = Str::Conv::ToAnsi(path.Get());
        if (tmp) {
            ok = _SymSetSearchPath(GetCurrentProcess(), tmp);
            free(tmp);
        }
    }

    return ok;    
}

static bool InitializeDbgHelp()
{
    if (!LoadDbgHelpFuncs())
        return false;

    if (!_SymInitialize)
        return false;

    gSymInitializeOk = _SymInitialize(GetCurrentProcess(), NULL, TRUE);
    if (!gSymInitializeOk)
        return false;

    DWORD symOptions =_SymGetOptions();
    symOptions = (SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS; // don't show system msg box on errors
    _SymSetOptions(symOptions);
    SetupSymbolPath();
    return true;
}

static bool CanStackWalk()
{
    return gSymInitializeOk && _SymInitialize && _SymGetOptions 
        && _SymSetOptions&& _StackWalk64 && _SymFunctionTableAccess64 
        && _SymGetModuleBase64 && _SymFromAddr;
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

static char *OsNameFromVer(OSVERSIONINFOEX ver)
{
    if (VER_PLATFORM_WIN32_NT != ver.dwPlatformId)
        return "No NT";

    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 1)
        return "7"; // or Server 2008
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 0)
        return "Vista";
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 2)
        return "Sever 2003";
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 1)
        return "XP";
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 0)
        return "2000";

    // either a newer or an older NT version, neither of which we support
    static char osVerStr[32];
    wsprintfA(osVerStr, "NT %d.%d", ver.dwMajorVersion, ver.dwMinorVersion);
    return osVerStr;
}

static bool IsWow64()
{
    typedef BOOL (WINAPI *IsWow64ProcessProc)(HANDLE, PBOOL);
    IsWow64ProcessProc _IsWow64Process;

    _IsWow64Process = (IsWow64ProcessProc)LoadDllFunc(_T("kernel32.dll"), "IsWow64Process");
    if (!_IsWow64Process)
        return false;
    BOOL isWow = FALSE;
    _IsWow64Process(GetCurrentProcess(), &isWow);
    return isWow;
}

static void GetOsVersion(Str::Str<char>& s)
{
    OSVERSIONINFOEX ver;
    ZeroMemory(&ver, sizeof(ver));
    ver.dwOSVersionInfoSize = sizeof(ver);
    BOOL ok = GetVersionEx((OSVERSIONINFO*)&ver);
    if (!ok)
        return;
    char *os = OsNameFromVer(ver);
    int servicePackMajor = ver.wServicePackMajor;
    int servicePackMinor = ver.wServicePackMinor;
    int buildNumber = ver.dwBuildNumber & 0xFFFF;
#ifdef _WIN64
    char *arch = "64-bit";
#else
    char *arch = IsWow64() ? "Wow64" : "32-bit";
#endif
    s.AppendFmt("OS: Windows %s %d.%d build %d %s\r\n", os, servicePackMajor, servicePackMinor, buildNumber, arch);
}

static void GetProcessorName(Str::Str<char>& s)
{
    TCHAR *name = ReadRegStr(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor"), _T("ProcessorNameString"));
    if (!name) // if more than one processor
        name = ReadRegStr(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), _T("ProcessorNameString"));;
    if (!name)
        return;
    char *tmp = Str::Conv::ToUtf8(name);
    s.AppendFmt("Processor: %s\r\n", tmp);
    free(tmp);
    free(name);
}

static void GetSystemInfo(Str::Str<char>& s)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s.AppendFmt("Number Of Processors: %d\r\n", si.dwNumberOfProcessors);
    GetProcessorName(s);

    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    float physMemGB   = (float)ms.ullTotalPhys     / (float)(1024 * 1024 * 1024);
    float totalPageGB = (float)ms.ullTotalPageFile / (float)(1024 * 1024 * 1024);
    DWORD usedPerc = ms.dwMemoryLoad;
    s.AppendFmt("Physical Memory: %.2f GB\r\nCommit Charge Limit: %.2f GB\r\nMemory Used: %d%%\r\n", physMemGB, totalPageGB, usedPerc);

    // Note: maybe more information, like:
    // * amount of memory used by Sumatra,
    // * system name (HKLM/HARDWARE/DESCRIPTION/BIOS/SystemFamily and/or SystemVersion
    // * graphics card and its driver version
    // * processor capabilities (mmx, sse, sse2 etc.)
    // * list of currently opened documents (by traversing gWindows)
}

// return true for static, single executable build, false for a build with libmupdf.dll
static bool IsStaticBuild()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return true;
    MODULEENTRY32 mod;
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    bool isStatic = true;
    while (cont) {
        TCHAR *name = mod.szModule;
        if (Str::EqI(name, _T("libmupdf.dll"))) {
            isStatic = false;
            break;
        }
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
    return isStatic;
}

static void GetModules(Str::Str<char>& s)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return;

    MODULEENTRY32 mod;
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        TCHAR *name = mod.szModule;
        TCHAR *path = mod.szExePath;
        char *nameA = Str::Conv::ToUtf8(name);
        char *pathA = Str::Conv::ToUtf8(path);
        s.AppendFmt("Module: %08X %06X %-16s %s\r\n", (DWORD)mod.modBaseAddr, (DWORD)mod.modBaseSize, nameA, pathA);
        free(nameA);
        free(pathA);
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
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

static bool HasSymbolsForAddress(DWORD64 addr)
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
    return ok && symInfo->Name[0];
}

// a heuristic to test if we have symbols for our own binaries by testing if
// we can get symbol for any of our functions.
static bool HasOwnSymbols()
{
    DWORD64 addr = (DWORD64)&HasSymbolsForAddress;
    return HasSymbolsForAddress(addr);
}

static void GetAddressInfo(Str::Str<char>& s, DWORD64 addr)
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
        Str::ToLower(module);
        const char *moduleShort = Path::GetBaseName(module);
#ifdef _WIN64
        s.AppendFmt("%016I64X %02X:%016I64X %s", (DWORD64)addr, section, (DWORD64)offset, moduleShort);
#else
        s.AppendFmt("%08X %02X:%08X %s", (DWORD)addr, section, (DWORD)offset, moduleShort);
#endif
        if (symName)
            s.AppendFmt("!%s+0x%x", symName, (int)symDisp);
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD disp;
        if (_SymGetLineFromAddr64(GetCurrentProcess(), addr, &disp, &line)) {
            s.AppendFmt(" %s+%d", line.FileName, line.LineNumber);
        }
        s.Append("\r\n");
    } else {
#ifdef _WIN64
        s.AppendFmt("%016I64X\r\n", addr);
#else
        s.AppendFmt("%08X\r\n", (DWORD)addr);
#endif
    }
}

static bool GetStackFrameInfo(Str::Str<char>& s, STACKFRAME64 *stackFrame,
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

static void GetCallstack(Str::Str<char>& s, CONTEXT& ctx, HANDLE hThread)
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
    if (0 == framesCount) {
        s.Append("StackWalk64() couldn't get even the first stack frame info");
    }
}

static void GetCurrentThreadCallstack(Str::Str<char>&s)
{
    CONTEXT ctx;
    RtlCaptureContext(&ctx);
    s.AppendFmt("Thread: %x\r\n", GetCurrentThreadId());
    GetCallstack(s, ctx, GetCurrentThread());
}

static void GetThreadCallstack(Str::Str<char>& s, DWORD threadId)
{
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
        CONTEXT ctx = { 0 };
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

static void GetAllThreadsCallstacks(Str::Str<char>& s)
{
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
        GetModuleHandleA("NTDLL.DLL"),
        excCode, 0, buf, sizeof(buf), 0);

    return buf;
}

static void GetExceptionInfo(Str::Str<char>& s, EXCEPTION_POINTERS *excPointers)
{
    if (!excPointers)
        return;
    EXCEPTION_RECORD *excRecord = excPointers->ExceptionRecord;
    DWORD excCode = excRecord->ExceptionCode;
    s.AppendFmt("Exception: %08X %s\r\n", (int)excCode, ExceptionNameFromCode(excCode));

    s.AppendFmt("Fault address: ");
    GetAddressInfo(s, (DWORD64)excRecord->ExceptionAddress);

    PCONTEXT ctx = excPointers->ContextRecord;
    s.AppendFmt("\r\nRegisters:\r\n");

#ifdef _WIN64
    s.AppendFmt("RAX:%016I64X\r\nRBX:%016I64X\r\nRCX:%016I64X\r\nRDX:%016I64X\r\nRSI:%016I64X\r\nRDI:%016I64X\r\n"
        "R8: %016I64X\r\nR9: %016I64X\r\nR10:%016I64X\r\nR11:%016I64X\r\nR12:%016I64X\r\nR13:%016I64X\r\nR14:%016I64X\r\nR15:%016I64X\r\n",
        ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx, ctx->Rsi, ctx->Rdi,
        ctx->R9,ctx->R10,ctx->R11,ctx->R12,ctx->R13,ctx->R14,ctx->R15);
    s.AppendFmt("CS:RIP:%04X:%016I64X\r\n", ctx->SegCs, ctx->Rip);
    s.AppendFmt("SS:RSP:%04X:%016X  RBP:%08X\r\n", ctx->SegSs, ctx->Rsp, ctx->Rbp);
    s.AppendFmt("DS:%04X  ES:%04X  FS:%04X  GS:%04X\r\n", ctx->SegDs, ctx->SegEs, ctx->SegFs, ctx->SegGs);
    s.AppendFmt("Flags:%08X\r\n", ctx->EFlags);
#else
    s.AppendFmt("EAX:%08X\r\nEBX:%08X\r\nECX:%08X\r\nEDX:%08X\r\nESI:%08X\r\nEDI:%08X\r\n",
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

static void GetProgramVersion(Str::Str<char>& s)
{
    s.AppendFmt("Ver: %s", QM(CURR_VERSION));
#ifdef SVN_PRE_RELEASE_VER
    s.AppendFmt(" pre-release %s", QM(SVN_PRE_RELEASE_VER));   
#endif
#ifdef DEBUG
    s.Append(" dbg");
#endif
    s.Append("\r\n");
}

static char *BuildCrashInfoText()
{    
    if (!InitializeDbgHelp())
        return NULL;

    Str::Str<char> s(16 * 1024);
    GetProgramVersion(s);
    GetOsVersion(s);
    GetSystemInfo(s);
    s.Append("\r\n");
    GetModules(s);
    s.Append("\r\n");
    GetExceptionInfo(s, gMei.ExceptionPointers);
    GetAllThreadsCallstacks(s);
    s.Append("\r\n");
    GetCurrentThreadCallstack(s);
    return s.StealData();
}

#define CRASH_SUBMIT_SERVER _T("blog.kowalczyk.info")
#define CRASH_SUBMIT_URL    _T("/app/crashsubmit?appname=SumatraPDF")

static void SendCrashInfo(char *s)
{
    if (!s)
        return;

    char *boundary = "0xKhTmLbOuNdArY";
    Str::Str<char> headers(256);
    headers.AppendFmt("Content-Type: multipart/form-data; boundary=%s", boundary);

    Str::Str<char> data(2048);
    data.AppendFmt("--%s\r\n", boundary);
    data.Append("Content-Disposition: form-data; name=\"file\"; filename=\"test.bin\"\r\n\r\n");
    data.Append(s);
    data.Append("\r\n");
    data.AppendFmt("\r\n--%s--\r\n", boundary);

    HttpPost(CRASH_SUBMIT_SERVER, CRASH_SUBMIT_URL, &headers, &data);
}

// We might have symbol files for older builds. If we're here, then we
// didn't get the symbols so we assume it's because symbols didn't match
// Returns false if files were there but we couldn't delete them
static bool DeleteSymbolsIfExist(const TCHAR *symDir)
{
    ScopedMem<TCHAR> path(Path::Join(symDir, _T("libmupdf.pdb")));
    if (!File::Delete(path))
        return false;
    path.Set(Path::Join(symDir, _T("SumatraPDF.pdb")));
    return File::Delete(path);
}

// In static (single executable) pre-release build, the pdb file inside 
// symbolsZipPath is named SumatraPDF-prelease-${buildno}.pdb and must be
// extracted under that name, to match executable name
static bool UnpackStaticPreReleaseSymbols(const TCHAR *symbolsZipPath, const TCHAR *symDir)
{
    FileToUnzip filesToUnnpack[] = {
        { "SumatraPDF-prerelease", NULL, false },
        { NULL, false }
    };
    return UnzipFilesStartingWith(symbolsZipPath, filesToUnnpack, symDir);
}

// In static (single executable) release build, the pdb file inside
// symbolsZipPath is named SumatraPDF-${ver}.pdb and must be extracted as
// SumatraPDF.pdb, to match executable name
static bool UnpackStaticReleaseSymbols(const TCHAR *symbolsZipPath, const TCHAR *symDir)
{
    FileToUnzip filesToUnnpack[] = {
        { "SumatraPDF-" QM(CURR_VERSION) ".pdb", _T("SumatraPDF.pdb"), false },
        { NULL, false }
    };
    return UnzipFilesStartingWith(symbolsZipPath, filesToUnnpack, symDir);
}

// In lib (.exe + libmupdf.dll) release and pre-release builds, the pdb files
// inside symbolsZipPath are libmupdf.pdb and SumatraPDF-no-MuPDF.pdb and
// must be extracted as libmupdf.pdb and SumatraPDF.pdb, to match the dll/exe
// names.
static bool UnpackLibSymbols(const TCHAR *symbolsZipPath, const TCHAR *symDir)
{
    FileToUnzip filesToUnnpack[] = {
        { "libmupdf.pdb", NULL, false },
        { "SumatraPDF-no-MuPDF.pdb", _T("SumatraPDF.pdb"), false },
        { NULL, false }
    };
    return UnzipFilesStartingWith(symbolsZipPath, filesToUnnpack, symDir);
}

static bool DownloadPreReleaseSymbols(const TCHAR *symDir)
{
    TCHAR *url = _T("http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-") _T(QM(SVN_PRE_RELEASE_VER)) _T(".pdb.zip");
    ScopedMem<TCHAR> symbolsZipPath(Path::Join(symDir, _T("symbols_tmp.zip")));
    if (!HttpGetToFile(url, symbolsZipPath))
        return false;
    bool ok = false;
    if (IsStaticBuild()) {
        ok = UnpackStaticPreReleaseSymbols(symbolsZipPath, symDir);
#if defined(DEBUG_CRASH_INFO)
        if (!ok)
            MessageBox(NULL, _T("Couldn't unpack pre-release symbols for static build"), _T("CrashHandler info"), MB_ICONERROR | MB_OK);
#endif
        
    } else {
        ok = UnpackLibSymbols(symbolsZipPath, symDir);
#if defined(DEBUG_CRASH_INFO)
        if (!ok)
            MessageBox(NULL, _T("Couldn't unpack pre-relase symbols for lib build"), _T("CrashHandler info"), MB_ICONERROR | MB_OK);
#endif
    }
    File::Delete(symbolsZipPath);
    return ok;
}

static bool DownloadReleaseSymbols(const TCHAR *symDir)
{
    TCHAR *url = _T("http://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-") _T(QM(CURR_VERSION)) _T(".pdb.zip");
    ScopedMem<TCHAR> symbolsZipPath(Path::Join(symDir, _T("symbols_tmp.zip")));
    if (!HttpGetToFile(url, symbolsZipPath)) {
#if defined(DEBUG_CRASH_INFO)
        MessageBox(NULL, _T("Couldn't download release symbols"), _T("CrashHandler info"), MB_ICONERROR | MB_OK);
#endif
        return false;
    }
    bool ok = false;
    if (IsStaticBuild()) {
        ok = UnpackStaticReleaseSymbols(symbolsZipPath, symDir);
#if defined(DEBUG_CRASH_INFO)
        if (!ok)
            MessageBox(NULL, _T("Couldn't unpack release symbols for static build"), _T("CrashHandler info"), MB_ICONERROR | MB_OK);
#endif
    } else {
        ok = UnpackLibSymbols(symbolsZipPath, symDir);
#if defined(DEBUG_CRASH_INFO)
        if (!ok)
            MessageBox(NULL, _T("Couldn't unpack release symbols for lib build"), _T("CrashHandler info"), MB_ICONERROR | MB_OK);
#endif
    }
    File::Delete(symbolsZipPath);
    return ok;
}

// TODO: *.pdb files are on S3 with a known name, try to download them here to a directory in symbol
// path to get better info 
static bool DownloadSymbols(const TCHAR *symDir)
{
    if (!symDir || !Dir::Exists(symDir)) return false;
    if (!DeleteSymbolsIfExist(symDir))
        return false;

#if defined(DEBUG)
    // don't care about debug builds because we don't release them
    return false;
#endif

#if defined(SVN_PRE_RELEASE_VER)
    return DownloadPreReleaseSymbols(symDir);
#else
    return DownloadReleaseSymbols(symDir);
#endif
}

// We're (potentially) doing it twice for reliability reason. First with whatever symbols we already
// have. Then, if we don't have symbols for our binaries, download the symbols from a website and
// redo the callstacks. But if our state is so corrupted that we can't download symbols, we'll
// at least have non-symbolized version of callstacks
// TODO: if it turns out that that downloading symbols is reliable, we will
// just do it once
void SubmitCrashInfo()
{
    char *s1, *s2 = NULL;
    s1 = BuildCrashInfoText();
    if (!s1)
        goto Exit;
    SendCrashInfo(s1);
    if (HasOwnSymbols())
        goto Exit;
    if (!DownloadSymbols(GetCrashDumpDir()))
        goto Exit;
    // TODO: should I re-initialize dbghlp? I'm getting empty callstack
    if (!HasOwnSymbols()) {
#if defined(DEBUG_CRASH_INFO)
        MessageBox(NULL, _T("Downloaded pdb but couldn't resolve symbols"), _T("CrashHandler info"), MB_ICONERROR | MB_OK);
#endif
        goto Exit;
    }
    s2 = BuildCrashInfoText();
    SendCrashInfo(s2);
Exit:
    free(s2);
    free(s1);    
}

static void WriteMiniDump()
{
    HANDLE dumpFile = CreateFile(gCrashDumpPath.Get(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (INVALID_HANDLE_VALUE == dumpFile)
        return;

    MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory);
    // set the SUMATRAPDF_FULLDUMP environment variable for far more complete minidumps
    if (GetEnvironmentVariable(_T("SUMATRAPDF_FULLDUMP"), NULL, 0))
        type = (MINIDUMP_TYPE)(type | MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithPrivateReadWriteMemory);
    MINIDUMP_CALLBACK_INFORMATION mci = { OpenMiniDumpCallback, NULL }; 

    _MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, type, &gMei, NULL, &mci);

    CloseHandle(dumpFile);
}

static DWORD WINAPI CrashDumpThread(LPVOID data)
{
    WaitForSingleObject(gDumpEvent, INFINITE);
    if (!LoadDbgHelpFuncs())
        return 0;

    // TODO: experimentally, don't write minidumps
    //WriteMiniDump();

    SubmitCrashInfo();
    return 0;
}

static LONG WINAPI DumpExceptionHandler(EXCEPTION_POINTERS *exceptionInfo)
{
    if (!exceptionInfo || (EXCEPTION_BREAKPOINT == exceptionInfo->ExceptionRecord->ExceptionCode))
        return EXCEPTION_CONTINUE_SEARCH;

    static bool wasHere = false;
    if (wasHere)
        return EXCEPTION_CONTINUE_SEARCH; // Note: or should TerminateProcess()?
    wasHere = true;

    gMei.ThreadId = GetCurrentThreadId();
    gMei.ExceptionPointers = exceptionInfo;
    // per msdn (which is backed by my experience), MiniDumpWriteDump() doesn't
    // write callstack for the calling thread correctly. We use msdn-recommended
    // work-around of spinning a thread to do the writing
    SetEvent(gDumpEvent);
    WaitForSingleObject(gDumpThread, INFINITE);

#if 0
    ScopedMem<TCHAR> msg(Str::Format(_T("%s\n\n%s"), _TR("Please include the following file in your crash report:"), gCrashDumpPath.Get()));
    MessageBox(NULL, msg.Get(), _TR("SumatraPDF crashed"), MB_ICONERROR | MB_OK);
#endif

    TerminateProcess(GetCurrentProcess(), 1);

    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler(const TCHAR *crashDumpPath)
{
    if (NULL == crashDumpPath)
        return;

    gCrashDumpPath.Set(Str::Dup(crashDumpPath));
    if (!gDumpEvent && !gDumpThread) {
        gDumpEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        gDumpThread = CreateThread(NULL, 0, CrashDumpThread, NULL, 0, 0);

        SetUnhandledExceptionFilter(DumpExceptionHandler);
    }
}

/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>

#include "Version.h"

#include "BaseUtil.h"
#include "StrUtil.h"
#include "Vec.h"
#include "WinUtil.h"
#include "FileUtil.h"
#include "Http.h"
#include "ZipUtil.h"
#include "SimpleLog.h"

#include "SumatraPDF.h"
#include "CrashHandler.h"
#include "AppTools.h"
#include "translations.h"

#ifndef CRASH_REPORT_URL
#define CRASH_REPORT_URL _T("http://blog.kowalczyk.info/software/sumatrapdf/develop.html")
#endif

#ifndef SYMBOL_DOWNLOAD_URL
#ifdef SVN_PRE_RELEASE_VER
#define SYMBOL_DOWNLOAD_URL _T("http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-") _T(QM(SVN_PRE_RELEASE_VER)) _T(".pdb.zip")
#else
#define SYMBOL_DOWNLOAD_URL _T("http://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-") _T(QM(CURR_VERSION)) _T(".pdb.zip")
#endif
#endif

#if !defined(CRASH_SUBMIT_SERVER) || !defined(CRASH_SUBMIT_URL)
#define CRASH_SUBMIT_SERVER _T("blog.kowalczyk.info")
#define CRASH_SUBMIT_URL    _T("/app/crashsubmit?appname=SumatraPDF")
#endif

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

static MiniDumpWriteDumpProc *          _MiniDumpWriteDump = NULL;
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

static ScopedMem<TCHAR> gCrashDumpPath(NULL);
static HANDLE gDumpEvent = NULL;
static HANDLE gDumpThread = NULL;
static MINIDUMP_EXCEPTION_INFORMATION gMei = { 0 };
static BOOL gSymInitializeOk = FALSE;

static slog::DebugLogger gDbgLog;
#define LogDbg(msg, ...) gDbgLog.LogFmt(_T(msg), __VA_ARGS__)
#if 0 // 1 for more detailed debugging of crash handler progress
#define LogDbgDetail LogDbg
#else
#define LogDbgDetail(msg, ...) NoOp()
#endif

static bool LoadDbgHelpFuncs()
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

static TCHAR *GetCrashDumpDir()
{
    TCHAR *symDir = AppGenDataFilename(_T("symbols"));
    if (symDir && !dir::Create(symDir)) {
        free(symDir);
        LogDbg("GetCrashDumpDir(): couldn't get symbols dir");
        return NULL;
    }
    return symDir;
}

/* Setting symbol path:
add GetEnvironmentVariableA("_NT_SYMBOL_PATH", ..., ...)
add GetEnvironmentVariableA("_NT_ALTERNATE_SYMBOL_PATH", ..., ...)
add: "srv*c:\\symbols*http://msdl.microsoft.com/download/symbols;cache*c:\\symbols"
(except a better directory than c:\\symbols

Note: I've decided to use just one, known to me location rather than the
more comprehensive list. It works so why give dbghelp.dll more directories
to scan?
*/
static WCHAR *GetSymbolPath()
{
    str::Str<WCHAR> path(1024);

#if 0
    WCHAR buf[512];
    DWORD res = GetEnvironmentVariableW(L"_NT_SYMBOL_PATH", buf, dimof(buf));
    if (0 < res && res < dimof(buf)) {
        path.Append(buf);
        path.Append(L";");
    }
    res = GetEnvironmentVariableW(L"_NT_ALTERNATE_SYMBOL_PATH", buf, dimof(buf));
    if (0 < res && res < dimof(buf)) {
        path.Append(buf);
        path.Append(L";");
    }
#endif

    ScopedMem<WCHAR> symDir(str::conv::ToWStrQ(GetCrashDumpDir()));
    if (symDir) {
        path.Append(symDir);
        //path.Append(_T(";"));
    }
#if 0
    // this probably wouldn't work anyway because it requires symsrv.dll in the same directory
    // as dbghelp.dll and it's not present with the os-provided dbghelp.dll
    path.Append(L"srv*");
    path.Append(symDir);
    path.Append(L"*http://msdl.microsoft.com/download/symbols;cache*");
    path.Append(symDir);
#endif
#if 0
    // when running local builds, *.pdb is in the same dir as *.exe 
    ScopedMem<TCHAR> exePath(GetExePath());
    ScopedMem<WCHAR> exeDir(str::conv::ToWStrQ(path::GetDir(exePath)));
    path.AppendFmt(L"%s", exeDir);
#endif
    return path.StealData();
}

#if 0
static bool SetupSymbolPath()
{
    if (!_SymSetSearchPathW && !_SymSetSearchPath) {
        LogDbg("SetupSymbolPath(): _SymSetSearchPathW and _SymSetSearchPath missing");
        return false;
    }

    ScopedMem<WCHAR> path(GetSymbolPath());
    if (!path) {
        LogDbg("SetupSymbolPath(): GetSymbolPath() returned NULL");
        return false;
    }

    BOOL ok = FALSE;
    ScopedMem<TCHAR> tpath(str::conv::FromWStr(path));
    if (_SymSetSearchPathW) {
        ok = _SymSetSearchPathW(GetCurrentProcess(), path);
        if (!ok)
            LogDbg("_SymSetSearchPathW() failed, path='%s'", tpath);
    } else {
        ScopedMem<char> tmp(str::conv::ToAnsi(tpath));
        ok = _SymSetSearchPath(GetCurrentProcess(), tmp);
        if (!ok)
            LogDbg("_SymSetSearchPath() failed, path='%s'", tpath);
    }

    _SymRefreshModuleList(GetCurrentProcess());
    free((void*)path);
    return ok;
}
#endif

static bool InitializeDbgHelp()
{
    if (!LoadDbgHelpFuncs()) {
        LogDbg("InitializeDbgHelp(): LoadDbgHelpFuncs() failed");
        return false;
    }

    if (!_SymInitializeW && !_SymInitialize) {
        LogDbg("InitializeDbgHelp(): SymInitializeW() or SymInitializeA() not present in dbghelp.dll");
        return false;
    }

    WCHAR *symPath = GetSymbolPath();
    if (!symPath) {
        LogDbg("InitializeDbgHelp(): GetSymbolPath() failed");
        return false;
    }

    if (_SymInitializeW) {
        gSymInitializeOk = _SymInitializeW(GetCurrentProcess(), symPath, TRUE);
    } else {
        ScopedMem<TCHAR> tstr(str::conv::FromWStr(symPath));
        ScopedMem<char> tmp(str::conv::ToAnsi(tstr));
        if (tmp)
            gSymInitializeOk = _SymInitialize(GetCurrentProcess(), tmp, TRUE);
    }

    if (!gSymInitializeOk) {
        LogDbg("InitializeDbgHelp(): _SymInitialize() failed");
        return false;
    }

    DWORD symOptions = _SymGetOptions();
    symOptions = (SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS; // don't show system msg box on errors
    _SymSetOptions(symOptions);

    //SetupSymbolPath();
    return true;
}

static bool CanStackWalk()
{
    return gSymInitializeOk && _SymCleanup && _SymGetOptions 
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
        return "9x";

    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 1)
        return "7"; // or Server 2008
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 0)
        return "Vista";
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 2)
        return "Server 2003";
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 1)
        return "XP";
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 0)
        return "2000";

    // either a newer or an older NT version, neither of which we support
    static char osVerStr[32];
    wsprintfA(osVerStr, "NT %d.%d", ver.dwMajorVersion, ver.dwMinorVersion);
    return osVerStr;
}

static void GetOsVersion(str::Str<char>& s)
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
    char *arch = IsRunningInWow64() ? "Wow64" : "32-bit";
#endif
    if (0 == servicePackMajor)
        s.AppendFmt("OS: Windows %s build %d %s\r\n", os, buildNumber, arch);
    else if (0 == servicePackMinor)
        s.AppendFmt("OS: Windows %s SP%d build %d %s\r\n", os, servicePackMajor, buildNumber, arch);
    else
        s.AppendFmt("OS: Windows %s %d.%d build %d %s\r\n", os, servicePackMajor, servicePackMinor, buildNumber, arch);
}

static void GetProcessorName(str::Str<char>& s)
{
    TCHAR *name = ReadRegStr(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor"), _T("ProcessorNameString"));
    if (!name) // if more than one processor
        name = ReadRegStr(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), _T("ProcessorNameString"));
    if (!name)
        return;

    ScopedMem<char> tmp(str::conv::ToUtf8(name));
    s.AppendFmt("Processor: %s\r\n", tmp);
    free(name);
}

static void GetMachineName(str::Str<char>& s)
{
    TCHAR *s1 = ReadRegStr(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\BIOS"), _T("SystemFamily"));
    TCHAR *s2 = ReadRegStr(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\BIOS"), _T("SystemVersion"));
    ScopedMem<char> s1u(s1 ? str::conv::ToUtf8(s1) : NULL);
    ScopedMem<char> s2u(s2 ? str::conv::ToUtf8(s2) : NULL);

    if (!s1u && !s2u)
        ; // pass
    else if (!s1u)
        s.AppendFmt("Machine: %s\r\n", s2u.Get());
    else if (!s2u || str::EqI(s1u, s2u))
        s.AppendFmt("Machine: %s\r\n", s1u.Get());
    else
        s.AppendFmt("Machine: %s %s\r\n", s1u.Get(), s2u.Get());

    free(s1);
    free(s2);
}

static void GetLanguage(str::Str<char>& s)
{
    char country[32] = { 0 }, lang[32] = { 0 };
    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, dimof(country) - 1);
    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, dimof(lang) - 1);
    s.AppendFmt("Lang: %s %s\r\n", lang, country);
}

static void GetSystemInfo(str::Str<char>& s)
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

    GetMachineName(s);
    GetLanguage(s);

    // Note: maybe more information, like:
    // * amount of memory used by Sumatra,
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
        if (str::EqI(name, _T("libmupdf.dll"))) {
            isStatic = false;
            break;
        }
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
    return isStatic;
}

static void GetModules(str::Str<char>& s)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return;

    MODULEENTRY32 mod;
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        ScopedMem<char> nameA(str::conv::ToUtf8(mod.szModule));
        ScopedMem<char> pathA(str::conv::ToUtf8(mod.szExePath));
        s.AppendFmt("Module: %08X %06X %-16s %s\r\n", (DWORD)mod.modBaseAddr, (DWORD)mod.modBaseSize, nameA, pathA);
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

static void GetCallstack(str::Str<char>& s, CONTEXT& ctx, HANDLE hThread)
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

// disabled because apparently RtlCaptureContext() crashes when code
// is compiled with  Omit Frame Pointers option (/Oy explicitly, can
// be turned on implictly by e.g. /O2)
// http://www.bytetalk.net/2011/06/why-rtlcapturecontext-crashes-on.html
// This thread isn't important - it's the CrashHandler thread.
#if 0
typedef VOID WINAPI RtlCaptureContextProc(
    PCONTEXT ContextRecord);
static void GetCurrentThreadCallstack(str::Str<char>& s)
{
    CONTEXT ctx;
    //Some blog post say this is an alternative way to get CONTEXT
    //but it doesn't work in practice
    //ctx = *(gMei.ExceptionPointers->ContextRecord);

    // not available under Win2000
    RtlCaptureContextProc *MyRtlCaptureContext = (RtlCaptureContextProc *)LoadDllFunc(_T("kernel32.dll"), "RtlCaptureContext");
    if (!RtlCaptureContext)
        return;

    MyRtlCaptureContext(&ctx);
    s.AppendFmt("Thread: %x\r\n", GetCurrentThreadId());
    GetCallstack(s, ctx, GetCurrentThread());
}
#endif

static void GetThreadCallstack(str::Str<char>& s, DWORD threadId)
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

static void GetAllThreadsCallstacks(str::Str<char>& s)
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

static void GetExceptionInfo(str::Str<char>& s, EXCEPTION_POINTERS *excPointers)
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

static void GetProgramInfo(str::Str<char>& s)
{
    s.AppendFmt("Ver: %s", QM(CURR_VERSION));
#ifdef SVN_PRE_RELEASE_VER
    s.AppendFmt(".%s pre-release", QM(SVN_PRE_RELEASE_VER));   
#endif
#ifdef DEBUG
    s.Append(" dbg");
#endif
    s.Append("\r\n");
    s.AppendFmt("Browser plugin: %s\r\n", gPluginMode ? "yes" : "no");
}

// in SumatraPDF.cpp
extern void GetFilesInfo(str::Str<char>& s);

static char *BuildCrashInfoText()
{
    LogDbgDetail("BuildCrashInfoText(): start");

    if (!gSymInitializeOk) {
        LogDbg("BuildCrashInfoText(): gSymInitializeOk is false");
        return NULL;
    }

    str::Str<char> s(16 * 1024);
    GetProgramInfo(s);
    LogDbgDetail("BuildCrashInfoText(): 1");
    GetOsVersion(s);
    LogDbgDetail("BuildCrashInfoText(): 2");
    GetSystemInfo(s);
    LogDbgDetail("BuildCrashInfoText(): 3");
    GetFilesInfo(s);
    s.Append("\r\n");
    LogDbgDetail("BuildCrashInfoText(): 4");
    GetExceptionInfo(s, gMei.ExceptionPointers);
    LogDbgDetail("BuildCrashInfoText(): 5");
    GetAllThreadsCallstacks(s);
    s.Append("\r\n");
    LogDbgDetail("BuildCrashInfoText(): 6");
#if 0 // disabled because crashes in release builds
    GetCurrentThreadCallstack(s);
    s.Append("\r\n");
    LogDbgDetail("BuildCrashInfoText(): 7");
#endif
    GetModules(s);
    LogDbgDetail("BuildCrashInfoText(): finish");
    return s.StealData();
}

static void SendCrashInfo(char *s)
{
    LogDbgDetail("SendCrashInfo(): started");
    if (str::IsEmpty(s)) {
        LogDbg("SendCrashInfo(): s is empty");
        return;
    }

    char *boundary = "0xKhTmLbOuNdArY";
    str::Str<char> headers(256);
    headers.AppendFmt("Content-Type: multipart/form-data; boundary=%s", boundary);

    str::Str<char> data(2048);
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
    ScopedMem<TCHAR> path(path::Join(symDir, _T("libmupdf.pdb")));
    if (!file::Delete(path))
        return false;
    path.Set(path::Join(symDir, _T("SumatraPDF.pdb")));
    return file::Delete(path);
}

// In static (single executable) builds, the pdb file inside
// symbolsZipPath are named SumatraPDF-${ver}.pdb (release) resp.
// SumatraPDF-prelease-${buildno}.pdb (pre-release) and must be
// extracted as SumatraPDF.pdb to match the executable name
static bool UnpackStaticSymbols(const TCHAR *symbolsZipPath, const TCHAR *symDir)
{
    FileToUnzip filesToUnnpack[] = {
#ifdef SVN_PRE_RELEASE_VER
        { "SumatraPDF-prerelease-" QM(SVN_PRE_RELEASE_VER) ".pdb", _T("SumatraPDF.pdb") },
#else
        { "SumatraPDF-" QM(CURR_VERSION) ".pdb", _T("SumatraPDF.pdb") },
#endif
        { NULL }
    };
    return UnzipFiles(symbolsZipPath, filesToUnnpack, symDir);
}

// In lib (.exe + libmupdf.dll) release and pre-release builds, the pdb files
// inside symbolsZipPath are libmupdf.pdb and SumatraPDF-no-MuPDF.pdb and
// must be extracted as libmupdf.pdb and SumatraPDF.pdb, to match the dll/exe
// names.
static bool UnpackLibSymbols(const TCHAR *symbolsZipPath, const TCHAR *symDir)
{
    FileToUnzip filesToUnnpack[] = {
        { "libmupdf.pdb", NULL },
        { "SumatraPDF-no-MuPDF.pdb", _T("SumatraPDF.pdb") },
        { NULL }
    };
    return UnzipFiles(symbolsZipPath, filesToUnnpack, symDir);
}

// *.pdb files are on S3 with a known name. Try to download them here to a directory
// in symbol path to get meaningful callstacks 
static bool DownloadSymbols(const TCHAR *symDir)
{
    if (!symDir || !dir::Exists(symDir))
        return false;
    if (!DeleteSymbolsIfExist(symDir))
        return false;

#ifdef DEBUG
    // don't care about debug builds because we don't release them
    return false;
#endif

    ScopedMem<TCHAR> symbolsZipPath(path::Join(symDir, _T("symbols_tmp.zip")));
    if (!HttpGetToFile(SYMBOL_DOWNLOAD_URL, symbolsZipPath)) {
#ifndef SVN_PRE_RELEASE_VER
        LogDbg("Couldn't download release symbols");
#endif
        return false;
    }

    bool ok = false;
    if (IsStaticBuild()) {
        ok = UnpackStaticSymbols(symbolsZipPath, symDir);
        if (!ok)
            LogDbg("Couldn't unpack symbols for static build");
    } else {
        ok = UnpackLibSymbols(symbolsZipPath, symDir);
        if (!ok)
            LogDbg("Couldn't unpack symbols for lib build");
    }
    file::Delete(symbolsZipPath);
    return ok;
}

// If we can't resolve the symbols, we assume it's because we don't have symbols
// so we'll try to download them and retry. If we can resolve symbols, we'll
// get the callstacks etc. and submit to our server for analysis.
void SubmitCrashInfo()
{
    LogDbgDetail("SubmitCrashInfo(): start");
    if (!HasPermission(Perm_InternetAccess)) {
        LogDbg("SubmitCrashInfo(): No internet access permission");
        return;
    }

    char *s = NULL;

    if (!InitializeDbgHelp()) {
        LogDbg("SubmitCrashInfo(): InitializeDbgHelp() failed");
        goto Exit;
    }

    if (!HasOwnSymbols()) {
        if (!DownloadSymbols(GetCrashDumpDir())) {
            LogDbg("SubmitCrashInfo(): failed to download symbols");
            goto Exit;
        }

        _SymCleanup(GetCurrentProcess());
        if (!InitializeDbgHelp()) {
            LogDbg("SubmitCrashInfo(): InitializeDbgHelp() failed");
            goto Exit;
        }
    }

    if (!HasOwnSymbols()) {
        LogDbg("SubmitCrashInfo(): HasOwnSymbols() false after downloading symbols");
        goto Exit;
    }

    s = BuildCrashInfoText();
    if (!s) {
        LogDbg("SubmitCrashInfo(): BuildCrashInfoText() returned NULL for second report");
        goto Exit;
    }
    SendCrashInfo(s);
Exit:
    free(s);
}

static void WriteMiniDump()
{
    if (NULL == _MiniDumpWriteDump)
        return;

    HANDLE dumpFile = CreateFile(gCrashDumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
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

    SubmitCrashInfo();
    // always write a MiniDump (for the latest crash only)
    WriteMiniDump();

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

    // don't show a message box in restricted use, as the user most likely won't be
    // able to do anything about it anyway and it's up to the application provider
    // to fix the unexpected behavior (of which for a restricted set of documents
    // there should be much less, anyway)
    if (HasPermission(Perm_DiskAccess)) {
        int res = MessageBox(NULL, _TR("Sorry, that shouldn't have happened!\n\nPlease press 'Cancel', if you want to help us fix the cause of this crash."), _TR("SumatraPDF crashed"), MB_ICONERROR | MB_OKCANCEL | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        if (IDCANCEL == res)
            LaunchBrowser(CRASH_REPORT_URL);
    }

    TerminateProcess(GetCurrentProcess(), 1);

    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler(const TCHAR *crashDumpPath)
{
    if (NULL == crashDumpPath)
        return;

    gCrashDumpPath.Set(str::Dup(crashDumpPath));
    if (!gDumpEvent && !gDumpThread) {
        gDumpEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        gDumpThread = CreateThread(NULL, 0, CrashDumpThread, NULL, 0, 0);

        SetUnhandledExceptionFilter(DumpExceptionHandler);
    }
}

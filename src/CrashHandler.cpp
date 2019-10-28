/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "utils/BaseUtil.h"

#pragma warning(disable : 4668)
#include <tlhelp32.h>
#include <signal.h>
#include <eh.h>

#include "utils/WinDynCalls.h"
#include "utils/DbgHelpDyn.h"
#include "utils/FileUtil.h"
#include "utils/HttpUtil.h"
#include "utils/LzmaSimpleArchive.h"
#include "utils/WinUtil.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "CrashHandler.h"
#include "Version.h"
#define NOLOG 1 // 0 for more detailed debugging, 1 to disable lf()
#include "utils/DebugLog.h"

#if !defined(CRASH_SUBMIT_SERVER) || !defined(CRASH_SUBMIT_URL)
#define CRASH_SUBMIT_SERVER L"kjktools.org"
#define CRASH_SUBMIT_PORT 443

//#define CRASH_SUBMIT_SERVER L"127.0.0.1"
//#define CRASH_SUBMIT_PORT 6020

#define CRASH_SUBMIT_URL L"/crashreports/submit?app=SumatraPDF&ver=" CURR_VERSION_STR
#endif

// The following functions allow crash handler to be used by both installer
// and sumatra proper. They must be implemented for each app.
extern void GetStressTestInfo(str::Str<char>* s);
extern bool CrashHandlerCanUseNet();
extern void ShowCrashHandlerMessage();
extern void GetProgramInfo(str::Str<char>& s);

/* Note: we cannot use standard malloc()/free()/new()/delete() in crash handler.
For multi-thread safety, there is a per-heap lock taken by HeapAlloc() etc.
It's possible that a crash originates from  inside such functions after a lock
has been taken. If we then try to allocate memory from the same heap, we'll
deadlock and won't send crash report.
For that reason we create a heap used only for crash handler and must only
allocate, directly or indirectly, from that heap.
I'm not sure what happens if a Windows function (e.g. http calls) has to
allocate memory. I assume it'll use GetProcessHeap() heap and further assume
that CRT creates its own heap for malloc()/free() etc. so that while a deadlock
is still possible, the probability should be greatly reduced. */

class CrashHandlerAllocator : public Allocator {
    HANDLE allocHeap;

  public:
    CrashHandlerAllocator() { allocHeap = HeapCreate(0, 128 * 1024, 0); }
    virtual ~CrashHandlerAllocator() { HeapDestroy(allocHeap); }
    virtual void* Alloc(size_t size) { return HeapAlloc(allocHeap, 0, size); }
    virtual void* Realloc(void* mem, size_t size) { return HeapReAlloc(allocHeap, 0, mem, size); }
    virtual void Free(void* mem) { HeapFree(allocHeap, 0, mem); }
};

enum ExeType {
    // this is an installer, SumatraPDF-${ver}-install.exe
    ExeInstaller,
    // this is a single-executable (portable) build (doesn't have libmupdf.dll)
    ExeSumatraStatic,
    // an installable build (has libmupdf.dll)
    ExeSumatraLib
};

static CrashHandlerAllocator* gCrashHandlerAllocator = nullptr;

// Note: intentionally not using ScopedMem<> to avoid
// static initializers/destructors, which are bad
static WCHAR* gSymbolsUrl = nullptr;
static WCHAR* gCrashDumpPath = nullptr;
static WCHAR* gSymbolPathW = nullptr;
static WCHAR* gSymbolsDir = nullptr;
static WCHAR* gPdbZipPath = nullptr;
static WCHAR* gLibMupdfPdbPath = nullptr;
static WCHAR* gSumatraPdfPdbPath = nullptr;
static WCHAR* gInstallerPdbPath = nullptr;
static char* gSystemInfo = nullptr;
static char* gModulesInfo = nullptr;
static HANDLE gDumpEvent = nullptr;
static HANDLE gDumpThread = nullptr;
static ExeType gExeType = ExeSumatraStatic;
static bool gCrashed = false;

static MINIDUMP_EXCEPTION_INFORMATION gMei = {0};
static LPTOP_LEVEL_EXCEPTION_FILTER gPrevExceptionFilter = nullptr;

static char* BuildCrashInfoText() {
    lf("BuildCrashInfoText(): start");

    str::Str<char> s(16 * 1024, gCrashHandlerAllocator);
    if (gSystemInfo)
        s.Append(gSystemInfo);

    GetStressTestInfo(&s);
    s.Append("\r\n");

    dbghelp::GetExceptionInfo(s, gMei.ExceptionPointers);
    dbghelp::GetAllThreadsCallstacks(s);
    s.Append("\r\n");
    s.Append(gModulesInfo);

    s.Append("\r\n");
    s.Append(dbglog::GetCrashLog());

    return s.StealData();
}

static void SendCrashInfo(char* s) {
    lf("SendCrashInfo(): started");
    if (str::IsEmpty(s)) {
        plog("SendCrashInfo(): s is empty");
        return;
    }

    const char* boundary = "0xKhTmLbOuNdArY";
    str::Str<char> headers(256, gCrashHandlerAllocator);
    headers.AppendFmt("Content-Type: multipart/form-data; boundary=%s", boundary);

    str::Str<char> data(2048, gCrashHandlerAllocator);
    data.AppendFmt("--%s\r\n", boundary);
    data.Append("Content-Disposition: form-data; name=\"file\"; filename=\"sumcrash.txt\"\r\n\r\n");
    data.Append(s);
    data.Append("\r\n");
    data.AppendFmt("\r\n--%s--\r\n", boundary);

    HttpPost(CRASH_SUBMIT_SERVER, CRASH_SUBMIT_PORT, CRASH_SUBMIT_URL, &headers, &data);
    plogf("SendCrashInfo() finished");
}

// We might have symbol files for older builds. If we're here, then we
// didn't get the symbols so we assume it's because symbols didn't match
// Returns false if files were there but we couldn't delete them
static bool DeleteSymbolsIfExist() {
    bool ok1 = file::Delete(gLibMupdfPdbPath);
    bool ok2 = file::Delete(gSumatraPdfPdbPath);
    bool ok3 = file::Delete(gInstallerPdbPath);
    bool ok = ok1 && ok2 && ok3;
    if (!ok)
        plog("DeleteSymbolsIfExist() failed to delete");
    return ok;
}

#ifndef DEBUG
// static (single .exe) build
static bool UnpackStaticSymbols(const char* pdbZipPath, const char* symDir) {
    lf("UnpackStaticSymbols(): unpacking %s to dir %s", pdbZipPath, symDir);
    const char* files[2] = {"SumatraPDF.pdb", nullptr};
    bool ok = lzma::ExtractFiles(pdbZipPath, symDir, &files[0], gCrashHandlerAllocator);
    if (!ok) {
        plog("Failed to unpack SumatraPDF.pdb");
        return false;
    }
    return true;
}

// lib (.exe + libmupdf.dll) release and pre-release builds
static bool UnpackLibSymbols(const char* pdbZipPath, const char* symDir) {
    lf("UnpackLibSymbols(): unpacking %s to dir %s", pdbZipPath, symDir);
    const char* files[3] = {"libmupdf.pdb", "SumatraPDF-mupdf-dll.pdb", nullptr};
    bool ok = lzma::ExtractFiles(pdbZipPath, symDir, &files[0], gCrashHandlerAllocator);
    if (!ok) {
        plog("Failed to unpack libmupdf.pdb or SumatraPDF-mupdf-dll.pdb");
        return false;
    }
    return true;
}

static bool UnpackInstallerSymbols(const char* pdbZipPath, const char* symDir) {
    lf("UnpackInstallerSymbols(): unpacking %s to dir %s", pdbZipPath, symDir);
    const char* files[2] = {"Installer.pdb", nullptr};
    bool ok = lzma::ExtractFiles(pdbZipPath, symDir, &files[0], gCrashHandlerAllocator);
    if (!ok) {
        plog("Failed to unpack Installer.pdb");
        return false;
    }
    return true;
}
#endif

// .pdb files are stored in a .zip file on a web server. Download that .zip
// file as pdbZipPath, extract the symbols relevant to our executable
// to symDir directory.
// Returns false if downloading or extracting failed
// note: to simplify callers, it could choose pdbZipPath by itself (in a temporary
// directory) as the file is deleted on exit anyway
static bool DownloadAndUnzipSymbols(const WCHAR* pdbZipPath, const WCHAR* symDir) {
    lf("DownloadAndUnzipSymbols() started");
    if (!symDir || !dir::Exists(symDir)) {
        plog("DownloadAndUnzipSymbols(): exiting because symDir doesn't exist");
        return false;
    }

    if (!DeleteSymbolsIfExist()) {
        plog("DownloadAndUnzipSymbols(): DeleteSymbolsIfExist() failed");
        return false;
    }

    if (!file::Delete(pdbZipPath)) {
        plog("DownloadAndUnzipSymbols(): deleting pdbZipPath failed");
        return false;
    }

#ifdef DEBUG
    // don't care about debug builds because we don't release them
    plog("DownloadAndUnzipSymbols(): DEBUG build so not doing anything");
    return false;
#else
    if (!HttpGetToFile(gSymbolsUrl, pdbZipPath)) {
        plog("DownloadAndUnzipSymbols(): couldn't download symbols");
        return false;
    }

    char pdbZipPathUtf[512];
    char symDirUtf[512];

    str::WcharToUtf8Buf(pdbZipPath, pdbZipPathUtf, sizeof(pdbZipPathUtf));
    str::WcharToUtf8Buf(symDir, symDirUtf, sizeof(symDirUtf));

    bool ok = false;
    if (ExeSumatraStatic == gExeType) {
        ok = UnpackStaticSymbols(pdbZipPathUtf, symDirUtf);
    } else if (ExeSumatraLib == gExeType) {
        ok = UnpackLibSymbols(pdbZipPathUtf, symDirUtf);
    } else if (ExeInstaller == gExeType) {
        ok = UnpackInstallerSymbols(pdbZipPathUtf, symDirUtf);
    } else {
        plog("DownloadAndUnzipSymbols(): unknown exe type");
    }

    file::Delete(pdbZipPath);
    return ok;
#endif
}

// If we can't resolve the symbols, we assume it's because we don't have symbols
// so we'll try to download them and retry. If we can resolve symbols, we'll
// get the callstacks etc. and submit to our server for analysis.
void SubmitCrashInfo() {
    if (!dir::Create(gSymbolsDir)) {
        plog("SubmitCrashInfo(): couldn't create symbols dir");
        return;
    }

    lf("SubmitCrashInfo(): start");
    lf(L"SubmitCrashInfo(): gSymbolPathW: '%s'", gSymbolPathW);
    if (!CrashHandlerCanUseNet()) {
        plog("SubmitCrashInfo(): internet access not allowed");
        return;
    }

    char* s = nullptr;
    if (!dbghelp::Initialize(gSymbolPathW, false)) {
        plog("SubmitCrashInfo(): dbghelp::Initialize() failed");
        return;
    }

    if (!dbghelp::HasSymbols()) {
        if (!DownloadAndUnzipSymbols(gPdbZipPath, gSymbolsDir)) {
            plog("SubmitCrashInfo(): failed to download symbols");
            return;
        }

        if (!dbghelp::Initialize(gSymbolPathW, true)) {
            plog("SubmitCrashInfo(): second dbghelp::Initialize() failed");
            return;
        }
    }

    if (!dbghelp::HasSymbols()) {
        plog("SubmitCrashInfo(): HasSymbols() false after downloading symbols");
        return;
    }

    s = BuildCrashInfoText();
    if (!s)
        return;
    SendCrashInfo(s);
    gCrashHandlerAllocator->Free(s);
}

static DWORD WINAPI CrashDumpThread(LPVOID data) {
    UNUSED(data);
    WaitForSingleObject(gDumpEvent, INFINITE);
    if (!gCrashed)
        return 0;

#ifndef HAS_NO_SYMBOLS
    SubmitCrashInfo();
#endif
    // always write a MiniDump (for the latest crash only)
    // set the SUMATRAPDF_FULLDUMP environment variable for more complete dumps
    DWORD n = GetEnvironmentVariableA("SUMATRAPDF_FULLDUMP", nullptr, 0);
    bool fullDump = (0 != n);
    dbghelp::WriteMiniDump(gCrashDumpPath, &gMei, fullDump);
    return 0;
}

static LONG WINAPI DumpExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (!exceptionInfo || (EXCEPTION_BREAKPOINT == exceptionInfo->ExceptionRecord->ExceptionCode))
        return EXCEPTION_CONTINUE_SEARCH;

    static bool wasHere = false;
    if (wasHere)
        return EXCEPTION_CONTINUE_SEARCH; // Note: or should TerminateProcess()?
    wasHere = true;
    gCrashed = true;

    gMei.ThreadId = GetCurrentThreadId();
    gMei.ExceptionPointers = exceptionInfo;
    // per msdn (which is backed by my experience), MiniDumpWriteDump() doesn't
    // write callstack for the calling thread correctly. We use msdn-recommended
    // work-around of spinning a thread to do the writing
    SetEvent(gDumpEvent);
    WaitForSingleObject(gDumpThread, INFINITE);

    ShowCrashHandlerMessage();
    TerminateProcess(GetCurrentProcess(), 1);

    return EXCEPTION_CONTINUE_SEARCH;
}

static const char* OsNameFromVer(OSVERSIONINFOEX ver) {
    if (VER_PLATFORM_WIN32_NT != ver.dwPlatformId)
        return "9x";
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 3)
        return "8.1"; // or Server 2012 R2
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 2)
        return "8"; // or Server 2012
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 1)
        return "7"; // or Server 2008 R2
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 0)
        return "Vista"; // or Server 2008
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 2)
        return "Server 2003";
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 1)
        return "XP";
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 0)
        return "2000";
    if (ver.dwMajorVersion == 10) {
        // ver.dwMinorVersion seems to always be 0
        return "10";
    }

    // either a newer or an older NT version, neither of which we support
    static char osVerStr[32];
    wsprintfA(osVerStr, "NT %u.%u", ver.dwMajorVersion, ver.dwMinorVersion);
    return osVerStr;
}

static void GetOsVersion(str::Str<char>& s) {
    OSVERSIONINFOEX ver = {0};
    ver.dwOSVersionInfoSize = sizeof(ver);
#pragma warning(push)
#pragma warning(disable : 4996)  // 'GetVersionEx': was declared deprecated
#pragma warning(disable : 28159) // Consider using 'IsWindows*' instead of 'GetVersionExW'
    // see: https://msdn.microsoft.com/en-us/library/windows/desktop/dn424972(v=vs.85).aspx
    // starting with Windows 8.1, GetVersionEx will report a wrong version number
    // unless the OS's GUID has been explicitly added to the compatibility manifest
    BOOL ok = GetVersionEx((OSVERSIONINFO*)&ver);
#pragma warning(pop)
    if (!ok)
        return;
    const char* os = OsNameFromVer(ver);
    int servicePackMajor = ver.wServicePackMajor;
    int servicePackMinor = ver.wServicePackMinor;
    int buildNumber = ver.dwBuildNumber & 0xFFFF;
#ifdef _WIN64
    const char* arch = "64-bit";
#else
    const char* arch = IsRunningInWow64() ? "Wow64" : "32-bit";
#endif
    if (0 == servicePackMajor)
        s.AppendFmt("OS: Windows %s build %d %s\r\n", os, buildNumber, arch);
    else if (0 == servicePackMinor)
        s.AppendFmt("OS: Windows %s SP%d build %d %s\r\n", os, servicePackMajor, buildNumber, arch);
    else
        s.AppendFmt("OS: Windows %s %d.%d build %d %s\r\n", os, servicePackMajor, servicePackMinor, buildNumber, arch);
}

static void GetProcessorName(str::Str<char>& s) {
    WCHAR* name =
        ReadRegStr(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor", L"ProcessorNameString");
    if (!name) // if more than one processor
        name = ReadRegStr(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                          L"ProcessorNameString");
    if (!name)
        return;

    OwnedData tmp(str::conv::ToUtf8(name));
    s.AppendFmt("Processor: %s\r\n", tmp.Get());
    free(name);
}

static void GetMachineName(str::Str<char>& s) {
    WCHAR* s1 = ReadRegStr(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemFamily");
    WCHAR* s2 = ReadRegStr(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemVersion");
    OwnedData s1u;
    if (s1) {
        s1u = std::move(str::conv::ToUtf8(s1));
    }
    OwnedData s2u;
    if (s2) {
        s2u = std::move(str::conv::ToUtf8(s2));
    }

    if (!s1u.Get() && !s2u.Get())
        ; // pass
    else if (!s1u.Get())
        s.AppendFmt("Machine: %s\r\n", s2u.Get());
    else if (!s2u.Get() || str::EqI(s1u.Get(), s2u.Get()))
        s.AppendFmt("Machine: %s\r\n", s1u.Get());
    else
        s.AppendFmt("Machine: %s %s\r\n", s1u.Get(), s2u.Get());

    free(s1);
    free(s2);
}

#define GFX_DRIVER_KEY_FMT L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\%04d"

static void GetGraphicsDriverInfo(str::Str<char>& s) {
    // the info is in registry in:
    // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000\
    //   Device Description REG_SZ (same as DriverDesc, so we don't read it)
    //   DriverDesc REG_SZ
    //   DriverVersion REG_SZ
    //   UserModeDriverName REG_MULTI_SZ
    //
    // There can be more than one driver, they are in 0000, 0001 etc.
    for (int i = 0;; i++) {
        AutoFreeW key(str::Format(GFX_DRIVER_KEY_FMT, i));
        AutoFreeW v1(ReadRegStr(HKEY_LOCAL_MACHINE, key, L"DriverDesc"));
        // I assume that if I can't read the value, there are no more drivers
        if (!v1)
            break;
        OwnedData v1a(str::conv::ToUtf8(v1));
        s.AppendFmt("Graphics driver %d\r\n", i);
        s.AppendFmt("  DriverDesc:         %s\r\n", v1.Get());
        v1.Set(ReadRegStr(HKEY_LOCAL_MACHINE, key, L"DriverVersion"));
        if (v1) {
            v1a.TakeOwnership(str::conv::ToUtf8(v1).StealData());
            s.AppendFmt("  DriverVersion:      %s\r\n", v1a.Get());
        }
        v1.Set(ReadRegStr(HKEY_LOCAL_MACHINE, key, L"UserModeDriverName"));
        if (v1) {
            v1a.TakeOwnership(str::conv::ToUtf8(v1).StealData());
            s.AppendFmt("  UserModeDriverName: %s\r\n", v1a.Get());
        }
    }
}

static void GetLanguage(str::Str<char>& s) {
    char country[32] = {0}, lang[32] = {0};
    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, dimof(country) - 1);
    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, dimof(lang) - 1);
    s.AppendFmt("Lang: %s %s\r\n", lang, country);
}

static void GetSystemInfo(str::Str<char>& s) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s.AppendFmt("Number Of Processors: %d\r\n", si.dwNumberOfProcessors);
    GetProcessorName(s);

    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    float physMemGB = (float)ms.ullTotalPhys / (float)(1024 * 1024 * 1024);
    float totalPageGB = (float)ms.ullTotalPageFile / (float)(1024 * 1024 * 1024);
    DWORD usedPerc = ms.dwMemoryLoad;
    s.AppendFmt("Physical Memory: %.2f GB\r\nCommit Charge Limit: %.2f GB\r\nMemory Used: %d%%\r\n", physMemGB,
                totalPageGB, usedPerc);

    GetMachineName(s);
    GetLanguage(s);
    GetGraphicsDriverInfo(s);

    // Note: maybe more information, like:
    // * processor capabilities (mmx, sse, sse2 etc.)
}

// returns true if running on wine (winex11.drv is present)
// it's not a logical, but convenient place to do it
static bool GetModules(str::Str<char>& s) {
    bool isWine = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE)
        return true;

    MODULEENTRY32 mod;
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        OwnedData nameA(str::conv::ToUtf8(mod.szModule));
        if (str::EqI(nameA.Get(), "winex11.drv"))
            isWine = true;
        OwnedData pathA(str::conv::ToUtf8(mod.szExePath));
        s.AppendFmt("Module: %p %06X %-16s %s\r\n", mod.modBaseAddr, mod.modBaseSize, nameA.Get(), pathA.Get());
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
    return isWine;
}

// returns true if running on wine
static bool BuildModulesInfo() {
    str::Str<char> s(1024);
    bool isWine = GetModules(s);
    gModulesInfo = s.StealData();
    return isWine;
}

static void BuildSystemInfo() {
    str::Str<char> s(1024);
    GetProgramInfo(s);
    GetOsVersion(s);
    GetSystemInfo(s);
    gSystemInfo = s.StealData();
}

static bool StoreCrashDumpPaths(const WCHAR* symDir) {
    if (!symDir)
        return false;
    gSymbolsDir = str::Dup(symDir);
    gPdbZipPath = path::Join(symDir, L"symbols_tmp.zip");
    gLibMupdfPdbPath = path::Join(symDir, L"SumatraPDF.pdb");
    gSumatraPdfPdbPath = path::Join(symDir, L"libmupdf.pdb");
    gInstallerPdbPath = path::Join(symDir, L"Installer.pdb");
    return true;
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
static bool BuildSymbolPath() {
    str::Str<WCHAR> path(1024);

#if 0
    WCHAR buf[512];
    DWORD res = GetEnvironmentVariable(L"_NT_SYMBOL_PATH", buf, dimof(buf));
    if (0 < res && res < dimof(buf)) {
        path.Append(buf);
        path.Append(L";");
    }
    res = GetEnvironmentVariable(L"_NT_ALTERNATE_SYMBOL_PATH", buf, dimof(buf));
    if (0 < res && res < dimof(buf)) {
        path.Append(buf);
        path.Append(L";");
    }
#endif

    path.Append(gSymbolsDir);
    // path.Append(L";");
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
    AutoFreeW exePath(GetExePath());
    path.Append(exePath);
#endif
    gSymbolPathW = path.StealData();
    if (!gSymbolPathW)
        return false;
    return true;
}

// Get url for file with symbols. Caller needs to free().
static WCHAR* BuildSymbolsUrl() {
#ifdef SYMBOL_DOWNLOAD_URL
    return str::Dup(SYMBOL_DOWNLOAD_URL);
#else
#ifdef SVN_PRE_RELEASE_VER
    WCHAR* urlBase =
        L"https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-" TEXT(QM(SVN_PRE_RELEASE_VER));
#else
    WCHAR* urlBase = L"https://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-" TEXT(QM(CURR_VERSION));
#endif
    WCHAR* is64 = IsProcess64() ? L"-64" : L"";
    return str::Format(L"%s%s.pdb.lzsa", urlBase, is64);
#endif
}

// detect which exe it is (installer, sumatra static or sumatra with dlls)
static ExeType DetectExeType() {
    ExeType exeType = ExeSumatraStatic;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) {
        plog("DetectExeType(): failed to detect type");
        return exeType;
    }
    MODULEENTRY32 mod;
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        WCHAR* name = mod.szModule;
        if (str::EqI(name, L"libmupdf.dll")) {
            exeType = ExeSumatraLib;
            break;
        }
        if (str::StartsWithI(name, L"SumatraPDF-") && str::EndsWithI(name, L"install.exe")) {
            exeType = ExeInstaller;
            break;
        }
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
    return exeType;
}

void __cdecl onSignalAbort(int code) {
    UNUSED(code);
    // put the signal back because can be called many times
    // (from multiple threads) and raise() resets the handler
    signal(SIGABRT, onSignalAbort);
    CrashMe();
}

void onTerminate() {
    CrashMe();
}

void onUnexpected() {
    CrashMe();
}

// shadow crt's _purecall() so that we're called instead of CRT
int __cdecl _purecall() {
    CrashMe();
    return 0;
}

void InstallCrashHandler(const WCHAR* crashDumpPath, const WCHAR* symDir) {
    AssertCrash(!gDumpEvent && !gDumpThread);

    if (!crashDumpPath)
        return;
    if (!StoreCrashDumpPaths(symDir))
        return;
    if (!BuildSymbolPath())
        return;

    // don't bother sending crash reports when running under Wine
    // as they're not helpful
    bool isWine = BuildModulesInfo();
    if (isWine)
        return;

    BuildSystemInfo();
    // at this point list of modules should be complete (except
    // dbghlp.dll which shouldn't be loaded yet)

    gExeType = DetectExeType();
    // we pre-allocate as much as possible to minimize allocations
    // when crash handler is invoked. It's ok to use standard
    // allocation functions here.
    gCrashHandlerAllocator = new CrashHandlerAllocator();
    gSymbolsUrl = BuildSymbolsUrl();
    gCrashDumpPath = str::Dup(crashDumpPath);
    gDumpEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!gDumpEvent)
        return;
    gDumpThread = CreateThread(nullptr, 0, CrashDumpThread, nullptr, 0, 0);
    if (!gDumpThread)
        return;
    gPrevExceptionFilter = SetUnhandledExceptionFilter(DumpExceptionHandler);

    signal(SIGABRT, onSignalAbort);
#if COMPILER_MSVC
    ::set_terminate(onTerminate);
    ::set_unexpected(onUnexpected);
#endif
}

void UninstallCrashHandler() {
    if (!gDumpEvent || !gDumpThread)
        return;

    if (gPrevExceptionFilter)
        SetUnhandledExceptionFilter(gPrevExceptionFilter);

    SetEvent(gDumpEvent);
    WaitForSingleObject(gDumpThread, 1000); // 1 sec

    CloseHandle(gDumpThread);
    CloseHandle(gDumpEvent);

    free(gCrashDumpPath);
    free(gSymbolsUrl);
    free(gSymbolsDir);
    free(gPdbZipPath);
    free(gLibMupdfPdbPath);
    free(gSumatraPdfPdbPath);
    free(gInstallerPdbPath);

    free(gSymbolPathW);
    free(gSystemInfo);
    free(gModulesInfo);
    delete gCrashHandlerAllocator;
}

/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
#include "utils/Log.h"

#include "DisplayMode.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "AppUtil.h"
#include "CrashHandler.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "AppPrefs.h"

// TODO: when gReducedLogging is set to true, logf() no longer log
// decide if will risk it and enable logf() calls or convert
// logf() into a series of log() calls

#if !defined(CRASH_SUBMIT_SERVER) || !defined(CRASH_SUBMIT_URL)
#define CRASH_SUBMIT_SERVER L"updatecheck.io"
#define CRASH_SUBMIT_PORT 443

#define CRASH_SUBMIT_URL L"/uploadfile/sumatrapdf-crashes"
#endif

// The following functions allow crash handler to be used by both installer
// and sumatra proper. They must be implemented for each app.
extern void GetStressTestInfo(str::Str* s);
extern bool CrashHandlerCanUseNet();
extern void ShowCrashHandlerMessage();
extern void GetProgramInfo(str::Str& s);

// in DEBUG we don't enable symbols download because they are not uploaded
#if defined(DEBUG)
static bool gDisableSymbolsDownload = true;
#else
static bool gDisableSymbolsDownload = false;
#endif

#define DLURLBASE L"https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/"

// Get url for file with symbols. Caller needs to free().
static WCHAR* BuildSymbolsUrl() {
    const WCHAR* urlBase = nullptr;
    if (gIsPreReleaseBuild) {
        urlBase = DLURLBASE "prerel/SumatraPDF-prerel-" TEXT(QM(PRE_RELEASE_VER));
    } else {
        // assuming this is release vers
        urlBase = DLURLBASE "rel/SumatraPDF-" TEXT(QM(CURR_VERSION));
    }
    const WCHAR* is64 = IsProcess64() ? L"-64" : L"";
    return str::Format(L"%s%s.pdb.lzsa", urlBase, is64);
}

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

static HeapAllocator* gCrashHandlerAllocator = nullptr;

// Note: intentionally not using ScopedMem<> to avoid
// static initializers/destructors, which are bad
static WCHAR* gSymbolsUrl = nullptr;
static WCHAR* gCrashDumpPath = nullptr;
static WCHAR* gSymbolPathW = nullptr;
static WCHAR* gSymbolsDir = nullptr;
static WCHAR* gLibMupdfPdbPath = nullptr;
static WCHAR* gSumatraPdfDllPdbPath = nullptr;
static WCHAR* gSumatraPdfPdbPath = nullptr;
static char* gSystemInfo = nullptr;
static char* gSettingsFile = nullptr;
static char* gModulesInfo = nullptr;
static HANDLE gDumpEvent = nullptr;
static HANDLE gDumpThread = nullptr;
static bool isDllBuild = false;
static bool gCrashed = false;
WCHAR* gCrashFilePath = nullptr;

static MINIDUMP_EXCEPTION_INFORMATION gMei = {0};
static LPTOP_LEVEL_EXCEPTION_FILTER gPrevExceptionFilter = nullptr;

// returns true if running on wine (winex11.drv is present)
// it's not a logical, but convenient place to do it
static bool GetModules(str::Str& s, bool additionalOnly) {
    bool isWine = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) {
        return true;
    }

    MODULEENTRY32 mod;
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        auto nameA = ToUtf8Temp(mod.szModule);
        if (str::EqI(nameA.Get(), "winex11.drv")) {
            isWine = true;
        }
        auto pathA = ToUtf8Temp(mod.szExePath);
        if (additionalOnly && gModulesInfo) {
            auto pos = str::FindI(gModulesInfo, pathA);
            if (!pos) {
                s.AppendFmt("Module: %p %06X %-16s %s\n", mod.modBaseAddr, mod.modBaseSize, nameA.Get(), pathA.Get());
            }
        } else {
            s.AppendFmt("Module: %p %06X %-16s %s\n", mod.modBaseAddr, mod.modBaseSize, nameA.Get(), pathA.Get());
        }
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
    return isWine;
}

static std::string_view BuildCrashInfoText(bool forCrash) {
    str::Str s(16 * 1024, gCrashHandlerAllocator);
    if (!forCrash) {
        s.Append("Type: deubg report (not crash)\n");
    }
    if (gSystemInfo) {
        s.Append(gSystemInfo);
    }

    GetStressTestInfo(&s);
    s.Append("\n");

    if (forCrash) {
        dbghelp::GetExceptionInfo(s, gMei.ExceptionPointers);
    } else {
        // This is not a crash but debug report, we don't have an exception
        s.Append("\r\nCrashed thread:\r\n");
        dbghelp::GetCurrentThreadCallstack(s);
    }

    dbghelp::GetAllThreadsCallstacks(s);
    s.Append("\n");
    s.Append(gModulesInfo);
    s.Append("\nModules loaded later:\n");
    GetModules(s, true);

    s.Append("\n\n-------- Log -----------------\n\n");
    s.AppendView(gLogBuf->AsView());

    if (gSettingsFile) {
        s.Append("\n\n----- Settings file ----------\n\n");
        s.Append(gSettingsFile);
    }

    return s.StealAsView();
}

static void SaveCrashInfo(std::span<u8> d) {
    if (!gCrashFilePath) {
        return;
    }
    file::WriteFile(gCrashFilePath, d);
}

static void SendCrashInfo(std::span<u8> d) {
    log("SendCrashInfo()\n");
    if (d.empty()) {
        return;
    }

    str::Str headers(256, gCrashHandlerAllocator);
    headers.AppendFmt("Content-Type: text/plain");

    str::Str data(16 * 1024, gCrashHandlerAllocator);
    data.AppendSpan(d);

    HttpPost(CRASH_SUBMIT_SERVER, CRASH_SUBMIT_PORT, CRASH_SUBMIT_URL, &headers, &data);
}

// We might have symbol files for older builds. If we're here, then we
// didn't get the symbols so we assume it's because symbols didn't match
// Returns false if files were there but we couldn't delete them
static void DeleteSymbolsIfExist() {
    // TODO: remove all files in symDir (symbols, previous crash files
    bool ok = file::Delete(gLibMupdfPdbPath);
    logf(L"DeleteSymbolsIfExist: deleted '%s' (%d)\n", gLibMupdfPdbPath, (int)ok);
    ok = file::Delete(gSumatraPdfPdbPath);
    logf(L"DeleteSymbolsIfExist: deleted '%s' (%d)\n", gSumatraPdfPdbPath, (int)ok);
    ok = file::Delete(gSumatraPdfDllPdbPath);
    logf(L"DeleteSymbolsIfExist: deleted '%s' (%d)\n", gSumatraPdfDllPdbPath, (int)ok);
}

static bool ExtractSymbols(const u8* archiveData, size_t dataSize, char* dstDir, Allocator* allocator) {
    logf("ExtractSymbols: dir '%s', size: %d\n", dstDir, (int)dataSize);
    lzma::SimpleArchive archive;
    bool ok = ParseSimpleArchive(archiveData, dataSize, &archive);
    if (!ok) {
        logf("ExtractSymbols: ParseSimpleArchive failed\n");
        return false;
    }

    for (int i = 0; i < archive.filesCount; i++) {
        lzma::FileInfo* fi = &(archive.files[i]);
        const char* name = fi->name;
        logf("ExtractSymbols: file %d is '%s'\n", i, name);
        u8* uncompressed = GetFileDataByIdx(&archive, i, allocator);
        if (!uncompressed) {
            return false;
        }
        char* filePath = path::Join(dstDir, name, allocator);
        if (!filePath) {
            return false;
        }
        std::span<u8> d = {uncompressed, fi->uncompressedSize};
        ok = file::WriteFile(filePath, d);

        Allocator::Free(allocator, filePath);
        Allocator::Free(allocator, uncompressed);
        if (!ok) {
            logf("ExtractSymbols: failed to write '%s'\n", filePath);
            return false;
        }
    }
    return ok;
}

// .pdb files are stored in a .zip file on a web server. Download that .zip
// file as pdbZipPath, extract the symbols relevant to our executable
// to symDir directory.
// Returns false if downloading or extracting failed
// note: to simplify callers, it could choose pdbZipPath by itself (in a temporary
// directory) as the file is deleted on exit anyway
static bool DownloadAndUnzipSymbols(const WCHAR* symDir) {
    if (gDisableSymbolsDownload) {
        // don't care about debug builds because we don't release them
        log("DownloadAndUnzipSymbols: DEBUG build so not doing anything\n");
        return false;
    }

    logf(L"DownloadAndUnzipSymbols: symDir: '%s', url: '%s'\n", symDir, gSymbolsUrl);
    if (!symDir || !dir::Exists(symDir)) {
        log("DownloadAndUnzipSymbols: exiting because symDir doesn't exist\n");
        return false;
    }

    DeleteSymbolsIfExist();

    HttpRsp rsp;
    if (!HttpGet(gSymbolsUrl, &rsp)) {
        log("DownloadAndUnzipSymbols: couldn't download symbols\n");
        return false;
    }
    if (!HttpRspOk(&rsp)) {
        log("DownloadAndUnzipSymbols: HttpRspOk() returned false\n");
    }

    auto symDirUtf = ToUtf8Temp(symDir);
    bool ok = ExtractSymbols((const u8*)rsp.data.Get(), rsp.data.size(), symDirUtf, gCrashHandlerAllocator);
    if (!ok) {
        log("DownloadAndUnzipSymbols: ExtractSymbols() failed\n");
    }
    return ok;
}

bool CrashHandlerDownloadSymbols() {
    log("CrashHandlerDownloadSymbols()\n");
    if (!dir::Create(gSymbolsDir)) {
        log("CrashHandlerDownloadSymbols: couldn't create symbols dir\n");
        return false;
    }

    if (!dbghelp::Initialize(gSymbolPathW, false)) {
        log("CrashHandlerDownloadSymbols: dbghelp::Initialize() failed\n");
        return false;
    }

    if (dbghelp::HasSymbols()) {
        log("CrashHandlerDownloadSymbols(): skipping because dbghelp::HasSymbols()\n");
        return true;
    }

    if (!DownloadAndUnzipSymbols(gSymbolsDir)) {
        log("CrashHandlerDownloadSymbols: failed to download symbols\n");
        return false;
    }

    if (!dbghelp::Initialize(gSymbolPathW, true)) {
        log("CrashHandlerDownloadSymbols: second dbghelp::Initialize() failed\n");
        return false;
    }

    if (!dbghelp::HasSymbols()) {
        log("CrashHandlerDownloadSymbols: HasSymbols() false after downloading symbols, gSymbolPathW:");
        log(gSymbolPathW);
        log("\n");
        return false;
    }
    return true;
}

// like crash report, but can be triggered without a crash
void _submitDebugReport(const char* condStr) {
    if (!CrashHandlerCanUseNet()) {
        return;
    }

    logf(L"_submitDebugReport: gSymbolPathW: '%s'\n", gSymbolPathW);

    bool ok = CrashHandlerDownloadSymbols();
    if (!ok) {
        log("_submitDebugReport(): CrashHandlerDownloadSymbols() failed\n");
        return;
    }

    auto sv = BuildCrashInfoText(false);
    if (sv.empty()) {
        log("_submitDebugReport(): skipping because !BuildCrashInfoText()\n");
        return;
    }
    auto d = ToSpanU8(sv);
    // SaveCrashInfo(d);
    SendCrashInfo(d);
    // gCrashHandlerAllocator->Free((const void*)d.data());
    log("_submitDebugReport() finished\n");
}

// we want to avoid submitting multiple reports for the same
// condition. I'm too lazy to implement tracking this granuarly
// so only allow once submition in a given session
static bool didSubmitDebugReport = false;

void _submitDebugReportIfFunc(bool cond, __unused const char* condStr) {
    if (!cond || didSubmitDebugReport) {
        return;
    }
    didSubmitDebugReport = true;
#if defined(PRE_RELEASE_VER)
    _submitDebugReport(condStr);
#endif
}

// If we can't resolve the symbols, we assume it's because we don't have symbols
// so we'll try to download them and retry. If we can resolve symbols, we'll
// get the callstacks etc. and submit to our server for analysis.
void SubmitCrashReport() {
    log("SubmitCrashReport()\n");
    if (!CrashHandlerCanUseNet()) {
        log("SubmitCrashReport(): skipping because !CrashHandlerCanUseNet()\n");
        return;
    }

    logf(L"SubmitCrashReport: gSymbolPathW: '%s'\n", gSymbolPathW);

    bool ok = CrashHandlerDownloadSymbols();
    if (!ok) {
        log("SubmitCrashReport(): CrashHandlerDownloadSymbols() failed\n");
    }

    auto sv = BuildCrashInfoText(true);
    if (sv.empty()) {
        log("SubmitCrashReport(): skipping because !BuildCrashInfoText()\n");
        return;
    }
    auto d = ToSpanU8(sv);
    SaveCrashInfo(d);
    SendCrashInfo(d);
    // gCrashHandlerAllocator->Free((const void*)d.data());
    log("SubmitCrashReport() finished\n");
}

static DWORD WINAPI CrashDumpThread(__unused LPVOID data) {
    WaitForSingleObject(gDumpEvent, INFINITE);
    if (!gCrashed) {
        return 0;
    }

    SubmitCrashReport();

    // always write a MiniDump (for the latest crash only)
    // set the SUMATRAPDF_FULLDUMP environment variable for more complete dumps
    DWORD n = GetEnvironmentVariableA("SUMATRAPDF_FULLDUMP", nullptr, 0);
    bool fullDump = (0 != n);
    dbghelp::WriteMiniDump(gCrashDumpPath, &gMei, fullDump);
    return 0;
}

static LONG WINAPI DumpExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (!exceptionInfo || (EXCEPTION_BREAKPOINT == exceptionInfo->ExceptionRecord->ExceptionCode)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    gReducedLogging = true;

    log("DumpExceptionHandler\n");
    static bool wasHere = false;
    if (wasHere) {
        log("DumpExceptionHandler: wasHere set\n");
        return EXCEPTION_CONTINUE_SEARCH; // Note: or should TerminateProcess()?
    }

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

static const char* OsNameFromVer(const OSVERSIONINFOEX& ver) {
    if (VER_PLATFORM_WIN32_NT != ver.dwPlatformId) {
        return "9x";
    }
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 3) {
        return "8.1"; // or Server 2012 R2
    }
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 2) {
        return "8"; // or Server 2012
    }
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 1) {
        return "7"; // or Server 2008 R2
    }
    if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 0) {
        return "Vista"; // or Server 2008
    }
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 2) {
        return "Server 2003";
    }
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 1) {
        return "XP";
    }
    if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 0) {
        return "2000";
    }
    if (ver.dwMajorVersion == 10) {
        // ver.dwMinorVersion seems to always be 0
        return "10";
    }

    // either a newer or an older NT version, neither of which we support
    static char osVerStr[32];
    wsprintfA(osVerStr, "NT %u.%u", ver.dwMajorVersion, ver.dwMinorVersion);
    return osVerStr;
}

static void GetOsVersion(str::Str& s) {
    OSVERSIONINFOEX ver = {0};
    ver.dwOSVersionInfoSize = sizeof(ver);
#pragma warning(push)
#pragma warning(disable : 4996)  // 'GetVersionEx': was declared deprecated
#pragma warning(disable : 28159) // Consider using 'IsWindows*' instead of 'GetVersionExW'
    // see: https://msdn.microsoft.com/en-us/library/windows/desktop/dn424972(v=vs.85).aspx
    // starting with Windows 8.1, GetVersionEx will report a wrong version number
    // unless the OS's GUID has been explicitly added to the compatibility manifest
    BOOL ok = GetVersionExW((OSVERSIONINFO*)&ver); // NOLINT
#pragma warning(pop)
    if (!ok) {
        return;
    }

    const char* os = OsNameFromVer(ver);
    int servicePackMajor = ver.wServicePackMajor;
    int servicePackMinor = ver.wServicePackMinor;
    int buildNumber = ver.dwBuildNumber & 0xFFFF;
#ifdef _WIN64
    const char* arch = "64-bit";
#else
    const char* arch = IsRunningInWow64() ? "Wow64" : "32-bit";
#endif
    if (0 == servicePackMajor) {
        s.AppendFmt("OS: Windows %s build %d %s\n", os, buildNumber, arch);
    } else if (0 == servicePackMinor) {
        s.AppendFmt("OS: Windows %s SP%d build %d %s\n", os, servicePackMajor, buildNumber, arch);
    } else {
        s.AppendFmt("OS: Windows %s %d.%d build %d %s\n", os, servicePackMajor, servicePackMinor, buildNumber, arch);
    }
}

static void GetProcessorName(str::Str& s) {
    auto key = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor";
    char* name = ReadRegStrUtf8(HKEY_LOCAL_MACHINE, key, L"ProcessorNameString");
    if (!name) {
        // if more than one processor
        key = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
        name = ReadRegStrUtf8(HKEY_LOCAL_MACHINE, key, L"ProcessorNameString");
    }
    if (name) {
        s.AppendFmt("Processor: %s\n", name);
        free(name);
    }
}

static void GetMachineName(str::Str& s) {
    char* s1 = ReadRegStrUtf8(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemFamily");
    char* s2 = ReadRegStrUtf8(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemVersion");

    if (!s1 && !s2) {
        // no-op
    } else if (!s1) {
        s.AppendFmt("Machine: %s\n", s2);
    } else if (!s2 || str::EqI(s1, s2)) {
        s.AppendFmt("Machine: %s\n", s1);
    } else {
        s.AppendFmt("Machine: %s %s\n", s1, s2);
    }
    free(s2);
    free(s1);
}

#define GFX_DRIVER_KEY_FMT L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\%04d"

static void GetGraphicsDriverInfo(str::Str& s) {
    // the info is in registry in:
    // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000\
    //   Device Description REG_SZ (same as DriverDesc, so we don't read it)
    //   DriverDesc REG_SZ
    //   DriverVersion REG_SZ
    //   UserModeDriverName REG_MULTI_SZ
    //
    // There can be more than one driver, they are in 0000, 0001 etc.
    for (int i = 0;; i++) {
        AutoFreeWstr key(str::Format(GFX_DRIVER_KEY_FMT, i));
        char* v = ReadRegStrUtf8(HKEY_LOCAL_MACHINE, key, L"DriverDesc");
        // I assume that if I can't read the value, there are no more drivers
        if (!v) {
            break;
        }
        s.AppendFmt("Graphics driver %d\n", i);
        s.AppendFmt("  DriverDesc:         %s\n", v);
        str::Free(v);

        v = ReadRegStrUtf8(HKEY_LOCAL_MACHINE, key, L"DriverVersion");
        if (v) {
            s.AppendFmt("  DriverVersion:      %s\n", v);
            str::Free(v);
        }

        v = ReadRegStrUtf8(HKEY_LOCAL_MACHINE, key, L"UserModeDriverName");
        if (v) {
            s.AppendFmt("  UserModeDriverName: %s\n", v);
            str::Free(v);
        }
    }
}

static void GetLanguage(str::Str& s) {
    char country[32] = {0}, lang[32] = {0};
    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, dimof(country) - 1);
    GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, dimof(lang) - 1);
    s.AppendFmt("Lang: %s %s\n", lang, country);
}

static void GetSystemInfo(str::Str& s) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s.AppendFmt("Number Of Processors: %d\n", si.dwNumberOfProcessors);
    GetProcessorName(s);

    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    float physMemGB = (float)ms.ullTotalPhys / (float)(1024 * 1024 * 1024);
    float totalPageGB = (float)ms.ullTotalPageFile / (float)(1024 * 1024 * 1024);
    DWORD usedPerc = ms.dwMemoryLoad;
    s.AppendFmt("Physical Memory: %.2f GB\nCommit Charge Limit: %.2f GB\nMemory Used: %d%%\n", physMemGB, totalPageGB,
                usedPerc);

    GetMachineName(s);
    GetLanguage(s);
    GetGraphicsDriverInfo(s);

    // Note: maybe more information, like:
    // * processor capabilities (mmx, sse, sse2 etc.)
}

// returns true if running on wine
static bool BuildModulesInfo() {
    str::Str s(1024);
    bool isWine = GetModules(s, false);
    gModulesInfo = s.StealData();
    return isWine;
}

static void BuildSystemInfo() {
    str::Str s(1024);
    GetProgramInfo(s);
    GetOsVersion(s);
    GetSystemInfo(s);
    gSystemInfo = s.StealData();
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
static void BuildSymbolPath() {
    str::WStr path(1024);

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
    WCHAR* exePath = GetExePathTemp().Get();
    path.Append(L";");
    path.Append(exePath);
#endif

    free(gSymbolPathW);
    gSymbolPathW = path.StealData();
}

bool SetSymbolsDir(const WCHAR* symDir) {
    if (!symDir) {
        return false;
    }

    free(gSymbolsDir);
    free(gLibMupdfPdbPath);
    free(gSumatraPdfDllPdbPath);
    free(gSumatraPdfPdbPath);

    gSymbolsDir = str::Dup(symDir);
    gSumatraPdfPdbPath = path::Join(symDir, L"SumatraPDF.pdb");
    gSumatraPdfDllPdbPath = path::Join(symDir, L"SumatraPDF-dll.pdb");
    gLibMupdfPdbPath = path::Join(symDir, L"libmupdf.pdb");
    BuildSymbolPath();
    return true;
}

void __cdecl onSignalAbort(__unused int code) {
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

void InstallCrashHandler(const WCHAR* crashDumpPath, const WCHAR* crashFilePath, const WCHAR* symDir) {
    CrashIf(gDumpEvent || gDumpThread);

    if (!crashDumpPath) {
        log("InstallCrashHandler: skipping because !crashDumpPath\n");
        return;
    }

    if (!SetSymbolsDir(symDir)) {
        log("InstallCrashHandler: skipping because !SetSymbolsDir()\n");
        return;
    }

    logf(L"InstallCrashHandler crashDumpPath: '%s', crashFilePath: '%s', symDir: '%s'\n", crashDumpPath, crashFilePath,
         symDir);

    gCrashDumpPath = str::Dup(crashDumpPath);
    gCrashFilePath = str::Dup(crashFilePath);

    // don't bother sending crash reports when running under Wine
    // as they're not helpful
    bool isWine = BuildModulesInfo();
    if (isWine) {
        log("InstallCrashHandler: skipping because isWine\n");
        return;
    }

    isDllBuild = IsDllBuild();

    BuildSystemInfo();
    // at this point list of modules should be complete (except
    // dbghlp.dll which shouldn't be loaded yet)

    // we pre-allocate as much as possible to minimize allocations
    // when crash handler is invoked. It's ok to use standard
    // allocation functions here.
    gCrashHandlerAllocator = new HeapAllocator();
    gSymbolsUrl = BuildSymbolsUrl();

    AutoFreeWstr path = prefs::GetSettingsPath();
    // can be empty on first run but that's fine because then we know it has default values
    gSettingsFile = (char*)file::ReadFile(path).data();

    gDumpEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!gDumpEvent) {
        log("InstallCrashHandler: skipping because !gDumpEvent\n");
        return;
    }
    gDumpThread = CreateThread(nullptr, 0, CrashDumpThread, nullptr, 0, 0);
    if (!gDumpThread) {
        log("InstallCrashHandler: skipping because !gDumpThread\n");
        return;
    }
    gPrevExceptionFilter = SetUnhandledExceptionFilter(DumpExceptionHandler);

    signal(SIGABRT, onSignalAbort);
#if COMPILER_MSVC
    ::set_terminate(onTerminate);
    ::set_unexpected(onUnexpected);
#endif
}

void UninstallCrashHandler() {
    if (!gDumpEvent || !gDumpThread) {
        return;
    }

    if (gPrevExceptionFilter) {
        SetUnhandledExceptionFilter(gPrevExceptionFilter);
    }

    SetEvent(gDumpEvent);
    WaitForSingleObject(gDumpThread, 1000); // 1 sec

    CloseHandle(gDumpThread);
    CloseHandle(gDumpEvent);

    free(gCrashDumpPath);
    free(gSymbolsUrl);
    free(gSymbolsDir);
    free(gLibMupdfPdbPath);
    free(gSumatraPdfPdbPath);
    free(gSumatraPdfDllPdbPath);
    free(gCrashFilePath);

    free(gSymbolPathW);
    free(gSystemInfo);
    free(gSettingsFile);
    free(gModulesInfo);
    delete gCrashHandlerAllocator;
}

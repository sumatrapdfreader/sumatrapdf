/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "utils/BaseUtil.h"

#pragma warning(disable : 4668)
#include <signal.h>
#include <eh.h>

#include "utils/WinDynCalls.h"
#include "utils/DbgHelpDyn.h"
#include "utils/FileUtil.h"
#include "utils/HttpUtil.h"
#include "utils/LzmaSimpleArchive.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppTools.h"
#include "CrashHandler.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "AppSettings.h"

#include "utils/Log.h"

// TODO: when gReducedLogging is set to true, logf() no longer log
// decide if will risk it and enable logf() calls or convert
// logf() into a series of log() calls

#define kCrashHandlerServer "www.sumatrapdfreader.org"
#define kCrashHandlerServerPort 443
#define kCrashHandlerServerSubmitURL "/uploadcrash/sumatrapdf-crashes"

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
char* gSymbolsDir = nullptr;
char* gCrashFilePath = nullptr;

static char* gSymbolsUrl = nullptr;
static char* gCrashDumpPath = nullptr;
static char* gSymbolPath = nullptr;
static char* gSystemInfo = nullptr;
static char* gSettingsFile = nullptr;
static char* gModulesInfo = nullptr;
static HANDLE gDumpEvent = nullptr;
static HANDLE gDumpThread = nullptr;
static bool isDllBuild = false;
static bool gCrashed = false;

static MINIDUMP_EXCEPTION_INFORMATION gMei{};
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
        if (str::EqI(nameA, "winex11.drv")) {
            isWine = true;
        }
        auto pathA = ToUtf8Temp(mod.szExePath);
        if (additionalOnly && gModulesInfo) {
            auto pos = str::FindI(gModulesInfo, pathA);
            if (!pos) {
                s.AppendFmt("Module: %p %06X %-16s %s\n", mod.modBaseAddr, mod.modBaseSize, nameA, pathA);
            }
        } else {
            s.AppendFmt("Module: %p %06X %-16s %s\n", mod.modBaseAddr, mod.modBaseSize, nameA, pathA);
        }
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
    return isWine;
}

char* BuildCrashInfoText(const char* condStr, bool isCrash, bool captureCallstack) {
    str::Str s(16 * 1024, gCrashHandlerAllocator);
    if (!isCrash) {
        captureCallstack = true;
        s.Append("Type: debug report (not crash)\n");
    }
    if (condStr) {
        s.Append("Cond: ");
        s.Append(condStr);
        s.Append("\n");
    }
    if (gSystemInfo) {
        s.Append(gSystemInfo);
        s.Append("\n");
    }

    //    GetStressTestInfo(&s);

    if (gMei.ExceptionPointers) {
        // those are only set when we capture exception
        dbghelp::GetExceptionInfo(s, gMei.ExceptionPointers);
        s.Append("\n");
    } else {
        // GetExceptionInfo() also adds current thread callstack
        if (captureCallstack) {
            s.Append("\nCrashed thread:\n");
            dbghelp::GetCurrentThreadCallstack(s);
            s.Append("\n");
        }
    }

    s.Append("\n-------- Log -----------------\n\n");
    s.Append(gLogBuf->LendData());

    if (gSettingsFile) {
        s.Append("\n\n----- Settings file ----------\n\n");
        s.Append(gSettingsFile);
        s.Append("\n\n");
    }

    s.Append("\n-------- Modules   ----------\n\n");
    s.Append(gModulesInfo);
    s.Append("\nModules loaded later:\n");
    GetModules(s, true);

    if (captureCallstack) {
        s.Append("\n-------- All Threads ----------\n\n");
        dbghelp::GetAllThreadsCallstacks(s);
        s.Append("\n");
    }

    return s.StealData();
}

void SaveCrashInfo(const ByteSlice& d) {
    if (!gCrashFilePath) {
        return;
    }
    dir::CreateForFile(gCrashFilePath);
    file::WriteFile(gCrashFilePath, d);
}

void UploadCrashReport(const ByteSlice& d) {
    log("UploadCrashReport()\n");
    if (d.empty()) {
        return;
    }

    str::Str headers(256, gCrashHandlerAllocator);
    headers.AppendFmt("Content-Type: text/plain");

    str::Str data(16 * 1024, gCrashHandlerAllocator);
    data.AppendSlice(d);

    HttpPost(kCrashHandlerServer, kCrashHandlerServerPort, kCrashHandlerServerSubmitURL, &headers, &data);
}

static bool ExtractSymbols(const u8* archiveData, size_t dataSize, const char* dstDir, Allocator* allocator) {
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
        char* filePath = path::Join(allocator, dstDir, name);
        if (!filePath) {
            return false;
        }
        ByteSlice d = {uncompressed, fi->uncompressedSize};
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
static bool DownloadAndUnzipSymbols(const char* symDir) {
    if (gDisableSymbolsDownload) {
        // don't care about debug builds because we don't release them
        log("DownloadAndUnzipSymbols: DEBUG build so not doing anything\n");
        return false;
    }

    if (!dir::CreateAll(symDir)) {
        logf("CrashHandlerDownloadSymbols: couldn't create symbols dir '%s'\n", gSymbolsDir);
        return false;
    }

    logf("DownloadAndUnzipSymbols: symDir: '%s', url: '%s'\n", symDir, gSymbolsUrl);
    if (!symDir || !dir::Exists(symDir)) {
        log("DownloadAndUnzipSymbols: exiting because symDir doesn't exist\n");
        return false;
    }

    // DeleteSymbolsIfExist();

    HttpRsp rsp;
    if (!HttpGet(gSymbolsUrl, &rsp)) {
        log("DownloadAndUnzipSymbols: couldn't download symbols\n");
        return false;
    }
    if (!IsHttpRspOk(&rsp)) {
        log("DownloadAndUnzipSymbols: HttpRspOk() returned false\n");
    }

    bool ok = ExtractSymbols((const u8*)rsp.data.Get(), rsp.data.size(), symDir, gCrashHandlerAllocator);
    if (!ok) {
        log("DownloadAndUnzipSymbols: ExtractSymbols() failed\n");
    }
    return ok;
}

bool CrashHandlerDownloadSymbols() {
    return DownloadAndUnzipSymbols(gSymbolsDir);
}

bool AreSymbolsDownloaded(const char* symDir) {
    TempStr path = path::JoinTemp(symDir, "SumatraPDF.pdb");
    if (file::Exists(path)) {
        logf("AreSymbolsDownloaded(): exist in '%s', symDir: '%s'\n", path, symDir);
        return true;
    }
    TempStr exePath = GetSelfExePathTemp();
    exePath = str::ReplaceTemp(exePath, ".exe", ".pdb");
    if (file::Exists(exePath)) {
        logf("AreSymbolsDownloaded(): exist in '%s', symDir: '%s'\n", exePath, symDir);
        return true;
    }
    logf("AreSymbolsDownloaded(): not downloaded, symDir: '%s'\n", symDir);
    return false;
}

bool InitializeDbgHelp(bool force) {
    TempWStr ws = ToWStrTemp(gSymbolPath);
    if (!dbghelp::Initialize(ws, force)) {
        logf("InitializeDbgHelp: dbghelp::Initialize('%s'), force: %d failed\n", gSymbolPath, (int)force);
        return false;
    }

    if (!dbghelp::HasSymbols()) {
        logf("InitializeDbgHelp(): dbghelp::HasSymbols(), gSymbolPath: '%s' force: %d failed\n", gSymbolPath,
             (int)force);
        return false;
    }
    log("InitializeDbgHelp(): did initialize ok\n");
    return true;
}

bool DownloadSymbolsIfNeeded() {
    logf("DownloadSymbolsIfNeeded(), gSymbolsDir: '%s'\n", gSymbolsDir);
    if (!AreSymbolsDownloaded(gSymbolsDir)) {
        bool ok = CrashHandlerDownloadSymbols();
        if (!ok) {
            return false;
        }
    }
    return InitializeDbgHelp(false);
}

// like crash report, but can be triggered without a crash
void _uploadDebugReport(const char* condStr, bool isCrash, bool captureCallstack) {
    // in release builds ReportIf()/ReportIfQuick() will break if running under
    // the debugger. In other builds it sends a debug report

    bool shouldUpload = gIsDebugBuild || gIsPreReleaseBuild || gIsAsanBuild;
    if (gIsStoreBuild && !isCrash) {
        // those would probably be too frequent
        shouldUpload = false;
    }
    if (!shouldUpload) {
        if (IsDebuggerPresent()) {
            DebugBreak();
        }
        return;
    }

    // we want to avoid submitting multiple reports for the same
    // condition. I'm too lazy to implement tracking this granularly
    // so only allow once submission in a given session
    static bool didSubmitDebugReport = false;

    if (condStr) {
        logfa("_uploadDebugReport: %s\n", condStr);
    } else {
        loga("_uploadDebugReport\n");
    }

    // don't send report if this is me debugging
    if (IsDebuggerPresent()) {
        DebugBreak();
        return;
    }

    if (didSubmitDebugReport) {
        return;
    }
    didSubmitDebugReport = true;

    // no longer can happen i.e. we exclude non-crashes from release builds via #ifdef
#if 0
    if (!isCrash) {
        if (!gIsPreReleaseBuild) {
            // only enabled for pre-release builds, don't want lots of non-crash
            // reports from official release
            return;
        }

        if (gIsDebugBuild) {
            // exclude debug builds. Those are most likely other people modyfing Sumatra
            return;
        }
    }
#endif

    if (!CrashHandlerCanUseNet()) {
        return;
    }

    logfa("_uploadDebugReport: isCrash: %d, captureCallstack: %d, gSymbolPath: '%s'\n", (int)isCrash,
          (int)captureCallstack, gSymbolPath);

    if (captureCallstack) {
        // we proceed even if we fail to download symbols
        DownloadSymbolsIfNeeded();
    }

    auto s = BuildCrashInfoText(condStr, isCrash, captureCallstack);
    if (str::IsEmpty(s)) {
        loga("_uploadDebugReport(): skipping because !BuildCrashInfoText()\n");
        return;
    }

    ByteSlice d(s);
    UploadCrashReport(d);
    SaveCrashInfo(d);
    // gCrashHandlerAllocator->Free((const void*)d.data());
    loga(s);
    loga("_uploadDebugReport() finished\n");
}

static DWORD WINAPI CrashDumpThread(LPVOID) {
    WaitForSingleObject(gDumpEvent, INFINITE);
    if (!gCrashed) {
        return 0;
    }

    log("CrashDumpThread\n");
    _uploadDebugReport(nullptr, true, true);

    // always write a MiniDump (for the latest crash only)
    // set the SUMATRAPDF_FULLDUMP environment variable for more complete dumps
    DWORD n = GetEnvironmentVariableA("SUMATRAPDF_FULLDUMP", nullptr, 0);
    bool fullDump = (0 != n);
    TempWStr ws = ToWStrTemp(gCrashDumpPath);
    dbghelp::WriteMiniDump(ws, &gMei, fullDump);
    return 0;
}

// This is needed to intercept memory corruption reports from windows heap manager
// https://peteronprogramming.wordpress.com/2017/07/30/crashes-you-cant-handle-easily-3-status_heap_corruption-on-windows/
// https://phabricator.services.mozilla.com/D83753
static LONG WINAPI CrashDumpVectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (exceptionInfo->ExceptionRecord->ExceptionCode != STATUS_HEAP_CORRUPTION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    //    gReducedLogging = true;
    log("CrashDumpVectoredExceptionHandler\n");

    static bool wasHere = false;
    if (wasHere) {
        log("CrashDumpVectoredExceptionHandler: wasHere set\n");
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

static LONG WINAPI CrashDumpExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (!exceptionInfo || (EXCEPTION_BREAKPOINT == exceptionInfo->ExceptionRecord->ExceptionCode)) {
        log("CrashDumpExceptionHandler: exiting because !exceptionInfo || EXCEPTION_BREAKPOINT == "
            "exceptionInfo->ExceptionRecord->ExceptionCode\n");
        return EXCEPTION_CONTINUE_SEARCH;
    }

    //    gReducedLogging = true;
    log("CrashDumpExceptionHandler\n");

    static bool wasHere = false;
    if (wasHere) {
        log("CrashDumpExceptionHandler: wasHere set\n");
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

static void GetOsVersion(str::Str& s) {
    OSVERSIONINFOEX ver{};
    bool ok = GetOsVersion(ver);
    ver.dwOSVersionInfoSize = sizeof(ver);
    if (!ok) {
        return;
    }

    TempStr os = OsNameFromVerTemp(ver);
    int servicePackMajor = ver.wServicePackMajor;
    int servicePackMinor = ver.wServicePackMinor;
    int buildNumber = ver.dwBuildNumber & 0xFFFF;
    auto arch = "64-bit";
    if (IsProcess32()) {
        arch = IsRunningInWow64() ? "Wow64" : "32-bit";
    }
    if (0 == servicePackMajor) {
        s.AppendFmt("OS: Windows %s build %d %s\n", os, buildNumber, arch);
    } else if (0 == servicePackMinor) {
        s.AppendFmt("OS: Windows %s SP%d build %d %s\n", os, servicePackMajor, buildNumber, arch);
    } else {
        s.AppendFmt("OS: Windows %s %d.%d build %d %s\n", os, servicePackMajor, servicePackMinor, buildNumber, arch);
    }
}

static void GetProcessorName(str::Str& s) {
    auto key = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor";
    char* name = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "ProcessorNameString");
    if (!name) {
        // if more than one processor
        key = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
        name = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "ProcessorNameString");
    }
    if (name) {
        s.AppendFmt("Processor: %s\n", name);
    }
}

#define GFX_DRIVER_KEY_FMT "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\%04d"

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
        TempStr key = str::FormatTemp(GFX_DRIVER_KEY_FMT, i);
        char* v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "DriverDesc");
        // I assume that if I can't read the value, there are no more drivers
        if (!v) {
            break;
        }
        s.AppendFmt("Graphics driver %d\n", i);
        s.AppendFmt("  DriverDesc:         %s\n", v);

        v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "DriverVersion");
        if (v) {
            s.AppendFmt("  DriverVersion:      %s\n", v);
        }

        v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "UserModeDriverName");
        if (v) {
            s.AppendFmt("  UserModeDriverName: %s\n", v);
        }
    }
}

static void GetSystemInfo(str::Str& s) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s.AppendFmt("Number Of Processors: %d\n", si.dwNumberOfProcessors);
    GetProcessorName(s);

    {
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);

        float physMemGB = (float)ms.ullTotalPhys / (float)(1024 * 1024 * 1024);
        float totalPageGB = (float)ms.ullTotalPageFile / (float)(1024 * 1024 * 1024);
        DWORD usedPerc = ms.dwMemoryLoad;
        s.AppendFmt("Physical Memory: %.2f GB\nCommit Charge Limit: %.2f GB\nMemory Used: %d%%\n", physMemGB,
                    totalPageGB, usedPerc);
    }
    {
        TempStr ver = GetWebView2VersionTemp();
        if (str::IsEmpty(ver)) {
            ver = (TempStr) "no WebView2 installed";
        }
        s.AppendFmt("WebView2: %s\n", ver);
    }
    {
        // get computer name
        char* s1 = ReadRegStrTemp(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemFamily");
        char* s2 = ReadRegStrTemp(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemVersion");

        if (!s1 && !s2) {
            // no-op
        } else if (!s1) {
            s.AppendFmt("Machine: %s\n", s2);
        } else if (!s2 || str::EqI(s1, s2)) {
            s.AppendFmt("Machine: %s\n", s1);
        } else {
            s.AppendFmt("Machine: %s %s\n", s1, s2);
        }
    }
    {
        // get language
        char country[32] = {}, lang[32]{};
        GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, dimof(country) - 1);
        GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, dimof(lang) - 1);
        s.AppendFmt("Lang: %s %s\n", lang, country);
    }
    GetGraphicsDriverInfo(s);
    {
        auto cpu = CpuID();
        s.Append("CPU: ");
        if (cpu & kCpuMMX) {
            s.Append("MMX ");
        }
        if (cpu & kCpuSSE) {
            s.Append("SSE ");
        }
        if (cpu & kCpuSSE2) {
            s.Append("SSE2 ");
        }
        if (cpu & kCpuSSE3) {
            s.Append("SSE3 ");
        }
        if (cpu & kCpuSSE41) {
            s.Append("SSE41 ");
        }
        if (cpu & kCpuSSE42) {
            s.Append("SSE42 ");
        }
        if (cpu & kCpuAVX) {
            s.Append("AVX ");
        }
        if (cpu & kCpuAVX2) {
            s.Append("AVX2 ");
        }
    }

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

/*
https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/symbol-path
http://p-nand-q.com/python/procmon.html
https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-download-tools

Setting symbol path:
add GetEnvironmentVariableA("_NT_SYMBOL_PATH", ..., ...)
add GetEnvironmentVariableA("_NT_ALT_SYMBOL_PATH ", ..., ...)
add: cache*C:\MySymbols;srv*https://msdl.microsoft.com/download/symbols

dbghelp.dll should be installed with os but might be outdated
for symbols server symsrv.dll is needed, installed with debug tools for windows
*/

static bool gAddNtSymbolPath = false;
static bool gAddSymbolServer = false;
static bool gAddExeDir = false;

static void BuildSymbolPath(const char* symDir) {
    str::Str path(1024);

    bool symDirExists = dir::Exists(symDir);

    // at this point symDir might not exist but we add it anyway
    path.Append(symDir);
    path.Append(";");

    // in debug builds the symbols are in the same directory as .exe
    if (gIsDebugBuild || gAddExeDir) {
        TempStr dir = GetSelfExeDirTemp();
        path.Append(dir);
        path.Append(";");
    }

    if (gAddNtSymbolPath) {
        TempStr ntSymPath = GetEnvVariableTemp("_NT_SYMBOL_PATH");
        // internet talks about both _NT_ALT_SYMBOL_PATH and _NT_ALTERNATE_SYMBOL_PATH
        if (str::IsEmpty(ntSymPath)) {
            ntSymPath = GetEnvVariableTemp("_NT_ALT_SYMBOL_PATH");
        }
        if (str::IsEmpty(ntSymPath)) {
            ntSymPath = GetEnvVariableTemp("_NT_ALTERNATE_SYMBOL_PATH");
        }
        if (!str::IsEmpty(ntSymPath)) {
            path.Append(ntSymPath);
            path.Append(";");
        }
    }
    if (gAddSymbolServer && symDirExists) {
        // this probably won't work as it needs symsrv.dll and that's not included with Windows
        // TODO: maybe try to scan system directories for symsrv.dll and somehow add it?
        path.AppendFmt("cache*%s;srv*https://msdl.microsoft.com/download/symbols;", symDir);
    }

    // remove ";" from the end
    path.RemoveLast();

    char* p = path.CStr();
    str::ReplaceWithCopy(&gSymbolPath, p);
}

bool SetSymbolsDir(const char* symDir) {
    if (!symDir) {
        return false;
    }
    str::ReplaceWithCopy(&gSymbolsDir, symDir);
    BuildSymbolPath(gSymbolsDir);
    return true;
}

void __cdecl onSignalAbort(int) {
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

// Get url for file with symbols. Caller needs to free().
static char* BuildSymbolsUrl() {
    const char* urlBase = "https://www.sumatrapdfreader.org/dl/";
    if (gIsPreReleaseBuild) {
        const char* ver = preReleaseVersion;
        urlBase = str::JoinTemp(urlBase, "prerel/", ver, "/SumatraPDF-prerel");
    } else {
        // assuming this is release version
        const char* ver = QM(CURR_VERSION);
        urlBase = str::JoinTemp(urlBase, "rel/", ver, "/SumatraPDF-", ver);
    }
    const char* suff = "-32.pdb.lzsa";
#if IS_ARM_64 == 1
    suff = "-arm64.pdb.lzsa";
#elif IS_INTEL_64 == 1
    suff = "-64.pdb.lzsa";
#endif
    return str::Join(urlBase, suff);
}

void InstallCrashHandler(const char* crashDumpPath, const char* crashFilePath, const char* symDir) {
    ReportIf(gDumpEvent || gDumpThread);

    if (!crashDumpPath) {
        log("InstallCrashHandler: skipping because !crashDumpPath\n");
        return;
    }

    if (!SetSymbolsDir(symDir)) {
        log("InstallCrashHandler: skipping because !SetSymbolsDir()\n");
        return;
    }

    logf("InstallCrashHandler:\n  crashDumpPath: '%s'\n  crashFilePath: '%s'\n  symDir: '%s'\n", crashDumpPath,
         crashFilePath, symDir);

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

    TempStr path = GetSettingsPathTemp();
    // can be empty on first run but that's fine because then we know it has default values
    ByteSlice prefsData = file::ReadFile(path);
    if (!prefsData.empty()) {
        // serialize without FileStates info because it's the largest
        GlobalPrefs* gp = NewGlobalPrefs((const char*)prefsData.data());
        delete gp->fileStates;
        gp->fileStates = new Vec<FileState*>();
        // TODO: also sessionData?
        ByteSlice d = SerializeGlobalPrefs(gp, nullptr);
        gSettingsFile = (char*)d.data();
        DeleteGlobalPrefs(gp);
        prefsData.Free();
    }

    gDumpEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!gDumpEvent) {
        log("InstallCrashHandler: skipping because !gDumpEvent\n");
        return;
    }
    gDumpThread = CreateThread(nullptr, 0, CrashDumpThread, nullptr, 0, nullptr);
    if (!gDumpThread) {
        log("InstallCrashHandler: skipping because !gDumpThread\n");
        return;
    }
    gPrevExceptionFilter = SetUnhandledExceptionFilter(CrashDumpExceptionHandler);
    // 1 means that our handler will be called first, 0 would be: last
    AddVectoredExceptionHandler(1, CrashDumpVectoredExceptionHandler);

    signal(SIGABRT, onSignalAbort);
#if COMPILER_MSVC
    ::set_terminate(onTerminate);
    // TODO: breaks starting in 17.3. Requires _HAS_EXCEPTION
    // but it is disabled by _HAS_CXX17 because P0003R5
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0003r5.html
    //::set_unexpected(onUnexpected);
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

    str::FreePtr(&gCrashDumpPath);
    str::FreePtr(&gSymbolsUrl);
    str::FreePtr(&gSymbolsDir);

    str::FreePtr(&gSymbolPath);
    str::FreePtr(&gSystemInfo);
    str::FreePtr(&gSettingsFile);
    str::FreePtr(&gModulesInfo);
    delete gCrashHandlerAllocator;
}

// Tests that various ways to crash will generate crash report.
// Commented-out because they are ad-hoc. Left in code because
// I don't want to write them again if I ever need to test crash reporting
#if 0
#include <signal.h>
static void TestCrashAbort()
{
    raise(SIGABRT);
}

struct Base;
void foo(Base* b);

struct Base {
    Base() {
        foo(this);
    }
    virtual ~Base() = 0;
    virtual void pure() = 0;
};
struct Derived : public Base {
    void pure() { }
};

void foo(Base* b) {
    b->pure();
}

static void TestCrashPureCall()
{
    Derived d; // should crash
}

// tests that making a big allocation with new raises an exception
static int TestBigNew()
{
    size_t size = 1024*1024*1024*1;  // 1 GB should be out of reach
    char *mem = (char*)1;
    while (mem) {
        mem = new char[size];
    }
    // just some code so that compiler doesn't optimize this code to null
    for (size_t i = 0; i < 1024; i++) {
        mem[i] = i & 0xff;
    }
    int res = 0;
    for (size_t i = 0; i < 1024; i++) {
        res += mem[i];
    }
    return res;
}
#endif

/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "base/Base.h"

#pragma warning(disable : 4668)
#include <signal.h>
#include <memory>

#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "base/File.h"
#include "base/Http.h"
#include "base/LzmaSimpleArchive.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppTools.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "AppSettings.h"

#include "base/Log.h"

// logf()/logfa() are now macros that format with fmt() and route through
// log()/loga(), so they keep logging (to at least the debugger) even when
// gReducedLogging is set.

#define kCrashHandlerServer "www.sumatrapdfreader.org"
#define kCrashHandlerServerPort 443
#define kCrashHandlerServerSubmitURL "/uploadcrash/sumatrapdf-crashes"

// The following functions allow crash handler to be used by both installer
// and sumatra proper. They must be implemented for each app.
extern void GetStressTestInfo(str::Builder* s);
extern bool CrashHandlerCanUseNet();
extern void ShowCrashHandlerMessage();
extern void GetProgramInfo(str::Builder& s);

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

static Arena* gCrashHandlerArena = nullptr;

// Note: intentionally not using ScopedMem<> to avoid
// static initializers/destructors, which are bad
Str gSymbolsDir;
Str gCrashFilePath;

static Str gSymbolsUrl;
static Str gCrashDumpPath;
static Str gSystemInfo;
static Str gSettingsFile;
static Str gModulesInfo;
static HANDLE gDumpEvent = nullptr;
static HANDLE gDumpThread = nullptr;
static bool isDllBuild = false;
static bool gLocalOnlyCrashHandler = false;
static bool gCrashed = false;
static volatile LONG gCrashHandlerStarted = 0;
static DWORD gCrashThreadId = 0;
static DWORD gDumpThreadId = 0;

static MINIDUMP_EXCEPTION_INFORMATION gMei{};
static LPTOP_LEVEL_EXCEPTION_FILTER gPrevExceptionFilter = nullptr;

static bool TryStartCrashHandling(Str handlerName) {
    if (InterlockedCompareExchange(&gCrashHandlerStarted, 1, 0) == 0) {
        gCrashThreadId = GetCurrentThreadId();
        gReducedLogging = true;
        return true;
    }

    OutputDebugStringA(CStrTemp(handlerName));
    OutputDebugStringA(": ignoring nested crash\n");

    DWORD threadId = GetCurrentThreadId();
    if (threadId == gCrashThreadId || threadId == gDumpThreadId) {
        TerminateProcess(GetCurrentProcess(), 1);
    }

    if (gDumpThread) {
        WaitForSingleObject(gDumpThread, INFINITE);
    }

    // The first crash handler will show the crash message and terminate the process.
    Sleep(INFINITE);
    return false;
}

// returns true if running on wine
static bool GetModules(str::Builder& s, bool additionalOnly) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) {
        return true;
    }

    MODULEENTRY32 mod;
    mod.dwSize = sizeof(mod);
    BOOL cont = Module32First(snap, &mod);
    while (cont) {
        auto nameA = ToUtf8Temp(mod.szModule);
        auto pathA = ToUtf8Temp(mod.szExePath);
        if (additionalOnly && gModulesInfo) {
            if (!str::ContainsI(gModulesInfo, pathA)) {
                s.Append(str::Format(s.allocator, "Module: %p %06X %-16s %s\n", mod.modBaseAddr, mod.modBaseSize, nameA,
                                     pathA));
            }
        } else {
            s.Append(
                str::Format(s.allocator, "Module: %p %06X %-16s %s\n", mod.modBaseAddr, mod.modBaseSize, nameA, pathA));
        }
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
    return IsRunningOnWine();
}

static Str BuildCrashInfoText(Str condStr, Str fileLine, bool isCrash, bool captureCallstack) {
    str::Builder s(16 * 1024, gCrashHandlerArena);
    if (!isCrash) {
        captureCallstack = true;
        s.Append("Type: debug report (not crash)\n");
    }
    if (condStr) {
        // format into the pre-allocated crash arena, not the temp allocator
        s.Append(str::Format(s.allocator, "Cond: %s @ %s\n", condStr, fileLine));
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
    if (gLogBuf) {
        s.Append(ToStr(*gLogBuf));
    } else {
        s.Append("(no log - crashed before initializing logging)\n");
    }

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

    return s.TakeStr();
}

static Str BuildLocalCrashInfoText(Str condStr, Str fileLine, bool isCrash, bool captureCallstack) {
    str::Builder s(16 * 1024, gCrashHandlerArena);
    if (!isCrash) {
        captureCallstack = true;
        s.Append("Type: debug report (not crash)\n");
    }
    if (condStr) {
        // format into the pre-allocated crash arena, not the temp allocator
        s.Append(str::Format(s.allocator, "Cond: %s @ %s\n", condStr, fileLine));
    }
    if (gSystemInfo) {
        s.Append(gSystemInfo);
        s.Append("\n");
    }

    DWORD crashedThreadId = gMei.ThreadId;
    if (gMei.ExceptionPointers) {
        dbghelp::GetExceptionInfo(s, gMei.ExceptionPointers);
    } else if (captureCallstack) {
        crashedThreadId = GetCurrentThreadId();
        s.Append("\nCrashed thread:\n");
        dbghelp::GetCurrentThreadCallstack(s);
    }

    if (captureCallstack) {
        s.Append("\nOther threads:\n");
        dbghelp::GetAllThreadsCallstacksExcept(s, crashedThreadId);
        s.Append("\n");
    }

    return s.TakeStr();
}

void SaveCrashInfo(Str d) {
    if (!gCrashFilePath) {
        logf("SaveCrashInfo: skipping because !gCrashFilePath");
        return;
    }
    logf("SaveCrashInfo: gCrashFilePath='%s'\n", gCrashFilePath);
    dir::CreateForFile(gCrashFilePath);
    file::WriteFile(gCrashFilePath, d);
}

static void WriteCrashInfoToStdErr(Str d) {
    if (len(d) == 0) {
        return;
    }
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(h, (u8*)d.s, (DWORD)d.len, &written, nullptr);
}

void UploadCrashReport(Str d) {
    log("UploadCrashReport()\n");
    if (len(d) == 0) {
        return;
    }

    str::Builder headers(256, gCrashHandlerArena);
    headers.Append("Content-Type: text/plain");

    str::Builder data(16 * 1024, gCrashHandlerArena);
    data.Append(d);

    HttpPost(kCrashHandlerServer, kCrashHandlerServerPort, kCrashHandlerServerSubmitURL, &headers, &data);
}

static bool ExtractSymbols(Str archiveData, Str dstDir, Arena* allocator) {
    logf("ExtractSymbols: dir '%s', size: %d\n", dstDir, archiveData.len);
    lzma::SimpleArchive archive;
    bool ok = ParseSimpleArchive((const u8*)archiveData.s, (size_t)archiveData.len, &archive);
    if (!ok) {
        logf("ExtractSymbols: ParseSimpleArchive failed\n");
        return false;
    }

    for (int i = 0; i < archive.filesCount; i++) {
        lzma::FileInfo* fi = &(archive.files[i]);
        Str name = fi->name;
        logf("ExtractSymbols: file %d is '%s'\n", i, name);
        u8* uncompressed = GetFileDataByIdx(&archive, i, allocator);
        if (!uncompressed) {
            return false;
        }
        TempStr filePath = path::JoinTemp(dstDir, name);
        if (!filePath) {
            return false;
        }
        Str d = Str((char*)uncompressed, (int)fi->uncompressedSize);
        ok = file::WriteFile(filePath, d);
        if (!ok) {
            DWORD err = GetLastError();
            logf("ExtractSymbols: failed to write '%s'\n", filePath);
            LogLastError(err);
        }
        Free(allocator, uncompressed);
        if (!ok) {
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
static bool DownloadAndUnzipSymbols(Str symDir) {
    if (gDisableSymbolsDownload) {
        // don't care about debug builds because we don't release them
        log("DownloadAndUnzipSymbols: DEBUG build so not doing anything\n");
        return false;
    }

    if (!dir::CreateAll(symDir)) {
        logf("CrashHandlerDownloadSymbols: couldn't create symbols dir '%s'\n", symDir);
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

    bool ok = ExtractSymbols(ToStr(rsp.data), symDir, gCrashHandlerArena);
    if (!ok) {
        log("DownloadAndUnzipSymbols: ExtractSymbols() failed\n");
    }
    return ok;
}

bool CrashHandlerDownloadSymbols() {
    if (gLocalOnlyCrashHandler) {
        log("CrashHandlerDownloadSymbols: skipping in local-only crash handler\n");
        return false;
    }
    return DownloadAndUnzipSymbols(gSymbolsDir);
}

bool AreSymbolsDownloaded(Str symDir) {
    TempStr path = path::JoinTemp(symDir, StrL("SumatraPDF.pdb"));
    if (file::Exists(path)) {
        logf("AreSymbolsDownloaded(): exist in '%s', symDir: '%s'\n", path, symDir);
        return true;
    }
    TempStr exePath = GetSelfExePathTemp();
    exePath = str::ReplaceTemp(exePath, StrL(".exe"), StrL(".pdb"));
    if (file::Exists(exePath)) {
        logf("AreSymbolsDownloaded(): exist in '%s', symDir: '%s'\n", exePath, symDir);
        return true;
    }
    logf("AreSymbolsDownloaded(): not downloaded, symDir: '%s'\n", symDir);
    return false;
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

static TempStr BuildSymbolPathTemp(Str symDir) {
    str::Builder path(2048, GetTempArena());

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
        TempStr ntSymPath = GetEnvVariableTemp(StrL("_NT_SYMBOL_PATH"));
        // internet talks about both _NT_ALT_SYMBOL_PATH and _NT_ALTERNATE_SYMBOL_PATH
        if (len(ntSymPath) == 0) {
            ntSymPath = GetEnvVariableTemp(StrL("_NT_ALT_SYMBOL_PATH"));
        }
        if (len(ntSymPath) == 0) {
            ntSymPath = GetEnvVariableTemp(StrL("_NT_ALTERNATE_SYMBOL_PATH"));
        }
        if (len(ntSymPath) > 0) {
            path.Append(ntSymPath);
            path.Append(";");
        }
    }
    if (gAddSymbolServer && symDirExists) {
        // this probably won't work as it needs symsrv.dll and that's not included with Windows
        // TODO: maybe try to scan system directories for symsrv.dll and somehow add it?
        path.Append(fmt("cache*%s;srv*https://msdl.microsoft.com/download/symbols;", symDir));
    }

    // remove ";" from the end
    path.RemoveLast();
    return ToStrTemp(path);
}

bool InitializeDbgHelp(bool force) {
    TempStr symPath = BuildSymbolPathTemp(gSymbolsDir);
    TempWStr ws = ToWStrTemp(symPath);
    if (!dbghelp::Initialize(ws, force)) {
        logf("InitializeDbgHelp: dbghelp::Initialize('%s'), force: %d failed\n", symPath, (int)force);
        return false;
    }

    if (!dbghelp::HasSymbols()) {
        logf("InitializeDbgHelp(): dbghelp::HasSymbols(), symPath: '%s' force: %d failed\n", symPath, (int)force);
        return false;
    }
    logf("InitializeDbgHelp(): did initialize ok, symPath: '%s'\n", symPath);
    return true;
}

static bool DownloadSymbolsIfNeededAndInitializeDbgHelp() {
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
void _uploadDebugReport(Str condStr, Str fileLine, bool isCrash, bool captureCallstack) {
    // in release builds ReportIf()/ReportIfFast() will break if running under
    // the debugger. In other builds it sends a debug report
    if (condStr) {
        logfa("_uploadDebugReport: %s %s\n", condStr, fileLine);
    } else {
        loga("_uploadDebugReport\n");
    }

    bool shouldUpload = true;
    // debug build is likely other people modyfing
    bool downloadSymbols = true;
    if (gIsDebugBuild || gIsAsanBuild) {
        shouldUpload = false;
        downloadSymbols = false;
    }
    if (!isCrash) {
        // for non-crashes, don't upload in release builds (too much info)
        shouldUpload = gIsPreReleaseBuild;
    }

    if (gLocalOnlyCrashHandler) {
        InitializeDbgHelp(false);
        auto s = BuildLocalCrashInfoText(condStr, fileLine, isCrash, captureCallstack);
        if (len(s) == 0) {
            loga("_uploadDebugReport(): skipping because !BuildLocalCrashInfoText()\n");
            return;
        }
        Str d = s;
        SaveCrashInfo(d);
        WriteCrashInfoToStdErr(d);
        loga(s);
        loga("_uploadDebugReport() finished local-only\n");
        return;
    }

    if (!shouldUpload) {
        if (IsDebuggerPresent()) {
            DebugBreak();
        } else {
            InitializeDbgHelp(false);
            auto s = BuildCrashInfoText(condStr, fileLine, isCrash, captureCallstack);
            if (len(s) == 0) {
                loga("_uploadDebugReport(): skipping because !BuildCrashInfoText()\n");
                return;
            }
            Str d = s;
            SaveCrashInfo(d);
            log(s);
        }
        log("_uploadDebugReport skipping because !shouldUpload\n");
        return;
    }

    // we want to avoid submitting multiple reports for the same
    // condition. I'm too lazy to implement tracking this granularly
    // so only allow once submission in a given session
    static bool didSubmitDebugReport = false;

    // don't send report if this is me debugging
    if (IsDebuggerPresent()) {
        log("_uploadDebugReport skipping because IsDebuggerPresent\n");
        DebugBreak();
        return;
    }

    if (didSubmitDebugReport) {
        return;
    }
    didSubmitDebugReport = true;

    if (!CrashHandlerCanUseNet()) {
        log("_uploadDebugReport skipping because !CrashHandlerCanUseNet()\n");
        return;
    }

    logfa("_uploadDebugReport: isCrash: %d, captureCallstack: %d, gSymbolsDir: '%s'\n", (int)isCrash,
          (int)captureCallstack, gSymbolsDir);

    if (captureCallstack && downloadSymbols) {
        // we proceed even if we fail to download symbols
        DownloadSymbolsIfNeededAndInitializeDbgHelp();
    }

    auto s = BuildCrashInfoText(condStr, fileLine, isCrash, captureCallstack);
    if (len(s) == 0) {
        loga("_uploadDebugReport(): skipping because !BuildCrashInfoText()\n");
        return;
    }
    Str d = s;
    SaveCrashInfo(d);

    UploadCrashReport(d);
    // gCrashHandlerArena->Free((const void*)d.data());
    loga(s);
    loga("_uploadDebugReport() finished\n");
}

static DWORD WINAPI CrashDumpThread(LPVOID) {
    WaitForSingleObject(gDumpEvent, INFINITE);
    if (!gCrashed) {
        return 0;
    }

    log("CrashDumpThread\n");
    _uploadDebugReport(nullptr, "", true, true);

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

    if (!TryStartCrashHandling("CrashDumpVectoredExceptionHandler")) {
        return EXCEPTION_CONTINUE_SEARCH; // Note: or should TerminateProcess()?
    }

    log("CrashDumpVectoredExceptionHandler\n");
    gCrashed = true;

    gMei.ThreadId = GetCurrentThreadId();
    gMei.ExceptionPointers = exceptionInfo;
    // per msdn (which is backed by my experience), MiniDumpWriteDump() doesn't
    // write callstack for the calling thread correctly. We use msdn-recommended
    // work-around of spinning a thread to do the writing
    SetEvent(gDumpEvent);
    WaitForSingleObject(gDumpThread, INFINITE);

    if (!gLocalOnlyCrashHandler) {
        ShowCrashHandlerMessage();
    }
    TerminateProcess(GetCurrentProcess(), 1);

    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI CrashDumpExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (!exceptionInfo || (EXCEPTION_BREAKPOINT == exceptionInfo->ExceptionRecord->ExceptionCode)) {
        log("CrashDumpExceptionHandler: exiting because !exceptionInfo || EXCEPTION_BREAKPOINT == "
            "exceptionInfo->ExceptionRecord->ExceptionCode\n");
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (!TryStartCrashHandling("CrashDumpExceptionHandler")) {
        return EXCEPTION_CONTINUE_SEARCH; // Note: or should TerminateProcess()?
    }

    log("CrashDumpExceptionHandler\n");
    gCrashed = true;

    gMei.ThreadId = GetCurrentThreadId();
    gMei.ExceptionPointers = exceptionInfo;
    // per msdn (which is backed by my experience), MiniDumpWriteDump() doesn't
    // write callstack for the calling thread correctly. We use msdn-recommended
    // work-around of spinning a thread to do the writing
    SetEvent(gDumpEvent);
    WaitForSingleObject(gDumpThread, INFINITE);

    if (!gLocalOnlyCrashHandler) {
        ShowCrashHandlerMessage();
    }
    TerminateProcess(GetCurrentProcess(), 1);

    return EXCEPTION_CONTINUE_SEARCH;
}

static void GetOsVersion(str::Builder& s) {
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
        s.Append(fmt("OS: Windows %s build %d %s\n", os, buildNumber, Str(arch)));
    } else if (0 == servicePackMinor) {
        s.Append(fmt("OS: Windows %s SP%d build %d %s\n", os, servicePackMajor, buildNumber, Str(arch)));
    } else {
        s.Append(
            fmt("OS: Windows %s %d.%d build %d %s\n", os, servicePackMajor, servicePackMinor, buildNumber, Str(arch)));
    }
}

static void GetProcessorName(str::Builder& s) {
    auto key = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor";
    TempStr name = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "ProcessorNameString");
    if (!name) {
        // if more than one processor
        key = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
        name = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "ProcessorNameString");
    }
    if (name) {
        s.Append(fmt("Processor: %s\n", name));
    }
}

// note: don't build this with fmt() - its format grammar treats the '\{' before
// the GUID as an escape and would drop the backslash, corrupting the key
#define GFX_DRIVER_KEY_PREFIX "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\"

static void GetGraphicsDriverInfo(str::Builder& s) {
    // the info is in registry in:
    // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000\
    //   Device Description REG_SZ (same as DriverDesc, so we don't read it)
    //   DriverDesc REG_SZ
    //   DriverVersion REG_SZ
    //   UserModeDriverName REG_MULTI_SZ
    //
    // There can be more than one driver, they are in 0000, 0001 etc.
    for (int i = 0;; i++) {
        TempStr key = str::JoinTemp(GFX_DRIVER_KEY_PREFIX, fmt("%04d", i));
        TempStr v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "DriverDesc");
        // I assume that if I can't read the value, there are no more drivers
        if (!v) {
            break;
        }
        s.Append(fmt("Graphics driver %d\n", i));
        s.Append(fmt("  DriverDesc:         %s\n", v));

        v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "DriverVersion");
        if (v) {
            s.Append(fmt("  DriverVersion:      %s\n", v));
        }

        v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, "UserModeDriverName");
        if (v) {
            s.Append(fmt("  UserModeDriverName: %s\n", v));
        }
    }
}

static void GetSystemInfo(str::Builder& s) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s.Append(fmt("Number Of Processors: %d\n", si.dwNumberOfProcessors));
    GetProcessorName(s);

    {
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);

        float physMemGB = (float)ms.ullTotalPhys / (float)(1024 * 1024 * 1024);
        float totalPageGB = (float)ms.ullTotalPageFile / (float)(1024 * 1024 * 1024);
        DWORD usedPerc = ms.dwMemoryLoad;
        s.Append(fmt("Physical Memory: %.2f GB\nCommit Charge Limit: %.2f GB\nMemory Used: %d%%\n", physMemGB,
                     totalPageGB, usedPerc));
    }
    {
        TempStr ver = GetWebView2VersionTemp();
        if (len(ver) == 0) {
            ver = "no WebView2 installed";
        }
        s.Append(fmt("WebView2: %s\n", ver));
    }
    {
        // get computer name
        TempStr s1 = ReadRegStrTemp(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemFamily");
        TempStr s2 = ReadRegStrTemp(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemVersion");

        if (!s1 && !s2) {
            // no-op
        } else if (!s1) {
            s.Append(fmt("Machine: %s\n", s2));
        } else if (!s2 || str::EqI(s1, s2)) {
            s.Append(fmt("Machine: %s\n", s1));
        } else {
            s.Append(fmt("Machine: %s %s\n", s1, s2));
        }
    }
    {
        // get language
        char country[32] = {}, lang[32]{};
        GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, dimof(country) - 1);
        GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, dimof(lang) - 1);
        s.Append(fmt("Lang: %s %s\n", Str(lang), Str(country)));
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
        if (cpu & kCpuNEON) {
            s.Append("NEON ");
        }
        if (cpu & kCpuArmCrypto) {
            s.Append("Crypto ");
        }
        if (cpu & kCpuArmAtomics) {
            s.Append("Atomics ");
        }
        if (cpu & kCpuArmDotProd) {
            s.Append("DotProd ");
        }
    }
}

// returns true if running on wine
static bool BuildModulesInfo() {
    str::Builder s(1024, gCrashHandlerArena);
    bool isWine = GetModules(s, false);
    gModulesInfo = s.TakeStr();
    return isWine;
}

static void BuildSystemInfo() {
    str::Builder s(1024, gCrashHandlerArena);
    GetProgramInfo(s);
    GetOsVersion(s);
    GetSystemInfo(s);
    gSystemInfo = s.TakeStr();
}

bool SetSymbolsDir(Str symDir) {
    if (!symDir) {
        return false;
    }
    gSymbolsDir = str::Dup(gCrashHandlerArena, symDir);
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

static Str BuildSymbolsUrl() {
    Str urlBase = StrL("https://www.sumatrapdfreader.org/dl/");
    if (gIsPreReleaseBuild) {
        urlBase = str::JoinTemp(urlBase, StrL("prerel/"), preReleaseVersion, StrL("/SumatraPDF-prerel"));
    } else {
        // assuming this is release version
        Str ver = StrL(QM(CURR_VERSION));
        urlBase = str::JoinTemp(urlBase, StrL("rel/"), ver, StrL("/SumatraPDF-"), ver);
    }
    // TODO: ugly it's different between release and pre-release
    Str suff = StrL(".pdb.lzsa");
    if (gIsPreReleaseBuild) {
        suff = StrL("-32.pdb.lzsa");
    }

#if IS_ARM_64 == 1
    suff = StrL("-arm64.pdb.lzsa");
#elif IS_INTEL_64 == 1
    suff = StrL("-64.pdb.lzsa");
#endif
    return str::Join(gCrashHandlerArena, urlBase, suff, Str());
}

void InstallCrashHandler(Str crashDumpPath, Str crashFilePath, Str symDir, bool localOnly) {
    ReportIf(gDumpEvent || gDumpThread);

    if (!crashDumpPath) {
        log("InstallCrashHandler: skipping because !crashDumpPath\n");
        return;
    }

    // we pre-allocate as much as possible to minimize allocations
    // when crash handler is invoked. It's ok to use standard
    // allocation functions here.
    gCrashHandlerArena = ArenaNew();

    if (!SetSymbolsDir(symDir)) {
        log("InstallCrashHandler: skipping because !SetSymbolsDir()\n");
        return;
    }

    logf("InstallCrashHandler:\n  crashDumpPath: '%s'\n  crashFilePath: '%s'\n  symDir: '%s'\n", crashDumpPath,
         crashFilePath, symDir);

    gCrashDumpPath = str::Dup(gCrashHandlerArena, crashDumpPath);
    gCrashFilePath = str::Dup(gCrashHandlerArena, crashFilePath);
    gLocalOnlyCrashHandler = localOnly;
    gCrashThreadId = 0;
    gDumpThreadId = 0;
    InterlockedExchange(&gCrashHandlerStarted, 0);

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

    gSymbolsUrl = BuildSymbolsUrl();

    // installer/uninstaller don't use app settings; reading them here would
    // trigger GetAppDataDirTemp() before installation is complete
    if (!IsInstallerOrUninstallerExe()) {
        TempStr path = GetSettingsPathTemp();
        // can be empty on first run but that's fine because then we know it has default values
        Str prefsData = file::ReadFile(path);
        if (!str::IsEmpty(prefsData)) {
            // serialize without FileStates info because it's the largest
            GlobalPrefs* gp = NewGlobalPrefs(prefsData);
            DeleteFileStates(gp->fileStates);
            gp->fileStates = new Vec<FileState*>();
            // TODO: also sessionData?
            Str d = SerializeGlobalPrefs(gp, nullptr);
            gSettingsFile = str::Dup(gCrashHandlerArena, d);
            str::Free(d);
            DeleteGlobalPrefs(gp);
            str::Free(prefsData);
        }
    }

    gDumpEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!gDumpEvent) {
        log("InstallCrashHandler: skipping because !gDumpEvent\n");
        return;
    }
    gDumpThread = CreateThread(nullptr, 0, CrashDumpThread, nullptr, 0, &gDumpThreadId);
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
    // set_unexpected() is unavailable with MSVC 17.3+ (_HAS_CXX17 / P0003R5).
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

    // those are allocated from gCrashHandlerArena so are freed by ArenaDelete()
    gCrashDumpPath = {};
    gSymbolsUrl = {};
    gSymbolsDir = {};

    gSystemInfo = {};
    gSettingsFile = {};
    gModulesInfo = {};
    gCrashFilePath = {};
    ArenaDelete(gCrashHandlerArena);
    gCrashHandlerArena = nullptr;
    gCrashThreadId = 0;
    gDumpThreadId = 0;
    gLocalOnlyCrashHandler = false;
    InterlockedExchange(&gCrashHandlerStarted, 0);
}

// Tests that various ways to crash will generate crash report.
// Commented-out because they are ad-hoc. Left in code because
// I don't want to write them again if I ever need to test crash reporting
#if 0
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
    char* mem = (char*)1;
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

/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "base/Base.h"

#pragma warning(disable : 4668)
#include <signal.h>
#include <memory>
#include <new.h> // _set_new_handler

#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "base/File.h"
#include "base/Http.h"
#include "base/LzmaSimpleArchive.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/win/WebView.h"

#include "Settings.h"
#include "AppTools.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "AppSettings.h"
#include "CrashHandler.h"
#include "SumatraLog.h"

// logf() is a template that formats with fmt() and routes through log(), so it
// keeps logging (to at least the debugger) even when gReducedLogging is set.

#define kCrashHandlerServer "www.sumatrapdfreader.org"
constexpr int kCrashHandlerServerPort = 443;
#define kCrashHandlerServerSubmitURL "/uploadcrash/sumatrapdf-crashes"

// The following functions allow crash handler to be used by both installer
// and sumatra proper. They must be implemented for each app.
extern void GetStressTestInfo();
extern bool CrashHandlerCanUseNet();
extern void ShowCrashHandlerMessage();
extern void GetProgramInfo();

// in DEBUG we don't enable symbols download because they are not uploaded
#if IS_DEBUG
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

// The report is built here rather than threaded through every helper as a
// str::Builder&. Lives in the arena so there is no static ctor/dtor.
static str::Builder* gCrashInfo = nullptr;

void CrashInfoAppend(Str s) {
    if (!gCrashInfo) {
        return;
    }
    str::BuilderAppend(gCrashHandlerArena, *gCrashInfo, s);
}

// start a fresh report, keeping the storage already allocated
static void CrashInfoStart(int cap) {
    if (!gCrashInfo) {
        return;
    }
    gCrashInfo->Reset();
    str::BuilderReserve(gCrashHandlerArena, *gCrashInfo, cap);
}

static Str CrashInfoTake() {
    if (!gCrashInfo) {
        return {};
    }
    return str::BuilderTakeStr(gCrashHandlerArena, *gCrashInfo);
}

// exit code for a debug report (ReportIf) in a -for-testing run; test runners
// (tests/control.ts) treat it as "assertion fired", so keep the value in sync
constexpr UINT kDebugReportTestExitCode = 105;

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
static ThreadHandle gDumpThread = nullptr;
static bool isDllBuild = false;
static bool gLocalOnlyCrashHandler = false;
static bool gCrashed = false;
static volatile LONG gCrashHandlerStarted = 0;
static ThreadId gCrashThreadId = 0;
static ThreadId gDumpThreadId = 0;

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

    ThreadId threadId = GetCurrentThreadId();
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
static bool GetModules(bool additionalOnly) {
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
                CrashInfoAppend(str::Format(gCrashHandlerArena, "Module: %p %06X %-16s %s\n", mod.modBaseAddr,
                                            mod.modBaseSize, nameA, pathA));
            }
        } else {
            CrashInfoAppend(str::Format(gCrashHandlerArena, "Module: %p %06X %-16s %s\n", mod.modBaseAddr,
                                        mod.modBaseSize, nameA, pathA));
        }
        cont = Module32Next(snap, &mod);
    }
    CloseHandle(snap);
    return IsRunningOnWine();
}

// Message from MuPDF's uncaught-throw abort (error.c). Looked up at crash time
// so we do not need a hard link for every tool that builds CrashHandlerNoOp.
// libsumatrapdf.dll (or the static main module) exports fz_last_uncaught_error.
static const char* LookupUncaughtMupdfError() {
#if OS_WIN
    using Fn = const char* (*)();
    HMODULE modules[2] = {
        GetModuleHandleW(L"libsumatrapdf.dll"),
        GetModuleHandleW(nullptr),
    };
    for (HMODULE h : modules) {
        if (!h) {
            continue;
        }
        auto fn = (Fn)GetProcAddress(h, "fz_last_uncaught_error");
        if (fn) {
            const char* msg = fn();
            if (msg && msg[0]) {
                return msg;
            }
        }
    }
#endif
    return nullptr;
}

static void AppendUncaughtMupdfError() {
    const char* msg = LookupUncaughtMupdfError();
    if (!msg || !msg[0]) {
        return;
    }
    // High-visibility: empty callstacks from the intentional null-write still
    // need to explain the real failure (MuPDF throw with no fz_try).
    CrashInfoAppend(str::Format(gCrashHandlerArena, "Uncaught MuPDF error: %s\n\n", Str(msg)));
}

static Str BuildCrashInfoText(Str condStr, Str fileLine, bool isCrash, bool captureCallstack) {
    CrashInfoStart(16 * 1024);
    if (!isCrash) {
        captureCallstack = true;
        CrashInfoAppend(StrL("Type: debug report (not crash)\n"));
    }
    if (condStr) {
        // format into the pre-allocated crash arena, not the temp allocator
        CrashInfoAppend(str::Format(gCrashHandlerArena, "Cond: %s @ %s\n", condStr, fileLine));
    }
    AppendUncaughtMupdfError();
    if (gSystemInfo) {
        CrashInfoAppend(gSystemInfo);
        CrashInfoAppend(StrL("\n"));
    }

    //    GetStressTestInfo();

    if (gMei.ExceptionPointers) {
        // those are only set when we capture exception
        dbghelp::GetExceptionInfo(gCrashHandlerArena, *gCrashInfo, gMei.ExceptionPointers);
        CrashInfoAppend(StrL("\n"));
    } else {
        // GetExceptionInfo() also adds current thread callstack
        if (captureCallstack) {
            CrashInfoAppend(StrL("\nCrashed thread:\n"));
            dbghelp::GetCurrentThreadCallstack(gCrashHandlerArena, *gCrashInfo);
            CrashInfoAppend(StrL("\n"));
        }
    }

    CrashInfoAppend(StrL("\n-------- Log -----------------\n\n"));
    if (gLogBuf) {
        CrashInfoAppend(ToStr(*gLogBuf));
    } else {
        CrashInfoAppend(StrL("(no log - crashed before initializing logging)\n"));
    }

    if (gSettingsFile) {
        CrashInfoAppend(StrL("\n\n----- Settings file ----------\n\n"));
        CrashInfoAppend(gSettingsFile);
        CrashInfoAppend(StrL("\n\n"));
    }

    CrashInfoAppend(StrL("\n-------- Modules   ----------\n\n"));
    CrashInfoAppend(gModulesInfo);
    CrashInfoAppend(StrL("\nModules loaded later:\n"));
    GetModules(true);

    if (captureCallstack) {
        CrashInfoAppend(StrL("\n-------- All Threads ----------\n\n"));
        dbghelp::GetAllThreadsCallstacks(gCrashHandlerArena, *gCrashInfo);
        CrashInfoAppend(StrL("\n"));
    }

    return CrashInfoTake();
}

static Str BuildLocalCrashInfoText(Str condStr, Str fileLine, bool isCrash, bool captureCallstack) {
    CrashInfoStart(16 * 1024);
    if (!isCrash) {
        captureCallstack = true;
        CrashInfoAppend(StrL("Type: debug report (not crash)\n"));
    }
    if (condStr) {
        // format into the pre-allocated crash arena, not the temp allocator
        CrashInfoAppend(str::Format(gCrashHandlerArena, "Cond: %s @ %s\n", condStr, fileLine));
    }
    AppendUncaughtMupdfError();
    if (gSystemInfo) {
        CrashInfoAppend(gSystemInfo);
        CrashInfoAppend(StrL("\n"));
    }

    ThreadId crashedThreadId = gMei.ThreadId;
    if (gMei.ExceptionPointers) {
        dbghelp::GetExceptionInfo(gCrashHandlerArena, *gCrashInfo, gMei.ExceptionPointers);
    } else if (captureCallstack) {
        crashedThreadId = GetCurrentThreadId();
        CrashInfoAppend(StrL("\nCrashed thread:\n"));
        dbghelp::GetCurrentThreadCallstack(gCrashHandlerArena, *gCrashInfo);
    }

    if (captureCallstack) {
        CrashInfoAppend(StrL("\nOther threads:\n"));
        dbghelp::GetAllThreadsCallstacksExcept(gCrashHandlerArena, *gCrashInfo, crashedThreadId);
        CrashInfoAppend(StrL("\n"));
    }

    return CrashInfoTake();
}

static void SaveCrashInfo(Str d) {
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

static void UploadCrashReport(Str d) {
    log(StrL("UploadCrashReport()\n"));
    if (len(d) == 0) {
        return;
    }

    str::Builder headers;
    str::BuilderReserve(gCrashHandlerArena, headers, 256);
    str::BuilderAppend(gCrashHandlerArena, headers, StrL("Content-Type: text/plain"));

    str::Builder data;
    str::BuilderReserve(gCrashHandlerArena, data, 16 * 1024);
    str::BuilderAppend(gCrashHandlerArena, data, d);

    HttpPost(StrL(kCrashHandlerServer), kCrashHandlerServerPort, StrL(kCrashHandlerServerSubmitURL), &headers, &data);
}

static bool ExtractSymbols(Str archiveData, Str dstDir, Arena* a) {
    logf("ExtractSymbols: dir '%s', size: %d\n", dstDir, archiveData.len);
    lzma::SimpleArchive archive;
    bool ok = ParseSimpleArchive((const u8*)archiveData.s, archiveData.len, &archive);
    if (!ok) {
        logf("ExtractSymbols: ParseSimpleArchive failed\n");
        return false;
    }

    for (int i = 0; i < archive.filesCount; i++) {
        lzma::FileInfo* fi = &(archive.files[i]);
        Str name = fi->name;
        logf("ExtractSymbols: file %d is '%s'\n", i, name);
        if (!name || str::Eq(name, StrL(".")) || str::Eq(name, StrL("..")) || str::Contains(name, StrL("/")) ||
            str::Contains(name, StrL("\\")) || str::Contains(name, StrL(":"))) {
            return false;
        }
        u8* uncompressed = GetFileDataByIdx(&archive, i, a);
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
        Free(a, uncompressed);
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
        log(StrL("DownloadAndUnzipSymbols: DEBUG build so not doing anything\n"));
        return false;
    }

    if (!dir::CreateAll(symDir)) {
        logf("CrashHandlerDownloadSymbols: couldn't create symbols dir '%s'\n", symDir);
        return false;
    }

    logf("DownloadAndUnzipSymbols: symDir: '%s', url: '%s'\n", symDir, gSymbolsUrl);
    if (!symDir || !dir::Exists(symDir)) {
        log(StrL("DownloadAndUnzipSymbols: exiting because symDir doesn't exist\n"));
        return false;
    }

    // DeleteSymbolsIfExist();

    HttpRsp rsp;
    if (!HttpGet(gSymbolsUrl, &rsp)) {
        log(StrL("DownloadAndUnzipSymbols: couldn't download symbols\n"));
        return false;
    }
    if (!IsHttpRspOk(&rsp)) {
        log(StrL("DownloadAndUnzipSymbols: HttpRspOk() returned false\n"));
    }

    bool ok = ExtractSymbols(ToStr(rsp.data), symDir, gCrashHandlerArena);
    if (!ok) {
        log(StrL("DownloadAndUnzipSymbols: ExtractSymbols() failed\n"));
    }
    return ok;
}

bool CrashHandlerDownloadSymbols() {
    if (gLocalOnlyCrashHandler) {
        log(StrL("CrashHandlerDownloadSymbols: skipping in local-only crash handler\n"));
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
    str::Builder path;
    str::BuilderReserve(nullptr, path, 2048);
    Arena* a = GetTempArena();

    bool symDirExists = dir::Exists(symDir);

    // at this point symDir might not exist but we add it anyway
    str::BuilderAppend(a, path, symDir);
    str::BuilderAppend(a, path, StrL(";"));

    // in debug builds the symbols are in the same directory as .exe
    if (gIsDebugBuild || gAddExeDir) {
        TempStr dir = GetSelfExeDirTemp();
        str::BuilderAppend(a, path, dir);
        str::BuilderAppend(a, path, StrL(";"));
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
            str::BuilderAppend(a, path, ntSymPath);
            str::BuilderAppend(a, path, StrL(";"));
        }
    }
    if (gAddSymbolServer && symDirExists) {
        // this probably won't work as it needs symsrv.dll and that's not included with Windows
        // TODO: maybe try to scan system directories for symsrv.dll and somehow add it?
        str::BuilderAppend(a, path, fmt("cache*%s;srv*https://msdl.microsoft.com/download/symbols;", symDir));
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
        logf("_uploadDebugReport: %s %s\n", condStr, fileLine);
    } else {
        log(StrL("_uploadDebugReport\n"));
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
            log(StrL("_uploadDebugReport(): skipping because !BuildLocalCrashInfoText()\n"));
            return;
        }
        Str d = s;
        SaveCrashInfo(d);
        WriteCrashInfoToStdErr(d);
        log(s);
        log(StrL("_uploadDebugReport() finished local-only\n"));
        if (gForTesting && !isCrash && !IsDebuggerPresent()) {
            // automated tests must fail on debug reports (crashes already
            // kill the process with a non-zero code on their own)
            log(StrL("_uploadDebugReport(): -for-testing, terminating with exit code 105\n"));
            ::TerminateProcess(GetCurrentProcess(), kDebugReportTestExitCode);
        }
        return;
    }

    if (!shouldUpload) {
        if (IsDebuggerPresent()) {
            DebugBreak();
        } else {
            InitializeDbgHelp(false);
            auto s = BuildCrashInfoText(condStr, fileLine, isCrash, captureCallstack);
            if (len(s) == 0) {
                log(StrL("_uploadDebugReport(): skipping because !BuildCrashInfoText()\n"));
                return;
            }
            Str d = s;
            SaveCrashInfo(d);
            log(s);
        }
        log(StrL("_uploadDebugReport skipping because !shouldUpload\n"));
        return;
    }

    // we want to avoid submitting multiple reports for the same
    // condition. I'm too lazy to implement tracking this granularly
    // so only allow once submission in a given session
    static bool didSubmitDebugReport = false;

    // don't send report if this is me debugging
    if (IsDebuggerPresent()) {
        log(StrL("_uploadDebugReport skipping because IsDebuggerPresent\n"));
        DebugBreak();
        return;
    }

    if (didSubmitDebugReport) {
        return;
    }
    didSubmitDebugReport = true;

    if (!CrashHandlerCanUseNet()) {
        log(StrL("_uploadDebugReport skipping because !CrashHandlerCanUseNet()\n"));
        return;
    }

    logf("_uploadDebugReport: isCrash: %d, captureCallstack: %d, gSymbolsDir: '%s'\n", (int)isCrash,
         (int)captureCallstack, gSymbolsDir);

    if (captureCallstack && downloadSymbols) {
        // we proceed even if we fail to download symbols
        DownloadSymbolsIfNeededAndInitializeDbgHelp();
    }

    auto s = BuildCrashInfoText(condStr, fileLine, isCrash, captureCallstack);
    if (len(s) == 0) {
        log(StrL("_uploadDebugReport(): skipping because !BuildCrashInfoText()\n"));
        return;
    }
    Str d = s;
    SaveCrashInfo(d);

    UploadCrashReport(d);
    // gCrashHandlerArena->Free((const void*)d.data());
    log(s);
    log(StrL("_uploadDebugReport() finished\n"));
}

static DWORD WINAPI CrashDumpThread(LPVOID /*data*/) {
    WaitForSingleObject(gDumpEvent, INFINITE);
    if (!gCrashed) {
        return 0;
    }

    log(StrL("CrashDumpThread\n"));
    _uploadDebugReport(Str(), StrL(""), true, true);

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

    if (!TryStartCrashHandling(StrL("CrashDumpVectoredExceptionHandler"))) {
        return EXCEPTION_CONTINUE_SEARCH; // Note: or should TerminateProcess()?
    }

    log(StrL("CrashDumpVectoredExceptionHandler\n"));
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
        log(
            StrL("CrashDumpExceptionHandler: exiting because !exceptionInfo || EXCEPTION_BREAKPOINT == "
                 "exceptionInfo->ExceptionRecord->ExceptionCode\n"));
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (!TryStartCrashHandling(StrL("CrashDumpExceptionHandler"))) {
        return EXCEPTION_CONTINUE_SEARCH; // Note: or should TerminateProcess()?
    }

    log(StrL("CrashDumpExceptionHandler\n"));
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

static void GetOsVersion() {
    OSVERSIONINFOEX ver{};
    bool ok = GetOsVersion(ver);
    ver.dwOSVersionInfoSize = sizeof(ver);
    if (!ok) {
        return;
    }

    TempStr os = OsNameFromVerTemp(ver);
    int servicePackMajor = ver.wServicePackMajor;
    int servicePackMinor = ver.wServicePackMinor;
    int buildNumber = (int)ver.dwBuildNumber & 0xFFFF;
    const auto* arch = "64-bit";
    if (IsProcess32()) {
        arch = IsRunningInWow64() ? "Wow64" : "32-bit";
    }
    if (0 == servicePackMajor) {
        CrashInfoAppend(fmt("OS: Windows %s build %d %s\n", os, buildNumber, Str(arch)));
    } else if (0 == servicePackMinor) {
        CrashInfoAppend(fmt("OS: Windows %s SP%d build %d %s\n", os, servicePackMajor, buildNumber, Str(arch)));
    } else {
        CrashInfoAppend(
            fmt("OS: Windows %s %d.%d build %d %s\n", os, servicePackMajor, servicePackMinor, buildNumber, Str(arch)));
    }
}

static void GetProcessorName() {
    const auto* key = R"(HARDWARE\DESCRIPTION\System\CentralProcessor)";
    TempStr name = ReadRegStrTemp(HKEY_LOCAL_MACHINE, Str(key), StrL("ProcessorNameString"));
    if (!name) {
        // if more than one processor
        key = R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)";
        name = ReadRegStrTemp(HKEY_LOCAL_MACHINE, Str(key), StrL("ProcessorNameString"));
    }
    if (name) {
        CrashInfoAppend(fmt("Processor: %s\n", name));
    }
}

#define kGfxDriverKeyPrefix "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\"

static void GetGraphicsDriverInfo() {
    // the info is in registry in:
    // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000\
    //   Device Description REG_SZ (same as DriverDesc, so we don't read it)
    //   DriverDesc REG_SZ
    //   DriverVersion REG_SZ
    //   UserModeDriverName REG_MULTI_SZ
    //
    // There can be more than one driver, they are in 0000, 0001 etc.
    for (int i = 0;; i++) {
        TempStr key = str::JoinTemp(StrL(kGfxDriverKeyPrefix), fmt("%04d", i));
        TempStr v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, StrL("DriverDesc"));
        // I assume that if I can't read the value, there are no more drivers
        if (!v) {
            break;
        }
        CrashInfoAppend(fmt("Graphics driver %d\n", i));
        CrashInfoAppend(fmt("  DriverDesc:         %s\n", v));

        v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, StrL("DriverVersion"));
        if (v) {
            CrashInfoAppend(fmt("  DriverVersion:      %s\n", v));
        }

        v = ReadRegStrTemp(HKEY_LOCAL_MACHINE, key, StrL("UserModeDriverName"));
        if (v) {
            CrashInfoAppend(fmt("  UserModeDriverName: %s\n", v));
        }
    }
}

static void GetSystemInfo() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    CrashInfoAppend(fmt("Number Of Processors: %d\n", si.dwNumberOfProcessors));
    GetProcessorName();

    {
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);

        float physMemGB = (float)ms.ullTotalPhys / (float)(1024 * 1024 * 1024);
        float totalPageGB = (float)ms.ullTotalPageFile / (float)(1024 * 1024 * 1024);
        DWORD usedPerc = ms.dwMemoryLoad;
        CrashInfoAppend(fmt("Physical Memory: %.2f GB\nCommit Charge Limit: %.2f GB\nMemory Used: %d%%\n", physMemGB,
                            totalPageGB, usedPerc));
    }
    {
        TempStr ver = GetWebView2VersionTemp();
        if (len(ver) == 0) {
            ver = StrL("no WebView2 installed");
        }
        CrashInfoAppend(fmt("WebView2: %s\n", ver));
    }
    {
        // get computer name
        TempStr s1 =
            ReadRegStrTemp(HKEY_LOCAL_MACHINE, StrL(R"(HARDWARE\DESCRIPTION\System\BIOS)"), StrL("SystemFamily"));
        TempStr s2 =
            ReadRegStrTemp(HKEY_LOCAL_MACHINE, StrL(R"(HARDWARE\DESCRIPTION\System\BIOS)"), StrL("SystemVersion"));

        if (!s1 && !s2) {
            // no-op
        } else if (!s1) {
            CrashInfoAppend(fmt("Machine: %s\n", s2));
        } else if (!s2 || str::EqI(s1, s2)) {
            CrashInfoAppend(fmt("Machine: %s\n", s1));
        } else {
            CrashInfoAppend(fmt("Machine: %s %s\n", s1, s2));
        }
    }
    {
        // get language
        char country[32] = {}, lang[32]{};
        GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, dimof(country) - 1);
        GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, dimof(lang) - 1);
        CrashInfoAppend(fmt("Lang: %s %s\n", Str(lang), Str(country)));
    }
    GetGraphicsDriverInfo();
    {
        auto cpu = CpuID();
        CrashInfoAppend(StrL("CPU: "));
        if (cpu & kCpuMMX) {
            CrashInfoAppend(StrL("MMX "));
        }
        if (cpu & kCpuSSE) {
            CrashInfoAppend(StrL("SSE "));
        }
        if (cpu & kCpuSSE2) {
            CrashInfoAppend(StrL("SSE2 "));
        }
        if (cpu & kCpuSSE3) {
            CrashInfoAppend(StrL("SSE3 "));
        }
        if (cpu & kCpuSSE41) {
            CrashInfoAppend(StrL("SSE41 "));
        }
        if (cpu & kCpuSSE42) {
            CrashInfoAppend(StrL("SSE42 "));
        }
        if (cpu & kCpuAVX) {
            CrashInfoAppend(StrL("AVX "));
        }
        if (cpu & kCpuAVX2) {
            CrashInfoAppend(StrL("AVX2 "));
        }
        if (cpu & kCpuNEON) {
            CrashInfoAppend(StrL("NEON "));
        }
        if (cpu & kCpuArmCrypto) {
            CrashInfoAppend(StrL("Crypto "));
        }
        if (cpu & kCpuArmAtomics) {
            CrashInfoAppend(StrL("Atomics "));
        }
        if (cpu & kCpuArmDotProd) {
            CrashInfoAppend(StrL("DotProd "));
        }
    }
}

// returns true if running on wine
static bool BuildModulesInfo() {
    CrashInfoStart(1024);
    bool isWine = GetModules(false);
    gModulesInfo = CrashInfoTake();
    return isWine;
}

static void BuildSystemInfo() {
    CrashInfoStart(1024);
    GetProgramInfo();
    GetOsVersion();
    GetSystemInfo();
    gSystemInfo = CrashInfoTake();
}

bool SetSymbolsDir(Str symDir) {
    if (!symDir) {
        return false;
    }
    gSymbolsDir = str::Dup(gCrashHandlerArena, symDir);
    return true;
}

static void __cdecl onSignalAbort(int /*sig*/) {
    // put the signal back because can be called many times
    // (from multiple threads) and raise() resets the handler
    signal(SIGABRT, onSignalAbort);
    CrashMe();
}

static void onTerminate() {
    CrashMe();
}

// The CRT calls this when an API is handed something it refuses to work with -
// atof(nullptr), a bad printf format, an out-of-range index in a checked
// iterator... The default handler is _invoke_watson(), which fails the process
// fast without going through SetUnhandledExceptionFilter or the vectored
// handler, so those crashes died silently with no report (#5909). Crash the way
// everything else here does, so the normal machinery walks the stack and sends
// a report. The arguments only carry anything in a debug CRT, so ignore them.
static void __cdecl onInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    CrashMe();
}

// new couldn't get the memory. Without a handler this ends in std::bad_alloc,
// which with _HAS_EXCEPTIONS=0 aborts somewhere down in the CRT; crashing here
// keeps the frame that asked for the memory on the stack, which is the only
// interesting part of an out-of-memory report. Never returns (returning 0 would
// tell new to give up, 1 to retry the allocation).
static int __cdecl onNewFailed(size_t) {
    CrashMe();
    return 0;
}

__unused static void onUnexpected() {
    CrashMe();
}

// shadow crt's _purecall() so that we're called instead of CRT.
// must keep external linkage: that's how it overrides the CRT's definition
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
        log(StrL("InstallCrashHandler: skipping because !crashDumpPath\n"));
        return;
    }

    // we pre-allocate as much as possible to minimize allocations
    // when crash handler is invoked. It's ok to use standard
    // allocation functions here.
    gCrashHandlerArena = ArenaNew();
    gCrashInfo = New<str::Builder>(gCrashHandlerArena);

    if (!SetSymbolsDir(symDir)) {
        log(StrL("InstallCrashHandler: skipping because !SetSymbolsDir()\n"));
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
        log(StrL("InstallCrashHandler: skipping because isWine\n"));
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
        if (len(prefsData) > 0) {
            // serialize without FileStates info because it's the largest
            Settings* gp = NewSettings(prefsData);
            DeleteFileStates(gp->fileStates);
            gp->fileStates = new Vec<FileState*>();
            // TODO: also sessionData?
            Str d = SerializeSettings(gp, {});
            gSettingsFile = str::Dup(gCrashHandlerArena, d);
            str::Free(d);
            DeleteSettings(gp);
            str::Free(prefsData);
        }
    }

    gDumpEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!gDumpEvent) {
        log(StrL("InstallCrashHandler: skipping because !gDumpEvent\n"));
        return;
    }
    gDumpThread = CreateThread(nullptr, 0, CrashDumpThread, nullptr, 0, &gDumpThreadId);
    if (!gDumpThread) {
        log(StrL("InstallCrashHandler: skipping because !gDumpThread\n"));
        return;
    }
    gPrevExceptionFilter = SetUnhandledExceptionFilter(CrashDumpExceptionHandler);
    // 1 means that our handler will be called first, 0 would be: last
    AddVectoredExceptionHandler(1, CrashDumpVectoredExceptionHandler);

    signal(SIGABRT, onSignalAbort);
#if COMPILER_MSVC
    // must be the global one: threads that never call the thread-local setter
    // (i.e. all of ours) fall back to it
    _set_invalid_parameter_handler(onInvalidParameter);
    _set_new_handler(onNewFailed);
    // deliberately not _set_new_mode(1): that would route failed malloc() to the
    // new handler too, and plenty of code here checks malloc for null and
    // recovers instead of dying
    //
    // abort() runs the SIGABRT handler above, which crashes and reports. Take
    // the CRT's own reactions out of the way: _CALL_REPORTFAULT would hand the
    // process to WER, _WRITE_ABORT_MSG prints a message no user of a GUI app
    // ever sees
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
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

    SafeCloseThreadHandle(&gDumpThread);
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
    gCrashInfo = nullptr;
    gCrashHandlerArena = nullptr;
    gCrashThreadId = 0;
    gDumpThreadId = 0;
    gLocalOnlyCrashHandler = false;
    InterlockedExchange(&gCrashHandlerStarted, 0);
}

// Tests that various ways to crash will generate crash report.
// Commented-out because they are ad-hoc. Left in code because
// I don't want to write them again if I ever need to test crash reporting

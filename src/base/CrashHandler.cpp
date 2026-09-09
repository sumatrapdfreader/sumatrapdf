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
#include "base/Win.h"
#include "base/CrashHandler.h"

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

// everything app-specific, handed to us by InstallCrashHandler()
static CrashHandlerConfig gCfg{};

Arena* CrashHandlerArena() {
    return gCrashHandlerArena;
}

static void CallCb(void (*cb)()) {
    if (cb) {
        cb();
    }
}

// The report is built here rather than threaded through every helper as a
// str::Builder&. Lives in the arena so there is no static ctor/dtor.
static str::Builder* gCrashInfo = nullptr;

void CrashInfoAppend(Str s) {
    if (!gCrashInfo) {
        return;
    }
    gCrashInfo->Append(s);
}

// start a fresh report, keeping the storage already allocated
static void CrashInfoStart(int cap) {
    if (!gCrashInfo) {
        return;
    }
    gCrashInfo->Reset();
    gCrashInfo->Reserve(cap);
}

static Str CrashInfoTake() {
    if (!gCrashInfo) {
        return {};
    }
    return gCrashInfo->TakeStr();
}

// exit code for a debug report (ReportIf) in a -for-testing run; test runners
// (tests/control.ts) treat it as "assertion fired", so keep the value in sync
constexpr UINT kDebugReportTestExitCode = 105;

// Note: intentionally not using ScopedMem<> to avoid
// static initializers/destructors, which are bad
static Str gSystemInfo;
static HANDLE gDumpEvent = nullptr;
static ThreadHandle gDumpThread = nullptr;
static bool gCrashed = false;
static volatile LONG gCrashHandlerStarted = 0;
static ThreadId gCrashThreadId = 0;
static ThreadId gDumpThreadId = 0;

static MINIDUMP_EXCEPTION_INFORMATION gMei{};
static LPTOP_LEVEL_EXCEPTION_FILTER gPrevExceptionFilter = nullptr;

static bool TryStartCrashHandling(Str handlerName) {
    if (InterlockedCompareExchange(&gCrashHandlerStarted, 1, 0) == 0) {
        gCrashThreadId = GetCurrentThreadId();
        CallCb(gCfg.onCrashBegin);
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
    CallCb(gCfg.appendExtraInfo);
    if (gSystemInfo) {
        CrashInfoAppend(gSystemInfo);
        CrashInfoAppend(StrL("\n"));
    }

    ThreadId crashedThreadId = gMei.ThreadId;
    if (gMei.ExceptionPointers) {
        dbghelp::GetExceptionInfo(*gCrashInfo, gMei.ExceptionPointers);
    } else if (captureCallstack) {
        crashedThreadId = GetCurrentThreadId();
        CrashInfoAppend(StrL("\nCrashed thread:\n"));
        dbghelp::GetCurrentThreadCallstack(*gCrashInfo);
    }

    if (captureCallstack) {
        CrashInfoAppend(StrL("\nOther threads:\n"));
        dbghelp::GetAllThreadsCallstacksExcept(*gCrashInfo, crashedThreadId);
        CrashInfoAppend(StrL("\n"));
    }

    return CrashInfoTake();
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

// Program info plus whatever the app adds; no stacks/modules/exception (those
// are in the .dmp itself).
static Str BuildMinidumpLogText(Str condStr, Str fileLine, bool isCrash) {
    CrashInfoStart(16 * 1024);
    if (isCrash) {
        CrashInfoAppend(StrL("Type: crash (minidump)\n"));
    } else {
        CrashInfoAppend(StrL("Type: debug report (not crash)\n"));
    }
    CrashInfoAppend(str::Format(gCrashHandlerArena, "Minidump: %s\n", gCfg.crashDumpPath));
    if (condStr) {
        // format into the pre-allocated crash arena, not the temp allocator
        CrashInfoAppend(str::Format(gCrashHandlerArena, "Cond: %s @ %s\n", condStr, fileLine));
    }
    CallCb(gCfg.appendProgramInfo);
    CallCb(gCfg.appendExtraInfo);
    CallCb(gCfg.appendMinidumpComment);
    return CrashInfoTake();
}

// Writes the .dmp with logText attached as its comment stream, then uploads it.
// mei is null for a debug report (no exception to describe).
static void WriteAndUploadMinidump(Str logText, MINIDUMP_EXCEPTION_INFORMATION* mei) {
    bool fullDump = false;
    if (len(gCfg.fullDumpEnvVar) > 0) {
        fullDump = GetEnvironmentVariableA(CStrTemp(gCfg.fullDumpEnvVar), nullptr, 0) != 0;
    }
    dir::CreateForFile(gCfg.crashDumpPath);
    TempWStr ws = ToWStrTemp(gCfg.crashDumpPath);
    dbghelp::WriteMiniDump(ws, mei, fullDump, logText);

    if (IsDebuggerPresent()) {
        log(StrL("WriteAndUploadMinidump: skipping upload, debugger present\n"));
        return;
    }
    if (len(gCfg.submitUrl) == 0) {
        log(StrL("WriteAndUploadMinidump: skipping upload, no submit url\n"));
        return;
    }

    Str dump = file::ReadFileWithArena(gCfg.crashDumpPath, gCrashHandlerArena);
    if (len(dump) == 0) {
        log(StrL("WriteAndUploadMinidump: empty dump, not uploading\n"));
        return;
    }

    str::Builder url(gCrashHandlerArena);
    url.Append(gCfg.submitUrl);
    if (gCfg.appendUploadQuery) {
        gCfg.appendUploadQuery(url);
    }
    Str urlStr = ToStr(url);
    HttpRsp rsp;
    bool ok = HttpPostUrl(urlStr, StrL("application/octet-stream"), {}, dump, &rsp);
    logf("WriteAndUploadMinidump: upload ok=%d status=%u err=%u bytes=%d url=%s\n", (int)ok,
         (unsigned)rsp.httpStatusCode, (unsigned)rsp.error, dump.len, urlStr);
}

static void HandleCrashWithMinidump() {
    log(StrL("HandleCrashWithMinidump\n"));

    Str logText = BuildMinidumpLogText(Str(), StrL(""), true);
    WriteCrashInfoToStdErr(logText);
    WriteAndUploadMinidump(logText, &gMei);
}

// Symbols are only ever the .pdb sitting next to the .exe (a local build); we
// no longer download them, released crashes are symbolized from the minidump.
static bool InitializeDbgHelp() {
    TempStr symPath = GetSelfExeDirTemp();
    TempWStr ws = ToWStrTemp(symPath);
    if (!dbghelp::Initialize(ws, false)) {
        logf("InitializeDbgHelp: dbghelp::Initialize('%s') failed\n", symPath);
        return false;
    }

    if (!dbghelp::HasSymbols()) {
        logf("InitializeDbgHelp(): dbghelp::HasSymbols(), symPath: '%s' failed\n", symPath);
        return false;
    }
    logf("InitializeDbgHelp(): did initialize ok, symPath: '%s'\n", symPath);
    return true;
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

    bool shouldUpload = isCrash ? gCfg.uploadCrashes : gCfg.uploadDebugReports;

    if (gCfg.localOnly) {
        auto s = BuildCrashInfoText(condStr, fileLine, isCrash, captureCallstack);
        if (len(s) == 0) {
            log(StrL("_uploadDebugReport(): skipping because !BuildCrashInfoText()\n"));
            return;
        }
        WriteCrashInfoToStdErr(s);
        log(s);
        log(StrL("_uploadDebugReport() finished local-only\n"));
        if (gCfg.forTesting && !isCrash && !IsDebuggerPresent()) {
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
            auto s = BuildCrashInfoText(condStr, fileLine, isCrash, captureCallstack);
            if (len(s) == 0) {
                log(StrL("_uploadDebugReport(): skipping because !BuildCrashInfoText()\n"));
                return;
            }
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

    logf("_uploadDebugReport: isCrash: %d\n", (int)isCrash);

    // a debug report has no exception, so the .dmp only carries the stacks
    Str logText = BuildMinidumpLogText(condStr, fileLine, isCrash);
    WriteAndUploadMinidump(logText, nullptr);
    log(logText);
    log(StrL("_uploadDebugReport() finished\n"));
}

static DWORD WINAPI CrashDumpThread(LPVOID /*data*/) {
    WaitForSingleObject(gDumpEvent, INFINITE);
    if (!gCrashed) {
        return 0;
    }

    log(StrL("CrashDumpThread\n"));
    HandleCrashWithMinidump();
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

    if (!gCfg.localOnly) {
        CallCb(gCfg.showCrashMessage);
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

    if (!gCfg.localOnly) {
        CallCb(gCfg.showCrashMessage);
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
    if (len(name) == 0) {
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
        if (len(v) == 0) {
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
        // get computer name
        TempStr s1 =
            ReadRegStrTemp(HKEY_LOCAL_MACHINE, StrL(R"(HARDWARE\DESCRIPTION\System\BIOS)"), StrL("SystemFamily"));
        TempStr s2 =
            ReadRegStrTemp(HKEY_LOCAL_MACHINE, StrL(R"(HARDWARE\DESCRIPTION\System\BIOS)"), StrL("SystemVersion"));

        if (len(s1) == 0 && len(s2) == 0) {
            // no-op
        } else if (len(s1) == 0) {
            CrashInfoAppend(fmt("Machine: %s\n", s2));
        } else if (len(s2) == 0 || str::EqI(s1, s2)) {
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

static void BuildSystemInfo() {
    CrashInfoStart(1024);
    CallCb(gCfg.appendProgramInfo);
    GetOsVersion();
    GetSystemInfo();
    gSystemInfo = CrashInfoTake();
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

void InstallCrashHandler(const CrashHandlerConfig& cfg) {
    ReportIf(gDumpEvent || gDumpThread);

    if (len(cfg.crashDumpPath) == 0) {
        log(StrL("InstallCrashHandler: skipping because !crashDumpPath\n"));
        return;
    }

    // we pre-allocate as much as possible to minimize allocations
    // when crash handler is invoked. It's ok to use standard
    // allocation functions here.
    gCrashHandlerArena = ArenaNew();
    gCrashInfo = New<str::Builder>(gCrashHandlerArena, gCrashHandlerArena);

    logf("InstallCrashHandler:\n  crashDumpPath: '%s'\n", cfg.crashDumpPath);

    gCfg = cfg;
    // the caller's strings can be temporary, ours must outlive any crash
    gCfg.crashDumpPath = str::Dup(gCrashHandlerArena, cfg.crashDumpPath);
    gCfg.submitUrl = str::Dup(gCrashHandlerArena, cfg.submitUrl);
    gCfg.fullDumpEnvVar = str::Dup(gCrashHandlerArena, cfg.fullDumpEnvVar);
    gCrashThreadId = 0;
    gDumpThreadId = 0;
    InterlockedExchange(&gCrashHandlerStarted, 0);

    // don't bother sending crash reports when running under Wine
    // as they're not helpful
    if (IsRunningOnWine()) {
        log(StrL("InstallCrashHandler: skipping because isWine\n"));
        return;
    }

    BuildSystemInfo();
    // do it now so that a crash doesn't have to load dbghelp and its symbols
    InitializeDbgHelp();

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
    gSystemInfo = {};
    gCfg = {};
    ArenaDelete(gCrashHandlerArena);
    gCrashInfo = nullptr;
    gCrashHandlerArena = nullptr;
    gCrashThreadId = 0;
    gDumpThreadId = 0;
    InterlockedExchange(&gCrashHandlerStarted, 0);
}

// Tests that various ways to crash will generate crash report.
// Commented-out because they are ad-hoc. Left in code because
// I don't want to write them again if I ever need to test crash reporting

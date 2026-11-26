/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

// ng: base/CrashHandler.cpp is win32 minidumps + dbghelp. POSIX has no
// equivalent, so this is the same API backed by a signal handler that writes
// a text crash report (the app's getCrashComment plus the stacks) next to
// where the .dmp would have gone, and logs it. A report meant for the server
// is also left as a pending file and POSTed to submitUrl on the next start:
// a crashed process is in no state to talk to the network.

#include "base/Base.h"

#include "base/File.h"
#include "base/DirScan.h"
#include "base/Http.h"
#include "base/CrashHandler.h"

#include <csignal>
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#if OS_WASM
#include <emscripten/emscripten.h>
#else
#include <execinfo.h>
#endif
#if OS_DARWIN
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

static Arena* gCrashHandlerArena = nullptr;
static CrashHandlerConfig gCfg{};
static Str gSysInfo;
// Pre-computed at InstallCrashHandler() time so SignalHandler never allocates
// paths from the temp arena or calls dir::CreateForFile().
static Str gReportBasePath;
static Str gReportTxtPath;

Arena* CrashHandlerArena() {
    return gCrashHandlerArena;
}

Str CrashHandlerDumpPath() {
    return gCfg.crashDumpPath;
}

Str CrashHandlerSystemInfo() {
    return gSysInfo;
}

constexpr int kMaxFrames = 64;

// first line of every report; the crash server rejects a body without it
#define kCrashReportMagic "Crash report v1\n"

constexpr int kAltStackSize = 128 * 1024;
// a handler stuck on a lock the faulting code held must still end the process
constexpr int kHandlerTimeoutSecs = 20;
// showCrashMessage waits for the user; its own timeout must be shorter
constexpr int kCrashMessageTimeoutSecs = 90;
// pending reports are <dump name>-<unix time>-pending.txt
#define kPendingSuffix "-pending.txt"
constexpr int kPendingMaxAgeSecs = 7 * 24 * 60 * 60;
constexpr int kMaxPendingReports = 5;
// the server keeps at most 1 MB (crashReportMaxBytes in crash_report.go)
constexpr int kMaxUploadBytes = 1000 * 1000;
constexpr int kUploadHeadBytes = 64 * 1024;
constexpr int kHttpClientErrorMin = 400;
constexpr int kHttpClientErrorMax = 499;

#if OS_DARWIN
void MacAppendSignalInfo(str::Builder& b, int sig, void* sigInfo);
void MacAppendStacks(str::Builder& b, void* uctx);
TempStr MacSysInfoTemp();
#endif

#if OS_WASM

// emscripten has no execinfo.h; the runtime formats the JS/wasm stack instead
static void AppendBacktrace(str::Builder& b) {
    constexpr int kMaxStackChars = 8 * 1024;
    char* buf = AllocArrayTemp<char>(kMaxStackChars);
    if (emscripten_get_callstack(EM_LOG_C_STACK, buf, kMaxStackChars) <= 0) {
        b.Append(StrL("(no backtrace)\n"));
        return;
    }
    b.Append(Str(buf));
    b.Append(StrL("\n"));
}

#elif !OS_DARWIN
// Used only for non-crash debug reports; crashes use backtrace_symbols_fd()
// to avoid malloc() inside a signal handler.
static void AppendBacktrace(str::Builder& b) {
    void* frames[kMaxFrames];
    int n = backtrace(frames, kMaxFrames);
    char** syms = backtrace_symbols(frames, n);
    if (!syms) {
        b.Append(StrL("(no backtrace)\n"));
        return;
    }
    for (int i = 0; i < n; i++) {
        b.Append(Str(syms[i]));
        b.Append(StrL("\n"));
    }
    free(syms);
}
#endif

// the report path is the .dmp path with the extension swapped: a POSIX crash
// has no minidump, only text
static TempStr ReportPathTemp(Str suffix) {
    if (len(gCfg.crashDumpPath) == 0) {
        return {};
    }
    TempStr path = str::DupTemp(gCfg.crashDumpPath);
    TempStr ext = path::GetExtTemp(path);
    if (len(ext) > 0) {
        path.len -= ext.len;
        path.s[path.len] = 0;
    }
    return str::JoinTemp(path, suffix);
}

// pthread_self() of the thread writing a report, 0 if none
static uintptr_t gReportingThread = 0;

enum class ReportSlot {
    Taken,
    Nested, // this thread faulted while writing a report
};

// One report at a time. A thread that finds another one reporting waits: a
// crash report ends the process, a debug report frees the slot.
static ReportSlot TakeReportSlot() {
    uintptr_t self = (uintptr_t)pthread_self();
    timespec wait{0, 10 * 1000 * 1000};
    for (;;) {
        uintptr_t prev = __sync_val_compare_and_swap(&gReportingThread, (uintptr_t)0, self);
        if (prev == 0) {
            return ReportSlot::Taken;
        }
        if (prev == self) {
            return ReportSlot::Nested;
        }
        nanosleep(&wait, nullptr);
    }
}

static void FreeReportSlot() {
    __sync_lock_release(&gReportingThread);
}

// index just past the '\n' at or after i, so a cut never splits a UTF-8 sequence
static int NextLineStart(Str s, int i) {
    if (i <= 0) {
        return 0;
    }
    while (i < len(s) && s.s[i - 1] != '\n') {
        i++;
    }
    return i;
}

// Fit a report in what the server keeps. Drop the middle, the start of the
// log, and keep the head (build info) and the tail (end of
// the log, settings, stacks, modules).
#define kCutMarker "\n... (truncated) ...\n\n"
// Async-signal-safe decimal formatter into a caller-supplied buffer.
static int FormatDecBuf(char* dst, int cap, i64 v) {
    char rev[24];
    u64 u;
    int pos = 0;
    if (v < 0) {
        if (pos < cap) {
            dst[pos++] = '-';
        }
        u = (u64)(-(v + 1)) + 1;
    } else {
        u = (u64)v;
    }
    int n = 0;
    do {
        rev[n++] = (char)('0' + (u % 10));
        u /= 10;
    } while (u != 0);
    while (n > 0 && pos < cap) {
        dst[pos++] = rev[--n];
    }
    return pos;
}

static bool FormatPendingPath(char* dst, int cap, i64 unixTime) {
    if (len(gReportBasePath) == 0 || cap <= 0) {
        return false;
    }
    int pos = 0;
    auto append = [&](Str s) -> bool {
        if (len(s) > cap - 1 - pos) {
            return false;
        }
        memcpy(dst + pos, s.s, (size_t)len(s));
        pos += len(s);
        return true;
    };
    char numBuf[24];
    int numLen = FormatDecBuf(numBuf, sizeofi(numBuf), unixTime);
    if (!append(gReportBasePath) || !append(StrL("-")) || !append(Str(numBuf, numLen)) ||
        !append(StrL(kPendingSuffix))) {
        return false;
    }
    dst[pos] = 0;
    return true;
}

// Write all bytes to fd without stdio buffering or heap allocation.
static void WriteAllFd(int fd, Str s) {
    const char* p = s.s;
    size_t left = (size_t)len(s);
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n > 0) {
            p += n;
            left -= (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
}

static void WriteStderr(Str s) {
    WriteAllFd(STDERR_FILENO, s);
}

// Write report (and optional raw backtrace frames) to an open fd, truncating
// the middle when maxBytes > 0 and report exceeds it.
static void WriteReportToFd(int fd, Str report, void* const* frames, int nFrames, int maxBytes) {
    if (maxBytes <= 0 || len(report) <= maxBytes) {
        WriteAllFd(fd, report);
    } else {
        int cutMarkerLen = (int)sizeof(kCutMarker) - 1;
        int headEnd = NextLineStart(report, kUploadHeadBytes);
        if (headEnd > maxBytes - cutMarkerLen) {
            headEnd = maxBytes - cutMarkerLen;
        }
        int tailLen = maxBytes - headEnd - cutMarkerLen;
        int tailStart = tailLen > 0 ? NextLineStart(report, len(report) - tailLen) : len(report);
        WriteAllFd(fd, Str(report.s, headEnd));
        WriteAllFd(fd, StrL(kCutMarker));
        if (tailStart < len(report)) {
            WriteAllFd(fd, Str(report.s + tailStart, len(report) - tailStart));
        }
    }
#if !OS_WASM && !OS_DARWIN
    if (frames && nFrames > 0) {
        backtrace_symbols_fd((void* const*)frames, nFrames, fd);
    }
#else
    (void)frames;
    (void)nFrames;
#endif
}

static bool ShouldUpload(bool isCrash) {
    if (gCfg.localOnly || len(gCfg.submitUrl) == 0 || gCfg.UploadsDisabled()) {
        return false;
    }
    return isCrash ? gCfg.uploadCrashes : gCfg.uploadDebugReports;
}

static void WriteReportFileRaw(const char* path, Str report, void* const* frames, int nFrames, int maxBytes) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;
    }
    WriteReportToFd(fd, report, frames, nFrames, maxBytes);
    close(fd);
}

// sig, sigInfo and uctx are the signal handler's arguments; 0 / null for a debug report
static void WriteCrashReport(Str condStr, Str fileLine, bool isCrash, int sig, void* sigInfo, void* uctx) {
    str::Builder b(gCrashHandlerArena);
    b.Reserve(16 * 1024);
    b.Append(StrL(kCrashReportMagic));
#if OS_DARWIN
    if (sig != 0) {
        MacAppendSignalInfo(b, sig, sigInfo);
    }
#else
    (void)sig;
    (void)sigInfo;
    (void)uctx;
#endif
    if (gCfg.getCrashComment) {
        b.Append(gCfg.getCrashComment(gCrashHandlerArena, condStr, fileLine, isCrash));
    }
    void* crashFrames[kMaxFrames];
    int nCrashFrames = 0;
#if OS_DARWIN
    MacAppendStacks(b, uctx);
#elif OS_WASM
    b.Append(StrL("\n--- backtrace ---\n"));
    AppendBacktrace(b);
#else
    b.Append(StrL("\n--- backtrace ---\n"));
    if (isCrash) {
        nCrashFrames = backtrace(crashFrames, kMaxFrames);
        if (nCrashFrames <= 0) {
            b.Append(StrL("(no backtrace)\n"));
        }
    } else {
        AppendBacktrace(b);
    }
#endif
    Str report = ToStr(b);
    if (len(gReportTxtPath) > 0) {
        WriteReportFileRaw(gReportTxtPath.s, report, crashFrames, nCrashFrames, 0);
        if (ShouldUpload(isCrash)) {
            char pendingPath[1024];
            if (FormatPendingPath(pendingPath, sizeofi(pendingPath), (i64)time(nullptr))) {
                WriteReportFileRaw(pendingPath, report, crashFrames, nCrashFrames, kMaxUploadBytes);
            }
        }
    }
    if (!isCrash) {
        logf("crash report written to '%s'\n%s\n", gReportTxtPath, report);
        return;
    }
    // log() takes a lock the faulting code may hold
    WriteStderr(StrL("crash report written to '"));
    WriteStderr(gReportTxtPath);
    WriteStderr(StrL("'\n"));
    WriteReportToFd(STDERR_FILENO, report, crashFrames, nCrashFrames, 0);
}

static void RestoreDefaultSignals();

static void UnblockAndRaise(int sig) {
#if !OS_WASM
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
#endif
    raise(sig);
}

static void SignalHandler(int sig, siginfo_t* info, void* uctx) {
    if (TakeReportSlot() == ReportSlot::Nested) {
        RestoreDefaultSignals();
        UnblockAndRaise(sig);
        return;
    }
    // restore the default so a fault while reporting terminates us
    signal(sig, SIG_DFL);
    alarm(kHandlerTimeoutSecs);
    if (gCfg.onCrashBegin) {
        gCfg.onCrashBegin();
    }
    char condBuf[32];
    memcpy(condBuf, "signal ", 7);
    int n = 7 + FormatDecBuf(condBuf + 7, sizeofi(condBuf) - 8, sig);
    condBuf[n] = 0;
    WriteCrashReport(Str(condBuf, n), {}, true, sig, info, uctx);
    if (gCfg.showCrashMessage && !gCfg.localOnly) {
        alarm(kCrashMessageTimeoutSecs);
        gCfg.showCrashMessage();
    }
    UnblockAndRaise(sig);
}

NO_INLINE void _uploadDebugReport(Str condStr, Str fileLine, bool isCrash) {
    if (!gCrashHandlerArena) {
        return;
    }
    if (TakeReportSlot() == ReportSlot::Nested) {
        return;
    }
    // a ReportIf in a hot path would rebuild the report into the never-reset
    // crash arena and rewrite the report file on every hit: write one per session
    static bool didWriteDebugReport = false;
    if (!isCrash && didWriteDebugReport) {
        FreeReportSlot();
        logf("_uploadDebugReport: %s %s\n", condStr, fileLine);
        return;
    }
    if (!isCrash) {
        didWriteDebugReport = true;
    }
    WriteCrashReport(condStr, fileLine, isCrash, 0, nullptr, nullptr);
    FreeReportSlot();
    if (gCfg.forTesting && !isCrash) {
        // a ReportIf() under -for-testing must fail the test run
        _exit(105);
    }
}

// lldb/gdb are already attached when ng-dbg launches the process. Leave the
// signal to them so the stop is the fault, not this handler.
static bool DebuggerAttached() {
#if OS_DARWIN
    struct kinfo_proc info{};
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    size_t size = sizeof(info);
    if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0) {
        return false;
    }
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#elif OS_LINUX
    int fd = open("/proc/self/status", O_RDONLY);
    if (fd < 0) {
        return false;
    }
    char buf[4096];
    int n = (int)read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        return false;
    }
    buf[n] = 0;
    const char* p = strstr(buf, "TracerPid:");
    if (!p) {
        return false;
    }
    p += 10;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p != '0';
#else
    return false;
#endif
}

static const int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};

static void RestoreDefaultSignals() {
    for (int sig : kSignals) {
        signal(sig, SIG_DFL);
    }
}

static void SetSignalHandlers() {
    struct sigaction sa{};
    sa.sa_sigaction = SignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
#if !OS_WASM
    // a stack overflow leaves no room on the thread's own stack
    static char altStack[kAltStackSize];
    stack_t ss{};
    ss.ss_sp = altStack;
    ss.ss_size = kAltStackSize;
    if (sigaltstack(&ss, nullptr) == 0) {
        sa.sa_flags |= SA_ONSTACK;
    }
#endif
    for (int sig : kSignals) {
        sigaction(sig, &sa, nullptr);
    }
}

#if !OS_WASM
struct PendingUpload {
    StrVec paths;
    Str url;
};

static bool IsHttpClientError(int status) {
    return status >= kHttpClientErrorMin && status <= kHttpClientErrorMax;
}

static void UploadPending(Str path, Str url) {
    Str report = file::ReadFile(path);
    HttpRsp rsp;
    bool ok = len(report) > 0 && HttpPostUrl(url, StrL("text/plain"), {}, report, &rsp);
    int status = (int)rsp.httpStatusCode;
    logf("UploadPending: ok=%d status=%d bytes=%d url=%s\n", (int)ok, status, len(report), url);
    // a report the server refused stays refused; an unreachable server gets another try
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    bool isOld = FileTimeDiffInSecs(now, file::GetModificationTime(path)) > kPendingMaxAgeSecs;
    if (ok || IsHttpClientError(status) || isOld || len(report) == 0) {
        file::Delete(path);
    }
    str::Free(report);
}

static void UploadPendingThread(PendingUpload* d) {
    for (Str path : d->paths) {
        if (gCfg.UploadsDisabled()) {
            break;
        }
        UploadPending(path, d->url);
    }
    str::Free(d->url);
    delete d;
}

// Sends the reports earlier runs left behind, newest kMaxPendingReports;
// older ones are deleted.
static void UploadPendingReports() {
    if (gCfg.localOnly || len(gCfg.submitUrl) == 0 || gCfg.UploadsDisabled() ||
        (!gCfg.uploadCrashes && !gCfg.uploadDebugReports)) {
        return;
    }
    TempStr dir = path::GetDirTemp(gCfg.crashDumpPath);
    TempStr prefix = path::GetBaseNameTemp(ReportPathTemp({}));
    StrVec paths;
    DirIter di{dir};
    for (DirIterEntry* de : di) {
        if (str::StartsWith(de->name, prefix) && str::EndsWith(de->name, StrL(kPendingSuffix))) {
            paths.Append(de->filePath);
        }
    }
    if (len(paths) == 0) {
        return;
    }
    // names differ only in the unix time
    Sort(&paths);
    auto d = new PendingUpload();
    for (int i = 0; i < len(paths); i++) {
        if (i < len(paths) - kMaxPendingReports) {
            file::Delete(paths[i]);
            continue;
        }
        d->paths.Append(paths[i]);
    }
    d->url = str::Dup(gCfg.submitUrl);
    RunAsync(MkFunc0<PendingUpload>(UploadPendingThread, d), StrL("UploadCrashReport"));
}
#endif

void InstallCrashHandler(const CrashHandlerConfig& cfg) {
    if (gCrashHandlerArena || DebuggerAttached()) {
        return;
    }
    gCrashHandlerArena = ArenaNew();
    gCfg = cfg;
    gCfg.crashDumpPath = str::Dup(gCrashHandlerArena, cfg.crashDumpPath);
    gCfg.submitUrl = str::Dup(gCrashHandlerArena, cfg.submitUrl);
    gCfg.fullDumpEnvVar = str::Dup(gCrashHandlerArena, cfg.fullDumpEnvVar);
    TempStr basePath = ReportPathTemp({});
    if (len(basePath) > 0) {
        gReportBasePath = str::Dup(gCrashHandlerArena, basePath);
        gReportTxtPath = str::Dup(gCrashHandlerArena, str::JoinTemp(basePath, StrL(".txt")));
        dir::CreateForFile(gReportTxtPath);
    }
#if OS_DARWIN
    gSysInfo = str::Dup(gCrashHandlerArena, MacSysInfoTemp());
#else
    gSysInfo = str::Dup(gCrashHandlerArena, StrL("Os: posix\n"));
#endif

    SetSignalHandlers();
#if !OS_WASM
    UploadPendingReports();
#endif
}

void UninstallCrashHandler() {
    if (!gCrashHandlerArena) {
        return;
    }
    RestoreDefaultSignals();
    gCfg = {};
    gSysInfo = {};
    gReportBasePath = {};
    gReportTxtPath = {};
    ArenaDelete(gCrashHandlerArena);
    gCrashHandlerArena = nullptr;
}

/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

// ng: base/CrashHandler.cpp is win32 minidumps + dbghelp. POSIX has no
// equivalent, so this is the same API backed by a signal handler that writes
// the crash report (the app's getCrashComment plus a backtrace) next to where
// the .dmp would have gone, and logs it. No upload: the minidump service only
// takes Windows dumps.

#include "base/Base.h"

#include "base/File.h"
#include "base/CrashHandler.h"

#include <csignal>
#include <unistd.h>
#if OS_WASM
#include <emscripten/emscripten.h>
#else
#include <execinfo.h>
#endif

static Arena* gCrashHandlerArena = nullptr;
static CrashHandlerConfig gCfg{};
static Str gSysInfo;

Arena* CrashHandlerArena() {
    return gCrashHandlerArena;
}

Str CrashHandlerSystemInfo() {
    return gSysInfo;
}

constexpr int kMaxFrames = 64;

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

#else

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
static TempStr ReportPathTemp() {
    if (len(gCfg.crashDumpPath) == 0) {
        return {};
    }
    TempStr path = str::DupTemp(gCfg.crashDumpPath);
    TempStr ext = path::GetExtTemp(path);
    if (len(ext) > 0) {
        path.len -= ext.len;
        path.s[path.len] = 0;
    }
    return str::JoinTemp(path, StrL(".txt"));
}

static void WriteCrashReport(Str condStr, Str fileLine, bool isCrash) {
    str::Builder b(gCrashHandlerArena);
    b.Reserve(16 * 1024);
    if (gCfg.getCrashComment) {
        b.Append(gCfg.getCrashComment(gCrashHandlerArena, condStr, fileLine, isCrash));
    }
    b.Append(StrL("\n--- backtrace ---\n"));
    AppendBacktrace(b);
    Str report = ToStr(b);
    TempStr path = ReportPathTemp();
    if (len(path) > 0) {
        dir::CreateForFile(path);
        file::WriteFile(path, report);
    }
    logf("crash report written to '%s'\n%s\n", path, report);
}

static void SignalHandler(int sig) {
    // restore the default so a fault while reporting terminates us
    signal(sig, SIG_DFL);
    if (gCfg.onCrashBegin) {
        gCfg.onCrashBegin();
    }
    WriteCrashReport(fmt("signal %d", sig), {}, true);
    if (gCfg.showCrashMessage && !gCfg.localOnly) {
        gCfg.showCrashMessage();
    }
    raise(sig);
}

NO_INLINE void _uploadDebugReport(Str condStr, Str fileLine, bool isCrash) {
    if (!gCrashHandlerArena) {
        return;
    }
    WriteCrashReport(condStr, fileLine, isCrash);
    if (gCfg.forTesting && !isCrash) {
        // a ReportIf() under -for-testing must fail the test run
        _exit(105);
    }
}

void InstallCrashHandler(const CrashHandlerConfig& cfg) {
    if (gCrashHandlerArena) {
        return;
    }
    gCrashHandlerArena = ArenaNew();
    gCfg = cfg;
    gCfg.crashDumpPath = str::Dup(gCrashHandlerArena, cfg.crashDumpPath);
    gCfg.submitUrl = str::Dup(gCrashHandlerArena, cfg.submitUrl);
    gCfg.fullDumpEnvVar = str::Dup(gCrashHandlerArena, cfg.fullDumpEnvVar);
    gSysInfo = str::Dup(gCrashHandlerArena, StrL("Os: posix\n"));

    static const int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    for (int sig : kSignals) {
        signal(sig, SignalHandler);
    }
}

void UninstallCrashHandler() {
    if (!gCrashHandlerArena) {
        return;
    }
    static const int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    for (int sig : kSignals) {
        signal(sig, SIG_DFL);
    }
    ArenaDelete(gCrashHandlerArena);
    gCrashHandlerArena = nullptr;
}

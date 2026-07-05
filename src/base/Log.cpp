/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Thread.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/Log.h"

constexpr const WCHAR* kPipeName = L"\\\\.\\pipe\\LOCAL\\ArsLexis-Logger";

Str gLogAppName = StrL("SumatraPDF");

Mutex gLogMutex;

// we use a dedicated Arena so we can do logging during crash handling
// where we want to avoid allocator deadlocks by calling malloc()
Arena* gLogAllocator = nullptr;

str::Builder* gLogBuf = nullptr;
bool gLogToConsole = false;
// we always log if IsDebuggerPresent()
// this forces logging to debuger always
bool gLogToDebugger = false;
// meant to avoid doing stuff during crash reporting
// will log to debugger (if no need for formatting)
bool gReducedLogging = false;
// when main thread exists other threads might still
// try to log. when true, this stops logging
bool gDestroyedLogging = false;

// if true, doesn't log if the same text has already been logged
// reduces logging but also can be confusing i.e. log lines are not showing up
bool gSkipDuplicateLines = false;

bool gLogToPipe = true;
HANDLE hLogPipe = INVALID_HANDLE_VALUE;
static Mutex gPipeMutex;

Str gLogFilePath;

// 1 MB - 128 to stay under 1 MB even after appending (an estimate)
constexpr int kMaxLogBuf = 1024 * 1024 - 128;

#if 0
// TODO: add more codes
static const char* getWinError(DWORD errCode) {
#define V(err) \
    case err:  \
        return #err
    switch (errCode) {
        V(ERROR_PIPE_LOCAL);
        V(ERROR_BAD_PIPE);
        V(ERROR_PIPE_BUSY);
        V(ERROR_NO_DATA);
        V(ERROR_PIPE_NOT_CONNECTED);
        V(ERROR_MORE_DATA);
        V(ERROR_NO_WORK_DONE);
    }
    return "error code unknown";
#undef V
}
#endif

static LARGE_INTEGER lastPipeOpenTryTime = {};

static void maybeOpenLogPipe() {
    // only re-try every 10 secs to minimize cost because pipe is rarely
    // opened and logging is frequent
    if (lastPipeOpenTryTime.QuadPart != 0) {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double diffSecs =
            static_cast<double>(now.QuadPart - lastPipeOpenTryTime.QuadPart) / static_cast<double>(freq.QuadPart);
        if (diffSecs < 10.0f) {
            return;
        }
    }
    QueryPerformanceCounter(&lastPipeOpenTryTime);
    hLogPipe = CreateFileW(kPipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    // TODO: retry if ERROR_PIPE_BUSY?
}

static void logToPipe(Str s) {
    if (!gLogToPipe) {
        return;
    }
    if (!s) {
        return;
    }
    size_t n = (size_t)s.len;

    gPipeMutex.Lock();

    DWORD cbWritten = 0;
    bool didConnect = false;
    if (!IsValidHandle(hLogPipe)) {
        maybeOpenLogPipe();
        if (!IsValidHandle(hLogPipe)) {
            gPipeMutex.Unlock();
            return;
        }
        didConnect = true;
    }

    if (didConnect) {
        // logview accepts logging from anyone, so announce ourselves
        TempStr initialMsg = fmt("app: %s\n", gLogAppName);
        WriteFile(hLogPipe, initialMsg.s, (DWORD)initialMsg.len, &cbWritten, nullptr);
    }

    DWORD cb = (DWORD)n;
    BOOL ok = WriteFile(hLogPipe, s.s, cb, &cbWritten, nullptr);
    if (!ok) {
        CloseHandle(hLogPipe);
        hLogPipe = INVALID_HANDLE_VALUE;
    }

    gPipeMutex.Unlock();
}

static void log2(Str s, bool always) {
    bool skipLog = !always && gSkipDuplicateLines && gLogBuf && str::Contains(*gLogBuf, s);

    if (!skipLog) {
        // in reduced logging mode, we do want to log to at least the debugger
        if (gLogToDebugger || IsDebuggerPresent() || gReducedLogging) {
            OutputDebugStringA(s.s);
        }
    }
    if (gDestroyedLogging) {
        return;
    }
    if (gReducedLogging) {
        // if the pipe already connected, do log to it even if disabled
        // we do want easy logging, just want to reduce doing stuff
        // that can break crash handling
        if (gLogToPipe && IsValidHandle(hLogPipe)) {
            logToPipe(s);
        }
        return;
    }
    gLogMutex.Lock();

    InterlockedIncrement(&gAllowAllocFailure);
    defer {
        InterlockedDecrement(&gAllowAllocFailure);
    };

    if (!gLogBuf) {
        gLogAllocator = ArenaNew();
        gLogBuf = new str::Builder(32 * 1024, gLogAllocator);
    } else {
        if (len(*gLogBuf) > kMaxLogBuf) {
            // TODO: use gLogBuf->Clear(), which doesn't free the allocated space
            gLogBuf->Reset();
        }
    }

    size_t n = (size_t)s.len;

    // when skipping, we skip buf (crash reports) and console
    // but write to file and logview
    if (!skipLog) {
        gLogBuf->Append(s);
    }

    if (!skipLog && gLogToConsole) {
        LogConsole(s);
    }

    if (gLogFilePath) {
        auto f = fopen(gLogFilePath.s, "a");
        if (f != nullptr) {
            fwrite(s.s, 1, n, f);
            fflush(f);
            fclose(f);
        }
    }
    logToPipe(s);
    gLogMutex.Unlock();
}

void log(Str s) {
    log2(s, false);
}

void loga(Str s) {
    if (gDestroyedLogging) {
        return;
    }
    log2(s, true);
}

void StartLogToFile(Str path, bool removeIfExists) {
    ReportIf(gLogFilePath);
    gLogFilePath = str::Dup(path);
    if (removeIfExists) {
        file::Delete(path);
    }
}

bool WriteCurrentLogToFile(Str path) {
    if (!gLogBuf) return false;
    Str slice = ToStr(*gLogBuf);
    if (len(slice) == 0) {
        return false;
    }
    bool ok = dir::CreateForFile(path);
    if (!ok) {
        logf("WriteCurrentLogToFile: dir::CreateForFile('%s') failed\n", path);
        return false;
    }
    ok = file::WriteFile(path, slice);
    if (!ok) {
        logf("WriteCurrentLogToFile: file::WriteFile('%s') failed\n", path);
    }
    return ok;
}

void DestroyLogging() {
    gDestroyedLogging = true;
    gLogMutex.Lock();
    delete gLogBuf;
    gLogBuf = nullptr;
    ArenaDelete(gLogAllocator);
    gLogAllocator = nullptr;
    gLogMutex.Unlock();
    str::FreePtr(&gLogFilePath);
}

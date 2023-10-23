/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"

constexpr const WCHAR* kPipeName = L"\\\\.\\pipe\\LOCAL\\ArsLexis-Logger";

const char* gLogAppName = "SumatraPDF";

Mutex gLogMutex;

// we use HeapAllocator because we can do logging during crash handling
// where we want to avoid allocator deadlocks by calling malloc()
HeapAllocator* gLogAllocator = nullptr;

str::Str* gLogBuf = nullptr;
bool gLogToConsole = false;
// we always log if IsDebuggerPresent()
// this forces logging to debuger always
bool gLogToDebugger = false;
// meant to avoid doing stuff during crash reporting
// will log to debugger (if no need for formatting)
bool gReducedLogging = false;
// when main thread exists other threads might still
// try to log. when true, this stops logging
bool gStopLogging = false;

// if true, doesn't log if the same text has already been logged
// reduces logging but also can be confusing i.e. log lines are not showing up
bool gSkipDuplicateLines = false;

bool gLogToPipe = true;
HANDLE hLogPipe = INVALID_HANDLE_VALUE;

char* gLogFilePath = nullptr;

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

static void logToPipe(const char* s, size_t n = 0) {
    if (!gLogToPipe) {
        return;
    }
    if (!s || (*s == 0)) {
        return;
    }
    if (n == 0) {
        n = str::Len(s);
    }

    DWORD cbWritten = 0;
    BOOL ok = false;
    bool didConnect = false;
    if (!IsValidHandle(hLogPipe)) {
        // try open pipe for logging
        hLogPipe = CreateFileW(kPipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!IsValidHandle(hLogPipe)) {
            // TODO: retry if ERROR_PIPE_BUSY ?
            // TODO: maybe remember when last we tried to open it and don't try to open for the
            // next 10 secs, to minimize CreateFileW() calls
            return;
        }
        didConnect = true;
    }

    // TODO: do I need this if I don't read from the pipe?
    DWORD mode = PIPE_READMODE_MESSAGE;
    ok = SetNamedPipeHandleState(hLogPipe, &mode, nullptr, nullptr);
    if (!ok) {
        OutputDebugStringA("logPipe: SetNamedPipeHandleState() failed\n");
    }

    if (didConnect) {
        // logview accepts logging from anyone, so announce ourselves
        TempStr initialMsg = str::FormatTemp("app: %s\n", gLogAppName);
        WriteFile(hLogPipe, initialMsg, (DWORD)str::Len(initialMsg), &cbWritten, nullptr);
    }

    DWORD cb = (DWORD)n;
    // TODO: what happens when we write more than the server can read?
    // should I loop if cbWritten < cb?
    ok = WriteFile(hLogPipe, s, cb, &cbWritten, nullptr);
    if (!ok) {
#if 0
        DWORD err = GetLastError();
        OutputDebugStringA("logPipe: WriteFile() failed with error: ");
        char buf[256]{};
        snprintf(buf, sizeof(buf) - 1, "%d %s\n", (int)err, getWinError(err));
        OutputDebugStringA(buf);
#endif
        CloseHandle(hLogPipe);
        hLogPipe = INVALID_HANDLE_VALUE;
    }
}

void log(const char* s, bool always) {
    bool skipLog = !always && gSkipDuplicateLines && gLogBuf && gLogBuf->Contains(s);

    if (!skipLog) {
        // in reduced logging mode, we do want to log to at least the debugger
        if (gLogToDebugger || IsDebuggerPresent() || gReducedLogging) {
            OutputDebugStringA(s);
        }
    }
    if (gStopLogging) {
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
        gLogAllocator = new HeapAllocator();
        gLogBuf = new str::Str(32 * 1024, gLogAllocator);
    } else {
        if (gLogBuf->isize() > kMaxLogBuf) {
            // TODO: use gLogBuf->Clear(), which doesn't free the allocated space
            gLogBuf->Reset();
        }
    }

    size_t n = str::Len(s);

    // when skipping, we skip buf (crash reports) and console
    // but write to file and logview
    if (!skipLog) {
        gLogBuf->Append(s, n);
    }

    if (!skipLog && gLogToConsole) {
        fwrite(s, 1, n, stdout);
        fflush(stdout);
    }

    if (gLogFilePath) {
        auto f = fopen(gLogFilePath, "a");
        if (f != nullptr) {
            fwrite(s, 1, n, f);
            fflush(f);
            fclose(f);
        }
    }
    logToPipe(s, n);
    gLogMutex.Unlock();
}

void logf(const char* fmt, ...) {
    if (gReducedLogging || gStopLogging) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    AutoFreeStr s = str::FmtV(fmt, args);
    log(s.Get(), false);
    va_end(args);
}

void logfa(const char* fmt, ...) {
    if (gStopLogging) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    AutoFreeStr s = str::FmtV(fmt, args);
    log(s.Get(), true);
    va_end(args);
}

void StartLogToFile(const char* path, bool removeIfExists) {
    CrashIf(gLogFilePath);
    gLogFilePath = str::Dup(path);
    if (removeIfExists) {
        remove(path);
    }
}

bool WriteCurrentLogToFile(const char* path) {
    ByteSlice slice = gLogBuf->AsByteSlice();
    if (slice.empty()) {
        return false;
    }
    bool ok = file::WriteFile(path, slice);
    return ok;
}

void DestroyLogging() {
    gStopLogging = true;
    gLogMutex.Lock();
    delete gLogBuf;
    gLogBuf = nullptr;
    delete gLogAllocator;
    gLogAllocator = nullptr;
    gLogMutex.Unlock();
    str::FreePtr(&gLogFilePath);
}

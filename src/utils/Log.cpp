/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/WinUtil.h"

constexpr const WCHAR* kPipeName = L"\\\\.\\pipe\\SumatraPDFLogger";

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

bool gLogToPipe = true;
HANDLE hLogPipe = INVALID_HANDLE_VALUE;

static char* logFilePath;

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

static void logToPipe(std::string_view sv) {
    if (!gLogToPipe) {
        return;
    }
    if (sv.empty()) {
        return;
    }

    DWORD cbWritten{0};
    BOOL ok{false};
    bool didConnect{false};
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
        char* initialMsg = str::Format("app: %s\n", gLogAppName);
        WriteFile(hLogPipe, initialMsg, (DWORD)str::Len(initialMsg), &cbWritten, nullptr);
        str::Free(initialMsg);
    }

    DWORD cb = (DWORD)sv.size();
    // TODO: what happens when we write more than the server can read?
    // should I loop if cbWritten < cb?
    ok = WriteFile(hLogPipe, sv.data(), cb, &cbWritten, nullptr);
    if (!ok) {
#if 0
        DWORD err = GetLastError();
        OutputDebugStringA("logPipe: WriteFile() failed with error: ");
        char buf[256]{0};
        snprintf(buf, sizeof(buf) - 1, "%d %s\n", (int)err, getWinError(err));
        OutputDebugStringA(buf);
#endif
        CloseHandle(hLogPipe);
        hLogPipe = INVALID_HANDLE_VALUE;
    }
}

void log(std::string_view s) {
    // in reduced logging mode, we do want to log to at least the debugger
    if (gLogToDebugger || IsDebuggerPresent() || gReducedLogging) {
        OutputDebugStringA(s.data());
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

    gAllowAllocFailure++;
    defer {
        gAllowAllocFailure--;
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

    gLogBuf->Append(s.data(), s.size());
    if (gLogToConsole) {
        fwrite(s.data(), 1, s.size(), stdout);
        fflush(stdout);
    }

    if (logFilePath) {
        auto f = fopen(logFilePath, "a");
        if (f != nullptr) {
            fwrite(s.data(), 1, s.size(), f);
            fflush(f);
            fclose(f);
        }
    }
    logToPipe(s);
    gLogMutex.Unlock();
}

void log(const char* s) {
    auto sv = std::string_view(s);
    log(sv);
}

void logf(const char* fmt, ...) {
    if (gReducedLogging) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    AutoFree s = str::FmtV(fmt, args);
    log(s.AsView());
    va_end(args);
}

void StartLogToFile(const char* path) {
    logFilePath = str::Dup(path);
    remove(path);
}

void log(const WCHAR* s) {
    if (!s) {
        return;
    }
    auto tmp = ToUtf8Temp(s);
    log(tmp.AsView());
}

void logf(const WCHAR* fmt, ...) {
    if (gReducedLogging) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    AutoFreeWstr s = str::FmtV(fmt, args);
    log(s);
    va_end(args);
}

/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ThreadUtil.h"

Mutex gLogMutex;

// we use HeapAllocator because we can do logging during crash handling
// where we want to avoid allocator deadlocks by calling malloc()
HeapAllocator* gLogAllocator = nullptr;

str::Str* gLogBuf = nullptr;
bool logToStderr = false;
bool logToDebugger = false;

static char* logFilePath;

// 1 MB - 128 to stay under 1 MB even after appending (an estimate)
constexpr int kMaxLogBuf = 1024 * 1024 - 128;

static bool shouldLog() {
    gLogMutex.Lock();
    bool bufFull = gLogBuf && gLogBuf->isize() > kMaxLogBuf;
    gLogMutex.Unlock();
    return !bufFull;
}

void log(std::string_view s) {
    if (!shouldLog()) {
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
    }
    gLogBuf->Append(s.data(), s.size());
    if (logToStderr) {
        fwrite(s.data(), 1, s.size(), stderr);
        fflush(stderr);
    }

    if (logFilePath) {
        auto f = fopen(logFilePath, "a");
        if (f != nullptr) {
            fwrite(s.data(), 1, s.size(), f);
            fflush(f);
            fclose(f);
        }
    }
    if (logToDebugger) {
        OutputDebugStringA(s.data());
    }
    gLogMutex.Unlock();
}

void log(const char* s) {
    auto sv = std::string_view(s);
    log(sv);
}

void logf(const char* fmt, ...) {
    if (!shouldLog()) {
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

#if OS_WIN
void log(const WCHAR* s) {
    if (!s) {
        return;
    }
    if (!shouldLog()) {
        return;
    }
    AutoFree tmp = strconv::WstrToUtf8(s);
    auto sv = tmp.AsView();
    log(sv);
}

void logf(const WCHAR* fmt, ...) {
    if (!shouldLog()) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    AutoFreeWstr s = str::FmtV(fmt, args);
    log(s);
    va_end(args);
}
#endif

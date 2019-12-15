#include "utils/BaseUtil.h"

// we use HeapAllocator because we can do logging during crash handling
// where we want to avoid allocator deadlocks by calling malloc()
HeapAllocator* gLogAllocator = nullptr;

str::Str* gLogBuf = nullptr;
bool logToStderr = false;
bool logToDebugger = false;

static char* logFilePath;

void log(std::string_view s) {
    if (!gLogBuf) {
        gLogAllocator = new HeapAllocator();
        gLogBuf = new str::Str(16 * 1024, gLogAllocator);
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
}

void log(const char* s) {
    auto sv = std::string_view(s);
    log(sv);
}

void logf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFree s(str::FmtV(fmt, args));
    log(s.as_view());
    va_end(args);
}

void logToFile(const char* path) {
    logFilePath = str::Dup(path);
    remove(path);
}

#if OS_WIN
void log(const WCHAR* s) {
    if (!s) {
        return;
    }
    AutoFree tmp = strconv::WstrToUtf8(s);
    auto sv = tmp.as_view();
    log(sv);
}

void logf(const WCHAR* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFreeWstr s(str::FmtV(fmt, args));
    log(s);
    va_end(args);
}
#endif

#if OS_WIN
void dbglogf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFree s(str::FmtV(fmt, args));
    OutputDebugStringA(s.Get());
    va_end(args);
}
#else
void dbglogf(const char* fmt, ...) {
    // no-op
}
#endif

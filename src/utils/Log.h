/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern HeapAllocator* gLogAllocator;
extern str::Str* gLogBuf;
extern bool gLogToConsole;
extern bool gLogToDebugger;
extern bool gReducedLogging;
extern bool gLogToPipe;
extern bool gStopLogging;
extern const char* gLogAppName;
void StartLogToFile(const char* path, bool removeIfExists);

/*
If you do:

#define NO_LOG
#include "utils/Log.h"

then logging will be disabled.
This is an easy way to disable logging per file
*/

#ifdef NO_LOG
static inline void log(std::string_view) {
    // do nothing
}
static inline void log(const char*) {
    // do nothing
}
static inline void logf(const char*, ...) {
    // do nothing
}
static inline void log(const WCHAR*) {
    // do nothing
}
static inline void logf(const WCHAR*, ...) {
    // do nothing
}
#else
void log(std::string_view s);
void log(const char* s);
void logf(const char* fmt, ...);
void log(const WCHAR* s);
void logf(const WCHAR* fmt, ...);
#endif

void DestroyLogging();

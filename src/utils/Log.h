/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern HeapAllocator* gLogAllocator;
extern str::Str* gLogBuf;
extern bool gLogToConsole;
extern bool gLogToDebugger;
extern bool gReducedLogging;
extern bool gLogToPipe;
extern const char* gLogAppName;
extern char* gLogFilePath;
void StartLogToFile(const char* path, bool removeIfExists);
bool WriteCurrentLogToFile(const char* path);

/*
If you do:

#define NO_LOG
#include "utils/Log.h"

then logging will be disabled.
This is an easy way to disable logging per file
*/

#ifdef NO_LOG
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
void log(const char* s, bool always = false);
void logf(const char* fmt, ...);
#endif

// always log, even if NO_LOG is defined or reduced logging
void logfa(const char* fmt, ...);
void loga(const char* s);

void DestroyLogging();

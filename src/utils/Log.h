/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern HeapAllocator* gLogAllocator;
extern str::Str* gLogBuf;
extern bool logToStderr;
extern bool logToDebugger;
void StartLogToFile(const char* path);

/*
If you do:

#define NO_LOG
#include "utils/Log.h"

then logging will be disabled.
This is an easy way to disable logging per file
*/

#ifdef NO_LOG
#define log(x)
#define logf(x, ...)
#else
void log(std::string_view s);
void log(const char* s);
void logf(const char* fmt, ...);
void log(const WCHAR* s);
void logf(const WCHAR* fmt, ...);
#endif

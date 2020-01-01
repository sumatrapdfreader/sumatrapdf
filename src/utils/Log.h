/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern HeapAllocator* gLogAllocator;
extern str::Str* gLogBuf;
extern bool logToStderr;
extern bool logToDebugger;

void log(std::string_view s);
void log(const char* s);
void logf(const char* fmt, ...);
void logToFile(const char* path);

void dbglogf(const char* fmt, ...);

#if OS_WIN
void log(const WCHAR* s);
void logf(const WCHAR* fmt, ...);
#endif

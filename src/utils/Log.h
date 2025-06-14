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

void log(const char* s, bool always = false);
void logf(const char* fmt, ...);

void logvf(const char* fmt, ...);
void logv(const char* s);

void logValueSize(const char* name, i64 v);

// log always
void logfa(const char* fmt, ...);
void loga(const char* s);

void DestroyLogging();

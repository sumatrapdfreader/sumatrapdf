/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Arena* gLogAllocator;
extern StrBuilder* gLogBuf;
extern bool gLogToConsole;
extern bool gLogToDebugger;
extern bool gReducedLogging;
extern bool gLogToPipe;
extern Str gLogAppName;
extern Str gLogFilePath;
void StartLogToFile(Str path, bool removeIfExists);
bool WriteCurrentLogToFile(Str path);

void log(Str s);
void logf(const char* fmt, ...);

void logvf(const char* fmt, ...);
void logv(Str s);

void logValueSize(Str name, i64 v);

// log always
void logfa(const char* fmt, ...);
void loga(Str s);

void DestroyLogging();

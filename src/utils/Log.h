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
void logf(Str fmt, ...);

void logvf(Str fmt, ...);
void logv(Str s);

void logPipe(Str fmt, ...);

void logValueSize(Str name, i64 v);

// log always
void logfa(Str fmt, ...);
void loga(Str s);

void DestroyLogging();
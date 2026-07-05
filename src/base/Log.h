/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Arena* gLogAllocator;
extern str::Builder* gLogBuf;
extern bool gLogToConsole;
extern bool gLogToDebugger;
extern bool gReducedLogging;
extern bool gLogToPipe;
extern Str gLogAppName;
extern Str gLogFilePath;
void StartLogToFile(Str path, bool removeIfExists);
bool WriteCurrentLogToFile(Str path);

void log(Str s);
void loga(Str s); // log always

// logf/logfa format via fmt() and then route through log()/loga(). Going through
// log() (rather than a dedicated varargs logf) means they still log under
// gReducedLogging, where log() logs to at least the debugger.
// ::fmt/::log/::loga are global-qualified so a local named 'fmt' (common) can't
// shadow them; s__ avoids colliding with a caller-supplied argument named 's'.
#define logf(...)                     \
    do {                              \
        Str s__ = ::fmt(__VA_ARGS__); \
        ::log(s__);                   \
    } while (0)
#define logfa(...)                    \
    do {                              \
        Str s__ = ::fmt(__VA_ARGS__); \
        ::loga(s__);                  \
    } while (0)

void DestroyLogging();

/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* Simple logging for ad-hoc logging with OutputDebugString().
Only enabled in debug builds by default.

To disable logging, #define NOLOG to 1 before including this file i.e.

#define NOLOG 1
#include "DebugLog.h"

To always enable logging, #define NOLOG to 0 before the #include

#define NOLOG 0
#include "DebugLog.h"

Any other value for NOLOG (including 0), enables logging. This provides
for an easy switch for turning logging on/off in a given .cpp file.
*/

#ifndef NOLOG
  #ifdef NDEBUG
    #define NOLOG 1
  #else
    #define NOLOG 0
  #endif
#endif

namespace dbglog {

void LogF(const char *fmt, ...);
void LogF(const WCHAR *fmt, ...);

void LogFV(const char *fmt, va_list args);
void LogFV(const WCHAR *fmt, va_list args);

// call this in the event of an exception to add
// more information to the crash log
void CrashLogF(const char *fmt, ...);

// returns a copy of the data recorded with CrashLogF
const char *GetCrashLog();

} // namespace dbglog

// short names are important for this use case
#if NOLOG == 1
inline void lf(const char *, ...) {
    NoOp();
}
inline void lf(const WCHAR *, ...) {
    NoOp();
}
#else
inline void lf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    dbglog::LogFV(fmt, args);
    va_end(args);
}

inline void lf(const WCHAR *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    dbglog::LogFV(fmt, args);
    va_end(args);
}
#endif

// use to indicate that the log messages are meant to be more
// permanent (mostly for rarely executed code paths so that
// the log isn't unnecessarily spammed)
inline void plogf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    dbglog::LogFV(fmt, args);
    va_end(args);
}

inline void plogf(const WCHAR *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    dbglog::LogFV(fmt, args);
    va_end(args);
}

/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef DebugLog_h
#define DebugLog_h

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

#include "BaseUtil.h"

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

} // namespace dbglog

// short names are important for this use case
#if NOLOG == 1
#define lf(fmt, ...) NoOp()
#else
#define lf(fmt, ...) dbglog::LogF(fmt, __VA_ARGS__)
#endif
// use to indicate that the log messages are meant to be more
// permanent (mostly for rarely executed code paths so that
// the log isn't unnecessarily spammed)
#define plogf(fmt, ...) lf(fmt, __VA_ARGS__)

#endif

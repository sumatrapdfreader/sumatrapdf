/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef DebugLog_h
#define DebugLog_h

/* Simple logging for ad-hoc logging with OutputDebugString().

To disable logging, #define NOLOG to 1 before including this file i.e.

#define NOLOG 1
#include "DebugLog.h"

To only have logging in debug build:

#define NOLOG defined(NDEBUG)
#include "DebugLog.h"

Any other value for NOLOG (including 0), enables logging. This provides
for an easy switch for turning logging on/off in a given .cpp file.
*/

#include "Scoped.h"
#include "StrUtil.h"

namespace dbglog {

void lf(const char *fmt, ...);
void lf(const WCHAR *fmt, ...);

} // namespace dbglog

// short names are important for this use case
#if NOLOG == 1
inline void l(const char *s) { }
inline void l(const WCHAR *s) { }
#define lf(fmt, ...) ((void)0)
#else
inline void l(const char *s) {
    OutputDebugStringA(s);
    OutputDebugStringA("\n");
}

inline void l(const WCHAR *s) {
    OutputDebugStringW(s);
    OutputDebugStringA("\n");
}

#define lf(fmt, ...) dbglog::lf(fmt, __VA_ARGS__)
#endif

#endif

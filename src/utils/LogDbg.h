/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern bool gEnableDbgLog;

/*
If you do:

#define NO_LOG
#include "utils/LogDbg.h"

then logging will be disabled.
This is an easy way to disable logging per file
*/

#ifdef NO_LOG
#define dbglog(x)
#define dbglogf(x, ...)
#else
void dbglog(const char*);
void dbglogf(const char* fmt, ...);

void dbglog(const WCHAR*);
void dbglogf(const WCHAR* fmt, ...);
#endif

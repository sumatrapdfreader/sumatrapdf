/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern bool gEnableDbgLog;

void dbglog(const char*);
void dbglogf(const char* fmt, ...);

void dbglog(const WCHAR*);
void dbglogf(const WCHAR* fmt, ...);

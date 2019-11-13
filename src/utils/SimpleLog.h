/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern str::Str<char> logBuf;
extern bool logToStderr;

void log(const char* s);
void log(const WCHAR* s);
void logf(const char* fmt, ...);
void logf(const WCHAR* fmt, ...);
void dbglogf(const char* fmt, ...);

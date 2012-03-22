/* Copyright 2012 the ucrt project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ucrt_log_h
#define ucrt_log_h

#ifndef NOLOG
#define NOLOG defined(NDEBUG)
#endif

#define NoOp()      ((void)0)

namespace ucrt {

void LogF(const char *fmt, ...);
//void LogF(const WCHAR *fmt, ...);

} // namespace ucrt

// short names are important for this use case
#if NOLOG == 1
#define lf(fmt, ...) NoOp()
#else
#define lf(fmt, ...) ucrt::LogF(fmt, __VA_ARGS__)
#endif
// use to indicate that the log messages are meant to be more
// permanent (mostly for rarely executed code paths so that
// the log isn't unnecessarily spammed)
#define plogf(fmt, ...) lf(fmt, __VA_ARGS__)

#endif

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Relatively high-precision timer. Can be used e.g. for measuring execution
// time of a piece of code.
//
// TimeStamp is an opaque instant from a monotonic counter - only meaningful
// when passed to TimeSinceInMs(). Resolution:
//  - Windows: QueryPerformanceCounter. Its frequency is fixed for the lifetime
//    of the OS install and is guaranteed to be at least 1 MHz (< 1 us);
//    typically it is 10 MHz, i.e. 100 ns ticks.
//  - posix: clock_gettime(CLOCK_MONOTONIC), which reports in nanoseconds. The
//    real resolution is whatever clock_getres() says, usually tens of ns.
// So on both platforms a single tick is well under a microsecond, and printing
// TimeSinceInMs() with 3 decimals (microseconds) is meaningful.

#if OS_WIN
using TimeStamp = LARGE_INTEGER;

inline TimeStamp TimeGet() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t;
}

inline double TimeSinceInMs(TimeStamp start) {
    LARGE_INTEGER t = TimeGet();
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    double timeInSecs = (double)(t.QuadPart - start.QuadPart) / (double)freq.QuadPart;
    return timeInSecs * 1000.0;
}
#else
struct TimeStamp {
    timespec t;
};

inline TimeStamp TimeGet() {
    TimeStamp res;
    clock_gettime(CLOCK_MONOTONIC, &res.t);
    return res;
}

inline double TimeSinceInMs(TimeStamp start) {
    TimeStamp now = TimeGet();
    double secs = (double)(now.t.tv_sec - start.t.tv_sec);
    double nsecs = (double)(now.t.tv_nsec - start.t.tv_nsec);
    return secs * 1000.0 + nsecs / 1000000.0;
}
#endif

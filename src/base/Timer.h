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
// A single tick is well under a microsecond, so printing TimeSinceInMs()
// with 3 decimals (microseconds) is meaningful.

using TimeStamp = LARGE_INTEGER;

inline TimeStamp TimeGet() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t;
}

inline double TimeSinceInMs(TimeStamp start) {
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER t = TimeGet();
    return (double)(t.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
}

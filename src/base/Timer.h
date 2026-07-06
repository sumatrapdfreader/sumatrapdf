/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Relatively high-precision timer. Can be used e.g. for measuring execution
// time of a piece of code.

#if OS_WIN
inline LARGE_INTEGER TimeGet() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t;
}

inline double TimeSinceInMs(LARGE_INTEGER start) {
    LARGE_INTEGER t = TimeGet();
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    double timeInSecs = (double)(t.QuadPart - start.QuadPart) / (double)freq.QuadPart;
    return timeInSecs * 1000.0;
}
#else
struct TimePoint {
    timespec t;
};

inline TimePoint TimeGet() {
    TimePoint res;
    clock_gettime(CLOCK_MONOTONIC, &res.t);
    return res;
}

inline double TimeSinceInMs(TimePoint start) {
    TimePoint now = TimeGet();
    double secs = (double)(now.t.tv_sec - start.t.tv_sec);
    double nsecs = (double)(now.t.tv_nsec - start.t.tv_nsec);
    return secs * 1000.0 + nsecs / 1000000.0;
}
#endif

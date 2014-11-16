/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Relatively high-precision timer. Can be used e.g. for measuring execution
// time of a piece of code.
class Timer {
    LARGE_INTEGER   start;
    LARGE_INTEGER   end;

    double TimeSince(LARGE_INTEGER t) const
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        double timeInSecs = (double)(t.QuadPart-start.QuadPart)/(double)freq.QuadPart;
        return timeInSecs * 1000.0;
    }

public:
    explicit Timer() {
        Start();
    }

    void Start() {
        end.QuadPart = 0;
        QueryPerformanceCounter(&start);
    }

    double Stop() { 
        QueryPerformanceCounter(&end);
        return GetTimeInMs();
    }

    // If stopped, get the time at point it was stopped,
    // otherwise get current time
    double GetTimeInMs()
    {
        if (0 == end.QuadPart) {
            LARGE_INTEGER curr;
            QueryPerformanceCounter(&curr);
            return TimeSince(curr);
        }
        return TimeSince(end);
    }
};

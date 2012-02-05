/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Timer_h
#define Timer_h

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
    Timer(bool start=false) {
        if (start)
            Start();
    }

    void Start() { QueryPerformanceCounter(&start); }
    void Stop() { QueryPerformanceCounter(&end); }

    double GetCurrTimeInMs() const
    {
        LARGE_INTEGER curr;
        QueryPerformanceCounter(&curr);
        return TimeSince(curr);
    }

    double GetTimeInMs() const
    {
        return TimeSince(end);
    }
};

#endif


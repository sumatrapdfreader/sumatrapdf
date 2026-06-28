/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Mutex {
    CRITICAL_SECTION cs;

    Mutex() { InitializeCriticalSection(&cs); }
    ~Mutex() { DeleteCriticalSection(&cs); }

    void Lock() { EnterCriticalSection(&cs); }
    void Unlock() { LeaveCriticalSection(&cs); }
};

void SetThreadName(Str threadName, DWORD threadId = 0);

void RunAsync(const Func0&, Str threadName = {});
HANDLE StartThread(const Func0&, Str threadName = {});

extern AtomicInt gDangerousThreadCount;
bool AreDangerousThreadsPending();

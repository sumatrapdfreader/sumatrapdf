/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Mutex {
    CRITICAL_SECTION cs;

    Mutex() {
        InitializeCriticalSection(&cs);
    }
    ~Mutex() {
        DeleteCriticalSection(&cs);
    }

    void Lock() {
        EnterCriticalSection(&cs);
    }
    void Unlock() {
        LeaveCriticalSection(&cs);
    }
};

void SetThreadName(const char* threadName, DWORD threadId = 0);

void RunAsync(const Func0&, const char* threadName = nullptr);
HANDLE StartThread(const Func0&, const char* threadName = nullptr);

extern AtomicInt gDangerousThreadCount;
bool AreDangerousThreadsPending();

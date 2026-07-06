/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Mutex {
    SRWLOCK lock = SRWLOCK_INIT;

    Mutex() = default;
    ~Mutex() = default;

    void Lock() { AcquireSRWLockExclusive(&lock); }
    void Unlock() { ReleaseSRWLockExclusive(&lock); }
};

struct ScopedMutex {
    Mutex* mutex = nullptr;
    CRITICAL_SECTION* cs = nullptr;

    explicit ScopedMutex(Mutex* mutex) : mutex(mutex) { mutex->Lock(); }
    explicit ScopedMutex(CRITICAL_SECTION* cs) : cs(cs) { EnterCriticalSection(cs); }
    ~ScopedMutex() {
        if (mutex) {
            mutex->Unlock();
        }
        if (cs) {
            LeaveCriticalSection(cs);
        }
    }
};

void SetThreadName(Str threadName, DWORD threadId = 0);

void RunAsync(const Func0&, Str threadName = {});
HANDLE StartThread(const Func0&, Str threadName = {});

extern AtomicInt gDangerousThreadCount;
bool AreDangerousThreadsPending();

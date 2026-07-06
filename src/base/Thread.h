/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#if OS_WIN
using ThreadId = DWORD;
using ThreadHandle = HANDLE;

struct Mutex {
    SRWLOCK lock = SRWLOCK_INIT;

    Mutex() = default;
    ~Mutex() = default;

    void Lock() { AcquireSRWLockExclusive(&lock); }
    void Unlock() { ReleaseSRWLockExclusive(&lock); }
};

struct RecursiveMutex {
    CRITICAL_SECTION lock;

    RecursiveMutex() { InitializeCriticalSection(&lock); }
    ~RecursiveMutex() { DeleteCriticalSection(&lock); }

    void Lock() { EnterCriticalSection(&lock); }
    void Unlock() { LeaveCriticalSection(&lock); }
    bool TryLock() { return TryEnterCriticalSection(&lock); }
};
#else
using ThreadId = u64;

struct ThreadHandlePosix;
using ThreadHandle = ThreadHandlePosix*;

struct Mutex {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    Mutex() = default;
    ~Mutex() = default;

    void Lock() { pthread_mutex_lock(&lock); }
    void Unlock() { pthread_mutex_unlock(&lock); }
};

struct RecursiveMutex {
    pthread_mutex_t lock;

    RecursiveMutex() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&lock, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    ~RecursiveMutex() { pthread_mutex_destroy(&lock); }

    void Lock() { pthread_mutex_lock(&lock); }
    void Unlock() { pthread_mutex_unlock(&lock); }
    bool TryLock() { return pthread_mutex_trylock(&lock) == 0; }
};

ThreadId GetCurrentThreadId();
#endif

struct ScopedMutex {
    Mutex* mutex;

    explicit ScopedMutex(Mutex* mutex) : mutex(mutex) { mutex->Lock(); }
    ~ScopedMutex() { mutex->Unlock(); }
};

struct ScopedRecursiveMutex {
    RecursiveMutex* mutex;

    explicit ScopedRecursiveMutex(RecursiveMutex* mutex) : mutex(mutex) { mutex->Lock(); }
    ~ScopedRecursiveMutex() { mutex->Unlock(); }
};

void SetThreadName(Str threadName, ThreadId threadId = 0);

void RunAsync(const Func0&, Str threadName = {});
ThreadHandle StartThread(const Func0&, Str threadName = {});
#if OS_WIN
inline bool SafeCloseThreadHandle(ThreadHandle* hPtr) {
    ThreadHandle h = *hPtr;
    if (!h || h == INVALID_HANDLE_VALUE) {
        *hPtr = nullptr;
        return false;
    }
    BOOL ok = CloseHandle(h);
    *hPtr = nullptr;
    return !!ok;
}
#else
bool SafeCloseThreadHandle(ThreadHandle*);
#endif

extern AtomicInt gDangerousThreadCount;
bool AreDangerousThreadsPending();

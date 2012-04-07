/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ThreadUtil_h
#define ThreadUtil_h

class Functor {
public:
    virtual void operator()() = 0;
};

template <typename Func, class Cls>
class Functor0 : public Functor {
    Func func;
    Cls cls;
public:
    Functor0(Func func, Cls cls) :
        func(func), cls(cls) { }
    virtual void operator()() { (cls->*func)(); }
};

template <typename Func, class Cls>
inline Functor *Bind(Func func, Cls cls)
{
    return new Functor0<Func, Cls>(func, cls);
}

template <typename Func, class Cls, typename Arg1>
class Functor1 : public Functor {
    Func func;
    Cls cls;
    Arg1 arg1;
public:
    Functor1(Func func, Cls cls, Arg1 arg1) :
        func(func), cls(cls), arg1(arg1) { }
    virtual void operator()() { (cls->*func)(arg1); }
};

template <typename Func, class Cls, typename Arg1>
inline Functor *Bind(Func func, Cls cls, Arg1 arg1)
{
    return new Functor1<Func, Cls, Arg1>(func, cls, arg1);
}

template <typename Func, class Cls, typename Arg1, typename Arg2>
class Functor2 : public Functor {
    Func func;
    Cls cls;
    Arg1 arg1;
    Arg2 arg2;
public:
    Functor2(Func func, Cls cls, Arg1 arg1, Arg2 arg2) :
        func(func), cls(cls), arg1(arg1), arg2(arg2) { }
    virtual void operator()() { (cls->*func)(arg1, arg2); }
};

template <typename Func, class Cls, typename Arg1, typename Arg2>
inline Functor *Bind(Func func, Cls cls, Arg1 arg1, Arg2 arg2)
{
    return new Functor2<Func, Cls, Arg1, Arg2>(func, cls, arg1, arg2);
}

class WorkerThread {
    HANDLE thread;

    static DWORD WINAPI ThreadProc(LPVOID data);

public:
    WorkerThread(Functor *f);
    ~WorkerThread();

    bool Join(DWORD waitMs=INFINITE);
};

class UiMessageLoop {
    DWORD threadId;
    Vec<Functor *> queue;
    CRITICAL_SECTION cs;

public:
    UiMessageLoop();
    ~UiMessageLoop();

    int Run();
    void AddTask(Functor *f);

    bool IsUiThread() { return GetCurrentThreadId() == threadId; }
};

/* A very simple thread class that allows stopping a thread */
class ThreadBase : public RefCounted {
protected:
    int                 threadNo;

    HANDLE              hThread;

    // it's a bool but we're using LONG as this is operated on with
    // IterlockedIncrement() etc. functions.
    LONG               cancelRequested;

    char *             threadName;

    static DWORD WINAPI ThreadProc(void* data);

    virtual ~ThreadBase();

public:
    ThreadBase();

    // name is for debugging purposes, can be NULL.
    ThreadBase(const char *name);

    // TODO: do I need to use some Interlocked*() funtion like InterlockedCompareExchange()
    // for this to be safe? It's only ever changed via RequestCancel()
    bool WasCancelRequested() { return 0 != cancelRequested; }

    // request the thread to stop. It's up to Run() function
    // to call WasCancelRequested() and stop processing if it returns true.
    void RequestCancel() { InterlockedIncrement(&cancelRequested); }

    // call this to start executing Run() function.
    void Start();

    // ask the thread to stop with RequestCancel(), wait for it
    // to end and terminate if didn't end and terminate is true
    // returns true if thread stopped by itself
    bool RequestCancelAndWaitToStop(DWORD waitMs, bool terminate);

    // get a unique number that identifies a thread and unlike an
    // address of the object, will not be reused
    int GetNo() const { return threadNo; }

    // over-write this to implement the actual thread functionality
    virtual void Run() = 0;
};

#endif

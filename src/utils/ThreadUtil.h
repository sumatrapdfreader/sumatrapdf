/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ThreadUtil_h
#define ThreadUtil_h

#include "Vec.h"

class Functor {
public:
    virtual void operator()() = 0;
};

template <typename Func>
class Functor0 : public Functor {
    Func func;
public:
    Functor0(Func func) : func(func) { }
    virtual void operator()() { func(); }
};

template <typename Func>
inline Functor0<Func> *Bind(Func func)
{
    return new Functor0<Func>(func);
}

template <typename Func, typename Arg1>
class Functor1 : public Functor {
    Func func;
    Arg1 arg1;
public:
    Functor1(Func func, Arg1 arg1) : func(func), arg1(arg1) { }
    virtual void operator()() { func(arg1); }
};

template <typename Func, typename Arg1>
inline Functor1<Func, Arg1> *Bind(Func func, Arg1 arg1)
{
    return new Functor1<Func, Arg1>(func, arg1);
}

template <typename Func, typename Arg1, typename Arg2>
class Functor2 : public Functor {
    Func func;
    Arg1 arg1;
    Arg2 arg2;
public:
    Functor2(Func func, Arg1 arg1, Arg2 arg2) : func(func), arg1(arg1), arg2(arg2) { }
    virtual void operator()() { func(arg1, arg2); }
};

template <typename Func, typename Arg1, typename Arg2>
inline Functor2<Func, Arg1, Arg2> *Bind(Func func, Arg1 arg1, Arg2 arg2)
{
    return new Functor2<Func, Arg1, Arg2>(func, arg1, arg2);
}

class FunctorQueue {
    Vec<Functor *> tasks;
    CRITICAL_SECTION cs;

protected:
    FunctorQueue();
    ~FunctorQueue();

    void AddTask(Functor *f);
    Functor *GetNextTask();
};

class WorkerThread : FunctorQueue {
    HANDLE thread, event;

    static DWORD WINAPI ThreadProc(LPVOID data);

public:
    WorkerThread();
    ~WorkerThread();

    void AddTask(Functor *f);
};

class UiMessageLoop : FunctorQueue {
    DWORD threadId;
public:
    UiMessageLoop() : threadId(GetCurrentThreadId()) { }

    int Run();
    void AddTask(Functor *f);

    bool IsUiThread() { return GetCurrentThreadId() == threadId; }
};

#endif

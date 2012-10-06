/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ThreadUtil_h
#define ThreadUtil_h

#include "RefCounted.h"

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

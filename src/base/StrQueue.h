/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// multi-threaded queue of strings
struct StrQueue {
    StrQueue();
    ~StrQueue();

    void Lock();
    void Unlock();
    Str append(Str s);
    Str PopFront();
    bool IsSentinel(Str s);
    void MarkFinished();
    bool IsFinished();
    bool Access(const Func1<StrQueue*>& fn);

    StrVec strings;

    volatile bool isFinished = false;
    Mutex cs;
    ConditionVariable nonEmpty;
};

int len(StrQueue& q);

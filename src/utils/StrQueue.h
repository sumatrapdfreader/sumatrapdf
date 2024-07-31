/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// multi-threaded queue of strings
struct StrQueue {
    StrQueue();
    ~StrQueue();

    void Lock();
    void Unlock();
    int Size();
    char* Append(const char*, int len = -1);
    char* PopFront();
    bool IsSentinel(char*);
    void MarkFinished();
    bool IsFinished();
    bool Access(const Func1<StrQueue*>& fn);

    StrVec strings;

    volatile bool isFinished = false;
    CRITICAL_SECTION cs;
    HANDLE hEvent = nullptr;
};

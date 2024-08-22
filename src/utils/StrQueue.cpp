/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrQueue.h"

StrQueue::StrQueue() {
    InitializeCriticalSection(&cs);
    BOOL manualReset = FALSE;
    BOOL initialState = FALSE;
    hEvent = CreateEventW(nullptr /* SECURITY_ATTRIBUTES* */, manualReset, initialState, nullptr /* name */);
}

StrQueue::~StrQueue() {
    DeleteCriticalSection(&cs);
    CloseHandle(hEvent);
}

void StrQueue::Lock() {
    EnterCriticalSection(&cs);
}

void StrQueue::Unlock() {
    LeaveCriticalSection(&cs);
}

void StrQueue::MarkFinished() {
    Lock();
    ReportIf(isFinished);
    isFinished = true;
    Unlock();
    SetEvent(hEvent);
}

bool StrQueue::IsFinished() {
    Lock();
    auto res = isFinished;
    Unlock();
    return res;
}

int StrQueue::Size() {
    Lock();
    auto res = strings.Size();
    Unlock();
    return res;
}

char* StrQueue::Append(const char* s, int len) {
    Lock();
    auto res = strings.Append(s, len);
    Unlock();
    SetEvent(hEvent);
    return res;
}

constexpr uintptr_t kStrQueueSentinel = (uintptr_t)-2;

bool StrQueue::IsSentinel(char* s) {
    return s == (char*)kStrQueueSentinel;
}

// is blocking
// retuns sentinel value if no more strings
// use IsSentinel() to check if returned value is a sentinel
char* StrQueue::PopFront() {
again:
    Lock();
    if (strings.Size() == 0) {
        bool end = isFinished;
        Unlock();
        if (end) {
            return (char*)kStrQueueSentinel;
        }
        WaitForSingleObject(hEvent, INFINITE);
        goto again;
    }
    char* s = strings.RemoveAt(0);
    Unlock();
    return s;
}

// is blocking
// returns false if we finished i.e. there was no more strings to access
// and we finished adding. In that case fn was called
// calls fn() and returns false if there are strings available
bool StrQueue::Access(const Func1<StrQueue*>& fn) {
again:
    Lock();
    if (strings.Size() == 0) {
        bool end = isFinished;
        Unlock();
        if (end) {
            return false;
        }
        WaitForSingleObject(hEvent, INFINITE);
        goto again;
    }
    fn.Call(this);
    Unlock();
    return true;
}

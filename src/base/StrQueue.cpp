/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "StrQueue.h"

StrQueue::StrQueue() = default;

StrQueue::~StrQueue() = default;

void StrQueue::Lock() {
    cs.Lock();
}

void StrQueue::Unlock() {
    cs.Unlock();
}

void StrQueue::MarkFinished() {
    Lock();
    ReportIf(isFinished);
    isFinished = true;
    Unlock();
    nonEmpty.WakeAll();
}

bool StrQueue::IsFinished() {
    Lock();
    auto res = isFinished;
    Unlock();
    return res;
}

int len(StrQueue& q) {
    q.Lock();
    int res = len(q.strings);
    q.Unlock();
    return res;
}

Str StrQueue::append(Str s) {
    Lock();
    auto res = strings.Append(s);
    Unlock();
    nonEmpty.Wake();
    return res;
}

constexpr uintptr_t kStrQueueSentinel = (uintptr_t)-2;

static Str StrQueueSentinel() {
    return Str((char*)kStrQueueSentinel, 0);
}

bool StrQueue::IsSentinel(Str s) {
    return s.s == (char*)kStrQueueSentinel;
}

// is blocking
// retuns sentinel value if no more strings
// use IsSentinel() to check if returned value is a sentinel
Str StrQueue::PopFront() {
    Lock();
    while (len(strings) == 0 && !isFinished) {
        nonEmpty.Wait(&cs);
    }
    if (len(strings) == 0) {
        Unlock();
        return StrQueueSentinel();
    }
    Str s = strings.RemoveAt(0);
    Unlock();
    return s;
}

// is blocking
// returns false if we finished i.e. there was no more strings to access
// and we finished adding. In that case fn was called
// calls fn() and returns false if there are strings available
bool StrQueue::Access(const Func1<StrQueue*>& fn) {
    Lock();
    while (len(strings) == 0 && !isFinished) {
        nonEmpty.Wait(&cs);
    }
    if (len(strings) == 0) {
        Unlock();
        return false;
    }
    fn.Call(this);
    Unlock();
    return true;
}

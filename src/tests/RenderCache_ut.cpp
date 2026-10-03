/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"

struct RequestLockProbe {
    RecursiveMutex* mutex = nullptr;
    bool available = false;
};

static void ProbeRequestLock(RequestLockProbe* probe) {
    probe->available = probe->mutex->TryLock();
    if (probe->available) {
        probe->mutex->Unlock();
    }
}

class LockProbeAbortCookie : public AbortCookie {
  public:
    RecursiveMutex* mutex = nullptr;
    bool* available = nullptr;

    LockProbeAbortCookie(RecursiveMutex* mutex, bool* available) : mutex(mutex), available(available) {}
    ~LockProbeAbortCookie() override {
        RequestLockProbe probe{mutex};
        ThreadHandle thread = StartThread(MkFunc0(ProbeRequestLock, &probe), StrL("request-lock-probe"));
        if (!thread) {
            return;
        }
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        *available = probe.available;
    }

    void Abort() override {}
    void* GetData() override { return nullptr; }
};

bool RenderCache_UnitTestCookieUnlocked() {
    RenderCache cache;
    PageRenderRequest req;
    bool available = false;
    req.abortCookie = new LockProbeAbortCookie(&cache.requestAccess, &available);
    cache.curReqs[0] = &req;

    cache.ClearCurrentRequest(0);
    return available;
}

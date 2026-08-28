/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "PageRenderPolicy.h"
#include "gui/Gfx.h"
#include "gui/PlatformWindow.h"
#include "PageRenderService.h"

struct PageRenderNotify {
    AtomicInt refs = 1;
    AtomicInt active = 1;
    Func0 callback;
};

struct PageRenderCacheEntry {
    PageRenderKey key;
    PageRenderPolicyCacheEntry policy;
    Pixmap* pixmap = nullptr;
};

struct PageRenderServiceData {
    Mutex mutex;
    ConditionVariable condition;
    ThreadHandle worker = nullptr;
    EngineBase* engine = nullptr;
    AbortCookie* activeCookie = nullptr;
    Vec<PageRenderPolicyRequest> requests;
    Vec<PageRenderCacheEntry> cache;
    PageRenderNotify* notify = nullptr;
    i64 cacheBytes = 0;
    i64 maxBytes = 0;
    u64 serial = 0;
    u64 useSerial = 0;
    u32 generation = 1;
    PageRenderKey activeKey;
    bool hasActive = false;
    bool stopping = false;
    bool workerStopped = false;
};

static PageRenderServiceData* ServiceData(PageRenderService* service) {
    return (PageRenderServiceData*)service->data;
}

static void ReleaseNotify(PageRenderNotify* notify) {
    if (AtomicIntDec(&notify->refs) == 0) {
        delete notify;
    }
}

static void RunNotify(PageRenderNotify* notify) {
    if (AtomicIntGet(&notify->active)) {
        notify->callback.Call();
    }
    ReleaseNotify(notify);
}

static void PostNotify(PageRenderNotify* notify) {
    AtomicIntInc(&notify->refs);
    PlatformPostTask(MkFunc0(RunNotify, notify));
}

static int FindCached(PageRenderServiceData* data, PageRenderKey key) {
    for (int i = 0; i < len(data->cache); i++) {
        if (data->cache[i].key == key) {
            return i;
        }
    }
    return -1;
}

static void RemoveCacheEntry(PageRenderServiceData* data, int idx) {
    PageRenderCacheEntry& entry = data->cache[idx];
    data->cacheBytes -= entry.policy.bytes;
    FreePixmap(entry.pixmap);
    VecRemoveAt(data->cache, idx);
}

static void ClearCache(PageRenderServiceData* data) {
    for (PageRenderCacheEntry& entry : data->cache) {
        FreePixmap(entry.pixmap);
    }
    VecReset(data->cache);
    data->cacheBytes = 0;
}

// Add the result only if it fits the fixed memory budget. Protect the new
// entry while evicting older LRU entries so a useful visible result survives.
static bool AddToCache(PageRenderServiceData* data, PageRenderKey key, Pixmap* pixmap) {
    i64 bytes = PixmapByteSize(pixmap);
    if (bytes <= 0 || bytes > data->maxBytes) {
        FreePixmap(pixmap);
        return false;
    }
    int old = FindCached(data, key);
    if (old >= 0) {
        RemoveCacheEntry(data, old);
    }

    PageRenderCacheEntry entry;
    entry.key = key;
    entry.policy.bytes = bytes;
    entry.policy.lastUse = ++data->useSerial;
    entry.pixmap = pixmap;
    VecAppend(data->cache, entry);
    data->cacheBytes += bytes;

    int protectedIndex = len(data->cache) - 1;
    while (data->cacheBytes > data->maxBytes) {
        Vec<PageRenderPolicyCacheEntry> policyEntries;
        for (const PageRenderCacheEntry& cached : data->cache) {
            VecAppend(policyEntries, cached.policy);
        }
        int evict = PageRenderPolicyPickEviction(policyEntries, protectedIndex);
        if (evict < 0) {
            break;
        }
        RemoveCacheEntry(data, evict);
        if (evict < protectedIndex) {
            protectedIndex--;
        }
    }
    return true;
}

static void RenderWorker(PageRenderServiceData* data) {
    for (;;) {
        data->mutex.Lock();
        while (!data->stopping && len(data->requests) == 0) {
            data->condition.Wait(&data->mutex);
        }
        if (data->stopping) {
            data->workerStopped = true;
            data->condition.WakeAll();
            data->mutex.Unlock();
            return;
        }

        int requestIdx = PageRenderPolicyPickRequest(data->requests);
        PageRenderPolicyRequest request = data->requests[requestIdx];
        VecRemoveAt(data->requests, requestIdx);
        data->activeKey = request.key;
        data->hasActive = true;
        data->activeCookie = nullptr;
        data->mutex.Unlock();

        RenderPageArgs args(request.key.pageNo, request.key.zoom, request.key.rotation, nullptr, RenderTarget::View,
                            &data->activeCookie);
        Pixmap* pixmap = data->engine->RenderPage(args);

        data->mutex.Lock();
        delete data->activeCookie;
        data->activeCookie = nullptr;
        data->hasActive = false;
        bool accepted = false;
        if (!data->stopping && request.generation == data->generation && pixmap) {
            accepted = AddToCache(data, request.key, pixmap);
            pixmap = nullptr;
        }
        data->mutex.Unlock();
        FreePixmap(pixmap);
        if (accepted) {
            PostNotify(data->notify);
        }
    }
}

PageRenderService* PageRenderService::Create(EngineBase* engine, const Func0& onPageReady, i64 maxBytes) {
    if (!engine || maxBytes <= 0) {
        return nullptr;
    }
    EngineBase* clone = engine->Clone();
    if (!clone) {
        return nullptr;
    }

    auto* service = new PageRenderService();
    auto* serviceData = new PageRenderServiceData();
    service->data = serviceData;
    serviceData->engine = clone;
    serviceData->maxBytes = maxBytes;
    serviceData->notify = new PageRenderNotify();
    serviceData->notify->callback = onPageReady;
    serviceData->worker = StartThread(MkFunc0(RenderWorker, serviceData), StrL("page-render"));
    if (!serviceData->worker) {
        serviceData->engine->Release();
        ReleaseNotify(serviceData->notify);
        delete serviceData;
        delete service;
        return nullptr;
    }
    return service;
}

PageRenderService::~PageRenderService() {
    auto* serviceData = ServiceData(this);
    if (!serviceData) {
        return;
    }

    serviceData->mutex.Lock();
    serviceData->stopping = true;
    VecReset(serviceData->requests);
    if (serviceData->activeCookie) {
        serviceData->activeCookie->Abort();
    }
    serviceData->condition.WakeAll();
    while (!serviceData->workerStopped) {
        serviceData->condition.Wait(&serviceData->mutex);
    }
    serviceData->mutex.Unlock();

    SafeCloseThreadHandle(&serviceData->worker);
    AtomicIntSet(&serviceData->notify->active, 0);
    ReleaseNotify(serviceData->notify);
    ClearCache(serviceData);
    serviceData->engine->Release();
    delete serviceData;
    data = nullptr;
}

void PageRenderService::NewGeneration() {
    auto* serviceData = ServiceData(this);
    ScopedMutex lock(&serviceData->mutex);
    serviceData->generation++;
    VecReset(serviceData->requests);
    ClearCache(serviceData);
    if (serviceData->activeCookie) {
        serviceData->activeCookie->Abort();
    }
}

void PageRenderService::Request(PageRenderKey key, PageRenderPriority priority) {
    auto* serviceData = ServiceData(this);
    ScopedMutex lock(&serviceData->mutex);
    if (serviceData->stopping || FindCached(serviceData, key) >= 0 ||
        (serviceData->hasActive && serviceData->activeKey == key)) {
        return;
    }
    PageRenderPolicyRequest request;
    request.key = key;
    request.priority = priority;
    request.generation = serviceData->generation;
    request.serial = ++serviceData->serial;
    PageRenderPolicyUpsert(serviceData->requests, request);
    serviceData->condition.Wake();
}

Pixmap* PageRenderService::CopyPage(PageRenderKey key) {
    auto* serviceData = ServiceData(this);
    ScopedMutex lock(&serviceData->mutex);
    int idx = FindCached(serviceData, key);
    if (idx < 0) {
        return nullptr;
    }
    PageRenderCacheEntry& entry = serviceData->cache[idx];
    entry.policy.lastUse = ++serviceData->useSerial;
    return ClonePixmap(entry.pixmap);
}

bool PageRenderService::DrawPage(Gfx* gfx, PageRenderKey key, const Rect& target) {
    auto* serviceData = ServiceData(this);
    ScopedMutex lock(&serviceData->mutex);
    int idx = FindCached(serviceData, key);
    if (idx < 0) {
        return false;
    }
    PageRenderCacheEntry& entry = serviceData->cache[idx];
    entry.policy.lastUse = ++serviceData->useSerial;
    gfx->DrawPixmap(entry.pixmap, target);
    return true;
}

i64 PageRenderService::CacheBytes() const {
    auto* serviceData = ServiceData((PageRenderService*)this);
    ScopedMutex lock(&serviceData->mutex);
    return serviceData->cacheBytes;
}

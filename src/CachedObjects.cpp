/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "CachedObjects.h"

Kind kindCachedRender = "render";
Kind kindCachedImage = "image";

Vec<CachedObject> gCachedObjects;
int gSaveMemory = 50;

WindowTab* (*gFindTabByEngine)(EngineBase*) = nullptr;
WindowTab* (*gCurrentTabForCache)() = nullptr;
void (*gOnCachedObjectsChanged)() = nullptr;

static RecursiveMutex gCachedObjectsLock;
static AtomicInt gFreeCachedObjectsReenter;
static thread_local uintptr_t gSkipFreeId = 0;

static void EnsureCachedObjectHooks();

static WindowTab* CurrTab() {
    if (gCurrentTabForCache) {
        return gCurrentTabForCache();
    }
    return nullptr;
}

static WindowTab* TabForEngine(EngineBase* engine) {
    if (!engine || !gFindTabByEngine) {
        return nullptr;
    }
    return gFindTabByEngine(engine);
}

static int FindCachedObjectIdx(uintptr_t id) {
    for (int i = 0; i < len(gCachedObjects); i++) {
        if (gCachedObjects[i].id == id) {
            return i;
        }
    }
    return -1;
}

static u64 CachedObjectsTotalSize() {
    u64 n = 0;
    for (int i = 0; i < len(gCachedObjects); i++) {
        n += gCachedObjects[i].size;
    }
    return n;
}

static void NotifyCachedObjectsChanged() {
    if (gOnCachedObjectsChanged) {
        gOnCachedObjectsChanged();
    }
}

void UnregisterCachedObject(uintptr_t id) {
    if (!id) {
        return;
    }
    bool removed = false;
    {
        ScopedRecursiveMutex scope(&gCachedObjectsLock);
        int idx = FindCachedObjectIdx(id);
        if (idx >= 0) {
            VecRemoveAtFast(gCachedObjects, idx);
            removed = true;
        }
    }
    if (removed) {
        NotifyCachedObjectsChanged();
    }
}

void DidAllocateCachedObject(CachedObject* o) {
    EnsureCachedObjectHooks();
    if (!o || !o->id || !o->free) {
        return;
    }

    CachedObject obj = *o;
    if (!obj.tab) {
        obj.tab = TabForEngine(obj.engine);
    }

    {
        ScopedRecursiveMutex scope(&gCachedObjectsLock);
        int idx = FindCachedObjectIdx(obj.id);
        if (idx >= 0) {
            gCachedObjects[idx] = obj;
        } else {
            VecAppend(gCachedObjects, obj);
        }
    }
    NotifyCachedObjectsChanged();

    gSkipFreeId = obj.id;
    TryFreeCachedObjects(obj.size);
    gSkipFreeId = 0;
}

static bool ObjectCanFree(WindowTab* currTab, CachedObject* o, uintptr_t skipId) {
    if (!o || o->id == 0 || o->id == skipId || !o->free) {
        return false;
    }
    if (!o->canFree) {
        return true;
    }
    return o->canFree(currTab, o);
}

static u64 FreeMatching(u64 wantBytes, bool aggressive, uintptr_t skipId) {
    if (wantBytes == 0) {
        return 0;
    }
    if (AtomicIntGet(&gFreeCachedObjectsReenter) != 0) {
        return 0;
    }
    AtomicIntInc(&gFreeCachedObjectsReenter);

    WindowTab* currTab = CurrTab();
    u64 freed = 0;

    // Pass 0: other tabs. Pass 1: current tab. Aggressive: both, everything CanFree.
    int nPasses = 2;
    for (int pass = 0; pass < nPasses && freed < wantBytes; pass++) {
        Vec<CachedObject> snap;
        {
            ScopedRecursiveMutex scope(&gCachedObjectsLock);
            snap = gCachedObjects;
        }
        for (int i = 0; i < len(snap) && freed < wantBytes; i++) {
            CachedObject o = snap[i];
            bool otherTab = o.tab != currTab;
            if (!aggressive) {
                if (pass == 0 && !otherTab) {
                    continue;
                }
                if (pass == 1 && otherTab) {
                    continue;
                }
            } else if (pass == 1) {
                break;
            }
            if (!ObjectCanFree(currTab, &o, skipId)) {
                continue;
            }
            u64 sz = o.size;
            uintptr_t id = o.id;
            if (!o.free(currTab, &o)) {
                continue;
            }
            UnregisterCachedObject(id);
            freed += sz;
        }
    }

    AtomicIntDec(&gFreeCachedObjectsReenter);
    return freed;
}

static u64 BytesWeWantFreed(u64 newAllocationSize, bool aggressive) {
    if (aggressive) {
        return (u64)-1;
    }

    int level = gSaveMemory;
    if (level < 0) {
        level = 0;
    }
    if (level > 100) {
        level = 100;
    }
    if (level == 0) {
        return 0;
    }
    if (level >= 100) {
        return (u64)-1;
    }

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) {
        return newAllocationSize;
    }

    u64 avail = ms.ullAvailPhys;
    u64 cached = 0;
    {
        ScopedRecursiveMutex scope(&gCachedObjectsLock);
        cached = CachedObjectsTotalSize();
    }

    // pressure rises with OS memory load and gSaveMemory.
    // 0 never reaches here; 100 already returned max.
    int pressure = (int)ms.dwMemoryLoad + level - 100;
    if (newAllocationSize > 0 && avail < newAllocationSize * 2) {
        if (pressure < level) {
            pressure = level;
        }
    }
    if (pressure <= 0) {
        return 0;
    }

    u64 want = cached * (u64)pressure / 100;
    if (want < newAllocationSize) {
        want = newAllocationSize;
    }
    return want;
}

u64 TryFreeCachedObjects(u64 newAllocationSize) {
    EnsureCachedObjectHooks();
    u64 want = BytesWeWantFreed(newAllocationSize, false);
    return FreeMatching(want, false, gSkipFreeId);
}

u64 FreeCachedObjects() {
    EnsureCachedObjectHooks();
    return FreeMatching((u64)-1, true, 0);
}

void FreeCachedObjectsForEngine(EngineBase* engine) {
    if (!engine) {
        return;
    }

    Vec<CachedObject> snap;
    {
        ScopedRecursiveMutex scope(&gCachedObjectsLock);
        snap = gCachedObjects;
    }
    WindowTab* currTab = CurrTab();
    for (int i = 0; i < len(snap); i++) {
        CachedObject o = snap[i];
        if (o.engine != engine) {
            continue;
        }
        if (o.free) {
            o.free(currTab, &o);
        }
        UnregisterCachedObject(o.id);
    }
}

static void EnsureCachedObjectHooks() {
    if (!gOnEngineDestroyed.IsValid()) {
        gOnEngineDestroyed = MkFunc1Void(FreeCachedObjectsForEngine);
    }
    gTryFreeCachedObjects = TryFreeCachedObjects;
    gFreeCachedObjects = FreeCachedObjects;
}

static int InitCachedObjectHooks() {
    EnsureCachedObjectHooks();
    return 1;
}
static int gCachedObjectHooksInit = InitCachedObjectHooks();

static TempStr FormatCachedSizeTemp(u64 bytes) {
    if (bytes < 1024) {
        return fmt("%d B", (int)bytes);
    }
    if (bytes < 1024ull * 1024) {
        return fmt("%.1f KB", bytes / 1024.0);
    }
    return fmt("%.2f MB", bytes / (1024.0 * 1024.0));
}

static int CmpCachedSizeDesc(const CachedObject* a, const CachedObject* b) {
    if (a->size > b->size) {
        return -1;
    }
    if (a->size < b->size) {
        return 1;
    }
    return 0;
}

static TempStr CachedObjectFileTemp(const CachedObject* o) {
    if (!o->engine) {
        return StrL("-");
    }
    Str path = o->engine->FilePath();
    if (len(path) == 0) {
        return StrL("-");
    }
    TempStr name = path::GetBaseNameTemp(path);
    if (len(name) == 0) {
        return StrL("-");
    }
    return name;
}

// One row per registered object, largest first. Columns are fixed-width so
// the Cache Info window can show them as a table (no wrap).
void SerializeCachedObjects(str::Builder& s) {
    Vec<CachedObject> snap;
    {
        ScopedRecursiveMutex scope(&gCachedObjectsLock);
        snap = gCachedObjects;
    }
    VecSort(snap, CmpCachedSizeDesc);

    u64 total = 0;
    for (int i = 0; i < len(snap); i++) {
        total += snap[i].size;
    }

    s.Append(fmt("Cached objects: %d  (%s)  SaveMemory %d\n\n", len(snap), FormatCachedSizeTemp(total), gSaveMemory));
    s.Append(fmt("%s %-8s %10s %5s %7s  %s\n", StrL(" "), StrL("kind"), StrL("size"), StrL("page"), StrL("zoom"),
                 StrL("file")));

    WindowTab* currTab = CurrTab();
    for (int i = 0; i < len(snap); i++) {
        const CachedObject& o = snap[i];
        Str cur = (o.tab && o.tab == currTab) ? StrL("*") : StrL(" ");
        Str kindName = o.kind ? Str(o.kind) : StrL("-");
        TempStr pageS = o.pageNo > 0 ? fmt("%d", o.pageNo) : StrL("-");
        TempStr zoomS = o.zoom > 0 ? fmt("%.1f", o.zoom) : StrL("-");
        s.Append(fmt("%s %-8s %10s %5s %7s  %s\n", cur, kindName, FormatCachedSizeTemp(o.size), pageS, zoomS,
                     CachedObjectFileTemp(&o)));
    }
}

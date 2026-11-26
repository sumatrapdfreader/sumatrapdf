/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "CachedObjects.h"

Kind kindCachedRender = "render";
Kind kindCachedImage = "image";

Vec<CachedObject> gCachedObjects;
AtomicInt gSaveMemory = 50;

WindowTab* (*gFindTabByEngine)(EngineBase*) = nullptr;
WindowTab* (*gCurrentTabForCache)() = nullptr;
void (*gOnCachedObjectsChanged)() = nullptr;

static RecursiveMutex gCachedObjectsLock;
static u64 gNextCacheGeneration = 0;
static AtomicBool gFreeCachedObjectsReenter;
static thread_local uintptr_t gSkipFreeId = 0;

static void EnsureCachedObjectHooks();

// Tabs belong to the UI thread. Render / load threads allocate cached objects
// too; walking the tab list from there raced CloseTab and read a freed tab.
static WindowTab* CurrTab() {
    if (gCurrentTabForCache && uitask::IsMainUIThread()) {
        return gCurrentTabForCache();
    }
    return nullptr;
}

static WindowTab* TabForEngine(EngineBase* engine) {
    if (!engine || !gFindTabByEngine || !uitask::IsMainUIThread()) {
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

static int FindCurrentCachedObject(const CachedObject* o) {
    int idx = FindCachedObjectIdx(o->id);
    return idx >= 0 && gCachedObjects[idx].generation == o->generation ? idx : -1;
}

bool IsCurrentCachedObject(const CachedObject* o) {
    ScopedRecursiveMutex scope(&gCachedObjectsLock);
    return FindCurrentCachedObject(o) >= 0;
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
        obj.generation = ++gNextCacheGeneration;
        obj.claimed = false;
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
    // swap, not get + set: two threads could both see it clear and free the same objects
    if (AtomicBoolSwap(&gFreeCachedObjectsReenter, true)) {
        return 0;
    }

    WindowTab* currTab = CurrTab();
    u64 freed = 0;

    // Pass 0: other tabs. Pass 1: current tab. Aggressive or off the UI thread
    // (current tab unknown): one pass over everything CanFree.
    int nPasses = (aggressive || !currTab) ? 1 : 2;
    for (int pass = 0; pass < nPasses && freed < wantBytes; pass++) {
        Vec<CachedObject> snap;
        {
            ScopedRecursiveMutex scope(&gCachedObjectsLock);
            snap = gCachedObjects;
        }
        for (int i = 0; i < len(snap) && freed < wantBytes; i++) {
            CachedObject o = snap[i];
            WindowTab* tab = o.tab ? o.tab : TabForEngine(o.engine);
            bool otherTab = tab != currTab;
            if (nPasses == 2) {
                if (pass == 0 && !otherTab) {
                    continue;
                }
                if (pass == 1 && otherTab) {
                    continue;
                }
            }
            if (!ObjectCanFree(currTab, &o, skipId)) {
                continue;
            }
            // Keep the claim registered until its callback completes. An
            // eviction or replacement can invalidate it while we run unlocked.
            bool claimed = false;
            {
                ScopedRecursiveMutex scope(&gCachedObjectsLock);
                int idx = FindCurrentCachedObject(&o);
                if (idx >= 0 && !gCachedObjects[idx].claimed) {
                    gCachedObjects[idx].claimed = true;
                    claimed = true;
                }
            }
            if (!claimed) {
                continue;
            }
            u64 sz = o.size;
            bool didFree = o.free(currTab, &o);
            {
                ScopedRecursiveMutex scope(&gCachedObjectsLock);
                int idx = FindCurrentCachedObject(&o);
                if (idx >= 0) {
                    if (didFree) {
                        VecRemoveAtFast(gCachedObjects, idx);
                    } else {
                        gCachedObjects[idx].claimed = false;
                    }
                }
            }
            if (didFree) {
                NotifyCachedObjectsChanged();
                freed += sz;
            }
        }
    }

    AtomicBoolSet(&gFreeCachedObjectsReenter, false);
    return freed;
}

static u64 BytesWeWantFreed(u64 newAllocationSize, bool aggressive) {
    if (aggressive) {
        return (u64)-1;
    }

    int level = AtomicIntGet(&gSaveMemory);
    level = ClampI(level, 0, 100);
    if (level == 0) {
        return 0;
    }
    if (level >= 100) {
        return (u64)-1;
    }

    u64 avail = 0;
    int memLoad = 0;
    if (!GetPhysMemoryInfo(&avail, &memLoad)) {
        return newAllocationSize;
    }

    u64 cached = 0;
    {
        ScopedRecursiveMutex scope(&gCachedObjectsLock);
        cached = CachedObjectsTotalSize();
    }

    // pressure rises with OS memory load and gSaveMemory.
    // 0 never reaches here; 100 already returned max.
    int pressure = memLoad + level - 100;
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

    Vec<CachedObject> toFree;
    bool changed = false;
    {
        ScopedRecursiveMutex scope(&gCachedObjectsLock);
        for (int i = len(gCachedObjects) - 1; i >= 0; i--) {
            CachedObject& o = gCachedObjects[i];
            if (o.engine != engine) {
                continue;
            }
            changed = true;
            if (!o.claimed && VecAppend(toFree, o)) {
                o.claimed = true;
            } else {
                VecRemoveAtFast(gCachedObjects, i);
            }
        }
    }
    if (!changed) {
        return;
    }
    WindowTab* currTab = CurrTab();
    for (int i = 0; i < len(toFree); i++) {
        CachedObject o = toFree[i];
        if (o.free) {
            o.free(currTab, &o);
        }
        ScopedRecursiveMutex scope(&gCachedObjectsLock);
        int idx = FindCurrentCachedObject(&o);
        if (idx >= 0) {
            VecRemoveAtFast(gCachedObjects, idx);
        }
    }
    NotifyCachedObjectsChanged();
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

    s.Append(fmt("Cached objects: %d  (%s)  SaveMemory %d", len(snap), FormatCachedSizeTemp(total),
                 AtomicIntGet(&gSaveMemory)));
    u64 availPhys = 0;
    u64 totalPhys = 0;
    int memLoad = 0;
    if (GetPhysMemoryInfo(&availPhys, &memLoad, &totalPhys)) {
        const double gb = 1024.0 * 1024.0 * 1024.0;
        s.Append(fmt("  Free Mem: %.2f GB of %.1f GB", (double)availPhys / gb, (double)totalPhys / gb));
    }
    s.Append(StrL("\n\n"));
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

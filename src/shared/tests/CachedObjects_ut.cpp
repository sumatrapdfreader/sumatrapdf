/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/TgaReader.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "CachedObjects.h"

#include "base/tests/UtAssert.h"

static bool gFreed[16];

static bool TestCanFree(WindowTab*, CachedObject* o) {
    return o->id != 1;
}

static bool TestFree(WindowTab*, CachedObject* o) {
    int i = (int)o->id;
    if (i >= 0 && i < 16) {
        gFreed[i] = true;
    }
    UnregisterCachedObject(o->id);
    return true;
}

static CachedObject MkObj(uintptr_t id, u64 size, EngineBase* engine) {
    CachedObject o{};
    o.id = id;
    o.size = size;
    o.engine = engine;
    o.canFree = TestCanFree;
    o.free = TestFree;
    return o;
}

static AtomicInt gFindTabCalls;

static WindowTab* CountingFindTab(EngineBase*) {
    AtomicIntInc(&gFindTabCalls);
    return nullptr;
}

static void AllocOnWorker() {
    CachedObject o = MkObj(8, 100, (EngineBase*)(uintptr_t)0x300);
    DidAllocateCachedObject(&o);
}

// tabs belong to the UI thread: a render thread walking them races CloseTab
static void TabLookupOffUIThreadTest() {
    auto saved = gFindTabByEngine;
    gFindTabByEngine = CountingFindTab;
    AtomicIntSet(&gFindTabCalls, 0);
    ThreadHandle thread = StartThread(MkFunc0Void(AllocOnWorker), StrL("cache-alloc-test"));
    utassert(JoinThread(&thread, 5000));
    utassert(AtomicIntGet(&gFindTabCalls) == 0);
    gFindTabByEngine = saved;
}

static bool FindPageObject(EngineBase* engine, int pageNo, CachedObject* out) {
    for (int i = 0; i < len(gCachedObjects); i++) {
        if (gCachedObjects[i].engine == engine && gCachedObjects[i].pageNo == pageNo) {
            *out = gCachedObjects[i];
            return true;
        }
    }
    return false;
}

static bool IsRegistered(uintptr_t id) {
    for (int i = 0; i < len(gCachedObjects); i++) {
        if (gCachedObjects[i].id == id) {
            return true;
        }
    }
    return false;
}

// FreeMatching walks a snapshot: entries may outlive their page or engine
static void StaleImagePageTest() {
    // one more than kMaxImagePageCache, to evict page 1
    constexpr int kPages = 33;

    TempStr dir = str::JoinTemp(GetTempFilePathTemp(StrL("cached-img")), StrL("-dir"));
    utassert(dir::CreateAll(dir));

    // TGA: not decodable by mupdf, so pages take the cached pixmap path
    Pixmap* px = AllocPixmap(4, 4);
    Str tga = tga::PixmapToTgaFormat(px);
    FreePixmap(px);
    for (int i = 1; i <= kPages; i++) {
        utassert(file::WriteFile(path::JoinTemp(dir, fmt("%03d.tga", i)), tga));
    }
    str::Free(tga);

    EngineBase* engine = CreateEngineImageDirFromFile(dir);
    utassert(engine && engine->PageCount() == kPages);

    utassert(engine->BenchLoadPage(1));
    CachedObject evicted{};
    utassert(FindPageObject(engine, 1, &evicted));
    utassert(evicted.canFree(nullptr, &evicted));

    for (int i = 2; i <= kPages; i++) {
        utassert(engine->BenchLoadPage(i));
    }
    CachedObject ignore{};
    utassert(!FindPageObject(engine, 1, &ignore));
    // the allocator may reuse the evicted address for a new page, which is then live
    if (!IsRegistered(evicted.id)) {
        utassert(!evicted.canFree(nullptr, &evicted));
        utassert(!evicted.free(nullptr, &evicted));
    }

    CachedObject live{};
    utassert(FindPageObject(engine, 2, &live));
    utassert(live.canFree(nullptr, &live));

    engine->Release();
    utassert(!live.canFree(nullptr, &live));
    utassert(!live.free(nullptr, &live));

    dir::RemoveAll(dir);
}

static void ResetCacheTest() {
    while (len(gCachedObjects) > 0) {
        UnregisterCachedObject(gCachedObjects[0].id);
    }
    for (bool& freed : gFreed) {
        freed = false;
    }
}

void CachedObjects_UnitTests() {
    int saved = gSaveMemory;
    ResetCacheTest();

    auto* e1 = (EngineBase*)(uintptr_t)0x100;
    auto* e2 = (EngineBase*)(uintptr_t)0x200;

    gSaveMemory = 0;
    CachedObject a = MkObj(1, 1000, e1);
    CachedObject b = MkObj(2, 2000, e1);
    CachedObject c = MkObj(3, 3000, e2);
    DidAllocateCachedObject(&a);
    DidAllocateCachedObject(&b);
    DidAllocateCachedObject(&c);
    utassert(len(gCachedObjects) == 3);
    utassert(!gFreed[2] && !gFreed[3]);

    u64 n = TryFreeCachedObjects(4000);
    utassert(n == 0);
    utassert(len(gCachedObjects) == 3);

    n = FreeCachedObjects();
    utassert(n == 5000);
    utassert(!gFreed[1]);
    utassert(gFreed[2] && gFreed[3]);
    utassert(len(gCachedObjects) == 1);
    utassert(gCachedObjects[0].id == 1);

    ResetCacheTest();

    gSaveMemory = 100;
    a = MkObj(1, 1000, e1);
    b = MkObj(2, 2000, e1);
    c = MkObj(3, 3000, e2);
    DidAllocateCachedObject(&a);
    DidAllocateCachedObject(&b);
    DidAllocateCachedObject(&c);
    // skip the object just added; id 1 cannot be freed, so 2 is dropped
    // when 3 is registered
    utassert(len(gCachedObjects) == 2);
    utassert(gFreed[2]);
    utassert(!gFreed[1] && !gFreed[3]);
    n = TryFreeCachedObjects(0);
    utassert(n == 3000);
    utassert(gFreed[3]);
    utassert(len(gCachedObjects) == 1);
    utassert(gCachedObjects[0].id == 1);

    ResetCacheTest();

    gSaveMemory = 0;
    a = MkObj(4, 100, e1);
    b = MkObj(5, 100, e2);
    DidAllocateCachedObject(&a);
    DidAllocateCachedObject(&b);
    FreeCachedObjectsForEngine(e1);
    utassert(gFreed[4]);
    utassert(!gFreed[5]);
    utassert(len(gCachedObjects) == 1);
    utassert(gCachedObjects[0].id == 5);

    ResetCacheTest();
    gSaveMemory = 0;
    a = MkObj(6, 2048, nullptr);
    a.kind = kindCachedRender;
    a.pageNo = 4;
    a.zoom = 125;
    b = MkObj(7, 4096, nullptr);
    b.kind = kindCachedImage;
    b.pageNo = 1;
    DidAllocateCachedObject(&a);
    DidAllocateCachedObject(&b);
    str::Builder dump;
    SerializeCachedObjects(dump);
    Str t = ToStr(dump);
    utassert(str::Contains(t, StrL("render")));
    utassert(str::Contains(t, StrL("image")));
    utassert(str::Contains(t, StrL("6.0 KB")));
    utassert(str::IndexOf(t, StrL("image")) < str::IndexOf(t, StrL("render")));

    ResetCacheTest();
    StaleImagePageTest();

    ResetCacheTest();
    if (kHasThreads) {
        TabLookupOffUIThreadTest();
        ResetCacheTest();
    }
    gSaveMemory = saved;
}

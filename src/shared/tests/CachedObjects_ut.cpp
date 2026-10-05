/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
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

static void ClearCachedObjects() {
    while (len(gCachedObjects) > 0) {
        UnregisterCachedObject(gCachedObjects[0].id);
    }
}

void CachedObjects_UnitTests() {
    int saved = gSaveMemory;
    ClearCachedObjects();
    for (int i = 0; i < 16; i++) {
        gFreed[i] = false;
    }

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

    ClearCachedObjects();
    for (int i = 0; i < 16; i++) {
        gFreed[i] = false;
    }

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

    ClearCachedObjects();
    for (int i = 0; i < 16; i++) {
        gFreed[i] = false;
    }

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

    ClearCachedObjects();
    for (int i = 0; i < 16; i++) {
        gFreed[i] = false;
    }
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

    ClearCachedObjects();
    gSaveMemory = saved;
}

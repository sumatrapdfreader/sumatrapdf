/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct WindowTab;
class EngineBase;
struct CachedObject;

using CachedObjectFn = bool (*)(WindowTab* currTab, CachedObject* o);

struct CachedObject {
    uintptr_t id = 0;
    u64 size = 0;
    Kind kind = nullptr;
    int pageNo = 0;
    float zoom = 0;
    EngineBase* engine = nullptr;
    WindowTab* tab = nullptr;
    CachedObjectFn canFree = nullptr;
    CachedObjectFn free = nullptr;
};

extern Kind kindCachedRender;
extern Kind kindCachedImage;

extern Vec<CachedObject> gCachedObjects;
// 0: trim only after allocation failure. 100: free everything CanFree allows.
extern int gSaveMemory;

extern WindowTab* (*gFindTabByEngine)(EngineBase*);
extern WindowTab* (*gCurrentTabForCache)();
extern void (*gOnCachedObjectsChanged)();

void DidAllocateCachedObject(CachedObject* o);
void UnregisterCachedObject(uintptr_t id);
u64 TryFreeCachedObjects(u64 newAllocationSize);
u64 FreeCachedObjects();
void FreeCachedObjectsForEngine(EngineBase* engine);
void SerializeCachedObjects(str::Builder& s);

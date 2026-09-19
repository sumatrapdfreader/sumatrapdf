/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

Kind kindNone = "none";

// if > 1 we won't crash when memory allocation fails
AtomicInt gAllowAllocFailure = 0;

u64 (*gTryFreeCachedObjects)(u64 newAllocationSize) = nullptr;
u64 (*gFreeCachedObjects)() = nullptr;

static void* MallocMaybeTrim(size_t size) {
    if (size >= kLargeAllocationSize && gTryFreeCachedObjects) {
        gTryFreeCachedObjects((u64)size);
    }
    void* p = malloc(size);
    if (p) {
        return p;
    }
    if (size >= kLargeAllocationSize && gFreeCachedObjects && gFreeCachedObjects() > 0) {
        p = malloc(size);
    }
    return p;
}

static void* ReallocMaybeTrim(void* mem, size_t newSize) {
    if (newSize >= kLargeAllocationSize && gTryFreeCachedObjects) {
        gTryFreeCachedObjects((u64)newSize);
    }
    void* p = realloc(mem, newSize);
    if (p || newSize == 0) {
        return p;
    }
    if (newSize >= kLargeAllocationSize && gFreeCachedObjects && gFreeCachedObjects() > 0) {
        p = realloc(mem, newSize);
    }
    return p;
}

// This exits so that I can add temporary instrumentation
// to catch allocations of a given size and it won't cause
// re-compilation of everything caused by changing Base.h
void* AllocZero(int count, int size) {
    return calloc(count, size);
}

bool MemEq(const void* s1, const void* s2, int n) {
    return 0 == memcmp(s1, s2, n);
}

int RoundUp(int n, int rounding) {
    if (rounding <= 1) {
        return n;
    }
    return ((n + rounding - 1) / rounding) * rounding;
}

void* RoundUp(void* d, int rounding) {
    if (rounding <= 1) {
        return d;
    }
    uintptr_t n = (uintptr_t)d;
    n = ((n + rounding - 1) / rounding) * rounding;
    return (void*)n;
}

int RoundToPowerOf2(int size) {
    int n = 1;
    while (n < size) {
        // Check before doubling so signed overflow is never UB.
        if (n > (INT_MAX / 2)) {
            return -1;
        }
        n *= 2;
    }
    return n;
}

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
static u32 hash_function_seed = 5381;

u32 MurmurHash2(const void* key, int n) {
    if (n <= 0) {
        return 0;
    }
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    const u32 m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    u32 h = hash_function_seed ^ (u32)n;

    /* Mix 4 bytes at a time into the hash */
    const u8* data = (const u8*)key;

    while (n >= 4) {
        u32 k = *(u32*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        n -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch (n) {
        case 3:
            h ^= data[2] << 16;
        case 2:
            h ^= data[1] << 8;
        case 1:
            h ^= data[0];
            h *= m;
    }

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

u32 MurmurHash2(Str s) {
    return MurmurHash2(s.s, s.len);
}

u32 MurmurHash2(WStr s) {
    return MurmurHash2(s.s, s.len * sizeofi(wchar_t));
}

Func0 MkFunc0Void(funcVoidPtr fn) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = Func0::kFuncNoArg;
    return res;
}

int setMinMax(int& v, int minVal, int maxVal) {
    v = std::max(v, minVal);
    v = std::min(v, maxVal);
    return v;
}

//--- Geom.cpp ----------------------------------------------------------------

bool QuadF::IsEmpty() const {
    return ul == ur && ul == ll && ul == lr;
}

// true when the glyph box is not axis-aligned (rotated / sheared text)
bool QuadF::IsRotated() const {
    if (IsEmpty()) {
        return false;
    }
    const float eps = 0.5f;
    return fabsf(ul.y - ur.y) > eps || fabsf(ll.y - lr.y) > eps || fabsf(ul.x - ll.x) > eps || fabsf(ur.x - lr.x) > eps;
}

PointF QuadF::Center() const {
    return {(ul.x + ur.x + ll.x + lr.x) / 4.f, (ul.y + ur.y + ll.y + lr.y) / 4.f};
}

// MuPDF winding is ul -> ur -> lr -> ll. Same-side cross products => inside.
bool QuadF::Contains(PointF p) const {
    if (IsEmpty()) {
        return false;
    }
    PointF pts[4] = {ul, ur, lr, ll};
    bool neg = false;
    bool pos = false;
    for (int i = 0; i < 4; i++) {
        PointF a = pts[i];
        PointF b = pts[(i + 1) % 4];
        float cross = ((b.x - a.x) * (p.y - a.y)) - ((b.y - a.y) * (p.x - a.x));
        if (cross < 0) {
            neg = true;
        } else if (cross > 0) {
            pos = true;
        }
        if (neg && pos) {
            return false;
        }
    }
    return true;
}

template <typename T>
RectG<T> RectG<T>::FromXY(T xs, T ys, T xe, T ye) {
    if (xs > xe) {
        std::swap(xs, xe);
    }
    if (ys > ye) {
        std::swap(ys, ye);
    }
    return {xs, ys, xe - xs, ye - ys};
}

// cf. fz_roundrect in mupdf/fitz/base_geometry.c
#ifndef FLT_EPSILON
constexpr float FLT_EPSILON = 1.192092896e-07f;
#endif

template <typename T>
RectG<int> RectG<T>::Round() const {
    float fx = (float)x, fy = (float)y, fdx = (float)dx, fdy = (float)dy;
    return Rect::FromXY((int)floorf(fx + FLT_EPSILON), (int)floorf(fy + FLT_EPSILON),
                        (int)ceilf(fx + fdx - FLT_EPSILON), (int)ceilf(fy + fdy - FLT_EPSILON));
}

// endpoint-exclusive, like RECT: https://devblogs.microsoft.com/oldnewthing/20040218-00/?p=40563
template <typename T>
bool RectG<T>::Contains(T px, T py) const {
    return px >= x && px < x + dx && py >= y && py < y + dy;
}

/* Returns an empty rectangle if there's no intersection (see IsEmpty). */
template <typename T>
RectG<T> RectG<T>::Intersect(RectG other) const {
    /* The intersection starts with the larger of the start coordinates
        and ends with the smaller of the end coordinates */
    T nx = std::max(x, other.x);
    T ny = std::max(y, other.y);
    T ndx = std::min(x + dx, other.x + other.dx) - nx;
    T ndy = std::min(y + dy, other.y + other.dy) - ny;

    /* return an empty rectangle if the dimensions aren't positive */
    if (ndx <= 0 || ndy <= 0) {
        return {};
    }
    return {nx, ny, ndx, ndy};
}

template <typename T>
RectG<T> RectG<T>::Union(RectG other) const {
    if (dx <= 0 || dy <= 0) {
        return other;
    }
    if (other.dx <= 0 || other.dy <= 0) {
        return *this;
    }

    /* The union starts with the smaller of the start coordinates
        and ends with the larger of the end coordinates */
    T nx = std::min(x, other.x);
    T ny = std::min(y, other.y);
    T ndx = std::max(x + dx, other.x + other.dx) - nx;
    T ndy = std::max(y + dy, other.y + other.dy) - ny;

    return {nx, ny, ndx, ndy};
}

template struct RectG<int>;
template struct RectG<float>;

// ------------- conversion functions

Point ToPoint(const PointF p) {
    return Point{(int)p.x, (int)p.y};
}

RectF ToRectF(const Rect& r) {
    return {(float)r.x, (float)r.y, (float)r.dx, (float)r.dy};
}

Rect ToRect(const RectF& r) {
    int x = (int)floor(r.x + 0.5);
    int y = (int)floor(r.y + 0.5);
    int dx = (int)floor(r.dx + 0.5);
    int dy = (int)floor(r.dy + 0.5);
    return {x, y, dx, dy};
}

// conversions to and from the Win32 / GDI+ geometry types; see Geom.h
POINT ToPOINT(const Point& p) {
    return {p.x, p.y};
}

RECT ToRECT(const Rect& r) {
    return {r.x, r.y, r.x + r.dx, r.y + r.dy};
}

RECT ToRECT(const RectF& r) {
    return {(int)r.x, (int)r.y, (int)(r.x + r.dx), (int)(r.y + r.dy)};
}

int RectDx(const RECT& r) {
    return r.right - r.left;
}
int RectDy(const RECT& r) {
    return r.bottom - r.top;
}

Rect ToRect(const RECT& r) {
    Rect r2 = {r.left, r.top, RectDx(r), RectDy(r)};
    return r2;
}

Gdiplus::Rect ToGdipRect(const Rect& r) {
    return {r.x, r.y, r.dx, r.dy};
}

Gdiplus::RectF ToGdipRectF(const Rect& r) {
    return {(float)r.x, (float)r.y, (float)r.dx, (float)r.dy};
}

Gdiplus::Rect ToGdipRect(const RectF& r) {
    Rect rect = ToRect(r);
    return {rect.x, rect.y, rect.dx, rect.dy};
}

Gdiplus::RectF ToGdipRectF(const RectF& r) {
    return {r.x, r.y, r.dx, r.dy};
}

int NormalizeRotation(int rotation) {
    while (rotation < 0) {
        rotation += 360;
    }
    while (rotation >= 360) {
        rotation -= 360;
    }
    if ((rotation % 90) != 0) {
        ReportIf(true);
        return 0;
    }
    return rotation;
}

//--- Thread.cpp ----------------------------------------------------------------

#include "base/WinDynCalls.h"

// Names the thread for debuggers; only Windows 10 1607+ has the API.
void SetThreadName(Str threadName, ThreadId threadId) {
    if (len(threadName) == 0 || !DynSetThreadDescription) {
        return;
    }
    HANDLE h = threadId ? OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, threadId) : GetCurrentThread();
    if (!h) {
        return;
    }
    DynSetThreadDescription(h, CWStrTemp(threadName));
    if (threadId) {
        CloseHandle(h);
    }
}

static DWORD WINAPI ThreadFunc0(void* data) {
    auto* fn = (Func0*)data;
    fn->Call();
    delete fn;
    DestroyTempArena();
    return 0;
}

ThreadHandle StartThread(const Func0& fn, Str threadName) {
    auto* fp = new Func0(fn);
    ThreadId threadId = 0;
    ThreadHandle hThread = CreateThread(nullptr, 0, ThreadFunc0, (void*)fp, 0, &threadId);
    if (!hThread) {
        delete fp;
        return nullptr;
    }
    if (threadName) {
        SetThreadName(threadName, threadId);
    }
    return hThread;
}

void RunAsync(const Func0& fn, Str threadName) {
    ThreadHandle hThread = StartThread(fn, threadName);
    SafeCloseThreadHandle(&hThread);
}

void SleepInMs(int ms) {
    if (ms <= 0) {
        return;
    }
    Sleep((DWORD)ms);
}

AtomicInt gDangerousThreadCount = 0;

bool AreDangerousThreadsPending() {
    auto count = AtomicIntGet(&gDangerousThreadCount);
    return count != 0;
}

//--- Arena.cpp ----------------------------------------------------------------

static u64 gArenaDefaultReserveSize = 64ull * 1024ull * 1024ull;
static u64 gArenaDefaultCommitSize = 64ull * 1024ull;

static u64 ArenaAlignPow2(u64 value, u64 align) {
    if (align <= 1) {
        return value;
    }
    ReportIf((align & (align - 1)) != 0);
    return (value + align - 1) & ~(align - 1);
}

static u64 ArenaPageSize() {
    static u64 pageSize = 0;
    if (pageSize == 0) {
        SYSTEM_INFO info = {};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
    }
    return pageSize;
}

static bool ArenaCommit(void* base, u64 size) {
    if (size == 0) {
        return true;
    }
    return VirtualAlloc(base, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

static void ArenaRelease(Arena* arena) {
    VirtualFree(arena, 0, MEM_RELEASE);
}

static void* ArenaPushLocked(Arena* arena, u64 size, u64 align, bool zero) {
    if (!arena) {
        return nullptr;
    }
    if (align == 0) {
        align = 1;
    }

    Arena* current = arena->current;
    u64 posPre = ArenaAlignPow2(current->pos, align);
    u64 posPost = posPre + size;

    u64 sizeToZero = 0;
    if (zero && current->committed > posPre) {
        sizeToZero = std::min(current->committed, posPost) - posPre;
    }

    if (current->reserved < posPost) {
        // from the head, not from `current`: a block made to hold one
        // oversized allocation carries that allocation's size as its chunk
        // size, and it stays `current` afterwards. Taking the next block's
        // size from it would reserve - and, since the two are equal there,
        // commit - the whole of it for the next small push.
        ArenaParams newParams = {arena->reserveChunkSize, arena->commitChunkSize};
        if (size + kArenaHeaderSize > newParams.reserveSize) {
            newParams.reserveSize = ArenaAlignPow2(size + kArenaHeaderSize, std::max(align, ArenaPageSize()));
            newParams.commitSize = newParams.reserveSize;
        }

        Arena* newBlock = ArenaNew(newParams);
        if (!newBlock) {
            return nullptr;
        }

        newBlock->basePos = current->basePos + current->reserved;
        newBlock->prev = current;
        arena->current = newBlock;
        current = newBlock;
        posPre = ArenaAlignPow2(current->pos, align);
        posPost = posPre + size;
        sizeToZero = 0;
    }

    if (current->committed < posPost) {
        u64 commitEnd = ArenaAlignPow2(posPost, current->commitChunkSize);
        u64 commitClamped = std::min(commitEnd, current->reserved);
        u64 commitSize = commitClamped - current->committed;
        void* commitPtr = (char*)current + current->committed;
        if (!ArenaCommit(commitPtr, commitSize)) {
            return nullptr;
        }
        current->committed = commitClamped;
    }

    if (current->committed < posPost) {
        return nullptr;
    }

    void* result = (char*)current + posPre;
    current->pos = posPost;

    // update allocation stats on the head arena (stats live on the head, not on
    // chained blocks). peak is the high-water mark of total bytes used.
    arena->nAllocsLifetime++;
    arena->nAllocsSinceReset++;
    u64 used = current->basePos + posPost;
    arena->peakBytesLifetime = std::max(used, arena->peakBytesLifetime);
    arena->peakBytesSinceReset = std::max(used, arena->peakBytesSinceReset);

    if (sizeToZero) {
        memset(result, 0, (size_t)sizeToZero);
    }
    return result;
}

ArenaParams ArenaDefaultParams() {
    return {gArenaDefaultReserveSize, gArenaDefaultCommitSize};
}

Arena* ArenaNew(const ArenaParams& params) {
    const u64 pageSize = ArenaPageSize();
    u64 reserveSize = params.reserveSize ? params.reserveSize : gArenaDefaultReserveSize;
    u64 commitSize = params.commitSize ? params.commitSize : gArenaDefaultCommitSize;
    reserveSize = ArenaAlignPow2(std::max(reserveSize, kArenaHeaderSize), pageSize);
    commitSize = std::min(ArenaAlignPow2(std::max(commitSize, kArenaHeaderSize), pageSize), reserveSize);

    void* base = VirtualAlloc(nullptr, (SIZE_T)reserveSize, MEM_RESERVE, PAGE_READWRITE);
    if (!base) {
        return nullptr;
    }
    if (!ArenaCommit(base, commitSize)) {
        VirtualFree(base, 0, MEM_RELEASE);
        return nullptr;
    }

    memset(base, 0, (size_t)std::min<u64>(commitSize, kArenaHeaderSize));
    Arena* arena = (Arena*)base;
    arena->prev = nullptr;
    arena->current = arena;
    arena->commitChunkSize = commitSize;
    arena->reserveChunkSize = reserveSize;
    arena->basePos = 0;
    arena->pos = kArenaHeaderSize;
    arena->committed = commitSize;
    arena->reserved = reserveSize;
    return arena;
}

void ArenaDelete(Arena* arena) {
    if (!arena) {
        return;
    }

    Arena* node = arena->current;
    while (node) {
        Arena* prev = node->prev;
        ArenaRelease(node);
        node = prev;
    }
}

void* Arena::Push(u64 size, u64 align, bool zero) {
    if (!this) {
        return nullptr;
    }
    lock.Lock();
    void* mem = ArenaPushLocked(this, size, align, zero);
    lock.Unlock();
    return mem;
}

u64 Arena::Pos() {
    if (!this) {
        return 0;
    }
    return current->basePos + current->pos;
}

void Arena::PopTo(u64 pos) {
    if (!this) {
        return;
    }

    lock.Lock();

    u64 bigPos = std::max(kArenaHeaderSize, pos);
    Arena* curr = current;
    while (curr && curr->basePos >= bigPos) {
        Arena* prev = curr->prev;
        ArenaRelease(curr);
        curr = prev;
    }

    if (!curr) {
        lock.Unlock();
        return;
    }

    current = curr;
    u64 newPos = bigPos - curr->basePos;
    ReportIf(newPos > curr->pos);
    curr->pos = newPos;
    lock.Unlock();
}

void Arena::Pop(u64 amt) {
    u64 posOld = Pos();
    u64 posNew = (amt < posOld) ? (posOld - amt) : 0;
    PopTo(posNew);
}

// ArenaPtrCompress / ArenaPtrUncompress: store a pointer as a u32 offset from
// the first block in the arena chain. The head has basePos 0; each chained
// block has basePos = sum of previous blocks' reserved. nullptr compresses to 0.
// Pointers must belong to this arena (any block). Offsets beyond u32 fail.

// Walk current -> prev to find the block whose reserved range contains ptr.
static Arena* ArenaFindBlockContaining(Arena* arena, const void* ptr) {
    for (Arena* block = arena->current; block; block = block->prev) {
        char* base = (char*)block;
        if ((const char*)ptr >= base && (const char*)ptr < base + block->reserved) {
            return block;
        }
    }
    return nullptr;
}

// Walk current -> prev to find the block whose basePos range contains offset.
static Arena* ArenaFindBlockForOffset(Arena* arena, u64 offset) {
    for (Arena* block = arena->current; block; block = block->prev) {
        if (offset >= block->basePos && offset < block->basePos + block->reserved) {
            return block;
        }
    }
    return nullptr;
}

u32 ArenaPtrCompress(Arena* arena, void* ptr) {
    if (!arena || !ptr) {
        return 0;
    }
    arena->lock.Lock();
    Arena* block = ArenaFindBlockContaining(arena, ptr);
    if (!block) {
        arena->lock.Unlock();
        ReportIf(true);
        return 0;
    }
    u64 off = block->basePos + (u64)((char*)ptr - (char*)block);
    arena->lock.Unlock();
    if (off > 0xffffffffull) {
        ReportIf(true);
        return 0;
    }
    return (u32)off;
}

void* ArenaPtrUncompress(Arena* arena, u32 compressed) {
    if (!arena || compressed == 0) {
        return nullptr;
    }
    arena->lock.Lock();
    Arena* block = ArenaFindBlockForOffset(arena, compressed);
    if (!block) {
        arena->lock.Unlock();
        ReportIf(true);
        return nullptr;
    }
    void* ptr = (char*)block + (compressed - block->basePos);
    arena->lock.Unlock();
    return ptr;
}

void* Arena::Alloc(int size) {
    if (size <= 0) {
        return nullptr;
    }
    return Push((u64)size, 8, false);
}

void Arena::Reset() {
    PopTo(0);
    nAllocsSinceReset = 0;
    peakBytesSinceReset = 0;
}

// size_t overloads that match the legacy Allocator::* static helper API
// and fall back to malloc/free when arena is nullptr.
void* Alloc(Arena* arena, int size) {
    if (size <= 0) {
        return nullptr;
    }
    if (!arena) {
        return MallocMaybeTrim((size_t)size);
    }
    return arena->Alloc(size);
}

void Free(Arena* arena, void* mem) {
    // Arena has no free
    if (arena) return;
    free(mem);
}

// size_t overloads that match the legacy Allocator::* static helper API
// and fall back to malloc/free when arena is nullptr.
void* Alloc(Arena* arena, size_t size) {
    if (size == 0) {
        return nullptr;
    }
    if (!arena) {
        return MallocMaybeTrim(size);
    }
    if (size >= kLargeAllocationSize && gTryFreeCachedObjects) {
        gTryFreeCachedObjects((u64)size);
    }
    return arena->Push((u64)size, 8, false);
}

void* AllocZero(Arena* arena, size_t size) {
    if (size == 0) {
        return nullptr;
    }
    if (!arena) {
        void* mem = MallocMaybeTrim(size);
        if (mem) {
            memset(mem, 0, size);
        }
        return mem;
    }
    if (size >= kLargeAllocationSize && gTryFreeCachedObjects) {
        gTryFreeCachedObjects((u64)size);
    }
    return arena->Push((u64)size, 8, true);
}

void* Realloc(Arena* arena, void* mem, size_t newSize, size_t copySize) {
    if (!arena) {
        return ReallocMaybeTrim(mem, newSize);
    }
    // Arena has no realloc: allocate fresh and copy. Old memory is not freed
    // (arena lifetime handles it).
    if (newSize == 0) {
        return nullptr;
    }
    void* newMem = arena->Push((u64)newSize, 8, false);
    if (newMem && mem && copySize > 0) {
        // Arena bump allocations can end up adjacent to (and overlapping) the
        // old block; memmove handles that. copySize is the caller's used bytes.
        size_t n = copySize;
        n = std::min(n, newSize);
        memmove(newMem, mem, n);
    }
    return newMem;
}

void* MemDup(Arena* arena, const void* mem, size_t size, size_t extraBytes) {
    void* newMem = Alloc(arena, size + extraBytes);
    if (!newMem) {
        return nullptr;
    }
    if (mem && size) {
        memcpy(newMem, mem, size);
    }
    // zero the tail so callers using extraBytes to append a null terminator
    // (e.g. str::Dup with extraBytes = sizeof(char)) don't read uninitialized
    // memory. When allocated from an arena via Push(..., zero=false) or from
    // malloc() the bytes past `size` aren't otherwise zeroed.
    if (extraBytes > 0) {
        memset((char*)newMem + size, 0, extraBytes);
    }
    return newMem;
}

thread_local Arena* gTempArena = nullptr;

Arena* GetTempArena() {
    if (!gTempArena) {
        gTempArena = ArenaNew();
    }
    return gTempArena;
}

void ResetTempArena() {
    if (gTempArena) {
        gTempArena->Reset();
    }
}

void DestroyTempArena() {
    ArenaDelete(gTempArena);
    gTempArena = nullptr;
}

Arena* gPermArena = nullptr;

Arena* GetPermArena() {
    if (!gPermArena) {
        gPermArena = ArenaNew();
    }
    return gPermArena;
}

void DestroyPermArena() {
    ArenaDelete(gPermArena);
    gPermArena = nullptr;
}

void* AllocTemp(int size, u64 align) {
    Arena* arena = GetTempArena();
    return arena->Push((u64)size, align, false);
}

// allocate null-terminated string
Str AllocStrTemp(int size) {
    // a negative size would ask the arena for close to 2^64 bytes and then
    // terminate at a negative offset from whatever came back
    if (size <= 0) {
        return {};
    }
    Arena* arena = GetTempArena();
    char* res = (char*)arena->Push((u64)size + 1, 1, false);
    if (!res) {
        return {};
    }
    res[size] = 0;
    return Str(res, size);
}

// Grow/shrink vec storage to newCap elements, plus one trailing zero-pad
// element (so Vec<char>/Vec<WCHAR> stay C-string compatible).
// Keeps the first min(len, newCap) elements; zeros the rest of the new block.
// Updates *els and *cap. len is not modified (caller owns logical length).
// Grow/shrink vec-like storage to newCap elements (+1 trailing zero pad).
// Updates *els and *cap; keeps min(len, newCap) elements.
NO_INLINE bool VecRealloc(Arena* a, void** els, int len, int* cap, int newCap, int elSize) {
    // newCap+1 must fit in int; newElCount * elSize must not overflow.
    if (elSize <= 0 || newCap < 0 || newCap > INT_MAX - 1) {
        return false;
    }
    int newElCount = newCap + 1;
    if (newElCount > INT_MAX / elSize) {
        return false;
    }

    int keep = len;
    keep = std::max(keep, 0);
    keep = std::min(keep, newCap);
    int oldSize = keep * elSize;
    int allocSize = newElCount * elSize;

    // Realloc(a, nullptr, n, 0) is malloc-like; single path for first alloc and grow.
    void* newEls = Realloc(a, *els, (size_t)allocSize, (size_t)oldSize);
    if (!newEls) {
        ReportIf(AtomicIntGet(&gAllowAllocFailure) == 0);
        return false;
    }
    int tail = allocSize - oldSize;
    if (tail > 0) {
        memset((char*)newEls + oldSize, 0, (size_t)tail);
    }
    *els = newEls;
    *cap = newCap;
    return true;
}

// Doubling, but never from a first capacity of one. max(cap * 2, wanted)
// out of an empty vec hands back 1, so a vec that ends up holding four
// elements reallocates and memcpys three times on the way there. The floor
// is in bytes rather than in elements (Rust's RawVec::MIN_NON_ZERO_CAP):
// four 192-byte items is a sensible first block and four 4 KB ones is not.
static int VecNextCap(int cap, int wanted, int elSize) {
    if (cap == 0) {
        int floorCap = elSize == 1 ? 8 : elSize <= 1024 ? 4 : 1;
        return std::max(floorCap, wanted);
    }
    return std::max(cap * 2, wanted);
}

// The bodies below take the element type erased in a VecNonTemplated, so they
// are compiled once rather than once per Vec<T>. A negative cap means the
// elements sit in storage the vec borrowed (VecUseExternalBuffer); growing past
// it allocates and copies, and leaves the borrowed block alone.

NO_INLINE bool VecReserveNT(Arena* arena, VecNonTemplated* v, int elSize, int wantedSize) {
    int cap = v->cap;
    int curCap = cap < 0 ? -cap : cap;
    // cap without els is a size hint (ByteWriter used to set it and drop writes)
    if (wantedSize <= curCap && v->els) {
        return true;
    }
    if (wantedSize <= 0) {
        return true;
    }
    int newCap = VecNextCap(curCap, wantedSize, elSize);
    if (cap < 0) {
        void* borrowed = v->els;
        v->els = nullptr;
        v->cap = 0;
        if (!VecRealloc(arena, &v->els, 0, &v->cap, newCap, elSize)) {
            v->els = borrowed;
            v->cap = -curCap;
            return false;
        }
        if (v->len > 0) {
            memcpy(v->els, borrowed, (size_t)v->len * (size_t)elSize);
        }
        return true;
    }
    return VecRealloc(arena, &v->els, v->len, &v->cap, newCap, elSize);
}

NO_INLINE void* VecInsertSpaceNT(VecNonTemplated* v, int elSize, int idx, int count) {
    int len = v->len;
    int newLen = std::max(len, idx) + count;
    if (!VecReserveNT(nullptr, v, elSize, newLen)) {
        return nullptr;
    }
    char* res = (char*)v->els + ((size_t)idx * (size_t)elSize);
    if (len > idx) {
        char* dst = res + ((size_t)count * (size_t)elSize);
        memmove(dst, res, (size_t)(len - idx) * (size_t)elSize);
    }
    v->len = newLen;
    return res;
}

NO_INLINE bool VecResizeNT(VecNonTemplated* v, int elSize, int newSize) {
    if (newSize < 0) {
        return false;
    }
    int curCap = v->cap < 0 ? -v->cap : v->cap;
    if (newSize > curCap) {
        if (!VecReserveNT(nullptr, v, elSize, newSize)) {
            return false;
        }
        curCap = v->cap < 0 ? -v->cap : v->cap;
    }
    v->len = newSize;
    if (v->els && curCap > newSize) {
        char* tail = (char*)v->els + ((size_t)newSize * (size_t)elSize);
        memset(tail, 0, (size_t)(curCap - newSize) * (size_t)elSize);
    }
    return true;
}

NO_INLINE void VecRemoveAtNT(VecNonTemplated* v, int elSize, int idx, int count) {
    int len = v->len;
    char* els = (char*)v->els;
    if (len > idx + count) {
        char* dst = els + ((size_t)idx * (size_t)elSize);
        char* src = els + ((size_t)(idx + count) * (size_t)elSize);
        memmove(dst, src, (size_t)(len - idx - count) * (size_t)elSize);
    }
    len -= count;
    memset(els + ((size_t)len * (size_t)elSize), 0, (size_t)count * (size_t)elSize);
    v->len = len;
}

// replaces the removed element with the last one, so it copies less than
// VecRemoveAtNT but does not keep the order
NO_INLINE void VecRemoveAtFastNT(VecNonTemplated* v, int elSize, int idx) {
    int len = v->len;
    ReportIf(idx >= len);
    if (idx >= len) {
        return;
    }
    char* els = (char*)v->els;
    char* toRemove = els + ((size_t)idx * (size_t)elSize);
    char* last = els + ((size_t)(len - 1) * (size_t)elSize);
    if (toRemove != last) {
        memcpy(toRemove, last, (size_t)elSize);
    }
    memset(last, 0, (size_t)elSize);
    v->len = len - 1;
}

// frees the storage and empties the vec: len, cap and els all go to 0
NO_INLINE void VecFreeElementsNT(VecNonTemplated* v) {
    v->len = 0;
    if (!v->els) {
        return;
    }
    if (v->cap > 0) {
        Free(nullptr, v->els);
    }
    v->cap = 0;
    v->els = nullptr;
}

NO_INLINE void VecClearNT(VecNonTemplated* v, int elSize) {
    v->len = 0;
    int curCap = v->cap < 0 ? -v->cap : v->cap;
    if (v->els && curCap > 0) {
        memset(v->els, 0, (size_t)curCap * (size_t)elSize);
    }
}

// hands the storage over to the caller, leaving the vec empty. Borrowed
// storage is copied to the heap first, since the caller gets to free it.
NO_INLINE void* VecTakeNT(VecNonTemplated* v, int elSize) {
    void* els = v->els;
    if (v->cap < 0) {
        int n = v->len;
        v->els = nullptr;
        v->cap = 0;
        v->len = 0;
        if (n <= 0) {
            return nullptr;
        }
        if (!VecRealloc(nullptr, &v->els, 0, &v->cap, n, elSize)) {
            return nullptr;
        }
        void* res = v->els;
        memcpy(res, els, (size_t)n * (size_t)elSize);
        v->els = nullptr;
        v->cap = 0;
        return res;
    }
    v->els = nullptr;
    v->len = 0;
    v->cap = 0;
    return els;
}

// Vec only holds POD, so copying is a reserve plus a memcpy. zeroTail is for
// operator=, which zeroes the capacity past the new length.
NO_INLINE void VecCopyFromNT(VecNonTemplated* v, int elSize, int srcLen, const void* srcEls, bool zeroTail) {
    VecReserveNT(nullptr, v, elSize, srcLen);
    v->len = srcLen;
    if (srcLen > 0 && srcEls && v->els) {
        memcpy(v->els, srcEls, (size_t)srcLen * (size_t)elSize);
    }
    if (zeroTail && v->els) {
        int curCap = v->cap < 0 ? -v->cap : v->cap;
        if (curCap > srcLen) {
            char* tail = (char*)v->els + ((size_t)srcLen * (size_t)elSize);
            memset(tail, 0, (size_t)(curCap - srcLen) * (size_t)elSize);
        }
    }
}

// Logs an arena's lifetime allocation count and peak bytes. Call on exit, before
// logging is torn down.
void LogArenaStats(Str what, Arena* a) {
    if (!a) {
        return;
    }
    u64 nAllocs = a->nAllocsLifetime;
    u64 peakBytes = a->peakBytesLifetime;
    logf("%s lifetime: %s allocations, peak %s bytes (%s)\n", what, str::FormatNumWithThousandSepTemp((i64)nAllocs),
         str::FormatNumWithThousandSepTemp((i64)peakBytes), FormatFileSizeTemp(peakBytes));
}

//--- Str.cpp ----------------------------------------------------------------

//--- bodies shared by the str:: and wstr:: twins ------------------------------

template <typename S>
static S DupT(Arena* a, S s) {
    if (!s.s || s.len < 0) {
        return {};
    }
    using C = std::remove_pointer_t<decltype(s.s)>;
    return S((C*)MemDup(a, s.s, (size_t)s.len * sizeof(C), sizeof(C)), s.len);
}

static int LowerChar(char c) {
    return tolower((u8)c);
}
static int LowerChar(WCHAR c) {
    return WCharToLower(c);
}

// strcmp-style (<0, 0, >0), unsigned per char. Empty/null sorts before non-empty.
template <typename S>
static int CmpT(S a, S b, bool ignoreCase) {
    if (a.s == b.s && a.len == b.len) {
        return 0;
    }
    if (len(a) == 0) {
        return len(b) == 0 ? 0 : -1;
    }
    if (len(b) == 0) {
        return 1;
    }
    using U = std::make_unsigned_t<std::remove_pointer_t<decltype(a.s)>>;
    int n = std::min(a.len, b.len);
    for (int i = 0; i < n; i++) {
        int c1 = ignoreCase ? LowerChar(a.s[i]) : (int)(U)a.s[i];
        int c2 = ignoreCase ? LowerChar(b.s[i]) : (int)(U)b.s[i];
        if (c1 != c2) {
            return c1 < c2 ? -1 : 1;
        }
    }
    return a.len - b.len;
}

template <typename S>
static bool EndsWithT(S txt, S end, bool (*eq)(S, S)) {
    if (len(txt) == 0 || len(end) == 0 || end.len > txt.len) {
        return false;
    }
    return eq(S(txt.s + txt.len - end.len, end.len), end);
}

template <typename S, typename C>
static int IndexOfCharT(S s, C c) {
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == c) {
            return i;
        }
    }
    return -1;
}

template <typename S, typename C>
static S SliceFromCharT(S s, C c) {
    int idx = IndexOfCharT(s, c);
    return idx < 0 ? S{} : S(s.s + idx, s.len - idx);
}

template <typename S>
static void TransCharsInPlaceT(S& s, S oldChars, S newChars) {
    int nDiff = len(oldChars) - len(newChars);
    ReportIf(nDiff < 0);
    int nChanged = 0;
    for (int i = 0; i < s.len; i++) {
        int idx = IndexOfCharT(oldChars, s.s[i]);
        if (idx >= 0) {
            s.s[i] = newChars.s[idx];
            nChanged++;
        }
    }
    if (nChanged * nDiff > 0) {
        s.s[s.len] = 0;
    }
}

template <typename S>
static int RemoveCharsInPlaceT(S s, S toRemove) {
    if (len(s) == 0) {
        return 0;
    }
    int dst = 0;
    for (int src = 0; src < s.len; src++) {
        if (IndexOfCharT(toRemove, s.s[src]) < 0) {
            s.s[dst++] = s.s[src];
        }
    }
    s.s[dst] = 0;
    return s.len - dst;
}

// replaces all whitespace characters with spaces, collapses several
// consecutive spaces into one and strips heading/trailing ones
// returns the number of removed characters
template <typename S, typename C>
static int NormalizeWSInPlaceT(S s, bool (*isWs)(C)) {
    if (len(s) == 0) {
        return 0;
    }
    int dst = 0;
    bool addedSpace = true;
    for (int src = 0; src < s.len; src++) {
        if (!isWs(s.s[src])) {
            s.s[dst++] = s.s[src];
            addedSpace = false;
        } else if (!addedSpace) {
            s.s[dst++] = ' ';
            addedSpace = true;
        }
    }
    if (dst > 0 && isWs(s.s[dst - 1])) {
        dst--;
    }
    s.s[dst] = 0;
    return s.len - dst;
}

namespace str {

void Free(Str s) {
    free(s.s);
}
void FreePtr(Str* s) {
    free(s->s);
    *s = {};
}
Str Dup(Arena* a, Str s) {
    return DupT(a, s);
}
Str Dup(Str s) {
    return DupT(nullptr, s);
}
int Cmp(Str a, Str b) {
    return CmpT(a, b, false);
}
int CmpI(Str a, Str b) {
    return CmpT(a, b, true);
}
bool EndsWith(Str txt, Str end) {
    return EndsWithT(txt, end, str::Eq);
}
bool EndsWithI(Str txt, Str end) {
    return EndsWithT(txt, end, str::EqI);
}
int IndexOfChar(Str s, char c) {
    return IndexOfCharT(s, c);
}
bool ContainsChar(Str s, char c) {
    return IndexOfCharT(s, c) >= 0;
}
Str SliceFromChar(Str s, char c) {
    return SliceFromCharT(s, c);
}
void TransCharsInPlace(Str& s, Str oldChars, Str newChars) {
    TransCharsInPlaceT(s, oldChars, newChars);
}
int RemoveCharsInPlace(Str s, Str toRemove) {
    return RemoveCharsInPlaceT(s, toRemove);
}
int NormalizeWSInPlace(Str s) {
    return NormalizeWSInPlaceT(s, str::IsWs);
}

} // namespace str

namespace wstr {

void Free(WStr s) {
    free(s.s);
}
void FreePtr(WStr* s) {
    free(s->s);
    *s = {};
}
WStr Dup(Arena* a, WStr s) {
    return DupT(a, s);
}
WStr Dup(WStr s) {
    return DupT(nullptr, s);
}
int Cmp(WStr a, WStr b) {
    return CmpT(a, b, false);
}
int CmpI(WStr a, WStr b) {
    return CmpT(a, b, true);
}
bool EndsWith(WStr txt, WStr end) {
    return EndsWithT(txt, end, wstr::Eq);
}
bool EndsWithI(WStr txt, WStr end) {
    return EndsWithT(txt, end, wstr::EqI);
}
int IndexOfChar(WStr s, WCHAR c) {
    return IndexOfCharT(s, c);
}
bool ContainsChar(WStr s, WCHAR c) {
    return IndexOfCharT(s, c) >= 0;
}
WStr SliceFromChar(WStr s, WCHAR c) {
    return SliceFromCharT(s, c);
}
void TransCharsInPlace(WStr& s, WStr oldChars, WStr newChars) {
    TransCharsInPlaceT(s, oldChars, newChars);
}
int RemoveCharsInPlace(WStr s, WStr toRemove) {
    return RemoveCharsInPlaceT(s, toRemove);
}
int NormalizeWSInPlace(WStr s) {
    return NormalizeWSInPlaceT(s, wstr::IsWs);
}

} // namespace wstr

#ifndef _MSC_VER
#define _strdup strdup
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif

// StrArena: u32 handle from ArenaPtrCompress. Arena layout is unsigned LEB128
// length, length bytes of payload, trailing 0 for C APIs. 0 is the null handle.

// Unicode lowercase for one BMP code unit. ASCII is a fast path; Windows uses
// CharLowerW, other platforms a Latin/Cyrillic/Greek table then towlower.
wchar_t WCharToLower(wchar_t c) {
    if (c < 0x80) {
        if (c >= 'A' && c <= 'Z') {
            return c + ('a' - 'A');
        }
        return c;
    }
    return (wchar_t)(uintptr_t)CharLowerW((LPWSTR)(uintptr_t)c);
}

// locale-independent lowercase of a codepoint for case-insensitive matching
int FoldCaseRune(int c) {
    // CharLowerW maps İ (U+0130) to 'i' only under Turkish locale (issue #5597)
    if (c == 0x0130) {
        return 'i';
    }
    if (c > 0 && c <= 0xffff) {
        return WCharToLower((wchar_t)c);
    }
    return c;
}

bool IsCombiningMark(int c) {
    return c >= 0x300 && c <= 0x36f;
}

// strip diacritics from a codepoint: 'é' -> 'e', 'ł' -> 'l'. Case is preserved
int FoldDiacriticsRune(int c) {
    if (c < 0x80 || c > 0xffff) {
        return c;
    }

    // letters that don't decompose into base + combining mark: ŁłĐđØøĦħı
    // clang-format off
static const struct {
    u16 cp;
    char ch;
} kNoDecomp[] = {
    {0x141, 'L'}, {0x142, 'l'}, {0x110, 'D'}, {0x111, 'd'}, {0xd8, 'O'},
    {0xf8, 'o'},  {0x126, 'H'}, {0x127, 'h'}, {0x131, 'i'},
};
    // clang-format on
    for (auto& e : kNoDecomp) {
        if (e.cp == c) {
            return e.ch;
        }
    }

    // 'é' -> 'e' + U+0301
    WCHAR w = (WCHAR)c;
    WCHAR decomposed[8];
    int n = FoldStringW(MAP_COMPOSITE, &w, 1, decomposed, dimofi(decomposed));
    if (n > 1 && IsCombiningMark(decomposed[1])) {
        return decomposed[0];
    }
    return c;
}

// Locale-independent Unicode lowercase folding for case-insensitive matching.
static void FoldCaseWInPlace(WStr s) {
    CharLowerBuffW(s.s, (DWORD)s.len);
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == 0x0130) {
            s.s[i] = L'i';
        }
    }
}

static int Utf8ByteOffsetForWCharOffset(Str s, int wcharOff) {
    if (wcharOff <= 0) {
        return 0;
    }
    int byteOff = 0;
    int nWide = 0;
    while (byteOff < s.len && nWide < wcharOff) {
        int prevByteOff = byteOff;
        int codepoint = Utf8CodepointNext(s, byteOff);
        int wcharUnits = sizeof(wchar_t) == 2 && codepoint > 0xffff ? 2 : 1;
        if (nWide + wcharUnits > wcharOff) {
            return prevByteOff;
        }
        nWide += wcharUnits;
    }
    return byteOff;
}

// One allocation: sizeofi(StrNode) + s.len + 1. a==null => malloc; else arena.
StrNode* AllocStrNode(Arena* a, Str s) {
    int n = s.len;
    n = std::max(n, 0);
    int cb = sizeofi(StrNode) + n + 1;
    auto* node = (StrNode*)Alloc(a, cb);
    if (!node) {
        return nullptr;
    }
    char* dst = (char*)node + sizeofi(StrNode);
    if (n > 0 && s.s) {
        memcpy(dst, s.s, (size_t)n);
    }
    dst[n] = 0;
    node->next = nullptr;
    node->s = Str(dst, n);
    return node;
}

// first node whose string equals s (case-sensitive), null if none
StrNode* FindStrNode(StrNode* root, Str s) {
    StrNode* curr = root;
    while (curr) {
        if (str::Eq(curr->s, s)) {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

// Malloc path (a==null): free each node. Arena path: no per-node free.
// Frees the list with free() when a==null (malloc path). Arena path is a no-op.
void FreeStrNode(Arena* a, StrNode* head) {
    if (a) {
        return;
    }
    while (head) {
        StrNode* next = head->next;
        free(head);
        head = next;
    }
}

// Append n as the new last node. Clears n->next. List does not free nodes.
void StrNodeListPush(StrNodeList* list, StrNode* n) {
    ReportIf(!list || !n);
    n->next = nullptr;
    if (list->tail) {
        list->tail->next = n;
    } else {
        list->head = n;
    }
    list->tail = n;
}

// Unlink the last node. Does not free it; list becomes empty if it was the only node.
void StrNodeListPop(StrNodeList* list) {
    ReportIf(!list || !list->tail);
    if (list->head == list->tail) {
        list->head = nullptr;
        list->tail = nullptr;
        return;
    }
    StrNode* prev = list->head;
    while (prev->next != list->tail) {
        prev = prev->next;
    }
    prev->next = nullptr;
    list->tail = prev;
}

namespace str {

// length up to the first NUL within len: a Str may span more than its C string
static int CStrLen(Str s) {
    int n = 0;
    while (s.s && n < s.len && s.s[n]) {
        n++;
    }
    return n;
}

// return true if s1 == s2, case sensitive
bool Eq(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    int n = CStrLen(s1);
    return n == CStrLen(s2) && (n == 0 || MemEq(s1.s, s2.s, n));
}

// return true if s1 == s2, case insensitive
bool EqI(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (s1.len != s2.len) {
        return false;
    }
    return len(s1) == 0 || (s1.s && s2.s && 0 == _strnicmp(s1.s, s2.s, (size_t)s1.len));
}

// compares two strings ignoring case and whitespace
bool EqIS(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (len(s1) == 0 || len(s2) == 0) {
        return false;
    }

    int i1 = 0;
    int i2 = 0;
    while (i1 < s1.len && i2 < s2.len) {
        while (i1 < s1.len && IsWs(s1.s[i1])) {
            i1++;
        }
        while (i2 < s2.len && IsWs(s2.s[i2])) {
            i2++;
        }
        if (i1 >= s1.len || i2 >= s2.len) {
            break;
        }
        if (tolower(s1.s[i1]) != tolower(s2.s[i2])) {
            return false;
        }
        i1++;
        i2++;
    }
    while (i1 < s1.len && IsWs(s1.s[i1])) {
        i1++;
    }
    while (i2 < s2.len && IsWs(s2.s[i2])) {
        i2++;
    }
    return i1 >= s1.len && i2 >= s2.len;
}

bool EqN(Str s1, Str s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (len(s1) == 0 || len(s2) == 0 || n == 0) {
        return n == 0;
    }
    if (s1.len < n || s2.len < n) {
        return false;
    }
    return MemEq(s1.s, s2.s, n);
}

bool EqNI(Str s1, Str s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (len(s1) == 0 || len(s2) == 0 || n == 0) {
        return n == 0;
    }
    if (s1.len < n || s2.len < n) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (tolower(s1.s[i]) != tolower(s2.s[i])) {
            return false;
        }
    }
    return true;
}

bool StartsWith(Str s, Str prefix) {
    return EqN(s, prefix, len(prefix));
}

// Removes prefix from the string view, without modifying the underlying data.
int TrimPrefix(Str& s, Str prefix) {
    if (!s.s || len(s) == 0 || len(prefix) == 0 || !StartsWith(s, prefix)) {
        return 0;
    }
    s.s += prefix.len;
    s.len -= prefix.len;
    return prefix.len;
}

int TrimPrefixI(Str& s, Str prefix) {
    if (!s.s || len(s) == 0 || len(prefix) == 0 || !StartsWithI(s, prefix)) {
        return 0;
    }
    s.s += prefix.len;
    s.len -= prefix.len;
    return prefix.len;
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(Str s, Str prefix) {
    return EqNI(s, prefix, len(prefix));
}

bool StartsWithAny(Str s, const char* chars) {
    if (len(s) <= 0 || !s.s || !chars) {
        return false;
    }
    char c = s.s[0];
    while (*chars) {
        if (*chars == c) {
            return true;
        }
        chars++;
    }
    return false;
}

int TrimAny(Str& s, const char* chars) {
    if (len(s) <= 0 || !s.s || !chars) {
        return 0;
    }
    int origLen = len(s);
    while (len(s) > 0 && StartsWithAny(s, chars)) {
        s.s++;
        s.len--;
    }
    return origLen - len(s);
}

bool Contains(Str s, Str sub) {
    return str::IndexOf(s, sub) >= 0;
}

bool ContainsI(Str s, Str sub) {
    return str::IndexOfI(s, sub) >= 0;
}

bool EqNIx(Str s, int n, Str s2) {
    return len(s2) == n && str::StartsWithI(s, s2);
}

// case-insensitive variant of IndexOf: returns the byte offset of the first
// match of toFind in s, or -1 if not found
int IndexOfI(Str s, Str toFind) {
    if (len(s) == 0 || len(toFind) == 0) {
        return -1;
    }

    if (toFind.len <= 0) {
        return -1;
    }
    char first = (char)tolower(toFind.s[0]);
    if (!first) {
        return -1;
    }

    // Fast path: an ASCII needle can be matched byte-wise against a UTF-8
    // haystack (ASCII bytes never occur inside multi-byte UTF-8 sequences)
    // without any allocation. The Unicode path below is only needed to
    // case-fold a non-ASCII needle (e.g. Cyrillic), so that case-insensitive
    // search works for non-Latin text too (issue #5717).
    bool asciiNeedle = true;
    for (int i = 0; i < toFind.len; i++) {
        if ((u8)toFind.s[i] >= 0x80) {
            asciiNeedle = false;
            break;
        }
    }
    if (asciiNeedle) {
        for (int off = 0; off < s.len && s.s[off]; off++) {
            char c = (char)tolower(s.s[off]);
            if (c == first && str::StartsWithI(Str(s.s + off, s.len - off), toFind)) {
                return off;
            }
        }
        return -1;
    }

    // Unicode path: case-fold both strings (UTF-16) and search, then map the
    // match position back to a byte offset in the original UTF-8 string so the
    // returned offset keeps IndexOfI's contract (an offset into s).
    //
    // Scratch buffers come from the temporary arena; AutoArenaSavepoint restores
    // it to its entry position on return so repeated calls (e.g. the command
    // palette filtering every item) don't grow the arena unbounded.
    AutoArenaSavepoint scratch;

    TempWStr ws = ToWStrTemp(s); // unfolded, used to map the match back to bytes
    TempWStr wsLo = str::DupTemp(ws);
    TempWStr wfLo = ToWStrTemp(toFind);
    FoldCaseWInPlace(wsLo);
    FoldCaseWInPlace(wfLo);

    int res = -1;
    int idx = WStrFindSubstr(wsLo, wfLo); // common/str_util.cpp
    if (idx >= 0) {
        res = Utf8ByteOffsetForWCharOffset(s, idx);
    }
    return res;
}

void ReplacePtr(Str* s, Str snew) {
    if (s->s != snew.s) {
        str::Free(*s);
        *s = snew;
    }
}

void ReplaceWithCopy(Str* s, Str snew) {
    // dup before free so it's safe even if snew aliases *s; dup is always a
    // fresh allocation so it can never alias the old s->s -- no check needed
    Str dup = str::Dup(snew);
    str::Free(*s);
    *s = dup;
}

Str Join(Arena* a, Str s1, Str s2, Str s3, Str s4, Str s5) {
    int s1Len = len(s1);
    int s2Len = len(s2);
    int s3Len = len(s3);
    int s4Len = len(s4);
    int s5Len = len(s5);
    int n = s1Len + s2Len + s3Len + s4Len + s5Len + 1;
    char* res = (char*)Alloc(a, n);

    char* s = res;
    memcpy(s, s1.s, s1Len);
    s += s1Len;
    memcpy(s, s2.s, s2Len);
    s += s2Len;
    memcpy(s, s3.s, s3Len);
    s += s3Len;
    memcpy(s, s4.s, s4Len);
    s += s4Len;
    memcpy(s, s5.s, s5Len);
    s += s5Len;
    *s = 0;

    return Str(res, n - 1);
}

Str Join(Arena* a, Str s1, Str s2, Str s3) {
    return Join(a, s1, s2, s3, Str{}, Str{});
}

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
Str Join(Str s1, Str s2, Str s3) {
    return Join(nullptr, s1, s2, s3);
}

// index of last occurrence of c in s, or -1
int LastIndexOfChar(Str s, char c) {
    for (int i = s.len - 1; i >= 0; i--) {
        if (s.s[i] == c) {
            return i;
        }
    }
    return -1;
}

// Trims trailing whitespace, writes a NUL at the new end and returns the count.
int TrimSuffixWhitespace(Str& s) {
    int origLen = len(s);
    while (s.len > 0) {
        char c = s.s[s.len - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        s.len--;
        s.s[s.len] = 0;
    }
    return origLen - len(s);
}

} // namespace str
namespace wstr {

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
WStr Join(Arena* a, WStr s1, WStr s2, WStr s3) {
    int s1Len = s1.len, s2Len = s2.len, s3Len = s3.len;
    int n = s1Len + s2Len + s3Len + 1;
    WCHAR* res = (WCHAR*)Alloc(a, n * sizeofi(WCHAR));
    memcpy(res, s1.s, (size_t)s1Len * sizeof(WCHAR));
    memcpy(res + s1Len, s2.s, (size_t)s2Len * sizeof(WCHAR));
    memcpy(res + s1Len + s2Len, s3.s, (size_t)s3Len * sizeof(WCHAR));
    res[s1Len + s2Len + s3Len] = '\0';
    return WStr(res);
}

WStr Join(WStr s1, WStr s2, WStr s3) {
    return Join(nullptr, s1, s2, s3);
}

} // namespace wstr
namespace str {

Str ToLowerInPlace(Str s) {
    for (int i = 0; i < s.len; i++) {
        s.s[i] = (char)tolower((u8)s.s[i]);
    }
    return s;
}

// Note: I tried an optimization: return (unsigned)(c - '0') < 10;
// but it seems to mis-compile in release builds
bool IsDigit(char c) {
    return ('0' <= c) && (c <= '9');
}

bool IsWs(char c) {
    if (' ' == c) {
        return true;
    }
    if (('\t' <= c) && (c <= '\r')) {
        return true;
    }
    return false;
}

// true if s contains any one of the chars (each char of `chars` is a candidate,
// not a substring to find)
bool ContainsCharAny(Str s, Str chars) {
    for (int i = 0; i < s.len; i++) {
        if (IndexOfChar(chars, s.s[i]) >= 0) {
            return true;
        }
    }
    return false;
}

Str SliceFromCharLast(Str str, char c) {
    for (int i = str.len - 1; i >= 0; i--) {
        if (str.s[i] == c) {
            return Str(str.s + i, str.len - i);
        }
    }
    return {};
}

int IndexOf(Str buf, Str toFind) {
    if (len(buf) == 0 || len(toFind) == 0) {
        return -1;
    }
    int toFindLen = toFind.len;
    if (toFindLen <= 0 || buf.len < toFindLen) {
        return -1;
    }
    char c = toFind.s[0];
    int end = buf.len - toFindLen;
    for (int i = 0; i <= end; i++) {
        if (buf.s[i] == c && MemEq(buf.s + i, toFind.s, toFindLen)) {
            return i;
        }
    }
    return -1;
}

// offset just past the first occurrence of needle in s, or -1 if not found
int IndexOfAfter(Str s, Str needle) {
    int idx = IndexOf(s, needle);
    if (idx < 0) {
        return -1;
    }
    return idx + needle.len;
}

// Splits s around the first occurrence of sep (Go's strings.Cut). When sep is
// found, *before is the text before it and *after the text after it; returns
// true. When sep is not found, *before is all of s, *after is {} and it returns
// false. before/after may be null if not needed.
// splits s into the part before the separator (found at idx, sepLen chars long)
// and the part after it. idx < 0 means "not found": before = s, after = {}.
static bool CutAtIdx(Str s, int idx, int sepLen, Str* before, Str* after) {
    if (idx < 0) {
        if (before) {
            *before = s;
        }
        if (after) {
            *after = {};
        }
        return false;
    }
    if (before) {
        *before = Str(s.s, idx);
    }
    if (after) {
        int off = idx + sepLen;
        *after = Str(s.s + off, s.len - off);
    }
    return true;
}

bool Cut(Str s, Str sep, Str* before, Str* after) {
    return CutAtIdx(s, IndexOf(s, sep), sep.len, before, after);
}

// like Cut() but splits on the first occurrence of a single char
bool CutChar(Str s, char c, Str* before, Str* after) {
    return Cut(s, Str(&c, 1), before, after);
}

// like CutChar() but splits on the last occurrence of a single char
bool CutCharLast(Str s, char c, Str* before, Str* after) {
    return CutAtIdx(s, LastIndexOfChar(s, c), 1, before, after);
}

// Extracts the next line from s (up to a CR, LF or CRLF terminator) into line
// and sets rest to the remainder after the terminator. line excludes the
// terminator. Returns false when s is empty. Safe to alias s and rest, e.g.
// while (str::NextLine(rest, line, rest)) { ... }
bool NextLine(Str s, Str& line, Str& rest) {
    if (len(s) == 0) {
        return false;
    }
    int idx = -1;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        if (c == '\n' || c == '\r') {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        line = s;
        rest = {};
        return true;
    }
    line = Str(s.s, idx);
    int off = idx + 1;
    // treat CRLF as a single line terminator
    if (s.s[idx] == '\r' && off < s.len && s.s[off] == '\n') {
        off++;
    }
    rest = Str(s.s + off, s.len - off);
    return true;
}

// Trim whitespace characters, in-place, inside s.
// Updates s.len. Returns number of trimmed characters.
int TrimWSInPlace(Str& s, TrimOpt opt) {
    if (str::IsNull(s)) {
        return 0;
    }
    int start = 0;
    int end = s.len;
    if ((TrimOpt::Left == opt) || (TrimOpt::Both == opt)) {
        while (start < end && IsWs(s.s[start])) {
            start++;
        }
    }

    if ((TrimOpt::Right == opt) || (TrimOpt::Both == opt)) {
        while (end > start && IsWs(s.s[end - 1])) {
            end--;
        }
    }
    if (end < s.len) {
        s.s[end] = 0;
    }
    int trimmed = start + (s.len - end);
    if (start != 0) {
        memmove(s.s, s.s + start, (size_t)(end - start) + 1);
    }
    s.len = end - start;
    return trimmed;
}

// like NormalizeWSInPlace but non-mutating: returns s with whitespace runs
// collapsed to single spaces and leading/trailing whitespace removed. Allocates
// a temp copy only when normalization would change something; otherwise returns
// s unchanged (no allocation).
TempStr NormalizeWSTemp(Str s) {
    int n = s.len;
    if (n == 0) {
        return s;
    }
    // decide whether normalizing changes anything, so we can skip allocating
    bool changed = IsWs(s.s[0]) || IsWs(s.s[n - 1]);
    for (int i = 0; !changed && i < n; i++) {
        char c = s.s[i];
        if (IsWs(c)) {
            // a non-space whitespace char becomes ' ', or a run collapses to one
            changed = (c != ' ') || (i + 1 < n && IsWs(s.s[i + 1]));
        }
    }
    if (!changed) {
        return s;
    }
    TempStr res = DupTemp(s);
    res.len -= NormalizeWSInPlace(res);
    return res;
}

constexpr char kCR = '\r';
constexpr char kLF = '\n';

// kCR kLF and a lone kCR become kLF, in place: the result is never longer.
// Empty lines are preserved.
// s must own a writeable, nul-terminated buffer.
int NormalizeNewlinesToLFInPlace(Str& s) {
    if (len(s) == 0) {
        return 0;
    }

    int dst = 0;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        if (c == kCR) {
            // kCR followed by kLF is a single newline
            if (i + 1 < s.len && s.s[i + 1] == kLF) {
                i++;
            }
            c = kLF;
        }
        s.s[dst++] = c;
    }
    s.s[dst] = 0;
    s.len = dst;

    return dst;
}

// Every kLF not already preceded by a kCR becomes kCR kLF (what win32 edit
// controls expect). Returns s unchanged (no allocation) if there's nothing to do.
TempStr LFToCRLFTemp(Str s) {
    int n = s.len;
    int nLF = 0;
    for (int i = 0; i < n; i++) {
        if (s.s[i] == kLF && (i == 0 || s.s[i - 1] != kCR)) {
            nLF++;
        }
    }
    if (nLF == 0) {
        return s;
    }
    char* res = AllocArrayTemp<char>(n + nLF + 1);
    if (!res) {
        return {};
    }
    int dst = 0;
    for (int i = 0; i < n; i++) {
        char c = s.s[i];
        if (c == kLF && (i == 0 || s.s[i - 1] != kCR)) {
            res[dst++] = kCR;
        }
        res[dst++] = c;
    }
    res[dst] = 0;
    return Str(res, dst);
}

// Remove all characters in "toRemove" from "str", in place.
// Returns number of removed characters.

/* Convert binary data in <buf> to a hex-encoded string */
TempStr MemToHexTemp(Str buf) {
    int n = buf.len;
    /* 2 hex chars per byte, +1 for terminating 0 */
    char* ret = AllocArrayTemp<char>((2 * n) + 1);
    if (!ret) {
        return {};
    }
    static const char hex[] = "0123456789abcdef";
    int dst = 0;
    for (int i = 0; i < n; i++) {
        u8 b = (u8)buf.s[i];
        ret[dst++] = hex[b >> 4];
        ret[dst++] = hex[b & 0x0f];
    }
    ret[dst] = 0;
    return Str(ret, dst);
}

/* Reverse of MemToHexTemp. Convert a 0-terminatd hex-encoded string <s> to
   binary data pointed by <buf> of max size bufLen.
   Returns false if size of <s> doesn't match bufLen or is not a valid
   hex string. */
static int HexDigitVal(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool HexToMem(Str s, Str buf) {
    int bufLen = buf.len;
    int needed = bufLen * 2;
    if (s.len < needed) {
        return false;
    }
    for (int i = 0; i < bufLen; i++) {
        int off = i * 2;
        int hi = HexDigitVal(s.s[off]);
        int lo = HexDigitVal(s.s[off + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        buf.s[i] = (char)((hi << 4) | lo);
    }
    return s.len == needed || (s.len > needed && s.s[needed] == '\0');
}

bool IsAlNum(char c) {
    if (c >= '0' && c <= '9') {
        return true;
    }
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        return true;
    }
    return false;
}

/* compares two strings "naturally" by sorting numbers within a string
   numerically instead of by pure ASCII order; we imitate Windows Explorer
   by sorting special characters before alphanumeric characters
   (e.g. ".hg" < "2.pdf" < "100.pdf" < "zzz")
   // TODO: this should be utf8-aware, see e.g. cbx\bug1234-*.cbr file
*/
static bool CmpNaturalAtEnd(Str s, int i) {
    return i >= s.len || s.s[i] == '\0';
}

static char CmpNaturalAt(Str s, int i) {
    if (CmpNaturalAtEnd(s, i)) {
        return '\0';
    }
    return s.s[i];
}

int CmpNatural(Str aIn, Str bIn) {
    ReportIf(len(aIn) == 0 || len(bIn) == 0);
    int ai = 0;
    int bi = 0;
    int diff = 0;

    while (diff == 0) {
        // ignore leading and trailing spaces, and differences in whitespace only
        if (ai == 0 || bi == 0 || CmpNaturalAtEnd(aIn, ai) || CmpNaturalAtEnd(bIn, bi) ||
            (IsWs(aIn.s[ai]) && IsWs(bIn.s[bi]))) {
            while (!CmpNaturalAtEnd(aIn, ai) && IsWs(aIn.s[ai])) {
                ai++;
            }
            while (!CmpNaturalAtEnd(bIn, bi) && IsWs(bIn.s[bi])) {
                bi++;
            }
        }
        // if two strings are identical when ignoring case, leading zeroes and
        // whitespace, compare them traditionally for a stable sort order
        if (CmpNaturalAtEnd(aIn, ai) && CmpNaturalAtEnd(bIn, bi)) {
            return str::Cmp(aIn, bIn);
        }

        char ca = CmpNaturalAt(aIn, ai);
        char cb = CmpNaturalAt(bIn, bi);

        if (str::IsDigit(ca) && str::IsDigit(cb)) {
            // ignore leading zeroes
            while (!CmpNaturalAtEnd(aIn, ai) && aIn.s[ai] == '0') {
                ai++;
            }
            while (!CmpNaturalAtEnd(bIn, bi) && bIn.s[bi] == '0') {
                bi++;
            }
            // compare the two numbers as (positive) integers
            for (diff = 0; str::IsDigit(CmpNaturalAt(aIn, ai)) || str::IsDigit(CmpNaturalAt(bIn, bi)); ai++, bi++) {
                // if either isn't a number, they differ in magnitude
                if (!str::IsDigit(CmpNaturalAt(aIn, ai))) {
                    return -1;
                }
                if (!str::IsDigit(CmpNaturalAt(bIn, bi))) {
                    return 1;
                }
                // remember the difference for when the numbers are of the same magnitude
                if (0 == diff) {
                    diff = (unsigned char)aIn.s[ai] - (unsigned char)bIn.s[bi];
                }
            }
            // neither is a digit, so continue with them (unless diff != 0)
            ai--;
            bi--;
        } else if (str::IsAlNum(ca) && str::IsAlNum(cb)) {
            // sort letters case-insensitively
            diff = tolower((u8)ca) - tolower((u8)cb);
        } else if (str::IsAlNum(ca)) {
            // sort special characters before text and numbers
            return 1;
        } else if (str::IsAlNum(cb)) {
            return -1;
        } else {
            // sort special characters by ASCII code
            diff = (unsigned char)ca - (unsigned char)cb;
        }
        ai++;
        bi++;
    }

    return diff;
}

bool IsEmptyOrWhiteSpace(Str s) {
    for (int i = 0; i < s.len; i++) {
        if (!str::IsWs(s.s[i])) {
            return false;
        }
    }
    return true;
}

// advances s past any leading toSkip chars; returns how many were trimmed
int TrimChar(Str& s, char toSkip) {
    int i = 0;
    while (i < s.len && s.s[i] == toSkip) {
        i++;
    }
    s.s += i;
    s.len -= i;
    return i;
}

// advances s past its leading whitespace; returns how many chars were trimmed
int TrimWs(Str& s) {
    int i = 0;
    while (i < s.len && IsWs(s.s[i])) {
        i++;
    }
    s.s += i;
    s.len -= i;
    return i;
}

// advances s past its leading non-whitespace; returns how many chars were trimmed
int TrimNonWs(Str& s) {
    int i = 0;
    while (i < s.len && !IsWs(s.s[i])) {
        i++;
    }
    s.s += i;
    s.len -= i;
    return i;
}

// eats leading whitespace and the word (run of non-whitespace) after it,
// returning the word. Returns {} when only whitespace is left, so:
//   while (Str word = str::NextWord(s)) { ... }
// walks the words of s.
Str NextWord(Str& s) {
    TrimWs(s);
    Str word = s;
    word.len = TrimNonWs(s);
    if (len(word) == 0) {
        return {};
    }
    return word;
}

// Narrows the view past leading and trailing whitespace without modifying data.
int TrimWsBoth(Str& s) {
    int origLen = len(s);
    TrimWs(s);
    while (s.len > 0 && IsWs(s.s[s.len - 1])) {
        s.len--;
    }
    return origLen - len(s);
}

} // namespace str

namespace url {

// Percent-decodes url into the temp arena ("%20" -> ' ', "%C3%A4" -> the two
// UTF-8 bytes of 'ä'); an escape that isn't two hex digits is left as is.
// Returns a new (NUL-terminated) string rather than decoding in place because
// decoding shrinks the string: the in-place version this replaces could only
// shorten its caller's buffer, and a caller left holding the encoded length
// carried the bytes past the NUL along (a markdown file named "a ä.md" looked
// up "a ä.md\0.md" and was reported as missing; #5926).
TempStr DecodeTemp(Str url) {
    if (str::IsNull(url)) {
        return {};
    }
    TempStr res = str::DupTemp(url);
    int n = res.len;
    int dst = 0;
    for (int src = 0; src < n; src++) {
        int val;
        if (res.s[src] == '%' && src + 2 < n && !str::IsNull(str::Parse(Str(res.s + src, n - src), "%%%2x", &val))) {
            res.s[dst++] = (char)val;
            src += 2;
        } else {
            res.s[dst++] = res.s[src];
        }
    }
    res.s[dst] = '\0';
    res.len = dst;
    return res;
}

// RFC 3986 unreserved: ALPHA / DIGIT / "-" / "." / "_" / "~". Everything else
// (including URL delimiters ? # & = / and quotes) is %HH so the result is
// safe as a query value, not parsed as more URL syntax (discussion #6029).
static bool UrlUnreserved(u8 c) {
    bool alpha = (c | 0x20) >= 'a' && (c | 0x20) <= 'z';
    return alpha || str::IsDigit((char)c) || c == '-' || c == '.' || c == '_' || c == '~';
}

static int UrlEncodedByteLen(u8 c) {
    return UrlUnreserved(c) ? 1 : 3;
}

static void UrlAppendEncodedByte(char* dst, int& n, u8 c) {
    static const char kHex[] = "0123456789ABCDEF";
    if (UrlUnreserved(c)) {
        dst[n++] = (char)c;
        return;
    }
    dst[n++] = '%';
    dst[n++] = kHex[c >> 4];
    dst[n++] = kHex[c & 0xF];
}

// keepSlash: '/' stays a path separator, so a relative path with spaces or
// non-ASCII ("dir/Test Test.md") is a valid URI path
static TempStr EncodeT(Str s, bool keepSlash) {
    if (str::IsNull(s)) {
        return {};
    }
    int n = len(s);
    char* buf = AllocArrayTemp<char>((n * 3) + 1);
    int dst = 0;
    for (int i = 0; i < n; i++) {
        u8 c = (u8)s.s[i];
        if (keepSlash && c == '/') {
            buf[dst++] = '/';
        } else {
            UrlAppendEncodedByte(buf, dst, c);
        }
    }
    buf[dst] = '\0';
    return Str(buf, dst);
}

TempStr EncodeTemp(Str s) {
    return EncodeT(s, false);
}

// Like EncodeTemp but '/' stays a path separator, so a relative path with
// spaces or non-ASCII ("dir/Test Test.md") is a valid URI path.
TempStr EncodePathTemp(Str path) {
    return EncodeT(path, true);
}

// Encoded length depends on the bytes, not the rune count: ASCII stays 1, a
// space or '?' becomes 3, a CJK rune is 3 UTF-8 bytes so 9 encoded chars.
// Cut on a UTF-8 character boundary so we never emit a partial %HH sequence
// or a broken multi-byte character.
TempStr EncodeMayTruncateTemp(Str s, int maxEncodedLen, bool* didTruncateOut) {
    if (didTruncateOut) {
        *didTruncateOut = false;
    }
    if (str::IsNull(s)) {
        return {};
    }
    if (maxEncodedLen <= 0) {
        return EncodeTemp(s);
    }
    char* buf = AllocArrayTemp<char>(maxEncodedLen + 1);
    int dst = 0;
    int i = 0;
    bool truncated = false;
    while (i < len(s)) {
        int start = i;
        Utf8CodepointNext(s, i);
        if (i <= start) {
            i = start + 1;
        }
        int add = 0;
        for (int j = start; j < i; j++) {
            add += UrlEncodedByteLen((u8)s.s[j]);
        }
        if (dst + add > maxEncodedLen) {
            truncated = true;
            break;
        }
        for (int j = start; j < i; j++) {
            UrlAppendEncodedByte(buf, dst, (u8)s.s[j]);
        }
    }
    buf[dst] = '\0';
    if (didTruncateOut) {
        *didTruncateOut = truncated;
    }
    return Str(buf, dst);
}
} // namespace url

// SeqStrings is for size-efficient implementation of:
// string -> int and int->string.
// it's even more efficient than using char *[] array
// it comes at the cost of speed, so it's not good for places
// that are critial for performance. On the other hand, it's
// not that bad: linear scanning of memory is fast due to the magic
// of L1 cache
Str SeqStrFirst(SeqStrings strs) {
    if (!strs || !strs[0]) {
        return {};
    }
    return Str(strs);
}

Str SeqStrNext(Str s) {
    if (len(s) == 0) {
        return {};
    }
    const char* next = s.s + len(s) + 1;
    return next[0] ? Str(next) : Str{};
}

// conceptually strings is an array of 0-terminated strings where, laid
// out sequentially in memory, terminated with a 0-length string
// Returns index of toFind string in strings
// Returns -1 if string doesn't exist
int SeqStrIndex(SeqStrings strs, Str toFind) {
    if (!strs || len(toFind) == 0) {
        return -1;
    }

    const char* candidate = strs;
    int toFindLen = len(toFind);
    int idx = 0;
    while (*candidate) {
        int i = 0;
        while (i < toFindLen && candidate[i] && candidate[i] == toFind.s[i]) {
            i++;
        }
        if (i == toFindLen && !candidate[i]) {
            return idx;
        }

        candidate += i;
        while (*candidate) {
            candidate++;
        }
        candidate++;
        idx++;
    }
    return -1;
}

// like SeqStrIndex but ignores case and whitespace
// case-insensitive SeqStrIndex
int SeqStrIndexI(SeqStrings strs, Str toFind) {
    int idx = 0;
    for (Str s = SeqStrFirst(strs); len(s) > 0; s = SeqStrNext(s), idx++) {
        if (str::EqI(s, toFind)) {
            return idx;
        }
    }
    return -1;
}

int SeqStrIndexIS(SeqStrings strs, Str toFind) {
    if (!strs || len(toFind) == 0) {
        return -1;
    }

    const char* candidate = strs;
    int toFindLen = len(toFind);
    int idx = 0;
    while (*candidate) {
        int i = 0;
        int j = 0;
        while (candidate[i] && j < toFindLen) {
            while (candidate[i] && str::IsWs(candidate[i])) {
                i++;
            }
            while (j < toFindLen && str::IsWs(toFind.s[j])) {
                j++;
            }
            if (!candidate[i] || j >= toFindLen) {
                break;
            }
            if (tolower((u8)candidate[i]) != tolower((u8)toFind.s[j])) {
                break;
            }
            i++;
            j++;
        }
        while (candidate[i] && str::IsWs(candidate[i])) {
            i++;
        }
        while (j < toFindLen && str::IsWs(toFind.s[j])) {
            j++;
        }
        if (!candidate[i] && j == toFindLen) {
            return idx;
        }

        candidate += i;
        while (*candidate) {
            candidate++;
        }
        candidate++;
        idx++;
    }
    return -1;
}

// Given an index in the "array" of sequentially laid out strings,
// returns a strings at that index.
TempStr SeqStrByIndex(SeqStrings strs, int idx) {
    ReportIf(idx < 0);
    Str s = SeqStrFirst(strs);
    while (idx > 0 && len(s) > 0) {
        s = SeqStrNext(s);
        idx--;
    }
    return s;
}

// flat sequence of (extension, mime type) pairs
static SeqStrings gMimeTypes =
    ".html\0text/html\0"
    ".htm\0text/html\0"
    ".gif\0image/gif\0"
    ".png\0image/png\0"
    ".jpg\0image/jpeg\0"
    ".jpeg\0image/jpeg\0"
    ".bmp\0image/bmp\0"
    ".ico\0image/vnd.microsoft.icon\0"
    ".css\0text/css\0"
    ".js\0text/javascript\0"
    ".svg\0image/svg+xml\0"
    ".txt\0text/plain\0"
    ".md\0text/plain\0"
    ".json\0application/json\0";

// ext is like ".png"; returns e.g. "image/png", or {} if the extension is not a
// known type. If the matched type is an image and imgExt (the real extension
// detected from the file's data) is given, imgExt's type wins over the ext's.
TempStr MimeTypeFromExtTemp(Str ext, Str imgExt) {
    int idx = SeqStrIndexIS(gMimeTypes, ext);
    if (idx < 0) {
        return {};
    }
    Str mime = SeqStrByIndex(gMimeTypes, idx + 1);
    // trust an image's actual data over its extension
    if (imgExt && str::StartsWith(mime, StrL("image/"))) {
        int j = SeqStrIndex(gMimeTypes, imgExt);
        if (j >= 0) {
            return SeqStrByIndex(gMimeTypes, j + 1);
        }
    }
    return mime;
}

// unsigned LEB128 of zigzag-encoded i64
static int VarIntEncode(u8* dst, i64 val) {
    u64 n = ((u64)val << 1) ^ (u64)(val >> 63);
    int i = 0;
    for (;;) {
        u8 b = (u8)(n & 0x7f);
        n >>= 7;
        if (n) {
            b |= 0x80;
        }
        dst[i++] = b;
        if (!n) {
            return i;
        }
    }
}

static bool VarIntDecode(const u8*& p, i64* out) {
    u64 n = 0;
    int shift = 0;
    for (;;) {
        u8 b = *p++;
        n |= (u64)(b & 0x7f) << shift;
        if (!(b & 0x80)) {
            *out = (i64)((n >> 1) ^ (~(n & 1) + 1));
            return true;
        }
        shift += 7;
        if (shift >= 64) {
            return false;
        }
    }
}

static int SeqStrNumEntryEndOff(SeqStrNum strs, int off) {
    if (!strs || off < 0 || !strs[off]) {
        return off;
    }
    int next = off + len(strs + off) + 1;
    const u8* p = (const u8*)(strs + next);
    while (*p & 0x80) {
        p++;
    }
    return next + (int)(p - (const u8*)(strs + next)) + 1;
}

static void SeqStrNumEntryParts(SeqStrNum strs, int off, Str* strOut, i64* numOut) {
    if (strOut) {
        *strOut = SeqStrNumAt(strs, off);
    }
    const u8* p = (const u8*)(strs + off + len(strs + off) + 1);
    if (numOut) {
        VarIntDecode(p, numOut);
    }
}

void SeqStrNumAppend(str::Builder* b, Str s, i64 num) {
    b->Append(s);
    b->AppendChar('\0');
    u8 buf[12];
    int n = VarIntEncode(buf, num);
    b->Append(Str((char*)buf, n));
}

void SeqStrNumFinish(str::Builder* b) {
    b->AppendChar('\0');
}

TempStr SeqStrNumAt(SeqStrNum strs, int off) {
    if (!strs || off < 0 || !strs[off]) {
        return {};
    }
    return Str(strs + off);
}

bool SeqStrNumAdvance(SeqStrNum strs, int& off, int* idxInOut) {
    if (!strs || off < 0 || !strs[off]) {
        off = -1;
        if (idxInOut) {
            *idxInOut = -1;
        }
        return false;
    }
    off = SeqStrNumEntryEndOff(strs, off);
    if (!strs[off]) {
        off = -1;
        return false;
    }
    if (idxInOut) {
        (*idxInOut)++;
    }
    return true;
}

static int SeqStrNumIndexBy(SeqStrNum strs, Str toFind, i64* numOut, bool (*eq)(Str, Str)) {
    if (len(toFind) == 0) {
        return -1;
    }
    int off = 0;
    for (int idx = 0; strs && strs[off]; idx++) {
        if (eq(SeqStrNumAt(strs, off), toFind)) {
            if (numOut) {
                SeqStrNumEntryParts(strs, off, nullptr, numOut);
            }
            return idx;
        }
        if (!SeqStrNumAdvance(strs, off)) {
            break;
        }
    }
    return -1;
}

int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut) {
    return SeqStrNumIndexBy(strs, toFind, numOut, str::Eq);
}

int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut) {
    return SeqStrNumIndexBy(strs, toFind, numOut, str::EqIS);
}

TempStr SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut) {
    ReportIf(idx < 0);
    int off = 0;
    while (idx > 0) {
        if (!SeqStrNumAdvance(strs, off)) {
            return {};
        }
        idx--;
    }
    if (!strs || !strs[off]) {
        return {};
    }
    if (numOut) {
        SeqStrNumEntryParts(strs, off, nullptr, numOut);
    }
    return SeqStrNumAt(strs, off);
}

TempStr SeqStrNumStrByNumber(SeqStrNum strs, i64 num) {
    int off = 0;
    while (strs && strs[off]) {
        i64 n = 0;
        Str s;
        SeqStrNumEntryParts(strs, off, &s, &n);
        if (n == num) {
            return s;
        }
        if (!SeqStrNumAdvance(strs, off)) {
            break;
        }
    }
    return {};
}

// for compatibility with C string, the last character is always 0
// kPadding is number of characters needed for terminating character
static constexpr int kPadding = 1;

// storage that isn't a heap block of ours: a lent buffer, an arena block, or
// nothing at all
template <typename C>
static bool IsNotOurHeapBlock(const BuilderT<C>& b) {
    return !b.els || b.cap < 0;
}

// Vec allocates one element past the capacity and zeroes what it isn't using,
// so there is always room for the NUL. Writing it after every change keeps it
// right for lent buffers too, which nobody zeroes.
template <typename C>
static void Terminate(BuilderT<C>& b) {
    if (b.els) {
        b.els[b.len] = 0;
    }
}

// VecReserve() marks arena storage with a positive cap, which would have ~Vec()
// free() arena memory. Flip the sign, so it reads as "not ours", like a lent
// buffer does.
template <typename C>
static C* BuilderEnsureCap(BuilderT<C>& b, int needed) {
    C* els = VecReserve(b.a, b, needed);
    if (!els) {
        return nullptr;
    }
    if (b.a && b.cap > 0) {
        b.cap = -b.cap;
    }
    return els;
}

template <typename C>
void BuilderT<C>::Reset(S s) {
    // keeps the storage (heap or borrowed) for re-use, only empties it
    this->len = 0;
    Terminate(*this);
    Append(s); // no-op if s is empty
}

template <typename C>
void BuilderT<C>::UseExternalBuffer(S buf) {
    ReportIf(this->els || this->len != 0);
    if (buf.s && buf.len > kPadding) {
        this->els = buf.s;
        // one char of the caller's buffer is held back for the NUL
        this->cap = -(buf.len - kPadding);
        this->els[0] = 0;
    }
}

template <typename C>
bool BuilderT<C>::Reserve(int cap) {
    if (!BuilderEnsureCap(*this, cap)) {
        return false;
    }
    Terminate(*this);
    return true;
}

template <typename C>
bool BuilderT<C>::AppendChar(C c) {
    if (!BuilderEnsureCap(*this, this->len + 1)) {
        return false;
    }
    this->els[this->len++] = c;
    Terminate(*this);
    return true;
}

template <typename C>
bool BuilderT<C>::Append(S src) {
    if (!src.s || 0 == src.len) {
        return true;
    }
    if (!BuilderEnsureCap(*this, this->len + src.len)) {
        return false;
    }
    memcpy(this->els + this->len, src.s, (size_t)src.len * sizeof(C));
    this->len += src.len;
    Terminate(*this);
    return true;
}

template <typename C>
bool BuilderT<C>::AppendNonEmpty(S src) {
    if (::len(src) == 0) {
        return true;
    }
    return Append(src);
}

template <typename C>
C BuilderT<C>::RemoveAt(int idx, int count) {
    C res = this->els[idx];
    // VecRemoveAtN() zeroes the chars it frees at the end, so the NUL is there
    VecRemoveAtN(*this, idx, count);
    return res;
}

template <typename C>
C BuilderT<C>::RemoveLast() {
    if (this->len == 0) {
        return 0;
    }
    return RemoveAt(this->len - 1);
}

// perf hack for using as a buffer: client can get accumulated data
// without duplicate allocation. Note: since Vec over-allocates, this
// is likely to use more memory than strictly necessary, but in most cases
// it doesn't matter. A lent buffer or arena block is copied out instead,
// since the caller gets to free the result.
template <typename C>
typename BuilderT<C>::S BuilderT<C>::TakeStr() {
    int n = this->len;
    C* res = this->els;
    if (!res || n == 0) {
        Reset();
        return S{};
    }
    if (IsNotOurHeapBlock(*this)) {
        res = (C*)MemDup(a, res, (size_t)(n + kPadding) * sizeof(C));
    } else {
        // hand the block (heap or arena) to the caller and start over
        this->els = nullptr;
        this->cap = 0;
    }
    Reset();
    return S(res, n);
}

template <typename C>
C BuilderT<C>::LastChar() const {
    if (this->len == 0) {
        return 0;
    }
    return this->els[this->len - 1];
}

template struct BuilderT<char>;
template struct BuilderT<WCHAR>;

bool str::Contains(const str::Builder& b, Str sub) {
    return str::Contains(ToStr(b), sub);
}

namespace wstr {

bool IsWs(WCHAR c) {
    return iswspace(c);
}

bool IsDigit(WCHAR c) {
    return ('0' <= c) && (c <= '9');
}

bool IsNonCharacter(WCHAR c) {
    return c >= 0xFFFE || (c & ~1) == 0xDFFE || (0xFDD0 <= c && c <= 0xFDEF);
}

} // namespace wstr
namespace str {

// Reinterpret a UTF-16 byte buffer held in a Str as a WStr without a
// char*→WCHAR* cast (CodeQL cpp/incorrect-string-type-conversion).
WStr CastStrToWStr(Str s) {
    if (len(s) == 0) {
        return {};
    }
    WCHAR* w = nullptr;
    static_assert(sizeof(char*) == sizeof(WCHAR*), "pointer sizes must match");
    memcpy((void*)&w, (const void*)&s.s, sizeof(w));
    return WStr(w, s.len / sizeofi(WCHAR));
}

} // namespace str
namespace wstr {

// return true if s1 == s2, case sensitive
bool Eq(WStr s1, WStr s2) {
    if (s1.len != s2.len) {
        return false;
    }
    for (int i = 0; i < s1.len; i++) {
        if (s1.s[i] != s2.s[i]) {
            return false;
        }
    }
    return true;
}

bool EqNI(WStr s1, WStr s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (len(s1) == 0 || len(s2) == 0) {
        return n == 0;
    }
    if (n == 0) {
        return true;
    }
    if (s1.len < n || s2.len < n) {
        return false;
    }
    WCHAR* a = AllocArrayTemp<WCHAR>(n);
    WCHAR* b = AllocArrayTemp<WCHAR>(n);
    if (!a || !b) {
        return false;
    }
    memcpy(a, s1.s, (size_t)n * sizeof(WCHAR));
    memcpy(b, s2.s, (size_t)n * sizeof(WCHAR));
    WStr wa(a, n);
    WStr wb(b, n);
    FoldCaseWInPlace(wa);
    FoldCaseWInPlace(wb);
    return EqN(wa, wb, n);
}

// return true if s1 == s2, case insensitive
bool EqI(WStr s1, WStr s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (s1.len != s2.len) {
        return false;
    }
    return len(s1) == 0 || (s1.s && s2.s && EqNI(s1, s2, s1.len));
}

bool EqN(WStr s1, WStr s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (len(s1) == 0 || len(s2) == 0) {
        return false;
    }
    return 0 == wcsncmp(s1.s, s2.s, (size_t)n);
}

bool StartsWith(WStr str, WStr prefix) {
    if (len(prefix) == 0) {
        return true;
    }
    if (len(str) == 0 || prefix.len > str.len) {
        return false;
    }
    return EqN(str, prefix, prefix.len);
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(WStr str, WStr prefix) {
    if (str.s == prefix.s) {
        return true;
    }
    if (len(prefix) == 0) {
        return true;
    }
    if (len(str) == 0 || prefix.len > str.len) {
        return false;
    }
    return EqNI(str, prefix, prefix.len);
}

WStr FindFrom(WStr str, WStr find) {
    if (len(str) == 0 || len(find) == 0 || find.len > str.len) {
        return {};
    }
    for (int i = 0; i <= str.len - find.len; i++) {
        if (0 == wcsncmp(str.s + i, find.s, (size_t)find.len)) {
            return WStr(str.s + i, str.len - i);
        }
    }
    return {};
}

} // namespace wstr
namespace str {

Str ToUpperInPlace(Str s) {
    for (int i = 0; i < s.len; i++) {
        s.s[i] = (char)toupper((u8)s.s[i]);
    }
    return s;
}

} // namespace str
namespace wstr {

WStr ToLowerInPlace(WStr s) {
    for (int i = 0; i < s.len; i++) {
        s.s[i] = towlower(s.s[i]);
    }
    return s;
}

// free() the result via str::Free(s) or str::FreePtr(&s)
WStr Replace(WStr s, WStr toReplace, WStr replaceWith) {
    if (len(s) == 0 || len(toReplace) == 0 || len(replaceWith) == 0) {
        return {};
    }

    wstr::Builder result;
    result.Reserve(s.len);
    int findLen = toReplace.len;
    int start = 0;
    while (start < s.len) {
        WStr rest(s.s + start, s.len - start);
        WStr match = wstr::FindFrom(rest, toReplace);
        if (len(match) == 0) {
            result.Append(WStr(s.s + start, s.len - start));
            break;
        }
        int matchOff = (int)(match.s - s.s);
        result.Append(WStr(s.s + start, matchOff - start));
        result.Append(replaceWith);
        start = matchOff + findLen;
    }
    return result.TakeStr();
}

} // namespace wstr
namespace str {

// Bounded null-terminated copy into a fixed buffer (replaces lstrcpyn / strcpy_s /
// StringCchCopy). Only for OS structs with fixed fields — prefer owned Str/WStr
// otherwise. dst.len is capacity including the terminator. Returns chars written
// excluding the terminator.
int BufSet(Str dst, Str src) {
    int cchDst = dst.len;
    if (0 == cchDst || !dst.s) {
        ReportIf(true);
        return 0;
    }
    if (len(src) == 0) {
        *dst.s = 0;
        return 0;
    }

    int toCopy = std::min(cchDst - 1, src.len);

    memcpy(dst.s, src.s, (size_t)toCopy);
    dst.s[toCopy] = '\0';

    return toCopy;
}

} // namespace str
namespace wstr {

// WCHAR overload of BufSet — replaces lstrcpynW / wcscpy_s / wcsncpy_s / StringCchCopyW.
int BufSet(WStr dst, WStr src) {
    int cchDst = dst.len;
    if (0 == cchDst || !dst.s) {
        ReportIf(true);
        return 0;
    }
    if (len(src) == 0) {
        *dst.s = 0;
        return 0;
    }

    int toCopy = std::min(cchDst - 1, src.len);

    memset(dst.s, 0, cchDst * sizeof(WCHAR));
    memcpy(dst.s, src.s, toCopy * sizeof(WCHAR));
    return toCopy;
}

} // namespace wstr
namespace str {

// UTF-8 Str → fixed WCHAR buffer (converts then BufSet).
int BufSet(WCHAR* dst, int dstCchSize, Str src) {
    return wstr::BufSet(WStr(dst, dstCchSize), ToWStrTemp(src));
}

// append as much of s at the end of dst (which must be properly null-terminated)
// as will fit.
int BufAppend(Str dst, Str s) {
    int dstCch = dst.len;
    ReportIf(0 == dstCch);

    int currDstCchLen = len(dst.s);
    if (currDstCchLen + 1 >= dstCch) {
        return 0;
    }
    int left = dstCch - currDstCchLen - 1;
    int toCopy = std::min(left, s.len);

    memcpy(dst.s + currDstCchLen, s.s, (size_t)toCopy);
    dst.s[currDstCchLen + toCopy] = '\0';

    return toCopy;
}

} // namespace str

namespace url {

bool IsAbsolute(Str url) {
    int colon = str::IndexOfChar(url, ':');
    if (colon < 0) {
        return false;
    }
    int hash = str::IndexOfChar(url, '#');
    return hash < 0 || hash > colon;
}

// url up to its query / fragment, still encoded
static Str PathPart(Str url) {
    int n = 0;
    while (n < url.len && url.s[n] != '#' && url.s[n] != '?') {
        n++;
    }
    return Str(url.s, n);
}

TempStr GetFullPathTemp(Str url) {
    return DecodeTemp(PathPart(url));
}

// the last path segment, decoded after the split so an encoded '/' stays in the name
TempStr GetFileNameTemp(Str url) {
    Str path = PathPart(url);
    int base = path.len;
    while (base > 0 && path.s[base - 1] != '/' && path.s[base - 1] != '\\') {
        base--;
    }
    return base < path.len ? DecodeTemp(Str(path.s + base, path.len - base)) : Str{};
}

} // namespace url

int ParseInt(Str s) {
    if (len(s) == 0) {
        return 0;
    }
    int off = 0;
    bool negative = s.s[0] == '-';
    if (negative) {
        off = 1;
    }
    int value = 0;
    int overflowCheck = negative ? 1 : 0;
    for (; off < s.len && str::IsDigit(s.s[off]); off++) {
        value = (value * 10) + (s.s[off] - '0');
        // return 0 on overflow
        if (value - overflowCheck < 0) {
            return 0;
        }
    }
    return negative ? -value : value;
}

i64 ParseInt64(Str s) {
    if (len(s) == 0) {
        return 0;
    }
    int off = 0;
    bool negative = s.s[0] == '-';
    if (negative) {
        off = 1;
    }
    i64 value = 0;
    for (; off < s.len && str::IsDigit(s.s[off]); off++) {
        value = (value * 10) + (s.s[off] - '0');
    }
    return negative ? -value : value;
}

// the only valid chars are 0-9, . and newlines.
// a valid version has to match the regex /^\d+(\.\d+)*(\r?\n)?$/
// Return false if it contains anything else.
bool IsValidProgramVersion(Str ver) {
    if (len(ver) == 0 || !str::IsDigit(ver.s[0])) {
        return false;
    }

    for (int i = 0; i < ver.len; i++) {
        char c = ver.s[i];
        if (str::IsDigit(c)) {
            continue;
        }
        if (c == '.' && i + 1 < ver.len && str::IsDigit(ver.s[i + 1])) {
            continue;
        }
        if (c == '\r' && i + 1 < ver.len && ver.s[i + 1] == '\n') {
            continue;
        }
        if (c == '\n' && i + 1 == ver.len) {
            continue;
        }
        return false;
    }

    return true;
}

static unsigned int ExtractNextNumber(Str txt, int& off) {
    unsigned int val = 0;
    if (off >= txt.len) {
        off = txt.len;
        return 0;
    }
    Str slice(txt.s + off, txt.len - off);
    Str next = str::Parse(slice, "%u%?.", &val);
    if (next) {
        off += (int)(next.s - slice.s);
    } else {
        off = txt.len;
    }
    return val;
}

// compare two version string. Return 0 if they are the same,
// > 0 if the first is greater than the second and < 0 otherwise.
// e.g.
//   0.9.3.900 is greater than 0.9.3
//   1.09.300 is greater than 1.09.3 which is greater than 1.9.1
//   1.2.0 is the same as 1.2
int CompareProgramVersion(Str ver1, Str ver2) {
    int off1 = 0;
    int off2 = 0;
    while (off1 < ver1.len || off2 < ver2.len) {
        unsigned int v1 = ExtractNextNumber(ver1, off1);
        unsigned int v2 = ExtractNextNumber(ver2, off2);
        if (v1 != v2) {
            return (int)v1 - (int)v2;
        }
    }
    return 0;
}

// shorten a string to maxLen characters, adding ellipsis in the middle
// ascii version that doesn't handle UTF-8
// IsTextRtl is optimized version of checking if a string is rtl
// we look at max first 40 chars and
bool IsTextRtl(WStr s) {
    if (len(s) == 0) {
        return false;
    }
    int n = s.len > 40 ? 40 : s.len;
    int nRtl = 0;
    int nLtr = 0;
    WORD* charTypes = AllocArrayTemp<WORD>(n + 1);
    if (!GetStringTypeExW(LOCALE_INVARIANT, CT_CTYPE2, s.s, n, charTypes)) {
        return false; // API failure
    }
    for (int i = 0; i < n; ++i) {
        WORD type = charTypes[i];
        if (type == C2_LEFTTORIGHT) {
            nLtr++;
        } else if (type == C2_RIGHTTOLEFT) {
            nRtl++;
        }
    }
    return nRtl > nLtr;
}

bool IsTextRtl(Str s) {
    TempWStr ws = ToWStrTemp(s);
    return IsTextRtl(ws);
}

// ---- temp-arena variants of the str:: functions above ----

namespace str {
TempStr DupTemp(Str s) {
    return Dup(GetTempArena(), s);
}

TempWStr DupTemp(WStr s) {
    return wstr::Dup(GetTempArena(), s);
}

TempStr JoinTemp(Str s1, Str s2, Str s3) {
    return Join(GetTempArena(), s1, s2, s3);
}

TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4) {
    return Join(GetTempArena(), s1, s2, s3, s4, Str{});
}

TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4, Str s5) {
    return Join(GetTempArena(), s1, s2, s3, s4, s5);
}

TempWStr JoinTemp(WStr s1, WStr s2, WStr s3) {
    return wstr::Join(GetTempArena(), s1, s2, s3);
}

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith) {
    if (str::IsNull(s) || len(toReplace) == 0 || str::IsNull(replaceWith)) {
        return {};
    }

    Str curr = s;
    int idx = str::IndexOf(curr, toReplace);
    if (idx < 0) {
        // optimization: nothing to replace so do nothing
        return s;
    }

    int findLen = toReplace.len;
    int replLen = replaceWith.len;
    int lenDiff = 0;
    if (replLen > findLen) {
        lenDiff = replLen - findLen;
    }
    // heuristic: allow 6 replacements without reallocating
    str::Builder result;
    result.Reserve(s.len + 1 + (lenDiff * 6));
    bool ok;
    while (idx >= 0) {
        ok = result.Append(Str(curr.s, idx));
        if (!ok) {
            return {};
        }
        ok = result.Append(Str(replaceWith.s, replLen));
        if (!ok) {
            return {};
        }
        curr = Str(curr.s + idx + findLen, curr.len - idx - findLen);
        idx = str::IndexOf(curr, toReplace);
    }
    ok = result.Append(curr);
    if (!ok) {
        return {};
    }
    return ToStrTemp(result);
}

TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith) {
    int n = toReplace.len;
    int idx = str::IndexOfI(s, toReplace);
    if (idx < 0) {
        return s;
    }
    char* pos = s.s + idx;
    if (!MemEq(pos, toReplace.s, n)) {
        toReplace = str::DupTemp(Str(pos, n));
    }
    return str::ReplaceTemp(s, toReplace, replaceWith);
}
} // namespace str

// Temporary, guaranteed zero-terminated copy, for passing to C / win32 APIs
// that require a NUL-terminated string.
// Temporary, guaranteed zero-terminated copy of s (lives in the temp arena).
// Use when passing a Str/WStr to a C or win32 API that requires a
// NUL-terminated string; the name documents that intent at the call site.
// Returns non-const so it implicitly converts to both char* and const char*
// (some C/win32 APIs take non-const), avoiding casts at the call site.
char* CStrTemp(Str s) {
    return str::DupTemp(s).s;
}

WCHAR* CWStrTemp(WStr s) {
    return str::DupTemp(s).s;
}

WCHAR* CWStrTemp(WStr s, int& cch) {
    WStr ws = str::DupTemp(s);
    cch = ws.len;
    return ws.s;
}

// str::Builder/wstr::Builder always keep their data NUL-terminated; ToStr()
// returns a {ptr,len} view of it, which may contain embedded NULs
Str ToStr(const str::Builder& b) {
    return Str(b.els, (int)b.len);
}

// NO_INLINE: this is called in many places; keeping it out of line trims code size
// owning temp-arena copy of the builder's content (unlike ToStr()'s view)
NO_INLINE TempStr ToStrTemp(const str::Builder& b) {
    return str::DupTemp(ToStr(b));
}

WStr ToWStr(const wstr::Builder& b) {
    return WStr(b.els, (int)b.len);
}

// --- begin: merged from former src/common/str_util.cpp ---
int WStrFindSubstr(WStr str, WStr substr) {
    if (len(substr) == 0) return -1; // Empty search - no highlight
    if (substr.len > str.len) return -1;

    for (int i = 0; i <= str.len - substr.len; i++) {
        bool match = true;
        for (int j = 0; j < substr.len; j++) {
            if (WCharToLower(str.s[i + j]) != WCharToLower(substr.s[j])) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return -1;
}

// Format size in human readable form (e.g., "1.23 GB", "456 KB")
TempStr FormatFileSizeTemp(u64 size) {
    // clang-format off
static const struct {
    u64 divisor;
    Str suffix;
} kUnits[] = {
    {1ULL << 40, StrL("TB")}, {1ULL << 30, StrL("GB")}, {1ULL << 20, StrL("MB")}, {1ULL << 10, StrL("KB")},
};
    // clang-format on
    for (auto& u : kUnits) {
        if (size < u.divisor) {
            continue;
        }
        // up to 2 decimals, trailing zeros dropped: "1 GB", "1.5 GB", "1.23 GB"
        u64 whole = size / u.divisor;
        int frac = (int)(((size % u.divisor) * 100) / u.divisor);
        if (frac == 0) {
            return fmt("%llu %s", whole, u.suffix);
        }
        if (frac % 10 == 0) {
            return fmt("%llu.%d %s", whole, frac / 10, u.suffix);
        }
        return fmt("%llu.%02d %s", whole, frac, u.suffix);
    }
    return fmt("%llu B", size);
}

// --- end: merged from former src/common/str_util.cpp ---

//--- StrUtf8.cpp ----------------------------------------------------------------

#include <locale.h>

#ifdef _MSC_VER
static _locale_t GetUtf8FormatLocale() {
    // wrapped in a struct so the locale is freed at exit (keeps leak
    // detectors quiet); after the destructor runs, callers see nullptr
    // and fall back to plain vsnprintf
    struct Locale {
        _locale_t loc = _create_locale(LC_ALL, ".UTF-8");
        ~Locale() {
            if (loc) {
                _free_locale(loc);
                loc = nullptr;
            }
        }
    };
    static Locale l;
    return l.loc;
}
#endif

// The format string is a plain const char* because this is a thin wrapper around
// vsnprintf and is almost always called with a string literal.
int str::VsnprintfUtf8(Str buf, const char* fmt, va_list args) {
#ifdef _MSC_VER
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vsnprintf_l(buf.s, (size_t)buf.len, fmt, loc, args);
    }
#endif
    return vsnprintf(buf.s, (size_t)buf.len, fmt, args);
}

// How long the formatted output will be. vsnprintf reports it on the platforms
// whose vsnprintf does; MSVC keeps it in a call of its own.
static int VscprintfUtf8(const char* fmt, va_list args) {
#ifdef _MSC_VER
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vscprintf_l(fmt, loc, args);
    }
    return _vscprintf(fmt, args);
#else
    return vsnprintf(nullptr, 0, fmt, args);
#endif
}

// --- copyright for utf8 code below

/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
 * left as-is for anyone who may want to do such conversion, which was
 * allowed in earlier algorithms.
 */
static const u8 trailingBytesForUTF8[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};

static bool isLegalUTF8(const u8* src, int length) {
    u8 a;
    for (int i = 0; i < length; i++) {
        a = src[i];
        if (a == 0) {
            return false;
        }
    }
    const u8* end = src + length;

    switch (length) {
        default:
            return false;
        case 4:
            a = (*--end);
            if (a < 0x80 || a > 0xBF) {
                return false;
            }
            [[fallthrough]];
        case 3:
            a = (*--end);
            if (a < 0x80 || a > 0xBF) {
                return false;
            }
            [[fallthrough]];
        case 2:
            a = (*--end);
            if (a > 0xBF) {
                return false;
            }

            switch (*src) {
                case 0xE0:
                    if (a < 0xA0) {
                        return false;
                    }
                    break;
                case 0xED:
                    if (a > 0x9F) {
                        return false;
                    }
                    break;
                case 0xF0:
                    if (a < 0x90) {
                        return false;
                    }
                    break;
                case 0xF4:
                    if (a > 0x8F) {
                        return false;
                    }
                    break;
                default:
                    if (a < 0x80) {
                        return false;
                    }
            }
            [[fallthrough]];
        case 1:
            if (*src >= 0x80 && *src < 0xC2) {
                return false;
            }
    }

    return *src <= 0xF4;
}

int utf8RuneLen(const u8* s) {
    int n = trailingBytesForUTF8[*s] + 1;
    return n;
}

// note: include Base.h instead of including directly
static bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd) {
    int n = utf8RuneLen(source);
    if (source + n > sourceEnd) {
        return false;
    }
    return isLegalUTF8(source, n);
}

static bool isLegalUTF8String(const u8** source, const u8* sourceEnd) {
    const u8* s = *source;
    while (s != sourceEnd) {
        int n = utf8RuneLen(s);
        if (n > sourceEnd - s || !isLegalUTF8(s, n)) {
            return false;
        }
        s += n;
    }
    *source = s;
    return true;
}

int utf8StrLen(const u8* s) {
    int cch = 0;
    while (*s) {
        int n = utf8RuneLen(s);
        if (!isLegalUTF8(s, n)) {
            return -1;
        }
        s += n;
        cch++;
    }
    return cch;
}

// --- end of Unicode, Inc. utf8 code

void str::Utf8Encode(char* buf, int& off, int c) {
    u8* tmp = (u8*)(buf + off);
    if (c < 0x00080) {
        *tmp++ = (u8)(c & 0xFF);
    } else if (c < 0x00800) {
        *tmp++ = 0xC0 + (u8)((c >> 6) & 0x1F);
        *tmp++ = 0x80 + (u8)(c & 0x3F);
    } else if (c < 0x10000) {
        *tmp++ = 0xE0 + (u8)((c >> 12) & 0x0F);
        *tmp++ = 0x80 + (u8)((c >> 6) & 0x3F);
        *tmp++ = 0x80 + (u8)(c & 0x3F);
    } else {
        *tmp++ = 0xF0 + (u8)((c >> 18) & 0x07);
        *tmp++ = 0x80 + (u8)((c >> 12) & 0x3F);
        *tmp++ = 0x80 + (u8)((c >> 6) & 0x3F);
        *tmp++ = 0x80 + (u8)(c & 0x3F);
    }
    off = (int)((char*)tmp - buf);
}

static bool Utf8IsContinuationByte(char c) {
    return ((u8)c & 0xC0) == 0x80;
}

// the byte a sequence starts at, so that a byte index that landed in the middle
// of one can be turned into a codepoint
int Utf8CodepointStartByte(Str s, int byteIdx) {
    if (byteIdx <= 0) {
        return 0;
    }
    byteIdx = std::min(byteIdx, len(s));
    while (byteIdx > 0 && Utf8IsContinuationByte(s.s[byteIdx])) {
        byteIdx--;
    }
    return byteIdx;
}

// the codepoint the byte at byteIdx is part of, 0 if there is none
int Utf8CodepointContaining(Str s, int byteIdx) {
    if (len(s) == 0 || byteIdx < 0 || byteIdx >= len(s)) {
        return 0;
    }
    return Utf8CodepointAtByte(s, Utf8CodepointStartByte(s, byteIdx));
}

int Utf8CodepointAtByte(Str s, int byteIdx, int* bytesOut) {
    if (bytesOut) {
        *bytesOut = 0;
    }
    if (len(s) == 0 || byteIdx < 0 || byteIdx >= s.len) {
        return 0;
    }

    const u8* p = (const u8*)s.s + byteIdx;
    int n = utf8RuneLen(p);
    if (n <= 0 || byteIdx + n > s.len || !isLegalUTF8Sequence(p, p + n)) {
        if (bytesOut) {
            *bytesOut = 1;
        }
        return *p;
    }
    if (bytesOut) {
        *bytesOut = n;
    }
    if (n == 1) {
        return p[0];
    }
    int rune = p[0] & ((1 << (7 - n)) - 1);
    for (int i = 1; i < n; i++) {
        rune = (rune << 6) | (p[i] & 0x3f);
    }
    return rune;
}

int Utf8CodepointCount(Str s) {
    int nCodepoints = 0;
    for (int byteIdx = 0; s && byteIdx < s.len; nCodepoints++) {
        Utf8CodepointNext(s, byteIdx);
    }
    return nCodepoints;
}

int Utf8CodepointNext(Str s, int& byteIdx) {
    if (len(s) == 0 || byteIdx < 0 || byteIdx >= s.len) {
        return 0;
    }
    int n = 0;
    int c = Utf8CodepointAtByte(s, byteIdx, &n);
    byteIdx += n > 0 ? n : 1;
    return c;
}

int Utf8CodepointPrev(Str s, int& byteIdx) {
    if (len(s) == 0 || byteIdx <= 0) {
        return 0;
    }
    byteIdx = std::min(byteIdx, s.len);
    byteIdx = Utf8CodepointStartByte(s, byteIdx - 1);
    return Utf8CodepointAtByte(s, byteIdx);
}

static int Utf8AdvanceCodepoints(Str s, int byteIdx, int nCodepoints) {
    if (len(s) == 0 || byteIdx < 0) {
        return 0;
    }
    byteIdx = std::min(byteIdx, s.len);
    for (int i = 0; i < nCodepoints && byteIdx < s.len; i++) {
        Utf8CodepointNext(s, byteIdx);
    }
    return byteIdx;
}

int Utf8CodepointToByteIndex(Str s, int codepointIdx) {
    return Utf8AdvanceCodepoints(s, 0, codepointIdx);
}

Str Utf8SliceByCodepoints(Str s, int startCodepoint, int nCodepoints) {
    if (len(s) == 0 || nCodepoints <= 0) {
        return {};
    }
    startCodepoint = std::max(startCodepoint, 0);
    int startByte = Utf8CodepointToByteIndex(s, startCodepoint);
    int endByte = Utf8AdvanceCodepoints(s, startByte, nCodepoints);
    return Str(s.s + startByte, endByte - startByte);
}

static TempStr ShortenStringTemp(Str s, int maxLen) {
    int sLen = len(s);
    if (sLen <= maxLen) {
        return s;
    }
    char* ret = AllocArrayTemp<char>(maxLen + 2);
    const int half = maxLen / 2;
    for (int i = 0; i < half; i++) {
        ret[i] = s.s[i];
        ret[i + half] = s.s[sLen - half + i];
    }
    ret[half - 2] = ret[half - 1] = ret[half] = '.';
    return Str(ret, maxLen + 2);
}

TempStr ShortenStringUtf8Temp(Str s, int maxRunes) {
    int nRunes = utf8StrLen((u8*)s.s);
    if (nRunes < 0) {
        int sLen = len(s);
        if (sLen <= maxRunes) {
            return s;
        }
        int keep = maxRunes - 3;
        keep = std::max(keep, 0);
        char* ret = AllocArrayTemp<char>(keep + 4);
        memcpy(ret, s.s, keep);
        ret[keep] = '.';
        ret[keep + 1] = '.';
        ret[keep + 2] = '.';
        ret[keep + 3] = 0;
        return Str(ret, keep + 3);
    }
    if (nRunes <= maxRunes) {
        return s;
    }
    int keep = maxRunes - 3;
    keep = std::max(keep, 0);
    char* ret = AllocArrayTemp<char>((maxRunes * 4) + 1);
    int src = 0;
    int tmp = 0;
    int n;
    for (int i = 0; i < keep; i++) {
        n = utf8RuneLen((const u8*)(s.s + src));
        ReportIf(n <= 0 || n > 4);
        memcpy(ret + tmp, s.s + src, n);
        tmp += n;
        src += n;
    }
    ret[tmp++] = '.';
    ret[tmp++] = '.';
    ret[tmp++] = '.';
    ret[tmp] = 0;
    return Str(ret, tmp);
}

TempStr ShortenStringUtf8InTheMiddleTemp(Str s, int maxRunes) {
    int nRunes = utf8StrLen((u8*)s.s);
    if (nRunes < 0) {
        return ShortenStringTemp(s, maxRunes);
    }
    if (nRunes <= maxRunes) {
        return s;
    }
    int toRemove = (nRunes - maxRunes) + 3;
    int removeStartingAt = (nRunes / 2) - (toRemove / 2);
    char* ret = AllocArrayTemp<char>((maxRunes * 4) + 1);
    int src = 0;
    int tmp = 0;
    int n;
    for (int i = 0; i < nRunes; i++) {
        n = utf8RuneLen((const u8*)(s.s + src));
        ReportIf(n <= 0);
        if (i < removeStartingAt || i >= removeStartingAt + toRemove) {
            ReportIf(n > 4);
            memcpy(ret + tmp, s.s + src, n);
            tmp += n;
            src += n;
        } else if (i == removeStartingAt) {
            ret[tmp++] = '.';
            ret[tmp++] = '.';
            ret[tmp++] = '.';
            src += n;
        } else {
            src += n;
        }
    }
    return Str(ret, tmp);
}

static wchar_t emptyWideStr[1] = {0};

Str ToUtf8Temp(WStr wide) {
    return strconv::WStrToCodePage(CP_UTF8, wide, GetTempArena());
}

WStr ToWStrTemp(Str s) {
    if (len(s) == 0) {
        return WStr(&emptyWideStr[0], 0);
    }
    return strconv::CodePageToWStr(CP_UTF8, s, GetTempArena());
}

// Converts a UTF-8 Str to a NUL-terminated WCHAR* temp. Use when the wide
// result is only needed as a C/win32 string pointer.
WCHAR* CWStrTemp(Str s) {
    return ToWStrTemp(s).s;
}

WCHAR* CWStrTemp(Str s, int& cch) {
    WStr ws = ToWStrTemp(s);
    cch = ws.len;
    return ws.s;
}

//--- StrFormatParse.cpp ----------------------------------------------------------------

/*
str::Fmt is type-safe printf()-like system. Every directive starts with '%':
the usual %d / %s / %f etc., plus two that take an argument of any type:

  %{}   the next argument, whatever its type (same as %v)
  %{$n} the n-th argument (0-based), whatever its type

%% is the only escape; '{' on its own is ordinary text, so registry paths,
GUIDs, CSS and JS templates pass through untouched.

Type safety is achieved by using strongly typed methods for adding arguments
(i(), c(), s() etc.). We also verify that the type of the argument matches
the type of formatting directive.

Positional directives are useful in translations with more than 1 argument
because in some languages translation is akward if you can't re-arrange
the order of arguments.

Idiomatic usage:
str::Fmt fmt("%d = %s");
char *s = fmt.i(5).s("5").Get(); // returns "5 = 5"
// s is valid until fmt is valid
// use .GetDup() to get a copy that must be free()d
// you can re-use fmt as:
s = fmt.ParseFormat("%{1} = %{2} + %{0}").i(3).s("3").s(L"

You can mix %-style and %{$n} directives but beware, as the rule for assigning
argument number to a plain % directive is simple (n-th argument position for
n-th % directive) but it's easy to mis-count when adding %{$n} to the mix.

TODO: similar approach could be used for type-safe scanf() replacement.
*/

namespace str {

// formatting instruction
struct Inst {
    FmtArg::Kind t = FmtArg::Kind::None;
    int argNo = 0;  // <0 for strings that come from formatting string
    int rawOff = 0; // offset into format for FmtArg::Kind::RawStr / start of fwp for % spec
    int sLen = 0;   // length, for FmtArg::Kind::RawStr

    // for a % spec: the conversion char and the flags+width+precision range
    // (everything between '%' and the length-modifier/conversion). We delegate
    // the actual formatting to snprintf, only normalizing the length modifier so
    // 32/64-bit semantics match printf exactly.
    char conv = 0;
    int intBits = 0; // 32 or 64 for integer-family conversions
    int fwpOff = 0;  // offset into format of flags+width+precision
    int fwpLen = 0;
    int width = 0; // parsed width (for manual %s padding)
    int prec = -1; // parsed precision, -1 if none (for manual %s)
    bool leftJust = false;
};

struct Fmt {
    Fmt() = default;
    ~Fmt() = default;

    bool Eval(const FmtArg** args, int nArgs);

    bool isOk = true; // true if mismatch between formatting instruction and args

    Str format;
    Inst instructions[32]{}; // 32 should be big enough for everybody
    int nInst = 0;

    int currArgNo = 0;
    int currPercArgNo = 0;
    str::Builder res;

    // Scratch for one conversion. A field too wide for it is written straight
    // into `res` instead, so this is a fast path and not a limit.
    char buf[256] = {};
};

static void addRawStr(Fmt& fmt, int off, size_t n) {
    if (n == 0) {
        return;
    }
    if (fmt.nInst >= dimofi(fmt.instructions)) {
        fmt.isOk = false;
        return;
    }
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = FmtArg::Kind::RawStr;
    i.rawOff = off;
    i.sLen = (int)n;
    i.argNo = -1;
}

// parse: %{} (the next argument) or %{$n} (positional). off points at the '{',
// the '%' has already been consumed. Both take an argument of any type.
static int parseArgDefBrace(Fmt& fmt, int off) {
    ReportIf(fmt.format.s[off] != '{');
    off++;
    int n = 0;
    bool positional = false;
    // a '{' with no closing '}' must not walk past the end of the format string.
    // Reachable via a translated format string (fmt(Tr("...").s, ...)).
    while (off < fmt.format.len && fmt.format.s[off] != '}') {
        if (!str::IsDigit(fmt.format.s[off])) {
            fmt.isOk = false;
            return off;
        }
        n = (n * 10) + (fmt.format.s[off] - '0');
        positional = true;
        off++;
    }
    if (off >= fmt.format.len) {
        fmt.isOk = false;
        return off;
    }
    if (fmt.nInst >= dimofi(fmt.instructions)) {
        fmt.isOk = false;
        return off;
    }
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = FmtArg::Kind::Any;
    // %{} consumes arguments in order, like every other % directive
    i.argNo = positional ? n : fmt.currPercArgNo++;
    return off + 1;
}

static FmtArg::Kind typeFromConv(char c) {
    switch (c) {
        case 'c':
            return FmtArg::Kind::Char;
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            return FmtArg::Kind::Int;
        case 'p':
            return FmtArg::Kind::Ptr;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            return FmtArg::Kind::Float;
        case 's':
        case 'S':
            return FmtArg::Kind::Str;
        case 'v':
            return FmtArg::Kind::Any;
    }
    return FmtArg::Kind::None;
}

static bool startsWith(Str s, int off, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (off + i >= s.len || s.s[off + i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return true;
}

// parse: %[flags][width][.prec][length]<conv>
// We capture flags+width+precision verbatim (fed to snprintf) and normalize the
// length modifier into an explicit 32/64-bit width so output matches printf.
static int parseArgDefPerc(Fmt& fmt, int off) {
    Str f = fmt.format;
    ReportIf(f.s[off] != '%');
    off++; // past '%'
    int fwpStart = off;
    bool leftJust = false;
    // flags
    while (off < f.len &&
           (f.s[off] == '-' || f.s[off] == '+' || f.s[off] == ' ' || f.s[off] == '0' || f.s[off] == '#')) {
        if (f.s[off] == '-') {
            leftJust = true;
        }
        off++;
    }
    // width
    int width = 0;
    while (off < f.len && str::IsDigit(f.s[off])) {
        width = (width * 10) + (f.s[off] - '0');
        off++;
    }
    // precision
    int prec = -1;
    if (off < f.len && f.s[off] == '.') {
        off++;
        prec = 0;
        while (off < f.len && str::IsDigit(f.s[off])) {
            prec = (prec * 10) + (f.s[off] - '0');
            off++;
        }
    }
    int fwpEnd = off;
    // length modifier; determine integer width (32/64 on LLP64 / win64)
    // long is 32-bit on win64; z/j/t/I (size_t, intmax_t, ptrdiff_t, MS
    // size_t) are 64. Longer modifiers first so "I" doesn't eat "I64".
    // clang-format off
static const struct {
    const char* mod;
    int bits;
} kLenMods[] = {
    {"I64", 64}, {"I32", 32}, {"ll", 64}, {"hh", 32}, {"l", 32}, {"h", 32},
    {"L", 32},   {"w", 32},   {"z", 64},  {"j", 64},  {"t", 64}, {"I", 64},
};
    // clang-format on
    int bits = 32;
    for (auto& m : kLenMods) {
        if (startsWith(f, off, m.mod)) {
            bits = m.bits;
            off += (int)strlen(m.mod);
            break;
        }
    }
    char conv = (off < f.len) ? f.s[off] : 0;
    off++;

    if (fmt.nInst >= dimofi(fmt.instructions)) {
        fmt.isOk = false;
        return off;
    }
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = typeFromConv(conv);
    i.argNo = fmt.currPercArgNo++;
    i.conv = conv;
    i.intBits = bits;
    i.fwpOff = fwpStart;
    i.fwpLen = fwpEnd - fwpStart;
    i.width = width;
    i.prec = prec;
    i.leftJust = leftJust;
    return off;
}

static bool hasInstructionWithArgNo(Inst* insts, int nInst, int argNo) {
    for (int i = 0; i < nInst; i++) {
        if (insts[i].argNo == argNo) {
            return true;
        }
    }
    return false;
}

static bool isIntLike(FmtArg::Kind t) {
    return t == FmtArg::Kind::Char || t == FmtArg::Kind::Int || t == FmtArg::Kind::Ptr;
}

static bool validArgTypes(FmtArg::Kind instType, FmtArg::Kind argType) {
    if (instType == FmtArg::Kind::Any || instType == FmtArg::Kind::RawStr) {
        return true;
    }
    // integer-family specs (%c %d %u %x %p ...) accept any integer-like arg
    // (char / int / pointer), matching printf's leniency -- e.g. an HWND with
    // %x, or an int with %c.
    if (instType == FmtArg::Kind::Char || instType == FmtArg::Kind::Int || instType == FmtArg::Kind::Ptr) {
        return isIntLike(argType);
    }
    if (instType == FmtArg::Kind::Float) {
        return argType == FmtArg::Kind::Float || argType == FmtArg::Kind::Double;
    }
    if (instType == FmtArg::Kind::Str) {
        return argType == FmtArg::Kind::Str || argType == FmtArg::Kind::WStr;
    }
    return false;
}

static bool ParseFormat(Fmt& o, Str fmtStr) {
    o.format = fmtStr;
    o.nInst = 0;
    o.currPercArgNo = 0;
    o.currArgNo = 0;
    o.res.Reset();

    // parse formatting string, until a %$c, %{} or %{$n}
    // %% is how we escape %; nothing else is special, so a bare '{' is text
    int start = 0;
    int off = 0;
    while (off < fmtStr.len && fmtStr.s[off]) {
        char c = fmtStr.s[off];
        if ('%' == c) {
            // handle %%
            if (off + 1 < fmtStr.len && '%' == fmtStr.s[off + 1]) {
                addRawStr(o, start, off - start);
                start = off + 1;
                off += 2; // skip '%'
                continue;
            }
            addRawStr(o, start, off - start);
            if (off + 1 < fmtStr.len && '{' == fmtStr.s[off + 1]) {
                off = parseArgDefBrace(o, off + 1);
            } else {
                off = parseArgDefPerc(o, off);
            }
            start = off;
            continue;
        }
        off++;
    }
    addRawStr(o, start, off - start);

    int maxArgNo = -1; // -1 so an escape/literal-only format requires no args
    // check that arg numbers in %{$n} makes sense
    for (int i = 0; i < o.nInst; i++) {
        if (o.instructions[i].t == FmtArg::Kind::RawStr) {
            continue;
        }
        maxArgNo = std::max(o.instructions[i].argNo, maxArgNo);
    }

    // instructions[i].argNo can be duplicate
    // (we can have positional arg like {0} multiple times
    // but must cover all space from 0..nArgsExpected
    for (int i = 0; i <= maxArgNo; i++) {
        bool isOk = hasInstructionWithArgNo(o.instructions, o.nInst, i);
        ReportIf(!isOk);
        if (!isOk) {
            return false;
        }
    }
    return true;
}

// Format one conversion onto the answer via snprintf. The 256-byte scratch
// buffer takes all but the widest fields in one pass; a field that does not fit
// is written straight into the answer instead, so a width says what it says
// rather than being cut to the size of the buffer.
static bool appendConv(Fmt& fmt, const char* spec, ...) {
    va_list args;
    va_start(args, spec);
    va_list retry;
    va_copy(retry, args);
    Str bufS(fmt.buf, dimofi(fmt.buf));
    int n = str::VsnprintfUtf8(bufS, spec, args);
    va_end(args);
    fmt.buf[dimof(fmt.buf) - 1] = 0;
    if (n >= 0 && n < bufS.len) {
        va_end(retry);
        return fmt.res.Append(Str(fmt.buf, n));
    }

    // Wider than the scratch buffer. MSVC's vsnprintf answers -1 rather than the
    // length it wanted, so the length is asked for separately.
    va_list write;
    va_copy(write, retry);
    int need = VscprintfUtf8(spec, retry);
    va_end(retry);
    bool ok = false;
    str::Builder& res = fmt.res;
    int at = res.len;
    if (need >= 0 && need < INT_MAX - at - 1 && res.Reserve(at + need + 1)) {
        Str dst(res.els + at, need + 1);
        if (str::VsnprintfUtf8(dst, spec, write) == need) {
            res.len = at + need;
            Terminate(res);
            ok = true;
        }
    }
    va_end(write);
    return ok;
}

// default formatting for {n} positional and %v: format by the arg's runtime type
static bool evalDefault(Fmt& fmt, const FmtArg& arg) {
    TempStr s;
    switch (arg.t) {
        case FmtArg::Kind::Char:
            return fmt.res.AppendChar(arg.c);
        case FmtArg::Kind::Int:
            return appendConv(fmt, "%lld", (long long)arg.i);
        case FmtArg::Kind::Ptr:
            return appendConv(fmt, "%p", arg.ptr);
        case FmtArg::Kind::Float:
            // Note: %G, unlike %f, avoids trailing '0'
            return appendConv(fmt, "%G", (double)arg.f);
        case FmtArg::Kind::Double:
            return appendConv(fmt, "%G", arg.d);
        case FmtArg::Kind::Str:
            return fmt.res.Append(arg.str);
        case FmtArg::Kind::WStr:
            s = ToUtf8Temp(arg.wstr);
            return fmt.res.Append(s);
        default:
            ReportIf(true);
            return true;
    }
}

// extract an integer value from any integer-like arg (char / int / pointer) so
// %d/%x/%c/%p work with any of them, like printf.
static i64 argToI64(const FmtArg& arg) {
    switch (arg.t) {
        case FmtArg::Kind::Char:
            return (i64)arg.c;
        case FmtArg::Kind::Ptr:
            return (i64)(intptr_t)arg.ptr;
        default:
            return arg.i;
    }
}

// format a typed % spec by reconstructing a single-conversion printf format and
// delegating to snprintf (bufFmt), normalizing the length modifier so the
// 32/64-bit value width matches printf. %s padding/truncation is done by hand to
// avoid relying on the Str being NUL-terminated.
static bool evalPercInst(Fmt& fmt, const Inst& inst, const FmtArg& arg) {
    if (inst.conv == 's' || inst.conv == 'S') {
        Str sv = (arg.t == FmtArg::Kind::WStr) ? ToUtf8Temp(arg.wstr) : arg.str;
        int slen = sv.len;
        if (inst.prec >= 0 && inst.prec < slen) {
            slen = inst.prec;
        }
        int pad = inst.width - slen;
        pad = std::max(pad, 0);
        if (!inst.leftJust) {
            for (int j = 0; j < pad; j++) {
                if (!fmt.res.AppendChar(' ')) {
                    return false;
                }
            }
        }
        if (!fmt.res.Append(Str(sv.s, slen))) {
            return false;
        }
        if (inst.leftJust) {
            for (int j = 0; j < pad; j++) {
                if (!fmt.res.AppendChar(' ')) {
                    return false;
                }
            }
        }
        return true;
    }

    // build "%" + flags+width+precision into fbuf
    char fbuf[64];
    int k = 0;
    fbuf[k++] = '%';
    for (int j = 0; j < inst.fwpLen && k < dimofi(fbuf) - 5; j++) {
        fbuf[k++] = fmt.format.s[inst.fwpOff + j];
    }
    char conv = inst.conv;
    i64 ival = argToI64(arg);
    bool ok = true;
    switch (conv) {
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            bool isSigned = conv == 'd' || conv == 'i';
            bool is64 = inst.intBits == 64;
            if (is64) {
                fbuf[k++] = 'l';
                fbuf[k++] = 'l';
            }
            fbuf[k++] = isSigned ? 'd' : conv;
            fbuf[k] = 0;
            if (is64) {
                ok =
                    isSigned ? appendConv(fmt, fbuf, (long long)ival) : appendConv(fmt, fbuf, (unsigned long long)ival);
            } else {
                ok = isSigned ? appendConv(fmt, fbuf, (int)ival) : appendConv(fmt, fbuf, (unsigned int)ival);
            }
        } break;
        case 'c':
            fbuf[k++] = 'c';
            fbuf[k] = 0;
            ok = appendConv(fmt, fbuf, (int)ival);
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            fbuf[k++] = conv;
            fbuf[k] = 0;
            double dv = (arg.t == FmtArg::Kind::Double) ? arg.d : (double)arg.f;
            ok = appendConv(fmt, fbuf, dv);
        } break;
        case 'p': {
            // flags/width are uncommon (and platform-specific) for %p; emit plain
            const void* pv = (arg.t == FmtArg::Kind::Ptr) ? arg.ptr : (const void*)(intptr_t)ival;
            ok = appendConv(fmt, "%p", pv);
        } break;
        default:
            ReportIf(true);
            break;
    }
    return ok;
}

bool Fmt::Eval(const FmtArg** args, int nArgs) {
    if (!isOk) {
        // if failed parsing format
        return false;
    }

    for (int n = 0; n < nInst; n++) {
        ReportIf(n >= dimof(instructions));

        auto& inst = instructions[n];

        if (inst.t == FmtArg::Kind::RawStr) {
            if (!res.Append(Str(format.s + inst.rawOff, inst.sLen))) {
                isOk = false;
                return false;
            }
            continue;
        }

        int argNo = inst.argNo;
        ReportIf(argNo < 0 || argNo >= nArgs);
        if (argNo < 0 || argNo >= nArgs) {
            isOk = false;
            return false;
        }

        const FmtArg& arg = *args[argNo];
        isOk = validArgTypes(inst.t, arg.t);
        ReportIf(!isOk);
        if (!isOk) {
            return false;
        }

        // an append that could not allocate has to be told apart from one
        // that worked, or Eval answers true over a string missing the middle
        // of it
        bool appended = (inst.t == FmtArg::Kind::Any) ? evalDefault(*this, arg) : evalPercInst(*this, inst, arg);
        if (!appended) {
            isOk = false;
            return false;
        }
    }
    return true;
}

// Format into an explicit arena; the returned Str lives in `a`. Use this
// instead of fmt()/FormatTemp when the result must outlive the temp allocator's
// scope, or on paths that must not touch the temp allocator / heap at all (e.g.
// the crash handler, which pre-allocates its arena). FormatTempArgs() is just
// this with GetTempArena().
Str FormatArgs(Arena* a, const char* fmt, const FmtArg** args, int nArgs) {
    // trailing arguments could be empty (unused defaults from the variadic call)
    while (nArgs > 0 && args[nArgs - 1]->t == FmtArg::Kind::None) {
        nArgs--;
    }

    if (nArgs == 0) {
        // no args: if the format has no directives, return it verbatim (fast
        // path); otherwise still run it through so %% is unescaped
        bool hasDirective = false;
        for (const char* p = fmt; p && *p; p++) {
            if (*p == '%') {
                hasDirective = true;
                break;
            }
        }
        if (!hasDirective) {
            return str::Dup(a, Str(fmt));
        }
    }

    Fmt f;
    // format directly into the caller's arena so there are no temp-allocator /
    // heap allocations at all (matters for the crash handler's pre-allocated
    // arena). TakeStr() then returns that arena buffer without a second copy.
    f.res.a = a;
    bool ok = ParseFormat(f, Str(fmt));
    if (!ok) {
        return {};
    }
    ok = f.Eval(args, nArgs);
    if (!ok) {
        return {};
    }
    return f.res.TakeStr();
}

// advance s past n already-consumed bytes
static void Eat(Str& s, int n) {
    ReportIf(n < 0 || n > s.len);
    s.s += n;
    s.len -= n;
}

// extract the part of s before the first occurrence of c and eat it
// (c itself is not eaten). returns {} and doesn't eat if c is not found
static TempStr ExtractUntilTemp(Str& s, char c) {
    int foundOff = IndexOfChar(s, c);
    if (foundOff < 0) {
        return {};
    }
    TempStr res = str::DupTemp(Str(s.s, foundOff));
    Eat(s, foundOff);
    return res;
}

static bool ParseULongAt(Str& s, int base, unsigned long* val) {
    if (s.len <= 0) {
        return false;
    }
    unsigned long v = 0;
    str::TrimWs(s);
    int i = 0;
    if (base == 16 && i + 1 < s.len && s.s[i] == '0' && (s.s[i + 1] == 'x' || s.s[i + 1] == 'X')) {
        i += 2;
    }
    bool any = false;
    while (i < s.len) {
        char c = s.s[i];
        int digit = -1;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (base == 16) {
            digit = HexDigitVal(c);
        }
        if (digit < 0 || (unsigned)digit >= (unsigned)base) {
            break;
        }
        any = true;
        v = (v * (unsigned long)base) + (unsigned long)digit;
        i++;
    }
    if (!any) {
        return false;
    }
    *val = v;
    Eat(s, i);
    return true;
}

static bool ParseLongAt(Str& s, int base, long* val) {
    if (s.len <= 0) {
        return false;
    }
    Str rest = s;
    str::TrimWs(rest);
    if (rest.len <= 0) {
        return false;
    }
    bool neg = rest.s[0] == '-';
    if (neg || rest.s[0] == '+') {
        Eat(rest, 1);
    }
    unsigned long uv = 0;
    if (!ParseULongAt(rest, base, &uv)) {
        return false;
    }
    *val = neg ? -(long)uv : (long)uv;
    s = rest;
    return true;
}

static bool ParseDoubleAt(Str& s, double* val) {
    if (s.len <= 0) {
        return false;
    }
    char* sliceZ = CStrTemp(s);
    char* endPtr = nullptr;
    *val = strtod(sliceZ, &endPtr);
    if (!endPtr || endPtr == sliceZ) {
        return false;
    }
    Eat(s, (int)(endPtr - sliceZ));
    return true;
}

static int ParseLimitedNumber(Str& s, int formatOff, Str format, const ParseArg& valueOut) {
    unsigned int width;
    char f2[] = "% ";
    Str formatAt = Str(format.s + formatOff, format.len - formatOff);
    Str endF = Parse(formatAt, "%u%c", &width, &f2[1]);
    if (!str::IsNull(endF) && str::ContainsChar(StrL("udx"), f2[1]) && width <= (unsigned)s.len) {
        char limited[16]; // 32-bit integers are at most 11 characters long
        str::BufSet(Str(limited, std::min((int)width + 1, dimofi(limited))), Str(s.s, (int)width));
        Str end = ParseArgs(Str(limited), f2, &valueOut, 1);
        if (!str::IsNull(end) && !end.s[0]) {
            Eat(s, (int)width);
            return (int)(endF.s - format.s) - 1;
        }
    }
    return -1;
}

/* Parses a string into several variables sscanf-style (i.e. pass in pointers
   to where the parsed values are to be stored). Returns the part of str that
   hasn't been parsed when successful and {} otherwise.

   Supported formats:
     %u - parses an unsigned int
     %d - parses a signed int
     %x - parses an unsigned hex-int
     %f - parses a float
     %c - parses a single char
     %s - parses a string into an AutoFree (also on failure!)
     %S - parses a string into an AutoFree
     %? - makes the next single character optional (e.g. "x%?,y" parses both "xy" and "x,y")
     %$ - causes the parsing to fail if it's encountered when not at the end of the string
     %  - skips a single whitespace character
     %_ - skips one or multiple whitespace characters (or none at all)
     %% - matches a single '%'

   %u, %d and %x accept an optional width argument, indicating exactly how many
   characters must be read for parsing the number (e.g. "%4d" parses -123 out of "-12345"
   and doesn't parse "123" at all).
*/
Str ParseArgs(Str str, const char* fmt, const ParseArg* args, int nArgs) {
    if (str::IsNull(str) || !fmt) {
        return {};
    }
    Str format = Str(fmt);
    Str s = str; // the not yet parsed part of str
    int argIdx = 0;
    for (int fi = 0; fi < format.len; fi++) {
        char fc = format.s[fi];
        if (fc != '%') {
            if (s.len <= 0 || fc != s.s[0]) {
                return {};
            }
            Eat(s, 1);
            continue;
        }
        fi++;
        if (fi >= format.len) {
            return {};
        }
        char spec = format.s[fi];

        int lenBefore = s.len;
        if ('u' == spec) {
            unsigned long v = 0;
            if (!ParseULongAt(s, 10, &v)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(unsigned int*)args[argIdx++].ptr = (unsigned int)v;
        } else if ('d' == spec) {
            long v = 0;
            if (!ParseLongAt(s, 10, &v)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(int*)args[argIdx++].ptr = (int)v;
        } else if ('x' == spec) {
            unsigned long v = 0;
            if (!ParseULongAt(s, 16, &v)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(unsigned int*)args[argIdx++].ptr = (unsigned int)v;
        } else if ('f' == spec || 'g' == spec) {
            double v = 0;
            if (!ParseDoubleAt(s, &v)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(float*)args[argIdx++].ptr = (float)v;
        } else if ('c' == spec) {
            if (s.len <= 0) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(char*)args[argIdx++].ptr = s.s[0];
            Eat(s, 1);
        } else if ('s' == spec || 'S' == spec) {
            ReportIf(argIdx >= nArgs);
            const ParseArg& arg = args[argIdx++];
            TempStr val;
            if (fi + 1 < format.len) {
                // parse until the next character in the format string
                // (eats nothing if that character isn't found)
                val = ExtractUntilTemp(s, format.s[fi + 1]);
            } else {
                val = str::DupTemp(s);
                Eat(s, s.len);
            }
            if (arg.kind == ParseArg::Kind::WStrOut) {
                *(WStr*)arg.ptr = ToWStrTemp(val);
            } else {
                *(Str*)arg.ptr = val;
            }
        } else if ('$' == spec && s.len <= 0) {
            continue; // don't fail, if we're indeed at the end of the string
        } else if ('%' == spec) {
            if (s.len <= 0 || spec != s.s[0]) {
                return {};
            }
            Eat(s, 1);
        } else if (' ' == spec) {
            if (s.len <= 0 || !str::IsWs(s.s[0])) {
                return {};
            }
            Eat(s, 1);
        } else if ('_' == spec) {
            if (s.len <= 0 || !str::IsWs(s.s[0])) {
                continue; // don't fail, if there's no whitespace at all
            }
            int n = 1;
            while (n < s.len && str::IsWs(s.s[n])) {
                n++;
            }
            Eat(s, n);
        } else if ('?' == spec && fi + 1 < format.len) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            fi++;
            if (s.len <= 0 || s.s[0] != format.s[fi]) {
                continue;
            }
            Eat(s, 1);
        } else if (str::IsDigit(spec)) {
            ReportIf(argIdx >= nArgs);
            int formatIdx = ParseLimitedNumber(s, fi, format, args[argIdx++]);
            if (formatIdx < 0) {
                return {};
            }
            fi = formatIdx;
        }
        if (s.len >= lenBefore) {
            // nothing was parsed => failure
            return {};
        }
    }
    return s;
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale) {
    WCHAR thousandSepW[4]{};
    if (!GetLocaleInfoW(locale, LOCALE_STHOUSAND, thousandSepW, dimof(thousandSepW))) {
        str::BufSet(thousandSepW, dimof(thousandSepW), StrL(","));
    }
    TempStr thousandSep = ToUtf8Temp(thousandSepW);
    TempStr buf = str::FormatTemp("%d", num);

    // i64 with thousand seps is well under 48 bytes (e.g. "9,223,372,036,854,775,807").
    char resScratch[48]{};
    str::Builder res;
    res.UseExternalBuffer(Str(resScratch, sizeofi(resScratch)));
    int i = 3 - (buf.len % 3);
    for (int src = 0; src < buf.len; src++) {
        res.AppendChar(buf.s[src]);
        if (src + 1 < buf.len && i == 2) {
            res.Append(thousandSep);
        }
        i = (i + 1) % 3;
    }

    return ToStrTemp(res);
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
TempStr FormatFloatWithThousandSepTemp(double number, LCID locale, bool stripTrailingZero) {
    i64 num = (i64)llround(number * 100);

    TempStr tmp = FormatNumWithThousandSepTemp(num / 100, locale);
    WCHAR decimalW[4] = {};
    if (!GetLocaleInfoW(locale, LOCALE_SDECIMAL, decimalW, dimof(decimalW))) {
        decimalW[0] = '.';
        decimalW[1] = 0;
    }
    char decimal[4];
    int i = 0;
    for (WCHAR c : decimalW) {
        decimal[i++] = (char)c;
    }

    // add between one and two decimals after the point
    TempStr buf = str::FormatTemp("%s%s%02d", tmp, Str(decimal), num % 100);
    if (stripTrailingZero && str::EndsWith(buf, StrL("0"))) {
        buf.s[buf.len - 1] = '\0';
        buf.len--;
    }

    return buf;
}

constexpr double kKb = 1024;
constexpr double kMb = (double)1024 * (double)1024;
constexpr double kGb = (double)1024 * (double)1024 * (double)1024;

static Str sizeUnitsEnglish[3] = {StrL("GB"), StrL("MB"), StrL("KB")};

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// To be used in a context where translations are not yet available
TempStr FormatSizeShortTemp(i64 size) {
    return FormatSizeShortTemp(size, sizeUnitsEnglish);
}

TempStr FormatSizeShortTemp(i64 size, Str const* sizeUnits) {
    Str unit{};
    double s = (double)size;
    if (!sizeUnits) {
        sizeUnits = sizeUnitsEnglish;
    }
    // sizeUnits is GB, MB, KB
    const double kDivisors[] = {kGb, kMb, kKb};
    int i = s > kGb ? 0 : (s > kMb ? 1 : 2);
    s /= kDivisors[i];
    unit = sizeUnits[i];

    TempStr sizestr = str::FormatFloatWithThousandSepTemp(s, LOCALE_USER_DEFAULT, false);
    if (len(unit) == 0) {
        return sizestr;
    }
    return str::FormatTemp("%s %s", sizestr, unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTemp(i64 size) {
    if (size <= 0) {
        return str::FormatTemp("%d", (int)size);
    }
    TempStr n1 = str::FormatSizeShortTemp(size);
    TempStr n2 = str::FormatNumWithThousandSepTemp(size);
    return str::FormatTemp("%s (%s %s)", n1, n2, StrL("Bytes"));
}

// http://rosettacode.org/wiki/Roman_numerals/Encode#C.2B.2B
TempStr FormatRomanNumeralTemp(int n) {
    if (n < 1) {
        return {};
    }

    static struct {
        int value;
        Str numeral;
    } romandata[] = {{1000, StrL("M")}, {900, StrL("CM")}, {500, StrL("D")}, {400, StrL("CD")}, {100, StrL("C")},
                     {90, StrL("XC")},  {50, StrL("L")},   {40, StrL("XL")}, {10, StrL("X")},   {9, StrL("IX")},
                     {5, StrL("V")},    {4, StrL("IV")},   {1, StrL("I")}};

    // Page numbers in roman are short (e.g. 3999 -> "MMMCMXCIX" = 9 chars).
    char romanScratch[32]{};
    str::Builder roman;
    roman.UseExternalBuffer(Str(romanScratch, sizeofi(romanScratch)));
    for (auto& el : romandata) {
        for (; n >= el.value; n -= el.value) {
            roman.Append(el.numeral);
        }
    }
    return ToStrTemp(roman);
}

} // namespace str

//--- StrVec.cpp ----------------------------------------------------------------

// represents null string
constexpr u32 kNullOffset = (u32)-2;

bool StrLessNatural(Str s1, Str s2) {
    int n = str::CmpNatural(s1, s2);
    return n < 0;
}

struct PageOpResult {
    Str s;
    bool noSpace = false;

    static PageOpResult Fail() { return {{}, true}; }
};

struct StrVecPage {
    struct StrVecPage* next;
    int pageSize;
    int nStrings;
    int dataSize;
    u8* currEnd = nullptr;
    // now follows:
    // struct { size u32; offset u32}[nStrings] }
    // ... free space
    // strings (allocated from the end)

    Str AtStr(int i) const;
    void* AtDataRaw(int /*idx*/) const;

    Str RemoveAt(int /*idx*/);
    Str RemoveAtFast(int /*idx*/);

    int BytesLeft();
    PageOpResult Append(Str s);
    PageOpResult SetAt(int idxSet, Str s);
    PageOpResult InsertAt(int idxSet, Str s);
};

constexpr int kStrVecPageHdrSize = sizeofi(StrVecPage);

static int cbOffsetsSize(int nStrings, int dataSize) {
    ReportIf(dataSize % 4 != 0);
    int nOffsets = (2 * sizeofi(u32)) + dataSize;
    return nStrings * nOffsets;
}

int StrVecPage::BytesLeft() {
    u8* start = (u8*)this;
    start += kStrVecPageHdrSize;
    int cbTotal = (int)(currEnd - start);
    int cbOffsets = cbOffsetsSize(nStrings, dataSize);
    auto res = cbTotal - cbOffsets;
    ReportIf(res < 0);
    return res;
}

static StrVecPage* AllocStrVecPage(int pageSize, int dataSize) {
    auto* page = (StrVecPage*)AllocZero(nullptr, pageSize);
    page->next = nullptr;
    page->nStrings = 0;
    page->pageSize = pageSize;
    page->dataSize = dataSize;
    u8* start = (u8*)page;
    page->currEnd = start + pageSize;
    return page;
}

// how many bytes per index entry with data
// index entry is offset and size (both u32) + (optional) data
static int cbIndexSize(int dataSize) {
    // dataSize is guaranteed multiple of sizeofi(u32)
    return (2 * sizeofi(u32)) + dataSize;
}

static u32* OffsetsForString(const StrVecPage* p, int idx) {
    ReportIf(idx < 0 || idx > p->nStrings);
    u8* off = (u8*)p;
    off += kStrVecPageHdrSize;
    off += (size_t)idx * cbIndexSize(p->dataSize);
    return (u32*)off;
}

// must have enough space
static Str AppendJustString(StrVecPage* p, Str s, int idx) {
    ReportIf(str::IsNull(s));
    int sLen = s.len;
    u32* offsets = OffsetsForString(p, idx);
    u8* dst = p->currEnd - sLen - 1; // 1 for zero termination
    u32 off = (u32)(dst - (u8*)p);
    offsets[0] = off;
    offsets[1] = (u32)sLen;
    memcpy(dst, s.s, (size_t)sLen);
    dst[sLen] = 0; // zero-terminate for C compat
    p->currEnd = dst;
    return Str((char*)dst, sLen);
}

PageOpResult StrVecPage::SetAt(int idx, Str s) {
    u32* offsets = OffsetsForString(this, idx);
    if (str::IsNull(s)) {
        // fast path for null, doesn't require new space at all
        offsets[0] = kNullOffset;
        offsets[1] = 0;
        return {};
    }
    int sLen = s.len;
    u32 off = offsets[0];
    u8* start = (u8*)this;
    if (off != kNullOffset) {
        // fast path for when new string is smaller than the current string
        int currLen = (int)offsets[1];
        if (sLen <= currLen) {
            auto* dst = start + off;
            memcpy(dst, s.s, (size_t)sLen);
            dst[sLen] = 0; // zero-terminate for C compat
            offsets[1] = (u32)sLen;
            return {Str((char*)dst, sLen), false};
        }
    }

    int cbNeeded = sLen + 1; // +1 for zero termination

    int cbLeft = BytesLeft();
    if (cbNeeded > cbLeft) {
        return PageOpResult::Fail();
    }
    return {AppendJustString(this, s, idx), false};
}

PageOpResult StrVecPage::InsertAt(int idx, Str s) {
    ReportIf(idx < 0 || idx > nStrings);

    int cbIndex = cbIndexSize(dataSize);
    int cbNeeded = cbIndex;
    if (!str::IsNull(s)) {
        cbNeeded += s.len + 1; // +1 for zero termination
    }
    int cbLeft = BytesLeft();
    if (cbNeeded > cbLeft) {
        return PageOpResult::Fail();
    }

    u32* offsets = OffsetsForString(this, idx);
    if (idx != nStrings) {
        // make space for idx
        u32* src = offsets;
        u32* dst = OffsetsForString(this, idx + 1);
        int nToCopy = (nStrings - idx) * cbIndex;
        memmove(dst, src, (size_t)nToCopy);
    }

    PageOpResult res;
    if (!str::IsNull(s)) {
        res = {AppendJustString(this, s, idx), false};
    } else {
        offsets[0] = kNullOffset;
        offsets[1] = 0;
    }
    nStrings++;
    return res;
}

PageOpResult StrVecPage::Append(Str s) {
    return InsertAt(nStrings, s);
}

Str StrVecPage::AtStr(int idx) const {
    ReportIf(idx >= nStrings);
    u8* start = (u8*)this;
    u32* offsets = OffsetsForString(this, idx);
    u32 off = offsets[0];
    int sLen = (int)offsets[1];
    if (off == kNullOffset) {
        ReportIf(sLen != 0);
        return {};
    }
    return Str((char*)(start + off), sLen);
}

void* StrVecPage::AtDataRaw(int idx) const {
    u32* offsets = OffsetsForString(this, idx) + 2;
    return (void*)offsets;
}

// we don't de-allocate removed strings so we can safely return the string
Str StrVecPage::RemoveAt(int idx) {
    ReportIf(nStrings <= 0 || idx >= nStrings);
    Str removed = AtStr(idx);
    nStrings--;
    int nToCopy = cbOffsetsSize(nStrings - idx, dataSize);
    if (nToCopy == 0) {
        // last string
        return removed;
    }
    u32* dst = OffsetsForString(this, idx);
    u32* src = OffsetsForString(this, idx + 1);
    memmove((void*)dst, (void*)src, (size_t)nToCopy);
    return removed;
}

// we don't de-allocate removed strings so we can safely return the string
Str StrVecPage::RemoveAtFast(int idx) {
    ReportIf(nStrings <= 0 || idx >= nStrings);
    Str removed = AtStr(idx);
    nStrings--;
    if (idx == nStrings) {
        // last string
        return removed;
    }
    // over-write idx with last string idx
    u32* dst = OffsetsForString(this, idx);
    u32* src = OffsetsForString(this, nStrings);
    int nToCopy = cbIndexSize(dataSize);
    memmove((void*)dst, (void*)src, (size_t)nToCopy);
    return removed;
}

static void FreePages(StrVecPage* toFree) {
    StrVecPage* next;
    while (toFree) {
        next = toFree->next;
        Free(nullptr, toFree);
        toFree = next;
    }
}

static StrVecPage* CompactStrVecPages(StrVecPage* first, int extraSize) {
    if (!first) {
        ReportIf(extraSize > 0);
        return nullptr;
    }
    int dataSize = first->dataSize;
    auto* curr = first;
    u8 *pageStart, *pageEnd;
    int cbStrings;
    int nStrings = 0;
    int cbStringsTotal = 0; // including 0-termination
    while (curr) {
        nStrings += curr->nStrings;
        pageStart = (u8*)curr;
        pageEnd = pageStart + curr->pageSize;
        cbStrings = (int)(pageEnd - curr->currEnd);
        cbStringsTotal += cbStrings;
        curr = curr->next;
    }
    // strLenTotal might be more than needed if we removed strings, but that's ok
    int pageSize = kStrVecPageHdrSize + cbOffsetsSize(nStrings, dataSize) + cbStringsTotal;
    if (extraSize > 0) {
        pageSize += extraSize;
    }
    pageSize = RoundUp(pageSize, 64); // jic
    auto* page = AllocStrVecPage(pageSize, dataSize);
    int n;
    Str s;
    curr = first;
    int nStr = 0;
    while (curr) {
        n = curr->nStrings;
        // TODO(perf): could optimize slightly
        for (int i = 0; i < n; i++) {
            s = curr->AtStr(i);
            page->Append(s);
            if (dataSize > 0) {
                void* dst = page->AtDataRaw(nStr);
                void* src = curr->AtDataRaw(i);
                memcpy(dst, src, (size_t)dataSize);
                nStr++;
            }
        }
        curr = curr->next;
    }
    return page;
}

static void CompactPages(StrVec* v, int extraSize) {
    auto* first = CompactStrVecPages(v->first, extraSize);
    FreePages(v->first);
    v->first = first;
    v->last = first;
    ReportIf(first && (v->size != first->nStrings));
}

static inline void InvalidateSortIndexes(StrVec* v) {
    if (v->sortIndexes) {
        Free(nullptr, v->sortIndexes);
        v->sortIndexes = nullptr;
    }
}

void StrVec::Reset(StrVecPage* initWith) {
    InvalidateSortIndexes(this);
    FreePages(first);
    first = nullptr;
    last = nullptr;
    nextPageSize = 256; // TODO: or leave it alone?
    size = 0;
    if (initWith == nullptr) {
        return;
    }
    first = CompactStrVecPages(initWith, 0);
    last = first;
    size = first->nStrings;
}

StrVec::StrVec(int dataSize) {
    if (dataSize == 0) {
        return;
    }
    this->dataSize = RoundUp(dataSize, sizeofi(u32));
}

StrVec::~StrVec() {
    Reset(nullptr);
}

// copies in logical order: for a SortIndex()-sorted source, page compaction
// alone would materialize the copy in physical (insertion) order and silently
// drop the sorted view
static void CopyStrVec(StrVec* v, const StrVec& that) {
    v->dataSize = that.dataSize;
    if (!that.sortIndexes) {
        v->Reset(that.first);
        return;
    }
    v->Reset(nullptr);
    int n = len(that);
    for (int i = 0; i < n; i++) {
        v->Append(that.At(i));
        if (v->dataSize > 0) {
            memcpy(v->AtDataRaw(i), that.AtDataRaw(i), (size_t)v->dataSize);
        }
    }
}

StrVec::StrVec(const StrVec& that) {
    CopyStrVec(this, that);
}

StrVec& StrVec::operator=(const StrVec& that) {
    if (this == &that) {
        return *this;
    }
    CopyStrVec(this, that);
    return *this;
}

bool StrVec::IsEmpty() const {
    return size == 0;
}

StrVecPage* StrVecPageNext(StrVecPage* page) {
    return page->next;
}

int StrVecPageSize(StrVecPage* page) {
    return page->nStrings;
}

// grow fast at first (256 -> 1K -> 4K), then double, capped at 64 kB
static int CalcNextPageSize(int currSize) {
    if (currSize >= 64 * 1024) {
        return currSize;
    }
    return std::min(currSize < 4 * 1024 ? currSize * 4 : currSize * 2, 64 * 1024);
}

static StrVecPage* AllocatePage(StrVec* v, StrVecPage* last, int nBytesNeeded) {
    int minPageSize = kStrVecPageHdrSize + nBytesNeeded;
    int pageSize = RoundUp(minPageSize, 8);
    if (pageSize < v->nextPageSize) {
        pageSize = v->nextPageSize;
        v->nextPageSize = CalcNextPageSize(v->nextPageSize);
    }
    auto* page = AllocStrVecPage(pageSize, v->dataSize);
    if (last) {
        ReportIf(!v->first);
        last->next = page;
    } else {
        ReportIf(v->first);
        v->first = page;
    }
    v->last = page;
    return page;
}

Str StrVec::Append(Str s) {
    int cbIndex = cbIndexSize(dataSize);
    int cbNeeded = cbIndex;
    if (!str::IsNull(s)) {
        cbNeeded += (s.len + 1); // +1 for zero termination
    }
    auto* page = last;
    if (!page || page->BytesLeft() < cbNeeded) {
        page = AllocatePage(this, page, cbNeeded);
    }
    auto res = page->Append(s);
    ReportIf(res.noSpace);
    size++;
    InvalidateSortIndexes(this);
    return res.s;
}

Str StrVec::AppendNonEmpty(Str s) {
    if (len(s) == 0) {
        return {};
    }
    return Append(s);
}

// returns index of inserted string, -1 if not inserted
int AppendIfNotExists(StrVec* v, Str s) {
    if (v->Contains(s)) {
        return -1;
    }
    int idx = len(*v);
    v->Append(s);
    return idx;
}

static StrVecPage* PageForIdx(const StrVec* v, int idx, int* idxInPageOut) {
    auto* page = v->first;
    while (page) {
        if (page->nStrings > idx) {
            *idxInPageOut = idx;
            return page;
        }
        idx -= page->nStrings;
        page = page->next;
    }
    *idxInPageOut = 0;
    return page;
}

// callers use logical (sorted) indexes, pages use physical order
static int PhysIdx(const StrVec* v, int idx) {
    return v->sortIndexes ? v->sortIndexes[idx] : idx;
}

// SetAt / InsertAt: try the page holding idx first; when it has no room,
// compact all pages with extra space (assuming more calls will follow) and
// retry on the single page that leaves.
static Str SetOrInsertAt(StrVec* v, int idx, Str s, PageOpResult (StrVecPage::*op)(int, Str)) {
    idx = PhysIdx(v, idx);
    int idxInPage;
    auto* page = PageForIdx(v, idx, &idxInPage);
    auto res = (page->*op)(idxInPage, s);
    if (res.noSpace) {
        // s might point into one of our pages, which CompactPages() frees
        if (!str::IsNull(s)) {
            s = str::DupTemp(s);
        }
        CompactPages(v, RoundUp(s.len + 1, 2048));
        res = (v->first->*op)(idx, s);
        ReportIf(res.noSpace);
    }
    InvalidateSortIndexes(v);
    return res.s;
}

// returns a string
// note: this might invalidate previously returned strings because
// it might re-allocate memory used for those strings
Str StrVec::SetAt(int idx, Str s) {
    return SetOrInsertAt(this, idx, s, &StrVecPage::SetAt);
}

// returns a string
// note: this might invalidate previously returned strings because
// it might re-allocate memory used for those strings
Str StrVec::InsertAt(int idx, Str s) {
    if (idx == size) {
        return Append(s);
    }
    Str res = SetOrInsertAt(this, idx, s, &StrVecPage::InsertAt);
    size++;
    return res;
}

static Str RemoveAtHelper(StrVec* v, int idx, Str (StrVecPage::*op)(int)) {
    int idxInPage;
    auto* page = PageForIdx(v, PhysIdx(v, idx), &idxInPage);
    Str removed = page->AtStr(idxInPage);
    (page->*op)(idxInPage);
    v->size--;
    InvalidateSortIndexes(v);
    return removed;
}

Str StrVec::RemoveAt(int idx) {
    return RemoveAtHelper(this, idx, &StrVecPage::RemoveAt);
}

Str StrVec::RemoveAtFast(int idx) {
    return RemoveAtHelper(this, idx, &StrVecPage::RemoveAtFast);
}

static int FindHelper(const StrVec* v, Str s, int startAt, bool (*eq)(Str, Str)) {
    if (startAt < 0 || startAt >= v->size) {
        return -1;
    }
    auto end = v->end();
    for (auto it = v->begin() + startAt; it != end; it++) {
        Str s2 = *it;
        if (s2.len == s.len && eq(s, s2)) {
            return it.idx;
        }
    }
    return -1;
}

int StrVec::Find(Str s, int startAt) const {
    return FindHelper(this, s, startAt, str::Eq);
}

int StrVec::FindI(Str s, int startAt) const {
    return FindHelper(this, s, startAt, str::EqI);
}

// return true if did remove
bool StrVec::Remove(Str s) {
    int idx = Find(s);
    if (idx >= 0) {
        RemoveAt(idx);
        return true;
    }
    return false;
}

Str StrVec::At(int idx) const {
    int idxInPage;
    auto* page = PageForIdx(this, PhysIdx(this, idx), &idxInPage);
    return page->AtStr(idxInPage);
}

void* StrVec::AtDataRaw(int idx) const {
    ReportIf(dataSize == 0); // shouldn't call
    int idxInPage;
    auto* page = PageForIdx(this, PhysIdx(this, idx), &idxInPage);
    return page->AtDataRaw(idxInPage);
}

Str StrVec::operator[](int idx) const {
    ReportIf(idx < 0);
    return At(idx);
}

bool StrVec::Contains(Str s) const {
    int idx = Find(s);
    return idx != -1;
}

StrVec::iterator::iterator(const StrVec* v, int idx) {
    this->v = v;
    this->idx = idx;
    if (this->v->sortIndexes) {
        return;
    }
    int idxInPage;
    auto* page = PageForIdx(v, idx, &idxInPage);
    this->page = page;
    this->idxInPage = idxInPage;
}

StrVec::iterator StrVec::begin() const {
    return {this, 0};
}

StrVec::iterator StrVec::end() const {
    return {this, len(*this)};
}

Str StrVec::iterator::operator*() const {
    if (this->v->sortIndexes) {
        return v->At(idx);
    }
    return page->AtStr(idxInPage);
}

static void AdvanceStrVecIter(StrVec::iterator& it, int n) {
    if (it.v->sortIndexes) {
        it.idx += n;
        return;
    }
    // TODO: optimize for n > 1
    for (int i = 0; i < n; i++) {
        it.idx++;
        if (!it.page) {
            // advanced past the end
            continue;
        }
        it.idxInPage++;
        if (it.idxInPage >= it.page->nStrings) {
            it.idxInPage = 0;
            it.page = it.page->next;
            while (it.page && it.page->nStrings == 0) {
                it.page = it.page->next;
            }
        }
    }
}

// postfix increment
StrVec::iterator StrVec::iterator::operator++(int) {
    auto res = *this;
    AdvanceStrVecIter(*this, 1);
    return res;
}

StrVec::iterator& StrVec::iterator::operator++() {
    AdvanceStrVecIter(*this, 1);
    return *this;
}

StrVec::iterator StrVec::iterator::operator+(int n) const {
    iterator res = *this;
    AdvanceStrVecIter(res, n);
    return res;
}

bool operator==(const StrVec::iterator& a, const StrVec::iterator& b) {
    return (a.v == b.v) && (a.idx == b.idx);
};

bool operator!=(const StrVec::iterator& a, const StrVec::iterator& b) {
    return (a.v != b.v) || (a.idx != b.idx);
};

static void SortNoData(StrVec* v, StrLessFunc lessFn) {
    CompactPages(v, 0);
    if (len(*v) < 2) {
        return;
    }
    ReportIf(!v->first);
    ReportIf(v->first->next);
    int n = len(*v);

    u8* pageStart = (u8*)v->first;
    u64* b = (u64*)(pageStart + kStrVecPageHdrSize);
    u64* e = b + n;
    std::sort(b, e, [pageStart, lessFn](u64 offLen1, u64 offLen2) -> bool {
        u32 off1 = (u32)(offLen1 & 0xffffffff);
        u32 off2 = (u32)(offLen2 & 0xffffffff);
        int len1 = (int)(offLen1 >> 32);
        int len2 = (int)(offLen2 >> 32);
        Str s1 = (off1 == kNullOffset) ? Str{} : Str((char*)(pageStart + off1), len1);
        Str s2 = (off2 == kNullOffset) ? Str{} : Str((char*)(pageStart + off2), len2);
        bool ret = lessFn(s1, s2);
        return ret;
    });
}

static int* AllocateSortIndexes(StrVec* v) {
    InvalidateSortIndexes(v);
    int n = len(*v);
    auto* res = AllocArray<int>(n);
    for (int i = 0; i < n; i++) {
        res[i] = i;
    }
    return res;
}

static void SortIndex(StrVec* v, StrLessFunc lessFn) {
    if (len(*v) < 2) {
        return;
    }
    int* indexes = AllocateSortIndexes(v);
    int n = len(*v);
    int* b = indexes;
    int* e = indexes + n;
    std::sort(b, e, [v, lessFn](int idx1, int idx2) -> bool {
        Str s1 = v->At(idx1);
        Str s2 = v->At(idx2);
        bool ret = lessFn(s1, s2);
        return ret;
    });
    v->sortIndexes = indexes;
}

// null / empty string is smallest
bool StrLess(Str s1, Str s2) {
    return str::Cmp(s1, s2) < 0;
}

bool StrLessNoCase(Str s1, Str s2) {
    return str::CmpI(s1, s2) < 0;
}

void Sort(StrVec* v, StrLessFunc lessFn) {
    if (len(*v) < 2) {
        return;
    }
    if (v->dataSize == 0) {
        SortNoData(v, lessFn);
        return;
    }
    SortIndex(v, lessFn);
}

void SortNoCase(StrVec* v) {
    Sort(v, StrLessNoCase);
}

void SortNatural(StrVec* v) {
    Sort(v, StrLessNatural);
}

static bool reachedMax(int nAdded, int max) {
    if (max < 0) {
        return false;
    }
    if (max == 0) {
        max = 1;
    }
    return nAdded >= max;
}

/* splits a string into several substrings, separated by the separator
    (optionally collapsing several consecutive separators into one);
    e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
    (resp. "a", "b", "c" if separators are collapsed) */
int Split(StrVec* v, Str s, Str separator, bool collapse, int max) {
    int off = 0;
    int nAdded = 0;
    while (true) {
        if (reachedMax(nAdded, max)) {
            return nAdded;
        }
        Str rest = Str(s.s + off, s.len - off);
        int idx = str::IndexOf(rest, separator);
        if (idx < 0) {
            break;
        }
        if (!collapse || idx > 0) {
            nAdded++;
            if (reachedMax(nAdded, max)) {
                // this is the last one
                v->Append(rest);
                return nAdded;
            }
            v->Append(Str(rest.s, idx));
        }
        off += idx + separator.len;
    }
    bool shouldAddRest = true;
    if (off >= s.len) {
        // if we're collapsing, we're not adding empty string
        // at the end, unless we haven't added any strings yet
        // i.e. to match other languages, "".split(" ") => [""]
        shouldAddRest = !collapse || nAdded == 0;
    }
    if (shouldAddRest) {
        v->Append(Str(s.s + off, s.len - off));
        nAdded++;
    }
    return nAdded;
}

static int CalcCapForJoin(const StrVec* v, Str joint) {
    // it's ok to over-estimate
    int cap = 0;
    int jointLen = joint.len;
    for (auto s : *v) {
        cap += s.len + 1 + jointLen;
    }
    return cap + 32; // +32 arbitrary buffer
}

static void JoinInner(const StrVec* v, Str joint, str::Builder& res) {
    int jointLen = joint.len;
    int firstForJoint = 0;
    int i = 0;
    for (auto s : *v) {
        if (str::IsNull(s)) {
            firstForJoint++;
            i++;
            continue;
        }
        if (i > firstForJoint && jointLen > 0) {
            res.Append(joint);
        }
        res.Append(s);
        i++;
    }
}

Str Join(StrVec* v, Str sep) {
    str::Builder tmp;
    tmp.Reserve(CalcCapForJoin(v, sep));
    JoinInner(v, sep, tmp);
    return tmp.TakeStr();
}

TempStr JoinTemp(StrVec* v, Str sep) {
    str::Builder tmp(GetTempArena());
    tmp.Reserve(CalcCapForJoin(v, sep));
    JoinInner(v, sep, tmp);
    return ToStrTemp(tmp);
}

//--- Strconv.cpp ----------------------------------------------------------------

namespace strconv {

// null in => null out; empty in => allocated empty out
WStr CodePageToWStr(uint codePage, Str s, Arena* a) {
    if (str::IsNull(s)) {
        return {};
    }
    int cch = len(s) == 0 ? 0 : MultiByteToWideChar(codePage, 0, s.s, s.len, nullptr, 0);
    WCHAR* res = AllocArray<WCHAR>(a, cch + 1);
    if (!res) {
        return {};
    }
    if (cch > 0) {
        MultiByteToWideChar(codePage, 0, s.s, s.len, res, cch);
    }
    return WStr(res, cch);
}

Str WStrToCodePage(uint codePage, WStr s, Arena* a) {
    // subtle: if s.s is nullptr, we return empty. if empty string => we return empty string
    if (wstr::IsNull(s)) {
        return {};
    }
    int cb = len(s) == 0 ? 0 : WideCharToMultiByte(codePage, 0, s.s, s.len, nullptr, 0, nullptr, nullptr);
    char* res = AllocArray<char>(a, cb + 1);
    if (!res) {
        return {};
    }
    if (cb > 0) {
        WideCharToMultiByte(codePage, 0, s.s, s.len, res, cb, nullptr, nullptr);
    }
    return Str(res, cb);
}

// caller needs to free() the result
WStr StrCPToWStr(Str src, uint codePage) {
    return CodePageToWStr(codePage, src, nullptr);
}

TempWStr StrCPToWStrTemp(Str src, uint codePage) {
    return CodePageToWStr(codePage, src, GetTempArena());
}

TempStr ToMultiByteTemp(Str src, uint codePageSrc, uint codePageDest) {
    ReportIf(str::IsNull(src));
    if (str::IsNull(src)) {
        return {};
    }

    if (codePageSrc == codePageDest) {
        return str::DupTemp(src);
    }

    // 20127 is US-ASCII, which by definition is valid CP_UTF8
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
    // don't know what is CP_* name for it (if it exists)
    if ((codePageSrc == 20127) && (codePageDest == CP_UTF8)) {
        return str::DupTemp(src);
    }

    TempWStr tmp = StrCPToWStrTemp(src, codePageSrc);
    if (len(tmp) == 0) {
        return {};
    }
    Arena* a = GetTempArena();
    TempStr res = WStrToCodePage(codePageDest, tmp, a);
    return res;
}

TempStr StrToUtf8Temp(Str src, uint codePage) {
    return ToMultiByteTemp(src, codePage, CP_UTF8);
}

// tries to convert a string in unknown encoding to utf8, as best
// as it can
// caller has to free() it
TempStr UnknownToUtf8Temp(Str s) {
    if (s.len < 3) {
        return str::DupTemp(s);
    }

    if (str::TrimPrefix(s, Str(kUtf8Bom))) {
        return str::DupTemp(s);
    }

    if (str::TrimPrefix(s, Str(kUtf16Bom))) {
        WStr ws = str::CastStrToWStr(s);
        return ToUtf8Temp(ws);
    }

    if (str::TrimPrefix(s, Str(kUtf16BeBom))) {
        // convert from utf16 big endian to utf16
        WStr ws = str::CastStrToWStr(s);
        TempWStr tmpW = str::DupTemp(ws);
        int n = ws.len;
        u8* bytes = (u8*)tmpW.s;
        for (int i = 0; i < n; i++) {
            int idx = i * sizeofi(WCHAR);
            std::swap(bytes[idx], bytes[idx + 1]);
        }
        return ToUtf8Temp(WStr(tmpW.s, n));
    }

    // if s is valid utf8, leave it alone
    const u8* scan = (const u8*)s.s;
    const u8* end = scan + s.len;
    if (isLegalUTF8String(&scan, end)) {
        return str::DupTemp(s);
    }

    TempWStr ws = strconv::AnsiToWStrTemp(s);
    auto res = ToUtf8Temp(ws);
    return res;
}

TempWStr AnsiToWStrTemp(Str src) {
    return StrCPToWStrTemp(src, CP_ACP);
}

TempStr AnsiToUtf8Temp(Str src) {
    return ToUtf8Temp(AnsiToWStrTemp(src));
}

Str AnsiToUtf8(Str src) {
    return ToUtf8(AnsiToWStrTemp(src));
}

} // namespace strconv

Str ToUtf8(WStr s, Arena* a) {
    return strconv::WStrToCodePage(CP_UTF8, s, a);
}

WStr ToWStr(Str s, Arena* a) {
    return strconv::CodePageToWStr(CP_UTF8, s, a);
}

//--- Color.cpp ----------------------------------------------------------------

bool IsSpecialColor(Color col) {
    return col == kColorUnset || col == kColorNoChange || col == kColorTransparent;
}

// format: abgr
void UnpackColor(Color c, u8& r, u8& g, u8& b, u8& a) {
    r = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    b = (u8)(c & 0xff);
    c = c >> 8;
    a = (u8)(c & 0xff);
}

// format: bgr
void UnpackColor(Color c, u8& r, u8& g, u8& b) {
    u8 a;
    UnpackColor(c, r, g, b, a);
}

Gdiplus::Color GdiRgbFromColor(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    return {r, g, b};
}

TempStr SerializeColorTemp(Color c) {
    u8 r, g, b, a;
    UnpackColor(c, r, g, b, a);
    if (a > 0) {
        return fmt("#%02x%02x%02x%02x", a, r, g, b);
    }
    return fmt("#%02x%02x%02x", r, g, b);
}

void ParseColor(ParsedColor& parsed, Str txt) {
    if (parsed.wasParsed) {
        return;
    }
    parsed.wasParsed = true;
    parsed.parsedOk = false;
    if (len(txt) == 0) {
        return;
    }
    TempStr s = str::DupTemp(txt);
    str::TrimWSInPlace(s, str::TrimOpt::Both);
    if (str::EqI(s, StrL("checkered")) || str::EqI(s, StrL("unset"))) {
        parsed.col = kColorUnset;
        parsed.parsedOk = true;
        return;
    }
    if (!str::TrimPrefix(s, StrL("0x"))) {
        str::TrimPrefix(s, StrL("#"));
    }
    int n = len(s);
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;
    unsigned int a = 0;
    bool ok = n == 8 && !str::IsNull(str::Parse(s, "%2x%2x%2x%2x%$", &a, &r, &g, &b));
    if (ok) {
        parsed.col = MkRgba((u8)r, (u8)g, (u8)b, (u8)a);
        parsed.pdfCol = MkPdfColor((u8)r, (u8)g, (u8)b, (u8)a);
        parsed.parsedOk = true;
        return;
    }

    ok = n == 6 && !str::IsNull(str::Parse(s, "%2x%2x%2x%$", &r, &g, &b));
    if (!ok) {
        return;
    }
    parsed.col = MkRgb((u8)r, (u8)g, (u8)b);
    parsed.pdfCol = MkPdfColor((u8)r, (u8)g, (u8)b);
    parsed.parsedOk = true;
}

/* Parse 's' as hex color and return the result in 'destColor' */
void ParseColor(ParsedColor& parsed) {
    ParseColor(parsed, parsed.s);
}

// the cached parse belongs to the old text, so it has to go with it
void SetColorText(ParsedColor& parsed, Str txt) {
    str::ReplaceWithCopy(&parsed.s, txt);
    parsed.wasParsed = false;
    parsed.parsedOk = false;
}

void FreeColorText(ParsedColor& parsed) {
    str::Free(parsed.s);
    parsed.s = {};
    parsed.wasParsed = false;
    parsed.parsedOk = false;
}

bool ParseColor(Color* destColor, Str s) {
    ReportIf(!destColor);
    ParsedColor p;
    ParseColor(p, s);
    *destColor = p.col;
    return p.parsedOk;
}

void SerializePdfColor(PdfColor c, str::Builder& out) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    out.Append(fmt("#%02x%02x%02x", r, g, b));
}

Color ParseColor(Str s, Color defCol) {
    Color c;
    if (ParseColor(&c, s)) {
        return c;
    }
    return defCol;
}

// return argb
PdfColor MkPdfColor(u8 r, u8 g, u8 b, u8 a) {
    PdfColor b2 = (PdfColor)b;
    PdfColor g2 = (PdfColor)g << 8;
    PdfColor r2 = (PdfColor)r << 16;
    PdfColor a2 = (PdfColor)a << 24;
    return a2 | r2 | g2 | b2;
}

// argb
void UnpackPdfColor(PdfColor c, u8& r, u8& g, u8& b, u8& a) {
    b = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    r = (u8)(c & 0xff);
    c = c >> 8;
    a = (u8)(c & 0xff);
}

static Color AdjustLightness(Color c, float factor) {
    u8 R, G, B;
    UnpackColor(c, R, G, B);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Hue_and_chroma
    u8 M = std::max(std::max(R, G), B), m = std::min(std::min(R, G), B);
    if (M == m) {
        // for grayscale values, lightness is proportional to the color value
        u8 X = (u8)limitValue((int)floorf(((float)M * factor) + 0.5f), 0, 255);
        return MkRgb(X, X, X);
    }
    u8 C = M - m;
    int hueDiff;
    if (M == R) {
        hueDiff = G - B;
    } else if (M == G) {
        hueDiff = B - R;
    } else {
        hueDiff = R - G;
    }
    u8 Ha = (u8)abs(hueDiff);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Lightness
    float L2 = (float)(M + m);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Saturation
    float S = (float)C / (L2 > 255.0f ? 510.0f - L2 : L2);

    L2 = limitValue(L2 * factor, 0.0f, 510.0f);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#From_HSL
    float C1 = (L2 > 255.0f ? 510.0f - L2 : L2) * S;
    float X1 = C1 * (float)Ha / (float)C;
    float m1 = (L2 - C1) / 2;
    auto chromaOrX = [](bool isMax, bool isMin, float c1, float x1) -> float {
        if (isMax) {
            return c1;
        }
        if (!isMin) {
            return x1;
        }
        return 0.f;
    };
    R = (u8)floorf(chromaOrX(M == R, m == R, C1, X1) + m1 + 0.5f);
    G = (u8)floorf(chromaOrX(M == G, m == G, C1, X1) + m1 + 0.5f);
    B = (u8)floorf(chromaOrX(M == B, m == B, C1, X1) + m1 + 0.5f);
    return MkRgb(R, G, B);
}

// Adjusts lightness by 1/255 units.
Color AdjustLightness2(Color c, float units) {
    float lightness = GetLightness(c);
    units = limitValue(units, -lightness, 255.0f - lightness);
    if (0.0f == lightness) {
        u8 x = (u8)lroundf(units);
        return MkRgb(x, x, x);
    }
    return AdjustLightness(c, 1.0f + (units / lightness));
}

// http://en.wikipedia.org/wiki/HSV_color_space#Lightness
float GetLightness(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    u8 m1 = std::max(std::max(r, g), b);
    u8 m2 = std::min(std::min(r, g), b);
    return (float)(m1 + m2) / 2.0f;
}

// return true for light color, false for dark
// https://stackoverflow.com/questions/52879235/determine-color-lightness-via-rgb
bool IsLightColor(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    float y = (0.2126f * float(r)) + (0.7152f * float(g)) + (0.0722f * float(b));
    return y > 127.5f; // mid 256
}

bool IsNearBlack(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    return r < 10 && g < 10 && b < 10;
}

// Darken a light color, lighten a dark one, so the result stands out from `col`
// whatever the theme. `dark` defaults to `light` when 0.
// shift a color away from itself by `light` units when it's light, `dark` when
// it's dark (dark defaults to `light`), for hover / selected / accent states
Color AccentColor(Color col, int light, int dark) {
    if (dark == 0) {
        dark = light;
    }
    if (IsLightColor(col)) {
        return AdjustLightness2(col, (float)-light);
    }
    return AdjustLightness2(col, (float)dark);
}

// If `fg` is too close to `bg` in lightness, shift it away so muted labels
// (disabled buttons, secondary list text) stay readable. `minDelta` is in
// GetLightness units (0-255).
Color EnsureContrast(Color fg, Color bg, int minDelta) {
    if (minDelta <= 0 || IsSpecialColor(fg) || IsSpecialColor(bg)) {
        return fg;
    }
    float lf = GetLightness(fg);
    float lb = GetLightness(bg);
    float d = lf - lb;
    float ad = d < 0 ? -d : d;
    if (ad >= (float)minDelta) {
        return fg;
    }
    float sign = 1.f;
    if (ad > 0.5f) {
        sign = d > 0 ? 1.f : -1.f;
    } else if (IsLightColor(bg)) {
        sign = -1.f;
    }
    return AdjustLightness2(fg, sign * ((float)minDelta - ad));
}

DWORD PremultiplyPixel(Color c, u8 alpha) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    r = (u8)((r * alpha) / 255);
    g = (u8)((g * alpha) / 255);
    b = (u8)((b * alpha) / 255);
    return (alpha << 24) | (r << 16) | (g << 8) | b;
}

/* In debug mode, VS 2010 instrumentations complains about GetRValue() etc.
This adds equivalent functions that don't have this problem and ugly
substitutions to make sure we don't use Get*Value() in the future */
u8 GetRed(Color rgb) {
    rgb = rgb & 0xff;
    return (u8)rgb;
}

u8 GetGreen(Color rgb) {
    rgb = (rgb >> 8) & 0xff;
    return (u8)rgb;
}

u8 GetBlue(Color rgb) {
    rgb = (rgb >> 16) & 0xff;
    return (u8)rgb;
}

u8 GetAlpha(Color rgb) {
    rgb = (rgb >> 24) & 0xff;
    return (u8)rgb;
}

bool AtomicBoolGet(AtomicBool* p) {
    return InterlockedOr(p, 0) != 0;
}

void AtomicBoolSet(AtomicBool* p, bool v) {
    InterlockedExchange(p, v ? 1 : 0);
}

bool AtomicBoolSwap(AtomicBool* p, bool v) {
    return InterlockedExchange(p, v ? 1 : 0) != 0;
}

int AtomicIntGet(AtomicInt* p) {
    return (int)InterlockedOr(p, 0);
}

void AtomicIntSet(AtomicInt* p, int v) {
    InterlockedExchange(p, (LONG)v);
}

int AtomicIntAdd(AtomicInt* p, int v) {
    return (int)InterlockedAdd(p, (LONG)v);
}

int AtomicIntInc(AtomicInt* p) {
    return (int)InterlockedIncrement(p);
}

int AtomicIntDec(AtomicInt* p) {
    return (int)InterlockedDecrement(p);
}

// stores v and returns what was there before
void* AtomicPtrExchange(AtomicPtr* p, void* v) {
    return InterlockedExchangePointer(p, v);
}

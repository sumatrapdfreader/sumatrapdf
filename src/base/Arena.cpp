/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

u64 gArenaDefaultReserveSize = 64ull * 1024ull * 1024ull;
u64 gArenaDefaultCommitSize = 64ull * 1024ull;
ArenaFlags gArenaDefaultFlags = 0;

static u64 ArenaAlignPow2(u64 value, u64 align) {
    if (align <= 1) {
        return value;
    }
    ReportIf((align & (align - 1)) != 0);
    return (value + align - 1) & ~(align - 1);
}

static u64 ArenaMin(u64 a, u64 b) {
    return (a < b) ? a : b;
}

static u64 ArenaMax(u64 a, u64 b) {
    return (a > b) ? a : b;
}

static u64 ArenaClampTop(u64 value, u64 maxValue) {
    return (value < maxValue) ? value : maxValue;
}

static u64 ArenaClampBot(u64 minValue, u64 value) {
    return (value > minValue) ? value : minValue;
}

u64 ArenaPageSize();
u64 ArenaLargePageSize();
bool ArenaCommit(void* base, u64 size, bool largePages);
void* ArenaReserve(u64 size);
void* ArenaReserveAndCommit(u64 size, bool largePages);
void ArenaReleaseMemory(void* base, u64 size);

static void ArenaRelease(Arena* arena) {
    ArenaReleaseMemory(arena, arena->reserved);
}

static void* ArenaGetAvailableSpaceLocked(Arena* arena, int* bufSizeOut) {
    if (!bufSizeOut) {
        return nullptr;
    }

    Arena* current = arena ? arena->current : nullptr;
    if (!current) {
        *bufSizeOut = 0;
        return nullptr;
    }

    u64 pos = ArenaAlignPow2(current->pos, 8);
    if (pos >= current->committed) {
        *bufSizeOut = 0;
        return nullptr;
    }

    u64 available = current->committed - pos;
    available = std::min<u64>(available, 0x7fffffff);
    *bufSizeOut = (int)available;
    return (char*)current + pos;
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
        sizeToZero = ArenaMin(current->committed, posPost) - posPre;
    }

    if (current->reserved < posPost && !(arena->flags & ArenaFlagNoChain)) {
        u64 reserveChunkSize = current->reserveChunkSize;
        u64 commitChunkSize = current->commitChunkSize;
        if (size + kArenaHeaderSize > reserveChunkSize) {
            reserveChunkSize = ArenaAlignPow2(size + kArenaHeaderSize, ArenaMax(align, ArenaPageSize()));
            commitChunkSize = reserveChunkSize;
        }

        ArenaParams newParams = {};
        newParams.flags = current->flags;
        newParams.reserveSize = reserveChunkSize;
        newParams.commitSize = commitChunkSize;
        newParams.allocationSiteFile = current->allocationSiteFile;
        newParams.allocationSiteLine = current->allocationSiteLine;
        newParams.name = current->name;

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
        if (current->flags & ArenaFlagLargePages) {
            return nullptr;
        }

        u64 commitEnd = ArenaAlignPow2(posPost, current->commitChunkSize);
        u64 commitClamped = ArenaClampTop(commitEnd, current->reserved);
        u64 commitSize = commitClamped - current->committed;
        void* commitPtr = (char*)current + current->committed;
        if (!ArenaCommit(commitPtr, commitSize, false)) {
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
    ArenaParams params = {};
    params.flags = gArenaDefaultFlags;
    params.reserveSize = gArenaDefaultReserveSize;
    params.commitSize = gArenaDefaultCommitSize;
    return params;
}

Arena* ArenaNew(const ArenaParams& srcParams) {
    ArenaParams params = srcParams;
    if (params.reserveSize == 0) {
        params.reserveSize = gArenaDefaultReserveSize;
    }
    if (params.commitSize == 0) {
        params.commitSize = gArenaDefaultCommitSize;
    }

    bool useLargePages = (params.flags & ArenaFlagLargePages) != 0;
    const u64 pageSize = useLargePages ? ArenaLargePageSize() : ArenaPageSize();
    u64 reserveSize = ArenaAlignPow2(ArenaMax(params.reserveSize, kArenaHeaderSize), pageSize);
    u64 commitSize = ArenaAlignPow2(ArenaMax(params.commitSize, kArenaHeaderSize), pageSize);
    commitSize = ArenaClampTop(commitSize, reserveSize);

    void* base = params.optionalBackingBuffer;
    bool usesExternalBuffer = (base != nullptr);
    ArenaFlags actualFlags = params.flags;

    if (!usesExternalBuffer) {
        if (useLargePages) {
            base = ArenaReserveAndCommit(reserveSize, true);
            if (base) {
                commitSize = reserveSize;
            } else {
                actualFlags &= ~ArenaFlagLargePages;
                useLargePages = false;
                reserveSize = ArenaAlignPow2(reserveSize, ArenaPageSize());
                commitSize = ArenaAlignPow2(commitSize, ArenaPageSize());
            }
        }

        if (!base) {
            base = ArenaReserve(reserveSize);
            if (base && !ArenaCommit(base, commitSize, false)) {
                ArenaReleaseMemory(base, reserveSize);
                base = nullptr;
            }
        }
    } else {
        commitSize = reserveSize;
    }

    if (!base) {
        return nullptr;
    }

    memset(base, 0, (size_t)std::min<u64>(commitSize, kArenaHeaderSize));
    Arena* arena = (Arena*)base;
    arena->prev = nullptr;
    arena->current = arena;
    arena->flags = actualFlags;
    arena->commitChunkSize = useLargePages ? reserveSize : commitSize;
    arena->reserveChunkSize = reserveSize;
    arena->basePos = 0;
    arena->pos = kArenaHeaderSize;
    arena->committed = commitSize;
    arena->reserved = reserveSize;
    arena->allocationSiteFile = params.allocationSiteFile;
    arena->allocationSiteLine = params.allocationSiteLine;
    arena->name = params.name;
    arena->usesExternalBuffer = usesExternalBuffer;
    arena->nAllocsLifetime = 0;
    arena->peakBytesLifetime = 0;
    arena->nAllocsSinceReset = 0;
    arena->peakBytesSinceReset = 0;
    return arena;
}

void ArenaDelete(Arena* arena) {
    if (!arena) {
        return;
    }

    Arena* node = arena->current;
    while (node) {
        Arena* prev = node->prev;
        if (!node->usesExternalBuffer) {
            ArenaRelease(node);
        }
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
    Arena* arena = this;
    if (!arena) {
        return 0;
    }
    Arena* current = arena->current;
    return current->basePos + current->pos;
}

void Arena::PopTo(u64 pos) {
    Arena* arena = this;
    if (!arena) {
        return;
    }

    lock.Lock();

    u64 bigPos = ArenaClampBot(kArenaHeaderSize, pos);
    Arena* current = arena->current;
    while (current && current->basePos >= bigPos) {
        Arena* prev = current->prev;
        if (!current->usesExternalBuffer) {
            ArenaRelease(current);
        } else {
            current->pos = kArenaHeaderSize;
        }
        current = prev;
    }

    if (!current) {
        lock.Unlock();
        return;
    }

    arena->current = current;
    u64 newPos = bigPos - current->basePos;
    ReportIf(newPos > current->pos);
    current->pos = newPos;
    lock.Unlock();
}

void Arena::Pop(u64 amt) {
    u64 posOld = Pos();
    u64 posNew = (amt < posOld) ? (posOld - amt) : 0;
    PopTo(posNew);
}

ArenaSavepoint GetArenaSavepoint(Arena* arena) {
    ArenaSavepoint temp = {arena, arena ? arena->Pos() : 0};
    return temp;
}

void RestoreArenaSavepoint(ArenaSavepoint temp) {
    if (temp.arena) {
        temp.arena->PopTo(temp.pos);
    }
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

void* Arena::GetAvailableSpace(int* bufSizeOut) {
    if (!this) {
        if (bufSizeOut) {
            *bufSizeOut = 0;
        }
        return nullptr;
    }

    lock.Lock();
    void* mem = ArenaGetAvailableSpaceLocked(this, bufSizeOut);
    lock.Unlock();
    return mem;
}

void* Arena::CommitReserved(void* mem, int size) {
    if (size <= 0) {
        return nullptr;
    }

    lock.Lock();

    int availSize = 0;
    void* availMem = ArenaGetAvailableSpaceLocked(this, &availSize);
    if (mem == availMem && size <= availSize) {
        void* committed = ArenaPushLocked(this, (u64)size, 8, false);
        lock.Unlock();
        return committed;
    }

    void* dst = ArenaPushLocked(this, (u64)size, 8, false);
    lock.Unlock();
    if (!dst) {
        return nullptr;
    }
    if (mem) {
        memcpy(dst, mem, (size_t)size);
    }
    return dst;
}

// size_t overloads that match the legacy Allocator::* static helper API
// and fall back to malloc/free when arena is nullptr.
void* Alloc(Arena* arena, int size) {
    if (size <= 0) {
        return nullptr;
    }
    if (!arena) {
        return malloc(size);
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
        return malloc(size);
    }
    return arena->Push((u64)size, 8, false);
}

void* AllocZero(Arena* arena, size_t size) {
    if (size == 0) {
        return nullptr;
    }
    if (!arena) {
        void* mem = malloc(size);
        if (mem) {
            memset(mem, 0, size);
        }
        return mem;
    }
    return arena->Push((u64)size, 8, true);
}

void* Realloc(Arena* arena, void* mem, size_t newSize, size_t copySize) {
    if (!arena) {
        return realloc(mem, newSize);
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
    if (size == 0) {
        return {};
    }
    Arena* arena = GetTempArena();
    char* res = (char*)arena->Push((u64)size + 1, 1, false);
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

// Logs an arena's lifetime allocation count and peak bytes. Call on exit, before
// logging is torn down.
void LogArenaStats(Str what, Arena* a) {
    if (!a) {
        return;
    }
    u64 nAllocs = a->nAllocsLifetime;
    u64 peakBytes = a->peakBytesLifetime;
    char human[32];
    FormatSizeHumanIntoBuf(peakBytes, Str(human, sizeofi(human)));
    logf("%s lifetime: %s allocations, peak %s bytes (%s)\n", what, str::FormatNumWithThousandSepTemp((i64)nAllocs),
         str::FormatNumWithThousandSepTemp((i64)peakBytes), Str(human));
}

#include "common.h"

#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

u64 arena_default_reserve_size = 64ull * 1024ull * 1024ull;
u64 arena_default_commit_size = 64ull * 1024ull;
ArenaFlags arena_default_flags = 0;

static u64 ArenaAlignPow2(u64 value, u64 align) {
    if (align <= 1) {
        return value;
    }
    assert((align & (align - 1)) == 0);
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

static u64 ArenaPageSize() {
    static u64 pageSize = 0;
    if (pageSize == 0) {
        SYSTEM_INFO info = {};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
    }
    return pageSize;
}

static u64 ArenaLargePageSize() {
    static u64 largePageSize = 0;
    if (largePageSize == 0) {
        SIZE_T size = GetLargePageMinimum();
        largePageSize = size ? (u64)size : ArenaPageSize();
    }
    return largePageSize;
}

static bool ArenaCommit(void* base, u64 size, bool largePages) {
    if (size == 0) {
        return true;
    }
    DWORD flags = MEM_COMMIT;
    if (largePages) {
        flags |= MEM_LARGE_PAGES;
    }
    return VirtualAlloc(base, (SIZE_T)size, flags, PAGE_READWRITE) != nullptr;
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
    if (pos >= current->cmt) {
        *bufSizeOut = 0;
        return nullptr;
    }

    u64 available = current->cmt - pos;
    if (available > 0x7fffffff) {
        available = 0x7fffffff;
    }
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
    if (zero && current->cmt > posPre) {
        sizeToZero = ArenaMin(current->cmt, posPost) - posPre;
    }

    if (current->res < posPost && !(arena->flags & ArenaFlag_NoChain)) {
        u64 resSize = current->res_size;
        u64 cmtSize = current->cmt_size;
        if (size + ARENA_HEADER_SIZE > resSize) {
            resSize = ArenaAlignPow2(size + ARENA_HEADER_SIZE, ArenaMax(align, ArenaPageSize()));
            cmtSize = resSize;
        }

        ArenaParams newParams = {};
        newParams.flags = current->flags;
        newParams.reserve_size = resSize;
        newParams.commit_size = cmtSize;
        newParams.allocation_site_file = current->allocation_site_file;
        newParams.allocation_site_line = current->allocation_site_line;
        newParams.name = current->name;

        Arena* newBlock = ArenaNew(newParams);
        if (!newBlock) {
            return nullptr;
        }

        newBlock->base_pos = current->base_pos + current->res;
        newBlock->prev = current;
        arena->current = newBlock;
        current = newBlock;
        posPre = ArenaAlignPow2(current->pos, align);
        posPost = posPre + size;
        sizeToZero = 0;
    }

    if (current->cmt < posPost) {
        if (current->flags & ArenaFlag_LargePages) {
            return nullptr;
        }

        u64 commitEnd = ArenaAlignPow2(posPost, current->cmt_size);
        u64 commitClamped = ArenaClampTop(commitEnd, current->res);
        u64 commitSize = commitClamped - current->cmt;
        void* commitPtr = (char*)current + current->cmt;
        if (!ArenaCommit(commitPtr, commitSize, false)) {
            return nullptr;
        }
        current->cmt = commitClamped;
    }

    if (current->cmt < posPost) {
        return nullptr;
    }

    void* result = (char*)current + posPre;
    current->pos = posPost;
    if (sizeToZero) {
        memset(result, 0, (size_t)sizeToZero);
    }
    return result;
}

ArenaParams ArenaDefaultParams() {
    ArenaParams params = {};
    params.flags = arena_default_flags;
    params.reserve_size = arena_default_reserve_size;
    params.commit_size = arena_default_commit_size;
    return params;
}

Arena* ArenaNew(const ArenaParams& srcParams) {
    ArenaParams params = srcParams;
    if (params.reserve_size == 0) {
        params.reserve_size = arena_default_reserve_size;
    }
    if (params.commit_size == 0) {
        params.commit_size = arena_default_commit_size;
    }

    bool useLargePages = (params.flags & ArenaFlag_LargePages) != 0;
    const u64 pageSize = useLargePages ? ArenaLargePageSize() : ArenaPageSize();
    u64 reserveSize = ArenaAlignPow2(ArenaMax(params.reserve_size, ARENA_HEADER_SIZE), pageSize);
    u64 commitSize = ArenaAlignPow2(ArenaMax(params.commit_size, ARENA_HEADER_SIZE), pageSize);
    commitSize = ArenaClampTop(commitSize, reserveSize);

    void* base = params.optional_backing_buffer;
    bool usesExternalBuffer = (base != nullptr);
    ArenaFlags actualFlags = params.flags;

    if (!usesExternalBuffer) {
        if (useLargePages) {
            base = VirtualAlloc(nullptr, (SIZE_T)reserveSize, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
            if (base) {
                commitSize = reserveSize;
            } else {
                actualFlags &= ~ArenaFlag_LargePages;
                useLargePages = false;
                reserveSize = ArenaAlignPow2(reserveSize, ArenaPageSize());
                commitSize = ArenaAlignPow2(commitSize, ArenaPageSize());
            }
        }

        if (!base) {
            base = VirtualAlloc(nullptr, (SIZE_T)reserveSize, MEM_RESERVE, PAGE_READWRITE);
            if (base && !ArenaCommit(base, commitSize, false)) {
                VirtualFree(base, 0, MEM_RELEASE);
                base = nullptr;
            }
        }
    } else {
        commitSize = reserveSize;
    }

    if (!base) {
        return nullptr;
    }

    memset(base, 0, (size_t)std::min<u64>(commitSize, ARENA_HEADER_SIZE));
    Arena* arena = (Arena*)base;
    arena->prev = nullptr;
    arena->current = arena;
    arena->flags = actualFlags;
    arena->cmt_size = useLargePages ? reserveSize : commitSize;
    arena->res_size = reserveSize;
    arena->base_pos = 0;
    arena->pos = ARENA_HEADER_SIZE;
    arena->cmt = commitSize;
    arena->res = reserveSize;
    arena->allocation_site_file = params.allocation_site_file;
    arena->allocation_site_line = params.allocation_site_line;
    arena->name = params.name;
    arena->uses_external_buffer = usesExternalBuffer;
    arena->lock = SRWLOCK_INIT;
    return arena;
}

void ArenaDelete(Arena* arena) {
    if (!arena) {
        return;
    }

    Arena* node = arena->current;
    while (node) {
        Arena* prev = node->prev;
        if (!node->uses_external_buffer) {
            VirtualFree(node, 0, MEM_RELEASE);
        }
        node = prev;
    }
}

void* Arena::Push(u64 size, u64 align, bool zero) {
    if (!this) {
        return nullptr;
    }
    AcquireSRWLockExclusive(&lock);
    void* mem = ArenaPushLocked(this, size, align, zero);
    ReleaseSRWLockExclusive(&lock);
    return mem;
}

u64 Arena::Pos() {
    Arena* arena = this;
    if (!arena) {
        return 0;
    }
    Arena* current = arena->current;
    return current->base_pos + current->pos;
}

void Arena::PopTo(u64 pos) {
    Arena* arena = this;
    if (!arena) {
        return;
    }

    AcquireSRWLockExclusive(&lock);

    u64 bigPos = ArenaClampBot(ARENA_HEADER_SIZE, pos);
    Arena* current = arena->current;
    while (current && current->base_pos >= bigPos) {
        Arena* prev = current->prev;
        if (!current->uses_external_buffer) {
            VirtualFree(current, 0, MEM_RELEASE);
        } else {
            current->pos = ARENA_HEADER_SIZE;
        }
        current = prev;
    }

    if (!current) {
        ReleaseSRWLockExclusive(&lock);
        return;
    }

    arena->current = current;
    u64 newPos = bigPos - current->base_pos;
    assert(newPos <= current->pos);
    current->pos = newPos;
    ReleaseSRWLockExclusive(&lock);
}

void Arena::Pop(u64 amt) {
    u64 posOld = Pos();
    u64 posNew = (amt < posOld) ? (posOld - amt) : 0;
    PopTo(posNew);
}

ArenaSavepoint ArenaGetSavepoint(Arena* arena) {
    ArenaSavepoint temp = {arena, arena ? arena->Pos() : 0};
    return temp;
}

void ArenaRestoreSavepoint(ArenaSavepoint temp) {
    if (temp.arena) {
        temp.arena->PopTo(temp.pos);
    }
}

void* Arena::Alloc(int size) {
    if (size <= 0) {
        return nullptr;
    }
    return Push((u64)size, 8, false);
}

void Arena::Free(void* ptr) {
    (void)ptr;
}

void Arena::Reset() {
    PopTo(0);
}

void* Arena::GetAvailableSpace(int* bufSizeOut) {
    if (!this) {
        if (bufSizeOut) {
            *bufSizeOut = 0;
        }
        return nullptr;
    }

    AcquireSRWLockExclusive(&lock);
    void* mem = ArenaGetAvailableSpaceLocked(this, bufSizeOut);
    ReleaseSRWLockExclusive(&lock);
    return mem;
}

void* Arena::CommitReserved(void* mem, int size) {
    if (size <= 0) {
        return nullptr;
    }

    AcquireSRWLockExclusive(&lock);

    int availSize = 0;
    void* availMem = ArenaGetAvailableSpaceLocked(this, &availSize);
    if (mem == availMem && size <= availSize) {
        void* committed = ArenaPushLocked(this, (u64)size, 8, false);
        ReleaseSRWLockExclusive(&lock);
        return committed;
    }

    void* dst = ArenaPushLocked(this, (u64)size, 8, false);
    ReleaseSRWLockExclusive(&lock);
    if (!dst) {
        return nullptr;
    }
    if (mem) {
        memcpy(dst, mem, (size_t)size);
    }
    return dst;
}

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
    if (!mem) {
        return;
    }
    if (!arena) {
        free(mem);
        return;
    }
    arena->Free(mem);
}

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

void* Realloc(Arena* arena, void* mem, size_t size) {
    if (!arena) {
        return realloc(mem, size);
    }
    // Arena has no realloc: allocate fresh and copy. Old memory is not freed
    // (arena lifetime handles it).
    if (size == 0) {
        return nullptr;
    }
    void* newMem = arena->Push((u64)size, 8, false);
    if (newMem && mem) {
        // we don't know the old size; callers that end up here (Vec/StrBuilder)
        // only ever grow, and we can't overread because the arena block is
        // contiguous. Callers requiring exact copy must track old size.
        memcpy(newMem, mem, size);
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

void* AllocTemp(int size, u64 align) {
    Arena* arena = GetTempArena();
    return arena->Push((u64)size, align, false);
}

// allocate null-terminated string
Str AllocStrTemp(int size) {
    if (size == 0) {
        return Str();
    }
    Arena* arena = GetTempArena();
    char* res = (char*)arena->Push((u64)size+1, 1, false);
    res[size] = 0;
    return Str(res, size);
}

void* ReallocMem(Arena* arena, void* els, int* cap, int newCap, int elSize) {
    if (newCap <= 0 || elSize <= 0) {
        return els;
    }

    int newSize = newCap * elSize;
    if (!arena) {
        void* newEls = realloc(els, newSize);
        if (!newEls) {
            return els;
        }
        *cap = newCap;
        return newEls;
    }

    void* newEls = arena->Alloc(newSize);
    if (!newEls) {
        return els;
    }

    if (els && *cap > 0) {
        int oldSize = *cap * elSize;
        memcpy(newEls, els, oldSize);
    }

    *cap = newCap;
    return newEls;
}

void* ReallocToWantedSize(Arena* arena, void* els, int* cap, int wantedSize, int elSize) {
    int newCap = (*cap == 0) ? 8 : *cap * 2;
    while (newCap < wantedSize) {
        newCap *= 2;
    }
    return ReallocMem(arena, els, cap, newCap, elSize);
}

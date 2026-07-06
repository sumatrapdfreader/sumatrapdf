/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Reserve/commit arena allocator (implemented in Arena.cpp).
// Not self-sufficient: include after the part of utils/Base.h that defines
// u64 and pulls in <windows.h> / <utility>. Base.h includes this header.

#ifndef Arena_h
#define Arena_h

// Standalone reserve/commit arena
// 256 (not 128) to leave room in the header for the allocation stats below
static const u64 ARENA_HEADER_SIZE = 256;

typedef u64 ArenaFlags;
enum : ArenaFlags {
    ArenaFlag_NoChain = 1ull << 0,
    ArenaFlag_LargePages = 1ull << 1,
};

struct ArenaParams {
    ArenaFlags flags = 0;
    u64 reserve_size = 0;
    u64 commit_size = 0;
    void* optional_backing_buffer = nullptr;
    const char* allocation_site_file = nullptr;
    int allocation_site_line = 0;
    const char* name = nullptr;
};

struct Arena;

struct ArenaSavepoint {
    Arena* arena;
    u64 pos;
};

struct Arena {
    Arena* prev;    // Previous arena in chain
    Arena* current; // Current arena in chain
    ArenaFlags flags;
    u64 cmt_size;
    u64 res_size;
    u64 base_pos;
    u64 pos;
    u64 cmt;
    u64 res;
    const char* allocation_site_file;
    int allocation_site_line;
    const char* name;
    bool uses_external_buffer;
    Mutex lock;

    // allocation statistics, updated after every successful allocation
    // (see ArenaPushLocked). "peak bytes" is the high-water mark of total
    // bytes used by the arena (its position, including the header).
    u64 nAllocsLifetime;     // total allocations over the arena's whole life
    u64 peakBytesLifetime;   // largest total size the arena ever reached
    u64 nAllocsSinceReset;   // allocations since the last Reset()
    u64 peakBytesSinceReset; // largest total size reached since the last Reset()

    void* Alloc(int size);
    void Free(void* ptr);
    void Reset();
    void* Push(u64 size, u64 align = 8, bool zero = true);
    u64 Pos();
    void PopTo(u64 pos);
    void Pop(u64 amt);
    void* GetAvailableSpace(int* bufSizeOut);
    void* CommitReserved(void* mem, int size);

    Arena() = delete;  // use ArenaNew()
    ~Arena() = delete; // use ArenaDelete()
};

static_assert(sizeof(Arena) <= ARENA_HEADER_SIZE, "Arena header must fit in reserved header bytes");

extern u64 arena_default_reserve_size;
extern u64 arena_default_commit_size;
extern ArenaFlags arena_default_flags;

ArenaParams ArenaDefaultParams();
Arena* ArenaNew(const ArenaParams& params = ArenaDefaultParams());
void ArenaDelete(Arena* arena);

ArenaSavepoint ArenaGetSavepoint(Arena* arena);
void ArenaRestoreSavepoint(ArenaSavepoint temp);

// Thread-local temporary arena, reset after each message loop iteration
extern thread_local Arena* gTempArena;
Arena* GetTempArena();
void ResetTempArena();
void DestroyTempArena();

// Arena for allocations that live for the whole lifetime of the program (i.e.
// never freed until exit). Allocating them here avoids per-allocation frees and
// lets us track how much such memory we use (logged on exit). Never Reset().
extern Arena* gPermArena;
Arena* GetPermArena();
void DestroyPermArena();

template <typename T>
inline T* push_array_no_zero_aligned(Arena* arena, u64 count, u64 align) {
    return (T*)arena->Push(sizeof(T) * count, align, false);
}

template <typename T>
inline T* push_array_aligned(Arena* arena, u64 count, u64 align) {
    return (T*)arena->Push(sizeof(T) * count, align, true);
}

template <typename T>
inline T* push_array_no_zero(Arena* arena, u64 count) {
    return push_array_no_zero_aligned<T>(arena, count, (alignof(T) > 8) ? alignof(T) : 8);
}

template <typename T>
inline T* push_array(Arena* arena, u64 count) {
    return push_array_aligned<T>(arena, count, (alignof(T) > 8) ? alignof(T) : 8);
}

void* Alloc(struct Arena* arena, int size);
void Free(struct Arena* arena, void* mem);

// size_t overloads that match the legacy Allocator::* static helper API
// and fall back to malloc/free when arena is nullptr.
void* Alloc(struct Arena* arena, size_t size);
void* AllocZero(struct Arena* arena, size_t size);
void* Realloc(struct Arena* arena, void* mem, size_t newSize, size_t copySize);
void* MemDup(struct Arena* arena, const void* mem, size_t size, size_t extraBytes = 0);

template <typename T>
inline T* AllocArray(struct Arena* arena, int n = 1) {
    return (T*)AllocZero(arena, (size_t)n * sizeof(T));
}

// like AllocArray but in the thread-local temp arena (reset each message loop)
template <typename T>
inline T* AllocArrayTemp(int n = 1) {
    return AllocArray<T>(GetTempArena(), n);
}

void* AllocTemp(int size, u64 align = 8);

void* ReallocMem(struct Arena* arena, void* els, int* cap, int newCap, int elSize);
void* ReallocToWantedSize(struct Arena* arena, void* els, int* cap, int wantedSize, int elSize);

// Allocate and construct object using placement new (supports constructor args)
template <typename T, typename... Args>
T* New(Arena* arena, Args&&... args) {
    void* mem = Alloc(arena, (int)sizeof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

#endif // Arena_h

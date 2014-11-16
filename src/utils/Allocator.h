/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

// Base class for allocators that can be provided to Vec class
// (and potentially others). Needed because e.g. in crash handler
// we want to use Vec but not use standard malloc()/free() functions
class Allocator {
public:
    Allocator() {}
    virtual ~Allocator() { };
    virtual void *Alloc(size_t size) = 0;
    virtual void *Realloc(void *mem, size_t size) = 0;
    virtual void Free(void *mem) = 0;

    // helper functions that fallback to malloc()/free() if allocator is NULL
    // helps write clients where allocator is optional
    static void *Alloc(Allocator *a, size_t size) {
        if (!a)
            return malloc(size);
        return a->Alloc(size);
    }

    template <typename T>
    static T *Alloc(Allocator *a, size_t n=1) {
        size_t size = n * sizeof(T);
        return (T *)AllocZero(a, size);
    }

    static void *AllocZero(Allocator *a, size_t size) {
        void *m = Alloc(a, size);
        if (m)
            ZeroMemory(m, size);
        return m;
    }

    static void Free(Allocator *a, void *p) {
        if (!a)
            free(p);
        else
            a->Free(p);
    }

    static void* Realloc(Allocator *a, void *mem, size_t size) {
        if (!a)
            return realloc(mem, size);
        return a->Realloc(mem, size);
    }

    static void *Dup(Allocator *a, const void *mem, size_t size, size_t padding=0) {
        void *newMem = Alloc(a, size + padding);
        if (newMem)
            memcpy(newMem, mem, size);
        return newMem;
    }

    static char *StrDup(Allocator *a, const char *str) {
        return str ? (char *)Dup(a, str, strlen(str) + 1) : NULL;
    }
    static WCHAR *StrDup(Allocator *a, const WCHAR *str) {
        return str ? (WCHAR *)Dup(a, str, (wcslen(str) + 1) * sizeof(WCHAR)) : NULL;
    }
};

// PoolAllocator is for the cases where we need to allocate pieces of memory
// that are meant to be freed together. It simplifies the callers (only need
// to track this object and not all allocated pieces). Allocation and freeing
// is faster. The downside is that free() is a no-op i.e. it can't free memory
// for re-use.
//
// Note: we could be a bit more clever here by allocating data in 4K chunks
// via VirtualAlloc() etc. instead of malloc(), which would lower the overhead
class PoolAllocator : public Allocator {

    // we'll allocate block of the minBlockSize unless
    // asked for a block of bigger size
    size_t  minBlockSize;
    size_t  allocRounding;

    struct MemBlockNode {
        struct MemBlockNode *next;
        size_t               size;
        size_t               free;

        size_t               Used() { return size - free; }
        char *               DataStart() { return (char*)this + sizeof(MemBlockNode); }
        // data follows here
    };

    MemBlockNode *  currBlock;
    MemBlockNode *  firstBlock;

    // iteration state
    MemBlockNode *  currIter;
    size_t          iterPos;

    void Init() {
        currBlock = NULL;
        firstBlock = NULL;
        allocRounding = 8;
    }

public:

    // Iterator for easily traversing allocated memory as array
    // of values of type T. The caller has to enforce the fact
    // that the values stored are indeed values of T
    // Meant to be used in this way:
    // for (T *el = allocator.IterStart<T>(); el; el = allocator.IterNext<T>()) { ... }
    template <typename T>
    T *IterStart() {
        currIter = firstBlock;
        iterPos = 0;
        // minimum check that the allocated values are of type T
        CrashIf(currIter && (currIter->Used() % sizeof(T) != 0));
        return IterNext<T>();
    }

    template <typename T>
    T* IterNext() {
        if (!currIter)
            return NULL;
        if (currIter->Used() == iterPos) {
            currIter = currIter->next;
            if (!currIter)
                return NULL;
        }
        T *elPtr = reinterpret_cast<T*>(currIter->DataStart() + iterPos);
        iterPos += sizeof(T);
        return elPtr;
    }

    explicit PoolAllocator(size_t rounding=8) : minBlockSize(4096),
        allocRounding(rounding), currIter(NULL), iterPos((size_t)-1) {
        Init();
    }

    void SetMinBlockSize(size_t newMinBlockSize) {
        CrashIf(currBlock); // can only be changed before first allocation
        minBlockSize = newMinBlockSize;
    }

    void SetAllocRounding(size_t newRounding) {
        CrashIf(currBlock); // can only be changed before first allocation
        allocRounding = newRounding;
    }

    void FreeAll() {
        MemBlockNode *curr = firstBlock;
        while (curr) {
            MemBlockNode *next = curr->next;
            free(curr);
            curr = next;
        }
        Init();
    }

    virtual ~PoolAllocator() {
        FreeAll();
    }

    void AllocBlock(size_t minSize) {
        minSize = RoundUp(minSize, allocRounding);
        size_t size = minBlockSize;
        if (minSize > size)
            size = minSize;
        MemBlockNode *node = (MemBlockNode*)calloc(1, sizeof(MemBlockNode) + size);
        CrashAlwaysIf(!node);
        if (!firstBlock)
            firstBlock = node;
        node->size = size;
        node->free = size;
        if (currBlock)
            currBlock->next = node;
        currBlock = node;
    }

    // Allocator methods
    virtual void *Realloc(void *mem, size_t size) {
        // TODO: we can't do that because we don't know the original
        // size of memory piece pointed by mem. We could remember it
        // within the block that we allocate
        CrashAlwaysIf(true);
        return NULL;
    }

    virtual void Free(void *mem) {
        // does nothing, we can't free individual pieces of memory
    }

    virtual void *Alloc(size_t size) {
        size = RoundUp(size, allocRounding);
        if (!currBlock || (currBlock->free < size))
            AllocBlock(size);

        void *mem = (void*)(currBlock->DataStart() + currBlock->Used());
        currBlock->free -= size;
        return mem;
    }

    // assuming allocated memory was for pieces of uniform size,
    // find the address of n-th piece
    void *FindNthPieceOfSize(size_t size, size_t n) const {
        size = RoundUp(size, allocRounding);
        MemBlockNode *curr = firstBlock;
        while (curr) {
            size_t piecesInBlock = curr->Used() / size;
            if (piecesInBlock > n) {
                char *p = (char*)curr + sizeof(MemBlockNode) + (n * size);
                    return (void*)p;
            }
            n -= piecesInBlock;
            curr = curr->next;
        }
        return NULL;
    }

    template <typename T>
    T *GetAtPtr(size_t idx) const {
        void *mem = FindNthPieceOfSize(sizeof(T), idx);
        return reinterpret_cast<T*>(mem);
    }

    // only valid for structs, could alloc objects with
    // placement new()
    template <typename T>
    T *AllocStruct() {
        return (T *)Alloc(sizeof(T));
    }
};

// A helper for allocating an array of elements of type T
// either on stack (if they fit within StackBufInBytes)
// or in memory. Allocating on stack is a perf optimization
// note: not the best name
template <typename T, int StackBufInBytes>
class FixedArray {
    T stackBuf[StackBufInBytes / sizeof(T)];
    T *memBuf;
public:
    explicit FixedArray(size_t elCount) {
        memBuf = NULL;
        size_t stackEls = StackBufInBytes / sizeof(T);
        if (elCount > stackEls)
            memBuf = (T*)malloc(elCount * sizeof(T));
    }

    ~FixedArray() {
        free(memBuf);
    }

    T *Get() {
        if (memBuf)
            return memBuf;
        return &(stackBuf[0]);
    }
};

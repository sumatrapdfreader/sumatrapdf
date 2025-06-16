/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef BaseUtil_h
#define BaseUtil_h

/* OS_DARWIN - Any Darwin-based OS, including Mac OS X and iPhone OS */
#ifdef __APPLE__
#define OS_DARWIN 1
#else
#define OS_DARWIN 0
#endif

/* OS_LINUX - Linux */
#ifdef __linux__
#define OS_LINUX 1
#else
#define OS_LINUX 0
#endif

#if defined(WIN32) || defined(_WIN32)
#define OS_WIN 1
#else
#define OS_WIN 0
#endif

// https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros
#if defined(_M_IX86) || defined(__i386__)
#define IS_INTEL_32 1
#define IS_INTEL_64 0
#define IS_ARM_64 0
#elif defined(_M_X64) || defined(__x86_64__)
#define IS_INTEL_64 1
#define IS_INTEL_32 0
#define IS_ARM_64 0
#elif defined(_M_ARM64)
#define IS_INTEL_64 0
#define IS_INTEL_32 0
#define IS_ARM_64 1
#else
#error "unsupported arch"
#endif

/* OS_UNIX - Any Unix-like system */
#if OS_DARWIN || OS_LINUX || defined(unix) || defined(__unix) || defined(__unix__)
#define OS_UNIX 1
#endif

#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#define COMPILER_MSVC 0
#endif

#if defined(__GNUC__)
#define COMPILER_GCC 1
#else
#define COMPILER_GCC 0
#endif

#if defined(__clang__)
#define COMPILER_CLANG 1
#else
#define COMPILER_CLAGN 0
#endif

#if defined(__MINGW32__)
#define COMPILER_MINGW 1
#else
#define COMPILER_MINGW 0
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// Windows headers use _unused
#define __unused [[maybe_unused]]

#include "BuildConfig.h"

#define NOMINMAX
#include <winsock2.h> // must include before <windows.h>
#include <windows.h>
#include <ws2def.h>
#include <unknwn.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include <winsafer.h>
#include <wininet.h>
#include <versionhelpers.h>
#include <tlhelp32.h>

// nasty but necessary
#if defined(min) || defined(max)
#error "min or max defined"
#endif
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#include <gdiplus.h>
#undef NOMINMAX
#undef min
#undef max

#include <io.h>

// Most common C includes
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <locale.h>
#include <errno.h>

#include <fcntl.h>

#define _USE_MATH_DEFINES
#include <math.h>

// most common c++ includes
#include <cstdint>
#include <algorithm>
#include <memory>
#include <string>
#include <array>
#include <limits>

using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using uint = unsigned int;

// TODO: don't use INT_MAX and UINT_MAX
#ifndef INT_MAX
#define INT_MAX std::numeric_limits<int>::max()
#endif

#ifndef UINT_MAX
#define UINT_MAX std::numeric_limits<unsigned int>::max()
#endif

#if COMPILER_MSVC
#define NO_INLINE __declspec(noinline)
#else
// assuming gcc or similar
#define NO_INLINE __attribute__((noinline))
#endif

#define NoOp() ((void)0)
#define dimof(array) (sizeof(DimofSizeHelper(array)))
#define dimofi(array) (int)(sizeof(DimofSizeHelper(array)))

template <typename T, size_t N>
char (&DimofSizeHelper(T (&array)[N]))[N];

// like dimof minus 1 to account for terminating 0
#define static_strlen(array) (sizeof(DimofSizeHelper(array)) - 1)

#if COMPILER_MSVC
// https://msdn.microsoft.com/en-us/library/4dt9kyhy.aspx
// enable msvc equivalent of -Wundef gcc option, warns when doing "#if FOO" and FOO is not defined
// can't be turned on globally because windows headers have those
#pragma warning(default : 4668)
#endif

// TODO: is there a better way?
#if COMPILER_MSVC
#define IS_UNUSED
#else
#define IS_UNUSED __attribute__((unused))
#endif

// __analysis_assume is defined by msvc for prefast analysis
#if !defined(__analysis_assume)
#define __analysis_assume(x)
#endif

#if COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 6011) // silence /analyze: de-referencing a nullptr pointer
#endif
// Note: it's inlined to make it easier on crash reports analyzer (if wasn't inlined
// CrashMe() would show up as the cause of several different crash sites)
//
// Note: I tried doing this via RaiseException(0x40000015, EXCEPTION_NONCONTINUABLE, 0, 0);
// but it seemed to confuse callstack walking
inline void CrashMe() {
    char* p = nullptr;
    // cppcheck-suppress nullPointer
    *p = 0; // NOLINT
}
#if COMPILER_MSVC
#pragma warning(pop)
#endif

// ReportIf() is like assert() except it sends crash report in pre-release and debug
// builds.
// The idea is that assert() indicates "can't possibly happen" situation and if
// it does happen, we would like to fix the underlying cause.
// In practice in our testing we rarely get notified when an assert() is triggered
// and they are disabled in builds running on user's computers.
//
// ReportAlwaysIf() sends a report even in release builds. This is to catch the most
// thorny scenarios.
// Enabling it in pre-release builds but not in release builds is trade-off between
// shipping small executables (each ReportIf() adds few bytes of code) and having
// more testing on user's machines and not only in our personal testing.
// To crash uncoditionally use ReportIf(). It should only be used in
// rare cases where we really want to know a given condition happens. Before
// each release we should audit the uses of ReportAlwaysIf()

extern void _uploadDebugReport(const char*, bool, bool);

#define ReportIfCond(cond, condStr, isCrash, captureCallstack)      \
    __analysis_assume(!(cond));                                     \
    do {                                                            \
        if (cond) {                                                 \
            _uploadDebugReport(condStr, isCrash, captureCallstack); \
        }                                                           \
    } while (0)

#define ReportIf(cond) ReportIfCond(cond, #cond, false, true)
#define ReportIfQuick(cond) ReportIfCond(cond, #cond, false, false)
#if defined(DEBUG)
#define ReportDebugIf(cond) ReportIfCond(cond, #cond, false, true)
#else
#define ReportDebugIf(cond)
#endif

void* AllocZero(size_t count, size_t size);

template <typename T>
FORCEINLINE T* AllocArray(size_t n) {
    return (T*)AllocZero(n, sizeof(T));
}

template <typename T>
FORCEINLINE T* AllocStruct() {
    return (T*)AllocZero(1, sizeof(T));
}

template <typename T>
inline void ZeroStruct(T* s) {
    ZeroMemory((void*)s, sizeof(T));
}

template <typename T>
inline void ZeroArray(T& a) {
    size_t size = sizeof(a);
    ZeroMemory((void*)&a, size);
}

int limitValue(int val, int min, int max);
DWORD limitValue(DWORD val, DWORD min, DWORD max);
float limitValue(float val, float min, float max);

// return true if adding n to val overflows. Only valid for n > 0
template <typename T>
inline bool addOverflows(T val, T n) {
    ReportIf(!(n > 0));
    T res = val + n;
    return val > res;
}

void* memdup(const void* data, size_t len, size_t extraBytes = 0);
bool memeq(const void* s1, const void* s2, size_t len);

size_t RoundToPowerOf2(size_t size);
u32 MurmurHash2(const void* key, size_t len);
u32 MurmurHashWStrI(const WCHAR*);
u32 MurmurHashStrI(const char*);

size_t RoundUp(size_t n, size_t rounding);
int RoundUp(int n, int rounding);
char* RoundUp(char*, int rounding);

template <typename T>
void ListDelete(T* root) {
    T* next;
    T* curr = root;
    while (curr) {
        next = curr->next;
        delete curr;
        curr = next;
    }
}

template <typename T>
void ListInsertFront(T** root, T* el) {
    el->next = *root;
    *root = el;
}

template <typename T>
void ListInsertEnd(T** root, T* el) {
    el->next = nullptr;
    if (!*root) {
        *root = el;
        return;
    }
    T** prevPtr = root;
    T** currPtr = root;
    T* curr;
    while (*currPtr) {
        prevPtr = currPtr;
        curr = *currPtr;
        currPtr = &(curr->next);
    }
    T* prev = *prevPtr;
    prev->next = el;
}

template <typename T>
void ListReverse(T** root) {
    T* newRoot = nullptr;
    T* next;
    T* el = *root;
    while (el) {
        next = el->next;
        el->next = newRoot;
        newRoot = el;
        el = next;
    }
    *root = newRoot;
}

template <typename T>
bool ListRemove(T** root, T* el) {
    T** currPtr = root;
    T* curr;
    for (;;) {
        curr = *currPtr;
        if (!curr) {
            return false;
        }
        if (curr == el) {
            break;
        }
        currPtr = &(curr->next);
    }
    *currPtr = el->next;
    return true;
}

template <typename T>
int ListLen(T* root) {
    int n = 0;
    T* curr = root;
    while (curr) {
        n++;
        curr = curr->next;
    }
    return n;
}

// Base class for allocators that can be provided to Vec class
// (and potentially others). Needed because e.g. in crash handler
// we want to use Vec but not use standard malloc()/free() functions
struct Allocator {
    Allocator() = default;
    virtual ~Allocator() = default;

    virtual void* Alloc(size_t size) = 0;
    virtual void* Realloc(void* mem, size_t size) = 0;
    virtual void Free(const void* mem) = 0;

    // helper functions that fallback to malloc()/free() if allocator is nullptr
    // helps write clients where allocator is optional
    static void* Alloc(Allocator* a, size_t size);

    template <typename T>
    static T* AllocArray(Allocator* a, size_t n = 1) {
        size_t size = n * sizeof(T);
        return (T*)AllocZero(a, size);
    }

    static void* AllocZero(Allocator* a, size_t size);
    static void Free(Allocator* a, void* p);
    static void* Realloc(Allocator* a, void* mem, size_t size);
    static void* MemDup(Allocator* a, const void* mem, size_t size, size_t extraBytes = 0);
};

// PoolAllocator is for the cases where we need to allocate pieces of memory
// that are meant to be freed together. It simplifies the callers (only need
// to track this object and not all allocated pieces). Allocation and freeing
// is faster. The downside is that free() is a no-op i.e. it can't free memory
// for re-use.
//
// Note: we could be a bit more clever here by allocating data in 4K chunks
// via VirtualAlloc() etc. instead of malloc(), which would lower the overhead
struct PoolAllocator : Allocator {
    // we'll allocate block of the minBlockSize unless
    // asked for a block of bigger size
    size_t minBlockSize = 4096;

    // contains allocated data and index of each allocation
    struct Block {
        struct Block* next;
        size_t dataSize; // size of data in block
        size_t nAllocs;
        // curr points to free space
        char* freeSpace;
        // from the end, we store index of each allocation relative
        // to start of the block. <end> points at the current
        // reverse end of i32 array of indexes
        char* end;
        // data follows here
    };

    Block* currBlock = nullptr;
    Block* firstBlock = nullptr;
    int nAllocs = 0;
    CRITICAL_SECTION cs;

    PoolAllocator();

    // Allocator methods
    ~PoolAllocator() override;
    void* Realloc(void* mem, size_t size) override;
    void Free(const void*) override;
    void* Alloc(size_t size) override;

    void FreeAll();
    void Reset(bool poisonFreedMemory = false);
    void* At(int i);

    // only valid for structs, could alloc objects with
    // placement new()
    template <typename T>
    T* AllocStruct() {
        return (T*)Alloc(sizeof(T));
    }

    // Iterator for easily traversing allocated memory as array
    // of values of type T. The caller has to enforce the fact
    // that the values stored are indeed values of T
    // see http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    template <typename T>
    struct Iter {
        PoolAllocator* self;
        int idx;

        // TODO: can make it more efficient
        Iter(PoolAllocator* a, int startIdx) {
            self = a;
            idx = startIdx;
        }

        bool operator!=(const Iter& other) const {
            return idx != other.idx;
        }

        T* operator*() const {
            return (T*)self->At(idx);
        }

        Iter& operator++() {
            idx += 1;
            return *this;
        }
    };

    template <typename T>
    Iter<T> begin() {
        return Iter<T>(this, 0);
    }
    template <typename T>
    Iter<T> end() {
        return Iter<T>(this, nAllocs);
    }
};

struct HeapAllocator : Allocator {
    HANDLE allocHeap = nullptr;

    explicit HeapAllocator(size_t initialSize = 128 * 1024) : allocHeap(HeapCreate(0, initialSize, 0)) {
    }
    ~HeapAllocator() override {
        HeapDestroy(allocHeap);
    }
    void* Alloc(size_t size) override {
        return HeapAlloc(allocHeap, 0, size);
    }
    void* Realloc(void* mem, size_t size) override {
        return HeapReAlloc(allocHeap, 0, mem, size);
    }
    void Free(const void* mem) override {
        HeapFree(allocHeap, 0, (void*)mem);
    }

    HeapAllocator(const HeapAllocator&) = delete;
    HeapAllocator& operator=(const HeapAllocator&) = delete;
};

// A helper for allocating an array of elements of type T
// either on stack (if they fit within StackBufInBytes)
// or in memory. Allocating on stack is a perf optimization
// note: not the best name
template <typename T, int StackBufInBytes>
class FixedArray {
    T stackBuf[StackBufInBytes / sizeof(T)];
    T* memBuf;

  public:
    explicit FixedArray(size_t elCount) {
        memBuf = nullptr;
        size_t stackEls = StackBufInBytes / sizeof(T);
        if (elCount > stackEls) {
            memBuf = (T*)malloc(elCount * sizeof(T));
        }
    }

    ~FixedArray() {
        free(memBuf);
    }

    T* Get() {
        if (memBuf) {
            return memBuf;
        }
        return &(stackBuf[0]);
    }
};

/*
Poor-man's manual dynamic typing.
Identity of an object is an address of a unique, global string.
String is good for debugging

For classes / structs that we want to query for type at runtime, we add:

// in foo.h
struct Foo {
    Kind kind;
};

or:

struct Foo : KindBase {
};

extern Kind kindFoo;

// in foo.cpp
Kind kindFoo = "foo";
*/

using Kind = const char*;

struct KindBase {
    Kind kind;

    Kind GetKind() const {
        return kind;
    }
};

inline bool isOfKindHelper(Kind k1, Kind k2) {
    return k1 == k2;
}

#define IsOfKind(o, wantedKind) (o && isOfKindHelper(o->kind, wantedKind))

extern Kind kindNone; // unknown kind

// from https://pastebin.com/3YvWQa5c
// In my testing, in debug build defer { } creates somewhat bloated code
// but in release it seems to be optimized to optimally small code
#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

template <typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda) : lambda(lambda) { // NOLINT
    }
    ~ExitScope() {
        lambda();
    }
    ExitScope(const ExitScope&);

  private:
    ExitScope& operator=(const ExitScope&);
};

class ExitScopeHelp {
  public:
    template <typename T>
    ExitScope<T> operator+(T t) {
        return t;
    }
};

// it's 32-bit value which we cast to int for ease of use
struct AtomicInt {
    AtomicInt() = default;
    ~AtomicInt() = default;
    int Set(int n);
    int Inc();
    int Dec();
    int Add(int n);
    int Sub(int n);
    int Get() const;

  private:
    volatile LONG val = 0;
};

using func0Ptr = void (*)(void*);
using funcVoidPtr = void (*)();

#define kFuncNoArg (void*)-1

// the simplest possible function that ties a function and a single argument to it
// we get type safety and convenience with mkFunc()
struct Func0 {
    void* fn = nullptr;
    void* userData = nullptr;

    Func0() = default;
    // copy constructor
    Func0(const Func0& that) {
        this->fn = that.fn;
        this->userData = that.userData;
    }
    // copy assignment operator
    Func0& operator=(const Func0& that) {
        if (this != &that) {
            this->fn = that.fn;
            this->userData = that.userData;
        }
        return *this;
    }
    ~Func0() = default;

    bool IsEmpty() const {
        return fn == nullptr;
    }
    bool IsValid() const {
        return fn != nullptr;
    }
    void Call() const {
        if (!fn) {
            return;
        }
        if (userData == kFuncNoArg) {
            auto func = (funcVoidPtr)fn;
            func();
            return;
        }
        auto func = (func0Ptr)fn;
        func(userData);
    }
};
Func0 MkFunc0Void(funcVoidPtr fn);

template <typename T>
Func0 MkFunc0(void (*fn)(T*), T* d) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = (void*)d;
    return res;
}

template <typename T>
struct Func1 {
    void (*fn)(void*, T) = nullptr;
    void* userData = nullptr;

    Func1() = default;
    // copy constructor
    Func1(const Func1& that) {
        this->fn = that.fn;
        this->userData = that.userData;
    }
    // copy assignment operator
    Func1& operator=(const Func1& that) {
        if (this != &that) {
            this->fn = that.fn;
            this->userData = that.userData;
        }
        return *this;
    }
    ~Func1() = default;

    bool IsValid() const {
        return fn != nullptr;
    }
    bool IsEmpty() const {
        return fn == nullptr;
    }
    void Call(T arg) const {
        if (!fn) {
            return;
        }
        if (userData == kFuncNoArg) {
            using fptr = void (*)(T);
            auto func = (fptr)fn;
            func(arg);
            return;
        }
        fn(userData, arg);
    }
};

template <typename T1, typename T2>
Func1<T2> MkFunc1(void (*fn)(T1*, T2), T1* d) {
    auto res = Func1<T2>{};
    using fptr = void (*)(void*, T2);
    res.fn = (fptr)fn;
    res.userData = (void*)d;
    return res;
}

template <typename T2>
Func1<T2> MkFunc1Void(void (*fn)(T2)) {
    auto res = Func1<T2>{};
    using fptr = void (*)(void*, T2);
    res.fn = (fptr)fn;
    res.userData = kFuncNoArg;
    return res;
}

template <typename T1, typename T2>
Func1<T2>* NewFunc1(void (*fn)(T1*, T2), T1* d) {
    auto res = new Func1<T2>{};
    using fptr = void (*)(void*, T2);
    res->fn = (fptr)fn;
    res->userData = (void*)d;
    return res;
}

class AtomicBool {
  public:
    AtomicBool() = default;
    ~AtomicBool() = default;
    bool Get() const;
    bool Set(bool newValue);

  private:
    volatile LONG val = 0;
};

struct AtomicRefCount {
    AtomicRefCount() = default;
    ~AtomicRefCount() = default;
    int Add();
    // returns true if counter reaches 0, meaning it has been released
    // by all who held a reference to it
    int Dec();

  private:
    // starts life as acquired
    volatile LONG val = 1;
};

#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

extern LONG gAllowAllocFailure;

/* How to use:
defer { free(tools_filename); };
defer { fclose(f); };
defer { instance->Release(); };
*/

// exists just to mark the intent, needed by both StrUtil.h and TempAllocator.h
using TempStr = char*;
using TempWStr = WCHAR*;

#include "GeomUtil.h"
#include "Vec.h"
#include "StrUtil.h"
#include "TempAllocator.h"
#include "StrVec.h"
#include "StrconvUtil.h"
#include "Scoped.h"
#include "ColorUtil.h"

// lstrcpy is dangerous so forbid using it
#ifdef lstrcpy
#undef lstrcpy
#define lstrcpy dont_use_lstrcpy
#endif

#endif

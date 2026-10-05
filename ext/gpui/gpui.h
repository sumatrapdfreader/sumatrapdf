#ifndef GPUI_H_
#define GPUI_H_
#ifndef GPUI_AMALGAM
#define GPUI_AMALGAM 1
#endif
#define GPUI_MARKDOWN_FULL 1
#define GPUI_MARKDOWN_MINI 0
#define GPUI_HTML5EVER_FULL 1
#define GPUI_HTML5EVER_MINI 0
#ifndef GPUI_INCLUDE_PRIVATE_API
#define GPUI_INCLUDE_PRIVATE_API 0
#endif

#ifndef GPUI_BASE_H_
#define GPUI_BASE_H_
#line 1 "src/base.h"

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <algorithm>
#include <utility>

#if defined(__EMSCRIPTEN__)
#define GPUI_OS_WINDOWS 0
#define GPUI_OS_LINUX 0
#define GPUI_OS_MAC 0
#define GPUI_OS_IOS 0
#define GPUI_OS_ANDROID 0
#define GPUI_OS_WASM 1
#elif defined(_WIN32)
#define GPUI_OS_WINDOWS 1
#define GPUI_OS_LINUX 0
#define GPUI_OS_MAC 0
#define GPUI_OS_IOS 0
#define GPUI_OS_ANDROID 0
#define GPUI_OS_WASM 0
#elif defined(__ANDROID__)
#define GPUI_OS_WINDOWS 0
#define GPUI_OS_LINUX 0
#define GPUI_OS_MAC 0
#define GPUI_OS_IOS 0
#define GPUI_OS_ANDROID 1
#define GPUI_OS_WASM 0
#elif defined(__APPLE__) && \
    defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#define GPUI_OS_WINDOWS 0
#define GPUI_OS_LINUX 0
#define GPUI_OS_MAC 0
#define GPUI_OS_IOS 1
#define GPUI_OS_ANDROID 0
#define GPUI_OS_WASM 0
#elif defined(__APPLE__)
#define GPUI_OS_WINDOWS 0
#define GPUI_OS_LINUX 0
#define GPUI_OS_MAC 1
#define GPUI_OS_IOS 0
#define GPUI_OS_ANDROID 0
#define GPUI_OS_WASM 0
#elif defined(__linux__)
#define GPUI_OS_WINDOWS 0
#define GPUI_OS_LINUX 1
#define GPUI_OS_MAC 0
#define GPUI_OS_IOS 0
#define GPUI_OS_ANDROID 0
#define GPUI_OS_WASM 0
#else
#error "unsupported platform"
#endif

#define GPUI_OS_POSIX (!GPUI_OS_WINDOWS)

#if GPUI_OS_WINDOWS
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#else
#include <pthread.h>
#include <limits.h>
#endif

namespace base {

template <typename T, size_t N>
char (&DimofSizeHelper(T (&array)[N]) noexcept)[N];
#ifndef dimof
#define dimof(array) (sizeof(base::DimofSizeHelper(array)))
#endif

enum : uint16_t {
    kMaxPath = 1024
};

struct Arena;

struct Str {
    char* s;
    int len;

    constexpr Str() noexcept : s(nullptr), len(0) {}

    explicit Str(const char* s_) : s((char*)s_), len(0) {
        len = s_ ? (int)strlen(s_) : 0;
    }
    constexpr explicit Str(const char* s_, int len_) noexcept
        : s((char*)s_), len(len_) {}
    explicit Str(char* s_) : s(s_), len(0) { len = s ? (int)strlen(s) : 0; }
    constexpr explicit Str(char* s_, int len_) noexcept : s(s_), len(len_) {}

    explicit operator bool() const { return len > 0 && s; }
};

constexpr int len(Str s) noexcept {
    return s.len;
}

float StrToFloatUnchecked(Str s);

void log(Str s);

using PanicHook = void (*)(const char* msg);

PanicHook SetPanicHook(PanicHook hook);
void Panic(const char* msg);

using TempStr = Str;

#define StrL(lit) ::base::Str{(char*)(lit), (int)dimof(lit) - 1}

TempStr AllocStrTemp(int size);
TempStr StrDupTemp(Str s);
TempStr ReadBoundedFileTemp(Str path, int limit);

#if GPUI_OS_WINDOWS
WCHAR* ToCWstrTemp(Str s);
#endif

uint64_t PlatPageSize();
uint64_t PlatLargePageSize();

uint64_t PlatArenaReserveSize();
void* PlatMemReserve(uint64_t size);
bool PlatMemCommit(void* base, uint64_t size, bool largePages);
void* PlatMemReserveCommit(uint64_t size, bool largePages);
void PlatMemRelease(void* base, uint64_t size);

int StrCmpI(const char* a, const char* b);
int StrCmpNI(const char* a, const char* b, int n);
void StrCopyZ(char* dst, int cap, const char* src);

bool PlatDirExists(const char* path);
bool PlatFileExists(const char* path);
void PlatGetCwd(char* out, int cap);
bool PlatCanonicalPath(const char* path, char* out, int cap);
void PlatGetExeDir(char* out, int cap);

struct DirEntry {
    char name[260] = {};
    bool isDir = false;
    bool isFile = false;
    bool isSymlink = false;
    uint64_t size = 0;
    uint64_t modified = 0;
};

int PlatListDir(const char* dir, DirEntry* out, int max);
int PlatCoreCount();
bool PlatSelfUsage(uint64_t* cpu100ns, uint64_t* memBytes);

char PlatPathSep();
bool PlatPathsCaseFold();
bool PlatIsWindows();
bool PlatSecondaryIsCommand();
bool PlatShowsWindowControls();
float PlatCaretWidth();
bool PlatScrollBounce();
const char* PlatMonoFontName();
const char* PlatShellDataDir();
const char* PlatShellPlatformName();
bool PlatBlockSelectUsesControl();
bool PlatScrollGestureLocks();
bool PlatAsyncIo();
float PlatWindowShadowSize();

void* AllocZero(int count, int size);

template <typename T>
inline T* AllocArray(int n) {
    return (T*)AllocZero(n, (int)sizeof(T));
}

template <typename T>
inline void ZeroStruct(T* s) {
    memset((void*)s, 0, sizeof(T));
}

constexpr int ClampI(int x, int lo, int hi) {
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return x;
}
constexpr float ClampF(float x, float lo, float hi) {
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return x;
}
constexpr double ClampD(double x, double lo, double hi) {
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return x;
}

struct Func0 {

    static constexpr uintptr_t kFuncNoArg = ~(uintptr_t)1;

    void* fn = nullptr;
    uintptr_t userData = 0;

    Func0() = default;

    bool IsValid() const { return fn != nullptr; }
    void Call() const {
        if (!fn) {
            return;
        }
        if (userData == kFuncNoArg) {
            auto func = (void (*)())fn;
            func();
            return;
        }
        auto func = (void (*)(uintptr_t))fn;
        func(userData);
    }
};

template <typename T>
Func0 MkFunc0(void (*fn)(T*), T* d) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = (uintptr_t)d;
    return res;
}

inline Func0 MkFunc0Void(void (*fn)()) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = Func0::kFuncNoArg;
    return res;
}

template <typename T>
struct Func1 {
    static constexpr uintptr_t kDropsArgBit = 1;
    static constexpr uintptr_t kFuncNoArg = Func0::kFuncNoArg;

    void* fn = nullptr;
    uintptr_t userData = 0;

    Func1() = default;
    Func1(const Func0& that) {
        this->fn = that.fn;
        this->SetData(that.userData, true);
    }

    void SetData(uintptr_t d, bool dropsArg) {
        userData = d | (dropsArg ? kDropsArgBit : 0);
    }
    bool IsValid() const { return fn != nullptr; }
    void Call(T arg) const {
        if (!fn) {
            return;
        }
        uintptr_t d = userData & ~kDropsArgBit;
        if (userData & kDropsArgBit) {
            if (d == kFuncNoArg) {
                auto func = (void (*)())fn;
                func();
            } else {
                auto func = (void (*)(uintptr_t))fn;
                func(d);
            }
            return;
        }
        if (d == kFuncNoArg) {
            auto func = (void (*)(T))fn;
            func(arg);
            return;
        }
        auto func = (void (*)(uintptr_t, T))fn;
        func(d, arg);
    }
};

template <typename T1, typename T2>
Func1<T2> MkFunc1(void (*fn)(T1*, T2), T1* d) {
    auto res = Func1<T2>{};
    res.fn = (void*)fn;
    res.SetData((uintptr_t)d, false);
    return res;
}

template <typename T2>
Func1<T2> MkFunc1Void(void (*fn)(T2)) {
    auto res = Func1<T2>{};
    res.fn = (void*)fn;
    res.SetData(Func1<T2>::kFuncNoArg, false);
    return res;
}

struct Mutex {
#if GPUI_OS_WINDOWS
    SRWLOCK lock = SRWLOCK_INIT;
    void Lock() { AcquireSRWLockExclusive(&lock); }
    void Unlock() { ReleaseSRWLockExclusive(&lock); }
#else
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    void Lock() { pthread_mutex_lock(&lock); }
    void Unlock() { pthread_mutex_unlock(&lock); }
#endif
    Mutex() = default;
    ~Mutex() = default;
};

struct CondVar {
#if GPUI_OS_WINDOWS
    CONDITION_VARIABLE cv = CONDITION_VARIABLE_INIT;
    void Wait(Mutex* m, int timeoutMs) {
        DWORD t = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
        SleepConditionVariableSRW(&cv, &m->lock, t, 0);
    }
    void WakeOne() { WakeConditionVariable(&cv); }
    void WakeAll() { WakeAllConditionVariable(&cv); }
#else
    pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
    void Wait(Mutex* m, int timeoutMs);
    void WakeOne() { pthread_cond_signal(&cv); }
    void WakeAll() { pthread_cond_broadcast(&cv); }
#endif
    CondVar() = default;
    ~CondVar() = default;
};

bool PlatThreadRun(Func0 f);
uint64_t PlatThreadId();
void PlatSleepMs(int ms);

static const uint64_t kArenaHeaderSize = 256;

struct Arena {
    Arena* prev;
    Arena* current;
    uint64_t flags;
    uint64_t commitChunkSize;
    uint64_t reserveChunkSize;
    uint64_t basePos;
    uint64_t pos;
    uint64_t committed;
    uint64_t reserved;
    const char* allocationSiteFile;
    int allocationSiteLine;
    const char* name;
    bool usesExternalBuffer;
    Mutex lock;
    uint64_t nAllocsSinceReset;

    void* Alloc(int size);
    void Reset();
    void* Push(uint64_t size, uint64_t align = 8, bool zero = true);
    void PopTo(uint64_t pos);

    Arena() = delete;
    ~Arena() = delete;
};

Arena* ArenaNew();
void ArenaDelete(Arena* arena);

uint64_t ArenaUsed(Arena* arena);

int VarintSize(uint32_t v);
int VarintPut(char* dst, uint32_t v);
int VarintGet(const char* src, uint32_t* out);

using ArenaStr = uint32_t;

constexpr ArenaStr kArenaStrNone = 0;

constexpr bool ArenaStrIsSet(ArenaStr s) {
    return s != kArenaStrNone;
}

ArenaStr ArenaStrDup(Arena* a, Str src);
uint32_t ArenaStrLen(Arena* a, ArenaStr s);

ArenaStr ArenaStrAppend(Arena* a, ArenaStr s, Str more);
Str ArenaStrGet(Arena* a, ArenaStr s);

constexpr uint32_t kArenaPtrNone = 0;

uint32_t ArenaOffsetOf(Arena* a, const void* p);

inline void* ArenaAtOffset(Arena* a, uint32_t off) {
    if (off == kArenaPtrNone || !a) {
        return nullptr;
    }
    Arena* node = a->current;
    if (node && node->basePos <= (uint64_t)off) {
        return (char*)node + ((uint64_t)off - node->basePos);
    }
    while (node && node->basePos > (uint64_t)off) {
        node = node->prev;
    }
    return node ? (char*)node + ((uint64_t)off - node->basePos) : nullptr;
}

template <typename T>
struct ArenaPtr {
    uint32_t off = kArenaPtrNone;

    bool IsSet() const { return off != kArenaPtrNone; }
    bool operator==(const ArenaPtr<T>& o) const { return off == o.off; }
    bool operator!=(const ArenaPtr<T>& o) const { return off != o.off; }
};

template <typename T>
ArenaPtr<T> ArenaPtrOf(Arena* a, const T* p) {
    return ArenaPtr<T>{ArenaOffsetOf(a, p)};
}

template <typename T>
T* ArenaPtrGet(Arena* a, ArenaPtr<T> p) {
    return (T*)ArenaAtOffset(a, p.off);
}

Arena* GetTempArena();
void ResetTempArena();
void DestroyTempArena();

void* Alloc(struct Arena* arena, int size);
void Free(struct Arena* arena, void* mem);

template <typename T, typename... Args>
T* ArenaNew(Arena* arena, Args&&... args) {
    void* mem = Alloc(arena, (int)sizeof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

#if GPUI_OS_WINDOWS
#define GPUI_NOINLINE __declspec(noinline)
#else
#define GPUI_NOINLINE __attribute__((noinline))
#endif

void* ArenaVecAlloc(struct Arena* a, int count, int elSize, int align,
                    int hdrSize = 0);

GPUI_NOINLINE bool VecRealloc(struct Arena* a, void** els, int len, int* cap,
                              int newCap, int elSize);

inline int VecNextCap(int cap, int wanted, int elSize) {
    if (cap == 0) {
        int floorCap = elSize == 1 ? 8 : elSize <= 1024 ? 4 : 1;
        return std::max(floorCap, wanted);
    }
    return std::max(cap * 2, wanted);
}

inline int VecAbsCap(int cap) {
    return cap < 0 ? -cap : cap;
}

#if defined(DEBUG)
int VecDbgBirth(const char* file, int line, const char* func, char kind,
                int elSize) noexcept;
void VecDbgGrow(int id, int len, int oldCap, int needed, int newCap) noexcept;
void VecDbgSegment(int id, int len, int want, int lastSegCap, int newSegCap,
                   int totalCap, bool reused) noexcept;
void VecDbgDeath(int id, int len, int cap) noexcept;
void VecDbgArenaDeath(int id, int len, int totalCap, int segCount) noexcept;

#define GPUI_VEC_DBG_ARGS0                                            \
    const char *dbgF = __builtin_FILE(), int dbgL = __builtin_LINE(), \
               const char *dbgFn = __builtin_FUNCTION()
#define GPUI_VEC_DBG_ARGS , GPUI_VEC_DBG_ARGS0
#define GPUI_VEC_DBG_INIT(kind) \
    : dbgId(VecDbgBirth(dbgF, dbgL, dbgFn, kind, (int)sizeof(T)))
#else
#define GPUI_VEC_DBG_ARGS0
#define GPUI_VEC_DBG_ARGS
#define GPUI_VEC_DBG_INIT(kind)
#endif

template <typename T>
struct Vec;

template <typename T>
struct VecIdentity {
    using type = T;
};
template <typename T>
using VecIdentityT = typename VecIdentity<T>::type;

#if defined(__GNUC__) || defined(__clang__)

struct __attribute__((__may_alias__)) VecNonTemplated {
#else
struct VecNonTemplated {
#endif
    int len;
    int cap;
    void* els;
};

bool VecReserveNT(Arena* arena, VecNonTemplated* v, int elSize, int wantedSize);
void* VecInsertSpaceNT(VecNonTemplated* v, int elSize, int idx, int count);
bool VecResizeNT(VecNonTemplated* v, int elSize, int newSize);
void VecRemoveAtNT(VecNonTemplated* v, int elSize, int idx, int count);
void VecRemoveAtFastNT(VecNonTemplated* v, int elSize, int idx);
void VecFreeElementsNT(VecNonTemplated* v);
void VecClearNT(VecNonTemplated* v, int elSize);
void* VecTakeNT(VecNonTemplated* v, int elSize);
void VecCopyFromNT(VecNonTemplated* v, int elSize, int srcLen,
                   const void* srcEls, bool zeroTail);

template <typename T>
VecNonTemplated* VecNT(Vec<T>& v);

template <typename T>
struct Vec {
    int len = 0;

    int cap = 0;
    T* els = nullptr;
#if defined(DEBUG)
    int dbgId = 0;
#endif

    explicit Vec(GPUI_VEC_DBG_ARGS0) noexcept GPUI_VEC_DBG_INIT('V') {}

    Vec(const Vec& other GPUI_VEC_DBG_ARGS) GPUI_VEC_DBG_INIT('V') {
        VecCopyFromNT(VecNT(*this), (int)sizeof(T), other.len,
                      (const void*)other.els, false);
    }

    Vec& operator=(const Vec& other) {
        if (this == &other) {
            return *this;
        }
        VecReset(*this);
        VecCopyFromNT(VecNT(*this), (int)sizeof(T), other.len,
                      (const void*)other.els, true);
        return *this;
    }

    ~Vec() {
#if defined(DEBUG)
        VecDbgDeath(dbgId, len, VecAbsCap(cap));
#endif
        VecReset(*this);
    }

    T& operator[](int idx) const { return els[idx]; }

    using iterator = T*;
    using const_iterator = const T*;
    iterator begin() { return els; }
    const_iterator begin() const { return els; }
    iterator end() { return els ? els + len : nullptr; }
    const_iterator end() const { return els ? els + len : nullptr; }
};

static_assert(offsetof(Vec<char>, len) == offsetof(VecNonTemplated, len));
static_assert(offsetof(Vec<char>, cap) == offsetof(VecNonTemplated, cap));
static_assert(offsetof(Vec<char>, els) == offsetof(VecNonTemplated, els));
static_assert(offsetof(Vec<double>, els) == offsetof(VecNonTemplated, els));
#if !defined(DEBUG)
static_assert(sizeof(Vec<char>) == sizeof(VecNonTemplated));
static_assert(sizeof(Vec<double>) == sizeof(VecNonTemplated));
#endif

template <typename T>
VecNonTemplated* VecNT(Vec<T>& v) {
    return (VecNonTemplated*)&v;
}

template <typename T>
inline int len(const Vec<T>& v) {
    return v.len;
}

template <typename T>
auto VecReserve(Arena* arena, T& v, int n) -> decltype(v.els) {
    static_assert(offsetof(T, len) == offsetof(VecNonTemplated, len));
    static_assert(offsetof(T, cap) == offsetof(VecNonTemplated, cap));
    static_assert(offsetof(T, els) == offsetof(VecNonTemplated, els));
    if (!VecReserveNT(arena, (VecNonTemplated*)&v, (int)sizeof(*v.els), n)) {
        return nullptr;
    }
    return v.els;
}

template <typename T, int N>
inline void VecUseExternalBuffer(Vec<T>& v, T (&buf)[N]) {
    v.els = buf;
    v.cap = -N;
    v.len = 0;
}

template <typename T>
inline T* VecReserve(Vec<T>& v, int n) {
#if defined(DEBUG)
    int curCap = VecAbsCap(v.cap);
    if (n > curCap) {
        VecDbgGrow(v.dbgId, len(v), curCap, n,
                   VecNextCap(curCap, n, (int)sizeof(T)));
    }
#endif
    return VecReserve(nullptr, v, n);
}

template <typename T>
T* VecInsertSpace(Vec<T>& v, int idx, int count) {
    return (T*)VecInsertSpaceNT(VecNT(v), (int)sizeof(T), idx, count);
}

template <typename T>
bool VecResize(Vec<T>& v, int newSize) {
    return VecResizeNT(VecNT(v), (int)sizeof(T), newSize);
}

template <typename T>
void VecClear(Vec<T>& v) {
    VecClearNT(VecNT(v), (int)sizeof(T));
}

template <typename T>
void VecReset(Vec<T>& v) {
    VecFreeElementsNT(VecNT(v));
}

template <typename T>
T* VecTake(Vec<T>& v) {
    return (T*)VecTakeNT(VecNT(v), (int)sizeof(T));
}

template <typename T>
bool VecAppend(Vec<T>& v, const VecIdentityT<T>& el) {
    return VecInsertAt(v, len(v), el);
}

template <typename T>
bool VecAppendVec(Vec<T>& v, const Vec<T>& other) {
    return VecAppendN(v, other.els, len(other));
}

template <typename T>
bool VecAppendN(Vec<T>& v, const T* src, int count) {
    if (count == 0) {
        return true;
    }
    T* dst = VecInsertSpace(v, len(v), count);
    if (!dst) {
        return false;
    }
    memcpy((void*)dst, (const void*)src, (size_t)count * sizeof(T));
    return true;
}

template <typename T>
T* VecAppendBlanks(Vec<T>& v, int count) {
    return VecInsertSpace(v, len(v), count);
}

template <typename T>
bool VecInsertAt(Vec<T>& v, int idx, const VecIdentityT<T>& el) {
    T* p = VecInsertSpace(v, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

template <typename T, typename E>
bool VecPush(Arena* arena, T& v, E el) {
    if (!VecReserve(arena, v, v.len + 1)) {
        return false;
    }
    v.els[v.len++] = el;
    return true;
}

template <typename T>
void VecRemoveAtN(Vec<T>& v, int idx, int count) {
    VecRemoveAtNT(VecNT(v), (int)sizeof(T), idx, count);
}

template <typename T>
void VecRemoveAt(Vec<T>& v, int idx) {
    VecRemoveAtN(v, idx, 1);
}

template <typename T>
T VecPopAt(Vec<T>& v, int idx) {
    T el = v.els[idx];
    VecRemoveAt(v, idx);
    return el;
}

template <typename T>
void VecRemoveAtFast(Vec<T>& v, int idx) {
    VecRemoveAtFastNT(VecNT(v), (int)sizeof(T), idx);
}

template <typename T>
void VecRemoveLast(Vec<T>& v) {
    if (len(v) > 0) {
        VecRemoveAt(v, len(v) - 1);
    }
}

template <typename T>
T VecPop(Vec<T>& v) {
    T el = v.els[len(v) - 1];
    VecRemoveAtFast(v, len(v) - 1);
    return el;
}

template <typename T>
int VecRemove(Vec<T>& v, const T& el) {
    int i = VecFind(v, el);
    if (i >= 0) {
        VecRemoveAt(v, i);
    }
    return i;
}

template <typename T>
bool VecIsValidIndex(const Vec<T>& v, int idx) {
    return idx >= 0 && idx < len(v);
}

template <typename T>
T& VecLast(const Vec<T>& v) {
    return v.els[len(v) - 1];
}

template <typename T>
int VecFind(const Vec<T>& v, const T& el, int startAt = 0) {
    for (int i = startAt; i < len(v); i++) {
        if (v.els[i] == el) {
            return i;
        }
    }
    return -1;
}

template <typename T>
bool VecContains(const Vec<T>& v, const T& el) {
    return VecFind(v, el) >= 0;
}

template <typename T>
struct ArenaVecSegment {
    ArenaPtr<ArenaVecSegment<T>> next;
    int base;
    int len;
    int cap;

    static constexpr int HeaderSize() {
        return ((int)sizeof(ArenaVecSegment<T>) + (int)alignof(T) - 1) &
               ~((int)alignof(T) - 1);
    }

    T* Els() const { return (T*)((char*)(void*)this + HeaderSize()); }
};

constexpr int kArenaVecCap0 = 4;
constexpr int kArenaVecCap1 = 16;
constexpr int kArenaVecCap2 = 64;
constexpr int kArenaVecBytes0 = 64;
constexpr int kArenaVecBytes1 = 256;
constexpr int kArenaVecBytes2 = 1024;

template <typename T>
struct ArenaVec {
    using Segment = ArenaVecSegment<T>;

    Arena* a = nullptr;
    ArenaPtr<Segment> first = {};
    ArenaPtr<Segment> last = {};
    int len = 0;
#if defined(DEBUG)
    int dbgId = 0;
    int dbgTotalCap = 0;
    int dbgSegs = 0;

    ArenaVec(GPUI_VEC_DBG_ARGS0) noexcept GPUI_VEC_DBG_INIT('A') {}

    ~ArenaVec() { VecDbgArenaDeath(dbgId, len, dbgTotalCap, dbgSegs); }
#endif

    T& operator[](int idx) const {
        Segment* seg = ArenaPtrGet(a, first);
        if (first == last) {
            return seg->Els()[idx];
        }
        while (idx >= seg->base + seg->len) {
            seg = ArenaPtrGet(a, seg->next);
        }
        return seg->Els()[idx - seg->base];
    }

    struct Iter {
        Arena* a;
        Segment* seg;
        int idx;

        void Normalize() {
            while (seg && idx >= seg->len) {
                seg = ArenaPtrGet(a, seg->next);
                idx = 0;
            }
        }
        T& operator*() const { return seg->Els()[idx]; }
        T* operator->() const { return &seg->Els()[idx]; }
        Iter& operator++() {
            idx++;
            if (idx >= seg->len) {
                seg = ArenaPtrGet(a, seg->next);
                idx = 0;
                Normalize();
            }
            return *this;
        }
        bool operator!=(const Iter& o) const {
            return seg != o.seg || idx != o.idx;
        }
    };

    Iter begin() const {
        Iter it = {a, ArenaPtrGet(a, first), 0};
        it.Normalize();
        return it;
    }
    Iter end() const { return Iter{a, nullptr, 0}; }

    bool Append(Arena* arena, const T& el) { return AppendMany(arena, &el, 1); }

    bool AppendMany(Arena* arena, const T* src, int n) {
        while (n > 0) {
            Segment* seg = ArenaPtrGet(a, last);
            if (!seg || seg->len >= seg->cap) {
                seg = NextSegment(arena, n);
                if (!seg) {
                    return false;
                }
            }
            int room = seg->cap - seg->len;
            int take = n < room ? n : room;
            for (int i = 0; i < take; i++) {
                seg->Els()[seg->len + i] = src[i];
            }
            seg->len += take;
            len += take;
            src += take;
            n -= take;
        }
        return true;
    }

    bool Reserve(Arena* arena, int n) {
        Segment* seg = ArenaPtrGet(a, last);
        if (seg && seg->cap - seg->len >= n) {
            return true;
        }
        return NextSegment(arena, n) != nullptr;
    }

    void Truncate(int newLen) {
        if (newLen < 0) {
            newLen = 0;
        }
        if (newLen >= len) {
            return;
        }
        ArenaPtr<Segment> at = first;
        Segment* seg = ArenaPtrGet(a, at);
        while (seg && seg->base + seg->len <= newLen) {
            at = seg->next;
            seg = ArenaPtrGet(a, at);
        }
        if (seg) {
            seg->len = newLen - seg->base;
            last = at;
            for (Segment* s = ArenaPtrGet(a, seg->next); s;
                 s = ArenaPtrGet(a, s->next)) {
                s->len = 0;
            }
        }
        len = newLen;
    }

    void Pop() { Truncate(len - 1); }

    T* Flatten(Arena* into) const {
        if (len == 0) {
            return nullptr;
        }
        if (first == last) {
            return ArenaPtrGet(a, first)->Els();
        }
        T* out = (T*)ArenaVecAlloc(into, len, (int)sizeof(T), (int)alignof(T));
        if (!out) {
            return nullptr;
        }
        int at = 0;
        for (const T& el : *this) {
            out[at++] = el;
        }
        return out;
    }

    static constexpr int CapFor(int count, int bytes) {
        int byBytes = bytes / (int)sizeof(T);
        if (byBytes < 1) {
            byBytes = 1;
        }
        return byBytes < count ? byBytes : count;
    }

    static int NextCap(int prevCap) {
        const int steps[3] = {
            CapFor(kArenaVecCap0, kArenaVecBytes0),
            CapFor(kArenaVecCap1, kArenaVecBytes1),
            CapFor(kArenaVecCap2, kArenaVecBytes2),
        };
        for (int s : steps) {
            if (prevCap < s) {
                return s;
            }
        }
        return prevCap * 2;
    }

    GPUI_NOINLINE Segment* NextSegment(Arena* arena, int want) {
        a = arena;
#if defined(DEBUG)
        if (dbgId == 0) {
            dbgId = VecDbgBirth("<no-constructor>", 0, "", 'A', (int)sizeof(T));
        }
#endif
        Segment* lastSeg = ArenaPtrGet(a, last);
#if defined(DEBUG)
        int dbgPrevCap = lastSeg ? lastSeg->cap : 0;
#endif
        ArenaPtr<Segment> reuseAt =
            lastSeg ? lastSeg->next : ArenaPtr<Segment>{};
        Segment* reuse = ArenaPtrGet(a, reuseAt);
        if (reuse && reuse->cap >= want) {
            reuse->len = 0;
            reuse->base = len;
            last = reuseAt;
#if defined(DEBUG)
            VecDbgSegment(dbgId, len, want, dbgPrevCap, reuse->cap, dbgTotalCap,
                          true);
#endif
            return reuse;
        }
        int cap = NextCap(lastSeg ? lastSeg->cap : 0);
        if (cap < want) {
            cap = want;
        }
        int align = (int)alignof(T) > 8 ? (int)alignof(T) : 8;
        void* mem =
            ArenaVecAlloc(a, cap, (int)sizeof(T), align, Segment::HeaderSize());
        if (!mem) {
            return nullptr;
        }
        Segment* seg = (Segment*)mem;
        seg->next = {};
        seg->base = len;
        seg->len = 0;
        seg->cap = cap;
        ArenaPtr<Segment> at = ArenaPtrOf(a, seg);
        if (lastSeg) {
            lastSeg->next = at;
        } else {
            first = at;
        }
        last = at;
#if defined(DEBUG)
        dbgTotalCap += cap;
        dbgSegs++;
        VecDbgSegment(dbgId, len, want, dbgPrevCap, cap, dbgTotalCap, false);
#endif
        return seg;
    }
};

template <typename T>
inline int len(const ArenaVec<T>& v) {
    return v.len;
}

struct PointF {
    float x = 0.0f;
    float y = 0.0f;

    static constexpr PointF Zero() { return {0.0f, 0.0f}; }
};

constexpr bool operator==(PointF a, PointF b) {
    return a.x == b.x && a.y == b.y;
}

constexpr bool operator!=(PointF a, PointF b) {
    return !(a == b);
}

constexpr PointF operator+(PointF a, PointF b) {
    return {a.x + b.x, a.y + b.y};
}

struct SizeF {
    float w = 0.0f;
    float h = 0.0f;

    static constexpr SizeF Zero() { return {0.0f, 0.0f}; }
};

constexpr bool operator==(SizeF a, SizeF b) {
    return a.w == b.w && a.h == b.h;
}

constexpr bool operator!=(SizeF a, SizeF b) {
    return !(a == b);
}

constexpr SizeF operator+(SizeF a, SizeF b) {
    return {a.w + b.w, a.h + b.h};
}

constexpr SizeF operator-(SizeF a, SizeF b) {
    return {a.w - b.w, a.h - b.h};
}

struct RectF {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    static constexpr RectF Zero() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr RectF New(float l, float r, float t, float b) {
        return {l, r, t, b};
    }

    constexpr float HorizontalAxisSum() const { return left + right; }
    constexpr float VerticalAxisSum() const { return top + bottom; }
    constexpr SizeF SumAxes() const { return {left + right, top + bottom}; }
};

constexpr bool operator==(RectF a, RectF b) {
    return a.left == b.left && a.right == b.right && a.top == b.top &&
           a.bottom == b.bottom;
}

constexpr bool operator!=(RectF a, RectF b) {
    return !(a == b);
}

constexpr RectF operator+(RectF a, RectF b) {
    return {a.left + b.left, a.right + b.right, a.top + b.top,
            a.bottom + b.bottom};
}

struct LocalDate {
    int year = 0;
    int month = 0;
    int day = 0;
};

LocalDate DateToday();
LocalDate DateAddDays(LocalDate base, int days);

struct LocalTime {
    int hour = 0;
    int minute = 0;
    int second = 0;
};

inline bool operator==(LocalTime a, LocalTime b) {
    return a.hour == b.hour && a.minute == b.minute && a.second == b.second;
}
inline bool operator!=(LocalTime a, LocalTime b) {
    return !(a == b);
}

LocalTime TimeOfDayNow();

void StrFree(Str s);
void StrFree(const char*) = delete;

Str StrDup(Arena*, Str str);
Str StrDup(Str s);

void StrDup2(Str s1, Str s2, Str& s1Out, Str& s2Out);

GPUI_NOINLINE bool StrEqRest(Str s1, Str s2);
inline bool StrEq(Str s1, Str s2) {
    if (len(s1) != len(s2)) {
        return false;
    }
    return StrEqRest(s1, s2);
}
inline bool StrEq(Str s1, const char* s2) {
    return StrEq(s1, Str(s2));
}
int StrCmp(Str s1, Str s2);
GPUI_NOINLINE bool StrEqIRest(Str s1, Str s2);
inline bool StrEqI(Str s1, Str s2) {
    if (len(s1) != len(s2)) {
        return false;
    }
    return StrEqIRest(s1, s2);
}
inline bool StrEqI(Str s1, const char* s2) {
    return StrEqI(s1, Str(s2));
}
bool StrStartsWith(Str s, Str prefix);
inline bool StrStartsWith(Str s, const char* prefix) {
    return StrStartsWith(s, Str(prefix));
}
bool StrStartsWithAny(Str s, const char* chars);
bool StrStartsWithI(Str s, Str prefix);
inline bool StrStartsWithI(Str s, const char* prefix) {
    return StrStartsWithI(s, Str(prefix));
}
bool StrEndsWith(Str s, Str suffix);
inline bool StrEndsWith(Str s, const char* suffix) {
    return StrEndsWith(s, Str(suffix));
}
bool StrEndsWithI(Str s, Str suffix);
inline bool StrEndsWithI(Str s, const char* suffix) {
    return StrEndsWithI(s, Str(suffix));
}
int StrFind(Str s, Str sub);
inline int StrFind(Str s, const char* sub) {
    return StrFind(s, Str(sub));
}
int StrFindI(Str s, Str sub);
inline int StrFindI(Str s, const char* sub) {
    return StrFindI(s, Str(sub));
}
inline bool StrContains(Str s, Str sub) {
    return StrFind(s, sub) >= 0;
}
inline bool StrContainsI(Str s, Str sub) {
    return StrFindI(s, sub) >= 0;
}
Str StrTrimAscii(Str s);

Str StrTrim(Str s);
Str StrReplaceAll(Str value, Str from, Str to);

using SeqStrings = const char*;

Str SeqStrFirst(SeqStrings strs);
Str SeqStrNext(Str s);
int SeqStrIndex(SeqStrings strs, Str toFind);
int SeqStrIndexIS(SeqStrings strs, Str toFind);
bool SeqStrContainsI(SeqStrings strs, Str toFind);
Str SeqStrByIndex(SeqStrings strs, int idx);
int SeqStrCount(SeqStrings strs);
void StrLowerAscii(char* s);

struct StrBuilder : Vec<char> {
    Arena* a = nullptr;

    explicit StrBuilder(Arena* arena = nullptr) : a(arena) {}

    void Reset(Str s = {});
    bool Reserve(int cap);
    bool AppendChar(char c);
    bool Append(Str src);
    char RemoveAt(int idx, int count = 1);
    char RemoveLast();
    Str TakeStr();
    char LastChar() const;
};

void StrBuilderUseExternalBuffer(StrBuilder& b, Str buf);

struct FmtArg {
    enum class Kind : uint8_t {
        Char,
        Int,
        Ptr,
        Float,
        Double,
        Str,
        RawStr,
        Any,
        None,
    };

    Kind t{Kind::None};
    union {
        Str str;
        char c;
        int64_t i;
        float f;
        double d;
        const void* ptr;
    };

    FmtArg() : i{0} {}
    explicit FmtArg(char c_) : t{Kind::Char}, c{c_} {}
    explicit FmtArg(int arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(unsigned int arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(long arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(unsigned long arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(long long arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(unsigned long long arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(float f_) : t{Kind::Float}, f{f_} {}
    explicit FmtArg(double d_) : t{Kind::Double}, d{d_} {}
    explicit FmtArg(Str arg) : t{Kind::Str}, str{arg} {}
    explicit FmtArg(const void* p) : t{Kind::Ptr}, ptr{p} {}
    FmtArg(char*) = delete;
    FmtArg(const char*) = delete;
    FmtArg(wchar_t*) = delete;
    FmtArg(const wchar_t*) = delete;
};

TempStr FormatTempArgs(const char* fmt, const FmtArg** args, int nArgs);

inline TempStr FormatTemp(const char* fmt) {
    return FormatTempArgs(fmt, nullptr, 0);
}

template <typename... TArgs>
TempStr FormatTemp(const char* fmt, const TArgs&... args) {
    const FmtArg argv[] = {FmtArg(args)...};
    const FmtArg* argp[sizeof...(TArgs)];
    int n = (int)sizeof...(TArgs);
    for (int i = 0; i < n; i++) {
        argp[i] = &argv[i];
    }
    return FormatTempArgs(fmt, argp, n);
}

template <typename... TArgs>
inline TempStr fmt(const char* format, const TArgs&... args) {
    return FormatTemp(format, args...);
}

template <typename... TArgs>
inline void logf(const char* format, const TArgs&... args) {
    log(FormatTemp(format, args...));
}
}

#endif

#line 1 "src/taffy/geometry.h"

#include <cmath>
#include <cfloat>

namespace taffy {

using base::Arena;
using base::PointF;
using base::RectF;
using base::SizeF;
using base::Str;
using base::Vec;

using Optf = float;

inline constexpr uint32_t kOptfNoneBits = 0x7fc0beefu;

constexpr Optf None() {
    return __builtin_bit_cast(float, kOptfNoneBits);
}

constexpr Optf Some(float v) {
    return v;
}

constexpr bool IsNone(Optf v) {
    return __builtin_bit_cast(uint32_t, v) == kOptfNoneBits;
}

constexpr bool IsSome(Optf v) {
    return __builtin_bit_cast(uint32_t, v) != kOptfNoneBits;
}

constexpr float Unwrap(Optf v) {
    return v;
}

constexpr float UnwrapOr(Optf v, float alt) {
    return IsSome(v) ? v : alt;
}

constexpr Optf Or(Optf v, Optf alt) {
    return IsSome(v) ? v : alt;
}

constexpr bool OptfEq(Optf a, Optf b) {
    return IsNone(a) ? IsNone(b) : (a == b);
}

constexpr bool OptfNe(Optf a, Optf b) {
    return !OptfEq(a, b);
}

inline bool F32IsNan(float v) {
    return (__builtin_bit_cast(uint32_t, v) & 0x7fffffffu) > 0x7f800000u;
}

inline float F32Max(float a, float b) {
    if (F32IsNan(a)) {
        return b;
    }
    if (F32IsNan(b)) {
        return a;
    }
    return a > b ? a : b;
}

inline float F32Min(float a, float b) {
    if (F32IsNan(a)) {
        return b;
    }
    if (F32IsNan(b)) {
        return a;
    }
    return a < b ? a : b;
}

inline float F32Round(float v) {
    return floorf(v + 0.5f);
}

enum class AbsoluteAxis : uint8_t {
    Horizontal,
    Vertical
};

constexpr AbsoluteAxis OtherAxis(AbsoluteAxis axis) {
    return axis == AbsoluteAxis::Horizontal ? AbsoluteAxis::Vertical
                                            : AbsoluteAxis::Horizontal;
}

enum class AbstractAxis : uint8_t {
    Inline,
    Block
};

constexpr AbstractAxis Other(AbstractAxis axis) {
    return axis == AbstractAxis::Inline ? AbstractAxis::Block
                                        : AbstractAxis::Inline;
}

constexpr AbsoluteAxis AsAbsNaive(AbstractAxis axis) {
    return axis == AbstractAxis::Inline ? AbsoluteAxis::Horizontal
                                        : AbsoluteAxis::Vertical;
}

enum class FlexDirection : uint8_t {
    Row,
    Column,
    RowReverse,
    ColumnReverse
};

constexpr bool IsRow(FlexDirection d) {
    return d == FlexDirection::Row || d == FlexDirection::RowReverse;
}

constexpr bool IsColumn(FlexDirection d) {
    return d == FlexDirection::Column || d == FlexDirection::ColumnReverse;
}

constexpr bool IsReverse(FlexDirection d) {
    return d == FlexDirection::RowReverse || d == FlexDirection::ColumnReverse;
}

constexpr AbsoluteAxis MainAxis(FlexDirection d) {
    return IsRow(d) ? AbsoluteAxis::Horizontal : AbsoluteAxis::Vertical;
}

constexpr AbsoluteAxis CrossAxis(FlexDirection d) {
    return IsRow(d) ? AbsoluteAxis::Vertical : AbsoluteAxis::Horizontal;
}

constexpr float GetAbs(SizeF s, AbsoluteAxis a) {
    return a == AbsoluteAxis::Horizontal ? s.w : s.h;
}
constexpr float Get(SizeF s, AbstractAxis a) {
    return a == AbstractAxis::Inline ? s.w : s.h;
}
constexpr void Set(SizeF* s, AbstractAxis a, float v) {
    if (a == AbstractAxis::Inline) {
        s->w = v;
    } else {
        s->h = v;
    }
}
constexpr float Main(SizeF s, FlexDirection d) {
    return IsRow(d) ? s.w : s.h;
}
constexpr float Cross(SizeF s, FlexDirection d) {
    return IsRow(d) ? s.h : s.w;
}
constexpr void SetMain(SizeF* s, FlexDirection d, float v) {
    if (IsRow(d)) {
        s->w = v;
    } else {
        s->h = v;
    }
}
constexpr void SetCross(SizeF* s, FlexDirection d, float v) {
    if (IsRow(d)) {
        s->h = v;
    } else {
        s->w = v;
    }
}
constexpr SizeF WithMain(SizeF s, FlexDirection d, float v) {
    SetMain(&s, d, v);
    return s;
}
constexpr SizeF WithCross(SizeF s, FlexDirection d, float v) {
    SetCross(&s, d, v);
    return s;
}

inline SizeF Max(SizeF a, SizeF b) {
    return {F32Max(a.w, b.w), F32Max(a.h, b.h)};
}
inline SizeF Min(SizeF a, SizeF b) {
    return {F32Min(a.w, b.w), F32Min(a.h, b.h)};
}
constexpr bool HasNonZeroArea(SizeF s) {
    return s.w > 0.0f && s.h > 0.0f;
}

constexpr float Get(PointF p, AbstractAxis a) {
    return a == AbstractAxis::Inline ? p.x : p.y;
}
constexpr void Set(PointF* p, AbstractAxis a, float v) {
    if (a == AbstractAxis::Inline) {
        p->x = v;
    } else {
        p->y = v;
    }
}
constexpr float Main(PointF p, FlexDirection d) {
    return IsRow(d) ? p.x : p.y;
}
constexpr float Cross(PointF p, FlexDirection d) {
    return IsRow(d) ? p.y : p.x;
}
constexpr PointF Transpose(PointF p) {
    return {p.y, p.x};
}
constexpr SizeF IntoSize(PointF p) {
    return {p.x, p.y};
}

constexpr float GridAxisSum(RectF r, AbsoluteAxis a) {
    return a == AbsoluteAxis::Horizontal ? r.left + r.right : r.top + r.bottom;
}
constexpr float MainAxisSum(RectF r, FlexDirection d) {
    return IsRow(d) ? r.left + r.right : r.top + r.bottom;
}
constexpr float CrossAxisSum(RectF r, FlexDirection d) {
    return IsRow(d) ? r.top + r.bottom : r.left + r.right;
}
constexpr float MainStart(RectF r, FlexDirection d) {
    return IsRow(d) ? r.left : r.top;
}
constexpr float MainEnd(RectF r, FlexDirection d) {
    return IsRow(d) ? r.right : r.bottom;
}
constexpr float CrossStart(RectF r, FlexDirection d) {
    return IsRow(d) ? r.top : r.left;
}
constexpr float CrossEnd(RectF r, FlexDirection d) {
    return IsRow(d) ? r.bottom : r.right;
}

using SizeFOpt = SizeF;
using PointFOpt = PointF;
using RectFOpt = RectF;

constexpr SizeFOpt SizeFOptNone() {
    return {None(), None()};
}

constexpr PointFOpt PointFOptNone() {
    return {None(), None()};
}

constexpr RectFOpt RectFOptNone() {
    return {None(), None(), None(), None()};
}

constexpr SizeFOpt SizeFOptFromCross(FlexDirection d, Optf v) {
    return IsRow(d) ? SizeFOpt{None(), v} : SizeFOpt{v, None()};
}

constexpr SizeF UnwrapOr(SizeFOpt s, SizeF alt) {
    return {UnwrapOr(s.w, alt.w), UnwrapOr(s.h, alt.h)};
}

constexpr SizeFOpt Or(SizeFOpt s, SizeFOpt alt) {
    return {Or(s.w, alt.w), Or(s.h, alt.h)};
}

constexpr bool BothAxisDefined(SizeFOpt s) {
    return IsSome(s.w) && IsSome(s.h);
}

constexpr SizeFOpt MaybeApplyAspectRatio(SizeFOpt s, Optf aspectRatio) {
    if (IsNone(aspectRatio)) {
        return s;
    }
    if (IsSome(s.w) && IsNone(s.h)) {
        return {s.w, s.w / aspectRatio};
    }
    if (IsNone(s.w) && IsSome(s.h)) {
        return {s.h * aspectRatio, s.h};
    }
    return s;
}

constexpr bool SizeFOptEq(SizeFOpt a, SizeFOpt b) {
    return OptfEq(a.w, b.w) && OptfEq(a.h, b.h);
}

struct LineF {
    float start = 0.0f;
    float end = 0.0f;

    constexpr float Sum() const { return start + end; }
};

struct LineBool {
    bool start = false;
    bool end = false;

    static constexpr LineBool True() { return {true, true}; }
    static constexpr LineBool False() { return {false, false}; }
};

template <typename T>
struct Slice {
    T* els = nullptr;
    int len = 0;

    T& operator[](int i) const { return els[i]; }
    bool IsEmpty() const { return len == 0; }
    T* begin() const { return els; }
    T* end() const { return els + len; }
};

template <typename T>
Slice<T> SliceNew(Arena* a, int n) {
    if (n <= 0) {
        return {};
    }
    return {(T*)a->Alloc((int)sizeof(T) * n), n};
}

template <typename T>
Slice<T> SliceDup(Arena* a, const T* src, int n) {
    Slice<T> out = SliceNew<T>(a, n);
    if (out.els && src) {
        memcpy((void*)out.els, (const void*)src, sizeof(T) * (size_t)n);
    }
    return out;
}

template <typename T>
Slice<T> SliceOne(Arena* a, const T& v) {
    Slice<T> out = SliceNew<T>(a, 1);
    if (out.els) {
        out.els[0] = v;
    }
    return out;
}

}

#line 1 "src/taffy/style.h"

namespace taffy {

struct CompactLength {
    uint64_t bits = 0;

    static constexpr uint64_t kCalcTag = 0;
    static constexpr uint64_t kLengthTag = 0x01;
    static constexpr uint64_t kPercentTag = 0x02;
    static constexpr uint64_t kAutoTag = 0x03;
    static constexpr uint64_t kFrTag = 0x04;
    static constexpr uint64_t kMinContentTag = 0x07;
    static constexpr uint64_t kMaxContentTag = 0x0f;
    static constexpr uint64_t kFitContentPxTag = 0x17;
    static constexpr uint64_t kFitContentPercentTag = 0x1f;

    static constexpr uint64_t kTagMask = 0xff;
    static constexpr uint64_t kCalcTagMask = 0x07;

    uint64_t Tag() const { return bits & kTagMask; }

    float Value() const {
        return __builtin_bit_cast(float, (uint32_t)(bits >> 32));
    }
    const void* CalcValue() const { return (const void*)(uintptr_t)bits; }

    bool IsCalc() const { return (bits & kCalcTagMask) == 0; }
    bool IsZero() const { return bits == FromVal(0.0f, kLengthTag).bits; }
    bool IsLengthOrPercentage() const {
        uint64_t t = Tag();
        return t == kLengthTag || t == kPercentTag;
    }
    bool IsAuto() const { return Tag() == kAutoTag; }
    bool IsMinContent() const { return Tag() == kMinContentTag; }
    bool IsMaxContent() const { return Tag() == kMaxContentTag; }
    bool IsFitContent() const {
        uint64_t t = Tag();
        return t == kFitContentPxTag || t == kFitContentPercentTag;
    }
    bool IsMaxOrFitContent() const {
        uint64_t t = Tag();
        return t == kMaxContentTag || t == kFitContentPxTag ||
               t == kFitContentPercentTag;
    }

    bool IsMaxContentAlike() const {
        uint64_t t = Tag();
        return t == kAutoTag || t == kMaxContentTag || t == kFitContentPxTag ||
               t == kFitContentPercentTag;
    }
    bool IsMinOrMaxContent() const {
        uint64_t t = Tag();
        return t == kMinContentTag || t == kMaxContentTag;
    }
    bool IsIntrinsic() const {
        uint64_t t = Tag();
        return t == kAutoTag || t == kMinContentTag || t == kMaxContentTag ||
               t == kFitContentPxTag || t == kFitContentPercentTag;
    }
    bool IsFr() const { return Tag() == kFrTag; }

    bool UsesPercentage() const {
        uint64_t t = Tag();
        return t == kPercentTag || t == kFitContentPercentTag || IsCalc();
    }

    static CompactLength FromVal(float v, uint64_t tag) {
        return CompactLength{((uint64_t)__builtin_bit_cast(uint32_t, v) << 32) |
                             tag};
    }
    static CompactLength FromTag(uint64_t tag) { return CompactLength{tag}; }
    static CompactLength FromPtr(const void* p) {
        return CompactLength{(uint64_t)(uintptr_t)p};
    }

    static CompactLength Length(float v) { return FromVal(v, kLengthTag); }

    static CompactLength Percent(float v) { return FromVal(v, kPercentTag); }
    static CompactLength Auto() { return FromTag(kAutoTag); }
    static CompactLength Fr(float v) { return FromVal(v, kFrTag); }
    static CompactLength MinContent() { return FromTag(kMinContentTag); }
    static CompactLength MaxContent() { return FromTag(kMaxContentTag); }
    static CompactLength FitContentPx(float limit) {
        return FromVal(limit, kFitContentPxTag);
    }
    static CompactLength FitContentPercent(float limit) {
        return FromVal(limit, kFitContentPercentTag);
    }
    static CompactLength Zero() { return Length(0.0f); }

    static CompactLength Calc(const void* p) { return FromPtr(p); }
};

inline bool operator==(CompactLength a, CompactLength b) {
    return a.bits == b.bits;
}

inline bool operator!=(CompactLength a, CompactLength b) {
    return a.bits != b.bits;
}

using CalcResolverFn = float (*)(const void* handle, float parentSize,
                                 void* ctx);

struct CalcResolver {
    CalcResolverFn fn = nullptr;
    void* ctx = nullptr;

    float Resolve(const void* handle, float parentSize) const {
        return fn ? fn(handle, parentSize, ctx) : 0.0f;
    }
};

inline Optf ResolvedPercentageSize(CompactLength cl, float parentSize,
                                   CalcResolver calc) {
    if (cl.Tag() == CompactLength::kPercentTag) {
        return Some(cl.Value() * parentSize);
    }
    if (cl.IsCalc()) {
        return Some(calc.Resolve(cl.CalcValue(), parentSize));
    }
    return None();
}

struct LengthPercentage {
    CompactLength raw = CompactLength::Zero();

    static LengthPercentage Length(float v) {
        return {CompactLength::Length(v)};
    }
    static LengthPercentage Percent(float v) {
        return {CompactLength::Percent(v)};
    }
    static LengthPercentage Calc(const void* p) {
        return {CompactLength::Calc(p)};
    }
    static LengthPercentage Zero() { return {CompactLength::Zero()}; }
};

struct LengthPercentageAuto {
    CompactLength raw = CompactLength::Zero();

    static LengthPercentageAuto Length(float v) {
        return {CompactLength::Length(v)};
    }
    static LengthPercentageAuto Percent(float v) {
        return {CompactLength::Percent(v)};
    }
    static LengthPercentageAuto Auto() { return {CompactLength::Auto()}; }
    static LengthPercentageAuto Calc(const void* p) {
        return {CompactLength::Calc(p)};
    }
    static LengthPercentageAuto Zero() { return {CompactLength::Zero()}; }
    static LengthPercentageAuto From(LengthPercentage lp) { return {lp.raw}; }

    bool IsAuto() const { return raw.IsAuto(); }

    Optf ResolveToOption(float context, CalcResolver calc) const;

    Optf MaybeResolve(Optf context, CalcResolver calc) const;
};

struct Dimension {
    CompactLength raw = CompactLength::Zero();

    static Dimension Length(float v) { return {CompactLength::Length(v)}; }
    static Dimension Percent(float v) { return {CompactLength::Percent(v)}; }
    static Dimension Auto() { return {CompactLength::Auto()}; }
    static Dimension Calc(const void* p) { return {CompactLength::Calc(p)}; }
    static Dimension Zero() { return {CompactLength::Zero()}; }
    static Dimension From(LengthPercentage lp) { return {lp.raw}; }
    static Dimension From(LengthPercentageAuto lp) { return {lp.raw}; }

    bool IsAuto() const { return raw.IsAuto(); }
    uint64_t Tag() const { return raw.Tag(); }
    float Value() const { return raw.Value(); }

    Optf MaybeResolve(Optf context, CalcResolver calc) const;

    Optf IntoOption() const {
        return raw.Tag() == CompactLength::kLengthTag ? Some(raw.Value())
                                                      : None();
    }
};

inline bool operator==(LengthPercentage a, LengthPercentage b) {
    return a.raw == b.raw;
}
inline bool operator!=(LengthPercentage a, LengthPercentage b) {
    return a.raw != b.raw;
}
inline bool operator==(LengthPercentageAuto a, LengthPercentageAuto b) {
    return a.raw == b.raw;
}
inline bool operator!=(LengthPercentageAuto a, LengthPercentageAuto b) {
    return a.raw != b.raw;
}
inline bool operator==(Dimension a, Dimension b) {
    return a.raw == b.raw;
}
inline bool operator!=(Dimension a, Dimension b) {
    return a.raw != b.raw;
}

struct AvailableSpace {
    enum class Kind : uint8_t {
        Definite,
        MinContent,
        MaxContent
    };

    Kind kind = Kind::MaxContent;
    float value = 0.0f;

    static AvailableSpace Definite(float v) { return {Kind::Definite, v}; }
    static AvailableSpace MinContent() { return {Kind::MinContent, 0.0f}; }
    static AvailableSpace MaxContent() { return {Kind::MaxContent, 0.0f}; }
    static AvailableSpace Zero() { return Definite(0.0f); }

    static AvailableSpace From(Optf v) {
        return IsSome(v) ? Definite(v) : MaxContent();
    }

    bool IsDefinite() const { return kind == Kind::Definite; }
    Optf IntoOption() const {
        return kind == Kind::Definite ? Some(value) : None();
    }
    float UnwrapOr(float def) const {
        return kind == Kind::Definite ? value : def;
    }

    float Unwrap() const { return value; }
    AvailableSpace Or(AvailableSpace def) const {
        return kind == Kind::Definite ? *this : def;
    }
    AvailableSpace MaybeSet(Optf v) const {
        return IsSome(v) ? Definite(v) : *this;
    }
    float ComputeFreeSpace(float usedSpace) const {
        switch (kind) {
            case Kind::MaxContent:
                return INFINITY;
            case Kind::MinContent:
                return 0.0f;
            default:
                return value - usedSpace;
        }
    }

    bool IsRoughlyEqual(AvailableSpace other) const {
        if (kind != other.kind) {
            return false;
        }
        if (kind != Kind::Definite) {
            return true;
        }
        return fabsf(value - other.value) < FLT_EPSILON;
    }
};

inline bool operator==(AvailableSpace a, AvailableSpace b) {
    return a.kind == b.kind &&
           (a.kind != AvailableSpace::Kind::Definite || a.value == b.value);
}

inline bool operator!=(AvailableSpace a, AvailableSpace b) {
    return !(a == b);
}

enum class AlignItemsKeyword : uint8_t {
    Start,
    End,
    FlexStart,
    FlexEnd,
    SelfStart,
    SelfEnd,
    Center,
    Baseline,
    Stretch
};

enum class AlignContentKeyword : uint8_t {
    Start,
    End,
    FlexStart,
    FlexEnd,
    Center,
    Stretch,
    SpaceBetween,
    SpaceEvenly,
    SpaceAround
};

constexpr AlignContentKeyword Reversed(AlignContentKeyword k) {
    switch (k) {
        case AlignContentKeyword::Start:
            return AlignContentKeyword::End;
        case AlignContentKeyword::End:
            return AlignContentKeyword::Start;
        case AlignContentKeyword::FlexStart:
            return AlignContentKeyword::FlexEnd;
        case AlignContentKeyword::FlexEnd:
            return AlignContentKeyword::FlexStart;
        case AlignContentKeyword::Stretch:
            return AlignContentKeyword::End;
        default:
            return k;
    }
}

enum class AlignmentSafety : uint8_t {
    Unsafe,
    Safe
};

struct AlignItems {
    AlignItemsKeyword keyword = AlignItemsKeyword::Start;
    AlignmentSafety safety = AlignmentSafety::Unsafe;

    constexpr bool IsSafe() const { return safety == AlignmentSafety::Safe; }
    constexpr AlignItemsKeyword Keyword() const { return keyword; }
};

struct AlignContent {
    AlignContentKeyword keyword = AlignContentKeyword::Start;
    AlignmentSafety safety = AlignmentSafety::Unsafe;

    constexpr bool IsSafe() const { return safety == AlignmentSafety::Safe; }
    constexpr AlignContentKeyword Keyword() const { return keyword; }
};

constexpr bool operator==(AlignItems a, AlignItems b) {
    return a.keyword == b.keyword && a.safety == b.safety;
}
constexpr bool operator!=(AlignItems a, AlignItems b) {
    return !(a == b);
}
constexpr bool operator==(AlignContent a, AlignContent b) {
    return a.keyword == b.keyword && a.safety == b.safety;
}
constexpr bool operator!=(AlignContent a, AlignContent b) {
    return !(a == b);
}

using JustifyItems = AlignItems;
using AlignSelf = AlignItems;
using JustifySelf = AlignItems;
using JustifyContent = AlignContent;

struct OptAlignItems {
    AlignItems val;
    bool has = false;

    constexpr OptAlignItems() = default;
    constexpr explicit OptAlignItems(AlignItems v) : val(v), has(true) {}

    constexpr bool IsSome() const { return has; }
    constexpr AlignItems UnwrapOr(AlignItems alt) const {
        return has ? val : alt;
    }
    constexpr OptAlignItems Or(OptAlignItems alt) const {
        return has ? *this : alt;
    }
};

using OptAlignSelf = OptAlignItems;

struct OptAlignContent {
    AlignContent val;
    bool has = false;

    constexpr OptAlignContent() = default;
    constexpr explicit OptAlignContent(AlignContent v) : val(v), has(true) {}

    constexpr bool IsSome() const { return has; }
    constexpr AlignContent UnwrapOr(AlignContent alt) const {
        return has ? val : alt;
    }
};

using OptJustifyContent = OptAlignContent;

constexpr bool operator==(OptAlignItems a, OptAlignItems b) {
    return a.has == b.has && (!a.has || a.val == b.val);
}
constexpr bool operator!=(OptAlignItems a, OptAlignItems b) {
    return !(a == b);
}
constexpr bool operator==(OptAlignContent a, OptAlignContent b) {
    return a.has == b.has && (!a.has || a.val == b.val);
}
constexpr bool operator!=(OptAlignContent a, OptAlignContent b) {
    return !(a == b);
}

enum class Display : uint8_t {
    Block,
    FlowRoot,
    Flex,
    Grid,
    None
};

enum class BoxGenerationMode : uint8_t {
    Normal,
    None
};

enum class Position : uint8_t {
    Relative,
    Absolute
};

enum class BoxSizing : uint8_t {
    BorderBox,
    ContentBox
};

enum class Overflow : uint8_t {
    Visible,
    Clip,
    Hidden,
    Scroll
};

constexpr bool IsScrollContainer(Overflow o) {
    return o == Overflow::Hidden || o == Overflow::Scroll;
}

constexpr Optf MaybeIntoAutomaticMinSize(Overflow o) {
    return IsScrollContainer(o) ? Some(0.0f) : None();
}

enum class Direction : uint8_t {
    Ltr,
    Rtl
};

constexpr bool IsRtl(Direction d) {
    return d == Direction::Rtl;
}

constexpr AlignItems ResolveSelfRelative(AlignItems value,
                                         Direction itemDirection,
                                         Direction containerDirection,
                                         bool axisIsInline) {
    bool flip = axisIsInline && itemDirection != containerDirection;
    if (value.keyword == AlignItemsKeyword::SelfStart) {
        value
            .keyword = flip ? AlignItemsKeyword::End : AlignItemsKeyword::Start;
    } else if (value.keyword == AlignItemsKeyword::SelfEnd) {
        value
            .keyword = flip ? AlignItemsKeyword::Start : AlignItemsKeyword::End;
    }
    return value;
}

enum class TextAlign : uint8_t {
    Auto,
    LegacyLeft,
    LegacyRight,
    LegacyCenter
};

enum class FlexWrap : uint8_t {
    NoWrap,
    Wrap,
    WrapReverse
};

enum class Float : uint8_t {
    Left,
    Right,
    None
};

enum class FloatDirection : uint8_t {
    Left = 0,
    Right = 1
};

constexpr bool IsFloated(Float f) {
    return f == Float::Left || f == Float::Right;
}

struct OptFloatDirection {
    FloatDirection val = FloatDirection::Left;
    bool has = false;

    constexpr OptFloatDirection() = default;
    constexpr explicit OptFloatDirection(FloatDirection v)
        : val(v), has(true) {}

    constexpr bool IsSome() const { return has; }
};

constexpr OptFloatDirection FloatDir(Float f) {
    switch (f) {
        case Float::Left:
            return OptFloatDirection(FloatDirection::Left);
        case Float::Right:
            return OptFloatDirection(FloatDirection::Right);
        default:
            return OptFloatDirection();
    }
}

enum class Clear : uint8_t {
    Left,
    Right,
    Both,
    None
};

struct SizeDim {
    Dimension width = Dimension::Auto();
    Dimension height = Dimension::Auto();

    static SizeDim Auto() { return {}; }
    static SizeDim FromLengths(float w, float h) {
        return {Dimension::Length(w), Dimension::Length(h)};
    }
    static SizeDim FromPercent(float w, float h) {
        return {Dimension::Percent(w), Dimension::Percent(h)};
    }

    Dimension GetAbs(AbsoluteAxis a) const {
        return a == AbsoluteAxis::Horizontal ? width : height;
    }
    Dimension Get(AbstractAxis a) const {
        return a == AbstractAxis::Inline ? width : height;
    }
    Dimension Main(FlexDirection d) const { return IsRow(d) ? width : height; }
    Dimension Cross(FlexDirection d) const { return IsRow(d) ? height : width; }

    SizeFOpt MaybeResolve(SizeFOpt context, CalcResolver calc) const;
    SizeF ResolveOrZero(SizeFOpt context, CalcResolver calc) const;
};

struct SizeLp {
    LengthPercentage width = LengthPercentage::Zero();
    LengthPercentage height = LengthPercentage::Zero();

    static SizeLp Zero() { return {}; }

    LengthPercentage GetAbs(AbsoluteAxis a) const {
        return a == AbsoluteAxis::Horizontal ? width : height;
    }
    LengthPercentage Get(AbstractAxis a) const {
        return a == AbstractAxis::Inline ? width : height;
    }
    LengthPercentage Main(FlexDirection d) const {
        return IsRow(d) ? width : height;
    }
    LengthPercentage Cross(FlexDirection d) const {
        return IsRow(d) ? height : width;
    }
    SizeF ResolveOrZero(SizeFOpt context, CalcResolver calc) const;
    SizeF ResolveOrZero(Optf context, CalcResolver calc) const;
};

struct SizeAvail {
    AvailableSpace width = AvailableSpace::MaxContent();
    AvailableSpace height = AvailableSpace::MaxContent();

    static SizeAvail MaxContent() {
        return {AvailableSpace::MaxContent(), AvailableSpace::MaxContent()};
    }
    static SizeAvail MinContent() {
        return {AvailableSpace::MinContent(), AvailableSpace::MinContent()};
    }
    static SizeAvail Definite(SizeF s) {
        return {AvailableSpace::Definite(s.w), AvailableSpace::Definite(s.h)};
    }
    static SizeAvail From(SizeFOpt s) {
        return {AvailableSpace::From(s.w), AvailableSpace::From(s.h)};
    }

    AvailableSpace GetAbs(AbsoluteAxis a) const {
        return a == AbsoluteAxis::Horizontal ? width : height;
    }
    AvailableSpace Get(AbstractAxis a) const {
        return a == AbstractAxis::Inline ? width : height;
    }
    void Set(AbstractAxis a, AvailableSpace v) {
        if (a == AbstractAxis::Inline) {
            width = v;
        } else {
            height = v;
        }
    }
    AvailableSpace Main(FlexDirection d) const {
        return IsRow(d) ? width : height;
    }
    AvailableSpace Cross(FlexDirection d) const {
        return IsRow(d) ? height : width;
    }
    void SetMain(FlexDirection d, AvailableSpace v) {
        if (IsRow(d)) {
            width = v;
        } else {
            height = v;
        }
    }
    void SetCross(FlexDirection d, AvailableSpace v) {
        if (IsRow(d)) {
            height = v;
        } else {
            width = v;
        }
    }
    SizeFOpt IntoOptions() const {
        return {width.IntoOption(), height.IntoOption()};
    }
    SizeAvail MaybeSet(SizeFOpt v) const {
        return {width.MaybeSet(v.w), height.MaybeSet(v.h)};
    }
};

inline bool operator==(SizeAvail a, SizeAvail b) {
    return a.width == b.width && a.height == b.height;
}

inline bool operator!=(SizeAvail a, SizeAvail b) {
    return !(a == b);
}

struct RectLp {
    LengthPercentage left = LengthPercentage::Zero();
    LengthPercentage right = LengthPercentage::Zero();
    LengthPercentage top = LengthPercentage::Zero();
    LengthPercentage bottom = LengthPercentage::Zero();

    static RectLp Zero() { return {}; }

    RectF ResolveOrZero(SizeFOpt context, CalcResolver calc) const;
    RectF ResolveOrZero(Optf context, CalcResolver calc) const;
};

struct RectLpa {
    LengthPercentageAuto left = LengthPercentageAuto::Zero();
    LengthPercentageAuto right = LengthPercentageAuto::Zero();
    LengthPercentageAuto top = LengthPercentageAuto::Zero();
    LengthPercentageAuto bottom = LengthPercentageAuto::Zero();

    static RectLpa Zero() { return {}; }
    static RectLpa Auto() {
        return {LengthPercentageAuto::Auto(), LengthPercentageAuto::Auto(),
                LengthPercentageAuto::Auto(), LengthPercentageAuto::Auto()};
    }

    LengthPercentageAuto MainStart(FlexDirection d) const {
        return IsRow(d) ? left : top;
    }
    LengthPercentageAuto MainEnd(FlexDirection d) const {
        return IsRow(d) ? right : bottom;
    }
    LengthPercentageAuto CrossStart(FlexDirection d) const {
        return IsRow(d) ? top : left;
    }
    LengthPercentageAuto CrossEnd(FlexDirection d) const {
        return IsRow(d) ? bottom : right;
    }
    RectF ResolveOrZero(SizeFOpt context, CalcResolver calc) const;
    RectF ResolveOrZero(Optf context, CalcResolver calc) const;

    RectFOpt MaybeResolve(Optf context, CalcResolver calc) const;

    RectFOpt MaybeResolveZip(SizeFOpt context, CalcResolver calc) const;
};

struct PointOverflow {
    Overflow x = Overflow::Visible;
    Overflow y = Overflow::Visible;

    Overflow Get(AbstractAxis a) const {
        return a == AbstractAxis::Inline ? x : y;
    }
    Overflow Main(FlexDirection d) const { return IsRow(d) ? x : y; }
    Overflow Cross(FlexDirection d) const { return IsRow(d) ? y : x; }
    PointOverflow Transpose() const { return {y, x}; }
};

inline bool operator==(SizeDim a, SizeDim b) {
    return a.width == b.width && a.height == b.height;
}
inline bool operator!=(SizeDim a, SizeDim b) {
    return !(a == b);
}
inline bool operator==(SizeLp a, SizeLp b) {
    return a.width == b.width && a.height == b.height;
}
inline bool operator!=(SizeLp a, SizeLp b) {
    return !(a == b);
}
inline bool operator==(RectLp a, RectLp b) {
    return a.left == b.left && a.right == b.right && a.top == b.top &&
           a.bottom == b.bottom;
}
inline bool operator!=(RectLp a, RectLp b) {
    return !(a == b);
}
inline bool operator==(RectLpa a, RectLpa b) {
    return a.left == b.left && a.right == b.right && a.top == b.top &&
           a.bottom == b.bottom;
}
inline bool operator!=(RectLpa a, RectLpa b) {
    return !(a == b);
}
inline bool operator==(PointOverflow a, PointOverflow b) {
    return a.x == b.x && a.y == b.y;
}
inline bool operator!=(PointOverflow a, PointOverflow b) {
    return !(a == b);
}

struct GridLine {
    int16_t v = 0;

    int16_t AsI16() const { return v; }
};

struct OriginZeroLine {
    int16_t v = 0;
};

constexpr bool operator==(GridLine a, GridLine b) {
    return a.v == b.v;
}
constexpr bool operator!=(GridLine a, GridLine b) {
    return a.v != b.v;
}
constexpr bool operator==(OriginZeroLine a, OriginZeroLine b) {
    return a.v == b.v;
}
constexpr bool operator!=(OriginZeroLine a, OriginZeroLine b) {
    return a.v != b.v;
}
constexpr bool operator<(OriginZeroLine a, OriginZeroLine b) {
    return a.v < b.v;
}
constexpr bool operator>(OriginZeroLine a, OriginZeroLine b) {
    return a.v > b.v;
}
constexpr bool operator<=(OriginZeroLine a, OriginZeroLine b) {
    return a.v <= b.v;
}
constexpr bool operator>=(OriginZeroLine a, OriginZeroLine b) {
    return a.v >= b.v;
}
constexpr OriginZeroLine operator+(OriginZeroLine a, OriginZeroLine b) {
    return {(int16_t)(a.v + b.v)};
}
constexpr OriginZeroLine operator-(OriginZeroLine a, OriginZeroLine b) {
    return {(int16_t)(a.v - b.v)};
}
constexpr OriginZeroLine operator+(OriginZeroLine a, uint16_t b) {
    return {(int16_t)(a.v + (int16_t)b)};
}
constexpr OriginZeroLine operator-(OriginZeroLine a, uint16_t b) {
    return {(int16_t)(a.v - (int16_t)b)};
}

struct OptOriginZeroLine {
    OriginZeroLine val;
    bool has = false;

    constexpr OptOriginZeroLine() = default;
    constexpr explicit OptOriginZeroLine(OriginZeroLine v)
        : val(v), has(true) {}

    constexpr bool IsSome() const { return has; }
};

constexpr uint16_t kMaxGridTracks = 10000;
constexpr int16_t kMinOzLine = -(int16_t)kMaxGridTracks;
constexpr int16_t kMaxOzLine = (int16_t)kMaxGridTracks;

OriginZeroLine IntoOriginZeroLine(GridLine line, uint16_t explicitTrackCount);

constexpr uint16_t ImpliedNegativeImplicitTracks(OriginZeroLine l) {
    return l.v < 0 ? (uint16_t)(-(int32_t)l.v) : (uint16_t)0;
}

constexpr uint16_t ImpliedPositiveImplicitTracks(OriginZeroLine l,
                                                 uint16_t explicitTrackCount) {
    return l.v > (int16_t)explicitTrackCount
               ? (uint16_t)((uint16_t)l.v - explicitTrackCount)
               : (uint16_t)0;
}

struct LineOzl {
    OriginZeroLine start;
    OriginZeroLine end;

    uint16_t Span() const {
        int32_t d = (int32_t)end.v - (int32_t)start.v;
        return d > 0 ? (uint16_t)d : (uint16_t)0;
    }
};

struct LineOptOzl {
    OptOriginZeroLine start;
    OptOriginZeroLine end;
};

enum class GridAutoFlow : uint8_t {
    Row,
    Column,
    RowDense,
    ColumnDense
};

constexpr bool IsDense(GridAutoFlow f) {
    return f == GridAutoFlow::RowDense || f == GridAutoFlow::ColumnDense;
}

constexpr AbsoluteAxis PrimaryAxis(GridAutoFlow f) {
    return (f == GridAutoFlow::Row || f == GridAutoFlow::RowDense)
               ? AbsoluteAxis::Horizontal
               : AbsoluteAxis::Vertical;
}

enum class GridPlacementKind : uint8_t {
    Auto,
    Line,
    Span,
    NamedLine,
    NamedSpan
};

struct GridPlacement {
    GridPlacementKind kind = GridPlacementKind::Auto;
    int16_t line = 0;
    uint16_t span = 0;
    Str name;

    static GridPlacement Auto() { return {}; }
    static GridPlacement FromLineIndex(int16_t index) {
        GridPlacement p;
        p.kind = GridPlacementKind::Line;
        p.line = index;
        return p;
    }
    static GridPlacement FromSpan(uint16_t span) {
        GridPlacement p;
        p.kind = GridPlacementKind::Span;
        p.span = span;
        return p;
    }
    static GridPlacement FromNamedLine(Str name, int16_t index) {
        GridPlacement p;
        p.kind = GridPlacementKind::NamedLine;
        p.name = name;
        p.line = index;
        return p;
    }
    static GridPlacement FromNamedSpan(Str name, uint16_t span) {
        GridPlacement p;
        p.kind = GridPlacementKind::NamedSpan;
        p.name = name;
        p.span = span;
        return p;
    }
};

struct PlainPlacement {
    GridPlacementKind kind = GridPlacementKind::Auto;
    int16_t line = 0;
    uint16_t span = 0;

    static PlainPlacement Auto() { return {}; }
    static PlainPlacement AtLine(int16_t l) {
        return {GridPlacementKind::Line, l, 0};
    }
    static PlainPlacement Spanning(uint16_t s) {
        return {GridPlacementKind::Span, 0, s};
    }

    bool IsAuto() const { return kind == GridPlacementKind::Auto; }
    bool IsLine() const { return kind == GridPlacementKind::Line; }
    bool IsSpan() const { return kind == GridPlacementKind::Span; }
    OriginZeroLine Ozl() const { return OriginZeroLine{line}; }
};

constexpr bool operator==(PlainPlacement a, PlainPlacement b) {
    return a.kind == b.kind && a.line == b.line && a.span == b.span;
}
constexpr bool operator!=(PlainPlacement a, PlainPlacement b) {
    return !(a == b);
}

struct LinePlacement {
    GridPlacement start;
    GridPlacement end;

    bool IsDefinite() const;

    struct LinePlain IntoOriginZeroIgnoringNamed(
        uint16_t explicitTrackCount) const;
};

struct LinePlain {
    PlainPlacement start;
    PlainPlacement end;

    bool IsDefinite() const;

    bool IsDefiniteGridLine() const;

    LinePlain IntoOriginZero(uint16_t explicitTrackCount) const;

    uint16_t IndefiniteSpan() const;

    LineOzl ResolveDefiniteGridLines() const;

    LineOptOzl ResolveAbsolutelyPositionedGridTracks() const;

    LineOzl ResolveIndefiniteGridTracks(OriginZeroLine start) const;
};

struct MaxTrackSizingFunction {
    CompactLength raw = CompactLength::Auto();

    static MaxTrackSizingFunction Length(float v) {
        return {CompactLength::Length(v)};
    }
    static MaxTrackSizingFunction Percent(float v) {
        return {CompactLength::Percent(v)};
    }
    static MaxTrackSizingFunction Auto() { return {CompactLength::Auto()}; }
    static MaxTrackSizingFunction MinContent() {
        return {CompactLength::MinContent()};
    }
    static MaxTrackSizingFunction MaxContent() {
        return {CompactLength::MaxContent()};
    }
    static MaxTrackSizingFunction FitContentPx(float v) {
        return {CompactLength::FitContentPx(v)};
    }
    static MaxTrackSizingFunction FitContentPercent(float v) {
        return {CompactLength::FitContentPercent(v)};
    }
    static MaxTrackSizingFunction FitContent(LengthPercentage lp) {
        return lp.raw.Tag() == CompactLength::kPercentTag
                   ? FitContentPercent(lp.raw.Value())
                   : FitContentPx(lp.raw.Value());
    }
    static MaxTrackSizingFunction Fr(float v) { return {CompactLength::Fr(v)}; }
    static MaxTrackSizingFunction Zero() { return {CompactLength::Zero()}; }
    static MaxTrackSizingFunction From(LengthPercentage v) { return {v.raw}; }
    static MaxTrackSizingFunction From(LengthPercentageAuto v) {
        return {v.raw};
    }
    static MaxTrackSizingFunction From(Dimension v) { return {v.raw}; }

    bool IsIntrinsic() const { return raw.IsIntrinsic(); }
    bool IsMaxContentAlike() const { return raw.IsMaxContentAlike(); }
    bool IsFr() const { return raw.IsFr(); }
    bool IsAuto() const { return raw.IsAuto(); }
    bool IsMinContent() const { return raw.IsMinContent(); }
    bool IsMaxContent() const { return raw.IsMaxContent(); }
    bool IsFitContent() const { return raw.IsFitContent(); }
    bool IsMaxOrFitContent() const { return raw.IsMaxOrFitContent(); }
    bool UsesPercentage() const { return raw.UsesPercentage(); }

    bool HasDefiniteValue(Optf parentSize) const;
    Optf DefiniteValue(Optf parentSize, CalcResolver calc) const;

    Optf DefiniteLimit(Optf parentSize, CalcResolver calc) const;
    Optf ResolvedPercentageSize(float parentSize, CalcResolver calc) const {
        return taffy::ResolvedPercentageSize(raw, parentSize, calc);
    }
};

struct MinTrackSizingFunction {
    CompactLength raw = CompactLength::Auto();

    static MinTrackSizingFunction Length(float v) {
        return {CompactLength::Length(v)};
    }
    static MinTrackSizingFunction Percent(float v) {
        return {CompactLength::Percent(v)};
    }
    static MinTrackSizingFunction Auto() { return {CompactLength::Auto()}; }
    static MinTrackSizingFunction MinContent() {
        return {CompactLength::MinContent()};
    }
    static MinTrackSizingFunction MaxContent() {
        return {CompactLength::MaxContent()};
    }
    static MinTrackSizingFunction Zero() { return {CompactLength::Zero()}; }
    static MinTrackSizingFunction From(LengthPercentage v) { return {v.raw}; }
    static MinTrackSizingFunction From(LengthPercentageAuto v) {
        return {v.raw};
    }
    static MinTrackSizingFunction From(Dimension v) { return {v.raw}; }

    static MinTrackSizingFunction From(MaxTrackSizingFunction v) {
        if (v.raw.IsFr() || v.raw.IsFitContent()) {
            return Auto();
        }
        return {v.raw};
    }

    bool IsAuto() const { return raw.IsAuto(); }
    bool IsMinContent() const { return raw.IsMinContent(); }
    bool IsMaxContent() const { return raw.IsMaxContent(); }
    bool IsMinOrMaxContent() const { return raw.IsMinOrMaxContent(); }
    bool UsesPercentage() const {
        return raw.Tag() == CompactLength::kPercentTag || raw.IsCalc();
    }

    Optf DefiniteValue(Optf parentSize, CalcResolver calc) const;
    Optf ResolvedPercentageSize(float parentSize, CalcResolver calc) const {
        return taffy::ResolvedPercentageSize(raw, parentSize, calc);
    }
};

struct TrackSizingFunction {
    MinTrackSizingFunction min = MinTrackSizingFunction::Auto();
    MaxTrackSizingFunction max = MaxTrackSizingFunction::Auto();

    static TrackSizingFunction Auto() { return {}; }
    static TrackSizingFunction MinContent() {
        return {MinTrackSizingFunction::MinContent(),
                MaxTrackSizingFunction::MinContent()};
    }
    static TrackSizingFunction MaxContent() {
        return {MinTrackSizingFunction::MaxContent(),
                MaxTrackSizingFunction::MaxContent()};
    }
    static TrackSizingFunction Zero() {
        return {MinTrackSizingFunction::Zero(), MaxTrackSizingFunction::Zero()};
    }
    static TrackSizingFunction Length(float v) {
        return {MinTrackSizingFunction::Length(v),
                MaxTrackSizingFunction::Length(v)};
    }
    static TrackSizingFunction Percent(float v) {
        return {MinTrackSizingFunction::Percent(v),
                MaxTrackSizingFunction::Percent(v)};
    }
    static TrackSizingFunction Fr(float v) {
        return {MinTrackSizingFunction::Auto(), MaxTrackSizingFunction::Fr(v)};
    }
    static TrackSizingFunction FitContent(LengthPercentage lp) {
        return {MinTrackSizingFunction::Auto(),
                MaxTrackSizingFunction::FitContent(lp)};
    }
    static TrackSizingFunction MinMax(MinTrackSizingFunction mn,
                                      MaxTrackSizingFunction mx) {
        return {mn, mx};
    }
    static TrackSizingFunction From(LengthPercentage v) {
        return {MinTrackSizingFunction::From(v),
                MaxTrackSizingFunction::From(v)};
    }

    MinTrackSizingFunction MinSizingFunction() const { return min; }
    MaxTrackSizingFunction MaxSizingFunction() const { return max; }

    bool HasFixedComponent() const {
        return min.raw.IsLengthOrPercentage() || max.raw.IsLengthOrPercentage();
    }
};

struct RepetitionCount {
    enum class Kind : uint8_t {
        AutoFill,
        AutoFit,
        Count
    };
    Kind kind = Kind::Count;
    uint16_t count = 1;

    static RepetitionCount AutoFill() { return {Kind::AutoFill, 0}; }
    static RepetitionCount AutoFit() { return {Kind::AutoFit, 0}; }
    static RepetitionCount Exactly(uint16_t n) { return {Kind::Count, n}; }

    bool IsAuto() const {
        return kind == Kind::AutoFill || kind == Kind::AutoFit;
    }
};

constexpr bool operator==(RepetitionCount a, RepetitionCount b) {
    return a.kind == b.kind &&
           (a.kind != RepetitionCount::Kind::Count || a.count == b.count);
}

struct LineNameSet {
    Slice<Str> names;
};

struct GridTemplateRepetition {
    RepetitionCount count;
    Slice<TrackSizingFunction> tracks;
    Slice<LineNameSet> lineNames;

    uint16_t TrackCount() const { return (uint16_t)tracks.len; }
};

struct GridTemplateComponent {
    bool isRepeat = false;
    TrackSizingFunction single;
    GridTemplateRepetition repeat;

    static GridTemplateComponent Single(TrackSizingFunction t) {
        GridTemplateComponent c;
        c.single = t;
        return c;
    }
    static GridTemplateComponent Repeat(GridTemplateRepetition r) {
        GridTemplateComponent c;
        c.isRepeat = true;
        c.repeat = r;
        return c;
    }

    bool IsAutoRepetition() const { return isRepeat && repeat.count.IsAuto(); }
};

struct GridTemplateArea {
    Str name;
    uint16_t rowStart = 0;
    uint16_t rowEnd = 0;
    uint16_t columnStart = 0;
    uint16_t columnEnd = 0;
};

struct GridTemplateAreas {
    Slice<GridTemplateArea> areas;
    uint16_t rowCount = 0;
    uint16_t columnCount = 0;
};

enum class GridAreaAxis : uint8_t {
    Row,
    Column
};

enum class GridAreaEnd : uint8_t {
    Start,
    End
};

struct Style {
    Display display = Display::Flex;

    bool itemIsTable = false;

    bool itemIsReplaced = false;
    BoxSizing boxSizing = BoxSizing::BorderBox;
    Direction direction = Direction::Ltr;

    PointOverflow overflow;
    float scrollbarWidth = 0.0f;

    Float floatMode = Float::None;
    Clear clear = Clear::None;

    Position position = Position::Relative;
    RectLpa inset = RectLpa::Auto();

    SizeDim size;
    SizeDim minSize;
    SizeDim maxSize;
    Optf aspectRatio = None();

    RectLpa margin;
    RectLp padding;
    RectLp border;

    OptAlignItems alignItems;
    OptAlignSelf alignSelf;
    OptAlignItems justifyItems;
    OptAlignSelf justifySelf;
    OptAlignContent alignContent;
    OptJustifyContent justifyContent;
    SizeLp gap;

    TextAlign textAlign = TextAlign::Auto;

    FlexDirection flexDirection = FlexDirection::Row;
    FlexWrap flexWrap = FlexWrap::NoWrap;

    Dimension flexBasis = Dimension::Auto();
    float flexGrow = 0.0f;
    float flexShrink = 1.0f;

    Slice<GridTemplateComponent> gridTemplateRows;
    Slice<GridTemplateComponent> gridTemplateColumns;
    Slice<TrackSizingFunction> gridAutoRows;
    Slice<TrackSizingFunction> gridAutoColumns;
    GridAutoFlow gridAutoFlow = GridAutoFlow::Row;

    GridTemplateAreas gridTemplateAreas;
    Slice<LineNameSet> gridTemplateColumnNames;
    Slice<LineNameSet> gridTemplateRowNames;

    LinePlacement gridRow;
    LinePlacement gridColumn;

    BoxGenerationMode BoxGenMode() const {
        return display == Display::None ? BoxGenerationMode::None
                                        : BoxGenerationMode::Normal;
    }
    bool IsBlock() const { return display == Display::Block; }
    bool IsCompressibleReplaced() const { return itemIsReplaced; }
};

bool operator==(const Style& a, const Style& b);

inline bool operator!=(const Style& a, const Style& b) {
    return !(a == b);
}

inline bool SameOptf(Optf a, Optf b) {
    uint32_t ab = 0;
    uint32_t bb = 0;
    memcpy(&ab, &a, sizeof(ab));
    memcpy(&bb, &b, sizeof(bb));
    return ab == bb;
}

inline bool SameFloatBits(float a, float b) {
    uint32_t ab = 0;
    uint32_t bb = 0;
    memcpy(&ab, &a, sizeof(ab));
    memcpy(&bb, &b, sizeof(bb));
    return ab == bb;
}

inline bool operator==(MinTrackSizingFunction a, MinTrackSizingFunction b) {
    return a.raw == b.raw;
}

inline bool operator==(MaxTrackSizingFunction a, MaxTrackSizingFunction b) {
    return a.raw == b.raw;
}

inline bool operator==(TrackSizingFunction a, TrackSizingFunction b) {
    return a.min == b.min && a.max == b.max;
}

inline bool operator==(const GridPlacement& a, const GridPlacement& b) {
    return a.kind == b.kind && a.line == b.line && a.span == b.span &&
           base::StrEq(a.name, b.name);
}

inline bool operator==(const LinePlacement& a, const LinePlacement& b) {
    return a.start == b.start && a.end == b.end;
}

inline bool operator==(const GridTemplateArea& a, const GridTemplateArea& b) {
    return base::StrEq(a.name, b.name) && a.rowStart == b.rowStart &&
           a.rowEnd == b.rowEnd && a.columnStart == b.columnStart &&
           a.columnEnd == b.columnEnd;
}

template <typename T>
inline bool SameSlice(Slice<T> a, Slice<T> b) {
    if (a.len != b.len) {
        return false;
    }
    for (int i = 0; i < a.len; i++) {
        if (!(a.els[i] == b.els[i])) {
            return false;
        }
    }
    return true;
}

inline bool SameNames(Slice<base::Str> a, Slice<base::Str> b) {
    if (a.len != b.len) {
        return false;
    }
    for (int i = 0; i < a.len; i++) {
        if (!base::StrEq(a.els[i], b.els[i])) {
            return false;
        }
    }
    return true;
}

inline bool operator==(const LineNameSet& a, const LineNameSet& b) {
    return SameNames(a.names, b.names);
}

inline bool operator==(const GridTemplateRepetition& a,
                       const GridTemplateRepetition& b) {
    return a.count == b.count && SameSlice(a.tracks, b.tracks) &&
           SameSlice(a.lineNames, b.lineNames);
}

inline bool operator==(const GridTemplateComponent& a,
                       const GridTemplateComponent& b) {
    if (a.isRepeat != b.isRepeat) {
        return false;
    }
    return a.isRepeat ? a.repeat == b.repeat : a.single == b.single;
}

}

#line 1 "src/taffy/tree.h"

namespace taffy {

struct NodeId {
    uint64_t raw = 0;

    static constexpr NodeId New(uint64_t v) { return NodeId{v}; }
    constexpr uint64_t AsU64() const { return raw; }
};

constexpr bool operator==(NodeId a, NodeId b) {
    return a.raw == b.raw;
}

constexpr bool operator!=(NodeId a, NodeId b) {
    return a.raw != b.raw;
}

enum class RunMode : uint8_t {

    PerformLayout,

    ComputeSize,

    PerformHiddenLayout
};

enum class SizingMode : uint8_t {

    ContentSize,

    InherentSize
};

enum class RequestedAxis : uint8_t {
    Horizontal,
    Vertical,
    Both
};

constexpr RequestedAxis ToRequestedAxis(AbsoluteAxis a) {
    return a == AbsoluteAxis::Horizontal ? RequestedAxis::Horizontal
                                         : RequestedAxis::Vertical;
}

struct CollapsibleMarginSet {

    float positive = 0.0f;

    float negative = 0.0f;

    static constexpr CollapsibleMarginSet Zero() { return {}; }

    static CollapsibleMarginSet FromMargin(float margin) {
        if (margin >= 0.0f) {
            return {margin, 0.0f};
        }
        return {0.0f, margin};
    }
    CollapsibleMarginSet CollapseWithMargin(float margin) const {
        CollapsibleMarginSet out = *this;
        if (margin >= 0.0f) {
            out.positive = F32Max(out.positive, margin);
        } else {
            out.negative = F32Min(out.negative, margin);
        }
        return out;
    }
    CollapsibleMarginSet CollapseWithSet(CollapsibleMarginSet other) const {
        return {F32Max(positive, other.positive),
                F32Min(negative, other.negative)};
    }

    float Resolve() const { return positive + negative; }
};

struct LayoutInput {

    RunMode runMode = RunMode::PerformLayout;

    SizingMode sizingMode = SizingMode::InherentSize;

    RequestedAxis axis = RequestedAxis::Both;

    SizeFOpt knownDimensions = SizeFOptNone();

    SizeFOpt parentSize = SizeFOptNone();

    SizeAvail availableSpace = SizeAvail::MaxContent();

    LineBool verticalMarginsAreCollapsible;

    static LayoutInput Hidden() {
        LayoutInput in;
        in.runMode = RunMode::PerformHiddenLayout;
        return in;
    }
};

struct LayoutOutput {
    SizeF size;

    SizeF contentSize;

    PointFOpt firstBaselines = PointFOptNone();

    CollapsibleMarginSet topMargin;
    CollapsibleMarginSet bottomMargin;

    bool marginsCanCollapseThrough = false;

    static LayoutOutput Hidden() { return {}; }

    static LayoutOutput FromSizesAndBaselines(SizeF size, SizeF contentSize,
                                              PointFOpt firstBaselines) {
        LayoutOutput out;
        out.size = size;
        out.contentSize = contentSize;
        out.firstBaselines = firstBaselines;
        return out;
    }
    static LayoutOutput FromSizes(SizeF size, SizeF contentSize) {
        return FromSizesAndBaselines(size, contentSize, PointFOptNone());
    }
    static LayoutOutput FromOuterSize(SizeF size) {
        return FromSizes(size, SizeF::Zero());
    }
};

struct Layout {

    uint32_t order = 0;

    PointF location;
    SizeF size;

    SizeF contentSize;

    SizeF scrollbarSize;
    RectF border;
    RectF padding;
    RectF margin;

    static Layout New() { return {}; }
    static Layout WithOrder(uint32_t order) {
        Layout l;
        l.order = order;
        return l;
    }

    float ContentBoxWidth() const {
        return size.w - padding.left - padding.right - border.left -
               border.right;
    }
    float ContentBoxHeight() const {
        return size.h - padding.top - padding.bottom - border.top -
               border.bottom;
    }
    SizeF ContentBoxSize() const {
        return {ContentBoxWidth(), ContentBoxHeight()};
    }

    float ContentBoxX() const {
        return location.x + border.left + padding.left;
    }
    float ContentBoxY() const { return location.y + border.top + padding.top; }

    float ScrollWidth() const {
        return F32Max(0.0f, contentSize.w + F32Min(scrollbarSize.w, size.w) -
                                size.w + border.left + border.right);
    }
    float ScrollHeight() const {
        return F32Max(0.0f, contentSize.h + F32Min(scrollbarSize.h, size.h) -
                                size.h + border.top + border.bottom);
    }
};

constexpr int kCacheSize = 9;

struct CacheKey {
    uint64_t kdAvailableSpace = 0;
    uint64_t parentSize = 0;

    static CacheKey From(const LayoutInput& input);

    uint64_t XAxisParentSize() const;
};

constexpr bool operator==(CacheKey a, CacheKey b) {
    return a.kdAvailableSpace == b.kdAvailableSpace &&
           a.parentSize == b.parentSize;
}

template <typename T>
struct CacheEntry {
    CacheKey key;
    T content{};
};

struct Cache {
    CacheEntry<LayoutOutput> finalLayoutEntry;
    CacheEntry<SizeF> measureEntries[kCacheSize];

    uint16_t presentMask = 0;

    bool isEmpty = true;

    static constexpr uint16_t kFinalBit = 1;
    static constexpr uint16_t MeasureBit(int i) {
        return (uint16_t)(1u << (i + 1));
    }

    bool Get(const LayoutInput& input, LayoutOutput* out) const;
    void Store(const LayoutInput& input, const LayoutOutput& output);

    bool GetWithKey(CacheKey key, RunMode runMode, LayoutOutput* out) const;
    void StoreWithKey(CacheKey key, const LayoutInput& input,
                      const LayoutOutput& output);

    bool Clear();
    bool IsEmpty() const;
};

}

#line 1 "src/taffy/taffy_tree.h"

namespace taffy {

struct BlockContext;

using MeasureFn = SizeF (*)(SizeFOpt knownDimensions, SizeAvail availableSpace,
                            NodeId node, void* nodeContext, const Style* style,
                            void* userData);

struct NodeData {
    Style style;

    Layout unroundedLayout;

    Layout finalLayout;

    bool hasContext = false;
    void* context = nullptr;

    Cache cache;

    uint32_t generation = 0;
    bool alive = false;

    Vec<NodeId> children;
    NodeId parent;
    bool hasParent = false;
};

struct TaffyTree {

    Vec<NodeData*> slots;
    Vec<int32_t> freeSlots;
    int32_t liveCount = 0;

    int32_t allocs = 0;

    bool useRounding = true;

    MeasureFn measureFn = nullptr;
    void* measureUserData = nullptr;

    Arena* styleArena = nullptr;

    CalcResolver calc;

    void Init(int capacity = 16);
    void Free();

    void EnableRounding() { useRounding = true; }
    void DisableRounding() { useRounding = false; }

    NodeId NewLeaf(const Style& style);
    NodeId NewLeafWithContext(const Style& style, void* context);
    NodeId NewWithChildren(const Style& style, const NodeId* children, int n);

    void Clear();

    void Remove(NodeId node);

    void EachUnreachable(NodeId root, void (*fn)(NodeId, void*), void* user);

    void SetNodeContext(NodeId node, void* context, bool hasContext);
    void* GetNodeContext(NodeId node) const;

    void AddChild(NodeId parent, NodeId child);

    bool InsertChildAtIndex(NodeId parent, int childIndex, NodeId child);
    void SetChildren(NodeId parent, const NodeId* children, int n);

    NodeId RemoveChild(NodeId parent, NodeId child);

    NodeId RemoveChildAtIndex(NodeId parent, int childIndex);
    void RemoveChildrenRange(NodeId parent, int start, int end);

    NodeId ReplaceChildAtIndex(NodeId parent, int childIndex, NodeId newChild);

    NodeId ChildAtIndex(NodeId parent, int childIndex) const;
    int TotalNodeCount() const { return liveCount; }

    int SlotCount() const { return len(slots); }

    NodeId Parent(NodeId child, bool* hasParent) const;

    void SetStyle(NodeId node, const Style& style);
    const Style& GetStyle(NodeId node) const;

    const Layout& GetLayout(NodeId node) const;
    const Layout& UnroundedLayout(NodeId node) const;

    void MarkDirty(NodeId node);
    bool Dirty(NodeId node) const;

    void ComputeLayoutWithMeasure(NodeId node, SizeAvail availableSpace,
                                  MeasureFn measure, void* userData);
    void ComputeLayout(NodeId node, SizeAvail availableSpace);

    void PrintTree(NodeId root);

    int ChildCount(NodeId parent) const;
    NodeId GetChildId(NodeId parent, int index) const;

    void SetUnroundedLayout(NodeId node, const Layout& layout);
    Layout GetUnroundedLayout(NodeId node) const;
    void SetFinalLayout(NodeId node, const Layout& layout);
    Layout GetFinalLayout(NodeId node) const;

    const char* GetDebugLabel(NodeId node) const;

    bool CacheGet(NodeId node, const LayoutInput& input,
                  LayoutOutput* out) const;
    void CacheStore(NodeId node, const LayoutInput& input,
                    const LayoutOutput& output);
    void CacheClear(NodeId node);

    float ResolveCalcValue(const void* handle, float basis) const {
        return calc.Resolve(handle, basis);
    }

    LayoutOutput ComputeChildLayout(NodeId node, LayoutInput inputs);
    LayoutOutput ComputeBlockChildLayout(NodeId node, LayoutInput inputs,
                                         BlockContext* blockCtx);

    float MeasureChildSize(NodeId node, SizeFOpt knownDimensions,
                           SizeFOpt parentSize, SizeAvail availableSpace,
                           SizingMode sizingMode, AbsoluteAxis axis,
                           LineBool verticalMarginsAreCollapsible);
    SizeF MeasureChildSizeBoth(NodeId node, SizeFOpt knownDimensions,
                               SizeFOpt parentSize, SizeAvail availableSpace,
                               SizingMode sizingMode,
                               LineBool verticalMarginsAreCollapsible);
    LayoutOutput PerformChildLayout(NodeId node, SizeFOpt knownDimensions,
                                    SizeFOpt parentSize,
                                    SizeAvail availableSpace,
                                    SizingMode sizingMode,
                                    LineBool verticalMarginsAreCollapsible);

    NodeData* Get(NodeId node) const;
};

}

#line 1 "src/gpui/gpui.h"

namespace gpui {
using namespace base;
}

namespace base {
int StrToIntUnchecked(Str s);
}

namespace gpui {

struct App;
struct Window;

using WindowShouldCloseFn = bool (*)(void* data, Window* win);
struct KeyChord;

struct ClipboardItem {
    Str text = {};
    const uint8_t* imageBytes = nullptr;
    int imageBytesLen = 0;
    Str externalPaths = {};

    bool HasImage() const { return imageBytes && imageBytesLen > 0; }
    bool HasExternalPaths() const { return len(externalPaths) > 0; }
    bool IsEmpty() const {
        return len(text) <= 0 && !HasImage() && !HasExternalPaths();
    }
};

using InputPasteFn = bool (*)(void* data, const ClipboardItem& item, App* app,
                              Window* win);

struct Rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

inline Rgba Rgb(uint8_t r, uint8_t g, uint8_t b) {
    return Rgba{r, g, b, 255};
}
inline Rgba Rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return Rgba{r, g, b, a};
}
inline bool RgbaEq(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
inline Rgba RgbaHex(uint32_t hex) {

    if (hex > 0xFFFFFFu) {
        return Rgba{(uint8_t)((hex >> 16) & 0xff), (uint8_t)((hex >> 8) & 0xff),
                    (uint8_t)(hex & 0xff), (uint8_t)((hex >> 24) & 0xff)};
    }
    return Rgba{(uint8_t)((hex >> 16) & 0xff), (uint8_t)((hex >> 8) & 0xff),
                (uint8_t)(hex & 0xff), 255};
}
Rgba RgbaOpacity(Rgba c, float a01);

Rgba RgbaMix(Rgba a, Rgba b, float t);

struct Hsla {
    float h = 0;
    float s = 0;
    float l = 0;
    float a = 0;
};

Hsla HslaNew(float h, float s, float l, float a);

Hsla HslaFromRgba(Rgba c);

Rgba HslaToRgba(Hsla c);

Rgba RgbaHsla(float h, float s, float l, float a01);

Rgba RgbaWithHue(Rgba c, float h01);

Rgba RgbaMixHsl(Rgba a, Rgba b, float factor);

struct ColorStop {
    Rgba color = {};

    float percentage = 0;
};

struct Background {
    Rgba color = {};
    ColorStop from = {};
    ColorStop to = {};

    float angle = 180.f;
    bool gradient = false;

    Background() = default;

    Background(Rgba c) : color(c) {}
};

struct BoxShadow {
    float x = 0;
    float y = 0;
    float blur = 0;
    float spread = 0;
    Rgba color = {};
    bool inset = false;
};

Background BackgroundLinear(float angle, ColorStop from, ColorStop to);
inline ColorStop ColorStopAt(Rgba c, float pct) {
    return ColorStop{c, pct};
}

Background BackgroundOpacity(Background b, float factor);

Background BackgroundClampAlpha(Background b, float max);
inline bool BackgroundIsSolid(const Background& b) {
    return !b.gradient;
}

struct InputState;

struct RangeDecorationsState;
struct DecorationCollectionsState;

struct InputSyntaxCache;

constexpr float kAuto = -1.f;
constexpr float kFill = -2.f;
constexpr float kPi = 3.14159265358979f;

Rgba RgbaTransparent();

Rgba RgbaBlend(Rgba base, Rgba over);

Rgba RgbaLighten(Rgba c, float amount);
Rgba RgbaDarken(Rgba c, float amount);

Str RgbaToHex(Arena* a, Rgba c, bool upper = true);

Rgba RgbaMixOklab(Rgba a, Rgba b, float factor);

Rgba RgbaOklch(float lightness, float chroma, float hueDegrees);

enum class Axis : uint8_t {
    Horizontal,
    Vertical
};

using Point = base::PointF;
using Size = base::SizeF;
using Edges = base::RectF;

struct Bounds {
    float x = 0, y = 0, w = 0, h = 0;

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float CenterX() const { return x + w * 0.5f; }
    float CenterY() const { return y + h * 0.5f; }

    bool Contains(Point p) const {
        return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
    }

    Bounds Inset(float d) const { return {x + d, y + d, w - d - d, h - d - d}; }

    Bounds Inset(Edges e) const {
        return {x + e.left, y + e.top, w - e.HorizontalAxisSum(),
                h - e.VerticalAxisSum()};
    }
};

inline Bounds BoundsAt(Point origin, Size size) {
    return {origin.x, origin.y, size.w, size.h};
}

void BackgroundLine(const Background& b, Bounds box, Point* p0, Point* p1);

struct Window;
struct Ctx;
struct El;
struct SliderState;

struct TextLayout;

struct EntityId {
    int32_t index = -1;
    uint32_t gen = 0;

    bool IsValid() const { return index >= 0 && gen != 0; }
};

inline bool operator==(EntityId a, EntityId b) {
    return a.index == b.index && a.gen == b.gen;
}
inline bool operator!=(EntityId a, EntityId b) {
    return !(a == b);
}

using RenderFn = El* (*)(void* self, Ctx* cx);
using DropFn = void (*)(void* self);

struct EntitySlot {
    void* ptr = nullptr;
    uint32_t gen = 0;
    RenderFn render = nullptr;
    DropFn drop = nullptr;
};

struct MotionSlotRec {
    uint32_t key = 0;

    uint64_t frame = 0;
    void* ptr = nullptr;
};

struct KeyedSlot {
    uint32_t key = 0;
    void* ptr = nullptr;
    DropFn drop = nullptr;

    EntityId entity = {};

    uint64_t frame = 0;
    bool frameScoped = false;
};

enum class MouseButton : uint8_t {
    Left,
    Right,
    Middle,
    NavigateBack,
    NavigateForward
};

struct Modifiers {
    bool control = false;
    bool alt = false;
    bool shift = false;
    bool platform = false;
    bool function = false;

    bool Modified() const {
        return control || alt || shift || platform || function;
    }

    bool Secondary() const {
        return PlatSecondaryIsCommand() ? platform : control;
    }
    int Count() const {
        return (int)control + (int)alt + (int)shift + (int)platform +
               (int)function;
    }
};

enum class TouchPhase : uint8_t {
    Started,
    Moved,
    Ended,
    Cancelled
};

enum class TouchHostKind : uint8_t {
    None,
    Pending,
    LongPress,
    Scroll,
    BarDrag,
    HandleDrag,

    SliderDrag
};

struct OngoingScroll {
    Axis axis = Axis::Horizontal;
    bool active = false;

    void Filter(Point* delta, TouchPhase phase);
};

enum class DispatchPhase : uint8_t {
    Capture,
    Bubble
};

struct MouseDownEvent {
    MouseButton button = MouseButton::Left;
    float x = 0;
    float y = 0;

    Bounds el = {};
    Modifiers modifiers = {};

    int clickCount = 1;

    bool firstMouse = false;

    DispatchPhase phase = DispatchPhase::Bubble;

    bool IsFocusing() const { return button == MouseButton::Left; }
};

struct MouseUpEvent {
    MouseButton button = MouseButton::Left;
    float x = 0;
    float y = 0;

    Bounds el = {};
    Modifiers modifiers = {};
    int clickCount = 1;
    DispatchPhase phase = DispatchPhase::Bubble;

    bool IsFocusing() const { return button == MouseButton::Left; }
};

struct MouseMoveEvent {
    float x = 0;
    float y = 0;

    Bounds el = {};

    bool pressed = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers modifiers = {};

    bool Dragging() const {
        return pressed && pressedButton == MouseButton::Left;
    }
};

struct DragPayload {
    Str kind = {};
    int ix = 0;
    void* data = nullptr;

    bool IsValid() const { return kind.s != nullptr; }
};

struct DragMoveEvent {
    DragPayload drag = {};
    MouseMoveEvent event = {};

    Bounds el = {};
};

struct DropEvent {
    DragPayload drag = {};

    float x = 0;
    float y = 0;

    Bounds el = {};
};

struct MouseExitEvent {
    float x = 0;
    float y = 0;
    bool pressed = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers modifiers = {};
};

struct ScrollWheelEvent {
    float x = 0;
    float y = 0;

    float deltaX = 0;
    float deltaY = 0;

    bool precise = false;
    Modifiers modifiers = {};
    TouchPhase phase = TouchPhase::Moved;

    bool propagate = true;
};

struct TouchDragEvent {
    TouchPhase phase = TouchPhase::Moved;
    Point startPosition = {};
    Point position = {};
};

struct LongPressEvent {
    TouchPhase phase = TouchPhase::Moved;
    Point startPosition = {};
    Point position = {};
};

enum class PlatformInputKind : uint8_t {
    MouseDown,
    MouseUp,
    MouseMove,
    MouseExited,
    ScrollWheel,
    TouchDrag,
    LongPress
};

struct PlatformInput {
    PlatformInputKind kind = PlatformInputKind::MouseMove;
    union {
        MouseDownEvent mouseDown = {};
        MouseUpEvent mouseUp;
        MouseMoveEvent mouseMove;
        MouseExitEvent mouseExited;
        ScrollWheelEvent scrollWheel;
        TouchDragEvent touchDrag;
        LongPressEvent longPress;
    };
};

struct ClickEvent {
    float x = 0;
    float y = 0;
    MouseButton button = MouseButton::Left;

    int id = 0;

    Bounds el = {};
    int clickCount = 1;
    Modifiers modifiers = {};

    bool keyboard = false;

    int keyboardKey = 0;
};

enum : int {
    KeyBack = 8,
    KeyTab = 9,
    KeyReturn = 13,
    KeyShift = 16,
    KeyControl = 17,
    KeyAlt = 18,

    KeyMenu = KeyAlt,
    KeyEscape = 27,
    KeySpace = 32,
    KeyPageUp = 33,
    KeyPageDown = 34,
    KeyEnd = 35,
    KeyHome = 36,
    KeyLeft = 37,
    KeyUp = 38,
    KeyRight = 39,
    KeyDown = 40,
    KeyInsert = 45,
    KeyDelete = 46,

    KeyA = 65,
    KeyC = 67,
    KeyE = 69,
    KeyF = 70,
    KeyH = 72,
    KeyV = 86,
    KeyX = 88,
    KeyY = 89,
    KeyZ = 90,
    KeyApps = 93,
    KeyNumpad0 = 96,
    KeyNumpad9 = 105,
    KeyF1 = 112,
    KeyF24 = 135,
    KeyBrowserBack = 166,
    KeyBrowserForward = 167,

    KeySemicolon = 186,
    KeyEqual = 187,
    KeyComma = 188,
    KeyMinus = 189,
    KeyPeriod = 190,
    KeySlash = 191,
    KeyBacktick = 192,
    KeyLeftBracket = 219,
    KeyBackslash = 220,
    KeyRightBracket = 221,
    KeyQuote = 222,

    KeyF25 = 256,
    KeyF35 = 266,
    KeyCut = 267,
    KeyCopy = 268,
    KeyPaste = 269,
    KeyNew = 270,
    KeyOpen = 271,
    KeySave = 272,
    KeyKpAdd = 273,
    KeyKpSubtract = 274,
    KeyKpMultiply = 275,
    KeyKpDivide = 276,
    KeyKpDecimal = 277,
    KeyKpSeparator = 278,
    KeyKpEqual = 279,
    KeyKpBegin = 280
};

struct KeyEvent {
    int vk = 0;
    uint32_t ch = 0;
    bool down = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;

    bool platform = false;

    bool function = false;

    bool held = false;

    bool preferCharacterInput = false;

    bool imeInProgress = false;

    bool propagate = true;
};

enum class CursorKind : uint8_t {
    Arrow,
    IBeam,

    Pointer,

    ColResize,

    RowResize,

    Crosshair,
    ClosedHand,
    OpenHand,
    ResizeLeft,
    ResizeRight,
    ResizeLeftRight,
    ResizeUp,
    ResizeDown,
    ResizeUpDown,
    ResizeUpLeftDownRight,
    ResizeUpRightDownLeft,
    IBeamVertical,
    NotAllowed,
    DragLink,
    DragCopy,
    ContextMenu,
    Count
};

struct TickEvent {
    int ms = 0;
};

struct HoverEvent {
    bool hovered = false;
};

struct LineClampEvent {
    bool clamped = false;
};

using ListenerFn = void (*)(void* self, Ctx* cx, const void* ev);
using ListenerArgFn = void (*)(void* self, Ctx* cx, const void* ev,
                               int64_t arg);

struct Listener {

    static constexpr int kPointerBits = sizeof(uintptr_t) * 8;
    static constexpr uintptr_t kHasArg = uintptr_t(1) << (kPointerBits - 1);
    static constexpr uintptr_t kArgBound = uintptr_t(1) << (kPointerBits - 2);
    static constexpr uintptr_t kFlags = kHasArg | kArgBound;

    uintptr_t fn = 0;
    EntityId view = {};

    int64_t arg = 0;

    template <typename F>
    void SetFn(F value) {
        fn = (uintptr_t)value | (fn & kFlags);
    }

    uintptr_t Fn() const { return fn & ~kFlags; }

    bool HasArg() const { return (fn & kHasArg) != 0; }
    bool ArgBound() const { return (fn & kArgBound) != 0; }
    void SetHasArg() { fn |= kHasArg; }
    void SetArgBound() { fn |= kHasArg | kArgBound; }
    bool IsValid() const { return Fn() != 0; }
};

static_assert(sizeof(Listener) <= 24,
              "keep Listener flags packed into the function word");

struct TimerSub {
    int id = 0;
    int ms = 0;
    double dueAt = 0;
    bool repeat = false;
    Listener l;
};

enum class ElKind : uint8_t {
    Div,
    Text,
    Chart,
    Progress,
    Icon,

    Image
};

enum class ObjectFit : uint8_t {
    Fill,
    Contain,
    Cover,
    ScaleDown,
    None,
};

enum class ImageLoadState : uint8_t {
    Loading,
    Ready,
    Failed,
};

struct PaintApp;
struct RenderImage;
struct ImageStore;

enum class ImageSourceKind : uint8_t {
    Resource,
    Render,
    Image,
    Custom,
};

using ImageSourceLoader = ImageLoadState (*)(PaintApp* paint, void* user,
                                             RenderImage** image);

struct ImageSource {
    Str resource = {};
    const uint8_t* bytes = nullptr;
    RenderImage* render = nullptr;
    ImageSourceLoader loader = nullptr;
    void* user = nullptr;
    int bytesLen = 0;
    ImageSourceKind kind = ImageSourceKind::Resource;

    static ImageSource FromResource(Str resource);
    static ImageSource FromRender(RenderImage* image);
    static ImageSource FromImage(const uint8_t* bytes, int len);
    static ImageSource FromCustom(ImageSourceLoader loader, void* user);
};

Bounds ObjectFitBounds(ObjectFit fit, Bounds bounds, Size imageSize);

enum class Display : uint8_t {
    Block,
    Flex,
    Grid,
    None
};

enum class FlexDir : uint8_t {
    Row,
    Col,

    RowReverse,
    ColReverse
};

enum class FlexAlign : uint8_t {
    Start,
    Center,
    End,
    Stretch,
    Baseline,

    Normal
};
enum class Justify : uint8_t {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,

    Normal
};

enum class Overflow : uint8_t {
    Visible,
    Hidden,
    Scroll
};

enum class ScrollbarMode : uint8_t {
    Always,
    Hover,
    Scrolling
};

struct RuntimeStyle {
    Rgba background{Rgb(0xfa, 0xfa, 0xfa)};
    Rgba foreground{Rgb(0x17, 0x17, 0x17)};
    Rgba mutedForeground{Rgb(0x73, 0x73, 0x73)};
    Rgba border{Rgb(0xe5, 0xe5, 0xe5)};

    Rgba chartGrid{Rgba8(0xe5, 0xe5, 0xe5, 0x99)};
    Rgba ring{Rgb(0x17, 0x17, 0x17)};
    Rgba inspectorAccent{Rgb(0x3b, 0x82, 0xf6)};
    Rgba popover{Rgb(0xfa, 0xfa, 0xfa)};
    Rgba popoverForeground{Rgb(0x17, 0x17, 0x17)};
    Background progress{Rgb(0x17, 0x17, 0x17)};
    Background scrollbarThumb{Rgba8(0x17, 0x17, 0x17, 0x33)};
    Background scrollbarThumbHover{Rgba8(0x17, 0x17, 0x17, 0x66)};
    Background scrollbarTrack{Rgba8(0, 0, 0, 0)};

    Background legacyPrimary{Rgb(0x17, 0x17, 0x17)};
    Rgba legacyPrimaryForeground{Rgb(0xfa, 0xfa, 0xfa)};
    Rgba legacyPrimaryHover{Rgb(0x35, 0x35, 0x35)};
    Background legacyMuted{Rgb(0xf5, 0xf5, 0xf5)};
    Background legacySecondary{Rgb(0xf5, 0xf5, 0xf5)};
    Rgba legacySecondaryForeground{Rgb(0x17, 0x17, 0x17)};
    Rgba legacySecondaryHover{Rgb(0xe5, 0xe5, 0xe5)};
    Rgba legacySecondaryActive{Rgb(0xd4, 0xd4, 0xd4)};

    float radius = 6.f;
    float fontSize = 16.f;
    ScrollbarMode scrollbarMode = ScrollbarMode::Scrolling;
    bool focusRing = true;
};

const RuntimeStyle& RuntimeStyleNow(const App* app);
void RuntimeStyleInstall(App* app, const RuntimeStyle& style);

const float kRadiusFull = 9999.f;

const int kPaintLayerTree = 0;

const int kPaintLayerPopup = 1;

const int kPaintLayerTooltip = 2;

const int kPaintLayerInspector = 3;

enum class ScrollbarEntrance : uint8_t {

    Fade,

    SlideAndFade
};

struct ScrollbarMotion {

    float idle = 2;

    float enter = 0;

    float exit = 0;

    float expand = 0;
    ScrollbarEntrance entrance = ScrollbarEntrance::Fade;
    ScrollbarEntrance thumbHoverEntrance = ScrollbarEntrance::Fade;

    ScrollbarMotion WithIdle(float value) const {
        ScrollbarMotion out = *this;
        out.idle = value;
        return out;
    }
    ScrollbarMotion WithEnter(float value) const {
        ScrollbarMotion out = *this;
        out.enter = value;
        return out;
    }
    ScrollbarMotion WithExit(float value) const {
        ScrollbarMotion out = *this;
        out.exit = value;
        return out;
    }
    ScrollbarMotion WithExpand(float value) const {
        ScrollbarMotion out = *this;
        out.expand = value;
        return out;
    }
    ScrollbarMotion WithEntrance(ScrollbarEntrance value) const {
        ScrollbarMotion out = *this;
        out.entrance = value;
        return out;
    }
    ScrollbarMotion WithThumbHoverEntrance(ScrollbarEntrance value) const {
        ScrollbarMotion out = *this;
        out.thumbHoverEntrance = value;
        return out;
    }
};

ScrollbarMotion ScrollbarMotionFor(ScrollbarMode mode);

struct ScrollbarVisibility {
    float opacity = 0;
    float position = 0;
    bool running = false;
};

void ScrollbarVisibilitySet(int scrollId, bool visible,
                            ScrollbarEntrance entrance, float enter, float exit,
                            double now);
ScrollbarVisibility ScrollbarVisibilityAt(int scrollId, double now);

float ScrollbarSlideOffset(float trackWidth, float position);

void ScrollFadeClear();

enum class IconName : uint8_t {
    None = 0,
    ALargeSmall,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    Asterisk,
    Ban,
    Battery,
    BatteryCharging,
    BatteryFull,
    BatteryLow,
    BatteryMedium,
    BatteryWarning,
    Bell,
    BookOpen,
    Bot,
    Building2,
    Calendar,
    CaseSensitive,
    ChartPie,
    Check,
    ChevronDown,
    ChevronLeft,
    ChevronRight,
    ChevronUp,
    ChevronsUpDown,
    CircleAlert,
    CircleCheck,
    CircleUser,
    CircleX,
    Close,
    Copy,
    Cpu,
    Dash,
    Delete,
    Ellipsis,
    EllipsisVertical,
    ExternalLink,
    EyeOff,
    Eye,
    File,
    FileText,
    Folder,
    FolderClosed,
    FolderOpen,
    Frame,
    GalleryVerticalEnd,
    Github,
    Globe,
    HardDrive,
    Heart,
    HeartOff,
    Inbox,
    Info,
    Inspector,
    LayoutDashboard,
    Loader,
    LoaderCircle,
    Map,
    Maximize,
    MemoryStick,
    Menu,
    Mic,
    Minimize,
    Minus,
    Moon,
    Network,
    Palette,
    PanelBottom,
    PanelBottomOpen,
    PanelLeft,
    PanelLeftClose,
    PanelLeftOpen,
    PanelRight,
    PanelRightClose,
    PanelRightOpen,
    Pause,
    Play,
    Plus,
    Redo,
    Redo2,
    RefreshCw,
    Replace,
    ResizeCorner,
    RotateCw,
    Search,
    Settings,
    Settings2,
    SortAscending,
    SortDescending,
    Square,
    SquareTerminal,
    Star,
    StarFill,
    StarOff,
    Sun,
    ThumbsDown,
    ThumbsUp,
    TriangleAlert,
    Undo,
    Undo2,
    User,
    WindowClose,
    WindowMaximize,
    WindowMinimize,
    WindowRestore,

    X,
    AtSign,
    Image,
    Sparkles,
};

struct PaintCtx;

struct TextSpan {
    int lo = 0;
    int hi = 0;
    Rgba color = {};

    Rgba bg = {0, 0, 0, 0};

    bool underline = false;

    bool wavy = false;
};

struct TextFade {
    int lo = 0;
    int hi = 0;
    float fadeOut = 0;
};

Rgba TextFadeColor(Rgba c, float fadeOut);

int TextFadeSpans(Arena* a, int textLen, Rgba base, const TextSpan* spans,
                  int nSpans, const TextFade* fades, int nFades,
                  const TextSpan** out);

struct WhitespaceMark {
    int off = 0;
    float x = 0;
    float y = 0;
    float h = 0;
    bool tab = false;
};

typedef int (*WhitespaceRectsFn)(void* ud, int lo, int hi, Bounds* out,
                                 int max);
typedef void (*WhitespaceMarkFn)(void* ud, const WhitespaceMark* m);

int WhitespaceMarksVisit(Str s, float spaceMarkW, WhitespaceRectsFn rects,
                         WhitespaceMarkFn emit, void* ud);

int IndentGuideXs(float indentWidth, int indentCount, int tabSize, float* out,
                  int max);

enum class ChartKind : uint8_t {
    Area,
    Line,
    Bar,
    Candlestick,
    Radar
};

enum class ChartStroke : uint8_t {
    Natural,
    Linear,
    StepAfter
};

enum class BarAlign : uint8_t {
    Bottom,
    Top,
    Left,
    Right
};

struct ChartSeriesExtra {
    const float* ys = nullptr;
    Rgba stroke = {};
    Rgba fillTop = {};
    Rgba fillBot = {};

    Str name = {};
};

enum class AxisLabelPlacement : uint8_t {
    Outside,
    Inside
};

using ChartTickFormatFn = Str (*)(Arena* a, double value, void* user);

using ChartTooltipTitleFn = Str (*)(Arena* a, const void* d, void* user);
using ChartTooltipValueFn = Str (*)(Arena* a, const void* d, int row,
                                    double value, void* user);
using ChartTooltipValueColorFn = Rgba (*)(const void* d, int row, double value,
                                          void* user);

using ChartTooltipContentFn = El* (*)(Ctx * cx, const void* d, void* user);

struct ChartTooltipContent {
    ChartTooltipTitleFn title = nullptr;
    void* titleUser = nullptr;
    ChartTooltipValueFn value = nullptr;
    void* valueUser = nullptr;
    ChartTooltipValueColorFn valueColor = nullptr;
    void* valueColorUser = nullptr;
    ChartTooltipContentFn content = nullptr;
    void* contentUser = nullptr;

    const void* data = nullptr;
    int dataStride = 0;

    const void* Datum(int index, const float* own) const;

    bool TitleText(Arena* a, const void* d, Str fallback, bool hasFallback,
                   Str* out) const;

    Str ValueText(Arena* a, const void* d, int row, double value,
                  bool f64 = false) const;

    bool ValueColor(const void* d, int row, double value, Rgba* out) const;
};

Str ChartFormatValue(Arena* a, double value, bool f64 = false);

struct ChartSeries;

Rgba ChartBarTooltipColor(const ChartSeries& c, int index);

struct ChartSeries {
    ChartKind kind = ChartKind::Area;
    const float* ys = nullptr;

    const double* exact = nullptr;
    int n = 0;

    const ChartSeriesExtra* more = nullptr;
    int nMore = 0;
    int tickMargin = 15;

    const char* const* labels = nullptr;

    bool overlay = false;
    Rgba stroke = {};
    Rgba fillTop = {};
    Rgba fillBot = {};

    float domainMin = 0;
    float domainMax = 0;

    bool pinnedDomain = false;

    int pointCount = 0;

    bool yAxis = false;

    AxisLabelPlacement axisLabelPlacement = AxisLabelPlacement::Outside;

    int yTickCount = 5;

    ChartTickFormatFn tickFormat = nullptr;
    void* tickFormatUser = nullptr;

    int xTickCount = -1;

    int gridColumns = 0;

    bool gridDashed = true;

    float yPaddingTop = 10;
    float yPaddingBottom = 0;

    const double* referenceLines = nullptr;
    int nReferenceLines = 0;

    int bandCount = 0;

    const float* opens = nullptr;
    const float* highs = nullptr;
    const float* lows = nullptr;
    Rgba up = {};
    Rgba down = {};

    float bandPadding = 0.2f;
    float bandPaddingOuter = 0.1f;

    float maxBandWidth = 30.f;
    float barRadius = 4;
    BarAlign barAlign = BarAlign::Bottom;

    const float* bases = nullptr;

    bool barLabels = false;

    const Rgba* barLabelColors = nullptr;

    float barMinLength = 0;

    bool valueAxis = false;

    int valueTickCount = 5;

    const Rgba* barFills = nullptr;

    bool barGradient = false;
    bool barGradientPerBar = false;

    bool barGradientDiagonal = false;
    Rgba barFillFrom = {};
    Rgba barFillTo = {};

    ChartStroke strokeStyle = ChartStroke::Natural;
    bool dot = false;

    float bodyWidthRatio = 0.8f;

    float radarRadius = 0;
    int gridLevels = 4;

    bool grid = true;

    bool xAxis = true;

    bool tooltip = false;

    uint32_t id = 0;

    bool appear = false;
    uint64_t appearGeneration = 0;

    Str name = {};

    ChartTooltipContent tooltipContent = {};
};

struct Corners {
    float tl = 0;
    float tr = 0;
    float br = 0;
    float bl = 0;

    bool IsUniform() const { return tl == tr && tr == br && br == bl; }
};

enum class FontWeight : uint16_t {
    Thin = 100,
    ExtraLight = 200,
    Light = 300,
    Normal = 400,
    Medium = 500,
    Semibold = 600,
    Bold = 700,
    ExtraBold = 800,
    Black = 900
};

enum class TextAlign : uint8_t {
    Left,
    Center,
    Right,
};

enum class FontFeatures : uint8_t {
    Default,
    TabularFigures,
};

enum class Anchor : uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    LeftCenter,
    RightCenter
};

struct AnchoredPosition {
    Bounds bounds = {};
    int8_t placement = -1;
};

AnchoredPosition AnchoredSideResolve(Bounds trigger, Size popup, Size view,
                                     float margin, int preferred, int align,
                                     float offset);
AnchoredPosition AnchoredSideResolve(Bounds trigger, Size popup, Size view,
                                     Edges margin, int preferred, int align,
                                     float offset);
AnchoredPosition AnchoredCornerResolve(Anchor anchor, Point at, Size popup,
                                       Size view, float margin);
AnchoredPosition AnchoredCornerResolve(Anchor anchor, Point at, Size popup,
                                       Size view, Edges margin);

struct AnchoredPlacedHook {
    void (*fn)(void* user, AnchoredPosition position, Bounds trigger) = nullptr;
    void* user = nullptr;
};

Point AnchorPosition(Anchor anchor, Bounds trigger, float offset);

enum class FocusLine : uint8_t {

    Edge,

    Inside,

    Outside,
};

struct Style {

    const BoxShadow* shadows = nullptr;
    Str tooltip;

    float width = kAuto;
    float height = kAuto;

    float widthFrac = 0;

    float minW = kAuto;
    float minH = kAuto;
    float maxW = 1e9f;
    float maxH = 1e9f;

    float maxWFrac = 0;

    float aspect = 0;
    float flexGrow = 0;
    float flexShrink = 1;

    float flexBasis = kAuto;

    float flexBasisFrac = 0;
    Edges pad = {};
    Edges margin = {};

    float gapX = 0;
    float gapY = 0;
    float border = 0;
    float borderT = 0;
    float borderB = 0;
    float borderL = 0;
    float borderR = 0;
    float radius = 0;

    Corners corners = {};
    Background bg = {};
    Rgba borderColor = {};
    Rgba color = {};
    int shadowCount = 0;

    float rotate = 0;

    float opacity = 1;
    float fontSize = 0;

    float lineHeight = 0;

    float dashOn = 2;
    float dashOff = 1;

    float anchorGap = 0;
    float anchorMargin = 4;

    Bounds positionerTrigger = {};
    Point positionerPoint = {};
    float absTop = kAuto, absLeft = kAuto, absBottom = kAuto, absRight = kAuto;

    float absLeftRel = 0, absRightRel = 0;
    float absTopRel = 0, absBottomRel = 0;
    Background hoverBg = {};

    Rgba hoverFg = {};

    Background activeBg = {};

    Background groupHoverBg = {};
    int focusId = 0;

    int tabIndex = 0;

    uint32_t keyContext = 0;
    int trapId = 0;

    uint32_t hasAlignSelf : 1 = false;
    uint32_t hasCorners : 1 = false;
    uint32_t truncate : 1 = false;
    uint32_t wrap : 1 = false;

    uint32_t flexWrap : 1 = false;
    uint32_t hasBg : 1 = false;
    uint32_t hasColor : 1 = false;
    uint32_t fontBold : 1 = false;
    uint32_t fontSemibold : 1 = false;
    uint32_t fontMedium : 1 = false;
    uint32_t fontMono : 1 = false;
    uint32_t underline : 1 = false;
    uint32_t strike : 1 = false;
    uint32_t italic : 1 = false;
    uint32_t borderDashed : 1 = false;
    uint32_t absolute : 1 = false;

    uint32_t fixed : 1 = false;
    uint32_t deferred : 1 = false;

    uint32_t anchorFlip : 1 = false;
    uint32_t anchorBelow : 1 = false;
    uint32_t anchorAbove : 1 = false;
    uint32_t anchorCenterX : 1 = false;
    uint32_t anchorCorner : 1 = false;
    uint32_t explicitPositioner : 1 = false;
    uint32_t positionerCorner : 1 = false;
    uint32_t hasHoverBg : 1 = false;
    uint32_t hasHoverFg : 1 = false;
    uint32_t hasActiveBg : 1 = false;

    uint32_t group : 1 = false;

    uint32_t groupHoverVisible : 1 = false;
    uint32_t hasGroupHoverBg : 1 = false;

    uint32_t focusFromPath : 1 = false;

    uint16_t fontWeight = 0;
    Display display = Display::Block;
    FlexDir dir = FlexDir::Row;
    FlexAlign align = FlexAlign::Normal;

    FlexAlign alignSelf = FlexAlign::Stretch;
    Justify justify = Justify::Normal;
    Overflow overflowY = Overflow::Visible;
    Overflow overflowX = Overflow::Visible;

    uint8_t marginAuto = 0;

    int16_t zIndex = 0;

    Rgba focusLineColor = {};

    uint8_t deferredLayer = 0;

    Anchor anchor = Anchor::TopLeft;

    int8_t positionerPlacement = -1;
    uint8_t positionerAlign = 1;
    bool tabStop = true;

    bool focusOnPress = false;

    bool focusRing : 1 = false;

    uint8_t focusLine : 2 = (uint8_t)FocusLine::Edge;

    uint8_t textAlign : 2 = 0;

    uint8_t fontFeatures : 2 = 0;

    int8_t tooltipPlacement = -1;

    uint8_t alignContent = 0;

    uint8_t flexWrapReverse : 1 = false;

    uint8_t invisible : 1 = false;

    uint8_t whiteSpaceSet : 1 = false;

    uint8_t flexBasisPercent : 1 = false;

    uint8_t gridColStartLine : 1 = false;
    uint8_t gridColEndLine : 1 = false;
    uint8_t gridRowStartLine : 1 = false;
    uint8_t gridRowEndLine : 1 = false;

    uint16_t relLengths = 0;

    float heightFrac = 0;

    Rgba textBg = {0, 0, 0, 0};

    uint8_t underlineThickness = 1;
    uint8_t hasTextBg : 1 = false;

    uint8_t underlineSet : 1 = false;
    uint8_t underlineWavy : 1 = false;

    uint8_t textOverflow : 2 = 0;

    uint8_t hasFocusLineColor : 1 = false;

    uint8_t fontFamily = 0;
};

enum : uint8_t {
    kMarginAutoL = 1,
    kMarginAutoR = 2,
    kMarginAutoT = 4,
    kMarginAutoB = 8,
};

enum : uint16_t {
    kRelMinW = 1 << 0,
    kRelMinH = 1 << 1,
    kRelMaxH = 1 << 2,
    kRelPadL = 1 << 3,
    kRelPadR = 1 << 4,
    kRelPadT = 1 << 5,
    kRelPadB = 1 << 6,
    kRelMarginL = 1 << 7,
    kRelMarginR = 1 << 8,
    kRelMarginT = 1 << 9,
    kRelMarginB = 1 << 10,
    kRelGapX = 1 << 11,
    kRelGapY = 1 << 12,
    kRelPad = kRelPadL | kRelPadR | kRelPadT | kRelPadB,
    kRelMargin = kRelMarginL | kRelMarginR | kRelMarginT | kRelMarginB,
    kRelGap = kRelGapX | kRelGapY,
};

static_assert(sizeof(Style) <= 432, "keep Style members packed by alignment");

struct ActionSlot {
    uint32_t action = 0;
    Listener fn = {};
    ActionSlot* next = nullptr;
};

struct ActionEvent {
    uint32_t action = 0;

    int64_t arg = 0;

    bool propagate = false;
};

enum class SelectionFormat : uint8_t {
    Plain = 0,
    Source
};

struct SelBlock {

    Str pre;
    Str post;

    Str linePre;

    bool join = false;
};

struct SelSource {

    Str pre;
    Str post;

    const SelBlock* block = nullptr;
};

struct SourceSegment {
    int renderedStart = 0;
    int renderedEnd = 0;
    int sourceStart = 0;
    int sourceEnd = 0;

    bool linear = false;
};

struct SelSourceMap {
    const SourceSegment* segments = nullptr;
    int count = 0;
    int offset = 0;
    bool atomic = false;
};

struct FocusHandle {
    int id = 0;
    bool IsValid() const { return id != 0; }
    bool operator==(const FocusHandle& o) const { return id == o.id; }
    bool operator!=(const FocusHandle& o) const { return id != o.id; }
};

enum class AccessibilityRole : uint8_t {
    None,
    Unknown,
    TextRun,
    Cell,
    Label,
    Image,
    Link,
    Row,
    ListItem,
    ListMarker,
    TreeItem,
    ListBoxOption,
    MenuItem,
    MenuListOption,
    Paragraph,
    CheckBox,
    RadioButton,
    TextInput,
    Button,
    DefaultButton,
    Pane,
    RowHeader,
    ColumnHeader,
    RowGroup,
    List,
    Table,
    LayoutTableCell,
    LayoutTableRow,
    LayoutTable,
    Switch,
    Menu,
    MultilineTextInput,
    SearchInput,
    DateInput,
    DateTimeInput,
    WeekInput,
    MonthInput,
    TimeInput,
    EmailInput,
    NumberInput,
    PasswordInput,
    PhoneNumberInput,
    UrlInput,
    Abbr,
    Alert,
    AlertDialog,
    Application,
    Article,
    Audio,
    Banner,
    Blockquote,
    Canvas,
    Caption,
    Caret,
    Code,
    ColorWell,
    ComboBox,
    EditableComboBox,
    Complementary,
    Comment,
    ContentDeletion,
    ContentInsertion,
    ContentInfo,
    Definition,
    DescriptionList,
    Details,
    Dialog,
    DisclosureTriangle,
    Document,
    EmbeddedObject,
    Emphasis,
    Feed,
    FigureCaption,
    Figure,
    Footer,
    Form,
    Grid,
    GridCell,
    Group,
    Header,
    Heading,
    Iframe,
    IframePresentational,
    ImeCandidate,
    Keyboard,
    Legend,
    LineBreak,
    ListBox,
    Log,
    Main,
    Mark,
    Marquee,
    Math,
    MenuBar,
    MenuItemCheckBox,
    MenuItemRadio,
    MenuListPopup,
    Meter,
    Navigation,
    Note,
    PluginObject,
    ProgressIndicator,
    RadioGroup,
    Region,
    RootWebArea,
    Ruby,
    RubyAnnotation,
    ScrollBar,
    ScrollView,
    Search,
    Section,
    SectionFooter,
    SectionHeader,
    Slider,
    SpinButton,
    Splitter,
    Status,
    Strong,
    Suggestion,
    SvgRoot,
    Tab,
    TabList,
    TabPanel,
    Term,
    Time,
    Timer,
    TitleBar,
    Toolbar,
    Tooltip,
    Tree,
    TreeGrid,
    Video,
    WebView,
    Window,
    PdfActionableHighlight,
    PdfRoot,
    GraphicsDocument,
    GraphicsObject,
    GraphicsSymbol,
    DocAbstract,
    DocAcknowledgements,
    DocAfterword,
    DocAppendix,
    DocBackLink,
    DocBiblioEntry,
    DocBibliography,
    DocBiblioRef,
    DocChapter,
    DocColophon,
    DocConclusion,
    DocCover,
    DocCredit,
    DocCredits,
    DocDedication,
    DocEndnote,
    DocEndnotes,
    DocEpigraph,
    DocEpilogue,
    DocErrata,
    DocExample,
    DocFootnote,
    DocForeword,
    DocGlossary,
    DocGlossRef,
    DocIndex,
    DocIntroduction,
    DocNoteRef,
    DocNotice,
    DocPageBreak,
    DocPageFooter,
    DocPageHeader,
    DocPageList,
    DocPart,
    DocPreface,
    DocPrologue,
    DocPullquote,
    DocQna,
    DocSubtitle,
    DocTip,
    DocToc,
    ListGrid,
    Terminal
};

enum class AccessibilityToggled : uint8_t {
    Unset,
    False,
    True,
    Mixed
};

enum class AccessibilityOrientation : uint8_t {
    Unset,
    Horizontal,
    Vertical
};

enum AccessibilityActionBits : uint8_t {
    AccessibilityActionNone = 0,
    AccessibilityActionDefault = 1 << 0,
    AccessibilityActionFocus = 1 << 1,
    AccessibilityActionIncrement = 1 << 2,
    AccessibilityActionDecrement = 1 << 3,
    AccessibilityActionSetValue = 1 << 4
};

enum class AccessibilityAction : uint8_t {
    Default,
    Focus,
    Increment,
    Decrement,
    SetValue
};

struct AccessibilityInfo {
    AccessibilityRole role = AccessibilityRole::None;

    AccessibilityToggled toggled = AccessibilityToggled::Unset;
    AccessibilityOrientation orientation = AccessibilityOrientation::Unset;

    Str authorId = {};
    Str label = {};

    Str description = {};
    Str value = {};
    Str placeholder = {};
    float numericValue = 0;
    float minNumericValue = 0;
    float maxNumericValue = 0;
    float numericValueStep = 0;
    int positionInSet = 0;
    int sizeOfSet = 0;
    int rowCount = 0;
    int columnCount = 0;
    int rowIndex = 0;
    int columnIndex = 0;
    int level = 0;

    unsigned int hasNumericValue : 1 = false;
    unsigned int hasMinNumericValue : 1 = false;
    unsigned int hasMaxNumericValue : 1 = false;
    unsigned int hasNumericValueStep : 1 = false;
    unsigned int hasPositionInSet : 1 = false;
    unsigned int hasSizeOfSet : 1 = false;
    unsigned int hasRowCount : 1 = false;
    unsigned int hasColumnCount : 1 = false;
    unsigned int hasRowIndex : 1 = false;
    unsigned int hasColumnIndex : 1 = false;
    unsigned int hasLevel : 1 = false;
    unsigned int selected : 1 = false;
    unsigned int hasSelected : 1 = false;
    unsigned int expanded : 1 = false;
    unsigned int hasExpanded : 1 = false;
    unsigned int activeDescendant : 1 = false;
    unsigned int disabled : 1 = false;
};

static_assert(sizeof(AccessibilityInfo) <= 136,
              "keep AccessibilityInfo boolean state packed");

struct ElStyleStates {
    Style refine = {};
    Style hover = {};
    Style active = {};
    Style focus = {};
    Style dragOver = {};
    uint32_t refineSet = 0;
    uint32_t hoverSet = 0;
    uint32_t activeSet = 0;
    uint32_t focusSet = 0;
    uint32_t dragOverSet = 0;
    ArenaStr dragOverKind = kArenaStrNone;
};

struct Selection;
struct El;

struct ElRefiner {
    void (*apply)(El* target, void* user) = nullptr;
    void* user = nullptr;

    bool IsSet() const { return apply != nullptr; }
    void Apply(El* target) const {
        if (apply && target) apply(target, user);
    }
};

struct El {

    Arena* arena = nullptr;
#ifndef NDEBUG

    const char* sourceFile = nullptr;
    int sourceLine = 0;
    int sourceColumn = 0;
#endif
    Style style;
    Str id;
    Str text;
    Str iconPath;

    Str iconSvg;
    AccessibilityInfo accessibility = {};

    ImageSource imageSource;

    EntityId imageCache = {};

    uint32_t plotAppearScope = 0;

    uint32_t selectionScope = 0;
    Func0 onClick;

    Listener listener;

    Listener accessibilityDefault;
    Listener accessibilityIncrement;
    Listener accessibilityDecrement;

    Listener onHover;

    Listener onMouseMove;
    Listener onScroll;

    Listener onScrollWheel;

    Listener onMouseDown;
    Listener onMouseUp;

    Listener onDragMove;

    Listener onMouseDownOut;

    Listener onMouseUpOut;

    Listener onDrop;
    Listener onLineClamp;

    Func0 accessibilityIncrementDirect;
    Func0 accessibilityDecrementDirect;
    int64_t clickActionArg = 0;
    ActionSlot* actions = nullptr;

    DragPayload drag = {};

    Str dropKind = {};

    gpui::Bounds* boundsOut = nullptr;

    SliderState* slider = nullptr;

    InputState* input = nullptr;

    SliderState* sliderBounds = nullptr;
    void (*customPaint)(PaintCtx* ctx, El* e, void* user) = nullptr;

    void (*prePaint)(PaintCtx* ctx, El* e, void* user) = nullptr;
    void* customUser = nullptr;

    AnchoredPlacedHook* onPlaced = nullptr;
    El* first = nullptr;
    El* last = nullptr;
    El* next = nullptr;

    El* imageLoading = nullptr;
    El* imageFallback = nullptr;
    El* imageReplacement = nullptr;

    const TextSpan* spans = nullptr;

    const TextSpan* washes = nullptr;

    const TextSpan* underlines = nullptr;
    gpui::Bounds* rangeOut = nullptr;
    float* caretOutX = nullptr;
    float* caretOutY = nullptr;

    const Selection* extraSels = nullptr;
    const int* extraCarets = nullptr;

    int nExtraSels = 0;
    int nExtraCarets = 0;

    const SelSource* selSrc = nullptr;

    const SelSourceMap* selMap = nullptr;

    uint64_t layoutNode = 0;

    TextLayout* laidLayout = nullptr;

    ArenaPtr<ChartSeries> chart = {};
    float progress = 0;
    int clickId = 0;

    uint32_t pathId = 0;

    uint32_t clickAction = 0;

    int clickActionFocusId = 0;

    ArenaPtr<ElStyleStates> styleStates = {};

    ArenaPtr<TextFade> fades = {};
    float lineSpanHeight = 0;
    float lineClampCap = 0;

    float x = 0, y = 0, w = 0, h = 0;

    gpui::Bounds Bounds() const { return {x, y, w, h}; }

    float paintDx = 0;
    float paintDy = 0;
    float scrollY = 0;

    float scrollX = 0;

    ScrollbarMotion scrollMotion = {};
    Background scrollTrack = {};
    Background scrollTrackHover = {};
    Background scrollTrackActive = {};
    Background scrollThumb = {};
    Background scrollThumbHover = {};
    Background scrollThumbActive = {};
    Rgba scrollTrackBorder = {};
    Rgba scrollTrackHoverBorder = {};
    Rgba scrollTrackActiveBorder = {};
    float scrollTrackWidth = 16;
    float scrollThumbWidth = 6;
    float scrollThumbHoverWidth = 8;
    float scrollThumbActiveWidth = 8;
    float scrollThumbInset = 4;
    float scrollThumbHoverInset = 4;
    float scrollThumbActiveInset = 4;
    float scrollThumbRadius = 0;
    float scrollThumbHoverRadius = 0;
    float scrollThumbActiveRadius = 0;
    float scrollThumbMinLength = 48;
    float scrollThumbHoverMinLength = 48;
    float scrollThumbActiveMinLength = 48;
    int scrollId = 0;
    float contentW = 0;
    float contentH = 0;
    int selLo = -1;
    int selHi = -1;
    int nSpans = 0;
    int nWashes = 0;
    int nUnderlines = 0;

    int rangeOutLo = -1;
    int rangeOutHi = -1;
    Rgba selColor{Rgba8(0x6b, 0xb3, 0xf0, 90)};

    int markLo = -1;
    int markHi = -1;

    EntityId selectionOwner = {};

    int caretOff = -1;
    Rgba caretColor = {};

    Rgba whitespaceColor = {0, 0, 0, 0};

    Rgba indentGuideColor = {0, 0, 0, 0};
    int16_t indentGuideCount = 0;
    uint8_t indentGuideTab = 0;
    float caretW = 2;
    float laidFont = 0;
    float laidMaxW = 0;

    float measKeyW[4] = {};
    Size measSize[4] = {};

    unsigned int clickFromPath : 1 = false;

    unsigned int scrollFromPath : 1 = false;

    unsigned int stopClick : 1 = false;

    unsigned int stopMouseDown : 1 = false;

    unsigned int suppressTextSelection : 1 = false;

    unsigned int lineSpan : 1 = false;

    unsigned int lineClamp : 1 = false;

    unsigned int scrollModeSet : 1 = false;
    unsigned int scrollThemeSet : 1 = false;
    unsigned int noScrollbar : 1 = false;
    unsigned int noScrollbarX : 1 = false;
    unsigned int noScrollbarY : 1 = false;

    unsigned int selectable : 1 = false;
    unsigned int selJoin : 1 = false;
    unsigned int caretLineEndAffinity : 1 = false;

    unsigned int imageGrayscale : 1 = false;

    unsigned int debug : 1 = false;
    unsigned int debugBelow : 1 = false;

    unsigned int glyphWashes : 1 = false;

    unsigned int rootTooltip : 1 = false;

    IconName icon = IconName::None;
    ElKind kind = ElKind::Div;
    CursorKind cursor = CursorKind::Arrow;
    DispatchPhase mouseDownPhase = DispatchPhase::Bubble;
    DispatchPhase mouseUpPhase = DispatchPhase::Bubble;
    Axis sliderAxis = Axis::Horizontal;

    ObjectFit objectFit = ObjectFit::Contain;
    ImageLoadState imageLoadState = ImageLoadState::Loading;

    ScrollbarMode scrollMode = ScrollbarMode::Always;

    uint8_t scrollMaskAxes = 0;
    uint8_t measCount = 0;
    uint8_t measNext = 0;

    El* Flex();
    El* FlexRow();
    El* FlexCol();
    El* FlexRowReverse();
    El* FlexColReverse();
    El* FlexWrap();
    El* Grow(float g = 1);
    El* Shrink0();

    El* Flex1();

    El* Flex1Rel();

    El* FlexNone();
    El* Basis(float v);

    El* BasisFrac(float f);

    El* Shrink(float f);
    El* W(float v);
    El* WFrac(float f);

    El* Rotate(float turns);

    El* HideScrollbar();

    El* HideScrollbarX();
    El* HideScrollbarY();

    El* ScrollMask(Axis axis);

    El* Opacity(float f);
    El* Grayscale(bool grayscale = true);
    El* ObjectFitMode(gpui::ObjectFit fit);
    El* WithLoading(El* loading);
    El* WithFallback(El* fallback);
    El* WithImageCache(EntityId cache);

    El* WithPlotAppearScope(uint32_t key);
    El* H(float v);
    El* SizeFull();
    El* MinH(float v);
    El* MinW(float v);
    El* MaxW(float v);

    El* MaxWFrac(float f);

    El* Aspect(float ratio);
    El* MaxH(float v);
    El* Gap(float v);
    El* GapX(float v);
    El* GapY(float v);
    El* Pad(float v);
    El* PadX(float v);
    El* PadY(float v);
    El* PadL(float v);
    El* PadR(float v);
    El* PadT(float v);
    El* PadB(float v);
    El* Margin(float v);
    El* MarginX(float v);
    El* MarginY(float v);
    El* MarginL(float v);
    El* MarginR(float v);
    El* MarginT(float v);
    El* MarginB(float v);

    El* MlAuto();
    El* ItemsCenter();
    El* ItemsStart();
    El* ItemsEnd();
    El* ItemsStretch();

    El* SelfStart();
    El* SelfEnd();
    El* SelfCenter();
    El* JustifyBetween();
    El* JustifyAround();
    El* JustifyCenter();
    El* JustifyEnd();
    El* JustifyStart();
    El* Bg(Background c);
    El* Border(float width, Rgba c);
    El* BorderT(float width, Rgba c);
    El* BorderB(float width, Rgba c);
    El* BorderL(float width, Rgba c);
    El* BorderR(float width, Rgba c);
    El* Shadows(const BoxShadow* values, int count);
    El* Radius(float r);

    El* Corners(float tl, float tr, float br, float bl);
    El* Fg(Rgba c);
    El* Font(float px);
    El* LineHeight(float mult);

    El* TextAlignment(TextAlign align);

    El* FontFeatures(gpui::FontFeatures features);
    El* TextLeft() { return TextAlignment(TextAlign::Left); }
    El* TextCenter() { return TextAlignment(TextAlign::Center); }
    El* TextRight() { return TextAlignment(TextAlign::Right); }
    El* Truncate();
    El* ClipY();
    El* ScrollY(float off);
    El* ScrollX(float off);
    El* PaintOffset(float dx, float dy);
    El* ClipX();
    El* ScrollMode(ScrollbarMode m);
    El* ScrollId(int v);
    El* Click(int v);
    El* Role(AccessibilityRole role);
    El* AccessibilityId(Str authorId);
    El* AriaLabel(Str label);

    El* AriaDescription(Str description);
    El* AriaValue(Str value);
    El* AriaPlaceholder(Str placeholder);
    El* AriaDisabled(bool disabled = true);
    El* AriaToggled(AccessibilityToggled toggled);
    El* AriaSelected(bool selected);
    El* AriaExpanded(bool expanded);
    El* AriaActiveDescendant(bool active = true);
    El* AriaNumericValue(float value);
    El* AriaMinNumericValue(float value);
    El* AriaMaxNumericValue(float value);
    El* AriaNumericValueStep(float value);
    El* AriaOrientation(AccessibilityOrientation orientation);
    El* AriaPositionInSet(int position);
    El* AriaSizeOfSet(int size);
    El* AriaRowCount(int count);
    El* AriaColumnCount(int count);
    El* AriaRowIndex(int index);
    El* AriaColumnIndex(int index);
    El* AriaLevel(int level);
    El* OnAccessibilityDefault(Listener fn);
    El* OnAccessibilityIncrement(Listener fn);
    El* OnAccessibilityDecrement(Listener fn);
    El* OnAccessibilityIncrement(Func0 fn);
    El* OnAccessibilityDecrement(Func0 fn);

    El* PathId(Str name);

    El* PathClick(Str name);

    El* PathFocus(Str name);

    El* ScrollFromPath();
    El* OnClick(Func0 fn);
    El* OnClick(Listener l);

    El* OnScroll(Listener l);
    El* OnHover(Listener l);
    El* OnMouseMove(Listener l);
    El* OnMouseDown(Listener l, DispatchPhase phase = DispatchPhase::Bubble);
    El* OnMouseUp(Listener l, DispatchPhase phase = DispatchPhase::Bubble);
    El* OnDragMove(Listener l);
    El* OnDrag(Str dragKind, int ix = 0, void* data = nullptr);
    El* OnMouseDownOut(Listener l);
    El* OnMouseUpOut(Listener l);
    El* StopMouseDown();

    El* StopClick();
    El* SuppressTextSelection();
    El* OnDrop(Str acceptKind, Listener l);

    El* Refine(const Style& s, uint32_t fields);

    El* Hover(const struct StateStyle& s);
    El* Active(const struct StateStyle& s);
    El* Focus(const struct StateStyle& s);

    El* DragOver(Str dragKind, const struct StateStyle& s);
    ElStyleStates* StyleStates();
    const ElStyleStates* StyleStates() const;
    ElStyleStates* EnsureStyleStates();
    ChartSeries* Chart();
    const ChartSeries* Chart() const;
    El* BoundsOut(gpui::Bounds* out);
    El* ReportLineSpan(float lineHeight);
    El* LineClamp(float cap, Listener onChange = {});
    El* Cursor(CursorKind c);
    El* BindSlider(SliderState* s, Axis axis = Axis::Horizontal);
    El* BindSliderBounds(SliderState* s);
    El* BindInput(InputState* s);

    El* BindInputText(InputState* s);

    El* SelRange(int lo, int hi, Rgba color);

    El* ExtraSelRanges(const Selection* ranges, int n);
    El* ExtraCarets(const int* offsets, int n, Rgba color);

    El* CaretOut(float* outX, float* outY);

    El* RangeOut(int lo, int hi, gpui::Bounds* out);
    El* Washes(const TextSpan* runs, int n);
    El* Underlines(const TextSpan* runs, int n);

    El* Fades(const TextFade* runs, int n);

    const TextFade* FadesGet(int* n) const;

    El* Whitespaces(Rgba color);

    El* IndentGuides(Rgba color, int indentCount, int tabSize);
    El* Spans(const TextSpan* runs, int n);

    El* MarkRange(int lo, int hi);
    El* Caret(int off, Rgba color, float width = 2,
              bool lineEndAffinity = false);
    El* Child(El* c);
    El* Bold();
    El* Semibold();
    El* Medium();
    El* Weight(FontWeight value);
    El* Mono();

    El* FontFamily(Str name);

    El* FontFamilyId(uint8_t family);
    El* Underline();
    El* Strikethrough();
    El* Italic();
    El* Selectable();
    El* SelectionOwner(EntityId owner);

    El* SelSrc(const SelSource* s, bool join);
    El* SelMap(const SelSourceMap* m);
    El* Wrap();
    El* Dashed();
    El* DashArray(float on, float off);
    El* Absolute();
    El* Fixed();
    El* Deferred();
    El* DeferredLayer(int layer);
    El* ZIndex(int z);
    El* AnchorBelow(float gap = 0);

    El* AnchorFlip(bool on = true);
    El* AnchorAbove(float gap = 0);
    El* AnchorCenterX();

    El* AnchorCorner(Anchor anchor, float margin = 4, float offset = 0);
    El* Top(float v);
    El* Left(float v);
    El* Bottom(float v);
    El* Right(float v);
    El* LeftRel(float frac);
    El* RightRel(float frac);
    El* TopRel(float frac);
    El* BottomRel(float frac);
    El* HoverBg(Background c);
    El* HoverFg(Rgba c);

    El* ActiveBg(Background c);

    El* FocusOnPress(bool v = true);
    El* Group();
    El* GroupHoverVisible();

    El* GroupHoverBg(Background c);
    El* FocusId(int v);

    El* TrackFocus(FocusHandle handle);
    El* KeyContext(Str name);

    El* OnAction(uint32_t action, Listener fn);

    El* OnClickAction(uint32_t action, int64_t arg = 0);
    El* OnClickActionAt(uint32_t action, FocusHandle focus, int64_t arg = 0);

    El* OnKeyDown(Listener fn);

    El* CaptureKeyDown(Listener fn);

    El* OnKeyUp(Listener fn);

    El* OnScrollWheel(Listener fn);
    El* TabIndex(int v);
    El* TabStop(bool v);

    El* FocusRing(bool v = true);

    El* FocusLineStyle(FocusLine line);

    El* FocusLineStyle(FocusLine line, Rgba color);
#ifndef NDEBUG

    El* Debug();

    El* DebugBelow();
#endif
    El* TrapId(int v);
    El* Tip(Str s);

    El* TipPlacement(int placement);
    El* Id(Str s);
};

static_assert(sizeof(unsigned int) == 4,
              "El flags require a four-byte unsigned int");

#ifdef NDEBUG
static_assert(sizeof(El) <= 1880,
              "keep El flags packed and members alignment-ordered");
#else
static_assert(sizeof(El) <= 1896,
              "keep El flags packed and members alignment-ordered");
#endif

enum class BtnKind : uint8_t {
    Default,
    Primary,
    Outline
};

El* ButtonEl(Arena* a, int clickId, Str label, BtnKind kind = BtnKind::Default);
El* ButtonSmall(Arena* a, int clickId, Str label, BtnKind kind, bool selected);

#if defined(__clang__) || defined(_MSC_VER)
#define GPUI_CALLER_COLUMN() __builtin_COLUMN()
#else
#define GPUI_CALLER_COLUMN() 0
#endif
El* Div(Arena* a, const char* file = __builtin_FILE(),
        int line = __builtin_LINE(), int column = GPUI_CALLER_COLUMN());
El* TextEl(Arena* a, Str s);

Str ElementIdNamed(Arena* a, Str name, uint64_t ix);
Str ElementIdInteger(Arena* a, uint64_t ix);

int ElementIdKindOf(Str id, int* nameLen);

void ElementIdFrameBegin();

Str DebugSourceMessage(Arena* a, Str cwd, const char* file, int line,
                       int column);
El* IconEl(Arena* a, IconName name);
El* IconEl(Arena* a, IconName name, float size);

El* ImageEl(Arena* a, Str src, Str alt = {});
El* ImageEl(Arena* a, ImageSource source, Str alt = {});
El* ProgressEl(Arena* a, float value01to100, float barW, float barH);

void ChartValueDomain(const ChartSeries& c, float* outMin, float* outMax);

void ChartRadarDomain(const ChartSeries& c, float* outMin, float* outMax);

float ChartRadarFraction(float lo, float hi, float v);

El* ChartEl(Arena* a, const float* ys, int n, Rgba stroke, Rgba fillTop,
            Rgba fillBot, int tickMargin);

struct HitRect {

    int focusId = 0;
    int id = 0;

    uint32_t pathId = 0;
    Bounds bounds = {};
    Func0 onClick;
    Listener listener;
    Listener onHover;
    Listener onMouseMove;
    Listener onMouseDown;
    Listener onMouseUp;

    DispatchPhase mouseDownPhase = DispatchPhase::Bubble;
    DispatchPhase mouseUpPhase = DispatchPhase::Bubble;

    int parent = -1;
    Listener onDragMove;
    DragPayload drag = {};
    Listener onMouseDownOut;
    Listener onMouseUpOut;

    Listener onScrollWheel;
    Str dropKind = {};
    Listener onDrop;
    CursorKind cursor = CursorKind::Arrow;

    Str tooltip = {};
    int8_t tooltipPlacement = -1;

    bool rootTooltip = false;
    SliderState* slider = nullptr;
    Axis sliderAxis = Axis::Horizontal;
    InputState* input = nullptr;

    uint32_t clickAction = 0;
    int64_t clickActionArg = 0;
    int clickActionFocusId = 0;

    bool stopClick = false;
    bool stopMouseDown = false;
    bool suppressTextSelection = false;

    int paintLayer = 0;

    int sceneContext = 0;
};

struct AccessibilityNode {
    uint32_t id = 0;
    int parent = -1;
    Bounds bounds = {};
    AccessibilityInfo info = {};
    uint8_t actions = AccessibilityActionNone;
    int clickId = 0;
    int focusId = 0;
    Func0 onClick = {};
    Listener listener = {};
    Listener accessibilityDefault = {};
    Listener accessibilityIncrement = {};
    Listener accessibilityDecrement = {};
    Func0 accessibilityIncrementDirect = {};
    Func0 accessibilityDecrementDirect = {};
    uint32_t clickAction = 0;
    int64_t clickActionArg = 0;
    int clickActionFocusId = 0;
    SliderState* slider = nullptr;
    InputState* input = nullptr;
};

struct ScrollRect {
    int id = 0;
    Bounds bounds = {};
    float contentH = 0;
    float scrollY = 0;
    float contentW = 0;
    float scrollX = 0;
    ScrollbarMode mode = ScrollbarMode::Always;

    bool barX = true;
    bool barY = true;

    bool barVisible = true;
    float trackWidth = 16;
    float thumbWidth = 6;
    float thumbHoverWidth = 8;
    float thumbActiveWidth = 8;
    float thumbInset = 4;
    float thumbHoverInset = 4;
    float thumbActiveInset = 4;
    float thumbMinLength = 48;
    float thumbHoverMinLength = 48;
    float thumbActiveMinLength = 48;
    uint8_t maskAxes = 0;

    int maskHit = -1;
    Listener onScroll;

    InputState* input = nullptr;
};

const float kScrollbarThumbW = 6.f;
const float kScrollbarThumbActiveW = 8.f;
const float kScrollbarThumbMargin = 4.f;
const float kScrollbarBandW =
    kScrollbarThumbActiveW + kScrollbarThumbMargin * 2.f;

struct ScrollEvent {
    int id = 0;
    float offsetY = 0;

    float offsetX = 0;
};

struct TextHit {
    Bounds bounds = {};
    Str text;
    float font = 14;
    float maxW = 0;
    bool wrap = false;

    TextAlign align = TextAlign::Left;
    int docOff = 0;
    EntityId owner = {};

    const SelSource* src = nullptr;

    const SelSourceMap* map = nullptr;

    bool join = false;

    bool atom = false;

    int scope = 0;

    int paintLayer = 0;
};

struct TextMeasCache {
    void* slots = nullptr;
    int cap = 0;
    int used = 0;
    uint32_t frame = 0;
};

struct PaintApp;
struct PaintTarget;

enum StyleField : uint32_t {
    StyleFieldBg = 1u << 0,
    StyleFieldColor = 1u << 1,
    StyleFieldBorderColor = 1u << 2,
    StyleFieldPad = 1u << 3,
    StyleFieldGap = 1u << 4,
    StyleFieldRadius = 1u << 5,
    StyleFieldBorder = 1u << 6,
    StyleFieldFontSize = 1u << 7,
    StyleFieldWidth = 1u << 8,
    StyleFieldHeight = 1u << 9,
    StyleFieldOpacity = 1u << 10,

    StyleFieldHoverBg = 1u << 11,
    StyleFieldHoverFg = 1u << 12,
    StyleFieldActiveBg = 1u << 13,

    StyleFieldBorderT = 1u << 14,
    StyleFieldBorderB = 1u << 15,
    StyleFieldBorderL = 1u << 16,
    StyleFieldBorderR = 1u << 17,
    StyleFieldMargin = 1u << 18,

    StyleFieldMinWidth = 1u << 19,
    StyleFieldMinHeight = 1u << 20,
    StyleFieldMaxWidth = 1u << 21,
    StyleFieldMaxHeight = 1u << 22
};

void StyleApplyFields(Style* into, const Style& over, uint32_t fields);

void StyleOverrideSet(int clickId, uint32_t fields, const Style& style);
void StyleOverrideClear(int clickId);
void StyleOverrideClearAll();

void StyleOverrideApply(El* e);

struct InspectorPick {
    int id = 0;
    Str elId = {};

    Style style = {};
    Bounds bounds = {};

    int kind = 0;
    int depth = 0;
    bool hasBg = false;
    Rgba bg = {};
    float pad = 0;
    float gap = 0;
    float radius = 0;
    float border = 0;
    bool row = true;
    float font = 0;

    Str text = {};
};

struct InspectorState {
    bool on = false;
    bool picking = false;
    bool hasPick = false;
    InspectorPick pick = {};

    bool pending = false;
    float pendingX = 0;
    float pendingY = 0;
};

namespace scene {
struct State;
}

struct PaintIdLink {

    Str id = {};
    EntityId view = {};
    const PaintIdLink* parent = nullptr;
};

Str PaintIdChainDebug(Arena* a, const PaintIdLink* chain);

struct PaintCtx {
    App* app = nullptr;
    Window* window = nullptr;
    PaintApp* pa = nullptr;
    PaintTarget* rt = nullptr;

    bool headless = false;

    scene::State* sceneState = nullptr;

    float opacity = 1;
    float dpi = 96;
    float viewW = 0;
    float viewH = 0;

    float clientInset = 0;
    int hoverId = 0;

    int dragOverId = 0;
    Str dragKind = {};

    int activeId = 0;

    bool groupHovered = false;
    int focusId = 0;

    int focusGen = 0;

    int debugBelow = 0;

    const PaintIdLink* idChain = nullptr;

    float mouseX = -1;
    float mouseY = -1;

    int scrollDragId = 0;
    bool scrollDragHorizontal = false;

    bool wantsAnimFrame = false;

    int hitParent = -1;

    Bounds hitMask = {};
    bool hasHitMask = false;

    bool picking = false;
    bool pickHit = false;

    int paintDepth = 0;

    int paintLayer = 0;

    int pickTier = 0;
    InspectorPick pick = {};
    Vec<HitRect> hits;

    Vec<El*> deferredPaint;
    Vec<ScrollRect> scrolls;
    Vec<TextHit> texts;

    Vec<InputState*> inputs;
    int textDocLen = 0;
    int selA = -1;
    int selB = -1;

    int selScope = -1;
    TextMeasCache textCache;

    PaintCtx() = default;
};

struct LineSpan {
    float top = 0;
    float bottom = 0;
    float lineHeight = 0;
};

bool LineSafeClipBottom(const LineSpan* spans, int count, float boxBottom,
                        float contentBottom, float* outBottom);

struct FocusRect {
    int id = 0;
    int trapId = 0;
    int tabIndex = 0;
    bool tabStop = true;
    bool focusOnPress = false;

    InputState* input = nullptr;

    int dispatchIx = 0;
    Bounds bounds = {};

    Listener accessibilityIncrement = {};
    Listener accessibilityDecrement = {};
    Func0 accessibilityIncrementDirect = {};
    Func0 accessibilityDecrementDirect = {};
};

struct DispatchNode {
    int subtreeEnd = 0;

    int depth = 0;
    uint32_t context = 0;
    uint32_t action = 0;
    Listener fn = {};
};

enum class CharKind : uint8_t {
    Word,
    Whitespace,
    Newline,
    Other
};

int Utf8At(Str s, int i, uint32_t* out);

int Utf8Prev(Str s, int i);

enum class Bias : uint8_t {
    Left,
    Right
};

struct RopePoint {
    int row = 0;
    int column = 0;
};

int RopeClipOffset(Str text, int offset, Bias bias);

int RopeCharAt(Str text, int offset, uint32_t* out);
int RopeLinesLen(Str text);
int RopeLineStartOffset(Str text, int row);
int RopeLineEndOffset(Str text, int row);

Str RopeSliceLine(Str text, int row);
int RopeLineLen(Str text, int row);
RopePoint RopeOffsetToPoint(Str text, int offset);
int RopePointToOffset(Str text, RopePoint point);
int RopeOffsetUtf16ToOffset(Str text, int offsetUtf16);
int RopeOffsetToOffsetUtf16(Str text, int offset);
int RopeCharIndexToOffset(Str text, int charIndex);
int RopeOffsetToCharIndex(Str text, int offset);

enum class InputEventKind : uint8_t {
    Change,
    PressEnter,
    Focus,
    Blur
};

struct InputEvent {
    InputEventKind kind = InputEventKind::Change;

    bool secondary = false;
    bool shift = false;
};

struct Selection {
    int start = 0;
    int end = 0;

    int Len() const { return end > start ? end - start : 0; }
    bool IsEmpty() const { return start == end; }
    bool Contains(int offset) const { return offset >= start && offset < end; }
};

inline Selection SelectionAt(int offset) {
    return Selection{offset, offset};
}

struct CursorSelection {
    Selection range = {};
    bool reversed = false;

    int preferredColumn = -1;
    float preferredX = -1;

    int Cursor() const { return reversed ? range.start : range.end; }
    bool IsEmpty() const { return range.IsEmpty(); }
};

enum class EditIntent : uint8_t {
    Typing,
    Backspace,
    DeleteForward,
    Atomic
};

struct TokenDelta;

struct AutoClosedPairRange {
    int openStart = 0;
    int openEnd = 0;
    int closeStart = 0;
    int closeEnd = 0;
};

struct Change {
    Selection oldRange = {};
    Str oldText = {};
    Selection newRange = {};
    Str newText = {};
    Selection selBefore = {};
    Selection selAfter = {};
    TokenDelta* tokenDelta = nullptr;
};

struct UndoTransaction {
    EditIntent intent = EditIntent::Atomic;
    Change* changes = nullptr;
    int len = 0;
    int cap = 0;

    int lastBatchLen = 0;

    int recordedChanges = 0;

    CursorSelection* selsBefore = nullptr;
    int nSelsBefore = 0;
    CursorSelection* selsAfter = nullptr;
    int nSelsAfter = 0;

    AutoClosedPairRange* pairsBefore = nullptr;
    int nPairsBefore = 0;
    AutoClosedPairRange* pairsAfter = nullptr;
    int nPairsAfter = 0;
};

struct UndoManager {
    Vec<UndoTransaction> undos;
    Vec<UndoTransaction> redos;
    bool ignoring = false;

    int transactionDepth = 0;

    UndoTransaction pending = {};

    bool hasPendingIntent = false;
    EditIntent pendingIntent = EditIntent::Atomic;
    bool coalescingBoundary = false;

    ~UndoManager();
};

void UndoRecordTransaction(UndoManager* m, Change change, EditIntent intent);
void UndoBeginTransaction(UndoManager* m);

void UndoBeginTransactionWith(UndoManager* m, EditIntent intent);

void UndoRecordSelections(UndoManager* m, const CursorSelection* before,
                          int nBefore, const CursorSelection* after,
                          int nAfter);
void UndoCommitTransaction(UndoManager* m);
void UndoBreakCoalescing(UndoManager* m);
void UndoSetIgnoring(UndoManager* m, bool ignoring);
bool UndoIsIgnoring(const UndoManager* m);
void UndoClear(UndoManager* m);

const UndoTransaction* UndoPopUndo(UndoManager* m);
const UndoTransaction* UndoPopRedo(UndoManager* m);

enum class MaskToken : uint8_t {
    Digit,
    Letter,
    LetterOrDigit,
    Any,
    Sep
};

enum class MaskKind : uint8_t {
    None,
    Pattern,
    Number
};

struct MaskPattern {
    MaskKind kind = MaskKind::None;

    Str pattern = {};

    uint32_t separator = 0;

    int fraction = -1;
};

MaskPattern MaskPatternNew(Str pattern);
MaskPattern MaskPatternNumber(uint32_t separator);
void MaskPatternFree(MaskPattern* p);

bool MaskTokenAt(const MaskPattern& p, int pos, MaskToken* out, uint32_t* sep);
bool MaskIsNone(const MaskPattern& p);
bool MaskIsValid(const MaskPattern& p, Str maskText);
bool MaskIsValidAt(const MaskPattern& p, uint32_t ch, int pos);

Str MaskApply(Arena* a, const MaskPattern& p, Str text);

Str MaskUnapply(Arena* a, const MaskPattern& p, Str maskText);

Str MaskPlaceholder(Arena* a, const MaskPattern& p);

Str NormalizeNumberInput(Arena* a, Str text);

enum class InputKind : uint8_t {
    Input,
    Textarea,
    Editor
};

enum class LayoutModeKind : uint8_t {
    PlainText,
    AutoGrow,
    CodeEditor
};

struct LayoutMode {
    LayoutModeKind kind = LayoutModeKind::PlainText;
    int rows = 1;
    int minRows = 1;
    int maxRows = 0;

    int tabSize = 2;
    bool lineNumber = false;

    bool folding = false;
};

bool LayoutModeIsFolding(const LayoutMode& m);

void LayoutModeSetRows(LayoutMode* m, int rows);
int LayoutModeRows(const LayoutMode& m);
int LayoutModeMinRows(const LayoutMode& m);

const float kAutoScrollMinSpeed = 12.f;
const float kAutoScrollMaxSpeed = 64.f;

const float kAutoScrollInnerZone = 16.f;

const float kAutoScrollOuterRamp = 80.f;

bool AutoScrollComputeDelta(float y, Bounds bounds, float* out);

struct AutoScroll {

    float delta = 0;
    bool active = false;

    Point lastDrag = {};
    bool hasLastDrag = false;

    bool IsActive() const { return active; }

    void Set(float d) {
        delta = d;
        active = true;
    }
    void SetNone() {
        delta = 0;
        active = false;
    }

    void Stop() {
        SetNone();
        lastDrag = {};
        hasLastDrag = false;
    }
};

struct SearchMatcher {

    Vec<Selection> ranges;
    int current = 0;

    bool caseInsensitive = true;

    Str query = {};

    Vec<char> text;

    bool replacing = false;

    ~SearchMatcher() {
        VecReset(ranges);
        VecReset(text);
        StrFree(query);
    }
};

void SearchMatcherReset(SearchMatcher* m);

void SearchMatcherUpdate(SearchMatcher* m, Str text);
void SearchMatcherUpdateQuery(SearchMatcher* m, Str query, bool insensitive);
inline int SearchMatcherLen(const SearchMatcher* m) {
    return m->ranges.len;
}
inline bool SearchMatcherIsEmpty(const SearchMatcher* m) {
    return m->ranges.len == 0;
}
inline int SearchMatcherIndex(const SearchMatcher* m) {
    return m->current;
}

inline int SearchMatcherCurrentIndex(const SearchMatcher* m) {
    return m->ranges.len == 0 ? -1 : m->current;
}

Str SearchMatcherLabel(Arena* a, const SearchMatcher* m);

void SearchMatcherSetIndex(SearchMatcher* m, int ix);
void SearchMatcherBeginReplacement(SearchMatcher* m);
bool SearchMatcherHasNextWithoutWrap(const SearchMatcher* m);

bool SearchMatcherPeek(const SearchMatcher* m, Selection* out);

bool SearchMatcherCurrent(const SearchMatcher* m, Selection* out);

void SearchMatcherCursorByOffset(SearchMatcher* m, int offset);

bool SearchMatcherNext(SearchMatcher* m, Selection* out);
bool SearchMatcherPrev(SearchMatcher* m, Selection* out);

struct InputWrapLine {
    int firstStart = 0;
    int nRows = 1;
    float indent = 0;

    int rowsAbove = 0;
};

struct InputWrapMap {
    Vec<InputWrapLine> lines;

    Vec<int> starts;
    bool valid = false;
    uint64_t docVersion = 0;

    float width = 0;
    float fontSize = 0;
    uint16_t fontWord = 0;
    uint8_t wrappingIndent = 1;
    uint64_t tokenKey = 0;
    int totalRows = 0;

    float measuredWidth = 0;

    float charFont = 0;
    uint16_t charWord = 0;
    float asciiWidths[128] = {};
    uint32_t otherChars[256] = {};
    float otherWidths[256] = {};
    float spaceWidth = 0;

    Vec<float> tokenWidths;

    bool hasEdit = false;
    bool editWhole = false;
    int editStart = 0;
    int editOldEnd = 0;
    int editNewEnd = 0;

    Vec<int> dirtyLines;
};

struct FoldRange {
    int startLine = 0;
    int endLine = 0;
};

struct FoldHiddenRows {
    int start = 0;
    int end = 0;
    int hiddenBefore = 0;
};

struct FoldMap {

    Vec<FoldRange> candidates;

    Vec<FoldRange> folded;

    Vec<FoldHiddenRows> hidden;

    int totalHidden = 0;
    bool needsRebuild = true;

    int cachedLineCount = 0;
};

void FoldMapSetCandidates(FoldMap* m, const FoldRange* ranges, int n);

void FoldMapSetFolded(FoldMap* m, int startLine, bool folded);
void FoldMapToggle(FoldMap* m, int startLine);
bool FoldMapIsFolded(const FoldMap* m, int startLine);
bool FoldMapIsCandidate(const FoldMap* m, int startLine);

void FoldMapClearFolds(FoldMap* m);

void FoldMapAdjustForEdit(FoldMap* m, int editStartLine, int editEndLine,
                          int lineDelta);

void FoldMapRebuild(FoldMap* m, int lineCount);

void FoldMapSetHiddenRows(FoldMap* m, int lineCount, Selection* ranges, int n);

int FoldMapDisplayRowCount(const FoldMap* m);

int FoldMapDisplayRow(const FoldMap* m, int line);
int FoldMapLineAt(const FoldMap* m, int displayRow);

bool FoldMapLineHidden(const FoldMap* m, int line);

int FoldMapNearestVisibleLine(const FoldMap* m, int line);

struct FoldIconBox {
    int line = 0;
    Bounds bounds = {};
};

struct SearchSession {

    bool open = false;
    bool replaceMode = false;
    bool caseInsensitive = true;
    Str query = {};
    Str replacement = {};

    int anchorOffset = -1;
    SearchMatcher matcher;

    bool active = false;

    ~SearchSession() {
        StrFree(query);
        StrFree(replacement);
    }
};

inline bool SearchSessionIsActive(const SearchSession* s) {
    return s && s->active;
}

void SearchSessionSetQuery(SearchSession* s, Str query, bool insensitive);
void SearchSessionSetReplacement(SearchSession* s, Str replacement);

enum class DiagnosticSeverity : uint8_t {
    Hint,
    Error,
    Warning,
    Info
};

enum class DiagnosticTag : uint8_t {
    Unnecessary = 1,
    Deprecated = 2
};

struct DiagnosticRelatedInformation {
    Str uri = {};
    Selection range = {};
    Str message = {};
};

using RelatedInformation = DiagnosticRelatedInformation;

struct Diagnostic {
    Selection range = {};
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    Str message = {};
    Str source = {};
    Str code = {};
    Str codeDescriptionUri = {};
    const DiagnosticRelatedInformation* relatedInformation = nullptr;
    int nRelatedInformation = 0;
    const DiagnosticTag* tags = nullptr;
    int nTags = 0;

    Str data = {};
};

struct TextEditItem {
    Selection range = {};
    Str newText = {};
};

void InputApplyEdits(InputState* s, App* app, Window* win,
                     const TextEditItem* edits, int n);

struct CompletionItem {

    Str label = {};

    Str detail = {};

    Str insertText = {};

    Str documentation = {};
    bool deprecated = false;

    bool resolved = false;

    const TextEditItem* additionalEdits = nullptr;
    int nAdditionalEdits = 0;
};

using CompletionFn = int (*)(void* data, Str text, int offset, Str query,
                             CompletionItem* out, int cap);

struct DocumentColor {
    Selection range = {};
    Rgba color = {};
};

using DocumentColorFn = int (*)(void* data, Str text, DocumentColor* out,
                                int cap);

const int kMaxDocumentColors = 10000;

struct CodeActionItem {
    Str title = {};

    int provider = 0;
    Selection range = {};
    Str newText = {};

    const TextEditItem* edits = nullptr;
    int nEdits = 0;
};

struct CodeActionItem;
using CodeActionPerformFn = bool (*)(void* data, InputState* s, App* app,
                                     Window* win, const CodeActionItem* item);

using CodeActionFn = int (*)(void* data, Arena* a, Str text, Selection sel,
                             CodeActionItem* out, int cap);

struct CodeActionProviderEntry {
    CodeActionFn provide = nullptr;
    void* data = nullptr;
    CodeActionPerformFn perform = nullptr;
};

struct CodeActionSession {
    bool open = false;
    int selected = 0;
    Vec<CodeActionItem> items;

    uint64_t revision = 0;

    Arena* arena = nullptr;

    ~CodeActionSession();
};

using HoverFn = Str (*)(void* data, Str text, int offset);

enum class CompletionTrigger : uint8_t {

    Continue,

    Open,

    Close
};

using CompletionTriggerFn = CompletionTrigger (*)(void* data, Str text,
                                                  int offset, Str typed);

using CompletionResolveFn = Str (*)(void* data, Arena* a,
                                    const CompletionItem* item);

const float kCompletionMenuMaxW = 320.f;

using InlineCompletionFn = Str (*)(void* data, Arena* a, Str text, int offset);

const float kInlineCompletionDebounceMs = 300.f;

struct InlineCompletion {

    Str text = {};
    int at = -1;

    double dueAt = 0;
    bool asked = true;
    Arena* arena = nullptr;

    ~InlineCompletion();
};

struct SemanticToken {
    uint32_t deltaLine = 0;
    uint32_t deltaStart = 0;
    uint32_t length = 0;
    uint32_t tokenType = 0;
    uint32_t tokenModifiers = 0;
};

struct SemanticSpan {
    int line = 0;
    int col = 0;
    int len = 0;
    Str name = {};
};

struct SemanticRange {
    Selection range = {};
    Str name = {};
};

using SemanticTokensFn = int (*)(void* data, Str text, Selection range,
                                 SemanticToken* out, int cap);

int SemanticTokensDecode(const SemanticToken* toks, int n, const Str* names,
                         int nNames, SemanticSpan* out, int cap);

int SemanticTokensForRange(const SemanticSpan* toks, int n, Str text,
                           Selection visible, SemanticRange* out, int cap);

struct DefinitionLink {

    Selection origin = {};
    Str uri = {};

    Selection target = {};
};

using DefinitionFn = int (*)(void* data, Arena* a, Str text, int offset,
                             DefinitionLink* out, int cap);

using ShowDocumentFn = bool (*)(void* data, Str uri, bool external,
                                Selection selection);

struct HoverDefinition {
    Selection symbolRange = {};
    Vec<DefinitionLink> locations;

    Bounds bounds = {};

    Arena* arena = nullptr;

    ~HoverDefinition();
};

struct CompletionSession {
    bool open = false;

    int triggerStart = -1;
    int offset = 0;
    int selected = 0;

    Str query = {};

    Vec<CompletionItem> items;

    uint64_t revision = 0;

    Arena* arena = nullptr;

    ~CompletionSession();
};

struct DiagnosticColors {
    Rgba error = {};
    Rgba warning = {};
    Rgba info = {};
    Rgba hint = {};
};

enum class InputAction : uint8_t;

enum class InputOverlayKind : uint8_t {
    Completion,
    CodeAction
};

using OverlayActionFn = bool (*)(void* data, InputOverlayKind kind,
                                 InputAction action);

struct InputEdit {
    int startByte = 0;
    int oldEndByte = 0;
    int newEndByte = 0;
    RopePoint startPosition = {};
    RopePoint oldEndPosition = {};
    RopePoint newEndPosition = {};

    static InputEdit New(Str oldText, Selection range, Str inserted);
};

struct InputEditWithText {
    InputEdit edit = {};
    Str text = {};
};

struct InputHighlightEdit {
    InputEdit edit = {};
    int removedAt = 0;
    int removedLen = 0;
};

struct HighlightStyleResolver {
    void* data = nullptr;
    bool (*style)(void* data, Str name, TextSpan* out) = nullptr;

    bool Style(Str name, TextSpan* out) const;
};

using SharedHighlightStyleResolver = HighlightStyleResolver;

struct InputHighlighter {
    void* data = nullptr;
    Str (*language)(void* data) = nullptr;

    void (*update)(void* data, const InputEdit* edit, Str text,
                   bool folding) = nullptr;

    void (*updateBatch)(void* data, const InputEditWithText* edits, int n,
                        bool folding) = nullptr;

    int (*styles)(void* data, Selection range,
                  const HighlightStyleResolver* resolver, Arena* a,
                  TextSpan** out) = nullptr;

    int (*foldRanges)(void* data, Str text, Selection changedRange, Arena* a,
                      FoldRange** out) = nullptr;

    void (*drop)(void* data) = nullptr;

    Str Language() const;
    void Update(const InputEdit* edit, Str text, bool folding) const;
    void UpdateBatch(const InputEditWithText* edits, int n, bool folding) const;
    int Styles(Selection range, const HighlightStyleResolver* resolver,
               Arena* a, TextSpan** out) const;
    int FoldRanges(Str text, Selection changedRange, Arena* a,
                   FoldRange** out) const;
};

int InputComposeSpans(TextSpan* spans, int n, const TextSpan* decs, int nDecs,
                      int cap, TextSpan* tmp);

struct NativeMenu;
struct InputContextMenuCapabilities;

using InputContextMenuFn = void (*)(void* data, NativeMenu* menu,
                                    const InputContextMenuCapabilities& caps,
                                    Point position, App* app, Window* win);

struct InputState {
    InputKind kind = InputKind::Input;
    LayoutMode mode = {};

    FocusHandle focus = {};

    FocusHandle frameFocus = {};

    Vec<char> text;

    uint64_t docVersion = 0;

    Vec<int> lineStarts;
    uint64_t lineStartsVersion = 0;
    bool lineStartsValid = false;

    InputHighlighter highlighter = {};

    Vec<InputHighlightEdit> highlightEdits;
    Vec<char> highlightRemoved;
    bool highlightWhole = false;
    bool hasPendingEdit = false;
    Selection selectedRange = {};
    bool selectionReversed = false;

    bool hasSelectedWordRange = false;
    Selection selectedWordRange = {};

    Vec<CursorSelection> extraCursors;

    struct ColumnarPoint {
        int offset = -1;
        int columnsPastLineEnd = 0;

        bool IsValid() const { return offset >= 0; }
    } columnSelectStart;
    UndoManager undo;
    MaskPattern maskPattern = {};
    bool maskPatternSet = false;
    Str placeholder = {};
    bool focused = false;

    Window* focusWin = nullptr;
    bool disabled = false;
    bool readonly = false;

    bool enableContextMenu = true;

    InputContextMenuFn contextMenuHandler = nullptr;
    void* contextMenuData = nullptr;

    void (*contextMenuDrop)(void* data) = nullptr;

    bool hasPendingContextMenu = false;
    Point pendingContextMenuAt = {};
    int pendingContextMenuOffset = 0;
    bool loading = false;

    bool numberHasMin = false;
    bool numberHasMax = false;
    double numberMin = 0;
    double numberMax = 0;

    bool masked = false;

    struct InlineTokenStore* tokens = nullptr;
    bool cleanOnEscape = false;
    bool submitOnEnter = false;

    InputPasteFn pasteHandler = nullptr;
    void* pasteHandlerData = nullptr;

    bool autoClose = true;
    bool smartIndent = true;

    Vec<AutoClosedPairRange> autoClosed;

    bool searchable = false;
    bool replaceable = true;
    SearchSession search;
    uint64_t searchActivationRevision = 0;

    FoldMap folds;
    Vec<FoldIconBox> foldIcons;

    Bounds gutterBox = {};
    bool softWrap = true;

    bool showWhitespaces = false;

    int scrollBeyondLastLine = -1;
    int cursorSurroundingLines = -1;

    int align = 0;

    bool selecting = false;

    AutoScroll autoScroll;

    EntityId blink = {};
    Listener onChange = {};

    bool (*validate)(Str text, int64_t arg) = nullptr;
    int64_t validateArg = 0;

    Bounds lastBounds = {};
    float lastFont = 0;
    float lastLineH = 0;

    uint16_t lastFontWord = 0;

    InputWrapMap wrap;

    uint8_t wrappingIndent = 1;

    Vec<Diagnostic> diagnostics;

    int hoverDiagnostic = -1;
    float hoverDiagnosticX = 0;
    float hoverDiagnosticY = 0;

    HoverFn hoverProvider = nullptr;
    void* hoverData = nullptr;
    Str hoverText = {};
    Selection hoverRange = {};
    float hoverX = 0;
    float hoverY = 0;

    double hoverDueAt = 0;
    Selection hoverPending = {};
    bool hoverAsked = true;

    DocumentColorFn documentColorProvider = nullptr;
    void* documentColorData = nullptr;
    Vec<DocumentColor> documentColors;
    bool documentColorsDirty = true;

    RangeDecorationsState* rangeDecorations = nullptr;

    DecorationCollectionsState* textDecorations = nullptr;

    struct InputPaintedRows* paintedRows = nullptr;
    uint64_t paintedRowsFrame = 0;

    InputSyntaxCache* syntaxCache = nullptr;

    CodeActionSession codeActions;

    Vec<CodeActionProviderEntry> codeActionProviders;

    CodeActionFn codeActionProvider = nullptr;
    void* codeActionData = nullptr;

    InlineCompletionFn inlineCompletionProvider = nullptr;
    void* inlineCompletionData = nullptr;

    float inlineCompletionDebounceMs = kInlineCompletionDebounceMs;
    InlineCompletion inlineCompletion;

    SemanticTokensFn semanticTokensProvider = nullptr;
    void* semanticTokensData = nullptr;

    const Str* semanticLegend = nullptr;
    int nSemanticLegend = 0;
    Vec<SemanticSpan> semanticTokens;
    bool semanticTokensDirty = true;

    DefinitionFn definitionProvider = nullptr;
    void* definitionData = nullptr;
    ShowDocumentFn showDocument = nullptr;
    void* showDocumentData = nullptr;
    HoverDefinition hoverDef;

    CompletionSession completion;
    CompletionFn completionProvider = nullptr;
    void* completionData = nullptr;

    OverlayActionFn overlayAction = nullptr;
    void* overlayActionData = nullptr;

    bool silentReplace = false;

    CompletionTriggerFn completionTrigger = nullptr;
    CompletionResolveFn completionResolve = nullptr;

    float completionMenuMaxW = kCompletionMenuMaxW;

    Bounds contentBox = {};

    bool chipLine = false;

    Bounds inputBounds = {};

    Selection popoverTriggerRange = {};
    Bounds popoverTriggerBounds = {};
    Bounds popoverBounds = {};

    float scrollX = 0;
    float scrollY = 0;
    float viewW = 0;
    float viewH = 0;

    float caretWinX = 0;
    float caretWinY = 0;

    float caretX = 0;
    float contentW = 0;
    float contentH = 0;

    bool emitEvents = true;

    Selection imeMarked = {};
    bool imeMarking = false;

    int preferredColumn = -1;

    float preferredX = -1;

    bool cursorLineEndAffinity = false;

    bool touchLive = false;
    Selection touchRange = {};
    bool touchMenuOpen = false;
    bool touchDragging = false;
    uint8_t touchDragEdge = 0;
    Point touchDragOffset = {};

    ~InputState();
};

enum class InputMoveDir : uint8_t {
    None,
    Up,
    Down
};

void InputScrollToCaret(InputState* s, float caretX, float caretY,
                        InputMoveDir dir);

enum class InputScrollPadding : uint8_t {
    Minimal,
    SurroundingLines
};

void InputScrollToCaretWithPadding(InputState* s, float caretX, float caretY,
                                   InputMoveDir dir,
                                   InputScrollPadding padding);

float InputEmptyBottomHeight(bool isCodeEditor, int overrideRows,
                             float viewportH, float lineH);

int InputLineNumberLen(int totalLines);
int InputDisplayedLineNumber(int number);
float InputCursorSurroundingPadding(bool isAutoGrow, int overrideLines,
                                    int visibleLines, float lineH);

void InputScrollToOffset(InputState* s, int offset, InputMoveDir dir);

bool InputUpdateScrollOffset(InputState* s, App* app, Window* win,
                             const Point* offset);

bool InputOnScrollWheel(InputState* s, App* app, Window* win, float dx,
                        float dy, TouchPhase phase = TouchPhase::Moved);
void InputScrollToOffsetWithPadding(InputState* s, int offset, InputMoveDir dir,
                                    InputScrollPadding padding);

void InputScrollToCursor(InputState* s, InputMoveDir dir);

Str InputValue(const InputState* s);

const Vec<int>& InputLineStarts(const InputState* s);
int InputLinesLen(const InputState* s);
int InputLineStartOffset(const InputState* s, int row);
Str InputSliceLine(const InputState* s, int row);
RopePoint InputOffsetToPoint(const InputState* s, int offset);
const char* InputCStr(const InputState* s);

Str InputUnmaskValue(Arena* a, const InputState* s);

Str InputSelectedValue(const InputState* s);
bool InputIsMultiLine(const InputState* s);
bool InputIsSingleLine(const InputState* s);
bool InputIsEditable(const InputState* s);

struct InputPasteTarget {
    uint64_t documentRevision = 0;
    Selection active = {};
    bool reversed = false;
    Vec<Selection> extra;

    bool operator==(const InputPasteTarget& o) const;
    bool operator!=(const InputPasteTarget& o) const { return !(*this == o); }
};

InputPasteTarget InputPasteTargetOf(const InputState* s);

void InputInsertClipboard(InputState* s, App* app, Window* win,
                          const ClipboardItem& item);

bool InputIsCopyable(const InputState* s);

int InputCursor(const InputState* s);

RopePoint InputCursorPosition(const InputState* s);

void InputSetValue(InputState* s, Str value);

void InputDefaultValue(InputState* s, Str value);

void InputReplaceAll(InputState* s, App* app, Window* win, Str value);
void InputSetPlaceholder(InputState* s, Str value);

void TextareaSetAutoGrow(InputState* s, int minRows, int maxRows);
void TextareaSetRows(InputState* s, int rows);
void InputSetMaskPattern(InputState* s, MaskPattern pattern);

void InputClean(InputState* s, App* app, Window* win);

void InputInsert(InputState* s, App* app, Window* win, Str value);
void InputReplace(InputState* s, App* app, Window* win, Str value);

int InputPreviousBoundary(const InputState* s, int offset);
int InputNextBoundary(const InputState* s, int offset);

int InputStartOfLine(const InputState* s, Window* win = nullptr);
int InputEndOfLine(const InputState* s, Window* win = nullptr);
int InputPreviousStartOfWord(const InputState* s);
int InputNextEndOfWord(const InputState* s);
int InputPreviousStartOfWordAt(const InputState* s, int offset);
int InputNextEndOfWordAt(const InputState* s, int offset);

void InputMoveTo(InputState* s, App* app, Window* win, int offset);
void InputMoveToWithAffinity(InputState* s, App* app, Window* win, int offset,
                             bool lineEndAffinity);

void InputSelectTo(InputState* s, App* app, Window* win, int offset);
void InputSelectToWithAffinity(InputState* s, App* app, Window* win, int offset,
                               bool lineEndAffinity);
void InputSelectAll(InputState* s, App* app, Window* win);
void InputUnselect(InputState* s, App* app, Window* win);
void InputSetSelectedRange(InputState* s, App* app, Window* win, int a, int b);

void InputSelectWord(InputState* s, App* app, Window* win, int offset);
void InputSelectLine(InputState* s, App* app, Window* win, int offset);

int InputCursorCount(const InputState* s);

void InputRemoveExtraCursors(InputState* s);

void InputMergeOverlappingCursors(InputState* s);

void InputAddCursorAt(InputState* s, App* app, Window* win, int offset);

void InputBuildColumnarSelection(InputState* s, App* app, Window* win,
                                 InputState::ColumnarPoint start,
                                 InputState::ColumnarPoint end);
inline void InputBuildColumnarSelection(InputState* s, App* app, Window* win,
                                        int startOffset, int endOffset) {
    InputBuildColumnarSelection(s, app, win, {startOffset, 0}, {endOffset, 0});
}

bool InputReplaceTextInRanges(InputState* s, App* app, Window* win,
                              const Selection* ranges, const Str* texts, int n);

enum class InputAction : uint8_t {
    None,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    MoveHome,
    MoveEnd,
    MoveToStart,
    MoveToEnd,
    MoveToPreviousWord,
    MoveToNextWord,
    MovePageUp,
    MovePageDown,
    SelectLeft,
    SelectRight,
    SelectUp,
    SelectDown,
    SelectAll,
    SelectToStart,
    SelectToEnd,
    SelectToStartOfLine,
    SelectToEndOfLine,
    SelectToPreviousWordStart,
    SelectToNextWordEnd,

    AddCursorAbove,
    AddCursorBelow,
    Backspace,
    Delete,
    DeleteToBeginningOfLine,
    DeleteToEndOfLine,
    DeleteToPreviousWordStart,
    DeleteToNextWordEnd,
    Enter,
    Escape,

    IndentInline,
    OutdentInline,

    Indent,
    Outdent,
    Copy,
    Cut,
    Paste,
    Undo,
    Redo,

    Search,
    Replace,

    ToggleCodeActions,

    ActivateToken
};

InputAction InputActionForKey(const InputState* s, int vk, bool shift,
                              bool ctrl, bool alt, bool platform = false);

bool InputPerform(InputState* s, App* app, Window* win, InputAction action,
                  bool shift);

Str InputCompletionQuery(const InputState* s, int* startOut);

void InputRequestCompletion(InputState* s, App* app, Window* win, bool force);

void InputDismissCompletion(InputState* s);

void InputAcceptCompletion(InputState* s, App* app, Window* win);

bool InputCompletionAction(InputState* s, App* app, Window* win,
                           InputAction action);

void InputShowCompletions(InputState* s, App* app, Window* win);

void InputUpdateDocumentColors(InputState* s);

void InputPresentCompletionItems(InputState* s, int triggerStart, Str query,
                                 const CompletionItem* items, int n);

void InputPresentCodeActions(InputState* s, const CodeActionItem* items, int n);

void InputPresentHover(InputState* s, Selection symbolRange, Str text);
void InputPresentDiagnostic(InputState* s, int index);
void InputClearDiagnosticPopover(InputState* s);

bool InputRouteOverlayAction(InputState* s, App* app, Window* win,
                             InputAction action);

void InputDismissLspOverlays(InputState* s);

bool InputIsContextMenuOpen(const InputState* s);

void InputInsertCompletion(InputState* s, App* app, Window* win,
                           const CompletionItem* item, Selection fallback);

Str InputCompletionDocumentation(InputState* s);

void InputScheduleInlineCompletion(InputState* s);

bool InputUpdateInlineCompletion(InputState* s, bool menuOpen);

bool InputHasInlineCompletion(const InputState* s);
void InputClearInlineCompletion(InputState* s);
bool InputAcceptInlineCompletion(InputState* s, App* app, Window* win);

void InputAddCodeActionProvider(InputState* s, CodeActionFn fn, void* data,
                                CodeActionPerformFn perform = nullptr);

void InputLspUpdate(InputState* s);

void InputLspReset(InputState* s);

void InputUpdateSemanticTokens(InputState* s);

void InputHoverDefinition(InputState* s, int offset);

void InputClearHoverDefinition(InputState* s);

bool InputClickDefinition(InputState* s, App* app, Window* win, int offset,
                          bool secondary);

void InputGoToDefinition(InputState* s, App* app, Window* win);

bool InputCanGoToDefinition(const InputState* s);

void InputFollowDefinition(InputState* s, App* app, Window* win,
                           const DefinitionLink& link);

void InputToggleCodeActions(InputState* s, App* app, Window* win);
void InputDismissCodeActions(InputState* s);
void InputHideContextMenu(InputState* s);

void InputPerformCodeAction(InputState* s, App* app, Window* win);

bool InputCodeActionAction(InputState* s, App* app, Window* win,
                           InputAction action);

bool InputReplaceTextInRange(InputState* s, App* app, Window* win,
                             const Selection* range, Str newText);
void InputSetAutoClose(InputState* s, bool enabled, App* app,
                       Window* win = nullptr);
void InputSetSmartIndent(InputState* s, bool enabled, App* app,
                         Window* win = nullptr);

int Utf8OffsetToUtf16(Str s, int u8);
int Utf16OffsetToUtf8(Str s, int u16);

bool InputMarkedRange(const InputState* s, Selection* out);

void InputReplaceAndMarkText(InputState* s, App* app, Window* win,
                             const Selection* range, Str newText,
                             const Selection* sel);

void InputUnmarkText(InputState* s, App* app, Window* win);

void InputTypeChar(InputState* s, App* app, Window* win, uint32_t ch);

void InputOpenSearch(InputState* s, App* app, Window* win, bool replaceMode);
uint64_t InputSearchActivationRevision(const InputState* s);

void InputCloseSearch(InputState* s, App* app, Window* win);

bool InputIsReplaceable(const InputState* s);
void InputSetSearchReplaceMode(InputState* s, App* app, Window* win, bool on);

void InputSetSearchQuery(InputState* s, App* app, Window* win, Str query,
                         bool insensitive);

bool InputSearchNext(InputState* s, App* app, Window* win, Selection* out);
bool InputSearchPrev(InputState* s, App* app, Window* win, Selection* out);

bool InputSearchReplaceOne(InputState* s, App* app, Window* win, Str with);

int InputSearchReplaceAll(InputState* s, App* app, Window* win, Str with);

void InputUpdateSearch(InputState* s);

void InputFocus(InputState* s, App* app, Window* win);
void InputBlur(InputState* s, App* app, Window* win);

int InputIndexForPosition(const InputState* s, PaintCtx* ctx, float x, float y,
                          bool* lineEndAffinity = nullptr,
                          int* columnsPastLineEnd = nullptr);

int InputFoldIconAt(const InputState* s, float x, float y);

void InputToggleFold(InputState* s, App* app, Window* win, int line);

bool InputUnfoldAt(InputState* s, App* app, Window* win, RopePoint position);

void InputSetFoldCandidates(InputState* s, const FoldRange* ranges, int n);

InputState* InputAtPosition(PaintCtx* ctx, float x, float y);

struct SliderValue {
    float lo = 0;
    float hi = 0;
    bool range = false;

    float Start() const { return range ? lo : hi; }
    float End() const { return hi; }
};

inline SliderValue SliderSingle(float v) {
    return {0, v, false};
}
inline SliderValue SliderRange(float lo, float hi) {
    return {lo, hi, true};
}

SliderValue SliderValueClamp(SliderValue v, float min, float max);

void SliderValueSetStart(SliderValue* v, float value);
void SliderValueSetEnd(SliderValue* v, float value);

enum class SliderScale : uint8_t {
    Linear,
    Logarithmic
};

enum class SliderEventKind : uint8_t {
    Change,
    Release
};

struct SliderEvent {
    SliderEventKind kind = SliderEventKind::Change;
    SliderValue value = {};
};

struct SliderState {
    float min = 0;
    float max = 100;
    float step = 1;
    SliderValue value = {};

    float pctLo = 0;
    float pctHi = 0;

    Bounds bounds = {};
    SliderScale scale = SliderScale::Linear;

    bool dragging = false;

    bool dragStart = false;
    Listener onChange = {};
};

SliderState SliderStateNew(float min, float max, SliderValue value,
                           float step = 1,
                           SliderScale scale = SliderScale::Linear);

void SliderSetLimits(SliderState* s, float min, float max);

void SliderSetStep(SliderState* s, float step);

void SliderSetScale(SliderState* s, SliderScale scale);

void SliderSetValue(SliderState* s, SliderValue v);

inline void SliderSetBounds(SliderState* s, Bounds b) {
    s->bounds = b;
}

float SliderPctToValue(const SliderState* s, float pct);
float SliderValueToPct(const SliderState* s, float value);

void SliderUpdateThumbPos(SliderState* s);

bool SliderUpdateByPosition(SliderState* s, Axis axis, Point pos, bool isStart);

bool SliderIsStartAt(const SliderState* s, Axis axis, Point pos);

bool SliderHandleRelease(SliderState* s);

bool SliderStepBy(SliderState* s, int dir, bool isStart);

struct Overlay {
    int kind = 0;
    char title[128] = {};
    char body[2048] = {};
};

struct MenuState {
    bool open = false;
    float x = 0, y = 0;
    char items[8][32] = {};
    int nItems = 0;
    int clickBase = 0;
};

struct WinSize {
    float dipW = 0;
    float dipH = 0;
    int pxW = 0;
    int pxH = 0;
};

float PxToDip(PaintCtx* ctx, int px);
int DipToPx(PaintCtx* ctx, float dip);

Size MeasureText(PaintCtx* ctx, Str s, float fontSize, float maxW,
                 bool wrap = false, int weight = 0, float lineH = 0);

void DrawTextBaseline(PaintCtx* ctx, Str s, float x, float baselineY,
                      float fontSize, Rgba color, int weight = 0);
void TextMeasBeginFrame(PaintCtx* ctx);
void TextMeasEndFrame(PaintCtx* ctx);
void TextMeasClear(PaintCtx* ctx);

bool TextPointAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                 int off, float* outX, float* outY, float* outH,
                 uint16_t font = 0, float lineHeight = 0,
                 bool lineEndAffinity = true);
int TextIndexAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                float relX, float relY, uint16_t font = 0, float lineHeight = 0,
                TextAlign align = TextAlign::Left);

void PaintTextRange(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                    uint16_t weight, float lineH, float x, float y, int u8a,
                    int u8b, Rgba color, TextAlign align = TextAlign::Left);

struct TextGlyphBox {
    int lo = 0;
    int hi = 0;
    int row = 0;
    float left = 0;
    float right = 0;
    float top = 0;
    float h = 0;
};

int TextGlyphBoxes(TextLayout* layout, Str text, Vec<TextGlyphBox>* out);

float TextAlignedRowLeft(TextAlign align, float alignWidth, float width);

int TextRangeBoxes(const TextGlyphBox* glyphs, int n, int lo, int hi,
                   Vec<TextGlyphBox>* out);

void PaintTextRangeBoxes(PaintCtx* ctx, Str s, float fontSize, float maxW,
                         bool wrap, uint16_t weight, float lineH, float x,
                         float y, int lo, int hi, Rgba color,
                         TextAlign align = TextAlign::Left);
void PaintTextUnderline(PaintCtx* ctx, Str s, float fontSize, float maxW,
                        bool wrap, uint16_t weight, float lineH, float x,
                        float y, int u8a, int u8b, Rgba color,
                        bool wavy = false, TextAlign align = TextAlign::Left,
                        float thickness = 1.f);

struct LayoutCache;

bool LayoutReuseTakeArg(Str arg);

bool LayoutReuseOn();

LayoutCache* LayoutCacheNew();
void LayoutCacheFree(LayoutCache* lc);

struct LayoutCacheStats {

    int nodes = 0;

    int made = 0;
    int dropped = 0;

    int restyled = 0;
    int remeasured = 0;

    int allocs = 0;
};

LayoutCacheStats LayoutCacheLastStats(const LayoutCache* lc);

int LayoutCacheNodeCount(const LayoutCache* lc);

int LayoutCacheSlotCount(const LayoutCache* lc);

void LayoutScratchFree();

void LayoutEl(PaintCtx* ctx, El* e, float x, float y, float availW,
              float availH, float inheritFont, Rgba inheritFg,
              LayoutCache* lc = nullptr);

Size MeasureEl(PaintCtx* ctx, El* e, float inheritFont = 0,
               Rgba inheritFg = {});

Size MeasureElAtWidth(PaintCtx* ctx, El* e, float width);

bool LayoutInScratchPass();
void PaintEl(PaintCtx* ctx, El* e);

int ElTextRangeRects(PaintCtx* ctx, const El* e, int lo, int hi, Bounds* out,
                     int cap);

float ElTextSpaceWidth(PaintCtx* ctx, const El* e);
int HitTest(PaintCtx* ctx, float x, float y);
const HitRect* HitTestRect(PaintCtx* ctx, float x, float y);

const ScrollRect* WindowLastScrollRect(const Window* win, int id);

const HitRect* HitTestDrop(PaintCtx* ctx, float x, float y, Str kind);
const ScrollRect* HitScrollRect(PaintCtx* ctx, float x, float y);
int TextHitOffsetAt(PaintCtx* ctx, float x, float y, bool nearest);

int TextHitOffsetIn(PaintCtx* ctx, float x, float y, bool nearest, int scope,
                    int* outScope, int minLayer = 0);
int CopyTextHits(PaintCtx* ctx, int selA, int selB, char* out, int cap);

int CopyTextHitsIn(PaintCtx* ctx, int selA, int selB, int scope, char* out,
                   int cap, SelectionFormat fmt = SelectionFormat::Plain);
int CopyTextHitsInEntity(PaintCtx* ctx, int selA, int selB, int scope,
                         EntityId owner, char* out, int cap,
                         SelectionFormat fmt = SelectionFormat::Plain);

bool TextMultiClickRange(PaintCtx* ctx, float x, float y, int clickCount,
                         int* outA, int* outB);
bool TextMultiClickRangeIn(PaintCtx* ctx, float x, float y, int clickCount,
                           int scope, int* outA, int* outB, int* outScope);
int HashClickId(Str s);

enum class BoxFill : uint8_t {
    Base,
    Hover,
    Active
};
BoxFill BoxFillFor(bool hasActiveBg, bool hasHoverBg, int clickId, int activeId,
                   int hoverId);

bool ClickFromRelease(bool pending, int pressedId, MouseButton pressedButton,
                      bool dragged, int upId, MouseButton upButton);

bool ClickFromKeyRelease(bool pending, int pendingGen, int focusGen, int key,
                         bool modified);

enum : int8_t {
    ClickWinMin = -1,
    ClickWinMax = -2,
    ClickWinClose = -3,
    ClickWinCaption = -4,
};

struct App;
struct Window;

struct Tiling {
    bool top = false;
    bool bottom = false;
    bool left = false;
    bool right = false;

    bool IsTiled() const { return top || bottom || left || right; }
    bool AllTiled() const { return top && bottom && left && right; }
};

struct WinOpts {
    bool borderless = false;

    bool clientTitleBar = false;
    bool anim = false;
    int timerMs = 500;
};

struct FrameTiming {

    double drawAt = 0;
    float drawSecs = 0;

    uint64_t invalidations = 0;

    double presentAt = -1;
};

enum : uint16_t {
    kFrameTraceCap = 256
};

struct EntitySub {
    int id = 0;
    EntityId emitter = {};
    const void* eventType = nullptr;
    Listener handler = {};
};

using AppGlobalFreeFn = void (*)(void* value);

struct AppGlobalSlot {
    const void* key = nullptr;
    void* value = nullptr;
    AppGlobalFreeFn freeValue = nullptr;
};

void* AppGlobalGetRaw(const App* app, const void* key);
void AppGlobalSetRaw(App* app, const void* key, void* value,
                     AppGlobalFreeFn freeValue);
bool AppGlobalRemoveRaw(App* app, const void* key);
void AppGlobalClear(App* app);

template <typename T>
const void* AppGlobalKey() {
    static const uint8_t key = 0;
    return &key;
}

template <typename T>
T* AppGlobalGet(const App* app) {
    return (T*)AppGlobalGetRaw(app, AppGlobalKey<T>());
}

template <typename T>
void AppGlobalDelete(void* value) {
    delete (T*)value;
}

template <typename T>
T* AppGlobalEnsure(App* app) {
    T* value = AppGlobalGet<T>(app);
    if (value || !app) {
        return value;
    }
    value = new T();
    AppGlobalSetRaw(app, AppGlobalKey<T>(), value, &AppGlobalDelete<T>);
    return value;
}

template <typename T>
bool AppGlobalRemove(App* app) {
    return AppGlobalRemoveRaw(app, AppGlobalKey<T>());
}

struct App {
    PaintApp* paint = nullptr;
    Vec<Window*> windows;

    void (*hostThemeHandler)(App*, bool dark) = nullptr;

    Vec<EntitySlot> entities;
    Vec<int32_t> freeSlots;

    Vec<EntitySub> subs;

    Vec<EntitySub> observers;

    Vec<AppGlobalSlot> globals;
    int nextSubId = 1;
    int exitCode = 0;

    ImageStore* images = nullptr;

    App() = default;
};

struct PlatWindow;

struct WindowSelection;

struct WindowBlurSub {
    int id = 0;
    int focusId = 0;
    Listener handler = {};
};

struct Window {
    App* app = nullptr;
    PlatWindow* plat = nullptr;
    PaintCtx paint = {};
    Arena* frameArena = nullptr;

    LayoutCache* layout = nullptr;

    WindowSelection* sel = nullptr;

    EntityId root = {};

    Vec<EntityId> rendered;

    struct DebugViewRoot {
        const El* el = nullptr;
        EntityId view = {};
    };
    Vec<DebugViewRoot> debugViewRoots;
    Vec<uint32_t> debugHoveredPaths;

    bool debugLabelShown = false;
    bool hasDebugSource = false;
    Bounds debugSourceBounds = {};
    const char* debugSourceFile = nullptr;
    int debugSourceLine = 0;
    int debugSourceColumn = 0;

    int debugSourcePrints = 0;

    Modifiers modifiers = {};

    Vec<EntityId> imageCacheStack;

    Vec<uint32_t> plotAppearScopes;

    Vec<uint64_t> textSelectionScopes;

    bool hasAutoscroll = false;
    Bounds autoscroll = {};

    Vec<ScrollRect> prevScrolls;

    Vec<AccessibilityNode> accessibility;

    uint64_t accessibilityHash = 0;

    int hoverId = 0;

    bool lastInputKeyboard = false;

    float remSize = 16;
    int focusId = 0;

    int focusGen = 0;

    Vec<WindowBlurSub> blurSubs;
    int nextBlurSubId = 1;
    float mouseX = 0;
    float mouseY = 0;

    Modifiers mouseModifiers = {};

    CursorKind cursor = CursorKind::Arrow;
    bool cursorHidden = false;
    bool maximized = false;

    float clientInset = -1;
    Tiling tiling = {};

    float resizeHitSize = 4;

    bool active = true;
    bool running = true;
    WindowShouldCloseFn shouldClose = nullptr;
    void* shouldCloseData = nullptr;
    bool anim = false;

    bool animFrame = false;

    double frameNow = 0;

    double lastDrawTime = 0;
    bool mouseDown = false;

    bool stopPropagation = false;

    bool defaultPrevented = false;

    bool pressTookFocus = false;

    int scrollLockHorizontalId = 0;
    int scrollLockVerticalId = 0;
    OngoingScroll scrollLockHorizontal = {};
    OngoingScroll scrollLockVertical = {};

    double lastDownAt = 0;
    float lastDownX = 0;
    float lastDownY = 0;
    MouseButton lastDownButton = MouseButton::Left;
    int clickRun = 0;

    int pressedId = 0;

    int pressedCount = 1;

    bool pressPending = false;

    float pressedX = 0;
    float pressedY = 0;
    bool pressedMoved = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers pressedModifiers = {};

    DragPayload activeDrag = {};
    int dragOverId = 0;

    float dragOffX = 0;
    float dragOffY = 0;
    bool eatReturn = false;

    bool eatChar = false;

    bool keyPressPending = false;
    int keyPressGen = 0;

    bool eatSysChar = false;

    int scrollDragId = 0;
    float scrollDragGrab = 0;

    bool scrollDragHorizontal = false;

    InputState* scrollDragInput = nullptr;
    bool touchScrollbarDrag = false;

    bool touchInputHandle = false;

    bool touchPressPending = false;
    bool touchPress = false;

    bool scrollDragNotifyPending = false;
    double scrollDragLastNotify = 0;
    double scrollDragNotifyDue = 0;
    Listener scrollDragNotifyListener = {};
    ScrollEvent scrollDragNotifyEvent = {};
    bool longPressSelection = false;

    InputState* longPressInput = nullptr;

    TouchHostKind touchHost = TouchHostKind::None;

    SliderState* touchSlider = nullptr;
    Axis touchSliderAxis = Axis::Horizontal;
    bool touchSliderStart = false;
    Point touchHostStart = {};
    Point touchHostLast = {};
    double touchHostStartAt = 0;
    InputState* input = nullptr;

    EntityId tooltip = {};

    EntityId rootTooltip = {};
    Overlay overlay = {};
    InspectorState inspector = {};
    MenuState menu = {};
    Vec<FocusRect> focusEls;

    int pendingTrap = 0;

    int previousTrap = 0;

    int pendingTrapHost = 0;
    Vec<KeyedSlot> keyed;

    Vec<DispatchNode> dispatch;
    Vec<MotionSlotRec> motionSlots;
    WinOpts opts = {};

    Listener onKey = {};
    Listener onClick = {};
    Listener onMouseDown = {};
    Listener onMouseUp = {};
    Listener onMouseMove = {};
    Listener onMouseExit = {};
    Listener onScrollWheel = {};

    Vec<TimerSub> timers;
    int nextTimerId = 1;

    InputState* prevInput = nullptr;

    FrameTiming frameTrace[kFrameTraceCap] = {};
    uint64_t frameSeq = 0;

    uint64_t invalidations = 0;

    Window() = default;

    ~Window();
};

struct Ctx {
    App* app = nullptr;
    Window* win = nullptr;
    Arena* a = nullptr;
    EntityId self = {};

    uint32_t path = 0;
};

uint32_t IdFoldName(uint32_t parent, Str name);

struct IdScope {
    Ctx* cx = nullptr;
    uint32_t prev = 0;

    IdScope(Ctx* c, Str name) : cx(c), prev(c ? c->path : 0) {
        if (cx) {
            cx->path = IdFoldName(prev, name);
        }
    }
    ~IdScope() {
        if (cx) {
            cx->path = prev;
        }
    }
    IdScope(const IdScope&) = delete;
    IdScope& operator=(const IdScope&) = delete;
};

EntityId EntityNewRaw(App* app, void* ptr, RenderFn render, DropFn drop);
void* EntityGet(App* app, EntityId id);
void EntityDrop(App* app, EntityId id);
void EntityDropAll(App* app);

template <typename T>
struct Entity {
    EntityId id = {};

    bool IsValid() const { return id.IsValid(); }
    T* Get(App* app) const { return (T*)EntityGet(app, id); }
    T* Get(Ctx* cx) const { return (T*)EntityGet(cx->app, id); }
};

template <typename T>
void EntityDropT(void* p) {
    delete (T*)p;
}

template <typename T>
Entity<T> EntityNew(App* app) {
    Entity<T> e;
    e.id = EntityNewRaw(app, new T(), (RenderFn)&T::Render, &EntityDropT<T>);
    return e;
}

template <typename T>
Entity<T> EntityNew(Ctx* cx) {
    return EntityNew<T>(cx->app);
}

template <typename T>
Entity<T> EntityNewState(App* app) {
    Entity<T> e;
    e.id = EntityNewRaw(app, new T(), nullptr, &EntityDropT<T>);
    return e;
}

template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*)) {
    Listener l;
    l.SetFn(fn);
    l.view = cx->self;
    return l;
}

template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*, int64_t), int64_t arg) {
    Listener l;
    l.SetFn(fn);
    l.view = cx->self;
    l.arg = arg;
    l.SetArgBound();
    return l;
}

template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*, int64_t)) {
    Listener l;
    l.SetFn(fn);
    l.view = cx->self;
    l.SetHasArg();
    return l;
}

inline Listener ListenerArg(Listener l, int64_t arg) {
    if (l.IsValid()) {
        l.arg = arg;
        l.SetArgBound();
    }
    return l;
}

inline Listener ListenerFill(Listener l, int64_t v) {
    if (l.IsValid() && !l.ArgBound()) {
        l.arg = v;
        l.SetHasArg();
    }
    return l;
}

template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*)) {
    Listener l;
    l.SetFn(fn);
    l.view = e.id;
    return l;
}

template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*, int64_t)) {
    Listener l;
    l.SetFn(fn);
    l.view = e.id;
    l.SetHasArg();
    return l;
}

template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*, int64_t),
                  int64_t arg) {
    Listener l;
    l.SetFn(fn);
    l.view = e.id;
    l.arg = arg;
    l.SetArgBound();
    return l;
}

template <typename T, typename E>
struct EventEmitter;

template <typename T, typename E>
concept EmitsEvent = requires {
    sizeof(EventEmitter<T, E>);
};

struct DismissEvent {};

template <typename E>
const void* EntityEventType() {
    static const uint8_t key = 0;
    return &key;
}

struct Subscription {
    int id = 0;

    bool IsValid() const { return id != 0; }
};

Subscription EntitySubscribeRaw(App* app, EntityId emitter,
                                const void* eventType, Listener handler);
void EntityUnsubscribe(App* app, Subscription sub);

Subscription EntityObserveRaw(App* app, EntityId observed, Listener handler);
void EntityUnobserve(App* app, Subscription sub);
int EntityObserverCount(App* app, EntityId observed);

void NotifyEntity(App* app, EntityId id, Window* from);

void EntityEmitRaw(App* app, Window* win, EntityId emitter,
                   const void* eventType, const void* ev);

int EntitySubscriberCount(App* app, EntityId emitter);

template <typename T, typename S, typename E>
requires EmitsEvent<T, E> Subscription
Subscribe(Ctx* cx, Entity<T> emitter, void (*fn)(S*, Ctx*, const E*)) {
    Listener l;
    l.SetFn(fn);
    l.view = cx->self;
    return EntitySubscribeRaw(cx->app, emitter.id, EntityEventType<E>(), l);
}

template <typename T, typename S>
Subscription Observe(Ctx* cx, Entity<T> observed,
                     void (*handler)(S*, Ctx*, const EntityId*)) {
    Listener l = Listen(cx, handler);
    return EntityObserveRaw(cx->app, observed.id, l);
}

template <typename T, typename S>
Subscription ObserveTo(App* app, Entity<T> observed, Entity<S> observer,
                       void (*handler)(S*, Ctx*, const EntityId*)) {
    Listener l = ListenTo(observer, handler);
    return EntityObserveRaw(app, observed.id, l);
}

template <typename T, typename S, typename E>
requires EmitsEvent<T, E> Subscription SubscribeTo(App* app, Entity<T> emitter,
                                                   Entity<S> subscriber,
                                                   void (*fn)(S*, Ctx*,
                                                              const E*)) {
    Listener l;
    l.SetFn(fn);
    l.view = subscriber.id;
    return EntitySubscribeRaw(app, emitter.id, EntityEventType<E>(), l);
}

template <typename T, typename S, typename E>
requires EmitsEvent<T, E> Subscription
SubscribeTo(App* app, Entity<T> emitter, Entity<S> subscriber,
            void (*fn)(S*, Ctx*, const E*, int64_t), int64_t arg) {
    Listener l = ListenTo(subscriber, fn, arg);
    return EntitySubscribeRaw(app, emitter.id, EntityEventType<E>(), l);
}

template <typename T, typename E>
requires EmitsEvent<T, E> void EntityEmit(App* app, Window* win,
                                          Entity<T> emitter, const E* ev) {
    EntityEmitRaw(app, win, emitter.id, EntityEventType<E>(), ev);
}

template <typename T, typename E>
requires EmitsEvent<T, E> void Emit(Ctx* cx, Entity<T> emitter, const E* ev) {
    EntityEmit(cx->app, cx->win, emitter, ev);
}

void Notify(Ctx* cx);
void NotifyApp(App* app);
void ListenerCall(App* app, Window* win, const Listener& l, const void* ev);

El* EntityRender(App* app, Window* win, Arena* a, EntityId id);

void* WindowKeyedState(Window* win, uint32_t key, void* fresh, DropFn drop);
void WindowKeyedFree(Window* win);

void* WindowFrameKeyedState(Window* win, uint32_t key, void* fresh,
                            DropFn drop);
void WindowKeyedSweep(Window* win);

void* WindowMotionState(Window* win, uint32_t key, int size);

void WindowMotionSweep(Window* win);

uint32_t* WindowPlotAppearScopeToken(Window* win, uint32_t key);
uint32_t WindowPlotAppearScope(const Window* win);
void WindowMotionFree(Window* win);

template <typename T>
T* KeyedState(Ctx* cx, uint32_t key) {
    void* p = WindowKeyedState(cx->win, key, new T(), &EntityDropT<T>);
    return (T*)p;
}

template <typename T>
T* FrameKeyedState(Ctx* cx, uint32_t key) {
    void* p = WindowFrameKeyedState(cx->win, key, new T(), &EntityDropT<T>);
    return (T*)p;
}

EntityId WindowKeyedEntity(Window* win, App* app, uint32_t key, void* fresh,
                           DropFn drop);

inline uint32_t KeyedKey(uint32_t name, uint32_t kind) {
    uint32_t h = name * 2654435761u;
    h ^= kind + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h ? h : 1u;
}

inline uint32_t KeyedName(Ctx* cx, Str name) {
    return IdFoldName(cx ? cx->path : 0, name);
}

template <typename T>
Entity<T> KeyedEntity(Ctx* cx, uint32_t key) {
    Entity<T> e;
    e.id = WindowKeyedEntity(cx->win, cx->app, key, new T(), &EntityDropT<T>);
    return e;
}

template <typename T>
T* ElementState(Ctx* cx, Str name, Str kind) {
    return KeyedState<T>(cx, KeyedKey(KeyedName(cx, name), HashClickId(kind)));
}

template <typename T>
Entity<T> ElementStateEntity(Ctx* cx, Str name, Str kind) {
    return KeyedEntity<T>(cx, KeyedKey(KeyedName(cx, name), HashClickId(kind)));
}

struct LaidOutHeight {

    float measured = -1;
    float built = -1;

    int chase = 0;

    float inset = 0;
    bool contentBox = false;

    bool (*rebuild)(void* user, Ctx* cx, El* box, float height) = nullptr;
    void* rebuildUser = nullptr;
    Ctx cx = {};
};

LaidOutHeight* UseLaidOutHeight(Ctx* cx, Str name, float fallback);

bool TrackLaidOutHeight(Ctx* cx, El* e, LaidOutHeight* slot);

bool TrackLaidOutHeight(Ctx* cx, El* e, LaidOutHeight* slot,
                        bool (*rebuild)(void* user, Ctx* cx, El* box,
                                        float height),
                        void* user);

bool LaidOutHeightObserve(LaidOutHeight* slot, float boxHeight);

void WindowOnKey(Window* win, Listener l);

void WindowOnUnhandledClick(Window* win, Listener l);

void WindowToggleInspector(Window* win);
void WindowInspectorPick(Window* win, bool picking);
const InspectorState* WindowInspector(Ctx* cx);

const DragPayload* WindowActiveDrag(Ctx* cx);

bool WindowIsActive(Ctx* cx);

bool WindowIsTouchPress(Ctx* cx);

void WindowSetActive(Window* win, bool active);

int WindowDragOverId(Ctx* cx);

void WindowStopPropagation(Ctx* cx);

void WindowPreventDefault(Ctx* cx);
bool WindowDefaultPrevented(const Window* win);

Point WindowDragOffset(Ctx* cx);

void WindowOnMouseDown(Window* win, Listener l);
void WindowOnMouseUp(Window* win, Listener l);
void WindowOnMouseMove(Window* win, Listener l);
void WindowOnMouseExit(Window* win, Listener l);
void WindowOnScrollWheel(Window* win, Listener l);

void WindowPost(Window* win, Listener l, const void* ev = nullptr);

int WindowSetInterval(Window* win, int ms, Listener l);

int WindowSetTimeout(Window* win, int ms, Listener l);
void WindowCancelTimer(Window* win, int id);

struct BlinkCursor {
    bool visible = false;
    bool paused = false;

    int timer = 0;

    static void OnFlip(BlinkCursor* self, Ctx* cx, const TickEvent* ev);
    static void OnResume(BlinkCursor* self, Ctx* cx, const TickEvent* ev);
};

void BlinkStart(App* app, Window* win, EntityId* handle);
void BlinkStop(App* app, Window* win, EntityId* handle);

void BlinkPause(App* app, Window* win, EntityId* handle);

bool BlinkVisible(App* app, EntityId handle);

inline void BlinkStart(Ctx* cx, EntityId* handle) {
    BlinkStart(cx->app, cx->win, handle);
}
inline void BlinkStop(Ctx* cx, EntityId* handle) {
    BlinkStop(cx->app, cx->win, handle);
}
inline void BlinkPause(Ctx* cx, EntityId* handle) {
    BlinkPause(cx->app, cx->win, handle);
}
inline bool BlinkVisible(Ctx* cx, EntityId handle) {
    return BlinkVisible(cx->app, handle);
}

inline void InputFocus(InputState* s, Ctx* cx) {
    InputFocus(s, cx->app, cx->win);
}
inline void InputBlur(InputState* s, Ctx* cx) {
    InputBlur(s, cx->app, cx->win);
}
inline void InputMoveTo(InputState* s, Ctx* cx, int offset) {
    InputMoveTo(s, cx->app, cx->win, offset);
}
inline void InputSelectAll(InputState* s, Ctx* cx) {
    InputSelectAll(s, cx->app, cx->win);
}
inline void InputClean(InputState* s, Ctx* cx) {
    InputClean(s, cx->app, cx->win);
}
inline void InputReplaceAll(InputState* s, Ctx* cx, Str value) {
    InputReplaceAll(s, cx->app, cx->win, value);
}
inline void InputInsert(InputState* s, Ctx* cx, Str value) {
    InputInsert(s, cx->app, cx->win, value);
}
inline bool InputPerform(InputState* s, Ctx* cx, InputAction action,
                         bool shift = false) {
    return InputPerform(s, cx->app, cx->win, action, shift);
}

Window* WindowOpenView(App* app, Str title, int dipW, int dipH, EntityId root,
                       WinOpts opts);
int AppRunView(Str title, int dipW, int dipH, EntityId root, App* app,
               WinOpts opts);

template <typename T>
T* WindowRoot(Window* win) {
    return win ? (T*)EntityGet(win->app, win->root) : nullptr;
}

WinSize WindowSize(Window* win);

int WindowCollectFrames(Window* win, uint64_t* cursor, FrameTiming* out,
                        int max);

double TimeNow();

bool TestPlatformNow(double* out);
bool TestPlatformClipboardWrite(Str text);
bool TestPlatformClipboardRead(Arena* a, ClipboardItem* out);

App* AppNew();
void AppFree(App* app);

void ClipboardSetText(Window* win, Str text);

ClipboardItem ClipboardGetItem(Arena* a, Window* win);

Str ClipboardGetText(Arena* a, Window* win);

using ClipboardReadFn = void (*)(void* data, App* app, Window* win,
                                 const ClipboardItem& item);
bool ClipboardReadAsync(Window* win, ClipboardReadFn done, void* data);

void WindowSetTextContentType(Window* win, Str value);

void OpenUrl(Str url);

struct PathPrompt {

    bool files = true;
    bool directories = false;

    Str title = {};
};

TempStr PromptForPathTemp(Window* win, const PathPrompt& opts);

int AppRun(App* app);
Window* WindowOpen(App* app, Str title, int dipW, int dipH, WinOpts opts);
void AppSetTitle(Window* win, Str title);
void AppRequestAnim(Window* win, bool on);

void WindowRequestAnimationFrame(Window* win);

float WindowRemSize(const Window* win);
void WindowSetRemSize(Window* win, float size);

inline float Rems(const Ctx* cx, float r) {
    return r * WindowRemSize(cx ? cx->win : nullptr);
}

inline float FontPx(const Ctx* cx, float px) {
    return px * 16.f / WindowRemSize(cx ? cx->win : nullptr);
}

void WindowRequestAutoscroll(Window* win, Bounds bounds);
bool WindowTakeAutoscroll(Window* win, Bounds* out);

bool WindowLastInputWasKeyboard(const Window* win);

void FocusCollect(Window* win, El* root);
void IdsCollect(El* root);

void IdsCollectChildren(El* e);
void AccessibilityCollect(El* root, Vec<AccessibilityNode>* out);
const AccessibilityNode* WindowAccessibilityNode(const Window* win,
                                                 uint32_t nodeId);
bool WindowAccessibilityPerform(Window* win, uint32_t nodeId,
                                AccessibilityAction action, Str value = {});

bool WindowAccessibilitySetNumericValue(Window* win, uint32_t nodeId,
                                        float value);
int FocusNext(Window* win, int trapId, bool backward);

int FocusNextFrom(const Window* win, int fromId, int trapId, bool backward);

FocusHandle FocusHandleNew(App* app);
FocusHandle FocusHandleNew(Ctx* cx);

bool FocusHandleIsFocused(const Window* win, FocusHandle h);

bool FocusHandleContains(const Window* win, FocusHandle outer,
                         FocusHandle inner);

bool FocusHandleContainsFocused(const Window* win, FocusHandle h);
void FocusHandleFocus(Window* win, FocusHandle h);
FocusHandle WindowFocused(const Window* win);

bool FocusHandleRestore(Window* win, FocusHandle h);

Subscription WindowOnBlur(Window* win, FocusHandle h, Listener handler);
void WindowUnsubscribeBlur(Window* win, Subscription sub);

void WindowSetFocusId(Window* win, int id);

int WindowFocusedId(const Window* win);

bool WindowFocusContains(const Window* win, int outer, int inner);

bool WindowFocusWithin(const Window* win, int id);

bool WindowRestoreFocus(Window* win, int id);

bool WindowBindingForActionAtFocus(Window* win, uint32_t action,
                                   FocusHandle focus, KeyChord* out);

bool WindowDispatchKeyAction(Window* win, int vk, bool shift, bool ctrl,
                             bool alt, bool platform = false,
                             bool function = false);

uint32_t WindowResolveKeyAction(Window* win, int vk, bool shift, bool ctrl,
                                bool alt, bool platform, bool function,
                                int64_t* arg, bool* pending);

inline bool KeySecondary(bool ctrl, bool platform) {
    return PlatSecondaryIsCommand() ? platform : ctrl;
}

bool WindowDispatchAction(Window* win, uint32_t action, int64_t arg = 0);

bool WindowDispatchActionAtFocus(Window* win, FocusHandle focus,
                                 uint32_t action, int64_t arg = 0);

bool WindowDispatchKeyEvent(Window* win, KeyEvent* ev);

bool WindowDispatchKeyUpEvent(Window* win, KeyEvent* ev);

bool WindowDispatchKeyCaptureEvent(Window* win, KeyEvent* ev);

using ActionFn = void (*)(Window* win, ActionEvent* ev);
void AppOnAction(uint32_t action, ActionFn fn);
void AppQuit(Window* win);

void AppQuitAll(App* app);
void AppInvalidate(Window* win);

void AppRefreshWindows(App* app);

void AppSetHostThemeHandler(App* app, void (*handler)(App*, bool dark));
void AppHostSetTheme(App* app, bool dark);

void AppOnShutdown(void (*fn)());

bool WindowClientDecorated(Window* win);

struct MenuRow {
    Str label = {};

    uint32_t action = 0;
    int64_t arg = 0;
    bool separator = false;
    bool disabled = false;
    bool checked = false;
    const MenuRow* submenu = nullptr;
    int submenuN = 0;

    const char* stroke = nullptr;
};

struct MenuDef {
    Str name = {};
    const MenuRow* items = nullptr;
    int n = 0;

    bool disabled = false;
};

bool AppHasMenuBar();

void AppSetMenus(App* app, const MenuDef* menus, int n);

void AppSetMenuBarVisible(bool visible);

bool AppMenuRowForId(int id, uint32_t* action, int64_t* arg);
bool AppMenuRowForId(const App* app, int id, uint32_t* action, int64_t* arg);

void AppMenuClear(App* app);

void AppActivate(Window* win);
void AppMinimize(Window* win);
void AppToggleMaximize(Window* win);

void WindowSetFullScreen(Window* win, bool fullScreen);

void WindowSetCursorVisible(Window* win, bool visible);

void WindowOnShouldClose(Window* win, WindowShouldCloseFn fn, void* data);
void AppClose(Window* win);
void AppDrag(Window* win);
bool AppIsMaximized(Window* win);
}

int GpuiMain(int argc, char** argv);

#line 1 "src/gpui/assets.h"

namespace gpui {

void AssetsClear();

int AssetsRootCount();
void AssetsAddRoot(Str dir);

using AssetLoadFn = bool (*)(void* user, Str relPath, Vec<uint8_t>* out);
using AssetExistsFn = bool (*)(void* user, Str relPath);
int AssetsAddSource(void* user, AssetLoadFn load, AssetExistsFn exists);
void AssetsRemoveSource(int id);

void AssetsAddDefaultRoots(Str exampleName);
bool AssetsLoad(Str relPath, Vec<uint8_t>* out);
TempStr AssetsLoadTextTemp(Str relPath);

bool AssetsFindDir(Str relDir, char* out, int cap);
bool AssetsExists(Str relPath);
}

#line 1 "src/gpui/drawops.h"

namespace gpui {

enum DrawOp : uint8_t {
    kOpEnd = 0,

    kOpViewBox = 1,

    kOpStrokeWidth = 2,

    kOpColor = 3,
    kOpColorReset = 4,

    kOpLine = 5,
    kOpRect = 6,
    kOpFillRect = 7,
    kOpEllipse = 8,
    kOpFillEllipse = 9,

    kOpArc = 10,

    kOpMoveTo = 11,
    kOpLineTo = 12,
    kOpCubicTo = 13,
    kOpClosePath = 14,
    kOpFillPath = 15,
    kOpStrokePath = 16,

    kOpFillStrokePath = 17,

    kOpText = 18,
};

enum DrawOpTextFlag : uint8_t {
    kTextAnchorMask = 3,
    kTextAnchorStart = 0,
    kTextAnchorMiddle = 1,
    kTextAnchorEnd = 2,
    kTextBold = 4,
};

struct DrawOpsTarget {
    float x = 0;
    float y = 0;
    float w = 16;
    float h = 16;

    Rgba color = {};
    float turns = 0;
    bool grayscale = false;
};

bool ExecuteDrawOps(PaintCtx* ctx, const void* data, int dataLen,
                    const DrawOpsTarget& t);

bool DrawOpsViewBox(const void* data, int dataLen, Size* out);

struct DrawOpsBuilder {
    Vec<uint8_t> data;

    void Op(DrawOp op);

    void F2(float a, float b);
    void U32(uint32_t v);

    void ViewBox(float x, float y, float w, float h);
    void StrokeWidth(float w);
    void Color(Rgba c);
    void ColorReset();
    void Line(float x1, float y1, float x2, float y2);
    void MoveTo(float x, float y);
    void LineTo(float x, float y);
    void CubicTo(float x1, float y1, float x2, float y2, float x, float y);
    void ClosePath();

    void Text(float x, float y, float size, float textLength, uint32_t flags,
              Str s);
    void End();
};

}

#line 1 "src/gpui/svg.h"

namespace gpui {

bool SvgViewBox(Str assetPath, Size* out);

bool SvgDraw(PaintCtx* ctx, Str assetPath, float x, float y, float size,
             Rgba color, float turns = 0);

bool SvgDrawXml(PaintCtx* ctx, Str xml, float x, float y, float size,
                Rgba color, float turns = 0);

bool SvgDrawOps(PaintCtx* ctx, const uint8_t* ops, int len, float x, float y,
                float w, float h, Rgba color, float turns = 0,
                bool grayscale = false);

bool SvgRasterize(PaintApp* pa, Str assetPath, int px, Rgba color,
                  uint8_t* outBgra);
bool SvgRasterizeXml(PaintApp* pa, Str xml, int px, Rgba color,
                     uint8_t* outBgra);

bool SvgToDrawOps(Str xml, DrawOpsBuilder* out);

const uint8_t* SvgDrawOpsFor(Str assetPath, int* lenOut);

const uint8_t* SvgDrawOpsForXml(Str xml, int* lenOut);

void SvgCacheClear();

Str IconNamePath(IconName name);
}

#line 1 "src/base/element_ext.h"

namespace gpui {

inline El* UiRoot(Arena* a, Str id, int clickId = 0) {
    El* e = Div(a)->Id(id);
    if (clickId) {
        e->Click(clickId)->FocusId(clickId);
    }
    return e;
}
}

#line 1 "src/base/accordion.h"

namespace gpui {

struct Accordion {
    static El* New(Ctx* cx, Str id);
};

struct AccordionTrigger {
    static El* New(Ctx* cx, Str id, bool open = false, bool disabled = false,
                   Listener onChange = {});
};

struct AccordionHeader {
    static El* New(Ctx* cx, El* trigger, Str id = {}, int level = 3);
};

struct AccordionPanel {

    static El* New(Ctx* cx, Str id = {});
};

struct AccordionItem {
    El* root = nullptr;
    bool open = false;
    bool keepMounted = false;

    static AccordionItem* New(Ctx* cx);
    AccordionItem* Open(bool v);
    AccordionItem* KeepMounted(bool v);
    AccordionItem* Header(El* header);
    AccordionItem* Panel(El* panel);
    El* IntoEl();
};
}

#line 1 "src/base/actions.h"

namespace gpui {

namespace action {

uint32_t Confirm();

constexpr int64_t kConfirmSecondary = 1;
uint32_t Cancel();
uint32_t SelectUp();
uint32_t SelectDown();
uint32_t SelectLeft();
uint32_t SelectRight();
uint32_t SelectFirst();
uint32_t SelectLast();
uint32_t SelectPrevColumn();
uint32_t SelectNextColumn();
uint32_t SelectPageUp();
uint32_t SelectPageDown();

}

struct CancelKeys {
    Listener onCancel = {};

    static void OnAction(CancelKeys* self, Ctx* cx, const ActionEvent* ev);
};

void CancelInitKeys(const char* context);

void CancelBindKeys(Ctx* cx, El* root, const char* context, Str name,
                    Listener onCancel);

}

#line 1 "src/base/dialog.h"

namespace gpui {

enum class DialogChangeReason : uint8_t {
    TriggerPress,
    BackdropPress,
    Cancel,
    Confirm,
    Imperative
};

struct DialogOpenChangeEvent {
    bool open = false;
    DialogChangeReason reason = DialogChangeReason::Imperative;
};

struct DialogHandleState {
    EntityId self = {};
    bool open = false;
    Listener onOpenChange = {};
};

struct DialogHandle {
    Entity<DialogHandleState> state = {};

    static DialogHandle New(Ctx* cx, bool open = false);
    bool IsValid() const { return state.IsValid(); }
    bool IsOpen(App* app, bool fallback = false) const;
    void OnOpenChange(App* app, Listener listener) const;
    bool SetOpen(Ctx* cx, bool open, DialogChangeReason reason) const;
    bool Open(Ctx* cx) const;
    bool Close(Ctx* cx) const;
};

enum class DialogAction : uint8_t {
    None,
    Cancel,
    Confirm
};

void DialogInitKeys();
Str DialogContext();
DialogAction DialogActionOf(uint32_t id);

struct DialogKeys {

    Listener onCancel = {};
    Listener onOk = {};

    Listener onClose = {};

    static void OnAction(DialogKeys* self, Ctx* cx, const ActionEvent* ev);
};

void DialogBindKeys(Ctx* cx, El* popup, Str name, Listener onCancel,
                    Listener onOk, Listener onClose);

bool DialogBackdropCloses(bool overlayClosable, bool topmost,
                          MouseButton button, float pressY,
                          float dismissBelowY);

struct DialogBackdrop {
    static El* New(Ctx* cx);
};
struct DialogPopup {
    static El* New(Ctx* cx);
};
struct DialogTitle {
    static El* New(Ctx* cx);
};
struct DialogDescription {
    static El* New(Ctx* cx);
};
struct DialogClose {
    static El* New(Ctx* cx, int clickId = 0);

    static El* WithTrigger(Ctx* cx, El* trigger, int clickId = 0);
};

El* DialogCloseActivation(El* button);

struct DialogTrigger {
    static El* New(Ctx* cx, Listener onOpen = {}, DialogHandle handle = {},
                   Str id = StrL("dialog-trigger"));
};

struct Dialog {
    Ctx* cx = nullptr;
    El* root = nullptr;

    Str trap = {};
    bool open = true;
    DialogHandle handle = {};

    static Dialog* New(Ctx* cx);
    Dialog* Open(bool value);
    Dialog* Handle(DialogHandle value);
    Dialog* Trap(Str name);
    Dialog* Backdrop(El* backdrop);
    Dialog* Popup(El* popup);
    El* IntoEl();
};
}

#line 1 "src/base/alert_dialog.h"

namespace gpui {

const bool kAlertDialogClosesOnBackdropPress = false;

struct AlertDialogBackdrop {
    static El* New(Ctx* cx);
};
struct AlertDialogPopup {
    static El* New(Ctx* cx);
};
struct AlertDialogTitle {
    static El* New(Ctx* cx);
};
struct AlertDialogDescription {
    static El* New(Ctx* cx);
};

struct AlertDialogCancel {
    static El* New(Ctx* cx);
};
struct AlertDialogAction {
    static El* New(Ctx* cx);
};
struct AlertDialogTrigger {
    static El* New(Ctx* cx, Listener onOpen = {}, DialogHandle handle = {});
};

struct AlertDialog {
    Ctx* cx = nullptr;
    El* root = nullptr;

    Str trap = {};
    bool open = true;
    DialogHandle handle = {};

    static AlertDialog* New(Ctx* cx);
    AlertDialog* Open(bool value);
    AlertDialog* Handle(DialogHandle value);
    AlertDialog* Trap(Str name);
    AlertDialog* Backdrop(El* backdrop);
    AlertDialog* Popup(El* popup);
    El* IntoEl();
};
}

#line 1 "src/base/animation.h"

namespace gpui {

float CubicBezier(float x1, float y1, float x2, float y2, float t);

using EaseFn = float (*)(float);

float EaseLinear(float t);

float EaseOutCubic(float t);

float EaseInCubic(float t);

float EaseInOutCubic(float t);

float EaseQuadratic(float t);

float EaseInOutQuad(float t);
float EaseOutQuint(float t);

float EaseBounce(EaseFn e, float t);

float EaseBounceInOut(float t);

float ClampF01(float t);

float Lerp(float a, float b, float t);
Point Lerp(Point a, Point b, float t);

Rgba Lerp(Rgba a, Rgba b, float t);

struct EffectTransition {
    float durationMs = 0;

    static EffectTransition* New(Ctx* cx, float durationMs);
    EffectTransition* Ease(EaseFn fn);
    EffectTransition* SlideY(float from, float to);
    EffectTransition* SlideX(float from, float to);
    EffectTransition* Fade(float from, float to);
    EffectTransition* Width(float from, float to);
    EffectTransition* Height(float from, float to);
    El* Apply(El* element, Str id);

  private:

    enum class EffectKind : uint8_t {
        SlideY,
        SlideX,
        Fade,
        Width,
        Height
    };
    struct Effect {
        EffectKind kind = EffectKind::Fade;
        float from = 0;
        float to = 0;
    };

    Ctx* cx = nullptr;
    EaseFn easing = EaseOutCubic;
    ArenaVec<Effect> effects;

    EffectTransition* Add(EffectKind kind, float from, float to);
};

using Transition = EffectTransition;

}

#line 1 "src/base/auto_scroll.h"

namespace gpui {

bool AutoScrollComputeDelta(float y, Bounds bounds, float* out);

}

#line 1 "src/base/avatar.h"

namespace gpui {

struct Avatar {
    El* root = nullptr;
    El* image = nullptr;
    El* fallback = nullptr;

    static Avatar* New(Ctx* cx);
    Avatar* Size(float px);
    Avatar* Image(El* image);
    Avatar* Fallback(El* fallback);
    El* IntoEl();
};

struct AvatarImage {
    static El* New(Ctx* cx);
};
struct AvatarFallback {
    static El* New(Ctx* cx);
};
}

#line 1 "src/base/state_style.h"

namespace gpui {

enum StateField : uint32_t {
    StateFieldBg = StyleFieldBg,
    StateFieldFg = StyleFieldColor,

    StateFieldBorder = StyleFieldBorder | StyleFieldBorderColor,

    StateFieldBorderL = StyleFieldBorderL | StyleFieldBorderColor,
    StateFieldBorderR = StyleFieldBorderR | StyleFieldBorderColor,
    StateFieldBorderT = StyleFieldBorderT | StyleFieldBorderColor,
    StateFieldBorderB = StyleFieldBorderB | StyleFieldBorderColor,
    StateFieldRadius = StyleFieldRadius,
    StateFieldHoverBg = StyleFieldHoverBg,
    StateFieldHoverFg = StyleFieldHoverFg,
    StateFieldActiveBg = StyleFieldActiveBg,

    StateFieldOpacity = StyleFieldOpacity,
    StateFieldPad = StyleFieldPad,
};

struct StateStyle {
    Style style = {};
    uint32_t set = 0;

    StateStyle& Bg(Background c);
    StateStyle& Fg(Rgba c);
    StateStyle& Border(float w, Rgba c);
    StateStyle& BorderL(float w, Rgba c);
    StateStyle& BorderR(float w, Rgba c);
    StateStyle& BorderT(float w, Rgba c);
    StateStyle& BorderB(float w, Rgba c);
    StateStyle& Radius(float v);
    StateStyle& HoverBg(Background c);
    StateStyle& HoverFg(Rgba c);
    StateStyle& ActiveBg(Background c);
    StateStyle& Opacity(float v);

    StateStyle& Pad(float x, float y);

    bool Has(StateField f) const {
        uint32_t bits = (uint32_t)f;
        return (set & bits) == bits;
    }
};

void StateStyleRefine(StateStyle* into, const StateStyle& over);

StateStyle StateStyleResolve(const StateStyle& instance,
                             const StateStyle* const* states, int n);

El* ElRefine(El* e, const StateStyle& s);

}

#line 1 "src/base/button.h"

namespace gpui {

struct ButtonStyles {
    StateStyle selected = {};
    StateStyle disabled = {};
};

struct Button {
    static El* New(Ctx* cx, Str id, bool disabled = false,
                   Listener onClick = {}, bool focusable = true,
                   const ButtonStyles* styles = nullptr, bool selected = false);
};
}

#line 1 "src/base/calendar.h"

namespace gpui {

enum class CalendarView : uint8_t {
    Day,
    Month,
    Year
};

enum class DateKind : uint8_t {
    Single,
    Range
};

struct Date {
    DateKind kind = DateKind::Single;
    LocalDate start = {};
    LocalDate end = {};

    static Date Single(LocalDate value = {});
    static Date Range(LocalDate start = {}, LocalDate end = {});
    bool IsSome() const;
    bool IsComplete() const;
    bool IsSingle() const { return kind == DateKind::Single; }
    bool IsActive(LocalDate value) const;
    bool IsInRange(LocalDate value) const;
};

struct IntervalMatcher {
    LocalDate before = {};
    LocalDate after = {};
};

struct RangeMatcher {
    LocalDate from = {};
    LocalDate to = {};
};

enum class MatcherKind : uint8_t {
    None,
    DayOfWeek,

    Weekdays = DayOfWeek,
    Interval,
    Range,
    Custom
};

struct Matcher {
    MatcherKind kind = MatcherKind::None;
    uint8_t weekdayMask = 0;
    IntervalMatcher interval = {};
    RangeMatcher range = {};

    LocalDate from = {};
    LocalDate to = {};
    bool (*custom)(LocalDate date) = nullptr;
};

using DateMatcher = Matcher;
using DateMatcherKind = MatcherKind;

Matcher DateMatcherWeekdays(uint8_t weekdayMask);
Matcher DateMatcherInterval(LocalDate before, LocalDate after);
Matcher DateMatcherRange(LocalDate from, LocalDate to);
Matcher DateMatcherCustom(bool (*fn)(LocalDate date));
bool DateMatcherMatches(const Matcher& matcher, LocalDate date);
bool MatcherMatches(const Matcher& matcher, Date date);

enum class CalendarEventKind : uint8_t {
    Selected
};

struct CalendarEvent {
    CalendarEventKind kind = CalendarEventKind::Selected;
    Date date = {};
};

struct CalendarState {
    Entity<CalendarState> self = {};
    FocusHandle focus = {};
    CalendarView view = CalendarView::Day;
    Date date = {};
    int currentYear = 0;

    int currentMonth = 1;
    int numberOfMonths = 1;

    int yearPage = 0;
    int yearPageCount = 0;
    int yearMin = 0;
    int yearMax = 0;
    LocalDate today = {};
    Matcher disabledMatcher = {};

    static void OnDate(CalendarState* self, Ctx* cx, const ClickEvent* ev,
                       int64_t dateKey);
    static void OnPrev(CalendarState* self, Ctx* cx, const ClickEvent* ev);
    static void OnNext(CalendarState* self, Ctx* cx, const ClickEvent* ev);
    static void OnMonthToggle(CalendarState* self, Ctx* cx,
                              const ClickEvent* ev);
    static void OnYearToggle(CalendarState* self, Ctx* cx,
                             const ClickEvent* ev);
    static void OnMonth(CalendarState* self, Ctx* cx, const ClickEvent* ev,
                        int64_t month);
    static void OnYear(CalendarState* self, Ctx* cx, const ClickEvent* ev,
                       int64_t year);
};

void CalendarStateInit(CalendarState* s, Ctx* cx, Date date = Date::Single());
Entity<CalendarState> CalendarStateNew(Ctx* cx, Date date = Date::Single());
bool CalendarStateApplyDate(CalendarState* s, Date date);
void CalendarStateSetDate(CalendarState* s, Date date, Ctx* cx,
                          bool emit = false);
bool CalendarStateSelectDate(CalendarState* s, LocalDate value, Ctx* cx,
                             bool emit = true);
void CalendarStateSetDisabledMatcher(CalendarState* s, Matcher matcher,
                                     Ctx* cx = nullptr);
void CalendarStateSetYearRange(CalendarState* s, int minYear, int maxYear,
                               Ctx* cx = nullptr);

void CalendarPrevMonth(CalendarState* s);
void CalendarNextMonth(CalendarState* s);

bool CalendarHasPrevYearPage(const CalendarState* s);
bool CalendarHasNextYearPage(const CalendarState* s);
bool CalendarPrevYearPage(CalendarState* s);
bool CalendarNextYearPage(CalendarState* s);

int CalendarGridOffset(int firstWeekday);

int CalendarGridCells(int offset, int daysInMonth);

enum class CalendarItemKind : uint8_t {
    Previous,
    MonthToggle,
    YearToggle,
    Next,
    Weekday,
    Day,
    Month,
    Year
};

struct CalendarItemState {
    CalendarItemKind kind = CalendarItemKind::Day;

    int value = 0;

    LocalDate date = {};
    bool active = false;
    bool inRange = false;
    bool muted = false;
    bool disabled = false;
    bool today = false;
};

using CalendarItemFn = El* (*)(void* user, Ctx* cx, El* item,
                               const CalendarItemState& st);
using CalendarLabelFn = Str (*)(void* user, Ctx* cx, CalendarItemKind kind,
                                int value);

struct CalendarOpts {
    int year = 0;
    int month = 1;
    int numberOfMonths = 1;
    CalendarView view = CalendarView::Day;

    float cellSize = 32;

    LocalDate selected = {};
    LocalDate rangeEnd = {};

    LocalDate today = {};
    DateMatcher disabledMatcher = {};

    int firstDayOfWeek = 0;

    int yearMin = 0;
    int yearMax = 0;
    int yearPageStart = 0;
    Listener onDay = {};
    Listener onDate = {};
    Listener onPrev = {};
    Listener onNext = {};
    Listener onMonthToggle = {};
    Listener onYearToggle = {};
    Listener onMonth = {};
    Listener onYear = {};
    CalendarItemFn item = nullptr;
    void* user = nullptr;
    CalendarLabelFn label = nullptr;
    void* labelUser = nullptr;
};

struct Calendar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<CalendarState> state = {};
    CalendarOpts opts = {};
    Style style = {};
    uint32_t styleSet = 0;

    static Calendar* New(Ctx* cx, Str id, Entity<CalendarState> state);
    Calendar* NumberOfMonths(int count);
    Calendar* FirstDayOfWeek(int weekday);
    Calendar* Item(CalendarItemFn fn, void* user = nullptr);
    Calendar* Label(CalendarLabelFn fn, void* user = nullptr);
    Calendar* Refine(const Style& v, uint32_t fields);
    El* IntoEl();

    static El* New(Ctx* cx, Str id);

    static El* New(Ctx* cx, Str id, const CalendarOpts& o);
};

void CalendarOffsetMonth(int year, int month, int offset, int* outYear,
                         int* outMonth);

int CalendarWeekday(int year, int month, int day);

int CalendarDaysInMonth(int year, int month);

struct CalendarItem {
    static El* New(Ctx* cx, Str id = {}, Listener onClick = {});
};

template <>
struct EventEmitter<CalendarState, CalendarEvent> {};
}

#line 1 "src/base/checkbox.h"

namespace gpui {

enum class CheckboxState : uint8_t {
    Unchecked,
    Checked,
    Indeterminate
};

CheckboxState CheckboxActivated(CheckboxState state);

struct CheckboxStyles {
    StateStyle checked = {};
    StateStyle indeterminate = {};
    StateStyle disabled = {};

    CheckboxStyles& Checked(const StateStyle& style);
    CheckboxStyles& Indeterminate(const StateStyle& style);
    CheckboxStyles& Disabled(const StateStyle& style);
};

struct CheckboxIndicatorStyles {
    StateStyle checked = {};
    StateStyle indeterminate = {};
    StateStyle disabled = {};

    CheckboxIndicatorStyles& Checked(const StateStyle& style);
    CheckboxIndicatorStyles& Indeterminate(const StateStyle& style);
    CheckboxIndicatorStyles& Disabled(const StateStyle& style);
};

struct Checkbox {
    static El* New(Ctx* cx, Str id,
                   CheckboxState state = CheckboxState::Unchecked,
                   bool disabled = false, Listener onChange = {},
                   const CheckboxStyles* styles = nullptr,
                   const StateStyle* instance = nullptr,
                   Str accessibilityLabel = {}, int tabIndex = 0,
                   bool tabStop = true, FocusHandle focus = {},
                   AccessibilityRole role = AccessibilityRole::CheckBox);
};

struct CheckboxIndicator {
    static El* New(Ctx* cx, CheckboxState state = CheckboxState::Unchecked,
                   bool disabled = false,
                   const CheckboxIndicatorStyles* styles = nullptr,
                   const StateStyle* instance = nullptr);
};
}

#line 1 "src/base/motion.h"

namespace gpui {

template <typename T, typename E>
struct MotionResult {
    bool ok = false;
    T value = {};
    E error = {};

    bool IsOk() const { return ok; }
    bool IsErr() const { return !ok; }

    const T& Unwrap() const { return value; }

    E UnwrapErr() const { return error; }
};

enum class StepPosition : uint8_t {
    JumpStart,
    JumpEnd,
    JumpNone,
    JumpBoth,
};

struct LinearStop {
    float output = 0;
    float input = 0;
    bool hasInput = false;

    static LinearStop New(float output) {
        LinearStop s;
        s.output = output;
        return s;
    }
    static LinearStop At(float output, float input) {
        LinearStop s;
        s.output = output;
        s.input = input;
        s.hasInput = true;
        return s;
    }
};

enum class EasingError : uint8_t {

    InvalidBezierX,
    InvalidBezierControlPoint,
    InvalidStepCount,
    InvalidLinearStops,
};

const char* EasingErrorMessage(EasingError e);

enum class EasingKind : uint8_t {
    Linear,
    Ease,
    EaseIn,
    EaseOut,
    EaseInOut,
    CubicBezier,
    Steps,
    LinearStops,
    Custom,
};

struct Easing;
using EasingResult = MotionResult<Easing, EasingError>;

struct Easing {
    EasingKind kind = EasingKind::EaseOut;

    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    uint32_t count = 1;
    StepPosition position = StepPosition::JumpEnd;

    const float* stops = nullptr;
    int32_t stopsLen = 0;

    EaseFn custom = nullptr;

    static Easing Linear() { return Of(EasingKind::Linear); }
    static Easing Ease() { return Of(EasingKind::Ease); }
    static Easing EaseIn() { return Of(EasingKind::EaseIn); }
    static Easing EaseOut() { return Of(EasingKind::EaseOut); }
    static Easing EaseInOut() { return Of(EasingKind::EaseInOut); }
    static Easing Custom(EaseFn fn) {
        Easing e = Of(EasingKind::Custom);
        e.custom = fn;
        return e;
    }

    static EasingResult CubicBezier(float x1, float y1, float x2, float y2);

    static EasingResult Steps(uint32_t count, StepPosition position);

    static EasingResult LinearStops(Arena* a, const LinearStop* stops,
                                    int32_t len);

    float Sample(float progress) const;

  private:
    static Easing Of(EasingKind k) {
        Easing e;
        e.kind = k;
        return e;
    }
};

struct SignedDuration {
    float ms = 0;
    bool negative = false;

    static SignedDuration Zero() { return {}; }
    static SignedDuration Positive(float ms) { return {ms, false}; }
    static SignedDuration Negative(float ms) { return {ms, true}; }

    bool ActiveElapsed(float elapsedMs, float* out) const;

    bool operator==(const SignedDuration& o) const {
        return ms == o.ms && negative == o.negative;
    }
    bool operator!=(const SignedDuration& o) const { return !(*this == o); }
};

struct IterationCount {
    bool infinite = false;
    uint64_t count = 1;

    static IterationCount Finite(uint64_t n) { return {false, n}; }
    static IterationCount Infinite() { return {true, 0}; }
};

enum class PlaybackDirection : uint8_t {
    Normal,
    Reverse,
    Alternate,
    AlternateReverse,
};

enum class MotionPhase : uint8_t {
    Before,
    Active,
    After,
};

struct TimingSample {
    MotionPhase phase = MotionPhase::Before;
    float directedProgress = 0;
    uint64_t iteration = 0;
    bool active = false;
    bool finished = false;
};

struct Timing {
    SignedDuration delay = {};
    float durationMs = 0;
    IterationCount iterations = IterationCount::Finite(1);
    PlaybackDirection direction = PlaybackDirection::Normal;
    Easing easing = Easing::Linear();

    static Timing New(float durationMs) {
        Timing t;
        t.durationMs = durationMs;
        return t;
    }
    Timing Delay(SignedDuration d) const {
        Timing t = *this;
        t.delay = d;
        return t;
    }
    Timing Delay(float ms) const { return Delay(SignedDuration::Positive(ms)); }
    Timing Iterations(IterationCount n) const {
        Timing t = *this;
        t.iterations = n;
        return t;
    }
    Timing Direction(PlaybackDirection d) const {
        Timing t = *this;
        t.direction = d;
        return t;
    }
    Timing Ease(Easing e) const {
        Timing t = *this;
        t.easing = e;
        return t;
    }

    TimingSample Sample(float elapsedMs) const;

  private:
    TimingSample AfterSample(uint64_t count) const;
    float Directed(uint64_t iteration, float progress) const;
};

struct StaggerOrigin {
    enum Kind : uint8_t {
        First,
        Last,
        Center,
        Index
    };
    Kind kind = First;
    int32_t index = 0;

    static StaggerOrigin FirstOrigin() { return {First, 0}; }
    static StaggerOrigin LastOrigin() { return {Last, 0}; }
    static StaggerOrigin CenterOrigin() { return {Center, 0}; }
    static StaggerOrigin IndexOrigin(int32_t ix) { return {Index, ix}; }
};

struct Stagger {
    float intervalMs = 0;
    StaggerOrigin origin = {};

    static Stagger New(float intervalMs, StaggerOrigin origin) {
        return {intervalMs, origin};
    }
    float Delay(int32_t index, int32_t count) const;
};

constexpr float kDefaultSpringEpsilon = 0.001f;

struct MotionTransform {
    Point translation = {0, 0};
    Point scale = {1, 1};
    float rotationRadians = 0;
    float opacity = 1;

    static MotionTransform Identity() { return {}; }
};

namespace motion {

template <typename T>
struct Interpolate {
    static T Between(const T& from, const T& target, float progress) {
        return Lerp(from, target, progress);
    }
};

template <>
struct Interpolate<Size> {
    static Size Between(const Size& from, const Size& to, float p) {
        return {Lerp(from.w, to.w, p), Lerp(from.h, to.h, p)};
    }
};

template <>
struct Interpolate<Bounds> {
    static Bounds Between(const Bounds& from, const Bounds& to, float p) {
        return {Lerp(from.x, to.x, p), Lerp(from.y, to.y, p),
                Lerp(from.w, to.w, p), Lerp(from.h, to.h, p)};
    }
};

template <>
struct Interpolate<MotionTransform> {
    static MotionTransform Between(const MotionTransform& from,
                                   const MotionTransform& to, float p) {
        MotionTransform out;
        out.translation = Lerp(from.translation, to.translation, p);
        out.scale = {Lerp(from.scale.x, to.scale.x, p),
                     Lerp(from.scale.y, to.scale.y, p)};
        out.rotationRadians = Lerp(from.rotationRadians, to.rotationRadians, p);
        out.opacity = Lerp(from.opacity, to.opacity, p);
        return out;
    }
};

struct Transition {
    float durationMs = 0;
    float delayMs = 0;
    Easing easing = Easing::Custom(EaseOutCubic);

    static Transition New(float durationMs) {
        Transition policy;
        policy.durationMs = durationMs;
        return policy;
    }

    Transition Delay(float ms) const {
        Transition policy = *this;
        policy.delayMs = ms;
        return policy;
    }

    Transition Delay(SignedDuration d) const {
        return Delay(d.negative ? -d.ms : d.ms);
    }

    Transition Ease(EaseFn fn) const {
        Transition policy = *this;
        policy.easing = Easing::Custom(fn);
        return policy;
    }

    Transition Ease(Easing e) const {
        Transition policy = *this;
        policy.easing = e;
        return policy;
    }

    SignedDuration Delay() const {
        return delayMs < 0 ? SignedDuration::Negative(-delayMs)
                           : SignedDuration::Positive(delayMs);
    }
};

struct TransitionId {
    uint32_t key = 0;

    TransitionId() = default;
    explicit TransitionId(uint32_t value) : key(value) {}
    explicit TransitionId(Str id);
    TransitionId(Str id, Str channel);

    bool operator==(const TransitionId& other) const {
        return key == other.key;
    }
    bool operator!=(const TransitionId& other) const {
        return key != other.key;
    }
};

}

using Motion = motion::Transition;

inline Motion MotionNew(float durationMs) {
    return motion::Transition::New(durationMs);
}

uint32_t MotionId(Str id);
uint32_t MotionId(Str id, Str channel);

uint32_t MotionName(Ctx* cx, Str name);

enum class MotionStatus : uint8_t {
    Idle,
    Delayed,
    Running,
    Finished,
};

float MotionProgress(const Motion& m, float elapsedMs, float durationMs,
                     MotionStatus* status);
inline float MotionProgress(const Motion& m, float elapsedMs) {
    MotionStatus status;
    return MotionProgress(m, elapsedMs, m.durationMs, &status);
}

float MotionSample(const Motion& m, float progress);

bool MotionReduced();

void MotionSetReduced(bool on);

void MotionResetReduceForTest();

template <typename T>
struct MotionState {
    T from = {};
    T target = {};
    double startedAt = 0;
    float reversingFactor = 1.f;
    float durationMs = 0;
    bool init = false;
};

template <typename T>
struct MotionStep {
    T value = {};

    bool running = false;
    MotionStatus status = MotionStatus::Idle;
};

inline bool MotionEq(float a, float b) {
    return a == b;
}
inline bool MotionEq(Point a, Point b) {
    return a.x == b.x && a.y == b.y;
}
inline bool MotionEq(Size a, Size b) {
    return a.w == b.w && a.h == b.h;
}
inline bool MotionEq(Bounds a, Bounds b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
inline bool MotionEq(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
inline bool MotionEq(const MotionTransform& a, const MotionTransform& b) {
    return MotionEq(a.translation, b.translation) &&
           MotionEq(a.scale, b.scale) &&
           a.rotationRadians == b.rotationRadians && a.opacity == b.opacity;
}

double MotionNow(Ctx* cx);
void* MotionSlot(Ctx* cx, uint32_t key, int size);
void MotionWantsFrame(Ctx* cx);

template <typename T>
struct SequenceStep {
    T target = {};
    motion::Transition transition = motion::Transition::New(0);

    static SequenceStep New(T value, const motion::Transition& policy) {
        SequenceStep step;
        step.target = value;
        step.transition = policy;
        return step;
    }
    const T& Target() const { return target; }
    const motion::Transition& Transition() const { return transition; }
};

template <typename T>
struct SequenceSample {
    T value = {};
    int32_t step = 0;
    MotionStatus status = MotionStatus::Idle;

    const T& Value() const { return value; }
    T IntoValue() const { return value; }
    int32_t Step() const { return step; }
    MotionStatus Status() const { return status; }
    bool IsFinished() const { return status == MotionStatus::Finished; }
    bool IsActive() const {
        return status == MotionStatus::Delayed ||
               status == MotionStatus::Running;
    }
};

template <typename T>
struct SequenceState {
    int32_t step = 0;
    T from = {};
    T target = {};
    motion::Transition transition = motion::Transition::New(0);
    double startedAt = 0;
    bool init = false;
};

template <typename T>
struct Sequence {
    Arena* arena = nullptr;
    motion::TransitionId id = {};
    T from = {};
    ArenaVec<SequenceStep<T>> steps;

    static Sequence New(Arena* a, motion::TransitionId valueId, T value) {
        Sequence out;
        out.arena = a;
        out.id = valueId;
        out.from = value;
        return out;
    }
    static Sequence New(Ctx* cx, motion::TransitionId valueId, T value) {
        return New(cx ? cx->a : nullptr, valueId, value);
    }
    Sequence& WithStep(T target, const motion::Transition& policy) {
        steps.Append(arena, SequenceStep<T>::New(target, policy));
        return *this;
    }
    Sequence& WithSteps(const SequenceStep<T>* values, int32_t count) {
        for (int32_t i = 0; i < count; i++) steps.Append(arena, values[i]);
        return *this;
    }
    const T& From() const { return from; }
    const ArenaVec<SequenceStep<T>>& Steps() const { return steps; }

    SequenceSample<T> Sample(Ctx* cx) const {
        SequenceSample<T> out;
        out.value = from;
        if (!cx || len(steps) == 0) return out;

        const int32_t last = len(steps) - 1;
        uint32_t stateKey = id.key ^ 0x9e3779b9u;
        auto* state = (SequenceState<T>*)MotionSlot(
            cx, stateKey, (int)sizeof(SequenceState<T>));
        if (!state) {
            out.value = steps[last].target;
            out.step = last;
            out.status = MotionStatus::Finished;
            return out;
        }
        const double now = MotionNow(cx);
        auto start = [&](int32_t at, T startValue, double startedAt) {
            state->step = at;
            state->from = startValue;
            state->target = steps[at].target;
            state->transition = steps[at].transition;
            state->startedAt = startedAt;
            state->init = true;
        };
        if (!state->init) start(0, from, now);

        if (MotionReduced()) {
            start(last, steps[last].target, now);
            out.value = steps[last].target;
            out.step = last;
            out.status = MotionStatus::Finished;
            return out;
        }

        auto sample = [&](MotionStatus* status) {
            float elapsed =
                (float)(std::max(0.0, now - state->startedAt) * 1000.0);
            float progress =
                MotionProgress(state->transition, elapsed,
                               state->transition.durationMs, status);
            return motion::Interpolate<T>::Between(
                state->from, state->target,
                MotionSample(state->transition, progress));
        };

        MotionStatus status;
        T value = sample(&status);
        bool retargeted = state->step > last ||
                          !MotionEq(state->target, steps[state->step].target);
        if (retargeted) {
            start(0, value, now);
            value = sample(&status);
        } else {
            while (status == MotionStatus::Finished && state->step < last) {
                float finishMs =
                    state->transition.durationMs + state->transition.delayMs;
                if (finishMs < 0) finishMs = 0;
                double nextStart = state->startedAt + finishMs / 1000.0;
                T previousTarget = state->target;
                start(state->step + 1, previousTarget, nextStart);
                value = sample(&status);
            }
        }
        out.value = value;
        out.step = state->step;
        out.status = status;
        if (out.IsActive()) MotionWantsFrame(cx);
        return out;
    }
};

template <typename T>
MotionStep<T> MotionAdvance(MotionState<T>* st, T target, const Motion& m,
                            double now, bool reduced) {
    MotionStep<T> out;
    if (!st->init) {
        st->init = true;
        st->from = target;
        st->target = target;
        st->startedAt = now;
        st->reversingFactor = 1.f;
        st->durationMs = m.durationMs;
    }
    if (reduced || m.durationMs <= 0) {
        if (!MotionEq(st->from, target) || !MotionEq(st->target, target)) {
            st->from = target;
            st->target = target;
            st->startedAt = now;
            st->reversingFactor = 1.f;
            st->durationMs = m.durationMs;
        }
        out.value = target;
        out.status = MotionStatus::Finished;
        return out;
    }
    double elapsedS = now - st->startedAt;
    float elapsedMs = elapsedS > 0 ? (float)(elapsedS * 1000.0) : 0.f;
    MotionStatus status;
    float progress = MotionProgress(m, elapsedMs, st->durationMs, &status);
    float eased = MotionSample(m, progress);
    T sampled = motion::Interpolate<T>::Between(st->from, st->target, eased);
    if (!MotionEq(st->target, target)) {

        bool reversing = MotionEq(target, st->from);
        float factor = 1.f;
        if (reversing) {
            factor = eased * st->reversingFactor + (1.f - st->reversingFactor);
            factor = ClampF01(factor);
        }
        float duration = m.durationMs * factor;
        st->from = sampled;
        st->target = target;
        st->startedAt = now;
        st->reversingFactor = factor;
        st->durationMs = duration;
        MotionStatus initialStatus;
        float initial = MotionProgress(m, 0.f, duration, &initialStatus);
        out.value = motion::Interpolate<T>::Between(sampled, target,
                                                    MotionSample(m, initial));
        out.status = initialStatus;
    } else {
        out.value = sampled;
        out.status =
            MotionEq(st->from, st->target) ? MotionStatus::Idle : status;
    }
    out.running = out.status == MotionStatus::Delayed ||
                  out.status == MotionStatus::Running;
    return out;
}

float MotionRepeat(Ctx* cx, uint32_t key, float periodMs,
                   EaseFn ease = nullptr);

float MotionAppear(Ctx* cx, uint32_t key, float durationMs,
                   EaseFn ease = nullptr);

template <typename T>
void MotionSeed(Ctx* cx, uint32_t key, T from) {
    auto* st =
        (MotionState<T>*)MotionSlot(cx, key, (int)sizeof(MotionState<T>));
    if (!st) {
        return;
    }
    st->init = true;
    st->from = from;
    st->target = from;
    st->startedAt = MotionNow(cx);
    st->reversingFactor = 1.f;
    st->durationMs = 0;
}

enum class SpringError : uint8_t {
    InvalidDamping,
    InvalidEpsilon,
};

const char* SpringErrorMessage(SpringError e);

struct Spring;
using SpringResult = MotionResult<Spring, SpringError>;

struct Spring {

    float responseMs = 0;

    float damping = 1.f;

    float epsilon = kDefaultSpringEpsilon;

    bool travel = true;

    static Spring New(float responseMs) {
        Spring s;
        s.responseMs = responseMs;
        return s;
    }

    Spring WithDamping(float ratio) const;
    SpringResult TryWithDamping(float ratio) const;
    Spring WithTravel(bool v) const {
        Spring s = *this;
        s.travel = v;
        return s;
    }

    Spring WithEpsilon(float eps) const;
    SpringResult TryWithEpsilon(float eps) const;
    float Epsilon() const { return epsilon; }
};

inline Spring SpringNew(float responseMs) {
    return Spring::New(responseMs);
}

struct SpringState {
    float position = 0;
    float velocity = 0;
    float target = 0;
    double updatedAt = 0;
    bool init = false;
};

struct SpringStep {
    float value = 0;
    bool running = false;
};

SpringStep SpringAdvance(SpringState* st, float target, const Spring& s,
                         double now, bool reduced);

float SpringValue(Ctx* cx, uint32_t key, float target, const Spring& s);

inline void SpringSeed(Ctx* cx, uint32_t key, float from) {
    auto* st = (SpringState*)MotionSlot(cx, key, (int)sizeof(SpringState));
    if (!st) {
        return;
    }
    st->init = true;
    st->position = from;
    st->velocity = 0;
    st->target = from;
    st->updatedAt = MotionNow(cx);
}

template <typename T>
MotionStep<T> MotionValueWithStatus(Ctx* cx, uint32_t key, T target,
                                    const Motion& m) {
    auto* st =
        (MotionState<T>*)MotionSlot(cx, key, (int)sizeof(MotionState<T>));
    if (!st) {
        MotionStep<T> out;
        out.value = target;
        out.status = MotionStatus::Finished;
        return out;
    }
    MotionStep<T> step =
        MotionAdvance(st, target, m, MotionNow(cx), MotionReduced());
    if (step.running) {
        MotionWantsFrame(cx);
    }
    return step;
}

template <typename T>
T MotionValue(Ctx* cx, uint32_t key, T target, const Motion& m) {
    return MotionValueWithStatus(cx, key, target, m).value;
}

template <typename T>
struct Keyframe {
    float offset = 0;
    T value = {};
    Easing easing = Easing::Linear();

    static Keyframe New(float offset, T value) {
        Keyframe k;
        k.offset = offset;
        k.value = value;
        return k;
    }
    Keyframe Ease(Easing e) const {
        Keyframe k = *this;
        k.easing = e;
        return k;
    }
};

enum class KeyframeError : uint8_t {
    TooFewFrames,
    OffsetNotFinite,
    OffsetOutOfRange,
    OffsetsNotMonotonic,
    MissingEndpoint,
};

template <typename T>
struct Keyframes {
    const Keyframe<T>* frames = nullptr;
    int32_t len = 0;

    static MotionResult<Keyframes<T>, KeyframeError> TryNew(
        const Keyframe<T>* frames, int32_t len) {
        MotionResult<Keyframes<T>, KeyframeError> r;
        if (len < 2) {
            r.error = KeyframeError::TooFewFrames;
            return r;
        }
        for (int32_t i = 0; i < len; i++) {
            if (!IsFiniteF(frames[i].offset)) {
                r.error = KeyframeError::OffsetNotFinite;
                return r;
            }
        }
        for (int32_t i = 0; i < len; i++) {
            if (frames[i].offset < 0.f || frames[i].offset > 1.f) {
                r.error = KeyframeError::OffsetOutOfRange;
                return r;
            }
        }
        for (int32_t i = 0; i + 1 < len; i++) {
            if (frames[i].offset > frames[i + 1].offset) {
                r.error = KeyframeError::OffsetsNotMonotonic;
                return r;
            }
        }
        if (frames[0].offset != 0.f || frames[len - 1].offset != 1.f) {
            r.error = KeyframeError::MissingEndpoint;
            return r;
        }
        r.ok = true;
        r.value.frames = frames;
        r.value.len = len;
        return r;
    }

    T Sample(float progress) const {
        progress = ClampF01(progress);

        int32_t upper = 0;
        while (upper < len && frames[upper].offset <= progress) {
            upper++;
        }
        if (upper == 0) {
            return frames[0].value;
        }
        if (upper == len) {
            return frames[len - 1].value;
        }
        const Keyframe<T>& from = frames[upper - 1];
        const Keyframe<T>& to = frames[upper];
        if (from.offset == to.offset) {
            return to.value;
        }
        float segment = (progress - from.offset) / (to.offset - from.offset);
        return motion::Interpolate<T>::Between(from.value, to.value,
                                               from.easing.Sample(segment));
    }

    int32_t Len() const { return len; }
    bool IsEmpty() const { return len == 0; }

  private:
    static bool IsFiniteF(float v) { return v - v == 0.f; }
};

enum class DiscreteError : uint8_t {
    InvalidSwitchPoint,
};

template <typename T>
struct Discrete {
    T from = {};
    T to = {};
    float switchAt = 0.5f;

    static Discrete New(T from, T to) {
        Discrete d;
        d.from = from;
        d.to = to;
        return d;
    }

    MotionResult<Discrete<T>, DiscreteError> SwitchAt(float progress) const {
        MotionResult<Discrete<T>, DiscreteError> r;
        if (!(progress - progress == 0.f) || progress < 0.f || progress > 1.f) {
            r.error = DiscreteError::InvalidSwitchPoint;
            return r;
        }
        r.ok = true;
        r.value = *this;
        r.value.switchAt = progress;
        return r;
    }

    T Sample(float progress) const { return progress < switchAt ? from : to; }
};

struct KeyframePlayback {
    double startedAt = 0;
    bool init = false;
};

TimingSample AnimateKeyframesSample(Ctx* cx, uint32_t key, const Timing& timing,
                                    MotionStatus* status, bool* reduced);

template <typename T>
MotionStep<T> AnimateKeyframes(Ctx* cx, uint32_t key,
                               const Keyframes<T>& keyframes,
                               const Timing& timing) {
    MotionStep<T> out;
    bool reduced = false;
    TimingSample sample =
        AnimateKeyframesSample(cx, key, timing, &out.status, &reduced);
    if (reduced) {
        out.value = keyframes.Sample(1.f);
        return out;
    }
    out.value = keyframes.Sample(sample.directedProgress);
    out.running = out.status == MotionStatus::Delayed ||
                  out.status == MotionStatus::Running;
    return out;
}

enum class PresencePhase : uint8_t {
    Entering,
    Present,
    Exiting,
    Absent,
};

struct PresenceSample {
    PresencePhase phase = PresencePhase::Absent;
    float progress = 0;
    MotionStatus status = MotionStatus::Finished;

    bool ShouldRender() const { return phase != PresencePhase::Absent; }
};

using PresenceState = MotionState<float>;

PresenceSample PresenceAdvance(PresenceState* st, bool present,
                               const motion::Transition& transition, double now,
                               bool reduced);

struct Presence {
    uint32_t key = 0;
    bool present = false;
    motion::Transition transition = motion::Transition::New(0);

    static Presence New(uint32_t key, bool present) {
        Presence p;
        p.key = key;
        p.present = present;
        return p;
    }
    static Presence New(Str id, bool present) {
        return New(MotionId(id), present);
    }
    Presence Transition(const motion::Transition& t) const {
        Presence p = *this;
        p.transition = t;
        return p;
    }
    PresenceSample Sample(Ctx* cx) const;
};

struct MotionReveal {
    static El* New(Ctx* cx, Str id, float progress, El* child);
};

struct MotionRevealState {

    Bounds measured = {};

    float height = 0;
    bool hasHeight = false;
};

MotionRevealState* MotionRevealStateOf(Ctx* cx, Str id);

namespace motion {

template <typename T>
using MotionValue = gpui::MotionStep<T>;

template <typename T>
T transition(Ctx* cx, TransitionId id, T target, const Transition& policy) {
    return gpui::MotionValue(cx, id.key, target, policy);
}

template <typename T>
MotionValue<T> transition_with_status(Ctx* cx, TransitionId id, T target,
                                      const Transition& policy) {
    return MotionValueWithStatus(cx, id.key, target, policy);
}

template <typename T>
MotionValue<T> animate_keyframes(Ctx* cx, TransitionId id,
                                 const Keyframes<T>& keyframes,
                                 const Timing& timing) {
    return AnimateKeyframes(cx, id.key, keyframes, timing);
}

using Spring = gpui::Spring;
using SpringState = gpui::SpringState;
using SpringStep = gpui::SpringStep;

inline float spring(Ctx* cx, TransitionId id, float target,
                    const Spring& policy) {
    return SpringValue(cx, id.key, target, policy);
}

}

}

#line 1 "src/base/collapsible.h"

namespace gpui {

struct Collapsible {
    El* root = nullptr;
    Ctx* cx = nullptr;
    bool open = false;

    Str revealId = {};
    float revealProgress = 0;
    bool hasReveal = false;

    static Collapsible* New(Ctx* cx);

    Collapsible* FlexCol();
    Collapsible* Open(bool v);
    Collapsible* Reveal(Str id, float progress);
    Collapsible* Child(El* e);
    Collapsible* Content(El* e);
    El* IntoEl();
};
}

#line 1 "src/base/color_picker.h"

namespace gpui {

enum class ColorPickerEventKind : uint8_t {
    Change
};

struct ColorPickerEvent {
    ColorPickerEventKind kind = ColorPickerEventKind::Change;
    bool hasColor = false;
    Hsla color = {};
};

struct HslaSliders {
    SliderState hue = {};
    SliderState saturation = {};
    SliderState lightness = {};
    SliderState alpha = {};

    HslaSliders();
    SliderState* At(int index);
    const SliderState* At(int index) const;
    Hsla Read() const;
    void Write(Hsla color);
};

struct ColorPickerState {
    Entity<ColorPickerState> self = {};
    FocusHandle focus = {};
    uint32_t value = 0;
    bool hasValue = false;
    uint32_t preview = 0;
    bool hasPreview = false;
    bool open = false;
    int activeTab = 0;
    HslaSliders sliders;
    InputState hexInput;

    bool needsSliderSync = true;

    Listener onChange = {};

    ColorPickerState();

    static void OnToggleOpen(ColorPickerState* s, Ctx* cx, const ClickEvent*);
    static void OnOpenChange(ColorPickerState* s, Ctx* cx, const ClickEvent*,
                             int64_t open);
    static void OnTab(ColorPickerState* s, Ctx* cx, const ClickEvent*,
                      int64_t ix);
    static void OnSwatchClick(ColorPickerState* s, Ctx* cx, const ClickEvent*,
                              int64_t hex);
    static void OnSwatchHover(ColorPickerState* s, Ctx* cx,
                              const HoverEvent* ev, int64_t hex);
    static void OnSlider(ColorPickerState* s, Ctx* cx, const SliderEvent*);
    static void OnHexChange(ColorPickerState* s, Ctx* cx, const InputEvent* ev);
    static void OnHexFocus(ColorPickerState* s, Ctx* cx, const ClickEvent*);
};

void ColorPickerStateInit(ColorPickerState* s, Ctx* cx);
Entity<ColorPickerState> ColorPickerStateNew(Ctx* cx);

bool ColorPickerShown(const ColorPickerState* s, uint32_t* out);

void ColorPickerPreview(ColorPickerState* s, uint32_t color);
bool ColorPickerClearPreview(ColorPickerState* s);

void ColorPickerSetValue(ColorPickerState* s, uint32_t color);
void ColorPickerClearValue(ColorPickerState* s);

void ColorPickerSelect(ColorPickerState* s, uint32_t color);

void ColorPickerUpdateColor(ColorPickerState* s, uint32_t color);

void ColorPickerSetOpen(ColorPickerState* s, bool open);
void ColorPickerSetActiveTab(ColorPickerState* s, int tab);

void ColorPickerSyncPending(ColorPickerState* s);

uint32_t ColorPickerSliderColor(const ColorPickerState* s);

bool ColorPickerParseHex(Str text, uint32_t* out);

Str ColorPickerHexString(Arena* a, uint32_t color);

struct ColorPicker {
    static El* New(Ctx* cx, Str id, bool open = false, bool disabled = false,
                   Str accessibilityLabel = {},
                   AccessibilityRole role = AccessibilityRole::Button,
                   Listener onOpenChange = {}, FocusHandle focus = {},
                   int tabIndex = 0, bool tabStop = true,
                   const char* keyContext = "ColorPicker");
};

struct ColorSwatch {
    static El* New(Ctx* cx, Str id, Listener onClick = {},
                   Listener onHover = {}, uint32_t color = 0,
                   bool selected = false, bool disabled = false,
                   Str accessibilityLabel = {}, int tabIndex = 0,
                   bool tabStop = true,
                   AccessibilityRole role = AccessibilityRole::RadioButton);
};

template <>
struct EventEmitter<ColorPickerState, ColorPickerEvent> {};
}

#line 1 "src/base/select.h"

namespace gpui {

enum class SelectAction : uint8_t {

    None,

    Open,

    Confirm,

    Dismiss
};

void SelectInitKeys();
Str SelectContext();

SelectAction SelectActionOf(uint32_t id, bool open, bool disabled);

struct Select {

    static El* New(Ctx* cx, Str id, bool open = false, bool disabled = false,
                   Str accessibilityLabel = {}, Listener onOpenChange = {},
                   Str accessibilityValue = {});
};
}

#line 1 "src/base/combobox.h"

namespace gpui {

struct Combobox {
    static El* New(Ctx* cx, Str id);
};
}

#line 1 "src/base/component_traits.h"

namespace gpui {

struct ComponentStateFlags {
    bool selected = false;
    bool secondarySelected = false;

    bool open = false;
    bool disabled = false;
    bool focusRing = true;
    bool collapsed = false;
};

}

#line 1 "src/base/date_picker.h"

namespace gpui {

enum class DatePickerAction : uint8_t {
    None,
    Open,
    Dismiss,

    Clear
};

void DatePickerInitKeys();
Str DatePickerContext();

DatePickerAction DatePickerActionOf(uint32_t id, bool open, bool disabled);

struct DatePickerKeys {

    Listener onToggle = {};

    Listener onClear = {};
    bool open = false;
    bool disabled = false;

    static void OnAction(DatePickerKeys* self, Ctx* cx, const ActionEvent* ev);
};

void DatePickerBindKeys(Ctx* cx, El* root, Str name, Listener onToggle,
                        Listener onClear, bool open, bool disabled);

int64_t DatePickerDateKey(LocalDate date);
LocalDate DatePickerDateFromKey(int64_t key);

enum class DateSelectionResult : uint8_t {
    Rejected,
    Partial,
    Complete
};

DateSelectionResult DatePickerSelectDate(bool range, LocalDate value,
                                         LocalDate* start, LocalDate* end,
                                         const DateMatcher& disabled);

struct DatePicker {
    static El* New(Ctx* cx, Str id, bool disabled = false, bool open = false,
                   Listener onOpenChange = {});
};
}

#line 1 "src/base/geometry.h"

namespace gpui {

enum class Placement : uint8_t {
    Top,
    Bottom,
    Left,
    Right
};

enum class Side : uint8_t {
    Left,
    Right
};

inline bool SideIsLeft(Side s) {
    return s == Side::Left;
}

inline bool SideIsRight(Side s) {
    return s == Side::Right;
}

inline bool AxisIsHorizontal(Axis a) {
    return a == Axis::Horizontal;
}

inline bool AxisIsVertical(Axis a) {
    return a == Axis::Vertical;
}

inline bool PlacementIsHorizontal(Placement p) {
    return p == Placement::Left || p == Placement::Right;
}

inline bool PlacementIsVertical(Placement p) {
    return p == Placement::Top || p == Placement::Bottom;
}

inline Axis PlacementAxis(Placement p) {
    return PlacementIsHorizontal(p) ? Axis::Horizontal : Axis::Vertical;
}

inline Edges EdgesAll(float value) {
    return Edges::New(value, value, value, value);
}

}

#line 1 "src/base/resizable.h"

namespace gpui {

const float PANEL_MIN_SIZE = 100.f;
const float kResizablePanelMinSize = PANEL_MIN_SIZE;

bool ResizablePanelResize(float* sizes, const float* mins, const float* maxs,
                          int n, int ix, float size, float containerSize);

void ResizableAdjustToContainer(float* sizes, int n, float containerSize);

const float kResizeHandleSize = 1.f;
const float kResizeHandlePadding = 4.f;

namespace base_theme {
struct Theme;
}
Rgba ResizableHandleColor(const base_theme::Theme& theme, bool active);

enum class ResizeHandleState : uint8_t {

    Idle,

    Hovered,

    Pressed,

    Dragging,
};

inline bool ResizeHandleStateIsActive(ResizeHandleState s) {
    return s == ResizeHandleState::Pressed || s == ResizeHandleState::Dragging;
}

enum class HandleEdge : uint8_t {

    Leading,

    Trailing,
};

struct SharedHandleState {
    ResizeHandleState state = ResizeHandleState::Idle;

    Listener nextDown;
    Listener nextUp;
    Listener nextDrag;

    ResizeHandleState Get() const { return state; }

    bool Set(ResizeHandleState next) {
        bool changed = state != next;
        state = next;
        return changed;
    }

    static void OnDown(SharedHandleState* self, Ctx* cx,
                       const MouseDownEvent* ev);

    static void OnHover(SharedHandleState* self, Ctx* cx, const HoverEvent* ev);

    static void OnMove(SharedHandleState* self, Ctx* cx,
                       const MouseMoveEvent* ev);
    static void OnDragMove(SharedHandleState* self, Ctx* cx,
                           const DragMoveEvent* ev);

    static void OnUp(SharedHandleState* self, Ctx* cx, const MouseUpEvent* ev);

    static void OnUpOut(SharedHandleState* self, Ctx* cx,
                        const MouseUpEvent* ev);
};

Entity<SharedHandleState> ResizeHandleStateFor(Ctx* cx, Str name);

void ResizeHandleBindState(El* handle, Entity<SharedHandleState> state);

struct ResizeHandleContext {
    Axis axis = Axis::Horizontal;
    ResizeHandleState state = ResizeHandleState::Idle;
    HandleEdge edge = HandleEdge::Leading;
    bool hasEdge = false;

    Axis AxisValue() const { return axis; }

    bool Edge(HandleEdge* out) const {
        if (hasEdge && out) {
            *out = edge;
        }
        return hasEdge;
    }

    bool IsActive() const { return ResizeHandleStateIsActive(state); }

    ResizeHandleState State() const { return state; }
};

using ResizeHandleRenderer = El* (*)(void* user,
                                     const ResizeHandleContext* context,
                                     Ctx* cx);

struct ResizeHandle {
    Ctx* cx = nullptr;
    Str id = {};
    Axis axis = Axis::Horizontal;
    HandleEdge edge = HandleEdge::Leading;
    bool hasEdge = false;
    Listener onDrag = {};

    Str dragKind = {};
    int dragIx = 0;

    Listener onRelease = {};
    void* appearanceUser = nullptr;
    ResizeHandleRenderer appearance = nullptr;
    Rgba color = {};
    Rgba activeColor = {};

    static ResizeHandle* New(Ctx* cx, Str id, Axis axis);

    ResizeHandle* Inside(HandleEdge value);
    ResizeHandle* OnDrag(Listener listener);

    ResizeHandle* OnDrag(Str kind, int ix, Listener listener);
    ResizeHandle* OnRelease(Listener listener);
    ResizeHandle* WithAppearance(void* user, ResizeHandleRenderer renderer);
    ResizeHandle* Colors(Rgba rest, Rgba active);
    El* IntoEl();
};

ResizeHandle* resize_handle(Ctx* cx, Str id, Axis axis);

struct ResizableState {
    Axis axis = Axis::Horizontal;

    Vec<float> sizes;
    Vec<float> mins;
    Vec<float> maxs;

    Vec<bool> grows;

    Vec<bool> shown;

    Vec<Bounds> laid;

    Bounds bounds = {};
    float lastContainer = 0;

    int dragging = -1;

    Listener onResized = {};

    ~ResizableState() {
        VecReset(sizes);
        VecReset(mins);
        VecReset(maxs);
        VecReset(grows);
        VecReset(shown);
        VecReset(laid);
    }

    static void OnHandleDown(ResizableState* self, Ctx* cx,
                             const MouseDownEvent* ev, int64_t ix);
    static void OnHandleDrag(ResizableState* self, Ctx* cx,
                             const DragMoveEvent* ev);
    static void OnHandleUp(ResizableState* self, Ctx* cx,
                           const MouseUpEvent* ev);
    static void OnSettled(ResizableState* self, Ctx* cx, const void*);

    const Vec<float>& Sizes() const { return sizes; }
    float ContainerSize() const {
        return AxisIsHorizontal(axis) ? bounds.w : bounds.h;
    }
    bool ResizePanel(Ctx* cx, int ix, float size);
    bool InsertPanel(Ctx* cx, float size = PANEL_MIN_SIZE, int ix = -1);
    bool RemovePanel(Ctx* cx, int ix);
    bool ResetPanel(Ctx* cx, int ix);
    void Clear();
};

struct ResizablePanelEvent {
    const float* sizes = nullptr;
    int count = 0;
};

float ResizablePanelSize(const ResizableState* s, int ix, float declared);

struct ResizablePanelGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<ResizableState> state = {};
    gpui::Axis groupAxis = gpui::Axis::Horizontal;
    float width = kFill;
    float height = kFill;

    Rgba handleColor = {};
    Rgba handleDragColor = {};

    ArenaVec<El*> panels;
    ArenaVec<float> sizes;
    ArenaVec<float> mins;
    ArenaVec<float> maxs;
    ArenaVec<bool> grows;
    ArenaVec<bool> shown;

    ArenaVec<const struct ResizablePanel*> sources;
    void* handleAppearanceUser = nullptr;
    ResizeHandleRenderer handleAppearance = nullptr;
    Listener onResize = {};

    static ResizablePanelGroup* New(Ctx* cx, Str id,
                                    Entity<ResizableState> state = {},
                                    Axis axis = Axis::Horizontal);
    ResizablePanelGroup* W(float v);
    ResizablePanelGroup* H(float v);
    ResizablePanelGroup* WithState(Entity<ResizableState> value);
    ResizablePanelGroup* Axis(gpui::Axis value);
    ResizablePanelGroup* Size(float v);
    ResizablePanelGroup* HandleColors(Rgba rest, Rgba dragging);
    ResizablePanelGroup* WithHandleAppearance(void* user,
                                              ResizeHandleRenderer renderer);
    ResizablePanelGroup* OnResize(Listener listener);

    ResizablePanelGroup* Panel(El* content, float size,
                               float min = kResizablePanelMinSize,
                               float max = 0);

    ResizablePanelGroup* Grow(El* content, float min = kResizablePanelMinSize);

    ResizablePanelGroup* Flex();

    ResizablePanelGroup* Visible(bool v);
    ResizablePanelGroup* Child(struct ResizablePanel* panel);
    ResizablePanelGroup* Children(struct ResizablePanel** values, int count);
    El* IntoEl();
};

using Resizable = ResizablePanelGroup;

struct ResizablePanel {
    Ctx* cx = nullptr;

    El* content = nullptr;
    ArenaVec<El*> more;

    ElRefiner refiner = {};
    float size = 0;
    float min = PANEL_MIN_SIZE;
    float max = 0;
    bool grow = true;
    bool visible = true;

    static ResizablePanel* New(Ctx* cx);
    ResizablePanel* Child(El* value);
    ResizablePanel* Size(float value);
    ResizablePanel* SizeRange(float minValue, float maxValue = 0);
    ResizablePanel* FlexNone();
    ResizablePanel* Visible(bool value);
};

ResizablePanelGroup* h_resizable(Ctx* cx, Str id,
                                 Entity<ResizableState> state = {});
ResizablePanelGroup* v_resizable(Ctx* cx, Str id,
                                 Entity<ResizableState> state = {});
ResizablePanel* resizable_panel(Ctx* cx);
}

#line 1 "src/base/dock.h"

namespace gpui {

const float kDockPanelMinSize = 100.f;

const float kDockHandleW = 4;

const float kClosedBottomStrip = 29.f;

const float kDockDragPreviewW = 96.f;
const float kDockDragPreviewH = 30.f;

extern const Str kDockPanelDrag;
extern const Str kDockResizeDrag;

struct NodeId {
    uint64_t value = 0;

    static NodeId FromU64(uint64_t raw) { return NodeId{raw}; }
    uint64_t AsU64() const { return value; }
};

inline bool operator==(NodeId a, NodeId b) {
    return a.value == b.value;
}
inline bool operator!=(NodeId a, NodeId b) {
    return !(a == b);
}

struct PanelId {
    uint64_t value = 0;

    static PanelId FromU64(uint64_t raw) { return PanelId{raw}; }
    static PanelId FromEntity(EntityId id) {
        return PanelId{((uint64_t)id.gen << 32) | (uint32_t)id.index};
    }
    uint64_t AsU64() const { return value; }
};

inline bool operator==(PanelId a, PanelId b) {
    return a.value == b.value;
}
inline bool operator!=(PanelId a, PanelId b) {
    return !(a == b);
}

enum class DockPlacement : uint8_t {
    Center,
    Left,
    Bottom,
    Right
};

enum class DockDrop : uint8_t {
    Center,
    Left,
    Right,
    Top,
    Bottom
};

DockDrop DockDropAt(Bounds b, float x, float y);

Bounds DockDropPlaceholder(Bounds b, DockDrop d);

bool split_placement_at(Bounds bounds, Point position, Placement* out);

struct DropPlaceholderBounds {
    Point origin = {};
    Size size = {};

    static DropPlaceholderBounds ForPlacement(Bounds bounds,
                                              const Placement* placement);
    Bounds In(Bounds parent) const {
        return {parent.x + origin.x, parent.y + origin.y, size.w, size.h};
    }
};

struct DragPanel {
    PanelId panel = {};
    NodeId source = {};
    Point dragOffset = {};
    Size previewSize = {};
    uint64_t dragSessionId = 0;

    static DragPanel New(PanelId panel, NodeId source);
};

struct AnyDrag {
    void* value = nullptr;

    uint64_t type = 0;
};

enum class DropTarget : uint8_t {
    Canvas,
    Group
};

struct DropTargetValue {
    DropTarget kind = DropTarget::Canvas;
    NodeId node = {};
    bool hasPlacement = false;
    Placement placement = Placement::Top;
};

struct DropIndicator {
    Bounds bounds = {};
    bool hasPlacement = false;
    Placement placement = Placement::Top;
    DropPlaceholderBounds from = {};
    DropPlaceholderBounds to = {};
    uint64_t dragSessionId = 0;
    uint64_t epoch = 0;
};

const int kDockSideBase = 1 << 20;

enum class DockPanelControl : uint8_t {
    No,
    Menu,
    Toolbar,
    Both
};

inline bool DockPanelControlToolbar(DockPanelControl c) {
    return c == DockPanelControl::Toolbar || c == DockPanelControl::Both;
}
inline bool DockPanelControlMenu(DockPanelControl c) {
    return c == DockPanelControl::Menu || c == DockPanelControl::Both;
}

enum class DockPanelStyle : uint8_t {
    Auto,
    TabBar
};

struct PanelStateNode;

struct DockPanelDef {

    Str name = {};
    Str title = {};
    PanelId id = {};
    El* (*render)(Ctx* cx, void* data) = nullptr;

    El* (*titleEl)(Ctx* cx, void* data) = nullptr;

    bool (*titleStyle)(Ctx* cx, void* data, Rgba* background,
                       Rgba* foreground) = nullptr;

    El* (*titleSuffix)(Ctx* cx, void* data) = nullptr;

    El* (*toolbarButtons)(Ctx* cx, void* data) = nullptr;

    void (*dropdownMenu)(Ctx* cx, void* data, void* menu) = nullptr;

    void (*dump)(void* data, PanelStateNode* out) = nullptr;

    Str tabName = {};
    void* data = nullptr;
    bool closable = true;

    bool visible = true;

    void (*setActive)(Ctx* cx, void* data, bool active) = nullptr;
    void (*setZoomed)(Ctx* cx, void* data, bool zoomed) = nullptr;
    void (*onAddedTo)(Ctx* cx, void* data, int node) = nullptr;
    void (*onRemoved)(Ctx* cx, void* data) = nullptr;
    bool canZoom = true;
    DockPanelControl zoomable = DockPanelControl::Menu;

    bool titleBar = true;

    bool innerPadding = true;
};

using Panel = DockPanelDef;
using PanelView = DockPanelDef;

enum class PanelEvent : uint8_t {
    ZoomIn,
    ZoomOut,
    LayoutChanged
};

struct DockNode {
    DockNode() = default;

    bool used = false;
    bool split = false;
    Axis axis = Axis::Horizontal;
    int parent = -1;

    Vec<int> child;
    Vec<float> size;
    Vec<int> panel;
    int activeIx = 0;

    Bounds bounds = {};

    float tabScrollX = 0;
    int pendingScrollIx = -1;
    Bounds tabStripBounds = {};
    Bounds activeTabBounds = {};

    int activeTabBoundsIx = -1;
};

struct DockSide {
    int node = -1;
    bool open = true;
    bool collapsible = true;
    float size = 200;

    static DockSide New(float value) {
        DockSide dock;
        dock.size = std::max(value, kDockPanelMinSize);
        return dock;
    }
    bool IsOpen() const { return open; }
    void SetOpen(bool value) { open = value; }
    bool IsCollapsible() const { return collapsible; }
    void SetCollapsible(bool value) { collapsible = value; }
    float GetSize() const { return size; }
    void SetSize(float value) { size = std::max(value, kDockPanelMinSize); }
    bool IsResizing() const { return resizing; }
    void SetResizing(bool value) { resizing = value; }

    float LiveSize() const { return liveSize; }
    void SetLiveSize(float value) { liveSize = value; }

    bool resizing = false;
    float liveSize = -1;
};

using Dock = DockSide;

struct DockSizing {
    DockPlacement placement = DockPlacement::Center;
    Bounds area = {};
    float oppositeDockSize = 0;

    static DockSizing New(DockPlacement placement);
    DockSizing WithAreaBounds(Bounds value) const;
    DockSizing WithAreaWidth(float value) const;
    DockSizing WithAreaHeight(float value) const;
    DockSizing WithOppositeDockSize(float value) const;
    float SizeFromPointer(Point pointer) const;
    float Clamp(float value) const;
};

enum class DockEventKind : uint8_t {

    LayoutChanged
};

struct DockEvent {
    DockEventKind kind = DockEventKind::LayoutChanged;
};

struct DockState {
    Vec<DockPanelDef> panels;

    Vec<DockNode> nodes;
    int center = -1;
    DockSide left = {};
    DockSide bottom = {};
    DockSide right = {};

    int zoomPanel = -1;

    bool locked = false;

    DockPanelStyle panelStyle = DockPanelStyle::Auto;

    bool toggleButtonVisible = true;

    bool closeButtonVisible = false;

    bool hasVersion = false;
    int version = 0;

    Bounds bounds = {};

    int dropNode = -1;
    DockDrop dropAt = DockDrop::Center;

    Bounds dropFrom = {};
    bool dropFromPending = false;

    int menuNode = -1;

    DockPlacement resizingSide = DockPlacement::Center;
    bool resizing = false;

    Listener onEvent;

    static void OnTabClick(DockState* self, Ctx* cx, const ClickEvent* ev,
                           int64_t nodeAndIx);
    static void OnCloseClick(DockState* self, Ctx* cx, const ClickEvent* ev,
                             int64_t nodeAndIx);
    static void OnZoomClick(DockState* self, Ctx* cx, const ClickEvent* ev,
                            int64_t panelIx);
    static void OnToggleSide(DockState* self, Ctx* cx, const ClickEvent* ev,
                             int64_t placement);
    static void OnTabDragMove(DockState* self, Ctx* cx,
                              const DragMoveEvent* ev);
    static void OnTabDragEnd(DockState* self, Ctx* cx, const MouseUpEvent* ev);
    static void OnDropPanel(DockState* self, Ctx* cx, const DropEvent* ev,
                            int64_t node);

    static void OnDropTab(DockState* self, Ctx* cx, const DropEvent* ev,
                          int64_t nodeAndIx);
    static void OnDropTabBar(DockState* self, Ctx* cx, const DropEvent* ev,
                             int64_t node);

    static void OnMenuItem(DockState* self, Ctx* cx, const ClickEvent* ev,
                           int64_t nodeAndIx);

    static void OnTabBarScroll(DockState* self, Ctx* cx, const ScrollEvent* ev,
                               int64_t node);
    static void OnResizeDrag(DockState* self, Ctx* cx, const DragMoveEvent* ev);
    static void OnResizeEnd(DockState* self, Ctx* cx, const MouseUpEvent* ev);

    ~DockState() {
        for (int i = 0; i < len(nodes); i++) {
            VecReset(nodes[i].child);
            VecReset(nodes[i].size);
            VecReset(nodes[i].panel);
        }
        VecReset(nodes);
        VecReset(panels);
    }
};

inline int64_t DockPack(int node, int ix) {
    return (int64_t)(node * 64 + ix);
}
inline int DockUnpackNode(int64_t v) {
    return (int)(v / 64);
}
inline int DockUnpackIx(int64_t v) {
    return (int)(v % 64);
}

void DockNormalize(DockState* s);

int DockAddPanelDef(DockState* s, DockPanelDef def);

int DockNewTabs(DockState* s);
int DockNewSplit(DockState* s, Axis axis);

void DockTabsAdd(DockState* s, int node, int panelIx);

void DockTabsInsert(DockState* s, int node, int panelIx, int at);
void DockSplitAdd(DockState* s, int node, int childNode, float size);

void DockSetActive(DockState* s, Ctx* cx, int node, int ix);

void DockSelectPanel(DockState* s, Ctx* cx, PanelId panel);

void DockClosePanel(DockState* s, Ctx* cx, int node, int ix);

void DockMovePanel(DockState* s, Ctx* cx, int panelIx, int to, DockDrop drop,
                   int atIx = -1);

bool DockClosePanelAt(DockState* s, int node, int ix);
bool DockMovePanelTo(DockState* s, int panelIx, int to, DockDrop drop,
                     int atIx = -1);

float DockTabScrollTo(float scrollX, Bounds strip, Bounds tab);

void DockToggleSide(DockState* s, Ctx* cx, DockPlacement p);
void DockResizeSide(DockState* s, Ctx* cx, DockPlacement p, float x, float y);

void DockEndSideResize(DockState* s, Ctx* cx, DockPlacement p);

void DockSetDockSize(DockState* s, Ctx* cx, DockPlacement p, float size);

void DockSetSplitSizes(DockState* s, Ctx* cx, int node, const float* sizes,
                       int count);

void DockToggleZoom(DockState* s, Ctx* cx, int panelIx);

DockSide* DockSideOf(DockState* s, DockPlacement p);

int DockNodeOfPanel(const DockState* s, int panelIx);

int DockPanelByName(const DockState* s, Str name);

DockPlacement DockPlacementOfNode(const DockState* s, int node);

int DockVisibleCount(const DockState* s, int node);

bool DockNodeVisible(const DockState* s, int node);
int DockActiveIx(const DockState* s, int node);

bool DockNodeLocked(const DockState* s, int node);

bool DockIsLastPanel(const DockState* s, int node);

bool DockNodeDraggable(const DockState* s, int node);
bool DockNodeDroppable(const DockState* s, int node);

int DockLeftTopTabs(const DockState* s, int node);
int DockRightTopTabs(const DockState* s, int node);

void DockSetCollapsible(DockState* s, DockPlacement p, bool collapsible);

struct DockCtx {
    Ctx* cx = nullptr;
    Entity<DockState> state = {};
    Str id = {};
    DockPlacement placement = DockPlacement::Left;
    float size = 0;
    bool open = true;
    bool collapsible = true;
};

using DockContext = DockCtx;

float DockExtent(const DockCtx* dock);

El* DockFrame(Ctx* cx, const DockCtx* dock, float size);

struct DockHandleCtx {
    Axis axis = Axis::Horizontal;
    bool active = false;
    ResizeHandleState state = ResizeHandleState::Idle;
};

struct DockTabGroup {
    Ctx* cx = nullptr;
    Entity<DockState> state = {};
    Str id = {};
    int node = -1;

    bool collapsed = false;
};

using TabGroupContext = DockTabGroup;

enum class TabGroupEvent : uint8_t {
    Drop,
    DragDrop,
    ClosePanel,
    ActiveChanged,
    ZoomIn,
    ZoomOut
};

struct TabGroupConstraints {
    bool alone = true;
    bool dockLocked = true;
    bool collapsed = false;
    bool closable = false;

    static TabGroupConstraints Sealed() { return {}; }
    static TabGroupConstraints InSplit(bool isAlone) {
        TabGroupConstraints value;
        value.alone = isAlone;
        value.dockLocked = false;
        value.closable = true;
        return value;
    }
    TabGroupConstraints DockLocked(bool value) const {
        TabGroupConstraints copy = *this;
        copy.dockLocked = value;
        return copy;
    }
    TabGroupConstraints Collapsed(bool value) const {
        TabGroupConstraints copy = *this;
        copy.collapsed = value;
        return copy;
    }
    TabGroupConstraints Closable(bool value) const {
        TabGroupConstraints copy = *this;
        copy.closable = value;
        return copy;
    }
};

struct TabGroup {
    Entity<DockState> state = {};
    int node = -1;
    TabGroupConstraints constraints = {};
};

int DockGroupCount(const DockTabGroup* g);
const DockPanelDef* DockGroupPanel(const DockTabGroup* g, int ix);
int DockGroupActiveIx(const DockTabGroup* g);
DockPlacement DockGroupPlacement(const DockTabGroup* g);

bool DockGroupHasToggle(const DockTabGroup* g, DockPlacement p);

El* DockBindTab(const DockTabGroup* g, int ix, El* tab);

El* DockBindTabRest(const DockTabGroup* g, El* rest);

El* DockBindTabStrip(const DockTabGroup* g, El* strip);

bool DockGroupDroppable(const DockTabGroup* g);

El* DockBindTitleDrag(const DockTabGroup* g, int ix, El* e);
El* DockBindToggle(const DockTabGroup* g, DockPlacement p, El* e);
El* DockBindZoom(const DockTabGroup* g, int panelIx, El* e);
El* DockBindClose(const DockTabGroup* g, int ix, El* e);

bool DockGroupIsPanelClosable(const DockTabGroup* g, int ix);

ResizeHandle* DockBindResizeHandle(const DockCtx* d, ResizeHandle* h);

El* DockBindResizeStrip(const DockCtx* d, El* e);

void DockGroupOpenMenu(const DockTabGroup* g, bool open);

struct DockRenderer {
    void* data = nullptr;

    El* (*frame)(Ctx* cx, void* data) = nullptr;

    El* (*centerFrame)(Ctx* cx, void* data) = nullptr;
    El* (*splitFrame)(Ctx* cx, void* data, int node, Axis axis) = nullptr;

    El* (*splitHandle)(Ctx* cx, void* data, const DockHandleCtx* h) = nullptr;

    El* (*dock)(Ctx* cx, void* data, const DockCtx* d, El* content) = nullptr;

    El* (*tabGroupFrame)(Ctx* cx, void* data, const DockTabGroup* g) = nullptr;
    El* (*tabContentFrame)(Ctx* cx, void* data,
                           const DockTabGroup* g) = nullptr;
    El* (*tabBar)(Ctx* cx, void* data, const DockTabGroup* g) = nullptr;

    El* (*emptyGroup)(Ctx* cx, void* data, const DockTabGroup* g) = nullptr;

    El* (*dropIndicator)(Ctx* cx, void* data, Bounds to) = nullptr;

    El* (*dragPreview)(Ctx* cx, void* data, const DockPanelDef* def) = nullptr;
};

using DockAreaRenderer = DockRenderer;
using TabGroupRenderer = DockRenderer;

struct DockArea {
    static El* New(Ctx* cx, Str id, Entity<DockState> state,
                   const DockRenderer* r);
};

}

#line 1 "src/base/dock_layout.h"

namespace gpui {

enum class RootKind : uint8_t {
    Split,
    Any
};

enum class PaneKind : uint8_t {
    Split,
    Tabs
};

struct PaneNode;

struct PaneRef {
    PaneKind kind = PaneKind::Split;
    Axis axis = Axis::Horizontal;
    const Vec<PaneNode*>* children = nullptr;
    const Vec<float>* sizes = nullptr;
    const Vec<uint8_t>* sizeKnown = nullptr;
    const Vec<PanelId>* panels = nullptr;
    int activeIx = 0;
};

struct PaneNode {
    NodeId nodeId = {};
    PaneKind paneKind = PaneKind::Split;
    Axis axis = Axis::Horizontal;
    Vec<PaneNode*> children;

    Vec<float> sizes;
    Vec<uint8_t> sizeKnown;
    Vec<PanelId> panels;
    int activeIx = 0;

    static PaneNode* Split(NodeId id, Axis axis);
    static PaneNode* Tabs(NodeId id);
    NodeId Id() const { return nodeId; }
    PaneRef Kind() const;
    void Walk(Func1<const PaneNode*> visit) const;
    bool Empty() const;
    ~PaneNode();
};

enum class InsertTargetKind : uint8_t {
    Tabs,
    Split
};

struct InsertTarget {
    InsertTargetKind kind = InsertTargetKind::Tabs;
    NodeId node = {};
    int ix = -1;
    bool activate = true;
    Placement placement = Placement::Right;
    bool hasSize = false;
    float size = 0;
    static InsertTarget Tabs(NodeId node, int ix = -1, bool activate = true);
    static InsertTarget Split(NodeId node, Placement placement,
                              const float* size = nullptr);
};

struct EditResult {
    bool didChange = false;
    bool Changed() const { return didChange; }
};

struct DockLayout;
struct PanelSource;
struct DockAreaState;
struct PanelStateNode;

struct PaneBuilder {
    void* data = nullptr;
    PanelId (*build)(void* data, const PanelStateNode* state) = nullptr;
};

struct PaneTree {
    PaneNode* root = nullptr;
    RootKind rootKind = RootKind::Any;

    explicit PaneTree(RootKind kind = RootKind::Any);
    PaneTree(const PaneTree&) = delete;
    PaneTree& operator=(const PaneTree&) = delete;
    ~PaneTree();

    PaneNode* Root() { return root; }
    const PaneNode* Root() const { return root; }
    RootKind GetRootKind() const { return rootKind; }
    NodeId AllocateNodeId();
    PaneNode* FindNode(NodeId id);
    const PaneNode* FindNode(NodeId id) const;
    bool FindPanelNode(PanelId panel, NodeId* out) const;
    bool ContainsPanel(PanelId panel) const;
    void NodeIds(Vec<NodeId>* out) const;
    void Panels(Vec<PanelId>* out) const;

    NodeId SetRootSplit(Axis axis);
    NodeId SetRootTabs(const PanelId* panels, int count, int activeIx = 0);
    NodeId AddSplit(NodeId parent, Axis axis, const float* size = nullptr);
    NodeId AddTabs(NodeId parent, const PanelId* panels, int count,
                   const float* size = nullptr);

    EditResult InsertPanel(PanelId panel, InsertTarget target);
    EditResult RemovePanel(PanelId panel);
    EditResult MovePanel(PanelId panel, InsertTarget target);
    EditResult Split(NodeId at, PanelId panel, Placement placement,
                     const float* size = nullptr);
    EditResult SetActive(NodeId node, int ix);
    EditResult SetSizes(NodeId node, const float* sizes, const uint8_t* known,
                        int count);
    void Normalize();
    bool IsNormalized() const;

    int ToState(const PanelSource& source, DockAreaState* out) const;

    void FromState(const DockAreaState* st, int nodeIx,
                   const PaneBuilder& builder);

    static PaneTree* FromLayout(DockLayout* layout, RootKind kind,
                                Vec<DockPanelDef>* panels = nullptr);

  private:
    bool ApplyInsert(PanelId panel, InsertTarget target);
    bool DetachPanel(PanelId panel);
    bool InsertBeside(NodeId at, PanelId panel, Placement placement,
                      const float* size);
};

struct DockLayout {
    PaneKind kind = PaneKind::Split;
    Axis axis = Axis::Horizontal;
    Vec<DockLayout*> children;
    Vec<float> sizes;
    Vec<uint8_t> sizeKnown;
    Vec<PanelId> panelIds;
    Vec<DockPanelDef> panelViews;
    int activeIx = 0;

    static DockLayout* HSplit();
    static DockLayout* VSplit();
    static DockLayout* Tabs();
    DockLayout* Child(DockLayout* child, const float* size = nullptr);
    DockLayout* Panel(PanelId id, DockPanelDef view = {});
    DockLayout* ActiveIndex(int ix);
    ~DockLayout();
};

}

#line 1 "src/base/json.h"

namespace gpui {

enum class JsonKind : uint8_t {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonValue;

struct JsonValue {
    JsonKind kind = JsonKind::Null;
    bool b = false;
    double num = 0;
    Str str = {};
    Str key = {};

    JsonValue* first = nullptr;
    JsonValue* next = nullptr;
};

JsonValue* JsonParse(Arena* a, Str text);

const JsonValue* JsonGet(const JsonValue* v, const char* key);

const JsonValue* JsonAt(const JsonValue* v, int index);

int JsonLen(const JsonValue* v);

double JsonNumber(const JsonValue* v, double fallback = 0);
bool JsonBool(const JsonValue* v, bool fallback = false);
Str JsonString(const JsonValue* v, Str fallback = {});

struct JsonWriter {
    StrBuilder* out = nullptr;

    bool wrote[32] = {};
    int depth = 0;

    void BeginObject(const char* key = nullptr);
    void EndObject();
    void BeginArray(const char* key = nullptr);
    void EndArray();
    void Number(const char* key, double v);
    void Bool(const char* key, bool v);
    void String(const char* key, Str v);
    void Null(const char* key);

    void Raw(const char* key, Str json);
    void Value(const char* key, const JsonValue* value);
};

}

#line 1 "src/base/dock_state.h"

namespace gpui {

enum class PanelInfoKind : uint8_t {
    Panel,
    Stack,
    Tabs
};

using PanelInfo = PanelInfoKind;

struct PanelStateNode {
    Str panelName = {};

    Vec<int> children;
    PanelInfoKind kind = PanelInfoKind::Panel;

    Vec<float> sizes;
    Axis axis = Axis::Horizontal;

    int activeIndex = 0;

    Str info = {};
    bool infoIsJson = false;
};

using PanelState = PanelStateNode;

struct PanelSource {
    void* data = nullptr;
    Str (*panelName)(void* data, PanelId id) = nullptr;
    bool (*isVisible)(void* data, PanelId id) = nullptr;
    void (*dump)(void* data, PanelId id, PanelState* out) = nullptr;
};

struct PanelBuilder {
    void* data = nullptr;
    DockPanelDef (*build)(void* data, const PanelState* state,
                          const PanelInfo* info, Window* win,
                          App* app) = nullptr;
};

struct DockSideState {
    bool present = false;
    int node = -1;
    DockPlacement placement = DockPlacement::Left;
    float size = 0;
    bool open = true;
};

struct DockAreaState {

    bool hasVersion = false;
    int version = 0;
    Vec<PanelStateNode> nodes;
    int center = -1;
    DockSideState left = {};
    DockSideState right = {};
    DockSideState bottom = {};

    int NewNode(Str panelName);

    void Clear();

    ~DockAreaState() {
        for (int i = 0; i < len(nodes); i++) {
            VecReset(nodes[i].children);
            VecReset(nodes[i].sizes);
        }
        VecReset(nodes);
    }
};

bool DockAreaStateParse(Arena* a, Str json, DockAreaState* out);

void DockAreaStateWrite(const DockAreaState* s, StrBuilder* out);

void DockDump(const DockState* s, DockAreaState* out);

struct DockInvalidPanel {
    Str name = {};
};

bool DockLoad(DockState* s, const DockAreaState* st, Arena* a,
              El* (*invalidRender)(Ctx* cx, void* data) = nullptr,
              App* app = nullptr, Window* win = nullptr,
              Entity<DockState> dockArea = {});

}

#line 1 "src/base/dock_registry.h"

namespace gpui {

struct PanelBuildContext {
    Entity<DockState> dockArea = {};
    const PanelState* state = nullptr;
    const PanelInfo* info = nullptr;
};

using PanelRegistryBuild = DockPanelDef (*)(const PanelBuildContext* context,
                                            Window* win, App* app, void* data);

struct PanelRegistryEntry {
    Str name = {};
    PanelRegistryBuild build = nullptr;
    void* data = nullptr;
};

struct PanelRegistry {
    Arena* arena = nullptr;
    Vec<PanelRegistryEntry> items;

    PanelRegistry();
    ~PanelRegistry();
    void Register(Str panelName, PanelRegistryBuild build, void* data);
    bool BuildPanel(Str panelName, const PanelBuildContext* context,
                    Window* win, App* app, DockPanelDef* out) const;
};

PanelRegistry* PanelRegistryGlobal(App* app);
void register_panel(App* app, Str panelName, PanelRegistryBuild build,
                    void* data = nullptr);

}

#line 1 "src/base/event.h"

namespace gpui {

inline bool IsDoubleClick(const ClickEvent* ev) {
    return ev && ev->clickCount == 2;
}

}

#line 1 "src/base/focus_trap.h"

namespace gpui {

void FocusTrapInit(App* app);

struct FocusTrapContainer {
    static El* New(Ctx* cx, Str id, FocusHandle focus, El* child);
};

int FocusTrapId(Str name);

int FocusTrapActive(const Window* win);

int FocusTrapOf(const Window* win, int focusId);

int FocusTrapTab(Window* win, bool backward);

bool FocusTrapEnter(Window* win, int trapId, bool backward = false);

void FocusTrapArm(Window* win, int trapId, int hostFocusId = 0);

void FocusTrapApplyPending(Window* win);

}

#line 1 "src/base/global_state.h"

namespace gpui {

struct BaseGlobalState {

    Vec<EntityId> textViewStateStack;
    uint64_t selectionDocumentOrder = 1;
    Vec<EntityId> deferredPopovers;

    Arena* appMenuArena = nullptr;
    Vec<MenuDef> appMenus;
    bool suppressTextSelection = false;

    double lastTouch = -1;

    ~BaseGlobalState() {
        VecReset(textViewStateStack);
        VecReset(deferredPopovers);
        VecReset(appMenus);
        if (appMenuArena) {
            ArenaDelete(appMenuArena);
        }
    }
};

using GlobalState = BaseGlobalState;
using DeferredPopover = EntityId;

BaseGlobalState* BaseGlobalStateOf(App* app);
void BaseGlobalStateInit(App* app);
void BaseSuppressTextSelection(App* app);
void BaseResetTextSelectionSuppression(App* app);
bool BaseIsTextSelectionSuppressed(const App* app);

void BaseNoteTouch(App* app);
bool BaseIsTouchPress(const App* app);

const MenuDef* BaseAppMenus(const App* app, int* count);
void BaseSetAppMenus(App* app, const MenuDef* menus, int count);

void BaseSelectionFrameBegin(App* app);
uint64_t BaseSelectionNextDocumentOrder(App* app);

void BaseTextViewStatePush(App* app, EntityId state);
void BaseTextViewStatePop(App* app);
EntityId BaseTextViewStateCurrent(const App* app);

void BaseDeferredPopoverSet(App* app, EntityId popover, bool open);
DeferredPopover BaseRegisterDeferredPopover(App* app, EntityId popover);
bool BaseIsInDeferredContext(App* app);

}

#line 1 "src/base/history.h"

namespace gpui {
template <typename T>
struct HistoryForwardEntries {
    const T* items = nullptr;
    int len = 0;
    const T& operator[](int i) const { return items[len - 1 - i]; }
};
template <typename T>
struct History {
    Vec<T> entries;
    Vec<T> forwardEntries;
    int maxEntries = 1000;
    History& MaxEntries(int n) {
        maxEntries = std::max(0, n);
        EnforceMaxEntries();
        return *this;
    }
    void Push(T entry) {
        VecClear(forwardEntries);
        if (maxEntries == 0) return;
        VecAppend(entries, entry);
        EnforceMaxEntries();
    }
    const T* Current() const {
        return len(entries) ? &entries[len(entries) - 1] : nullptr;
    }
    void ReplaceCurrent(T entry) {
        if (len(entries))
            entries[len(entries) - 1] = entry;
        else
            Push(entry);
    }
    bool RemoveCurrent(T* out = nullptr) {
        if (!len(entries)) return false;
        if (out) *out = entries[len(entries) - 1];
        entries.len--;
        return true;
    }
    bool CanBack() const { return len(entries) > 1; }
    bool CanForward() const { return len(forwardEntries) > 0; }
    bool Back(T* out = nullptr) {
        if (!CanBack()) return false;
        VecAppend(forwardEntries, entries[len(entries) - 1]);
        entries.len--;
        if (out) *out = *Current();
        return true;
    }
    bool Forward(T* out = nullptr) {
        if (maxEntries == 0 || !CanForward()) return false;
        VecAppend(entries, forwardEntries[len(forwardEntries) - 1]);
        forwardEntries.len--;
        EnforceMaxEntries();
        if (out) *out = *Current();
        return true;
    }
    const Vec<T>& Entries() const { return entries; }
    HistoryForwardEntries<T> ForwardEntries() const {
        return {forwardEntries.els, len(forwardEntries)};
    }
    void Retain(bool (*keep)(const T&, void*), void* user = nullptr) {
        RetainIf(entries, keep, user);
        RetainIf(forwardEntries, keep, user);
    }
    void Clear() {
        VecClear(entries);
        VecClear(forwardEntries);
    }

  private:
    void EnforceMaxEntries() {
        int excess = len(entries) - maxEntries;
        if (excess <= 0) return;
        for (int i = excess; i < len(entries); i++)
            entries[i - excess] = entries[i];
        entries.len -= excess;
    }
    static void RetainIf(Vec<T>& values, bool (*keep)(const T&, void*),
                         void* user) {
        if (!keep) return;
        int n = 0;
        for (const T& value : values)
            if (keep(value, user)) values[n++] = value;
        values.len = n;
    }
};
}

#line 1 "src/base/undo_history.h"

namespace gpui {
template <typename T>
struct UndoHistory {
    Vec<Vec<T>*> undos;
    Vec<Vec<T>*> redos;
    double lastChangedAt = 0;
    bool hasLastChangedAt = false;
    int maxUndos = 1000;
    double groupInterval = 0;
    bool hasGroupInterval = false;
    bool grouping = false;
    bool ignoring = false;

    UndoHistory() = default;
    UndoHistory(const UndoHistory&) = delete;
    UndoHistory& operator=(const UndoHistory&) = delete;
    ~UndoHistory() { Clear(); }
    UndoHistory& MaxUndos(int n) {
        maxUndos = std::max(0, n);
        EnforceMaxUndos();
        return *this;
    }
    UndoHistory& GroupInterval(double seconds) {
        groupInterval = std::max(0.0, seconds);
        hasGroupInterval = true;
        return *this;
    }
    UndoHistory& GroupIntervalMs(int64_t ms) {
        return GroupInterval((double)ms / 1000);
    }
    void StartGrouping() { grouping = true; }
    void EndGrouping() { grouping = false; }
    bool IsIgnoring() const { return ignoring; }
    void SetIgnoring(bool value) { ignoring = value; }
    bool CanUndo() const { return len(undos) > 0; }
    bool CanRedo() const { return len(redos) > 0; }
    void Push(T item) {
        if (ignoring || maxUndos == 0) return;
        double now = TimeNow();
        bool group = grouping || (hasLastChangedAt && hasGroupInterval &&
                                  now - lastChangedAt <= groupInterval);
        if (!group || !len(undos)) {
            VecAppend(undos, new Vec<T>());
            EnforceMaxUndos();
        }
        VecAppend(*undos[len(undos) - 1], item);
        lastChangedAt = now;
        hasLastChangedAt = true;
        ClearStack(redos);
    }
    Vec<T> Undo() { return Move(undos, redos, true); }
    Vec<T> Redo() {
        if (maxUndos == 0) return Vec<T>();
        Vec<T> result = Move(redos, undos, false);
        EnforceMaxUndos();
        return result;
    }
    void Clear() {
        ClearStack(undos);
        ClearStack(redos);
        hasLastChangedAt = false;
    }

  private:
    static void ClearStack(Vec<Vec<T>*>& stack) {
        for (auto* transaction : stack) delete transaction;
        VecClear(stack);
    }
    void EnforceMaxUndos() {
        int excess = len(undos) - maxUndos;
        if (excess <= 0) return;
        for (int i = 0; i < excess; i++) delete undos[i];
        for (int i = excess; i < len(undos); i++) undos[i - excess] = undos[i];
        undos.len -= excess;
    }
    Vec<T> Move(Vec<Vec<T>*>& from, Vec<Vec<T>*>& to, bool reverse) {
        Vec<T> result;
        if (!len(from)) return result;
        Vec<T>* transaction = from[len(from) - 1];
        from.len--;
        for (int i = 0; i < transaction->len; i++) {
            VecAppend(result,
                      (*transaction)[reverse ? transaction->len - 1 - i : i]);
        }
        VecAppend(to, transaction);
        hasLastChangedAt = false;
        return result;
    }
};
}

#line 1 "src/base/hover_card.h"

namespace gpui {

struct HoverCardState {
    bool open = false;
    bool hoveringTrigger = false;
    bool hoveringContent = false;
    int openDelayMs = 600;
    int closeDelayMs = 300;

    Listener onOpenChange = {};

    int timer = 0;

    static void OnOpen(HoverCardState* self, Ctx* cx, const TickEvent* ev);
    static void OnClose(HoverCardState* self, Ctx* cx, const TickEvent* ev);
    static void OnTap(HoverCardState* self, Ctx* cx, const ClickEvent* ev,
                      int64_t open);
    static void OnDismiss(HoverCardState* self, Ctx* cx,
                          const MouseUpEvent* ev);
};

struct HoverCardOpenChangeEvent {
    bool open = false;
};

void HoverCardTriggerHover(HoverCardState* self, Ctx* cx, const HoverEvent* ev);
void HoverCardContentHover(HoverCardState* self, Ctx* cx, const HoverEvent* ev);

void HoverCardSetDelays(Ctx* cx, Entity<HoverCardState> state, int openMs,
                        int closeMs, Listener onOpenChange = {});
bool HoverCardIsOpen(Ctx* cx, Entity<HoverCardState> state);

Entity<HoverCardState> HoverCardStateFor(Ctx* cx, Str id);

bool HoverCardIsOpen(Ctx* cx, Str id);

struct HoverCard {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* root = nullptr;
    Str id = {};
    Entity<HoverCardState> state = {};

    bool tapToOpen = GPUI_OS_IOS || GPUI_OS_ANDROID;

    static HoverCard* New(Ctx* cx, Str id, Entity<HoverCardState> state = {});

    bool IsOpen() const;
    HoverCard* Trigger(El* trigger);
    HoverCard* Content(El* content);
    HoverCard* OnOpenChange(Listener fn);
    El* IntoEl();
};
}

#line 1 "src/base/index_path.h"

namespace gpui {

struct IndexPath {
    int section = 0;
    int row = 0;
    int column = 0;

    IndexPath Section(int v) const { return IndexPath{v, row, column}; }
    IndexPath Row(int v) const { return IndexPath{section, v, column}; }
    IndexPath Column(int v) const { return IndexPath{section, row, v}; }

    bool EqRow(IndexPath o) const {
        return section == o.section && row == o.row;
    }
};

inline IndexPath IndexPathNew(int row) {
    return IndexPath{0, row, 0};
}

inline bool operator==(IndexPath a, IndexPath b) {
    return a.section == b.section && a.row == b.row && a.column == b.column;
}
inline bool operator!=(IndexPath a, IndexPath b) {
    return !(a == b);
}

Str IndexPathIdStr(Arena* a, IndexPath p);

uint32_t IndexPathClickId(IndexPath p);

}

#line 1 "src/base/input_core.h"

namespace gpui {

struct InputMode {
    static constexpr InputKind KIND = InputKind::Input;
    static constexpr bool MULTI_LINE = false;
    static constexpr bool CODE_EDITOR = false;
};

struct TextareaMode {
    static constexpr InputKind KIND = InputKind::Textarea;
    static constexpr bool MULTI_LINE = true;
    static constexpr bool CODE_EDITOR = false;
};

struct EditorMode {
    static constexpr InputKind KIND = InputKind::Editor;
    static constexpr bool MULTI_LINE = true;
    static constexpr bool CODE_EDITOR = true;
};

struct InputModeKind {
    InputKind kind = InputKind::Input;

    static InputModeKind Of(const InputState* state);
    bool IsMultiLine() const;
    bool IsCodeEditor() const;
};

struct MultiLineMode {
    InputKind kind = InputKind::Textarea;

    static bool Includes(InputKind kind);
    bool IsEditor() const { return kind == InputKind::Editor; }
};

struct InputExtras {
    const InputState* state = nullptr;

    bool HasSemanticTokens() const;
    bool HasDocumentColors() const;
    bool HasHover() const;
    bool HasInlineCompletion() const;
};

struct EditorExtras : InputExtras {
    static EditorExtras Of(const InputState* state);
    bool HasDefinition() const;
    bool HasCodeActions() const;
};

using InputBaseState = InputState;
using EditorState = InputState;
using TextareaState = InputState;

struct InputContextMenuCapabilities {
    bool disabled = false;
    bool readonly = false;
    bool codeEditor = false;
    bool selection = false;
    bool masked = false;
    bool goToDefinition = false;
    bool codeActions = false;

    static InputContextMenuCapabilities New() { return {}; }
    static InputContextMenuCapabilities Of(const InputState* state);
    InputContextMenuCapabilities Disabled(bool value) const;
    InputContextMenuCapabilities Readonly(bool value) const;
    InputContextMenuCapabilities CodeEditor(bool value) const;
    InputContextMenuCapabilities Selection(bool value) const;
    InputContextMenuCapabilities Masked(bool value) const;
    InputContextMenuCapabilities GoToDefinition(bool value) const;
    InputContextMenuCapabilities CodeActions(bool value) const;
    bool IsDisabled() const { return disabled; }
    bool IsReadonly() const { return readonly; }
    bool IsEditable() const { return !disabled && !readonly; }
    bool IsCodeEditor() const { return codeEditor; }
    bool HasSelection() const { return selection; }
    bool IsMasked() const { return masked; }
    bool IsCopyable() const { return selection && !masked; }
    bool HasDefinition() const { return goToDefinition; }
    bool HasCodeActions() const { return codeActions; }
};

struct InputPresentation {
    FocusHandle focus = {};
    bool disabled = false;
    bool readonly = false;
    bool loading = false;
    bool masked = false;
    bool multiLine = false;
    bool codeEditor = false;
    int textAlign = 0;
    Str placeholder = {};
    Str maskPlaceholder = {};

    static InputPresentation Of(Arena* a, const InputState* state);
    bool IsEditable() const { return !disabled && !readonly; }
};

struct InputStyles {
    Style focused = {};
    uint32_t focusedFields = 0;
    Style disabled = {};
    uint32_t disabledFields = 0;

    InputStyles& Focused(const Style& style, uint32_t fields);
    InputStyles& Disabled(const Style& style, uint32_t fields);
    void Apply(Style* style, bool isFocused, bool isDisabled) const;
};

enum class NativeMenuItemKind : uint8_t {
    Separator,
    Action
};

struct NativeMenuItem {
    NativeMenuItemKind kind = NativeMenuItemKind::Separator;
    Str label = {};
    bool disabled = false;
    InputAction action = InputAction::None;
    bool goToDefinition = false;
};

struct NativeMenu {
    Arena* arena = nullptr;
    Vec<NativeMenuItem> items;

    NativeMenu();
    NativeMenu(const NativeMenu&) = delete;
    NativeMenu& operator=(const NativeMenu&) = delete;
    ~NativeMenu();
    NativeMenu& Menu(Str label, InputAction action);
    NativeMenu& MenuWithDisabled(Str label, bool disabled, InputAction action);
    NativeMenu& Separator();
    bool IsEmpty() const { return len(items) == 0; }
};

void InputDefaultNativeMenu(const InputState* state, NativeMenu* out);

inline void InputSetContextMenuEnabled(InputState* state, bool enabled) {
    if (state) {
        state->enableContextMenu = enabled;
    }
}
bool InputPerformNativeMenuItem(InputState* state, App* app, Window* win,
                                const NativeMenuItem& item);

void InputOnContextMenu(InputState* state, InputContextMenuFn handler,
                        void* data, void (*drop)(void* data) = nullptr);

void InputHandleRightClickMenu(InputState* state, App* app, Window* win,
                               Point position, int offset);

}

#line 1 "src/base/input_rope.h"

namespace gpui {

struct RopeLines {
    Str rope = {};
    int row = 0;
    int endRow = 0;

    static RopeLines New(Str rope);
    bool Next(Str* out);
    int Len() const { return std::max(0, endRow - row); }
};

struct RopeExt {
    Str text = {};

    static RopeExt Of(Str text) { return RopeExt{text}; }
    int LineStartOffset(int row) const;
    int LineEndOffset(int row) const;
    Str SliceLine(int row) const;
    Str SliceLines(int firstRow, int endRow) const;
    RopeLines IterLines() const;
    int LinesLen() const;
    int LineLen(int row) const;
    int CharAt(int offset, uint32_t* out) const;
    RopePoint OffsetToPoint(int offset) const;
    int PointToOffset(RopePoint point) const;
    int OffsetUtf16ToOffset(int offset) const;
    int OffsetToOffsetUtf16(int offset) const;
    int ClipOffset(int offset, Bias bias) const;
    bool WordRange(int offset, Selection* out) const;
    Str WordAt(int offset) const;
};

}

#line 1 "src/base/input_editor.h"

namespace gpui {

struct TabSize {
    int tabSize = 2;
    bool hardTabs = false;

    Str ToString(Arena* a) const;
    int IndentCount(Str line) const;
};

struct TextDecoration {
    Selection range = {};
    TextSpan style = {};

    static TextDecoration New(Selection range, const TextSpan& style);
};

struct DecorationCollectionsState;

struct TextDecorationCollection {
    DecorationCollectionsState* state = nullptr;
    uint64_t id = 0;

    TextDecorationCollection() = default;
    TextDecorationCollection(const TextDecorationCollection& other);
    TextDecorationCollection& operator=(const TextDecorationCollection& other);
    ~TextDecorationCollection();

    bool Set(const TextDecoration* decorations, int n);
    bool Append(const TextDecoration* decorations, int n);
    void Clear();
    int GetRanges(Selection* out, int cap) const;
    bool IsValid() const;
};

struct DecorationCollections {
    DecorationCollectionsState* state = nullptr;

    explicit DecorationCollections(InputState* input = nullptr);
    DecorationCollections(const DecorationCollections&) = delete;
    DecorationCollections& operator=(const DecorationCollections&) = delete;
    ~DecorationCollections();

    TextDecorationCollection Create(const TextDecoration* decorations = nullptr,
                                    int n = 0);
    void AdjustForEdit(Selection editedRange, int insertedLen);
    void Clear();

    int BuildSpans(TextSpan* out, int cap) const;
};

enum class RangeDecorationStyle : uint8_t {

    Fill,

    Frame,
};

struct RangeDecoration {
    Selection range = {};
    RangeDecorationStyle style = RangeDecorationStyle::Frame;
    Rgba color = {};
    bool hasColor = false;

    static RangeDecoration New(Selection range);
    Selection Range() const { return range; }
    RangeDecorationStyle Style() const { return style; }

    bool Color(Rgba* out) const;
    RangeDecoration WithStyle(RangeDecorationStyle value) const;
    RangeDecoration WithColor(Rgba value) const;
};

struct DecorationIndex {
    Vec<int> indices;
    Vec<int> maxEnds;

    void Rebuild(const RangeDecoration* decorations, int n);
    int Build(const RangeDecoration* decorations, int lo, int hi);

    int Query(const RangeDecoration* decorations, int lo, int hi,
              Selection range, Vec<int>* matches) const;
};

struct RangeDecorationEntries {
    uint64_t id = 0;
    Vec<RangeDecoration> decorations;
    DecorationIndex index;

    void Reindex();
};

struct RangeDecorationCollections {
    uint64_t nextId = 0;
    Vec<RangeDecorationEntries*> entries;

    RangeDecorationCollections() = default;
    RangeDecorationCollections(const RangeDecorationCollections&) = delete;
    RangeDecorationCollections& operator=(const RangeDecorationCollections&) =
        delete;
    ~RangeDecorationCollections();

    uint64_t Create(const RangeDecoration* decorations, int n);
    bool Set(uint64_t id, const RangeDecoration* decorations, int n);
    bool Append(uint64_t id, const RangeDecoration* decorations, int n);
    bool Remove(uint64_t id);
    RangeDecorationEntries* Get(uint64_t id) const;
    void AdjustForEdit(Selection editedRange, int insertedLen);
    void Clear();

    int Intersecting(const Selection* ranges, int nRanges,
                     const RangeDecoration** out, int cap) const;
};

int RangeDecorationsNormalize(Str text, const RangeDecoration* in, int n,
                              Vec<RangeDecoration>* out);

struct RangeDecorationsState;

struct RangeDecorationCollection {
    RangeDecorationsState* state = nullptr;
    uint64_t id = 0;

    RangeDecorationCollection() = default;
    RangeDecorationCollection(const RangeDecorationCollection& other);
    RangeDecorationCollection& operator=(
        const RangeDecorationCollection& other);
    ~RangeDecorationCollection();

    void Set(const RangeDecoration* decorations, int n);

    void Append(const RangeDecoration* decorations, int n);
    void Clear();
    void Dispose();

    int GetRanges(Selection* out, int cap) const;
    bool IsValid() const;
};

RangeDecorationCollection InputCreateRangeDecorationsCollection(
    InputState* s, const RangeDecoration* decorations, int n);

TextDecorationCollection InputCreateDecorationsCollection(
    InputState* s, const TextDecoration* decorations, int n);

int InputDecorationSpans(const InputState* s, TextSpan* out, int cap);

int InputRangeDecorations(const InputState* s, const Selection* ranges,
                          int nRanges, const RangeDecoration** out, int cap);

void InputDecorationsFree(InputState* s);
void InputDecorationsAdjustForEdit(InputState* s, Selection editedRange,
                                   int insertedLen);

void InputDecorationsReset(InputState* s);

struct RangeCorners {
    Point topLeft = {};
    Point topRight = {};
    Point bottomLeft = {};
    Point bottomRight = {};
};

int FrameOutlinePoints(const RangeCorners* corners, int n, Vec<Point>* out);

void PadFrameCorners(RangeCorners* corners, int n, float horizontalPadding);

float SnapFrameOutline(Point* points, int n, float strokeWidth,
                       float scaleFactor);

void ClampFrameToContentMask(Point* points, int n, float strokeWidth,
                             Bounds contentMask);

struct DiagnosticEntry {
    Selection range = {};
    Diagnostic diagnostic = {};
};

struct DiagnosticSummary {
    int count = 0;
    int start = 0;
    int end = 0;
};

struct DiagnosticSet {
    Arena* arena = nullptr;
    Str text = {};
    Vec<DiagnosticEntry> diagnostics;

    explicit DiagnosticSet(Str text = {});
    DiagnosticSet(const DiagnosticSet&) = delete;
    DiagnosticSet& operator=(const DiagnosticSet&) = delete;
    ~DiagnosticSet();

    void Reset(Str value);
    void Push(const Diagnostic& diagnostic);
    void Extend(const Diagnostic* values, int n);
    int Len() const { return len(diagnostics); }
    bool IsEmpty() const { return len(diagnostics) == 0; }
    void Clear();
    DiagnosticSummary Summary() const;
    int Range(Selection range, const DiagnosticEntry** out, int cap) const;
    const DiagnosticEntry* ForOffset(int offset) const;
    const DiagnosticEntry* At(int index) const;
};

struct BufferPoint {
    int line = 0;
    int col = 0;

    static BufferPoint New(int line, int col) { return {line, col}; }
};

struct DisplayPoint {
    int row = 0;
    int col = 0;

    static DisplayPoint New(int row, int col) { return {row, col}; }
};

enum class WrappingIndent : uint8_t {
    None,
    Same
};

struct DisplayMapRow {
    int bufferLine = 0;
    int startCol = 0;
    int endCol = 0;
};

struct DisplayMap {
    Str text = {};
    int wrapColumns = 0;
    WrappingIndent wrappingIndent = WrappingIndent::Same;
    TabSize tab = {};
    FoldMap foldMap;
    Vec<DisplayMapRow> rows;

    explicit DisplayMap(int wrapColumns = 0);
    DisplayMap(const DisplayMap&) = delete;
    DisplayMap& operator=(const DisplayMap&) = delete;
    ~DisplayMap();

    void SetText(Str value);
    void OnTextChanged(Str value);
    void SetWrapColumns(int columns);
    void SetWrappingIndent(WrappingIndent indent);
    void SetTabSize(TabSize value);
    BufferPoint ClipBufferPoint(BufferPoint point) const;
    DisplayPoint BufferPosToDisplayPos(BufferPoint point) const;
    BufferPoint DisplayPosToBufferPos(DisplayPoint point) const;
    int DisplayRowCount() const { return len(rows); }
    int WrapRowCount() const;
    int BufferLineCount() const;
    int DisplayRowToBufferLine(int row) const;
    Selection BufferLineToDisplayRowRange(int line) const;
    bool IsBufferLineHidden(int line) const;
    int BufferLineToDisplayRow(int line) const;
    void SetFoldCandidates(const FoldRange* ranges, int n);
    void SetFolded(int startLine, bool folded);
    void ToggleFold(int startLine);
    bool IsFoldedAt(int startLine) const;
    bool IsFoldCandidate(int startLine) const;
    void ClearFolds();
    void AdjustFoldsForEdit(Str oldText, Selection editedRange, Str inserted);

  private:
    void Rebuild();
};

struct InputHighlighterFactory {
    void* data = nullptr;
    bool (*create)(void* data, Str language, InputHighlighter* out) = nullptr;

    bool Create(Str language, InputHighlighter* out) const;
};

void InputDriveHighlighter(InputState* s, bool folding);

void InputSkipHighlighterEdits(InputState* s);

enum class SyntaxContext : uint8_t {
    Code,
    String,
    Comment,
};

struct SyntaxContextProvider {
    void* data = nullptr;
    SyntaxContext (*contextAt)(void* data, Str text, int offset) = nullptr;

    SyntaxContext ContextAt(Str text, int offset) const {
        return contextAt ? contextAt(data, text, offset) : SyntaxContext::Code;
    }
};

struct BracketPair {
    Str open = {};
    Str close = {};

    static BracketPair New(Str open, Str close) { return {open, close}; }
};

struct AutoClosingPair {
    Str open = {};
    Str close = {};
    const SyntaxContext* notIn = nullptr;
    int nNotIn = 0;

    static AutoClosingPair New(Str open, Str close) { return {open, close}; }
};

struct IndentationRules {

    Str increasePattern = {};
    Str decreasePattern = {};
    void* data = nullptr;
    bool (*increaseIndent)(void* data, Str text) = nullptr;
    bool (*decreaseIndent)(void* data, Str text) = nullptr;

    static IndentationRules FromPatterns(Str increase, Str decrease);
};

bool IndentPatternMatch(Str pattern, Str text);

struct LanguageConfig {
    const BracketPair* brackets = nullptr;
    int nBrackets = 0;
    const AutoClosingPair* autoClosingPairs = nullptr;
    int nAutoClosingPairs = 0;

    bool hasAutoClosingPairs = false;
    Str autoCloseBefore = {};
    IndentationRules indentation = {};
    bool hasIndentationRules = false;

    static LanguageConfig Default();
};

struct LanguageProvider {
    void* data = nullptr;
    Str (*languageName)(void* data, Arena* a, Str name) = nullptr;
    bool (*config)(void* data, Str canonicalName,
                   LanguageConfig* out) = nullptr;
    bool (*syntaxContextProvider)(void* data, Str canonicalName,
                                  SyntaxContextProvider* out) = nullptr;
};

void InputSetLanguageProvider(App* app, const LanguageProvider& provider);
void InputSetLanguageConfig(App* app, Str language,
                            const LanguageConfig& config);
LanguageConfig InputLanguageConfig(App* app, Str language);
SyntaxContextProvider InputSyntaxContextProvider(App* app, Str language);

SyntaxContext InputSyntaxContextAt(InputState* s, App* app, Str text,
                                   int offset);
void InputSyntaxCacheFree(InputState* s);

struct FoldIconRenderer {
    void* data = nullptr;
    El* (*render)(void* data, Ctx* cx, int line, bool folded) = nullptr;

    El* Render(Ctx* cx, int line, bool folded) const;
};

struct WrapBoundary {
    int ix = 0;
    int nextIndent = 0;

    bool operator==(const WrapBoundary& o) const {
        return ix == o.ix && nextIndent == o.nextIndent;
    }
};

struct LineFragment {
    Str text = {};
    float elementWidth = 0;
    int elementLen = 0;

    static LineFragment Text(Str text) { return {text, 0, 0}; }
    static LineFragment Element(float width, int len) {
        return {Str{}, width, len};
    }
};

constexpr int kLineWrapperMaxIndent = 256;

using WrapCharWidth = float (*)(void* user, uint32_t c);

bool LineWrapperIsWordChar(uint32_t c);

void LineWrapperWrapLine(const LineFragment* fragments, int n, float wrapWidth,
                         WrapCharWidth widthFor, void* user,
                         Vec<WrapBoundary>* out);

void LineBreakOpportunities(Str text, Vec<int>* out);

using WrapMeasureFn = float (*)(void* user, Str text);

void MeasuredWrapBoundaries(Str text, float width, WrappingIndent indent,
                            WrapMeasureFn measure, void* user,
                            Vec<WrapBoundary>* out,
                            WrapCharWidth hint = nullptr,
                            void* hintUser = nullptr);

using WrapLineFn = void (*)(void* user, Str line, int base,
                            Vec<WrapBoundary>* out);
void TextWrapperWrapItem(Str line, bool wrap, WrappingIndent indent,
                         WrapLineFn wrapLine, void* user, Vec<int>* rows,
                         int* indentChars);

}

#line 1 "src/base/input_lsp.h"

namespace gpui {

struct CompletionMenuOptions {
    float maxWidth = kCompletionMenuMaxW;

    static CompletionMenuOptions Default() { return {}; }
};

struct CompletionProvider {
    void* data = nullptr;
    CompletionFn completions = nullptr;
    InlineCompletionFn inlineCompletion = nullptr;
    CompletionTriggerFn isCompletionTrigger = nullptr;
    CompletionResolveFn resolveCompletions = nullptr;
    float inlineCompletionDebounceMs = kInlineCompletionDebounceMs;

    bool IsValid() const { return completions != nullptr; }
    void Install(InputState* state, CompletionMenuOptions options = {}) const;
};

struct CodeActionProvider {
    void* data = nullptr;
    Str (*id)(void* data) = nullptr;
    CodeActionFn codeActions = nullptr;
    CodeActionPerformFn performCodeAction = nullptr;

    bool IsValid() const { return codeActions != nullptr; }
    Str Id() const { return id ? id(data) : Str{}; }
    void Install(InputState* state) const;
};

struct DefinitionProvider {
    void* data = nullptr;
    DefinitionFn definitions = nullptr;

    bool IsValid() const { return definitions != nullptr; }
    void Install(InputState* state) const;
};

struct DocumentColorProvider {
    void* data = nullptr;
    DocumentColorFn documentColors = nullptr;

    bool IsValid() const { return documentColors != nullptr; }
    void Install(InputState* state) const;
};

struct HoverProvider {
    void* data = nullptr;
    HoverFn hover = nullptr;

    bool IsValid() const { return hover != nullptr; }
    void Install(InputState* state) const;
};

struct DocumentRangeSemanticTokensProvider {
    void* data = nullptr;
    const Str* legend = nullptr;
    int nLegend = 0;
    SemanticTokensFn semanticTokens = nullptr;

    bool IsValid() const { return semanticTokens != nullptr; }
    void Install(InputState* state) const;
};

struct ShowDocumentHandler {
    void* data = nullptr;
    ShowDocumentFn show = nullptr;

    bool IsValid() const { return show != nullptr; }
    bool Show(Str uri, bool external, Selection selection) const;
    void Install(InputState* state) const;
};

struct CompletionMenuState {
    bool open = false;
    int triggerStartOffset = -1;
    Str query = {};
    const CompletionItem* items = nullptr;
    int nItems = 0;
    int selected = 0;
    uint64_t revision = 0;

    static CompletionMenuState Of(const InputState* state);
    bool IsOpen() const { return open; }
    uint64_t Revision() const { return revision; }
};

struct CodeActionMenuState {
    bool open = false;
    const CodeActionItem* items = nullptr;
    int nItems = 0;
    int selected = 0;
    uint64_t revision = 0;

    static CodeActionMenuState Of(const InputState* state);
    bool IsOpen() const { return open; }
    uint64_t Revision() const { return revision; }
};

struct HoverPopoverState {
    Selection symbolRange = {};
    Str hover = {};
    bool open = false;

    static HoverPopoverState Of(const InputState* state);
};

struct Lsp {
    InputState* state = nullptr;
    CompletionProvider completionProvider = {};
    Vec<CodeActionProvider> codeActionProviders;
    HoverProvider hoverProvider = {};
    DefinitionProvider definitionProvider = {};
    DocumentColorProvider documentColorProvider = {};
    DocumentRangeSemanticTokensProvider semanticTokensProvider = {};
    ShowDocumentHandler showDocument = {};
    CompletionMenuOptions completionMenu = {};

    Lsp() = default;
    Lsp(const Lsp&) = delete;
    Lsp& operator=(const Lsp&) = delete;

    Lsp& Completion(const CompletionProvider& provider);
    Lsp& AddCodeAction(const CodeActionProvider& provider);
    Lsp& Hover(const HoverProvider& provider);
    Lsp& Definition(const DefinitionProvider& provider);
    Lsp& DocumentColors(const DocumentColorProvider& provider);
    Lsp& SemanticTokens(const DocumentRangeSemanticTokensProvider& provider);
    Lsp& ShowDocuments(const ShowDocumentHandler& handler);
    Lsp& CompletionMenu(const CompletionMenuOptions& options);
    void Install(InputState* input);
    void Update();
    void Reset();
    int DocumentColorsForRange(Selection visible, DocumentColor* out,
                               int cap) const;
    int SemanticTokensForRange(Selection visible,
                               const HighlightStyleResolver& resolver,
                               TextSpan* out, int cap) const;
};

}

#line 1 "src/base/input_tokens.h"

namespace gpui {

enum class InlineTokenError : uint8_t {
    Ok = 0,
    InvalidRange,
    InvalidBoundary,
    InvalidToken,
    OverlappingTokens,
    TextMismatch,
    UnsupportedMode,
    ValidationRejected,
    CompositionActive
};

const char* InlineTokenErrorMessage(InlineTokenError e);

struct InlineToken {
    Str id = {};
    Str text = {};
    Str label = {};

    static InlineToken New(Str id, Str text);
    InlineToken WithLabel(Str label) const;
    InlineTokenError Validate() const;
};

struct InlineTokenSpan {
    int start = 0;
    int end = 0;
    InlineToken token = {};
};

InlineToken InlineTokenDup(InlineToken t);
void InlineTokenFree(InlineToken* t);
InlineTokenSpan InlineTokenSpanDup(InlineTokenSpan s);
void InlineTokenSpanFree(InlineTokenSpan* s);
bool InlineTokenEq(InlineToken a, InlineToken b);

struct TokenDelta {
    Vec<InlineTokenSpan> removed;
    Vec<InlineTokenSpan> inserted;
};

void TokenDeltaFree(TokenDelta* d);
TokenDelta* TokenDeltaDup(const TokenDelta* d);

void InlineTokenSpansClear(Vec<InlineTokenSpan>* spans);

TokenDelta* TokenStoreReplace(Vec<InlineTokenSpan>* spans, int start, int end,
                              int newLen, const InlineTokenSpan* inserted,
                              int nInserted);

int TokenBoundary(const Vec<InlineTokenSpan>& spans, int offset, Bias bias);
void NormalizeTokenRange(const Vec<InlineTokenSpan>& spans, int* start,
                         int* end);

struct InputContent {
    Str text = {};
    Vec<InlineTokenSpan> tokens;

    static InputContent New(Str text);

    InlineTokenError WithToken(int start, int end, InlineToken token);
};

InputContent InputContentDup(const InputContent& content);
void InputContentFree(InputContent* content);
InputContent InputGetContent(const InputState* s);

struct InlineTokenContext {
    InlineTokenSpan span = {};
    bool selected = false;
    bool disabled = false;
    bool readonly = false;
    float lineHeight = 0;
    float availableWidth = 0;

    const InlineToken& Token() const { return span.token; }
};

struct InlineTokenClickEvent {
    InlineTokenSpan span = {};
    Bounds bounds = {};
    ClickEvent click = {};

    const InlineToken& Token() const { return span.token; }
};

struct InlineTokenHoverEvent {
    InlineTokenSpan span = {};
    Bounds bounds = {};
    bool hovered = false;

    int rangeUtf16Start = 0;
    int rangeUtf16End = 0;

    const InlineToken& Token() const { return span.token; }

    bool IsHovered() const { return hovered; }
};

struct InlineTokenHoverSnapshot {
    InlineTokenSpan span = {};
    Bounds bounds = {};
    int rangeUtf16Start = 0;
    int rangeUtf16End = 0;
};

struct InlineTokenPlaced {
    int start = 0;
    Bounds bounds = {};
};

typedef El* (*InlineTokenRenderer)(Ctx* cx, const InlineTokenContext* ctx,
                                   void* user);
typedef void (*InlineTokenClickListener)(const InlineTokenClickEvent* ev,
                                         Ctx* cx, void* user);

typedef void (*InlineTokenHoverListener)(const InlineTokenHoverEvent* ev,
                                         Ctx* cx, void* user);

struct InlineTokenStore {
    Vec<InlineTokenSpan> spans;
    InlineToken pending = {};
    bool hasPending = false;
    InlineTokenRenderer renderer = nullptr;
    void* rendererUser = nullptr;
    InlineTokenClickListener click = nullptr;
    void* clickUser = nullptr;
    InlineTokenHoverListener hover = nullptr;
    void* hoverUser = nullptr;
    bool secret = false;
    bool replaying = false;
    bool validatedEdit = false;

    InlineTokenPlaced* placed = nullptr;
    int nPlaced = 0;
    int capPlaced = 0;

    InlineTokenHoverSnapshot hovered = {};
    bool hasHovered = false;
    Vec<InlineTokenHoverSnapshot> pendingHoverExits;

    uint32_t hoverEpoch = 0;
};

void InlineTokenStoreFree(InlineTokenStore* store);
InlineTokenStore* InputTokenStore(InputState* s, bool create);
const InlineTokenStore* InputTokenStore(const InputState* s);
bool InputTokensVisible(const InputState* s);
const Vec<InlineTokenSpan>* InputTokens(const InputState* s);
int InputTokenBoundary(const InputState* s, int offset, Bias bias);
void InputNormalizeTokenRange(const InputState* s, int* start, int* end);
void InputSetTokenPresentation(InputState* s, InlineTokenRenderer renderer,
                               void* rendererUser,
                               InlineTokenClickListener click, void* clickUser,
                               bool secret);

void InputSetTokenHoverPresentation(InputState* s,
                                    InlineTokenHoverListener hover,
                                    void* hoverUser);

bool InputTokenHover(InputState* s, int start, Bounds bounds, bool hovered,
                     const InlineToken* expected, InlineTokenHoverEvent* out);

bool InputReconcileTokenHover(InputState* s, InlineTokenHoverEvent* out);

void InputTokenBoundsClear(InputState* s);

Bounds* InputTokenBoundsSlot(InputState* s, int start);
bool InputTokenBoundsGet(const InputState* s, int start, Bounds* out);
void InputSetValue(InputState* s, const InputContent& content);
InlineTokenError InputReplaceRangeWithToken(InputState* s, App* app,
                                            Window* win, int start, int end,
                                            InlineToken token);
InlineTokenError InputReplaceWithToken(InputState* s, App* app, Window* win,
                                       InlineToken token);

bool TextIsGraphemeBoundary(Str text, int off);
int InputPreviousStartOfWordAt(const InputState* s, int offset);
int InputNextEndOfWordAt(const InputState* s, int offset);

}

#line 1 "src/base/touch_selection.h"

namespace gpui {

enum class SelectionEdge : uint8_t {
    Start,
    End,
};

inline SelectionEdge SelectionEdgeOpposite(SelectionEdge edge) {
    return edge == SelectionEdge::Start ? SelectionEdge::End
                                        : SelectionEdge::Start;
}

struct TouchSelectionSnapshot {
    Bounds start = {};
    Bounds end = {};
    SelectionEdge dragging = SelectionEdge::Start;
    bool startVisible = true;
    bool endVisible = true;
    bool menuOpen = false;
    bool hasDragging = false;

    static TouchSelectionSnapshot New(Bounds start, Bounds end);
    TouchSelectionSnapshot WithEdgeVisible(SelectionEdge edge,
                                           bool visible) const;
    TouchSelectionSnapshot WithMenuOpen(bool open) const;
    TouchSelectionSnapshot WithDragging(SelectionEdge edge) const;
    TouchSelectionSnapshot WithoutDragging() const;
    Bounds Edge(SelectionEdge edge) const;
    bool IsEdgeVisible(SelectionEdge edge) const;
    bool IsEmpty() const;
    bool BoundsOfVisible(Bounds* out) const;
};

struct TouchHandle {
    static constexpr float kHitSize = 44.f;
    static constexpr float kKnobSize = 10.f;
    static constexpr float kBarWidth = 2.f;
    static constexpr float kExtent = 12.f;

    static Bounds HitBounds(SelectionEdge edge, Bounds caret);
    static Bounds BarBounds(Bounds caret);
    static Bounds KnobBounds(SelectionEdge edge, Bounds caret);
};

Bounds TouchCaretLineBox(Point position, float lineHeight);
bool TouchCaretInView(Bounds caret, Bounds viewport);

struct TouchEdgeDrag {
    SelectionEdge edge = SelectionEdge::Start;
    Point offset = {};

    static TouchEdgeDrag Begin(SelectionEdge edge, Bounds caret, Point finger);
    TouchEdgeDrag WithEdge(SelectionEdge value) const;
    Point TextPosition(Point finger) const;
};

}

#line 1 "src/base/input.h"

namespace gpui {

struct SemanticThemeTokens;

struct InputBase {
    static El* New(Ctx* cx, Str id, bool interactive = false,
                   AccessibilityRole role = AccessibilityRole::TextInput);
    static El* New(Ctx* cx, Str id, const InputPresentation& presentation,
                   const InputStyles& styles = {});
};

struct InputEditorStyle {
    Rgba foreground = {0, 0, 0, 0};
    Rgba mutedForeground = {0, 0, 0, 0};
    Rgba caret = {0, 0, 0, 0};
    Rgba selection = {0, 0, 0, 0};
    float fontSize = 12;

    bool mono = false;

    uint8_t fontFamily = 0;

    bool mask = false;
    int align = 0;

    HighlightStyleResolver highlightStyles = {};

    const TextSpan* spans = nullptr;
    int nSpans = 0;

    const Selection* matches = nullptr;
    int nMatches = 0;

    int currentMatch = -1;

    Rgba matchBg = {0, 0, 0, 0};
    Rgba currentMatchBg = {0, 0, 0, 0};

    Rgba background = {0, 0, 0, 0};
    Rgba border = {0, 0, 0, 0};

    Rgba linkText = {0, 0, 0, 0};

    DiagnosticColors diagnostics = {};

    Rgba activeLine = {0, 0, 0, 0};

    float activeLineBleedL = 0;
    Rgba indentGuide = {0, 0, 0, 0};

    int indentWidth = 4;
};

InputEditorStyle InputEditorStyleResolve(const InputEditorStyle& projected,
                                         const SemanticThemeTokens& tokens);

struct Input {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style);
};

struct Textarea {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style,
                   bool lineNumbers = false);
};

struct Editor {
    static El* New(Ctx* cx, InputState* state);
    static El* New(Ctx* cx, InputState* state, const InputEditorStyle& style);
};

int InputLastPaintedRows(const InputState* s, const Window* win, Selection* out,
                         int cap);

int InputLastRangeCorners(const InputState* s, Window* win, Selection range,
                          Vec<RangeCorners>* out);

bool InputLastRangeDecorationPaths(const InputState* s, const Window* win,
                                   int* fills, int* frames);

bool InputLastCaretPoint(const InputState* s, Window* win, int offset,
                         Point* out);

bool InputRangeToBounds(const InputState* s, Window* win, Selection range,
                        Bounds* out);

struct InputPaintedVisualRow {
    int start = 0;
    int len = 0;
    int lineStart = 0;
    int lineLen = 0;
    bool lastOfLine = false;
    Bounds text = {};
    float width = 0;
};
int InputLastVisualRows(const InputState* s, Window* win,
                        InputPaintedVisualRow* out, int cap);

bool InputLastActiveLine(const InputState* s, const Window* win, Bounds* out);

void InputUpdateWrapMap(InputState* s, PaintCtx* ctx, float width,
                        float fontSize, uint16_t fontWord);

int InputWrapRows(const InputState* s, int line, const int** starts,
                  float* indent);

void InputSetWrappingIndent(InputState* s, App* app, Window* win,
                            WrappingIndent indent);

bool InputTouchSelection(const InputState* s, Window* win,
                         TouchSelectionSnapshot* out);

void InputKeepTouchSelection(InputState* s, App* app, Window* win);

void InputDismissTouchSelection(InputState* s, App* app, Window* win);

void InputCloseEditMenu(InputState* s, App* app, Window* win);

bool InputReopenEditMenuAt(InputState* s, App* app, Window* win, Point at);

void InputEditMenuOnScroll(InputState* s, App* app, Window* win,
                           TouchPhase phase);

void InputSelectAllFromEditMenu(InputState* s, App* app, Window* win);

bool InputOnLongPress(InputState* s, App* app, Window* win,
                      const LongPressEvent& event);

void InputBeginEdgeDrag(InputState* s, App* app, Window* win,
                        SelectionEdge edge, Point finger);
void InputUpdateEdgeDrag(InputState* s, App* app, Window* win, Point finger);

void InputExtendEdgeDragTo(InputState* s, App* app, Window* win,
                           Point position);
void InputEndEdgeDrag(InputState* s, App* app, Window* win);

bool InputIsEditMenuOpen(const InputState* s);
bool InputTouchSelectionRange(const InputState* s, Selection* out);
}

#line 1 "src/gpui/keymap.h"

namespace gpui {

struct KeyChord {
    int vk = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;

    bool platform = false;

    bool function = false;
};

bool KeyChordParse(Str spec, KeyChord* out);
bool KeyChordEq(const KeyChord& a, const KeyChord& b);

const int kMaxContextDepth = 8;

const int kMaxStrokes = 3;

int KeyChordsParse(Str spec, KeyChord* out, int maxChords);

uint32_t ActionOf(Str name);

uint32_t KeyContextOf(Str name);

struct KeyBinding {
    const char* stroke = nullptr;
    uint32_t action = 0;
    const char* context = nullptr;

    int64_t arg = 0;
};

void KeymapBind(const KeyBinding* bindings, int n);
void KeymapClear();

uint32_t KeymapGeneration();

struct KeyMatch {
    uint32_t action = 0;

    int64_t arg = 0;
    bool pending = false;
};

KeyMatch KeymapMatch(const KeyChord& chord, const uint32_t* contexts,
                     int nContexts);

bool KeymapBindingForAction(uint32_t action, const uint32_t* contexts,
                            int nContexts, KeyChord* out);

bool KeymapAnyBindingForAction(uint32_t action, KeyChord* out);

bool KeymapAnyBindingForActionArg(uint32_t action, int64_t arg, KeyChord* out);

Str KeyName(int vk);

bool KeyIsPrintable(int vk);

bool KeymapPending();

void KeymapClearPending();

}

#line 1 "src/base/input_keys.h"

namespace gpui {

namespace input {

uint32_t ActivateToken();
uint32_t Backspace();
uint32_t Copy();
uint32_t Cut();
uint32_t Delete();
uint32_t DeleteToBeginningOfLine();
uint32_t DeleteToEndOfLine();
uint32_t DeleteToNextWordEnd();
uint32_t DeleteToPreviousWordStart();
uint32_t Escape();
uint32_t Indent();
uint32_t IndentInline();
uint32_t MoveDown();
uint32_t MoveEnd();
uint32_t MoveHome();
uint32_t MoveLeft();
uint32_t MovePageDown();
uint32_t MovePageUp();
uint32_t MoveRight();
uint32_t MoveToEnd();
uint32_t MoveToNextWord();
uint32_t MoveToPreviousWord();
uint32_t MoveToStart();
uint32_t MoveUp();
uint32_t Outdent();
uint32_t OutdentInline();
uint32_t Paste();
uint32_t Redo();
uint32_t Replace();
uint32_t Search();
uint32_t SelectAll();
uint32_t SelectDown();
uint32_t SelectLeft();
uint32_t SelectRight();
uint32_t SelectToEnd();
uint32_t SelectToEndOfLine();
uint32_t SelectToNextWordEnd();
uint32_t AddCursorAbove();
uint32_t AddCursorBelow();
uint32_t SelectToPreviousWordStart();
uint32_t SelectToStart();
uint32_t SelectToStartOfLine();
uint32_t SelectUp();
uint32_t Undo();
uint32_t Enter();
uint32_t ToggleCodeActions();

}

Str InputContext();

void InputInitKeys();

InputAction InputActionOf(uint32_t id, int64_t arg = 0);

constexpr bool InputEnterShift(int64_t arg) {
    return (arg & 2) != 0;
}

}

#line 1 "src/base/link.h"

namespace gpui {

struct LinkStyles {
    StateStyle disabled = {};
};

struct Link {
    static El* New(Ctx* cx, Str id, bool disabled = false,
                   Listener onActivate = {},
                   const LinkStyles* styles = nullptr);
};
}

#line 1 "src/base/list_settings.h"

namespace gpui {

struct ListSettings {

    bool activeHighlight = true;
};

const ListSettings& ListSettingsNow(App* app);
void ListSettingsSet(App* app, ListSettings s);

struct ListActiveStyle {
    Background bg = {};
    Rgba border = {};
    bool hasBorder = false;
};
ListActiveStyle ListActiveStyleOf(const ListSettings& settings,
                                  Background active, Rgba activeBorder,
                                  Background accent, bool selected);

El* ListActiveOverlay(Arena* a, Rgba border, float radius);

}

#line 1 "src/base/measure.h"

namespace gpui {

bool MeasurementEnabled();

struct Measure {
    Str name = {};
    double started = 0;
    bool active = false;
};

Measure MeasureBegin(Str name);
void MeasureEnd(Measure* measure);

using MeasureFn = Func0;
void MeasureRun(Str name, MeasureFn fn);
void MeasureRunIf(Str name, bool enabled, MeasureFn fn);

}

#line 1 "src/base/nav_stack.h"

namespace gpui {

enum class NavOperation : uint8_t {
    Push,
    Pop,
    Replace
};

enum class NavMotion : uint8_t {
    Animated,
    Immediate
};

enum class NavStackEvent : uint8_t {
    Pushed,
    Popped,
    Forwarded,
    Replaced,
    Cleared
};

struct NavEntry {
    EntityId view = {};
};

struct NavTransit {
    EntityId outgoing = {};

    int index = 0;
    NavOperation operation = NavOperation::Push;
    NavMotion motion = NavMotion::Animated;
};

struct NavStackState {
    History<NavEntry> history;
    NavTransit transit = {};
    bool hasTransit = false;

    Entity<NavStackState> self = {};

    int Depth() const { return history.Entries().len; }
    bool IsEmpty() const { return history.Entries().len == 0; }

    EntityId Current() const {
        const NavEntry* entry = history.Current();
        return entry ? entry->view : EntityId{};
    }

    EntityId ViewAt(int index) const {
        const Vec<NavEntry>& undos = history.Entries();
        return index >= 0 && index < len(undos) ? undos[index].view
                                                : EntityId{};
    }

    int ForwardCount() const { return history.ForwardEntries().len; }
    EntityId ForwardViewAt(int index) const {
        auto entries = history.ForwardEntries();
        return index >= 0 && index < entries.len ? entries[index].view
                                                 : EntityId{};
    }
};

Entity<NavStackState> NavStackStateNew(App* app);

void NavStackPush(NavStackState* s, Ctx* cx, EntityId view, NavMotion motion);

EntityId NavStackPop(NavStackState* s, Ctx* cx, NavMotion motion);

Vec<EntityId> NavStackPopToRoot(NavStackState* s, Ctx* cx, NavMotion motion);

EntityId NavStackForward(NavStackState* s, Ctx* cx, NavMotion motion);

EntityId NavStackReplace(NavStackState* s, Ctx* cx, EntityId view,
                         NavMotion motion);

void NavStackClear(NavStackState* s, Ctx* cx);

struct NavPage {
    EntityId view = {};
    int index = 0;
    PresencePhase phase = PresencePhase::Present;
    NavOperation operation = NavOperation::Push;
    bool hasOperation = false;
    float progress = 1.f;
    El* el = nullptr;

    int Index() const { return index; }
    PresencePhase Phase() const { return phase; }
    bool HasOperation() const { return hasOperation; }
    NavOperation Operation() const { return operation; }
    float Progress() const { return progress; }
};

using NavItemFn = El* (*)(void* user, Ctx* cx, const NavPage& page);

struct NavStack {
    Ctx* cx = nullptr;
    Entity<NavStackState> state = {};
    motion::Transition transition = {};
    bool hasTransition = false;
    NavItemFn item = nullptr;
    void* user = nullptr;

    static NavStack* New(Ctx* cx, Entity<NavStackState> state);

    NavStack* Transition(const motion::Transition& value);

    NavStack* Item(NavItemFn fn, void* user = nullptr);
    El* IntoEl();
};

template <>
struct EventEmitter<NavStackState, NavStackEvent> {};

}

#line 1 "src/base/number_input.h"

namespace gpui {

enum class StepAction : uint8_t {
    Decrement,
    Increment
};

enum class NumberStepKind : uint8_t {
    Fixed,
    ByValue
};

using NumberStepByValueFn = double (*)(double current, StepAction action,
                                       App* app, int64_t arg);

struct NumberStep {
    NumberStepKind kind = NumberStepKind::Fixed;
    double fixed = 1;
    NumberStepByValueFn byValue = nullptr;
    int64_t arg = 0;

    static NumberStep Fixed(double value);
    static NumberStep ByValue(NumberStepByValueFn fn, int64_t arg = 0);
    double Value(double current, StepAction action, App* app) const;
};

enum class NumberInputEventKind : uint8_t {
    Step
};

struct NumberInputEvent {
    NumberInputEventKind kind = NumberInputEventKind::Step;
    StepAction action = StepAction::Increment;
};

TempStr NumberStepValueTemp(Str value, StepAction action, double step,
                            bool hasMin, double min, bool hasMax, double max);

bool NumberParseValue(Str value, double* out);

bool NumberStepForKey(int key, StepAction* out);

bool NumberInputApplyStep(InputState* state, App* app, Window* win,
                          StepAction action, const NumberStep* step,
                          bool hasMin, double min, bool hasMax, double max,
                          bool disabled, Listener onStep = {});

Func0 NumberInputStepCallback(Ctx* cx, InputState* state, StepAction action,
                              const NumberStep* step, bool hasMin, double min,
                              bool hasMax, double max, bool disabled,
                              Listener onStep = {});

void NumberInputEnsureMask(InputState* state);

struct NumberInputText {
    El* root = nullptr;

    static NumberInputText* New(Ctx* cx);
    NumberInputText* Child(El* el);
    El* IntoEl();
};

struct NumberInput {
    static El* New(Ctx* cx, Str id);
    static El* New(Ctx* cx, Str id, InputState* state);

    static El* Compose(Ctx* cx, Str id, InputState* state, bool disabled,
                       El* decrement, El* input, El* increment,
                       bool controlsRight = false, El* children = nullptr);
};
}

#line 1 "src/base/otp_input.h"

namespace gpui {

enum class OtpEventKind : uint8_t {
    Change,
    Complete,
    Focus,
    Blur,
};

struct OtpEvent {
    OtpEventKind kind = OtpEventKind::Change;
};

struct OtpState {
    Entity<OtpState> self = {};

    Vec<char> value;
    int len = 0;
    int length = 6;
    bool masked = false;
    bool disabled = false;
    bool focused = false;

    EntityId blink = {};

    Listener onChange = {};

    FocusHandle focus = {};
};

Str OtpValue(const OtpState* s);

void OtpSetValue(OtpState* s, Str value);

char OtpDigitChar(uint32_t c);

bool OtpEditValue(OtpState* s, int key, uint32_t ch);

void OtpFocus(OtpState* s, App* app, Window* win);
void OtpBlur(OtpState* s, App* app, Window* win);
bool OtpCursorVisible(OtpState* s, App* app);

void OtpKeyDown(OtpState* self, Ctx* cx, const KeyEvent* ev);

void OtpClick(OtpState* self, Ctx* cx, const ClickEvent* ev);

struct OtpInput {
    static El* New(Ctx* cx, Str id = {});

    static El* New(Ctx* cx, Str id, Entity<OtpState> state);
};

template <>
struct EventEmitter<OtpState, OtpEvent> {};
}

#line 1 "src/base/pagination.h"

namespace gpui {

struct PaginationItem {
    int64_t page = 0;
    int64_t from = 0;
    int64_t to = 0;
};

struct PaginationState {
    int64_t currentPage = 1;
    int64_t totalPages = 1;
    int64_t visiblePages = 5;
    bool disabled = false;
};

PaginationState PaginationStateNew(int64_t currentPage, int64_t totalPages);

int64_t PaginationPrevPage(const PaginationState* s);
int64_t PaginationNextPage(const PaginationState* s);

bool PaginationCanRequest(const PaginationState* s, int64_t page);

int PaginationItems(const PaginationState* s, PaginationItem* out, int cap);

struct Pagination {
    static El* New(Ctx* cx, Str id);
};
}

#line 1 "src/base/positioner.h"

namespace gpui {

enum class Align : uint8_t {
    Start,
    Center,
    End
};
using PopupAlign = Align;

struct ResolvedPosition {
    Bounds bounds;
    gpui::Placement placement = gpui::Placement::Top;
    bool hasPlacement = false;
};
using Positioned = ResolvedPosition;

struct PositionerState {};

using PositionerOnPositionFn = void (*)(void* user, ResolvedPosition position);

struct Positioner {
    enum class Strategy : uint8_t {
        Side,
        Corner
    };

    Arena* a = nullptr;
    Strategy strategy = Strategy::Side;
    Bounds trigger = {};
    Anchor anchor = Anchor::TopLeft;
    Point point = {};
    gpui::Placement placement = gpui::Placement::Top;
    bool hasPlacement = false;
    gpui::Align align = gpui::Align::Center;
    float offset = 0;
    float margin = 4;
    bool occlude = false;
    PositionerOnPositionFn onPosition = nullptr;
    void* onPositionUser = nullptr;
    ArenaVec<El*> children;

    static Positioner* Side(Ctx* cx, Bounds trigger);
    static Positioner* Corner(Ctx* cx, Anchor anchor, Point point);

    Positioner* Position(Point value);
    Positioner* Placement(gpui::Placement value);
    Positioner* Align(gpui::Align value);
    Positioner* Offset(float value);

    Positioner* Occlude();
    Positioner* Margin(float value);

    Positioner* OnPosition(PositionerOnPositionFn fn, void* user);
    Positioner* Child(El* child);
    El* IntoEl();
};

constexpr float kPopupMargin = 4.f;

ResolvedPosition PositionSide(Bounds trigger, Size popup, Size view,
                              float margin, const Placement* preferred,
                              Align align, float offset);

ResolvedPosition PositionCorner(Anchor anchor, Point at, Size popup, Size view,
                                float margin);
ResolvedPosition PositionCorner(Anchor anchor, Point at, Size popup, Size view,
                                Edges margin);

Edges PositionerFrameInsets(bool clientDecorated, Tiling tiling,
                            float clientInset);

}

#line 1 "src/base/popup.h"

namespace gpui {

using PopupAnchor = Anchor;

constexpr int kPopupPriority = 100;
constexpr float kPopupWindowMargin = 8.f;

Point PopupResolvedCorner(PopupAnchor anchor, Bounds triggerBounds);

El* PopupPlaceContent(El* content, PopupAnchor anchor, float offset = 0);

using PopupOnPositionFn = void (*)(void* user, ResolvedPosition position,
                                   Bounds trigger);

struct Popup {
    Arena* a = nullptr;
    El* root = nullptr;

    PopupAnchor anchor = PopupAnchor::TopLeft;

    float offset = 0;
    PopupOnPositionFn onPosition = nullptr;
    void* onPositionUser = nullptr;

    bool contentReady = false;

    static Popup* New(Ctx* cx, Str id, El* trigger,
                      PopupAnchor anchor = PopupAnchor::TopLeft);
    Popup* Anchor(PopupAnchor a);

    Popup* AnchorRight(bool on = true);
    Popup* Offset(float offset);

    Popup* OnPosition(PopupOnPositionFn fn, void* user);
    Popup* Content(El* content);
    El* IntoEl();
};
}

#line 1 "src/base/popover.h"

namespace gpui {

struct PopoverOpenChangeEvent {
    bool open = false;
};

struct PopoverState {

    EntityId self = {};
    bool open = false;

    bool seeded = false;

    FocusHandle focus = {};
    FocusHandle trackedFocus = {};

    FocusHandle previousFocus = {};

    Listener onOpenChange = {};

    Listener onDismiss = {};
};

void PopoverInitKeys();

void PopoverSetOpenFocused(PopoverState* s, Ctx* cx, bool open);

bool PopoverIsOpen(Ctx* cx, Entity<PopoverState> state);
void PopoverSetOpen(Ctx* cx, Entity<PopoverState> state, bool open);

void PopoverToggle(PopoverState* self, Ctx* cx, const MouseDownEvent* ev,
                   int64_t button);
void PopoverConfirm(PopoverState* self, Ctx* cx, const ActionEvent* ev);
void PopoverDismiss(PopoverState* self, Ctx* cx, const ClickEvent* ev);
void PopoverDismissOnMouseDown(PopoverState* self, Ctx* cx,
                               const MouseDownEvent* ev);

struct Popover {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* trigger = nullptr;
    El* content = nullptr;
    Entity<PopoverState> state = {};
    FocusHandle focus = {};
    MouseButton button = MouseButton::Left;
    bool overlayClosable = true;

    PopupAnchor anchor = PopupAnchor::TopLeft;

    float offset = 0;
    PopupOnPositionFn onPosition = nullptr;
    void* onPositionUser = nullptr;

    static Popover* New(Ctx* cx, Str id, Entity<PopoverState> state = {},
                        MouseButton button = MouseButton::Left);
    Popover* Anchor(PopupAnchor v);
    Popover* Offset(float offset);

    Popover* OnPosition(PopupOnPositionFn fn, void* user);

    Popover* TrackedFocus(FocusHandle tracked);
    Popover* OverlayClosable(bool closable);
    Popover* OnOpenChange(Listener fn);

    Popover* OnDismiss(Listener fn);
    Popover* Trigger(El* trigger);
    Popover* Content(El* content);
    El* IntoEl();
};
}

#line 1 "src/base/progress.h"

namespace gpui {

float ProgressClampValue(float value);

struct Progress {
    static El* New(Ctx* cx, Str id, float value = 0, bool indeterminate = false,
                   Str accessibilityLabel = {});
};

struct ProgressTrack {
    static El* New(Ctx* cx);
};

struct ProgressIndicator {
    static El* New(Ctx* cx);
};
}

#line 1 "src/base/questionnaire.h"

namespace gpui {

struct QuestionnaireValidationContext;

using QuestionnaireValidator =
    bool (*)(const QuestionnaireValidationContext* context, Str* error);

struct QuestionnaireChoiceDefinition {
    Str value = {};
    Str accessibilityLabel = {};

    Str description = {};
    bool disabled = false;
    bool defaultSelected = false;

    static QuestionnaireChoiceDefinition New(Str value, Str accessibilityLabel);
    QuestionnaireChoiceDefinition WithDescription(Str description) const;
    QuestionnaireChoiceDefinition WithDisabled(bool disabled) const;
    QuestionnaireChoiceDefinition WithDefaultSelected(bool selected) const;

    Str Value() const { return value; }
    Str AccessibilityLabel() const { return accessibilityLabel; }
    Str Description() const { return description; }
    bool IsDisabled() const { return disabled; }
    bool IsDefaultSelected() const { return defaultSelected; }
};

struct QuestionnaireInputDefinition {
    InputState* state = nullptr;
    Str accessibilityLabel = {};
    bool disabled = false;

    static QuestionnaireInputDefinition New(InputState* state,
                                            Str accessibilityLabel);
    QuestionnaireInputDefinition WithDisabled(bool disabled) const;

    InputState* State() const { return state; }
    Str AccessibilityLabel() const { return accessibilityLabel; }
    bool IsDisabled() const { return disabled; }
};

struct QuestionnaireItemDefinition {
    Str name = {};
    Str accessibilityLabel = {};
    Str description = {};
    bool required = false;
    bool multiple = false;
    bool disabled = false;
    Vec<QuestionnaireChoiceDefinition> choices;
    QuestionnaireInputDefinition input = {};
    bool hasInput = false;
    QuestionnaireValidator validator = nullptr;

    static QuestionnaireItemDefinition New(Str name, Str accessibilityLabel);
    QuestionnaireItemDefinition WithDescription(Str description) const;
    QuestionnaireItemDefinition WithRequired(bool required) const;
    QuestionnaireItemDefinition WithMultiple(bool multiple) const;
    QuestionnaireItemDefinition WithDisabled(bool disabled) const;
    QuestionnaireItemDefinition WithChoices(
        const QuestionnaireChoiceDefinition* values, int n) const;
    QuestionnaireItemDefinition WithChoice(
        const QuestionnaireChoiceDefinition& choice) const;
    QuestionnaireItemDefinition WithInput(
        const QuestionnaireInputDefinition& input) const;
    QuestionnaireItemDefinition WithValidator(
        QuestionnaireValidator validator) const;

    Str Name() const { return name; }
    Str AccessibilityLabel() const { return accessibilityLabel; }
    Str Description() const { return description; }
    bool IsRequired() const { return required; }
    bool IsMultiple() const { return multiple; }
    bool IsDisabled() const { return disabled; }
    const Vec<QuestionnaireChoiceDefinition>& Choices() const {
        return choices;
    }

    const QuestionnaireInputDefinition* Input() const {
        return hasInput ? &input : nullptr;
    }
    QuestionnaireValidator Validator() const { return validator; }
};

enum class QuestionnaireItemStatus : uint8_t {
    Unanswered,
    Answered,
    Skipped
};

enum class QuestionnaireShortcutMode : uint8_t {
    Letters,
    Numbers
};

struct QuestionnaireAnswer {
    const Str* choices = nullptr;
    int nChoices = 0;
    Str freeform = {};

    static QuestionnaireAnswer New() { return {}; }

    QuestionnaireAnswer WithChoices(Arena* a, const Str* values, int n) const;

    QuestionnaireAnswer WithFreeform(Str value) const;

    int ChoicesLen() const { return nChoices; }
    Str ChoiceAt(int i) const { return choices[i]; }
    Str Freeform() const { return freeform; }
    bool HasFreeform() const { return len(freeform) > 0; }
    bool IsEmpty() const { return nChoices == 0 && !HasFreeform(); }
    bool HasChoice(Str value) const;
};

bool operator==(const QuestionnaireAnswer& a, const QuestionnaireAnswer& b);
inline bool operator!=(const QuestionnaireAnswer& a,
                       const QuestionnaireAnswer& b) {
    return !(a == b);
}

struct QuestionnaireAnswerEntry {
    Str name = {};
    QuestionnaireAnswer answer = {};
};

struct QuestionnaireAnswers {
    const QuestionnaireAnswerEntry* entries = nullptr;
    int n = 0;

    const QuestionnaireAnswer* Get(Str name) const;
    int Len() const { return n; }
    bool IsEmpty() const { return n == 0; }
};

struct QuestionnaireProgressState {
    int current = 0;
    int total = 0;

    int Current() const { return current; }
    int Total() const { return total; }
};

struct QuestionnaireItemState {
    Str name = {};
    QuestionnaireItemStatus status = QuestionnaireItemStatus::Unanswered;
    bool required = false;
    bool multiple = false;
    bool disabled = false;
    bool invalid = false;
    bool hasInput = false;

    Str Name() const { return name; }
    QuestionnaireItemStatus Status() const { return status; }
    bool IsRequired() const { return required; }
    bool IsMultiple() const { return multiple; }
    bool IsDisabled() const { return disabled; }
    bool IsInvalid() const { return invalid; }
    bool HasInput() const { return hasInput; }
};

struct QuestionnaireChoiceState {
    Str value = {};
    bool selected = false;
    bool disabled = false;
    bool invalid = false;

    Str shortcut = {};

    Str Value() const { return value; }
    bool IsSelected() const { return selected; }
    bool IsDisabled() const { return disabled; }
    bool IsInvalid() const { return invalid; }
    Str Shortcut() const { return shortcut; }
};

struct QuestionnaireNavigationState {
    bool previousVisible = false;
    bool nextVisible = false;
    bool skipVisible = false;
    bool submitVisible = false;
    bool confirmable = false;

    bool IsPreviousVisible() const { return previousVisible; }
    bool IsNextVisible() const { return nextVisible; }
    bool IsSkipVisible() const { return skipVisible; }
    bool IsSubmitVisible() const { return submitVisible; }
    bool IsConfirmable() const { return confirmable; }
};

struct QuestionnaireValidationContext {
    Str item = {};
    QuestionnaireAnswer answer = {};
    QuestionnaireAnswers answers = {};

    Str Item() const { return item; }
    const QuestionnaireAnswer& Answer() const { return answer; }
    const QuestionnaireAnswers& Answers() const { return answers; }
};

struct QuestionnaireAnswerChange {
    Str item = {};
    QuestionnaireAnswer answer = {};
    QuestionnaireItemStatus status = QuestionnaireItemStatus::Unanswered;

    Str Item() const { return item; }
    const QuestionnaireAnswer& Answer() const { return answer; }
    QuestionnaireItemStatus Status() const { return status; }
};

struct QuestionnaireSubmissionItem {
    Str name = {};
    QuestionnaireItemStatus status = QuestionnaireItemStatus::Unanswered;
    QuestionnaireAnswer answer = {};

    Str Name() const { return name; }
    QuestionnaireItemStatus Status() const { return status; }
    const QuestionnaireAnswer& Answer() const { return answer; }
};

struct QuestionnaireSubmission {
    const QuestionnaireSubmissionItem* items = nullptr;
    int n = 0;

    const QuestionnaireSubmissionItem* Items() const { return items; }
    int Len() const { return n; }

    const QuestionnaireAnswer* Answer(Str name) const;
};

enum class QuestionnaireEventKind : uint8_t {
    CurrentItemChanged,
    AnswerChanged,
    Completed,
    Submit
};

struct QuestionnaireEvent {
    QuestionnaireEventKind kind = QuestionnaireEventKind::CurrentItemChanged;

    Str previous = {};
    Str current = {};

    QuestionnaireAnswerChange change = {};

    QuestionnaireSubmission submission = {};
};

enum class QuestionnaireValidationErrorKind : uint8_t {
    Required,
    Unanswered,
    Message
};

struct QuestionnaireValidationError {
    QuestionnaireValidationErrorKind kind =
        QuestionnaireValidationErrorKind::Required;
    Str message = {};

    static QuestionnaireValidationError Required() {
        return {QuestionnaireValidationErrorKind::Required, {}};
    }
    static QuestionnaireValidationError Unanswered() {
        return {QuestionnaireValidationErrorKind::Unanswered, {}};
    }
    static QuestionnaireValidationError Message(Str message) {
        return {QuestionnaireValidationErrorKind::Message, message};
    }

    Str MessageText() const {
        return kind == QuestionnaireValidationErrorKind::Message ? message
                                                                 : Str{};
    }
};

bool operator==(const QuestionnaireValidationError& a,
                const QuestionnaireValidationError& b);

enum class QuestionnaireSchemaErrorKind : uint8_t {
    Ok,
    DuplicateItem,
    DuplicateChoice,
    MultipleDefaultsForSingleItem,
    UnknownItem,
    UnknownChoice,
    AnswerDoesNotMatchItem
};

struct QuestionnaireSchemaError {
    QuestionnaireSchemaErrorKind kind = QuestionnaireSchemaErrorKind::Ok;
    Str item = {};
    Str choice = {};

    bool IsOk() const { return kind == QuestionnaireSchemaErrorKind::Ok; }
    bool IsError() const { return !IsOk(); }
};

bool operator==(const QuestionnaireSchemaError& a,
                const QuestionnaireSchemaError& b);

TempStr QuestionnaireSchemaErrorMessageTemp(const QuestionnaireSchemaError& e);

struct QuestionnaireItemRuntime {
    bool disabled = false;
    bool* choiceDisabled = nullptr;
    bool inputDisabled = true;

    bool* selected = nullptr;
    Str freeform = {};
    bool* initialSelected = nullptr;
    Str initialFreeform = {};
    Str initialInputValue = {};
    bool hasInitialInputValue = false;
    bool skipped = false;
    bool validationAttempted = false;
    QuestionnaireValidationError internalError = {};
    bool hasInternalError = false;
    QuestionnaireValidationError externalError = {};
    bool hasExternalError = false;
    FocusHandle focus = {};
    FocusHandle* choiceFocus = nullptr;
    FocusHandle inputFocus = {};
};

struct QuestionnaireState {
    Arena* arena = nullptr;
    QuestionnaireItemDefinition* items = nullptr;
    QuestionnaireItemRuntime* runtime = nullptr;
    int nItems = 0;
    int current = -1;
    Str initialCurrent = {};
    QuestionnaireShortcutMode shortcutMode = QuestionnaireShortcutMode::Letters;
    bool hasShortcutMode = false;
    bool complete = false;

    bool pendingConfirm = false;
    uint32_t pendingConfirmGen = 0;
    int pendingConfirmTimer = 0;
    FocusHandle focus = {};

    Entity<QuestionnaireState> self = {};

    ~QuestionnaireState();

    static QuestionnaireSchemaError ValidateSchema(
        const QuestionnaireItemDefinition* items, int n);

    QuestionnaireSchemaError WithCurrentItem(Str name);
    void WithShortcuts(QuestionnaireShortcutMode mode);

    Str CurrentItem() const;

    int CurrentIx() const;
    int Total() const;
    QuestionnaireProgressState Progress() const;
    const QuestionnaireItemDefinition* ItemDefinition(Str name) const;
    const QuestionnaireChoiceDefinition* ChoiceDefinition(Str item,
                                                          Str value) const;
    bool ItemState(Str name, QuestionnaireItemState* out) const;
    bool ChoiceState(Str item, Str value, QuestionnaireChoiceState* out) const;

    bool ChoicePosition(Str item, Str value, int* position, int* total) const;
    QuestionnaireNavigationState NavigationState() const;
    bool Answer(Str name, Arena* a, QuestionnaireAnswer* out) const;
    QuestionnaireAnswers Answers(Arena* a) const;

    const QuestionnaireValidationError* Error(Str name) const;
    bool IsComplete() const { return complete; }
    InputState* InputStateOf(Str name) const;
    FocusHandle GetFocusHandle() const { return focus; }
    const FocusHandle* ItemFocusHandle(Str name) const;
    const FocusHandle* ChoiceFocusHandle(Str item, Str value) const;
    bool IsCurrentInputFocused(const Window* win) const;

    bool CurrentInputHasText() const;

    Str FocusedCurrentChoice(const Window* win) const;
    bool ShortcutMode(QuestionnaireShortcutMode* out) const;

    Str ShortcutForChoice(Str item, Str value) const;
    Str ChoiceForShortcut(Str item, Str key) const;
    bool ActivateShortcut(Str key, Ctx* cx);

    QuestionnaireSchemaError SetCurrentItem(Str name, Ctx* cx);
    QuestionnaireSchemaError SetAnswer(Str item,
                                       const QuestionnaireAnswer& answer,
                                       Ctx* cx);
    QuestionnaireSchemaError SetInputValue(Str item, Str value, Ctx* cx);
    QuestionnaireSchemaError SetItemDisabled(Str name, bool disabled, Ctx* cx);
    QuestionnaireSchemaError SetChoiceDisabled(Str item, Str value,
                                               bool disabled, Ctx* cx);
    QuestionnaireSchemaError SetExternalError(Str item, Str error, Ctx* cx);
    QuestionnaireSchemaError ClearExternalError(Str item, Ctx* cx);
    void Reset(Ctx* cx);

    QuestionnaireSchemaError ActivateChoice(Str item, Str value, Ctx* cx);

    QuestionnaireSchemaError Choose(Str item, Str value, Ctx* cx);
    bool ConfirmCurrent(Ctx* cx);
    bool GoPrevious(Ctx* cx);
    bool GoNext(Ctx* cx);
    bool SkipCurrent(Ctx* cx);
    bool Submit(Ctx* cx);
    bool FocusCurrentItem(Ctx* cx) const;
    bool FocusInvalidItem(Str item, Ctx* cx) const;
    bool FocusChoice(Str item, Str value, Ctx* cx) const;
    bool FocusInput(Str item, Ctx* cx) const;
    bool FocusPreviousAnswer(Ctx* cx);
    bool FocusNextAnswer(Ctx* cx);
    bool MoveCurrentRadio(int direction, Ctx* cx);

    static void OnInputChange(QuestionnaireState* self, Ctx* cx,
                              const InputEvent* ev, int64_t itemIx);

    static void OnPendingConfirm(QuestionnaireState* self, Ctx* cx,
                                 const TickEvent* ev, int64_t packed);
};

QuestionnaireSchemaError QuestionnaireStateNew(
    App* app, const QuestionnaireItemDefinition* items, int n,
    Entity<QuestionnaireState>* out);

template <>
struct EventEmitter<QuestionnaireState, QuestionnaireEvent> {};

struct QuestionnaireChoiceControl {
    enum class Kind : uint8_t {
        Checkbox,
        Radio
    };
    Kind kind = Kind::Radio;
    El* el = nullptr;

    static bool New(Ctx* cx, Entity<QuestionnaireState> state, Str item,
                    Str value, Str id, QuestionnaireChoiceControl* out);
};

void QuestionnaireHandleKeyDown(Entity<QuestionnaireState> state, KeyEvent* ev,
                                Ctx* cx);

}

#line 1 "src/base/radio.h"

namespace gpui {

struct RadioStyles {
    StateStyle checked = {};
    StateStyle disabled = {};

    RadioStyles& Checked(const StateStyle& style);
    RadioStyles& Disabled(const StateStyle& style);
};

struct Radio {
    static El* New(Ctx* cx, Str id, bool checked = false, bool disabled = false,
                   Listener onChange = {}, const RadioStyles* styles = nullptr,
                   const StateStyle* instance = nullptr);
};
}

#line 1 "src/base/radio_group.h"

namespace gpui {

struct RadioGroup {
    static El* New(Ctx* cx, Str id, Axis axis = Axis::Horizontal);
};
}

#line 1 "src/base/root.h"

namespace gpui {

struct Root;

struct RootPlugin {

    void* (*build)(Window* window, App* app) = nullptr;
    void (*drop)(void* state) = nullptr;

    void (*prepare)(void* state, Ctx* cx) = nullptr;

    void (*style)(void* state, El* surface, Ctx* cx) = nullptr;

    El* (*decorate)(void* state, El* surface, const Root* root,
                    Ctx* cx) = nullptr;

    El* (*render)(void* state, Ctx* cx) = nullptr;
};

struct RootPluginInstance {
    const RootPlugin* type = nullptr;
    void* state = nullptr;
};

struct Root {
    App* app = nullptr;

    EntityId view = {};

    Style style = {};
    uint32_t styleFields = 0;

    Vec<RootPluginInstance> plugins;
    bool captured = false;

    ~Root();

    static void RegisterPlugin(App* app, const RootPlugin* plugin);

    static Entity<Root> New(App* app, Window* window, EntityId view);

    EntityId View() const { return view; }

    void* Plugin(const RootPlugin* type) const;

    static Root* Read(Window* window);

    Root* Refine(const Style& s, uint32_t fields);

    static El* Render(Root* self, Ctx* cx);
};

El* RootSurface(Ctx* cx, Root* root, El* content);

Window* KitOpenWindow(App* app, Str title, int dipW, int dipH, EntityId content,
                      WinOpts opts);

int KitRunView(Str title, int dipW, int dipH, EntityId content, App* app,
               WinOpts opts);

}

#line 1 "src/base/scrollable_mask.h"

namespace gpui {

struct ScrollableMask {
    Arena* a = nullptr;
    Axis axis = Axis::Vertical;
    El* element = nullptr;
    Str id = {};
    bool debug = false;

    static ScrollableMask* New(Ctx* cx, Axis axis, El* element);
    static El* Apply(El* element, Axis axis);
    ScrollableMask* Id(Str v);
    ScrollableMask* Debug(bool v = true);
    El* IntoEl();
};

El* HorizontalScrollArea(Ctx* cx, Str id, El* viewport);

}

#line 1 "src/base/scrollbar.h"

namespace gpui {

enum class ScrollbarEntrance : uint8_t;
struct ScrollbarMotion;

struct ScrollbarHandle {
    void* user = nullptr;
    Bounds (*viewportBounds)(void* user) = nullptr;
    Point (*offset)(void* user) = nullptr;
    void (*setOffset)(void* user, Point value) = nullptr;
    Size (*contentSize)(void* user) = nullptr;
    void (*startDrag)(void* user) = nullptr;
    void (*endDrag)(void* user) = nullptr;

    bool IsValid() const {
        return user && viewportBounds && offset && setOffset && contentSize;
    }
    Bounds ViewportBounds() const {
        return viewportBounds ? viewportBounds(user) : Bounds{};
    }
    Point Offset() const { return offset ? offset(user) : Point{}; }
    void SetOffset(Point value) const {
        if (setOffset) {
            setOffset(user, value);
        }
    }
    Size ContentSize() const {
        return contentSize ? contentSize(user) : Size{};
    }
    void StartDrag() const {
        if (startDrag) {
            startDrag(user);
        }
    }
    void EndDrag() const {
        if (endDrag) {
            endDrag(user);
        }
    }
};

struct ScrollbarTrackStyle {
    Background background = {};
    Rgba border = {};
    float width = 0;
    bool hasBackground = false;
    bool hasBorder = false;
    bool hasWidth = false;

    ScrollbarTrackStyle Bg(Background value) const {
        ScrollbarTrackStyle out = *this;
        out.background = value;
        out.hasBackground = true;
        return out;
    }
    ScrollbarTrackStyle BorderColor(Rgba value) const {
        ScrollbarTrackStyle out = *this;
        out.border = value;
        out.hasBorder = true;
        return out;
    }
    ScrollbarTrackStyle Width(float value) const {
        ScrollbarTrackStyle out = *this;
        out.width = value;
        out.hasWidth = true;
        return out;
    }
};

struct ScrollbarThumbStyle {
    Background background = {};
    float width = 0;
    float inset = 0;
    float radius = 0;
    float minLength = 0;
    bool hasBackground = false;
    bool hasWidth = false;
    bool hasInset = false;
    bool hasRadius = false;
    bool hasMinLength = false;

    ScrollbarThumbStyle Bg(Background value) const {
        ScrollbarThumbStyle out = *this;
        out.background = value;
        out.hasBackground = true;
        return out;
    }
    ScrollbarThumbStyle Width(float value) const {
        ScrollbarThumbStyle out = *this;
        out.width = value;
        out.hasWidth = true;
        return out;
    }
    ScrollbarThumbStyle Inset(float value) const {
        ScrollbarThumbStyle out = *this;
        out.inset = value;
        out.hasInset = true;
        return out;
    }
    ScrollbarThumbStyle Radius(float value) const {
        ScrollbarThumbStyle out = *this;
        out.radius = value;
        out.hasRadius = true;
        return out;
    }
    ScrollbarThumbStyle MinLength(float value) const {
        ScrollbarThumbStyle out = *this;
        out.minLength = value;
        out.hasMinLength = true;
        return out;
    }
};

struct ScrollbarStyles {
    ScrollbarTrackStyle track = {};
    ScrollbarTrackStyle trackHover = {};
    ScrollbarTrackStyle trackActive = {};
    ScrollbarThumbStyle thumb = {};
    ScrollbarThumbStyle thumbHover = {};
    ScrollbarThumbStyle thumbActive = {};

    ScrollbarStyles Track(ScrollbarTrackStyle value) const {
        ScrollbarStyles out = *this;
        out.track = value;
        return out;
    }
    ScrollbarStyles TrackHover(ScrollbarTrackStyle value) const {
        ScrollbarStyles out = *this;
        out.trackHover = value;
        return out;
    }
    ScrollbarStyles TrackActive(ScrollbarTrackStyle value) const {
        ScrollbarStyles out = *this;
        out.trackActive = value;
        return out;
    }
    ScrollbarStyles Thumb(ScrollbarThumbStyle value) const {
        ScrollbarStyles out = *this;
        out.thumb = value;
        return out;
    }
    ScrollbarStyles ThumbHover(ScrollbarThumbStyle value) const {
        ScrollbarStyles out = *this;
        out.thumbHover = value;
        return out;
    }
    ScrollbarStyles ThumbActive(ScrollbarThumbStyle value) const {
        ScrollbarStyles out = *this;
        out.thumbActive = value;
        return out;
    }
};

struct ScrollbarThumbGeometry {
    float origin = 0;
    float inset = 0;
    float length = 0;
    float travel = 0;
    float extent = 0;

    float Start(float offset) const;
    float DragOffset(float position, float grab, float current) const;
};

ScrollbarThumbGeometry ScrollbarGeometry(float origin, float container,
                                         float content, float marginEnd,
                                         float inset, float minLength);

float ScrollbarThumbSize(float track, float container, float content);
float ScrollbarThumbSize(float track, float container, float content,
                         float minLength);

float ScrollbarThumbPos(float track, float thumb, float offset, float container,
                        float content);
float ScrollbarThumbPos(float track, float thumb, float offset, float container,
                        float content, float marginEnd);

float ScrollbarOffsetForTrackPress(float pos, float trackOrigin, float track,
                                   float thumb, float container, float content);

float ScrollbarOffsetForDrag(float pos, float grab, float trackOrigin,
                             float track, float thumb, float container,
                             float content);
float ScrollbarOffsetForDrag(float pos, float grab, float trackOrigin,
                             float track, float thumb, float container,
                             float content, float marginEnd);

enum class ScrollbarAxis : uint8_t {
    Vertical,
    Horizontal,
    Both
};

using ScrollAxis = ScrollbarAxis;

struct AxisPrepaintState {
    Axis axis = Axis::Vertical;

    Bounds barHitbox = {};
    Bounds bounds = {};
    float radius = 0;
    Rgba bg = {};
    Rgba border = {};
    Bounds thumbBounds = {};
    Bounds thumbFillBounds = {};
    Background thumbBg = {};
    float scrollSize = 0;
    float containerSize = 0;
    float thumbSize = 0;
    float marginEnd = 0;
    float trackWidth = 0;
    float visibilityOpacity = 0;
    float visibilityPosition = 0;
    bool visibilityRequested = false;
};

struct PrepaintState {
    Bounds hitbox = {};

    int scrollbarStateId = 0;
    AxisPrepaintState states[2] = {};
    int statesLen = 0;
};

AxisPrepaintState ScrollbarPrepaintAxis(Axis axis, Bounds track, float offset,
                                        float containerSize, float contentSize,
                                        const ScrollbarThumbStyle& style);

struct Scrollbar {

    static El* New(Ctx* cx);

    static El* New(Ctx* cx, Str id, float scrollY, float scrollX,
                   Listener onScroll, ScrollAxis axis = ScrollAxis::Vertical);
    static El* New(Ctx* cx, Str id, float scrollY, float scrollX,
                   Listener onScroll, ScrollAxis axis, ScrollbarMode mode);

    static El* Apply(Ctx* cx, El* element, Str id, float scrollY, float scrollX,
                     Listener onScroll, ScrollAxis axis = ScrollAxis::Vertical);
    static El* Apply(Ctx* cx, El* element, Str id, float scrollY, float scrollX,
                     Listener onScroll, ScrollAxis axis, ScrollbarMode mode);

    static El* ApplyStyles(Ctx* cx, El* element, const ScrollbarStyles& styles);

    static El* Vertical(Ctx* cx, Str id, float scrollY, Listener onScroll);
    static El* Vertical(Ctx* cx, Str id, float scrollY, Listener onScroll,
                        ScrollbarMode mode);
};
}

#line 1 "src/base/scroll_bounce.h"

namespace gpui {

struct ScrollBounceMotion {
    float tracking = 0.55f;
    float responseMs = 524.f;

    ScrollBounceMotion WithTracking(float value) const {
        ScrollBounceMotion out = *this;
        if (value > 0 && value == value) out.tracking = value;
        return out;
    }
    ScrollBounceMotion WithResponse(float ms) const {
        ScrollBounceMotion out = *this;
        out.responseMs = std::max(0.f, ms);
        return out;
    }
};

struct ScrollBouncePrepaintState {
    float offset = 0;
    float velocity = 0;
};

struct ScrollBouncePhysics {
    float position = 0;
    float velocity = 0;
    bool dragging = false;
    bool suppressMomentum = false;
    float extent = 0;
    ScrollBounceMotion motion = {};

    float Offset() const;
    void Begin(float viewportExtent);
    float Pull(float delta);
    void Release();
    bool Step(float seconds);
};

const float kCatchDragSlop = 8.f;

struct ScrollBounceCatch {
    float distance = 0;
    bool tracking = false;

    bool Observe(TouchPhase phase, float deltaY);
};

const double kMomentumGapSeconds = 0.25;

struct ScrollBounceSuppression {

    double lastWheelAt = -1;

    float direction = 0;

    void Release(ScrollBouncePhysics* physics, bool fromRest);

    bool Lifts(double now, float deltaY);
};

struct ScrollBounce {
    Ctx* cx = nullptr;
    Str id = {};
    El* child = nullptr;
    ScrollBounceMotion motion = {};
    bool enabled = false;
    Listener onScroll = {};

    static ScrollBounce* New(Ctx* cx, Str id, El* child);
    ScrollBounce* Enabled(bool value = true);
    ScrollBounce* Motion(ScrollBounceMotion value);
    ScrollBounce* OnScroll(Listener listener);
    El* IntoEl();
};

}

#line 1 "src/base/text_selection.h"

namespace gpui {

struct TouchHandleHitbox {
    SelectionEdge edge = SelectionEdge::Start;
    Bounds bounds = {};
};

struct TouchHandleLayout {
    Vec<TouchHandleHitbox> hitboxes;
    ~TouchHandleLayout() { VecReset(hitboxes); }
};

struct TextSelectionScopeId {
    uint64_t raw = 0;

    static TextSelectionScopeId New();
    static TextSelectionScopeId FromRaw(uint64_t value) { return {value}; }
    uint64_t Value() const { return raw; }
    int RuntimeScope() const { return (int)(raw & 0x7fffffffU); }
};

inline bool operator==(TextSelectionScopeId a, TextSelectionScopeId b) {
    return a.raw == b.raw;
}
inline bool operator!=(TextSelectionScopeId a, TextSelectionScopeId b) {
    return !(a == b);
}

struct TextSelectionContentKey {
    uint64_t raw = 0;

    static TextSelectionContentKey New(uint64_t value) { return {value}; }
    uint64_t Value() const { return raw; }
};

inline bool operator==(TextSelectionContentKey a, TextSelectionContentKey b) {
    return a.raw == b.raw;
}

enum class TextSelectionCoverage : uint8_t {
    Bounded,
    FromStart,
    ToEnd,
    Full
};

struct TextSelectionEndpoint {
    EntityId entity = {};
    Point point = {};
    TextSelectionContentKey contentKey = {};
    bool hasEntity = false;
    bool hasContentKey = false;

    static TextSelectionEndpoint New(EntityId entity, Point point);
    static TextSelectionEndpoint At(Point point);
    TextSelectionEndpoint WithContentKey(TextSelectionContentKey value) const;
    EntityId Entity() const { return entity; }
    Point ContentPoint() const { return point; }
};

struct TextSelectionWindowPoints {
    Point anchor = {};
    Point cursor = {};

    static TextSelectionWindowPoints New(Point anchor, Point cursor) {
        return {anchor, cursor};
    }
    Point Anchor() const { return anchor; }
    Point Cursor() const { return cursor; }
};

struct TextSelectionSnapshot {
    TextSelectionEndpoint anchor = {};
    TextSelectionEndpoint cursor = {};
    TextSelectionWindowPoints windowPoints = {};
    TextSelectionCoverage coverage = TextSelectionCoverage::Bounded;
    bool selecting = false;
    bool hasWindowPoints = false;

    static TextSelectionSnapshot New(TextSelectionEndpoint anchor,
                                     TextSelectionEndpoint cursor);
    TextSelectionSnapshot WithSelecting(bool value) const;
    TextSelectionSnapshot WithWindowPoints(
        TextSelectionWindowPoints value) const;
    TextSelectionSnapshot WithCoverage(TextSelectionCoverage value) const;
    TextSelectionEndpoint Anchor() const { return anchor; }
    TextSelectionEndpoint Cursor() const { return cursor; }
    bool IsSelecting() const { return selecting; }
    TextSelectionCoverage Coverage() const { return coverage; }
};

struct TextSelectionRegistration {
    Bounds hitbox = {};
    Bounds bounds = {};
    Point scrollOffset = {};
    TextSelectionScopeId scope = {};
    uint64_t documentOrder = 0;
    const Bounds* textBounds = nullptr;
    int textBoundsCount = 0;

    static TextSelectionRegistration New(Bounds hitbox, Bounds bounds);
    TextSelectionRegistration WithScrollOffset(Point value) const;
    TextSelectionRegistration WithScope(TextSelectionScopeId value) const;
    TextSelectionRegistration WithDocumentOrder(uint64_t value) const;
    TextSelectionRegistration WithTextBounds(const Bounds* values,
                                             int count) const;
};

struct TextSelectionRun {
    uint64_t documentOrder = 0;
    Str text = {};
    TextLayout* layout = nullptr;
    Bounds bounds = {};

    static TextSelectionRun New(Str text, TextLayout* layout, Bounds bounds);
    TextSelectionRun WithDocumentOrder(uint64_t value) const;
};

struct TextSelectionRange {
    int start = 0;
    int end = 0;
    bool selected = false;
};

enum class TextSelectionBand {
    Misses,
    Covers,
    Partial
};
TextSelectionBand TextSelectionBandFor(float rowsTop, float rowsBottom,
                                       Point selectionStart,
                                       Point selectionEnd);

struct TextSelectionProjection {
    Vec<TextSelectionRange> ranges;
    bool active = false;

    int Len() const { return len(ranges); }
    const TextSelectionRange* Ranges() const { return ranges.els; }
    bool IsActive() const { return active; }
    void Reset() { VecReset(ranges); }
};

enum class TextSelectionEventKind : uint8_t {
    SelectionChanged,
    AutoScroll,
    Cleared
};

struct TextSelectionEvent {
    TextSelectionEventKind kind = TextSelectionEventKind::SelectionChanged;
    TextSelectionSnapshot snapshot = {};
    float autoScroll = 0;
    bool hasSnapshot = false;
    bool hasAutoScroll = false;
};

using TextSelectionFocusFn = void (*)(void* user, Window* window, App* app);
using TextSelectionClearFn = void (*)(void* user, App* app);
using TextSelectionCopyFn = int (*)(void* user, App* app, char* out, int cap);
using TextSelectionContentKeyFn = bool (*)(void* user, Point point,
                                           const App* app,
                                           TextSelectionContentKey* out);

struct TextSelectionParticipantState;

template <>
struct EventEmitter<TextSelectionParticipantState, TextSelectionEvent> {};

struct TextSelectionHandle {
    gpui::Entity<TextSelectionParticipantState> state = {};

    static TextSelectionHandle New(Str fallbackCopyText, App* app);
    EntityId Entity() const { return state.id; }
    bool Snapshot(const App* app, TextSelectionSnapshot* out) const;
    void SetFallbackCopyText(Str text, App* app) const;
    void SetLocalSelection(bool active, App* app) const;
    bool HasLocalSelection(const App* app) const;
    void Register(TextSelectionRegistration registration, Window* window,
                  App* app) const;
    TextSelectionProjection UpdateRuns(const TextSelectionRun* runs, int count,
                                       App* app) const;
    Subscription RefreshWindowOnChange(App* app) const;
    void FocusWith(TextSelectionFocusFn fn, void* user, App* app) const;
    void ClearWith(TextSelectionClearFn fn, void* user, App* app) const;
    void CopyWith(TextSelectionCopyFn fn, void* user, App* app) const;
    void ResolveContentKeyWith(TextSelectionContentKeyFn fn, void* user,
                               App* app) const;

    template <typename S>
    Subscription Subscribe(Ctx* cx,
                           void (*fn)(S*, Ctx*,
                                      const TextSelectionEvent*)) const {
        return gpui::Subscribe(cx, state, fn);
    }
};

TextSelectionProjection TextSelectionProjectRanges(
    const TextSelectionSnapshot* snapshot, const TextSelectionRun* runs,
    int count);

void TextSelectionHandleSetSnapshot(const TextSelectionHandle& handle, App* app,
                                    const TextSelectionSnapshot* snapshot);

struct TextSelectionGesture {
    bool selecting = false;
    bool didHitText = false;
};

void TextSelectionBegin(TextSelectionGesture* g, bool insideText);

void TextSelectionExtend(TextSelectionGesture* g, bool insideText);

void TextSelectionEnd(TextSelectionGesture* g);

bool TextSelectionPublishes(const TextSelectionGesture* g);

void TextSelectionClear(TextSelectionGesture* g);

struct TextSelection {

    static El* New(Ctx* cx, Str id, int clickId = 0);
    static int SelectedText(Window* window, App* app, char* out, int cap);
    static bool HasSelection(Window* window, const App* app);
    static void Clear(Window* window, App* app);
    static void ClearForWindow(Window* window, App* app);
    static void End(Window* window, App* app);
    static void ActivateScope(TextSelectionScopeId scope, Window* window,
                              App* app);
};

struct TextSelectionLayerPrepaintState {
    WindowSelection* selection = nullptr;
};

struct TextSelectionLayer {
    static El* New(Ctx* cx);
};

El* TextSelectionScope(El* element, TextSelectionScopeId scope);

bool CurrentTextSelectionScope(const Window* win, TextSelectionScopeId* out);

struct TextSelectionScopeGuard {
    Window* win = nullptr;

    TextSelectionScopeGuard(Window* window, TextSelectionScopeId scope);
    ~TextSelectionScopeGuard();
    TextSelectionScopeGuard(const TextSelectionScopeGuard&) = delete;
    TextSelectionScopeGuard& operator=(const TextSelectionScopeGuard&) = delete;
};

template <typename F>
auto WithTextSelectionScope(Window* win, TextSelectionScopeId scope, F&& fn)
    -> decltype(fn()) {
    TextSelectionScopeGuard guard(win, scope);
    return fn();
}

struct WindowSelection {
    TextSelectionGesture gesture;

    int anchor = -1;
    int cursor = -1;

    int scope = 0;
    TextSelectionScopeId activeScope = {};
    Point anchorPoint = {};
    Point cursorPoint = {};
    bool hasWindowPoints = false;

    bool anchorInsideText = false;
    EntityId anchorParticipant = {};
    bool publishing = false;
    bool clearing = false;
    uint64_t frameGeneration = 0;

    uint64_t layerFrame = 0;

    Vec<EntityId> participants;

    SelectionFormat format = SelectionFormat::Plain;

    bool touchMenuOpen = false;
    TouchEdgeDrag touchEdgeDrag = {};
    bool hasTouchEdgeDrag = false;
    Bounds touchUi = {};
    bool hasTouchUi = false;
    bool touchUiThisFrame = false;
};

bool WindowSelectionTouchSnapshot(Window* win, TouchSelectionSnapshot* out);
void WindowSelectionCloseEditMenu(Window* win);
void WindowSelectionRegisterTouchUi(Window* win, Bounds bounds);
bool WindowSelectionTouchUiContains(const Window* win, Point at);

WindowSelection* WindowSelectionOf(Window* win);
void WindowSelectionFree(Window* win);

void WindowSelectionPress(Window* win, float x, float y, int clickCount,
                          bool extend);

bool WindowSelectionLongPressStart(Window* win, float x, float y);

void WindowSelectionDrag(Window* win, float x, float y);

void WindowSelectionRelease(Window* win);

bool WindowSelectionHas(const Window* win);

void WindowSelectionClear(Window* win);

int WindowSelectionText(Window* win, char* out, int cap);
int WindowSelectionTextAs(Window* win, char* out, int cap, SelectionFormat fmt);

int WindowSelectionTextForEntity(Window* win, EntityId owner, char* out,
                                 int cap, SelectionFormat fmt);
bool WindowSelectionHasEntity(const Window* win, EntityId owner);
void WindowSelectionSelectAll(Window* win, EntityId owner);

void WindowSelectionSelectAllTouched(Window* win);

void WindowSelectionSetFormat(Window* win, SelectionFormat fmt);
SelectionFormat WindowSelectionFormat(Window* win);

bool WindowSelectionCopy(Window* win);

void WindowSelectionApply(Window* win);

void WindowSelectionFinishFrame(Window* win);

bool WindowSelectionLayerLive(const Window* win);
}

#line 1 "src/base/selectable_text.h"

namespace gpui {

struct SelectableText {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str text = {};
    TextSelectionHandle handle = {};
    bool hasHandle = false;
    uint64_t documentOrder = 0;

    float font = 0;
    Rgba color = {};
    bool hasColor = false;
    int weight = 0;

    Rgba selectionColor = {};
    bool hasSelectionColor = false;

    static SelectableText* New(Ctx* cx, Str id, Str text);
    static SelectableText* WithHandle(Ctx* cx, Str id,
                                      TextSelectionHandle handle, Str text);

    SelectableText* DocumentOrder(uint64_t order);
    SelectableText* TextStyle(float fontSize, Rgba textColor);
    SelectableText* Font(float fontSize);
    SelectableText* Semibold();
    SelectableText* SelectionColor(Rgba value);
    El* IntoEl();
};

int SelectionQuadBounds(Point start, Point end, Bounds bounds, float lineHeight,
                        Bounds* out);

}

#line 1 "src/base/sheet.h"

namespace gpui {

enum class SheetOverlayPress : uint8_t {

    Ignore,

    Swallow,

    Close
};

SheetOverlayPress SheetOverlayPressAction(bool overlayInteractive,
                                          bool overlayClosable,
                                          MouseButton button, float pressY,
                                          bool hasDismissBefore,
                                          float dismissBeforeY);

bool SheetClosesOnKey(int key);

void SheetInitKeys();
Str SheetContext();

struct SheetState {
    bool overlayInteractive = true;
    bool overlayClosable = true;
    bool hasDismissBefore = false;
    float dismissBeforeY = 0;
    Listener requestClose = {};
    Listener onClose = {};

    void Close(Ctx* cx);
    static void OnOverlay(SheetState* self, Ctx* cx, const MouseDownEvent* ev);
    static void OnAction(SheetState* self, Ctx* cx, const ActionEvent* ev);
};

struct Sheet {
    Ctx* cx = nullptr;
    El* root = nullptr;

    Str trap = {};
    El* overlay = nullptr;
    El* surface = nullptr;
    bool overlayInteractive = true;
    bool overlayClosable = true;
    bool hasDismissBefore = false;
    float dismissBeforeY = 0;
    Listener requestClose = {};
    Listener onClose = {};

    static Sheet* New(Ctx* cx);
    Sheet* Trap(Str name);
    Sheet* Overlay(El* element);
    Sheet* Surface(El* element);
    Sheet* OverlayInteractive(bool interactive);
    Sheet* OverlayClosable(bool closable);
    Sheet* DismissBeforeY(float y);
    Sheet* RequestClose(Listener handler);
    Sheet* OnClose(Listener handler);
    El* IntoEl();
};
}

#line 1 "src/base/slider.h"

namespace gpui {

struct Slider {
    static El* New(Ctx* cx, SliderState* state = nullptr,
                   Axis axis = Axis::Horizontal, int clickId = 0);
};

struct SliderTrack {
    static El* New(Ctx* cx, SliderState* state = nullptr,
                   Axis axis = Axis::Horizontal);
};

struct SliderIndicator {
    static El* New(Ctx* cx, SliderState* state = nullptr);
};
struct SliderThumb {
    static El* New(Ctx* cx);
};
}

#line 1 "src/base/theme_tokens.h"

namespace gpui {

struct ColorTokens {
    Rgba background = {};
    Rgba foreground = {};
    Rgba surface = {};
    Rgba surfaceForeground = {};
    Rgba primary = {};
    Rgba primaryForeground = {};
    Rgba secondary = {};
    Rgba secondaryForeground = {};
    Rgba muted = {};
    Rgba mutedForeground = {};
    Rgba accent = {};
    Rgba accentForeground = {};
    Rgba destructive = {};
    Rgba destructiveForeground = {};
    Rgba border = {};
    Rgba input = {};
    Rgba ring = {};

    Rgba selection = {};

    ColorTokens();

    static ColorTokens Light();
    static ColorTokens Dark();

  private:

    struct Empty {};
    explicit ColorTokens(Empty) {}
};

bool operator==(const ColorTokens& a, const ColorTokens& b);
inline bool operator!=(const ColorTokens& a, const ColorTokens& b) {
    return !(a == b);
}

struct RadiusTokens {
    float none = 0;
    float sm = 3;
    float md = 6;
    float lg = 8;
    float xl = 12;
    float full = 9999;
};

struct SpacingTokens {
    float xxs = 2;
    float xs = 4;
    float sm = 8;
    float md = 12;
    float lg = 16;
    float xl = 24;
    float xxl = 32;
};

struct TextStyleToken {
    float size = 16;
    float lineHeight = 24;
    FontWeight weight = FontWeight::Normal;
};

struct TypographyTokens {
    Str sans = Str(".SystemUIFont");
    Str mono = Str(PlatMonoFontName());
    TextStyleToken xs = {12, 16, FontWeight::Normal};
    TextStyleToken sm = {14, 20, FontWeight::Normal};
    TextStyleToken md = {16, 24, FontWeight::Normal};
    TextStyleToken lg = {18, 28, FontWeight::Normal};
    TextStyleToken xl = {20, 28, FontWeight::Normal};
    TextStyleToken monoMd = {13, 20, FontWeight::Normal};
};

using SemanticShadow = BoxShadow;

struct ShadowTokens {
    Vec<BoxShadow> sm;
    Vec<BoxShadow> md;
    Vec<BoxShadow> lg;

    ShadowTokens() = default;
    static ShadowTokens Elevations(Rgba color);
};

const float kMonoFontSize = 13.f;

struct SemanticThemeTokens {
    ColorTokens colors = {};
    RadiusTokens radius = {};
    SpacingTokens spacing = {};
    TypographyTokens typography = {};
    ShadowTokens shadow;

    SemanticThemeTokens() = default;
};

using SemanticColorTokens = ColorTokens;
using SemanticRadiusTokens = RadiusTokens;
using SemanticSpacingTokens = SpacingTokens;
using SemanticTextStyle = TextStyleToken;
using SemanticTypographyTokens = TypographyTokens;
using SemanticShadowTokens = ShadowTokens;

SemanticShadowTokens SemanticShadowElevations(Rgba color);
const BoxShadow* ShadowFirst(const Vec<BoxShadow>& level);
BoxShadow* ShadowFirst(Vec<BoxShadow>& level);

}

#line 1 "src/base/styled.h"

namespace gpui {

inline El* HFlex(Arena* a) {
    return Div(a)->FlexRow()->ItemsCenter();
}

inline El* VFlex(Arena* a) {
    return Div(a)->FlexCol();
}

inline BoxShadow box_shadow(float x, float y, float blur, float spread,
                            Hsla color) {
    BoxShadow out;
    out.x = x;
    out.y = y;
    out.blur = blur;
    out.spread = spread;
    out.color = HslaToRgba(color);
    out.inset = false;
    return out;
}

enum class RoleOverrideKind : uint8_t {
    Implicit,
    Presentational,
    Role
};

struct RoleOverride {
    RoleOverrideKind kind = RoleOverrideKind::Implicit;
    AccessibilityRole role = AccessibilityRole::None;

    static RoleOverride Implicit();
    static RoleOverride Presentational();
    static RoleOverride Explicit(AccessibilityRole role);
    bool Resolve(AccessibilityRole defaultRole, AccessibilityRole* out) const;
};

struct StyledExt {
    static El* RefineStyle(El* element, const Style& style, uint32_t fields);

    static El* HFlex(El* element);

    static El* VFlex(El* element);
    static El* Paddings(El* element, Edges paddings);
    static El* Margins(El* element, Edges margins);
    static El* DebugRed(El* element);
    static El* DebugBlue(El* element);
    static El* DebugYellow(El* element);
    static El* DebugGreen(El* element);
    static El* DebugPink(El* element);
    static El* DebugFocused(El* element, FocusHandle focus,
                            const Window* window);
    static El* FontThin(El* element);
    static El* FontExtraLight(El* element);
    static El* FontLight(El* element);
    static El* FontNormal(El* element);
    static El* FontMedium(El* element);
    static El* FontSemibold(El* element);
    static El* FontBold(El* element);
    static El* FontExtraBold(El* element);
    static El* FontBlack(El* element);
    static El* CornerRadii(El* element, Corners radius);
};

const char* const* StyledExtReflectionMethods(int* count);

inline RoleOverride RoleOverride::Implicit() {
    return {};
}

inline RoleOverride RoleOverride::Presentational() {
    RoleOverride out;
    out.kind = RoleOverrideKind::Presentational;
    return out;
}

inline RoleOverride RoleOverride::Explicit(AccessibilityRole value) {
    RoleOverride out;
    out.kind = RoleOverrideKind::Role;
    out.role = value;
    return out;
}

inline bool RoleOverride::Resolve(AccessibilityRole defaultRole,
                                  AccessibilityRole* out) const {
    if (kind == RoleOverrideKind::Presentational) {
        return false;
    }
    if (out) {
        *out = kind == RoleOverrideKind::Role ? role : defaultRole;
    }
    return true;
}

inline El* StyledExt::RefineStyle(El* element, const Style& style,
                                  uint32_t fields) {
    return element ? element->Refine(style, fields) : nullptr;
}

inline El* StyledExt::HFlex(El* element) {
    return element ? element->FlexRow()->ItemsCenter() : nullptr;
}

inline El* StyledExt::VFlex(El* element) {
    return element ? element->FlexCol() : nullptr;
}

inline El* StyledExt::Paddings(El* element, Edges value) {
    return element ? element->PadL(value.left)
                         ->PadR(value.right)
                         ->PadT(value.top)
                         ->PadB(value.bottom)
                   : nullptr;
}

inline El* StyledExt::Margins(El* element, Edges value) {
    return element ? element->MarginL(value.left)
                         ->MarginR(value.right)
                         ->MarginT(value.top)
                         ->MarginB(value.bottom)
                   : nullptr;
}

inline El* StyledDebug(El* element, float h, float s, float l) {
#if defined(DEBUG) || !defined(NDEBUG)
    return element ? element->Border(1, HslaToRgba(HslaNew(h / 360.f, s / 100.f,
                                                           l / 100.f, 1.f)))
                   : nullptr;
#else
    (void)h;
    (void)s;
    (void)l;
    return element;
#endif
}

inline El* StyledExt::DebugRed(El* element) {
    return StyledDebug(element, 0.f, 72.2f, 50.6f);
}
inline El* StyledExt::DebugBlue(El* element) {
    return StyledDebug(element, 217.2f, 91.2f, 59.8f);
}
inline El* StyledExt::DebugYellow(El* element) {
    return StyledDebug(element, 47.9f, 95.8f, 53.1f);
}
inline El* StyledExt::DebugGreen(El* element) {
    return StyledDebug(element, 142.1f, 70.6f, 45.3f);
}
inline El* StyledExt::DebugPink(El* element) {
    return StyledDebug(element, 330.4f, 81.2f, 60.4f);
}

inline El* StyledExt::DebugFocused(El* element, FocusHandle focus,
                                   const Window* window) {
#if defined(DEBUG) || !defined(NDEBUG)
    return FocusHandleContainsFocused(window, focus) ? DebugBlue(element)
                                                     : element;
#else
    (void)focus;
    (void)window;
    return element;
#endif
}

inline El* StyledExt::FontThin(El* element) {
    return element ? element->Weight(FontWeight::Thin) : nullptr;
}
inline El* StyledExt::FontExtraLight(El* element) {
    return element ? element->Weight(FontWeight::ExtraLight) : nullptr;
}
inline El* StyledExt::FontLight(El* element) {
    return element ? element->Weight(FontWeight::Light) : nullptr;
}
inline El* StyledExt::FontNormal(El* element) {
    return element ? element->Weight(FontWeight::Normal) : nullptr;
}
inline El* StyledExt::FontMedium(El* element) {
    return element ? element->Weight(FontWeight::Medium) : nullptr;
}
inline El* StyledExt::FontSemibold(El* element) {
    return element ? element->Weight(FontWeight::Semibold) : nullptr;
}
inline El* StyledExt::FontBold(El* element) {
    return element ? element->Weight(FontWeight::Bold) : nullptr;
}
inline El* StyledExt::FontExtraBold(El* element) {
    return element ? element->Weight(FontWeight::ExtraBold) : nullptr;
}
inline El* StyledExt::FontBlack(El* element) {
    return element ? element->Weight(FontWeight::Black) : nullptr;
}

inline El* StyledExt::CornerRadii(El* element, Corners radius) {
    return element
               ? element->Corners(radius.tl, radius.tr, radius.br, radius.bl)
               : nullptr;
}

inline const char* const* StyledExtReflectionMethods(int* count) {
    static const char* methods[] = {
        "refine_style",    "h_flex",     "v_flex",         "paddings",
        "margins",         "debug_red",  "debug_blue",     "debug_yellow",
        "debug_green",     "debug_pink", "debug_focused",  "font_thin",
        "font_extralight", "font_light", "font_normal",    "font_medium",
        "font_semibold",   "font_bold",  "font_extrabold", "font_black",
        "corner_radii",
    };
    if (count) {
        *count = (int)(sizeof(methods) / sizeof(methods[0]));
    }
    return methods;
}

}

#line 1 "src/base/switch.h"

namespace gpui {

struct SwitchStyles {
    StateStyle checked = {};
    StateStyle disabled = {};

    SwitchStyles& Checked(const StateStyle& style);
    SwitchStyles& Disabled(const StateStyle& style);
};

struct SwitchTrackStyles {
    StateStyle checked = {};
    StateStyle disabled = {};

    SwitchTrackStyles& Checked(const StateStyle& style);
    SwitchTrackStyles& Disabled(const StateStyle& style);
};

struct SwitchThumbStyles {
    StateStyle checked = {};
    StateStyle disabled = {};

    SwitchThumbStyles& Checked(const StateStyle& style);
    SwitchThumbStyles& Disabled(const StateStyle& style);
};

struct Switch {
    static El* New(Ctx* cx, Str id, bool checked = false, bool disabled = false,
                   Listener onChange = {}, const SwitchStyles* styles = nullptr,
                   const StateStyle* instance = nullptr,
                   Str accessibilityLabel = {}, int tabIndex = 0,
                   bool tabStop = true, FocusHandle focus = {});
};

struct SwitchTrack {
    static El* New(Ctx* cx, Str id, bool checked = false, bool disabled = false,
                   const SwitchTrackStyles* styles = nullptr,
                   const StateStyle* instance = nullptr);
};

struct SwitchThumb {
    static El* New(Ctx* cx, bool checked = false, bool disabled = false,
                   const SwitchThumbStyles* styles = nullptr,
                   const StateStyle* instance = nullptr);
};
}

#line 1 "src/base/table.h"

namespace gpui {

struct Table {
    static El* New(Ctx* cx, Str id, int rowCount = -1, int columnCount = -1,
                   Str accessibilityLabel = {});
};
struct TableHeader {
    static El* New(Ctx* cx, Str id);
};
struct TableBody {
    static El* New(Ctx* cx, Str id);
};
struct TableRow {
    static El* New(Ctx* cx, Str id, int rowIndex = 0);
};
struct TableHead {
    static El* New(Ctx* cx, Str id, int columnIndex = 0);
};
struct TableCell {
    static El* New(Ctx* cx, Str id, int columnIndex = 0);
};
struct TableCaption {
    static El* New(Ctx* cx, Str id);
};
}

#line 1 "src/base/tabs.h"

namespace gpui {

struct TabStyles {
    StateStyle selected = {};
    StateStyle disabled = {};

    TabStyles& Selected(const StateStyle& style);
    TabStyles& Disabled(const StateStyle& style);
};

struct Tabs {
    static El* New(Ctx* cx, Str id);
};

struct Tab {
    static El* New(Ctx* cx, Str id, bool disabled = false,
                   Listener onClick = {}, bool selected = false,
                   Str accessibilityLabel = {}, int positionInSet = 0,
                   int sizeOfSet = 0, const TabStyles* styles = nullptr,
                   const StateStyle* instance = nullptr);
};
}

#line 1 "src/base/sankey.h"

namespace gpui {

enum class SankeyAlign : uint8_t {
    Left,
    Right,
    Center,
    Justify
};

enum class SankeyValueScale : uint8_t {
    Linear,
    Sqrt
};

enum class SankeyError : uint8_t {
    None,

    MissingNode,
    CircularLink
};

struct SankeyLink {
    int source = 0;
    int target = 0;
    double value = 0;
};

struct SankeyNodeLayout {
    int index = 0;

    double value = 0;

    int depth = 0;
    int height = 0;

    int layer = 0;
    float x0 = 0, x1 = 0, y0 = 0, y1 = 0;

    int srcStart = 0, srcCount = 0;
    int tgtStart = 0, tgtCount = 0;
};

struct SankeyLinkLayout {
    int index = 0;
    int source = 0;
    int target = 0;
    double value = 0;
    float y0 = 0, y1 = 0;

    float width = 0;
    float sourceWidth = 0;
    float targetWidth = 0;
};

struct SankeyGraph {
    Vec<SankeyNodeLayout> nodes;
    Vec<SankeyLinkLayout> links;

    Vec<int> srcLinks;
    Vec<int> tgtLinks;

    int errNode = 0;

    void Reset() {
        VecReset(nodes);
        VecReset(links);
        VecReset(srcLinks);
        VecReset(tgtLinks);
    }
};

int SankeyLayerCount(const SankeyGraph* g);

struct Sankey {
    float nodeWidth = 24.f;
    float nodePadding = 8.f;
    SankeyAlign align = SankeyAlign::Justify;
    int iterations = 6;
    SankeyValueScale valueScale = SankeyValueScale::Linear;

    float x0 = 0, y0 = 0, x1 = 1, y1 = 1;
};

SankeyError SankeyTopology(const Sankey* s, int nodeCount,
                           const SankeyLink* links, int nLinks,
                           SankeyGraph* out);

void SankeyLayoutFrom(const Sankey* s, SankeyGraph* g);

SankeyError SankeyLayout(const Sankey* s, int nodeCount,
                         const SankeyLink* links, int nLinks, SankeyGraph* out);

}

#line 1 "src/base/plot.h"

namespace gpui {

struct Path;

namespace plot {

struct PathCache {
    uint64_t key = 0;
    bool hasKey = false;

    bool Touch(uint64_t value) {
        bool warm = hasKey && key == value;
        key = value;
        hasKey = true;
        return warm;
    }
    bool IsWarm() const { return hasKey; }
};

struct PathCaches {
    Vec<PathCache> slots;

    ~PathCaches() { VecReset(slots); }
    PathCache* Slot(int index);
    void SlotPair(int index, PathCache** first, PathCache** second);
};

struct ShapeKey {
    uint64_t value = 1469598103934665603ull;

    static ShapeKey New(uint64_t extra = 0) {
        ShapeKey key;
        key.U64(extra);
        return key;
    }
    ShapeKey& U64(uint64_t v);
    ShapeKey& PointValue(Point point);
    ShapeKey& Float(float v);
    uint64_t Finish() const { return value; }
};

struct ScaleLinear {
    float domainStart = 0;
    float domainDiff = 0;
    float rangeStart = 0;
    float rangeDiff = 0;

    static ScaleLinear New(const float* domain, int domainN, const float* range,
                           int rangeN);

    bool Tick(float value, float* out) const;
};

struct ScalePoint {
    const float* domain = nullptr;
    int domainLen = 0;
    float rangeStart = 0;
    float rangeTick = 0;

    static ScalePoint New(const float* domain, int domainN, const float* range,
                          int rangeN);

    bool Tick(float value, float* out) const;

    bool TickAt(int index, float* out) const;

    int NearestIndex(float tick) const;
};

struct ScaleBand {
    int domainLen = 0;

    float rangeStart = 0;
    float rangeDiff = 0;
    float avgWidth = 0;
    float paddingInner = 0;
    float paddingOuter = 0;

    float maxBandWidth = -1;

    static ScaleBand New(int domainN, const float* range, int rangeN);

    float BandWidth() const;

    ScaleBand MaxBandWidth(float width) const;

    float Step() const;

    ScaleBand BandCount(int count) const;

    bool Tick(int index, float* out) const;

    int NearestIndex(float tick) const;
};

struct ScaleOrdinal {
    int rangeLen = 0;

    int unknown = -1;

    int Map(int domainIndex) const;
};

const float kPlotTextSize = 10;
const float kPlotTextGap = 2;
const float kPlotTextHeight = kPlotTextSize + kPlotTextGap;

const float kPlotAxisGap = 18;

inline float AxisGutter(float fontSize) {
    return fontSize + kPlotTextGap * 4.f;
}

using Curve = ChartStroke;
using ::gpui::AxisLabelPlacement;

inline Point OriginPoint(float x, float y, Point origin) {
    return Point{x + origin.x, y + origin.y};
}

Path* Polygon(PaintCtx* ctx, const Point* points, int count, Bounds bounds);

enum class PlotTextAlign : uint8_t {
    Left,
    Center,
    Right
};

struct Text {
    Str text = {};
    Point origin = {};
    Rgba color = {};
    float fontSize = kPlotTextSize;
    FontWeight fontWeight = FontWeight::Normal;
    PlotTextAlign align = PlotTextAlign::Left;

    static Text New(Str text, Point origin, Rgba color);
    Text* FontSize(float value);
    Text* Weight(FontWeight value);
    Text* Align(PlotTextAlign value);
};

float MeasureTextWidth(PaintCtx* ctx, Str text, float fontSize);

Str TruncateTextToWidth(PaintCtx* ctx, Arena* arena, Str text, float fontSize,
                        float maxWidth);

struct PlotLabel {
    Arena* a = nullptr;
    ArenaVec<Text> items;

    static PlotLabel New(Arena* arena);
    PlotLabel* Add(const Text& text);
    PlotLabel* AddMany(const Text* text, int count);
    void Paint(PaintCtx* ctx, Bounds bounds) const;
};

enum class AxisLabelSide : uint8_t {
    End,
    Start
};

struct AxisText {
    Str text = {};
    float tick = 0;
    Rgba color = {};
    float fontSize = kPlotTextSize;
    PlotTextAlign align = PlotTextAlign::Left;

    static AxisText New(Str text, float tick, Rgba color);
    AxisText* FontSize(float value);
    AxisText* Align(PlotTextAlign value);
};

struct PlotAxis {
    Arena* a = nullptr;
    bool hasX = false;
    float x = 0;
    ArenaVec<AxisText> xLabels;
    bool xAxis = true;
    AxisLabelSide xLabelSide = AxisLabelSide::End;
    bool hasY = false;
    float y = 0;
    ArenaVec<AxisText> yLabels;
    bool yAxis = false;
    AxisLabelSide yLabelSide = AxisLabelSide::End;
    Background stroke = {};

    static PlotAxis New(Arena* arena);

    PlotAxis* X(float value);

    PlotAxis* ShowXAxis(bool value);

    PlotAxis* XLabel(const AxisText* labels, int count);
    PlotAxis* XLabelSide(AxisLabelSide value);
    PlotAxis* Y(float value);

    PlotAxis* ShowYAxis(bool value);
    PlotAxis* YLabel(const AxisText* labels, int count);
    PlotAxis* YLabelSide(AxisLabelSide value);

    PlotAxis* Stroke(Background value);

    ArenaVec<Text> XTexts(Arena* arena, float at) const;
    ArenaVec<Text> YTexts(Arena* arena, float at) const;
    void Paint(PaintCtx* ctx, Bounds bounds) const;
};

Bounds GridLineBounds(Point start, Point end);

void GridDashSegments(Arena* arena, Point start, Point end, const float* dash,
                      int dashCount, ArenaVec<Point>* out);

struct Grid {
    const float* x = nullptr;
    int xCount = 0;
    const float* y = nullptr;
    int yCount = 0;
    Background stroke = {};
    const float* dashArray = nullptr;
    int dashCount = 0;

    static Grid New();
    Grid* X(const float* values, int count);
    Grid* Y(const float* values, int count);

    Grid* Stroke(Background value);
    Grid* DashArray(const float* values, int count);
    void Paint(PaintCtx* ctx, Bounds bounds) const;
};

struct PlotItems {
    const void* data = nullptr;
    int count = 0;
    int stride = 0;

    const void* At(int index) const;
};

typedef bool (*PlotValueFn)(const void* item, int index, void* user,
                            float* out);

struct Line {
    PlotItems items = {};
    PlotValueFn x = nullptr;
    void* xUser = nullptr;
    PlotValueFn y = nullptr;
    void* yUser = nullptr;
    Background stroke = {};
    float strokeWidth = 1;
    ::gpui::plot::Curve curve = ::gpui::plot::Curve::Natural;
    bool dot = false;
    float dotSize = 4;
    Background dotFill = Background(RgbaTransparent());
    bool hasDotStroke = false;
    Rgba dotStroke = {};

    static Line New();
    Line* Data(const void* values, int count, int stride);
    Line* X(PlotValueFn fn, void* user = nullptr);
    Line* Y(PlotValueFn fn, void* user = nullptr);
    Line* Stroke(Background value);
    Line* StrokeWidth(float value);

    Line* Curve(::gpui::plot::Curve value);

    Line* Dots(bool value = true);
    Line* DotSize(float value);

    Line* DotFill(Background value);

    Line* DotStroke(Rgba value);
    int Points(Bounds bounds, Point* out, int capacity) const;
    void Paint(PaintCtx* ctx, Bounds bounds) const;
};

struct Area {
    PlotItems items = {};
    PlotValueFn x = nullptr;
    void* xUser = nullptr;
    bool hasY0 = false;
    float y0 = 0;
    PlotValueFn y1 = nullptr;
    void* y1User = nullptr;
    Background fill = {};
    Background stroke = {};
    ::gpui::plot::Curve curve = ::gpui::plot::Curve::Natural;

    static Area New();
    Area* Data(const void* values, int count, int stride);
    Area* X(PlotValueFn fn, void* user = nullptr);
    Area* Y0(float value);
    Area* Y1(PlotValueFn fn, void* user = nullptr);
    Area* Fill(Background value);
    Area* Stroke(Background value);

    Area* Curve(::gpui::plot::Curve value);
    void Paint(PaintCtx* ctx, Bounds bounds) const;
};

enum class BarAlignment : uint8_t {
    Bottom,
    Top,
    Left,
    Right
};

bool BarAlignmentIsHorizontal(BarAlignment value);
float BarAlignmentGradientAngle(BarAlignment value);
Point BarLabelOrigin(BarAlignment alignment, float cross, float base,
                     float value, float bandWidth);

typedef Background (*PlotBarFillFn)(const void* item, int index, Bounds frame,
                                    BarAlignment alignment, void* user);
typedef void (*PlotBarLabelFn)(const void* item, int index, Point origin,
                               void* user, Vec<Text>* out);

struct Bar {
    PlotItems items = {};
    BarAlignment alignment = BarAlignment::Bottom;
    PlotValueFn cross = nullptr;
    void* crossUser = nullptr;
    float bandWidth = 0;
    PlotValueFn base = nullptr;
    void* baseUser = nullptr;
    PlotValueFn value = nullptr;
    void* valueUser = nullptr;
    PlotBarFillFn fill = nullptr;
    void* fillUser = nullptr;
    PlotBarLabelFn label = nullptr;
    void* labelUser = nullptr;
    Corners cornerRadii = {};

    static Bar New();
    Bar* Data(const void* values, int count, int stride);
    Bar* Alignment(BarAlignment value);
    Bar* Cross(PlotValueFn fn, void* user = nullptr);
    Bar* BandWidth(float value);
    Bar* Base(PlotValueFn fn, void* user = nullptr);
    Bar* Value(PlotValueFn fn, void* user = nullptr);
    Bar* Fill(PlotBarFillFn fn, void* user = nullptr);
    Bar* Label(PlotBarLabelFn fn, void* user = nullptr);
    Bar* CornerRadii(Corners value);
    void Paint(PaintCtx* ctx, Bounds bounds) const;
};

struct ArcData {
    const void* data = nullptr;
    int index = 0;
    float value = 0;
    float startAngle = 0;
    float endAngle = 0;
    float padAngle = 0;

    static ArcData New(const void* data, int index, float value,
                       float startAngle, float endAngle) {
        ArcData out;
        out.data = data;
        out.index = index;
        out.value = value;
        out.startAngle = startAngle;
        out.endAngle = endAngle;
        return out;
    }
};

struct Arc {
    float innerRadius = 0;
    float outerRadius = 0;

    static Arc New();
    Arc* InnerRadius(float value);
    Arc* OuterRadius(float value);
    Point Centroid(const ArcData& arc) const;
    Path* PathFor(PaintCtx* ctx, const ArcData& arc, Bounds bounds) const;

    bool Contains(const ArcData& arc, Point position, Bounds bounds) const;
    void Paint(PaintCtx* ctx, const ArcData& arc, Background fill,
               Bounds bounds) const;

    void PaintCached(PaintCtx* ctx, const ArcData& arc, Background fill,
                     Bounds bounds, PathCache* cache) const;
};

struct Pie {
    PlotValueFn value = nullptr;
    void* valueUser = nullptr;
    float startAngle = 0;
    float endAngle = 2 * kPi;
    float padAngle = 0;

    static Pie New();
    Pie* Value(PlotValueFn fn, void* user = nullptr);
    Pie* StartAngle(float value);
    Pie* EndAngle(float value);
    Pie* PadAngle(float value);
    void Arcs(Arena* arena, PlotItems items, ArenaVec<ArcData>* out) const;
};

struct RadialLine {
    PlotItems items = {};
    PlotValueFn angle = nullptr;
    void* angleUser = nullptr;
    PlotValueFn radius = nullptr;
    void* radiusUser = nullptr;
    bool closed = false;
    bool hasFill = false;
    Background fill = {};
    Background stroke = {};
    float strokeWidth = 1;
    bool dot = false;
    float dotSize = 4;
    Background dotFill = Background(RgbaTransparent());
    bool hasDotStroke = false;
    Rgba dotStroke = {};

    static RadialLine New();
    RadialLine* Data(const void* values, int count, int stride);
    RadialLine* Angle(PlotValueFn fn, void* user = nullptr);
    RadialLine* Radius(PlotValueFn fn, void* user = nullptr);
    RadialLine* Closed(bool value = true);
    RadialLine* Fill(Background value);
    RadialLine* Stroke(Background value);
    RadialLine* StrokeWidth(float value);
    RadialLine* Dots(bool value = true);
    RadialLine* DotSize(float value);

    RadialLine* DotFill(Background value);
    RadialLine* DotStroke(Rgba value);
    int Points(Bounds bounds, Point* out, int capacity) const;
    void Paint(PaintCtx* ctx, Bounds bounds) const;
};

struct StackPoint {
    float y0 = 0;
    float y1 = 0;
    const void* data = nullptr;
};

struct StackSeries {
    Str key = {};
    int index = 0;
    ArenaVec<StackPoint> points;
};

typedef bool (*PlotStackValueFn)(const void* item, int index, Str key,
                                 void* user, float* out);

struct Stack {
    PlotItems items = {};
    const Str* keys = nullptr;
    int keyCount = 0;
    PlotStackValueFn value = nullptr;
    void* valueUser = nullptr;

    static Stack New();
    Stack* Data(const void* values, int count, int stride);
    Stack* Keys(const Str* values, int count);
    Stack* Value(PlotStackValueFn fn, void* user = nullptr);
    void Series(Arena* arena, ArenaVec<StackSeries>* out) const;
};

Path* SankeyLinkPath(PaintCtx* ctx, const SankeyNodeLayout& source,
                     const SankeyNodeLayout& target,
                     const SankeyLinkLayout& link, float minWidth,
                     Point origin);

struct TooltipState {

    int index = 0;

    Point crossLine = {};

    const Point* dots = nullptr;
    int dotCount = 0;

    static TooltipState New(int index, Point crossLine, const Point* dots,
                            int dotCount);
};

struct PlotHover {
    TooltipState state = {};
    float progress = 0;
    bool hovered = false;

    const TooltipState& State() const { return state; }

    float Progress() const { return progress; }

    float Focus() const { return progress; }
    bool IsHovered() const { return hovered; }

    bool IsEntering() const { return hovered && progress == 0.f; }

    float Glide(Ctx* cx, motion::TransitionId id, float target) const;
};

struct PlotMotion {
    Spring pointer = Spring::New(0);
    motion::Transition enter = motion::Transition::New(0);
    motion::Transition exit = motion::Transition::New(0);
    motion::Transition appear = motion::Transition::New(0);

    PlotMotion WithPointer(Spring value) const {
        PlotMotion copy = *this;
        copy.pointer = value;
        return copy;
    }
    PlotMotion WithEnter(motion::Transition value) const {
        PlotMotion copy = *this;
        copy.enter = value;
        return copy;
    }
    PlotMotion WithExit(motion::Transition value) const {
        PlotMotion copy = *this;
        copy.exit = value;
        return copy;
    }

    PlotMotion WithAppear(motion::Transition value) const {
        PlotMotion copy = *this;
        copy.appear = value;
        return copy;
    }
    Spring Pointer() const { return pointer; }
    const motion::Transition& Enter() const { return enter; }
    const motion::Transition& Exit() const { return exit; }
    const motion::Transition& Appear() const { return appear; }
};

struct PlotAppear {

    float time = 1.f;
    Easing easing = Easing::Linear();

    static PlotAppear Complete() { return {}; }

    float Progress() const;

    bool IsAppearing() const { return time < 1.f; }

    float Staggered(int index, int count, float spread) const;
};

struct PlotAppearScope {

    static El* New(Ctx* cx, Str id, El* child);
};

PlotAppear TrackAppear(Ctx* cx, uint64_t generation);

Spring PointerSpring(const App* app);

bool TrackHover(Ctx* cx, const TooltipState* live, const Point* cursor,
                PlotHover* outHover, Point* outCursor);

float HoverProgress(Ctx* cx);

bool IsHoverEntering(Ctx* cx);

}
}

#line 1 "src/base/theme.h"

namespace gpui {

namespace base_theme {

enum class ThemeAppearance : uint8_t {
    Light,
    Dark,
};

struct ScrollbarTheme {
    ScrollbarMode mode = ScrollbarMode::Scrolling;
    ScrollbarMotion motion = {};
    ScrollbarStyles styles = {};

    static ScrollbarTheme New() { return {}; }
    ScrollbarTheme WithMode(ScrollbarMode value) const {
        ScrollbarTheme copy = *this;
        copy.mode = value;
        return copy;
    }
    ScrollbarTheme WithMotion(ScrollbarMotion value) const {
        ScrollbarTheme copy = *this;
        copy.motion = value;
        return copy;
    }
    ScrollbarTheme WithStyles(const ScrollbarStyles& value) const {
        ScrollbarTheme copy = *this;
        copy.styles = value;
        return copy;
    }
    ScrollbarMode Mode() const { return mode; }
    ScrollbarMotion Motion() const { return motion; }
    const ScrollbarStyles& Styles() const { return styles; }
};

struct ResizableTheme {
    Rgba handle = {0, 0, 0, 0};
    Rgba activeHandle = {0, 0, 0, 0};
    bool hasHandle = false;
    bool hasActiveHandle = false;
};

struct PlotTheme {
    plot::PlotMotion motion = {};

    static PlotTheme New() { return {}; }
    PlotTheme WithMotion(const plot::PlotMotion& value) const {
        PlotTheme copy = *this;
        copy.motion = value;
        return copy;
    }
    const plot::PlotMotion& Motion() const { return motion; }
};

struct Theme {
    ThemeAppearance appearance = ThemeAppearance::Light;
    SemanticThemeTokens tokens;
    ScrollbarTheme scrollbar = {};
    ResizableTheme resizable = {};
    PlotTheme plot = {};

    static Theme Global(const App* app);
    static Theme* GlobalMut(App* app);
};

}

using BaseScrollbarTheme = base_theme::ScrollbarTheme;
using BaseResizableTheme = base_theme::ResizableTheme;
using BaseThemeAppearance = base_theme::ThemeAppearance;
using BaseTheme = base_theme::Theme;

BaseTheme* BaseThemeGlobal(App* app);
const BaseTheme* BaseThemeGlobal(const App* app);
void BaseThemeSet(App* app, const BaseTheme& theme);

}

#line 1 "src/markdown/mdast.h"

namespace markdown {

using base::Arena;
using base::ArenaPtr;
using base::ArenaPtrGet;
using base::ArenaPtrOf;
using base::ArenaStr;
using base::ArenaStrGet;
using base::ArenaVec;
using base::kArenaStrNone;
using base::Str;

struct Node;

using ArenaNode = ArenaPtr<Node>;

struct UnistPoint {
    int32_t line = 1;
    int32_t column = 1;
    int32_t offset = 0;
};

struct UnistPosition {
    UnistPoint start = {};
    UnistPoint end = {};
};

UnistPosition GetUnistPosition(Str md, uint32_t start, uint32_t end);

enum class ReferenceKind : uint8_t {
    Shortcut,
    Collapsed,
    Full,
};

enum class AlignKind : uint8_t {
    Left,
    Right,
    Center,
    None,
};

using ArenaAlign = uint32_t;
constexpr ArenaAlign kArenaAlignNone = 0;

static_assert((int)AlignKind::None < 4, "an alignment has to fit in 2 bits");

ArenaAlign ArenaAlignNew(Arena* a, int32_t count);
int32_t ArenaAlignCount(Arena* a, ArenaAlign al);
AlignKind ArenaAlignAt(Arena* a, ArenaAlign al, int32_t i);
void ArenaAlignSet(Arena* a, ArenaAlign al, int32_t i, AlignKind k);

enum class NodeKind : uint8_t {
    Root,
    Blockquote,
    FootnoteDefinition,
    List,
    Toml,
    Yaml,
    Break,
    InlineCode,
    InlineMath,
    Delete,
    Emphasis,
    FootnoteReference,
    Html,
    Image,
    ImageReference,
    Link,
    LinkReference,
    Strong,
    Text,
    Code,
    Math,
    Heading,
    Table,
    ThematicBreak,
    TableRow,
    TableCell,
    ListItem,
    Definition,
    Paragraph,
};

enum class NodeStrKind : uint8_t {

    Value,

    Url,

    Title,

    Alt,

    Identifier,

    Label,

    Lang,
    Meta,

    PerKind,
};

enum NodeFlag : uint8_t {

    NodeHasStart = 1 << 0,

    NodeOrdered = 1 << 1,

    NodeSpread = 1 << 2,

    NodeChecked = 1 << 3,
    NodeHasChecked = 1 << 4,

    NodeRefKindMask = 3 << 6,
};

struct Node {

    ArenaNode lastKid = {};

    ArenaNode sibling = {};

    ArenaStr firstStr = kArenaStrNone;

    NodeKind kind = NodeKind::Root;

    uint8_t flags = 0;

    bool Has(NodeFlag f) const { return (flags & f) != 0; }
    void Set(NodeFlag f, bool on) {
        flags = on ? (uint8_t)(flags | f) : (uint8_t)(flags & ~f);
    }
};

static_assert(sizeof(Node) == 3 * 4 + 4,
              "Node has picked up padding; order the fields largest first");

static_assert(alignof(Node) == 4, "a Node holds nothing wider than a word");

uint32_t NodePerKind(Arena* a, const Node* n);
void NodeSetPerKind(Arena* a, Node* n, uint32_t word);

Str NodeGetStr(Arena* a, const Node* n, NodeStrKind k);

int32_t NodeGetStrLen(Arena* a, const Node* n, NodeStrKind k);

bool NodeHasStr(Arena* a, const Node* n, NodeStrKind k);

void NodeSetStr(Arena* a, Node* n, NodeStrKind k, Str s);

void NodeClearStr(Arena* a, Node* n, NodeStrKind k);

void NodeGrowStr(Arena* a, Node* n, NodeStrKind k, Str more);

inline ReferenceKind NodeRefKind(const Node* n) {
    return (ReferenceKind)((n->flags & NodeRefKindMask) >> 6);
}

inline void NodeSetRefKind(Node* n, ReferenceKind k) {
    n->flags =
        (uint8_t)((n->flags & ~NodeRefKindMask) | (((uint8_t)k & 3) << 6));
}

Node* NodeNew(Arena* a, NodeKind kind);

inline void NodeAddChild(Arena* a, Node* parent, Node* child) {
    ArenaNode at = ArenaPtrOf(a, child);
    if (parent->lastKid.IsSet()) {
        Node* last = ArenaPtrGet(a, parent->lastKid);
        child->sibling = last->sibling;
        last->sibling = at;
    } else {

        child->sibling = at;
    }
    parent->lastKid = at;
}

inline Node* NodeLastChild(Arena* a, const Node* n) {
    return n->lastKid.IsSet() ? ArenaPtrGet(a, n->lastKid) : nullptr;
}

inline ArenaNode NodeFirstKid(Arena* a, const Node* n) {
    Node* last = NodeLastChild(a, n);
    return last ? last->sibling : ArenaNode{};
}

inline Node* NodeFirstChild(Arena* a, const Node* n) {
    ArenaNode first = NodeFirstKid(a, n);
    return first.IsSet() ? ArenaPtrGet(a, first) : nullptr;
}

inline Node* NodeChild(Arena* a, const Node* n, int i) {
    if (i < 0 || !n->lastKid.IsSet()) {
        return nullptr;
    }
    Node* last = ArenaPtrGet(a, n->lastKid);
    Node* at = ArenaPtrGet(a, last->sibling);
    for (int k = 0; k < i; k++) {
        if (at == last) {
            return nullptr;
        }
        at = ArenaPtrGet(a, at->sibling);
    }
    return at;
}

inline int NodeChildCount(Arena* a, const Node* n) {
    if (!n->lastKid.IsSet()) {
        return 0;
    }
    Node* last = ArenaPtrGet(a, n->lastKid);
    int count = 1;
    for (Node* at = ArenaPtrGet(a, last->sibling); at != last;
         at = ArenaPtrGet(a, at->sibling)) {
        count++;
    }
    return count;
}

struct NodeKidsRange {
    Arena* a;
    ArenaNode at;
    ArenaNode last;

    struct Iter {
        Arena* a;
        ArenaNode at;
        ArenaNode last;

        Node* operator*() const { return ArenaPtrGet(a, at); }
        Iter& operator++() {
            at = at == last ? ArenaNode{} : ArenaPtrGet(a, at)->sibling;
            return *this;
        }
        bool operator!=(const Iter& o) const { return at != o.at; }
    };

    Iter begin() const { return Iter{a, at, last}; }
    Iter end() const { return Iter{a, ArenaNode{}, last}; }
};

inline NodeKidsRange NodeKids(Arena* a, const Node* n) {
    return NodeKidsRange{a, NodeFirstKid(a, n), n->lastKid};
}

inline Str NodeStr(Arena* a, ArenaStr s) {
    return ArenaStrGet(a, s);
}

bool NodeHasChildren(NodeKind kind);

Str NodeToString(Arena* a, const Node* node);

}

#line 1 "src/markdown/markdown.h"

namespace markdown {

struct Constructs {
    bool attention = true;
    bool autolink = true;
    bool blockQuote = true;
    bool characterEscape = true;
    bool characterReference = true;
    bool codeIndented = true;
    bool codeFenced = true;
    bool codeText = true;
    bool definition = true;
    bool frontmatter = false;
    bool gfmAutolinkLiteral = false;
    bool gfmFootnoteDefinition = false;
    bool gfmLabelStartFootnote = false;
    bool gfmStrikethrough = false;
    bool gfmTable = false;
    bool gfmTaskListItem = false;
    bool hardBreakEscape = true;
    bool hardBreakTrailing = true;
    bool headingAtx = true;
    bool headingSetext = true;
    bool htmlFlow = true;
    bool htmlText = true;
    bool labelStartImage = true;
    bool labelStartLink = true;
    bool labelEnd = true;
    bool listItem = true;
    bool mathFlow = false;
    bool mathText = false;
    bool thematicBreak = true;

    static Constructs Gfm();
};

struct ParseOptions {
    Constructs constructs = {};
    bool gfmStrikethroughSingleTilde = true;
    bool mathTextSingleDollar = true;

    static ParseOptions Gfm();
};

Node* ToMdast(Arena* a, Str source, const ParseOptions& options);

struct NodeSpan {
    const Node* node = nullptr;
    int32_t start = 0;
    int32_t end = 0;
};

struct NodePositions {
    base::Vec<NodeSpan> spans;
};

Node* ToMdast(Arena* a, Str source, const ParseOptions& options,
              NodePositions* positions);

bool NodePosition(const NodePositions* positions, const Node* n, int32_t* start,
                  int32_t* end);

Str DecodeNamed(Arena* a, Str name);

Str DecodeNumeric(Arena* a, Str value, int radix);

}

#line 1 "src/base/text.h"

namespace gpui {

struct TextView;

struct Span {
    int start = 0;
    int end = 0;
};

struct LinkMark {
    Str url = {};
    Str identifier = {};
    Str title = {};
};

struct TextMark {
    bool bold = false;
    bool italic = false;
    bool strikethrough = false;
    bool underline = false;
    bool code = false;
    Rgba highlight = {};
    LinkMark link = {};
    bool hasHighlight = false;
    bool hasLink = false;

    TextMark& Bold();
    TextMark& Italic();
    TextMark& Strikethrough();
    TextMark& Underline();
    TextMark& Code();
    TextMark& Highlight(Rgba color);
    TextMark& Link(LinkMark value);
    void Merge(const TextMark& other);
};

struct ImageNode {
    Str url = {};
    LinkMark link = {};
    Str title = {};
    Str alt = {};
    float width = 0;
    float height = 0;
    bool hasLink = false;

    Str Title(Arena* a) const;
};

struct MarkdownParseContext {
    Arena* arena = nullptr;
    Str source = {};
    int offset = 0;
    const markdown::NodePositions* positions = nullptr;

    Str Source() const { return source; }
    int Offset() const { return offset; }
    Str NodeSource(const markdown::Node* node) const;
    Str Value(const markdown::Node* node, markdown::NodeStrKind kind) const;
    Str Copy(Str value) const;
};

struct MarkdownNode {
    Str name = {};
    Str text = {};
    Str markdown = {};
    void* data = nullptr;
    Span span = {};
    bool hasSpan = false;

    static MarkdownNode New(Str name, void* data = nullptr);
    MarkdownNode& Text(Str value);
    MarkdownNode& Markdown(Str value);
    Str ToMarkdown() const;
};

struct InlineElement {
    El* element = nullptr;
    float baseline = 0;
    bool hasBaseline = false;

    static InlineElement New(El* element) { return {element, 0, false}; }
    InlineElement& WithBaseline(float px) {
        baseline = px;
        hasBaseline = true;
        return *this;
    }
};

struct InlineRenderContext {
    gpui::Style textStyle = {};
    float fontSize = 0;
    float lineHeight = 0;
    float remSize = 16;
};

using MarkdownBlockParserFn = bool (*)(const markdown::Node* node,
                                       const MarkdownParseContext* context,
                                       void* data, MarkdownNode* out);
using MarkdownBlockRenderFn = El* (*)(Ctx * cx, const MarkdownNode* node,
                                      void* data);
using MarkdownInlineRenderFn =
    InlineElement (*)(Ctx* cx, const MarkdownNode* node,
                      const InlineRenderContext* context, void* data);

struct MarkdownPlugin {
    Str name = {};
    MarkdownBlockParserFn parse = nullptr;
    MarkdownBlockRenderFn render = nullptr;
    MarkdownInlineRenderFn renderInline = nullptr;
    void* data = nullptr;
    bool isBlock = false;
};

struct MarkdownBlockParser {
    MarkdownBlockParserFn fn = nullptr;
    void* data = nullptr;
};

struct MarkdownBlockRenderer {
    Str name = {};
    MarkdownBlockRenderFn fn = nullptr;
    void* data = nullptr;
};

struct MarkdownInlineRenderer {
    Str name = {};
    MarkdownBlockRenderFn render = nullptr;
    MarkdownInlineRenderFn renderInline = nullptr;
    void* data = nullptr;
};

struct MarkdownExtensions {
    ArenaVec<MarkdownBlockParser> blockParsers{};
    ArenaVec<MarkdownBlockRenderer> blockRenderers{};
    ArenaVec<MarkdownBlockParser> inlineParsers{};
    ArenaVec<MarkdownInlineRenderer> inlineRenderers{};
    uint64_t revision = 0;

    uint64_t parserRevision = 0;
    bool enableMdx = false;
    bool enableFrontmatter = false;

    MarkdownExtensions& ParserRevision(uint64_t value);
    MarkdownExtensions& Mdx();
    MarkdownExtensions& Frontmatter();
    MarkdownExtensions& BlockParser(Arena* a, MarkdownBlockParserFn fn,
                                    void* data = nullptr);
    MarkdownExtensions& BlockRenderer(Arena* a, Str name,
                                      MarkdownBlockRenderFn fn,
                                      void* data = nullptr);
    MarkdownExtensions& Plugin(Arena* a, const MarkdownPlugin& plugin);
    const MarkdownBlockRenderer* Renderer(Str name) const;
    const MarkdownInlineRenderer* InlineRenderer(Str name) const;

    bool HasSameParserConfiguration(const MarkdownExtensions& other) const;

    uint64_t ParserFingerprint() const;
};

using TextViewSetupFn = TextView* (*)(TextView * view, void* data);
struct TextViewPlugin {
    TextViewSetupFn setup = nullptr;
    void* data = nullptr;

    TextView* Setup(TextView* view) const;
};

enum MdMark : uint8_t {
    MdBold = 1 << 0,
    MdItalic = 1 << 1,
    MdCode = 1 << 2,
    MdDel = 1 << 3,
    MdUnderline = 1 << 4,
    MdLink = 1 << 5,

    MdHighlight = 1 << 6,
};

struct MdRun {
    Str text = {};

    const SourceSegment* segments = nullptr;
    int segmentCount = 0;

    Span imgSpan = {};
    bool hasImgSpan = false;

    Str href = {};

    Str imgSrc = {};

    float imgW = 0;
    float imgH = 0;
    MarkdownNode custom = {};
    bool hasCustom = false;
    MdRun* next = nullptr;
    uint8_t marks = 0;
};

enum class MdKind : uint8_t {
    Doc,
    Paragraph,
    Heading,
    Quote,
    List,
    Item,
    Code,
    Table,
    Row,
    Cell,
    Rule,

    Html,

    Group,

    Custom,
};

enum MdAlign : uint8_t {
    MdAlignDefault = 0,
    MdAlignLeft = 1,
    MdAlignCenter = 2,
    MdAlignRight = 3,
};

struct MdNode {
    MdKind kind = MdKind::Doc;
    MdNode* parent = nullptr;
    MdNode* first = nullptr;
    MdNode* last = nullptr;
    MdNode* next = nullptr;

    MdRun* runFirst = nullptr;
    MdRun* runLast = nullptr;

    Str lang = {};

    Str raw = {};

    int start = 1;

    uint8_t level = 0;

    uint8_t align = 0;
    bool ordered = false;

    bool head = false;

    bool hasCheck = false;
    bool checked = false;

    int leafStart = -1;
    int leafOrdinal = 0;

    int blockStart = -1;
    MarkdownNode custom = {};
};

using MdPluginNode = MarkdownNode;

using MdPluginParseFn = bool (*)(Ctx* cx, MdNode* node, Str text, void* data,
                                 MdPluginNode* out);

using MdPluginRenderFn = El* (*)(Ctx * cx, const MdPluginNode* node,
                                 void* data);

struct MdPlugin {
    Str name = {};
    MdPluginParseFn parse = nullptr;
    MdPluginRenderFn render = nullptr;
    void* data = nullptr;
};

using CodeBlockActionsFn = El* (*)(Ctx * cx, void* data, Str code, Str lang);

struct TableData {

    const Str* header = nullptr;
    const Str* rows = nullptr;
    int cols = 0;
    int rowCount = 0;
    Str markdown = {};

    Str Cell(int row, int col) const {
        if (row < 0 || col < 0 || col >= cols || row >= rowCount) {
            return {};
        }
        return rows[row * cols + col];
    }
};

using TableActionsFn = El* (*)(Ctx * cx, void* data, const TableData* table);

using ImageSourceFn = gpui::ImageSource (*)(Str uri, void* data);

using HeadingStyleFn = uint32_t (*)(uint8_t level, gpui::Style* style,
                                    void* data);

struct TextViewStyle {

    Rgba foreground = {};
    Rgba mutedForeground = {};

    Rgba link = {};
    Rgba selection = {};

    Rgba codeBackground = {};

    Rgba border = {};

    float paragraphGap = 16;

    HeadingStyleFn heading = nullptr;
    void* headingData = nullptr;
    gpui::Style codeBlock = {};
    uint32_t codeBlockFields = 0;
    gpui::Style table = {};
    uint32_t tableFields = 0;
    gpui::Style tableHead = {};
    uint32_t tableHeadFields = 0;
    gpui::Style tableCell = {};
    uint32_t tableCellFields = 0;
    gpui::Style inlineCode = {};
    uint32_t inlineCodeFields = 0;

    Rgba tableBackground = {};
    bool hasTableBackground = false;
    bool isDark = false;

    static TextViewStyle Default();

    static TextViewStyle FromTheme(const base_theme::Theme& theme);
    static TextViewStyle FromColors(const ColorTokens& colors, bool isDark);

    uint32_t Heading(uint8_t level, gpui::Style* out) const;

    Rgba InlineCodeBackground() const;
    TextViewStyle& WithForeground(Rgba color);
    TextViewStyle& WithMutedForeground(Rgba color);
    TextViewStyle& WithLink(Rgba color);
    TextViewStyle& WithSelection(Rgba color);
    TextViewStyle& WithCodeBackground(Rgba color);
    TextViewStyle& WithBorder(Rgba color);
    TextViewStyle& WithParagraphGap(float gap);

    TextViewStyle& WithHeading(HeadingStyleFn fn, void* data = nullptr);
    TextViewStyle& WithCodeBlock(const gpui::Style& style, uint32_t fields);
    TextViewStyle& WithTable(const gpui::Style& style, uint32_t fields);
    TextViewStyle& WithTableHead(const gpui::Style& style, uint32_t fields);
    TextViewStyle& WithTableCell(const gpui::Style& style, uint32_t fields);
    TextViewStyle& WithInlineCode(const gpui::Style& style, uint32_t fields);
    TextViewStyle& WithDark(bool value);

    TextViewStyle OnTextColor(Rgba color) const;

    bool IsInvertedBy(Rgba color) const;
    bool Equals(const TextViewStyle& other) const;
};

struct CodeBlock {
    Str code = {};
    Str lang = {};

    static CodeBlock FromCode(Str code, Str lang = {});
    Str Code() const { return code; }
    Str Lang() const { return lang; }
};

struct CodeHighlight {
    int start = 0;
    int end = 0;
    Rgba color = {};
};

using CodeBlockHighlighterFn = void (*)(void* data, const CodeBlock* block,
                                        Arena* a, ArenaVec<CodeHighlight>* out);

struct TextViewDefaults {
    TextViewStyle style = {};
    bool hasStyle = false;
    CodeBlockHighlighterFn codeBlockHighlighter = nullptr;
    void* codeBlockHighlighterData = nullptr;
    bool inheritTextColor = false;

    static TextViewDefaults New() { return {}; }
    TextViewDefaults& WithStyle(const TextViewStyle& value);

    TextViewDefaults& WithInheritTextColor(bool inherit);

    bool InheritTextColor() const { return inheritTextColor; }
    TextViewDefaults& WithCodeBlockHighlighter(CodeBlockHighlighterFn fn,
                                               void* data = nullptr);
    void Install(App* app) const;
    static TextViewDefaults Global(const App* app);
    bool HasCodeBlockHighlighter() const {
        return codeBlockHighlighter != nullptr;
    }
};

enum class TextViewFormat : uint8_t {
    Markdown,
    Html
};

struct TextViewMotion {
    float streamFadeMs = 0;
    float streamFadeStaggerMs = 0;
    Easing streamFadeEasing = Easing::EaseOut();

    TextViewMotion WithStreamFade(float ms) const {
        TextViewMotion out = *this;
        out.streamFadeMs = std::max(0.f, ms);
        return out;
    }
    TextViewMotion WithStreamFadeStagger(float ms) const {
        TextViewMotion out = *this;
        out.streamFadeStaggerMs = std::max(0.f, ms);
        return out;
    }
    TextViewMotion WithStreamFadeEasing(Easing easing) const {
        TextViewMotion out = *this;
        out.streamFadeEasing = easing;
        return out;
    }

    float StaggerStepMs(int words) const {
        if (words < 2) {
            return 0;
        }
        if (streamFadeStaggerMs * (float)(words - 1) > streamFadeMs) {
            return 0;
        }
        return streamFadeStaggerMs;
    }
};

struct TextLeafKey {
    int blockStart = 0;
    int ordinal = 0;

    static TextLeafKey Block(int start) { return TextLeafKey{start, 0}; }
    static TextLeafKey TableCell(int start, int ordinal) {
        return TextLeafKey{start, ordinal + 1};
    }
    int BlockStart() const { return blockStart; }

    int CellIx() const { return ordinal - 1; }

    TextLeafKey MovedTo(int start) const { return TextLeafKey{start, ordinal}; }
};
inline bool operator==(TextLeafKey a, TextLeafKey b) {
    return a.blockStart == b.blockStart && a.ordinal == b.ordinal;
}
inline bool operator<(TextLeafKey a, TextLeafKey b) {
    return a.blockStart != b.blockStart ? a.blockStart < b.blockStart
                                        : a.ordinal < b.ordinal;
}

struct StreamFadeSegment {
    TextLeafKey key = {};
    Span range = {};
    double startedAt = 0;
};

struct StreamFadeRange {
    TextLeafKey key = {};
    Span range = {};
    float fadeOut = 0;
};

void StreamFadeUnits(Str text, int start, int end, Vec<Span>* out);

struct RangeHighlight {
    Span range = {};
    Rgba background = {0, 0, 0, 0};

    static RangeHighlight New(Span range, Rgba background) {
        RangeHighlight out;
        out.range = range;
        out.background = background;
        return out;
    }
    static RangeHighlight New(Span range, Hsla background) {
        return New(range, HslaToRgba(background));
    }
    Span Range() const { return range; }
    Rgba Background() const { return background; }
};

enum class RangeHighlightErrorKind : uint8_t {
    None,

    Unsupported,

    InvalidRange,
};

struct RangeHighlightError {
    RangeHighlightErrorKind kind = RangeHighlightErrorKind::None;
    int index = 0;

    static RangeHighlightError Unsupported() {
        return RangeHighlightError{RangeHighlightErrorKind::Unsupported, 0};
    }
    static RangeHighlightError InvalidRange(int index) {
        return RangeHighlightError{RangeHighlightErrorKind::InvalidRange,
                                   index};
    }
    bool IsOk() const { return kind == RangeHighlightErrorKind::None; }

    Str Display(Arena* a) const;
};
inline bool operator==(RangeHighlightError a, RangeHighlightError b) {
    return a.kind == b.kind && a.index == b.index;
}

struct RenderedIndex;
struct RenderedText {
    EntityId owner = {};
    uint64_t revision = 0;
    Str text = {};
    Str source = {};
    const RenderedIndex* index = nullptr;

    Str AsStr() const { return text; }
    int Len() const { return len(text); }
    bool IsEmpty() const { return len(text) == 0; }

    Str Source() const { return source; }

    bool RangeForSource(Span range, Span* out) const;
};
inline bool operator==(const RenderedText& a, const RenderedText& b) {
    return a.owner == b.owner && a.revision == b.revision;
}
inline bool operator!=(const RenderedText& a, const RenderedText& b) {
    return !(a == b);
}

struct RangeBackground {
    Span range = {};
    Rgba color = {0, 0, 0, 0};
};

struct RangeHighlightFrame {
    struct Leaf {
        TextLeafKey key = {};
        int first = 0;
        int count = 0;
    };

    Vec<Leaf> leaves;
    Vec<RangeBackground> backgrounds;

    const RangeBackground* Backgrounds(TextLeafKey key, int* count) const;
};

RenderedIndex* RenderedIndexNew(const MdNode* doc, Str source);
void RenderedIndexFree(RenderedIndex* index);
Str RenderedIndexText(const RenderedIndex* index);
Str RenderedIndexSource(const RenderedIndex* index);
bool RenderedIndexRangeForSource(const RenderedIndex* index, Span source,
                                 Span* out);

int RenderedIndexLeafCount(const RenderedIndex* index);
Span RenderedIndexLeafRange(const RenderedIndex* index, int ix);
const SourceSegment* RenderedIndexSourceMap(const RenderedIndex* index,
                                            int* count);

RangeHighlightError RangeHighlightFrameNew(const RenderedIndex* index,
                                           const RangeHighlight* highlights,
                                           int count,
                                           RangeHighlightFrame** out);

RangeHighlightFrame* RangeHighlightFrameRemap(const RangeHighlightFrame* frame,
                                              const RenderedIndex* prev,
                                              const RenderedIndex* next,
                                              bool tailOnly = false,
                                              int tailStart = -1);
void RangeHighlightFrameFree(RangeHighlightFrame* frame);

struct TextViewReveal {
    bool pending = false;

    bool block = false;
    TextLeafKey key = {};
    int offset = 0;
    int blockIx = 0;

    double requestedAt = 0;
    int attempts = 0;

    Bounds line = {};

    Bounds view = {};
};

struct TextViewRevealEvent {
    Bounds line = {};
};

bool RenderedIndexLocate(const RenderedIndex* index, Span range,
                         TextViewReveal* out);

struct TextViewParse;
struct TextViewParseJob;
struct TextViewBaselineAck;

struct TextViewState {
    EntityId self = {};
    Str text = {};
    TextViewFormat format = TextViewFormat::Markdown;

    const char* elementTextPtr = nullptr;
    int elementTextLen = 0;
    TextViewStyle textViewStyle = {};
    uint64_t revision = 0;
    uint64_t selectionRevision = 0;
    float scrollY = 0;
    bool selectable = false;
    bool scrollable = false;

    int maxLines = -1;

    bool clamped = false;
    gpui::SelectionFormat selectionFormat = gpui::SelectionFormat::Plain;
    TextViewMotion motion = {};

    Vec<StreamFadeSegment> fadeSegments;

    int selectAllAnchor = -1;
    int selectAllCursor = -1;
    bool streamFadePending = false;
    bool streamFadeReplace = false;

    int fadeTick = 0;

    RenderedIndex* renderedIndex = nullptr;
    uint64_t renderedRevision = 0;
    RangeHighlightFrame* rangeHighlights = nullptr;

    TextViewReveal reveal = {};

    TextViewParse* parsed = nullptr;
    TextViewParseJob* parseFlight = nullptr;

    TextViewParse* retiredParses[4] = {};

    bool parseQueued = false;

    bool baselinePending = false;

    TextViewBaselineAck* baselineAck = nullptr;
    uint64_t updateRevision = 0;
    uint64_t fullUpdateRevision = 0;
    uint64_t committedRevision = 0;

    uint64_t parserFingerprint = 0;
    bool parserFrontmatter = false;
    bool parserMdx = false;
    bool parserPlugins = false;

    ~TextViewState();
    static Entity<TextViewState> Markdown(App* app, Str text);
    static Entity<TextViewState> Html(App* app, Str text);

    Str Source() const;

    int ParsedBlockCount() const;
    void SetText(Str value, App* app, Window* window = nullptr);
    void PushStr(Str value, App* app, Window* window = nullptr);
    void SetSelectable(bool value, App* app, Window* window = nullptr);
    void SetScrollable(bool value, App* app, Window* window = nullptr);
    TextViewState& Motion(TextViewMotion value) {
        motion = value;
        return *this;
    }
    void SetMotion(TextViewMotion value, App* app, Window* window = nullptr);
    bool IsClamped() const { return clamped; }
    void SetSelectionFormat(gpui::SelectionFormat value, App* app,
                            Window* window = nullptr);
    int SelectedText(Window* window, char* out, int cap) const;

    bool SelectedSourceRange(const Window* window, Span* out) const;
    bool HasSelection(const Window* window) const;
    void ClearSelection(Window* window, App* app);
    void SelectAll(Window* window, App* app);

    gpui::RenderedText RenderedText() const;

    RangeHighlightError SetRangeHighlights(const RangeHighlight* highlights,
                                           int count, App* app,
                                           Window* window = nullptr);

    void ClearRangeHighlights(App* app, Window* window = nullptr);

    RangeHighlightError RevealRange(Span range, App* app,
                                    Window* window = nullptr);

    void ReconcileRangeHighlights(const MdNode* doc, Str source, double now,
                                  bool tailOnly = false, int tailStart = -1);

    void StartParse(App* app, Window* window,
                    const MarkdownExtensions* extensions = nullptr,
                    bool now = false);

    static void ParseLanded(TextViewParseJob* job);

    static void CommitParsedUpdate(TextViewParseJob* job);

    void RecordStreamFade(const RenderedIndex* prev, const RenderedIndex* next,
                          double now);

    int StreamFadeFrame(Arena* a, double now, StreamFadeRange** out);
    static void OnAction(TextViewState* self, Ctx* cx,
                         const ActionEvent* event);
    static void OnScroll(TextViewState* self, Ctx* cx,
                         const ScrollEvent* event);
    static void OnLineClamp(TextViewState* self, Ctx* cx,
                            const LineClampEvent* event);
    static void OnFadeTick(TextViewState* self, Ctx* cx,
                           const TickEvent* event);

  private:
    void Changed(App* app, Window* window, bool selectionCompatible);
};

struct TextViewLayoutState {
    Entity<TextViewState> state = {};
    El* element = nullptr;
};

Str MdTableToMarkdown(Arena* a, MdNode* table);

int OrderedListOrdinal(int start, int ix);

Str ListItemPrefix(Arena* a, int ix, int start, bool ordered, int depth);

struct TextViewLinkBinding {
    int64_t context = 0;
    const char* href = nullptr;
};

struct TextView {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str source = {};
    Entity<TextViewState> state = {};

    float baseFont = 16;

    float headingFont = 14;

    float codeFont = 13;

    float paragraphGap = 16;

    bool selectable = true;

    bool html = false;

    Listener onLink;
    int64_t onLinkContext = 0;
    bool onLinkHasContext = false;
    CodeBlockActionsFn codeActions = nullptr;

    CodeBlockHighlighterFn codeHighlighter = nullptr;
    void* codeHighlighterData = nullptr;
    TableActionsFn tableActions = nullptr;
    void* tableActionsData = nullptr;
    void* codeActionsData = nullptr;

    ImageSourceFn imageSource = nullptr;
    void* imageSourceData = nullptr;

    ArenaVec<MdPlugin> plugins{};

    float tableColW = 0;

    bool tableScroll = false;

    bool codeBlockScroll = false;

    bool scrollable = false;

    int maxLines = -1;

    int tableIx = 0;
    int codeIx = 0;

    gpui::SelectionFormat selFormat = gpui::SelectionFormat::Plain;
    TextViewStyle textViewStyle = {};

    bool textViewStyleSet = false;

    bool onInvertedSurface = false;
    MarkdownExtensions markdownExtensions = {};
    gpui::Style outerStyle = {};
    uint32_t outerStyleFields = 0;
    TextViewMotion motion = {};
    bool motionSet = false;

    int blockDepth = 0;

    const StreamFadeRange* streamFades = nullptr;
    int nStreamFades = 0;

    const RangeHighlightFrame* rangeHighlights = nullptr;

    const TextViewReveal* revealTarget = nullptr;
    Bounds* revealOut = nullptr;

    Listener onReveal;

    static TextView* New(Ctx* cx, Str source);
    static TextView* NewHtml(Ctx* cx, Str source);

    static TextView* New(Ctx* cx, Entity<TextViewState> state);
    TextView* Font(float px);
    TextView* HeadingFont(float px);
    TextView* Style(const TextViewStyle& style);
    TextView* Refine(const gpui::Style& style, uint32_t fields);
    TextView* Selectable(bool on = true);

    TextView* SelFormat(gpui::SelectionFormat fmt);
    TextView* TableColumnWidth(float px);
    TextView* TableScroll(bool on = true);

    TextView* CodeBlockScroll(bool on = true);
    TextView* Scrollable(bool on = true);

    TextView* MaxLines(int count);
    TextView* ParagraphGap(float px);
    TextView* Motion(TextViewMotion value);

    TextView* StreamFade(bool value = true);

    TextView* OnLink(Listener fn);

    TextView* OnReveal(Listener fn);

    TextView* OnLinkWithContext(Listener fn, int64_t context);

    TextView* CodeBlockActions(CodeBlockActionsFn fn, void* data = nullptr);

    TextView* CodeBlockHighlighter(CodeBlockHighlighterFn fn,
                                   void* data = nullptr);

    TextView* TableActions(TableActionsFn fn, void* data = nullptr);

    TextView* ImageSource(ImageSourceFn fn, void* data = nullptr);

    TextView* Plugin(Str name, MdPluginParseFn parse, MdPluginRenderFn render,
                     void* data = nullptr);
    TextView* MarkdownExtensionsSet(const MarkdownExtensions& extensions);
    TextView* MarkdownBlockParser(MarkdownBlockParserFn parser,
                                  void* data = nullptr);
    TextView* MarkdownBlockRenderer(Str name, MarkdownBlockRenderFn renderer,
                                    void* data = nullptr);
    TextView* Plugin(const MarkdownPlugin& plugin);
    TextView* Plugin(const TextViewPlugin& plugin);
    El* IntoEl();

  private:

    Rgba blockFg = {};
    bool blockFgSet = false;
    Rgba BlockFg() const;

    Str srcLinePre = {};

    Str srcMarker = {};

    Str srcItemMarker = {};

    Str srcItemPad = {};

    bool inTodo = false;

    const SelBlock* srcBlock = nullptr;
    bool srcLineStart = true;

    const SelSource* srcRunLast = nullptr;
    uint8_t srcRunMarks = 0;
    Str srcRunHref = {};

    const SelBlock* SrcOpen(Str marker, Str post, bool join = false);

    void SrcCell(MdNode* row, MdNode* c, int nCols, const uint8_t* colAlign);

    El* SrcMark(El* t, uint8_t marks, Str href = {});

    void SrcBreak();

    El* SrcImage(El* e, MdRun* r);

    El* SrcMap(El* t, const SourceSegment* segments, int count, int offset,
               bool atomic = false);
    Listener LinkListener(Str href);

    Str BlockText(MdNode* n);
    El* PluginBlock(MdNode* n);

    El* ScrollTable(MdNode* n);

    El* Block(MdNode* n, int depth, bool inList, bool isLast);
    El* Blocks(El* into, MdNode* n, int depth, bool inList);
    El* Item(MdNode* n, Str marker, int depth);
    El* Table(MdNode* n);

    El* TableActionsRow(MdNode* n, int nCols, const uint8_t* colAlign);
    El* CodeBlock(MdNode* n);

    El* CodeLines(Str code, const ArenaVec<CodeHighlight>& spans,
                  const SourceSegment* segments, int segmentCount,
                  const MdNode* leaf = nullptr);

    El* ImageRun(MdRun* r, float font, Rgba color, bool inFlow);

    El* Word(Str w, float font, Rgba color, uint8_t marks, int weight,
             Str href);

    El* Inline(MdNode* n, float font, Rgba color, int weight,
               uint8_t align = MdAlignDefault);

    El* RangeWashes(El* t, const MdNode* leaf, int lo, int hi,
                    bool markOver = false);

    void RevealFrame(TextViewState* managed);

    bool RevealIn(const MdNode* leaf, int* offset) const;

    void RevealMark(El* t, int lo, int offset);
};

MdNode* MdParse(Arena* a, Str source);

struct SourceRangeSelection {
    enum Kind : uint8_t {
        Unselected,
        Mapped,
        Unmapped,
    };
    Kind kind = Unselected;
    Span range = {};

    static SourceRangeSelection MappedRange(int start, int end);
    void Merge(const SourceRangeSelection& other);
    bool IntoRange(Span* out) const;
};

bool SourceRangeForSegments(const SourceSegment* segments, int count, int start,
                            int end, Span* out);

struct SourceCharPos {
    uint32_t key = 0;
    int offset = 0;
};
struct SourceCharIndex {
    Vec<SourceCharPos> pos;
    bool built = false;
};

int SourceCharOffset(Str raw, int rawCursor, const char* ch, int cl,
                     SourceCharIndex* positions);

void AlignedSourceSegments(Arena* a, Str raw, Str rendered, int sourceOffset,
                           bool decodeEntities, bool decodeEscapes,
                           Vec<SourceSegment>& out);

SourceRangeSelection MdSelectedSourceRange(const MdNode* n, int start, int end);

SourceRangeSelection TextHitsSourceRange(const PaintCtx* ctx, int selA,
                                         int selB, int scope, EntityId owner);

TextViewParseJob* TextViewParseNowForTest(TextViewState* s, App* app);
MdNode* MdParseCachedForTest(Ctx* cx, Arena* frame, Str source,
                             const MarkdownExtensions* extensions);

void TextViewInitKeys();

struct Text {
    Str string = {};
    TextView* view = nullptr;

    static Text FromStr(Str value);
    static Text FromView(TextView* value);

    Text Style(const TextViewStyle& style) const;

    Str GetText(const App* app) const;

    El* IntoEl(Ctx* cx) const;
};

Str MdDecodeEntity(Arena* a, Str e);

TextView* MarkdownView(Ctx* cx, Str source);
TextView* HtmlView(Ctx* cx, Str source);

}

#line 1 "src/base/text_boundary.h"

namespace gpui {

CharKind CharKindOf(uint32_t c);

int Utf8ClipLeft(Str s, int off);

bool TextWordRangeAt(Str s, int off, int* outA, int* outB);

void TextLineRangeAt(Str s, int off, int* outA, int* outB);

}

#line 1 "src/base/text_format.h"

namespace gpui {

struct Minifier {
    bool omitDoctype = false;
    bool collapseWhitespace = true;
    bool preserveComments = false;
    bool precedingWhitespace = false;

    Minifier& OmitDoctype(bool value = true);
    Minifier& CollapseWhitespace(bool value = true);
    Minifier& PreserveComments(bool value = true);
    Str Minify(Arena* a, Str source);
    Str WriteCollapseWhitespace(Arena* a, Str source);
};

Str HtmlMinify(Arena* a, Str source);

MdNode* HtmlParse(Arena* a, Str source);

void HtmlParseInto(Arena* a, MdNode* parent, Str source);

struct HtmlInlineTag {

    uint8_t mark = 0;
    bool close = false;
    bool known = false;

    bool isBreak = false;

    Str href = {};
    Str alt = {};
    Str src = {};
    float width = 0;
    float height = 0;
    bool isImage = false;
};

HtmlInlineTag HtmlParseInlineTag(Arena* a, Str tag);

Str HtmlAttrValue(Arena* a, Str attrs, const char* name);

}

#line 1 "src/base/time_field.h"

namespace gpui {

enum class TimePrecision : uint8_t {

    Minute,

    Second,
};

enum class HourCycle : uint8_t {

    H23,

    H12,
};

enum class TimeSegment : uint8_t {
    Hour,
    Minute,
    Second,

    Period,
};

Str TimePrecisionFormat(TimePrecision precision, HourCycle hourCycle);

LocalTime TimePrecisionTruncate(TimePrecision precision, LocalTime time);

Str TimeFormat(Arena* a, Str pattern, LocalTime time);

enum class TimeFieldEventKind : uint8_t {

    Change,
};

struct TimeFieldEvent {
    TimeFieldEventKind kind = TimeFieldEventKind::Change;
    LocalTime time = {};
};

struct SegmentEditor {
    LocalTime time = {};
    TimePrecision precision = TimePrecision::Minute;
    HourCycle hourCycle = HourCycle::H23;
    TimeSegment segment = TimeSegment::Hour;

    int pendingDigit = -1;

    int Segments(TimeSegment out[4]) const;
    bool HasSegment(TimeSegment s) const;

    void Bounds(TimeSegment s, int* min, int* max) const;

    int Value(TimeSegment s) const;

    Str Label(Arena* a, TimeSegment s) const;
    LocalTime WithValue(TimeSegment s, int value) const;
    void SetPrecision(TimePrecision value);
    void SetHourCycle(HourCycle value);
    void ResetSegment();

    bool SetTime(LocalTime value);
    bool SelectSegment(TimeSegment s);

    bool MoveSegment(int offset);

    bool Step(int delta);
    bool InputDigit(int digit);

    bool InputPeriod(bool pm);

    bool ClearSegment();
    bool ReplaceSegment(int value);
};

struct TimeFieldState {
    Entity<TimeFieldState> self = {};
    FocusHandle focus = {};
    SegmentEditor editor = {};

    LocalTime Time() const { return editor.time; }

    TimeSegment SelectedSegment() const { return editor.segment; }

    static void OnAction(TimeFieldState* self, Ctx* cx, const ActionEvent* ev);
    static void OnKeyDown(TimeFieldState* self, Ctx* cx, const KeyEvent* ev);

    static void OnSegmentDown(TimeFieldState* self, Ctx* cx,
                              const MouseDownEvent* ev, int64_t segment);
};

Entity<TimeFieldState> TimeFieldStateNew(
    Ctx* cx, TimePrecision precision = TimePrecision::Minute,
    HourCycle hourCycle = HourCycle::H23);

void TimeFieldStateSetPrecision(TimeFieldState* s, TimePrecision precision,
                                Ctx* cx = nullptr);
void TimeFieldStateSetHourCycle(TimeFieldState* s, HourCycle hourCycle,
                                Ctx* cx = nullptr);
void TimeFieldStateSetTime(TimeFieldState* s, LocalTime time,
                           Ctx* cx = nullptr);
void TimeFieldStateFocus(TimeFieldState* s, Window* win);

void TimeFieldInitKeys();
Str TimeFieldContext();
uint32_t TimeFieldIncrement();
uint32_t TimeFieldDecrement();

struct TimeFieldSegmentState {
    TimeSegment segment = TimeSegment::Hour;
    int value = 0;
    bool selected = false;
    bool disabled = false;

    TimeSegment Segment() const { return segment; }

    int Value() const { return value; }

    bool IsSelected() const { return selected; }
    bool IsDisabled() const { return disabled; }
};

using TimeFieldSegmentRenderer = El* (*)(void* user, El* segment,
                                         const TimeFieldSegmentState* state,
                                         Ctx* cx);
void TimeFieldSegmentClearChildren(El* segment);

struct TimeField {
    Ctx* cx = nullptr;
    Str id = {};
    Entity<TimeFieldState> state = {};
    bool disabled = false;
    Style style = {};
    uint32_t styleSet = 0;
    TimeFieldSegmentRenderer segment = nullptr;
    void* segmentUser = nullptr;

    static TimeField* New(Ctx* cx, Str id, Entity<TimeFieldState> state);
    TimeField* Disabled(bool v = true);

    TimeField* RenderSegment(TimeFieldSegmentRenderer fn, void* user = nullptr);
    TimeField* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

template <>
struct EventEmitter<TimeFieldState, TimeFieldEvent> {};

}

#line 1 "src/base/toast.h"

namespace gpui {

enum class ToastTransitionStatus : uint8_t {
    Starting,
    Present,
    Ending
};

using ToastStatus = ToastTransitionStatus;

struct ToastMotion {
    int durationMs = 400;
    int exitDurationMs = 200;
    float collapsedPeek = 14.f;
    float expandedGap = 14.f;
    float collapsedScaleStep = 0.05f;
    int collapsedVisible = 3;

    static ToastMotion Sonner();
};

struct ToastOptions {
    bool hasTimeout = false;
    int timeoutMs = 0;

    static ToastOptions Timeout(int milliseconds);
    static ToastOptions Persistent();
};

template <typename I, typename T>
struct ManagedToast {
    I id = {};
    T value = {};
    ToastTransitionStatus status = ToastTransitionStatus::Starting;
    bool hasTimeout = false;
    int timeoutRemainingMs = 0;
    int transitionElapsedMs = 0;
    int64_t lastAdvanceMs = 0;
};

template <typename I, typename T>
struct ToastRemoved {
    I id = {};
    T value = {};
};

template <typename I, typename T>
struct ToastAdvance {
    bool changed = false;
    Vec<I> presented;
    Vec<I> ending;
    Vec<ToastRemoved<I, T>> removed;
};

template <typename I, typename T>
struct ToastVisible {
    const I* id = nullptr;
    const T* value = nullptr;
    ToastTransitionStatus status = ToastTransitionStatus::Starting;
};

template <typename I, typename T>
struct ToastManager {
    Vec<ManagedToast<I, T>> entries;
    int transitionDurationMs = 400;
    int exitDurationMs = 200;

    static ToastManager New(ToastMotion motion) {
        ToastManager out;
        out.transitionDurationMs = motion.durationMs;
        out.exitDurationMs = motion.exitDurationMs;
        return out;
    }

    int Len() const { return len(entries); }
    bool IsEmpty() const { return len(entries) == 0; }

    const ManagedToast<I, T>* At(int index) const {
        return index >= 0 && index < len(entries) ? &entries[index] : nullptr;
    }

    const T* Get(const I& id) const {
        for (int i = 0; i < len(entries); i++) {
            if (entries[i].id == id) {
                return &entries[i].value;
            }
        }
        return nullptr;
    }

    int Visible(int limit, ToastVisible<I, T>* out, int cap) const {
        int active = 0;
        for (int i = 0; i < len(entries); i++) {
            if (entries[i].status != ToastTransitionStatus::Ending) {
                active++;
            }
        }
        int first = active > limit ? active - limit : 0;
        int activeIndex = 0;
        int count = 0;
        for (int i = 0; i < len(entries); i++) {
            const ManagedToast<I, T>& entry = entries[i];
            bool ending = entry.status == ToastTransitionStatus::Ending;
            bool visible = ending || activeIndex >= first;
            if (!ending) {
                activeIndex++;
            }
            if (!visible) {
                continue;
            }
            if (out && count < cap) {
                out[count] = {&entry.id, &entry.value, entry.status};
            }
            count++;
        }
        return count;
    }

    bool Push(const I& id, const T& value, ToastOptions options, int64_t nowMs,
              T* replaced = nullptr, bool* hadReplaced = nullptr) {
        if (hadReplaced) {
            *hadReplaced = false;
        }
        for (int i = 0; i < len(entries); i++) {
            if (!(entries[i].id == id)) {
                continue;
            }
            if (replaced) {
                *replaced = entries[i].value;
            }
            if (hadReplaced) {
                *hadReplaced = true;
            }
            EraseAt(i);
            break;
        }
        ManagedToast<I, T> entry;
        entry.id = id;
        entry.value = value;
        entry.hasTimeout = options.hasTimeout;
        entry.timeoutRemainingMs = options.timeoutMs;
        entry.lastAdvanceMs = nowMs;
        return VecAppend(entries, entry);
    }

    bool Dismiss(const I& id, int64_t nowMs) {
        for (int i = 0; i < len(entries); i++) {
            ManagedToast<I, T>& entry = entries[i];
            if (!(entry.id == id) ||
                entry.status == ToastTransitionStatus::Ending) {
                continue;
            }
            entry.status = ToastTransitionStatus::Ending;
            entry.transitionElapsedMs = 0;
            entry.lastAdvanceMs = nowMs;
            return true;
        }
        return false;
    }

    Vec<I> DismissAll(int64_t nowMs) {
        Vec<I> changed;
        for (int i = 0; i < len(entries); i++) {
            ManagedToast<I, T>& entry = entries[i];
            if (entry.status == ToastTransitionStatus::Ending) {
                continue;
            }
            entry.status = ToastTransitionStatus::Ending;
            entry.transitionElapsedMs = 0;
            entry.lastAdvanceMs = nowMs;
            VecAppend(changed, entry.id);
        }
        return changed;
    }

    ToastAdvance<I, T> Advance(int64_t nowMs, bool paused) {
        ToastAdvance<I, T> out;
        for (int i = 0; i < len(entries); i++) {
            ManagedToast<I, T>& entry = entries[i];
            int64_t elapsed = nowMs - entry.lastAdvanceMs;
            int delta = elapsed > 0x7fffffffLL ? 0x7fffffff
                        : elapsed > 0          ? (int)elapsed
                                               : 0;
            entry.lastAdvanceMs = nowMs;
            switch (entry.status) {
                case ToastTransitionStatus::Starting:
                    entry.transitionElapsedMs += delta;
                    if (entry.transitionElapsedMs >= transitionDurationMs) {
                        entry.status = ToastTransitionStatus::Present;
                        entry.transitionElapsedMs = 0;
                        VecAppend(out.presented, entry.id);
                        out.changed = true;
                    }
                    break;
                case ToastTransitionStatus::Present:
                    if (!paused && entry.hasTimeout) {
                        entry.timeoutRemainingMs -= delta;
                        if (entry.timeoutRemainingMs <= 0) {
                            entry.timeoutRemainingMs = 0;
                            entry.status = ToastTransitionStatus::Ending;
                            entry.transitionElapsedMs = 0;
                            VecAppend(out.ending, entry.id);
                            out.changed = true;
                        }
                    }
                    break;
                case ToastTransitionStatus::Ending:
                    entry.transitionElapsedMs += delta;
                    break;
            }
        }
        int index = 0;
        while (index < len(entries)) {
            ManagedToast<I, T>& entry = entries[index];
            if (entry.status != ToastTransitionStatus::Ending ||
                entry.transitionElapsedMs < exitDurationMs) {
                index++;
                continue;
            }
            ToastRemoved<I, T> removed = {entry.id, entry.value};
            if (!VecAppend(out.removed, removed)) {
                index++;
                continue;
            }
            EraseAt(index);
            out.changed = true;
        }
        return out;
    }

  private:
    void EraseAt(int index) {
        for (int i = index; i < len(entries) - 1; i++) {
            entries[i] = entries[i + 1];
        }
        entries.len--;
        if (entries.els) {
            entries.els[len(entries)] = {};
        }
    }
};

struct ToastEntry {
    int id = 0;
    ToastTransitionStatus status = ToastTransitionStatus::Starting;
    bool hasTimeout = false;
    int timeoutRemainingMs = 0;
    int elapsedMs = 0;
};

constexpr int kToastTransitionMs = 400;
constexpr int kToastExitMs = 200;
constexpr float kToastCollapsedPeek = 14.f;
constexpr float kToastExpandedGap = 14.f;
constexpr float kToastCollapsedScaleStep = 0.05f;
constexpr int kToastCollapsedVisible = 3;

struct ToastMeasurement {
    uint32_t id = 0;
    Bounds bounds = {};
};

struct ToastStackState {

    Vec<ToastEntry> entries;
    Vec<ToastMeasurement> heights;
    Bounds bounds = {};
    int transitionMs = kToastTransitionMs;
    int exitMs = kToastExitMs;
    bool hovered = false;
    bool focused = false;

    bool IsExpanded() const { return hovered || focused; }
};

float ToastStackGeometry(const float* heights, int n, float peek, float gap,
                         bool anchoredBottom, float* collapsedOffsets,
                         float* expandedOffsets, float* expandedHeight);

bool ToastPush(ToastStackState* state, int id, int timeoutMs);
bool ToastRemove(ToastStackState* state, int id);
bool ToastStackAdvance(ToastStackState* state, int deltaMs, bool paused);

struct ToastStackItem {
    Str id = {};
    uint32_t key = 0;
    El* child = nullptr;
};

struct ToastStack {
    Arena* arena = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ToastStackState* state = nullptr;
    ToastMotion motion = {};
    Anchor placement = Anchor::TopRight;
    FocusHandle focus = {};
    bool hasFocus = false;
    Style style = {};
    uint32_t styleFields = 0;
    ArenaVec<ToastStackItem> children;

    static ToastStack* New(Ctx* cx, Str id, ToastStackState* state);
    ToastStack* Item(Str id, El* child);
    ToastStack* Child(El* child);
    ToastStack* Motion(ToastMotion value);
    ToastStack* Placement(Anchor value);
    ToastStack* Focus(FocusHandle value);
    ToastStack* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct Toast {
    El* root = nullptr;
    ToastTransitionStatus transitionStatus = ToastTransitionStatus::Starting;

    static Toast* New(Ctx* cx, Str id);
    Toast* TransitionStatus(ToastTransitionStatus value);
    ToastTransitionStatus Status() const;
    Toast* Child(El* child);
    Toast* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

}

#line 1 "src/base/toggle.h"

namespace gpui {

struct ToggleStyles {
    StateStyle pressed = {};
    StateStyle disabled = {};

    ToggleStyles& Pressed(const StateStyle& style);
    ToggleStyles& Disabled(const StateStyle& style);
};

struct Toggle {
    static El* New(Ctx* cx, Str id, bool pressed = false, bool disabled = false,
                   Listener onChange = {}, const ToggleStyles* styles = nullptr,
                   const StateStyle* instance = nullptr);
};
}

#line 1 "src/base/toggle_group.h"

namespace gpui {

struct ToggleGroup {
    static El* New(Ctx* cx, Str id, Axis axis = Axis::Horizontal);
};
}

#line 1 "src/base/toolbar.h"

namespace gpui {

const int kToolbarMaxFocusAttempts = 100;

struct ToolbarState {
    FocusHandle focus = {};
    bool disabled = false;

    static void OnKeyDown(ToolbarState* self, Ctx* cx, const KeyEvent* ev);
};

bool ToolbarMoveFocus(Window* win, FocusHandle container, bool forward);

struct Toolbar {
    Ctx* cx = nullptr;
    Str id = {};
    bool disabled = false;

    El* root = nullptr;

    static Toolbar* New(Ctx* cx, Str id);

    Toolbar* Disabled(bool value);
    Toolbar* Child(El* child);
    El* IntoEl();
};

struct ToolbarGroup {
    Ctx* cx = nullptr;
    Str label = {};
    El* root = nullptr;

    static ToolbarGroup* New(Ctx* cx, Str id);

    ToolbarGroup* Label(Str value);
    ToolbarGroup* Child(El* child);
    El* IntoEl();
};

}

#line 1 "src/base/tooltip.h"

namespace gpui {

constexpr int kTooltipPriority = 200;
constexpr float kTooltipWindowMargin = 4.f;
constexpr int kTooltipGracePeriodMs = 300;
constexpr int kTooltipShowDelayMs = 500;

struct Tooltip {
    static El* New(Ctx* cx, Str id);
};

using TooltipBuilder = El* (*)(Ctx * cx, void* data);

enum class TooltipTransitionKind : uint8_t {
    Enter,
    Switch
};

struct TooltipTransition {
    TooltipTransitionKind kind = TooltipTransitionKind::Enter;
    uint64_t epoch = 0;
    Bounds previous = {};
    Bounds current = {};

    static TooltipTransition Enter(uint64_t epoch);
    static TooltipTransition Switch(uint64_t epoch, Bounds previous,
                                    Bounds current);
};

using TooltipRenderer = El* (*)(Ctx * cx, El* view,
                                const TooltipTransition& transition,
                                void* data);

struct TooltipRequest {
    TooltipBuilder build = nullptr;
    void* buildData = nullptr;
    Bounds triggerBounds = {};
    gpui::Placement preferredPlacement = gpui::Placement::Top;
    bool hasPreferredPlacement = false;
    Str text = {};

    static TooltipRequest New(Bounds triggerBounds, TooltipBuilder build,
                              void* data = nullptr);
    static TooltipRequest Text(Bounds triggerBounds, Str text);
    TooltipRequest& Placement(gpui::Placement value);
};

struct TooltipOverlay {

    bool enabled = !(GPUI_OS_IOS || GPUI_OS_ANDROID);
    TooltipRequest content = {};
    TooltipRequest pending = {};
    Bounds previousBounds = {};
    bool hasContent = false;
    bool hasPending = false;
    bool hasPreviousBounds = false;
    bool hadRecentTooltip = false;
    bool isSwitching = false;
    uint64_t epoch = 0;
    uint64_t animationEpoch = 0;
    int showTask = 0;
    int hideTask = 0;
    TooltipRenderer renderer = nullptr;
    void* rendererData = nullptr;

    ~TooltipOverlay();

    TooltipOverlay* RenderWith(TooltipRenderer renderer, void* data = nullptr);
    uint64_t NextEpoch();
    void RequestShow(const TooltipRequest& request, Window* window, Ctx* cx);
    void RequestHide(Window* window, Ctx* cx);
    void Hide(Ctx* cx);
    static El* Render(TooltipOverlay* self, Ctx* cx);
    static void OnShow(TooltipOverlay* self, Ctx* cx, const TickEvent* ev);
    static void OnHide(TooltipOverlay* self, Ctx* cx, const TickEvent* ev);
};

struct TooltipPositioner {
    Positioner* positioner = nullptr;

    static TooltipPositioner* New(Ctx* cx, Bounds triggerBounds);
    TooltipPositioner* Placement(gpui::Placement value);
    TooltipPositioner* Child(El* child);
    El* IntoEl();
};

void TooltipRequestShow(Window* win, Str text, Bounds triggerBounds,
                        int placement = -1, bool rootLayer = false);
void TooltipRequestHide(Window* win);
void TooltipHide(Window* win);
const TooltipOverlay* TooltipShowing(Window* win);

}

#line 1 "src/base/virtual_list.h"

namespace gpui {

struct VirtualRange {
    int first = 0;
    int end = 0;
};

VirtualRange VirtualListVisibleRange(const float* sizes, int count,
                                     float offset, float viewport);

VirtualRange VirtualListVisibleRangeFromLayout(const float* origins,
                                               const float* sizes, int count,
                                               float offset, float viewport);

VirtualRange VirtualListVisibleRows(int count, float rowSize, float offset,
                                    float viewport);

float VirtualListItemOrigin(const float* sizes, int count, int ix);

enum class ScrollStrategy : uint8_t {
    Top,
    Center,
    Bottom
};

float VirtualListScrollTo(float origin, float size, float offset,
                          float viewport, float contentSize,
                          ScrollStrategy strategy);

float VirtualListScrollToItem(const float* sizes, int count, int ix,
                              float offset, float viewport,
                              ScrollStrategy strategy);

float VirtualListScrollToRow(int count, float rowSize, int ix, float offset,
                             float viewport, ScrollStrategy strategy);

float VirtualListContentSize(const float* sizes, int count);

struct VirtualListScrollHandle {
    Axis axis = Axis::Vertical;
    int itemsCount = 0;

    float offset = 0;
    float viewport = 0;
    float contentSize = 0;

    bool pending = false;
    int pendingIx = 0;

    int pendingOffset = 0;
    ScrollStrategy pendingStrategy = ScrollStrategy::Top;
};

void VirtualListScrollToItemDeferred(VirtualListScrollHandle* h, int ix,
                                     ScrollStrategy strategy);
void VirtualListScrollToItemDeferredWithOffset(VirtualListScrollHandle* h,
                                               int ix, ScrollStrategy strategy,
                                               int offset);

void VirtualListScrollToBottomDeferred(VirtualListScrollHandle* h);

bool VirtualListHandleLayout(VirtualListScrollHandle* h, const float* sizes,
                             int count, float itemSize, float viewport);

VirtualRange VirtualListHandleRange(const VirtualListScrollHandle* h,
                                    const float* sizes, int count,
                                    float itemSize);

struct ItemSizeLayout {
    Vec<float> sizes;
    Vec<float> origins;
    Size contentSize = {};
    Bounds lastLayoutBounds = {};

    ~ItemSizeLayout() {
        VecReset(sizes);
        VecReset(origins);
    }
};

void ItemSizeLayoutBuild(ItemSizeLayout* layout, Axis axis,
                         const float* itemSizes, int count,
                         float uniformItemSize, float gap, float crossSize);

struct VirtualListFrameState {
    VirtualRange visible = {};
    ItemSizeLayout sizeLayout;
    float scrollOffset = 0;
    float before = 0;
    float after = 0;
};

using VirtualRowFn = El* (*)(void* user, Ctx* cx, int ix);

using VirtualRangeFn = void (*)(void* user, Ctx* cx, int first, int end,
                                El** out);

struct VirtualListOpts {
    int count = 0;
    float rowH = 32;

    float viewH = 0;

    float viewW = 0;
    const float* sizes = nullptr;

    uint8_t* needsMeasure = nullptr;

    float scrollY = 0;
    float scrollX = 0;
    VirtualListScrollHandle* handle = nullptr;

    int scrollId = 0;
    Listener onScroll = {};

    ScrollAxis axis = ScrollAxis::Both;

    Axis layoutAxis = Axis::Vertical;

    float gap = 0;

    float pad = 0;

    float overdraw = 0;

    bool logicalScroll = false;
    int topItem = 0;
    float topInto = 0;
    VirtualRowFn row = nullptr;
    VirtualRangeFn range = nullptr;
    void* user = nullptr;
};

struct VirtualList {

    static El* New(Ctx* cx, Str id);

    static El* New(Ctx* cx, Str id, const VirtualListOpts& o);
};

float VirtualListPixelFromLogical(const float* sizes, int count, int item,
                                  float into);

void VirtualListLogicalFromPixel(const float* sizes, int count, float rowH,
                                 float pixel, int* item, float* into);

El* virtual_list(Ctx* cx, Str id, Axis axis, const VirtualListOpts& opts);
El* v_virtual_list(Ctx* cx, Str id, const VirtualListOpts& opts);
El* h_virtual_list(Ctx* cx, Str id, const VirtualListOpts& opts);
}

#line 1 "src/base/tree.h"

namespace gpui {

enum class TreeAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    Collapse,
    Expand,
    Confirm
};

void TreeInitKeys();
Str TreeContext();
TreeAction TreeActionOf(uint32_t id);

namespace tree {

void init();
}

int TreeSelectPrev(int selected, int count);
int TreeSelectNext(int selected, int count);

bool TreeCollapses(bool isFolder, bool isExpanded);
bool TreeExpands(bool isFolder, bool isExpanded);

struct TreeItem {

    Str id = {};
    Str label = {};
    int parent = -1;
    int depth = 0;

    bool folder = false;
    bool expanded = false;
    bool disabled = false;
};

struct TreeEntry {
    const TreeItem* item = nullptr;
    int itemIx = -1;
    int depth = 0;

    bool IsRoot() const { return depth == 0; }
    bool IsFolder() const { return item && item->folder; }
    bool IsExpanded() const { return item && item->expanded; }
    bool IsDisabled() const { return item && item->disabled; }
};

struct TreeEntryState {
    bool selected = false;
    bool rightClicked = false;

    bool IsSelected() const { return selected; }
    bool IsRightClicked() const { return rightClicked; }
};

enum class TreeEventKind : uint8_t {
    Expanded,
    Collapsed
};

struct TreeEvent {
    TreeEventKind kind = TreeEventKind::Expanded;

    Str id = {};
    int ix = 0;
};

struct TreeState {

    Vec<TreeItem> items;

    Vec<int> entries;
    int selected = -1;
    int rightClicked = -1;

    float rowH = 34;
    float scrollY = 0;

    float viewportH = 0;
    Listener onEvent;

    Entity<TreeState> self = {};

    static void OnRowClick(TreeState* self, Ctx* cx, const ClickEvent* ev,
                           int64_t entryIx);
    static void OnRowMouseDown(TreeState* self, Ctx* cx,
                               const MouseDownEvent* ev, int64_t entryIx);
    static void OnScroll(TreeState* self, Ctx* cx, const ScrollEvent* ev);

    ~TreeState() {
        for (int i = 0; i < len(items); i++) {
            StrFree(items[i].id);
        }
        VecReset(items);
        VecReset(entries);
    }
};

int TreeAddItem(TreeState* s, Str id, Str label, int parent);

void TreeRebuild(TreeState* s);

int TreeIndexOf(const TreeState* s, Str id);

const TreeItem* TreeEntryItem(const TreeState* s, int entryIx);
TreeEntry TreeEntryAt(const TreeState* s, int entryIx);

void TreeSetItems(TreeState* s, Ctx* cx, const TreeItem* items, int count);

bool TreeToggleExpandAt(TreeState* s, int entryIx, bool* expandedOut);
void TreeToggleExpand(TreeState* s, Ctx* cx, int entryIx);

int TreeRevealItem(TreeState* s, Str id);

int TreeRevealItem(TreeState* s, Ctx* cx, Str id, ScrollStrategy strategy);

void TreeScrollToItem(TreeState* s, int entryIx, ScrollStrategy strategy);
void TreeSetSelected(TreeState* s, Ctx* cx, int entryIx);
void TreeSetSelectedItem(TreeState* s, Ctx* cx, Str id);

void TreeClickEntry(TreeState* s, Ctx* cx, int entryIx);
void TreePerform(TreeState* s, Ctx* cx, TreeAction act);

void TreeOnAction(TreeState* self, Ctx* cx, const ActionEvent* ev);
void TreeBindKeys(Ctx* cx, El* root, Entity<TreeState> state);

struct Tree {
    static El* New(Ctx* cx);
};

using TreeRowFn = El* (*)(void* user, Ctx* cx, int entryIx,
                          const TreeEntry& entry, TreeEntryState state);

struct TreeList {

    static El* New(Ctx* cx, Str id, Entity<TreeState> state, float h,
                   TreeRowFn row, void* user);
};

struct TreeItemEl {
    static El* New(Ctx* cx, Str id = {}, Listener onClick = {});
};

template <>
struct EventEmitter<TreeState, TreeEvent> {};
}

#line 1 "src/base/lib.h"

namespace gpui {

constexpr bool IsMobile() {
    return GPUI_OS_IOS || GPUI_OS_ANDROID;
}

void ApplySystemReduceMotion();

void ApplyReduceMotionPreference(bool reduce);

void BaseInit(App* app);

}

#line 1 "src/ui/notification_settings.h"

namespace gpui {
namespace component {

enum class NotificationType : uint8_t {
    Info,
    Success,
    Warning,
    Error
};

enum class NotificationDelivery : uint8_t {
    InApp,
    System,
    InAppAndSystem
};

struct NotificationSettings {
    Anchor placement = Anchor::TopRight;

    Edges margins = Edges::New(16.f, 16.f, 50.f, 16.f);
    int maxItems = 10;
    float width = 382.f;
    NotificationDelivery delivery = NotificationDelivery::InApp;
};

}
}

#line 1 "src/ui/sheet_settings.h"

namespace gpui {
namespace component {

constexpr float kSheetDefaultMarginTop = 34.f;

struct SheetSettings {

    float marginTop = kSheetDefaultMarginTop;
};

}
}

#line 1 "src/ui/theme.h"

namespace gpui {

struct App;
struct ThemeConfig;

enum class ThemeMode : uint8_t {
    Light,
    Dark
};

struct ThemeToken {
    Rgba color = {};
    Background background = {};

    static ThemeToken New(Rgba color, Background background);
    static ThemeToken Solid(Rgba color);
};

struct ThemeTokens {
    Background background = {};
    Background titleBar = {};
    Background statusBar = {};
    Background tabBar = {};
    Background tabActiveBg = {};
    Background primary = {};
    Background secondary = {};
    Background accent = {};
    Background muted = {};
    Background popover = {};
    Background danger = {};
    Background info = {};
    Background success = {};
    Background warning = {};
    Background progress = {};
    Background scrollbarThumb = {};
    Background scrollbarThumbHover = {};
    Background skeleton = {};
    Background selection = {};
    Background listActive = {};
    Background tableBg = {};
    Background tableActive = {};
    Background tableEven = {};
    Background tableHead = {};
    Background tableFoot = {};
    Background sidebarAccent = {};
    Background sidebarPrimary = {};
    Background overlay = {};
    Background switchThumb = {};
    Background sliderThumb = {};

    Background button = {};
    Background buttonHover = {};
    Background buttonActive = {};
    Background primaryHover = {};
    Background primaryActive = {};
    Background buttonPrimary = {};
    Background buttonPrimaryHover = {};
    Background buttonPrimaryActive = {};
    Background secondaryHover = {};
    Background secondaryActive = {};
    Background buttonSecondary = {};
    Background buttonSecondaryHover = {};
    Background buttonSecondaryActive = {};
    Background successHover = {};
    Background successActive = {};
    Background buttonSuccess = {};
    Background buttonSuccessHover = {};
    Background buttonSuccessActive = {};
    Background infoHover = {};
    Background infoActive = {};
    Background buttonInfo = {};
    Background buttonInfoHover = {};
    Background buttonInfoActive = {};
    Background warningHover = {};
    Background warningActive = {};
    Background buttonWarning = {};
    Background buttonWarningHover = {};
    Background buttonWarningActive = {};
    Background dangerHover = {};
    Background dangerActive = {};
    Background buttonDanger = {};
    Background buttonDangerHover = {};
    Background buttonDangerActive = {};
    Background accordion = {};
    Background dropTarget = {};
    Background list = {};
    Background listEven = {};
    Background listHead = {};
    Background listHover = {};
    Background sliderBar = {};
    Background switchBg = {};
    Background tab = {};
    Background tabBarSegmented = {};
    Background tableHover = {};
    Background tiles = {};
    Background scrollbarBg = {};
    Background sidebar = {};
    Background groupBox = {};
    Background descListLabel = {};
};

struct MotionTokens {
    float durationInstantMs = 0;
    float durationFastMs = 0;
    float durationNormalMs = 0;
    float durationSlowMs = 0;
    Easing easingEnter = Easing::EaseOut();
    Easing easingExit = Easing::EaseOut();
    Easing easingMove = Easing::EaseOut();
    Spring springControl = {};
    Spring springMove = {};
    float distanceShort = 0;
    float distanceMedium = 0;

    static MotionTokens Default();
};

struct Theme {
    Rgba background;
    Rgba foreground;
    Rgba border;
    Rgba mutedFg;

    Rgba inputBorder;
    Rgba inputBg;

    Rgba ring;
    Rgba caret;

    Rgba selection;

    Rgba dragBorder;
    Rgba titleBar;
    Rgba titleBarBorder;

    Rgba statusBar;

    Rgba statusBarBorder;
    Rgba tabBar;
    Rgba tabActiveBg;
    Rgba tabActiveFg;
    Rgba tabFg;
    Rgba tableBg;
    Rgba tableHead;
    Rgba tableHeadFg;

    Rgba tableFoot;
    Rgba tableFootFg;
    Rgba tableRowBorder;
    Rgba tableEven;

    Rgba listActive;
    Rgba listActiveBorder;
    Rgba tableActive;
    Rgba tableActiveBorder;
    Rgba progress;
    Rgba red;
    Rgba green;
    Rgba blue;
    Rgba yellow;
    Rgba cyan;
    Rgba magenta;

    Rgba redLight;
    Rgba greenLight;
    Rgba blueLight;
    Rgba yellowLight;
    Rgba cyanLight;
    Rgba magentaLight;

    Rgba chart1;
    Rgba chart2;
    Rgba chart3;
    Rgba chart4;
    Rgba chart5;
    Rgba chartBullish;
    Rgba chartBearish;
    Rgba chartGrid;
    Rgba danger;
    Rgba dangerFg;

    Rgba popover;
    Rgba popoverFg;
    Rgba secondaryHover;
    Rgba secondaryActive;
    Rgba secondaryFg;
    Rgba secondary;
    Rgba muted;
    Rgba accent;

    Rgba accentFg;
    Rgba primary;
    Rgba primaryFg;
    Rgba primaryHover;
    Rgba primaryActive;
    Rgba sidebar;
    Rgba sidebarFg;
    Rgba sidebarPrimary;
    Rgba sidebarPrimaryFg;
    Rgba sidebarAccent;
    Rgba sidebarAccentFg;
    Rgba sidebarBorder;
    Rgba scrollbarThumb;

    Rgba scrollbarThumbHover;

    Rgba scrollbarBg;
    Rgba info;
    Rgba infoFg;
    Rgba success;
    Rgba successFg;
    Rgba warning;
    Rgba warningFg;
    Rgba skeleton;

    Rgba switchThumb;
    Rgba sliderThumb;

    Rgba overlay;

    Rgba groupBox;
    Rgba groupBoxFg;

    Rgba descListLabel;
    Rgba descListLabelFg;

    Rgba button;
    Rgba buttonFg;
    Rgba buttonHover;
    Rgba buttonActive;
    Rgba buttonPrimary;
    Rgba buttonPrimaryFg;
    Rgba buttonPrimaryHover;
    Rgba buttonPrimaryActive;
    Rgba buttonSecondary;
    Rgba buttonSecondaryFg;
    Rgba buttonSecondaryHover;
    Rgba buttonSecondaryActive;
    Rgba buttonDanger;
    Rgba buttonDangerFg;
    Rgba buttonDangerHover;
    Rgba buttonDangerActive;
    Rgba buttonSuccess;
    Rgba buttonSuccessFg;
    Rgba buttonSuccessHover;
    Rgba buttonSuccessActive;
    Rgba buttonInfo;
    Rgba buttonInfoFg;
    Rgba buttonInfoHover;
    Rgba buttonInfoActive;
    Rgba buttonWarning;
    Rgba buttonWarningFg;
    Rgba buttonWarningHover;
    Rgba buttonWarningActive;

    Rgba dangerHover;
    Rgba dangerActive;
    Rgba successHover;
    Rgba successActive;
    Rgba infoHover;
    Rgba infoActive;
    Rgba warningHover;
    Rgba warningActive;

    Rgba accordion;

    Rgba dropTarget;

    Rgba link;
    Rgba linkActive;
    Rgba linkHover;

    Rgba list;
    Rgba listEven;
    Rgba listHead;
    Rgba listHover;
    Rgba tableHover;

    Rgba sliderBar;

    Rgba switchBg;

    Rgba tab;
    Rgba tabBarSegmented;

    Rgba tiles;

    Rgba windowBorder;

    float radius;
    float radiusLg;

    float radiusFull;
    ThemeMode mode = ThemeMode::Light;

    Str fontFamily = Str(".SystemUIFont");
    float fontSize = 16.f;
    Str monoFontFamily = Str(PlatMonoFontName());
    float monoFontSize = kMonoFontSize;
    bool shadow = true;
    bool focusRing = true;
    ScrollbarMode scrollbarMode = ScrollbarMode::Scrolling;

    Rgba transparent = Rgba8(0, 0, 0, 0);
    float tileGridSize = 8.f;
    bool tileShadow = true;
    float tileRadius = 0.f;

    ListSettings listSettings = {};

    component::NotificationSettings notification = {};
    component::SheetSettings sheet = {};

    MotionTokens motion = MotionTokens::Default();

    ThemeTokens tokens = {};
};

using ThemeColor = Theme;

inline float ThemeRadius2xl(const Theme& t) {
    return t.radius * 2.5f;
}
inline float ThemeRadius3xl(const Theme& t) {
    return t.radius * 3.f;
}
inline float ThemeRadius4xl(const Theme& t) {
    return t.radius * 3.5f;
}

void ThemeTokensReset(Theme* t);

void ThemeFillDerived(Theme* t, bool dark);

const Theme& ThemeDark();
const Theme& ThemeLight();
const Theme& ThemeDark(const App* app);
const Theme& ThemeLight(const App* app);

const Theme& ThemeDefaultDark();
const Theme& ThemeDefaultLight();

void ThemeInstall(App* app, ThemeMode mode, const Theme& t);

void ThemeSetRadius(App* app, float radius);

float ThemeFontSize(const App* app);

float WheelNotchPixels(const App* app);
void ThemeSetFontSize(App* app, float px);

bool ThemeFocusRing(const App* app);
void ThemeSetFocusRing(App* app, bool on);
const Theme& ThemeNow(const App* app);
void ThemeSet(App* app, ThemeMode mode);

struct ThemeUpdateScope {
    App* app = nullptr;
    ThemeMode modeBefore = ThemeMode::Light;
    bool reloadMode = false;
    Theme colorsBefore = {};
    ThemeTokens tokensBefore = {};
    Str activeBefore[2] = {};
};
Theme* ThemeBeginUpdate(App* app, bool reloadMode, ThemeUpdateScope* scope);
void ThemeEndUpdate(ThemeUpdateScope* scope);
template <typename F>
void ThemeUpdate(App* app, F edit) {
    ThemeUpdateScope scope;
    Theme* t = ThemeBeginUpdate(app, false, &scope);
    edit(t);
    ThemeEndUpdate(&scope);
}

void ThemeSetColors(Theme* t, const Theme& colors);

bool ThemeApplyConfig(App* app, Theme* t, const ThemeConfig* cfg);
bool ThemeTokensEq(const ThemeTokens& a, const ThemeTokens& b);

Str ThemeSubstituteSystemFont(Str requested, Str resolved, const Str* installed,
                              int n);
ThemeMode ThemeGet(const App* app);

ScrollbarMode ScrollbarModeNow(const App* app);
void ScrollbarModeSet(App* app, ScrollbarMode mode);

SemanticThemeTokens ThemeSemanticTokens(const Theme& theme,
                                        float fontSize = 0.f);
void ThemeApplySemanticTokens(Theme* theme, const SemanticThemeTokens& tokens);

const int kNumShadcnColumns = 11;

struct ShadcnScale {
    const char* name;
    uint32_t hex[kNumShadcnColumns];
};

extern const ShadcnScale kShadcnScales[];
extern const int kNumShadcnScales;
extern const int kShadcnScaleNums[kNumShadcnColumns];

extern const uint32_t kShadcnStone[kNumShadcnColumns];
extern const uint32_t kShadcnBlack;
extern const uint32_t kShadcnWhite;

extern const char* const kDefaultThemeJson;

bool ThemeParseColor(Str s, Rgba* out);

bool ThemeParseBackground(Str s, Background* out);

enum class ColorName : uint8_t {
    White,
    Black,
    Neutral,
    Gray,
    Red,
    Orange,
    Amber,
    Yellow,
    Lime,
    Green,
    Emerald,
    Teal,
    Cyan,
    Sky,
    Blue,
    Indigo,
    Violet,
    Purple,
    Fuchsia,
    Pink,
    Rose
};

const ColorName* ColorNameAll(int* count);
bool ColorNameParse(Str value, ColorName* out);
Rgba ColorNameScale(ColorName name, int scale = 500);
Rgba ThemeHsl(float hueDegrees, float saturationPercent,
              float lightnessPercent);
Rgba ThemeOklch(float lightness, float chroma, float hueDegrees);
inline Rgba Oklch(float lightness, float chroma, float hueDegrees) {
    return ThemeOklch(lightness, chroma, hueDegrees);
}
Rgba ThemeBlack();
Rgba ThemeWhite();

inline const Theme& ActiveTheme(const App* app) {
    return ThemeNow(app);
}

template <typename T>
struct ThemeConfigValue {
    T value = {};
    bool has = false;

    static ThemeConfigValue Some(const T& value) {
        ThemeConfigValue out;
        out.value = value;
        out.has = true;
        return out;
    }
};

struct SemanticColorConfig {
    ThemeConfigValue<Str> background;
    ThemeConfigValue<Str> foreground;
    ThemeConfigValue<Str> surface;
    ThemeConfigValue<Str> surfaceForeground;
    ThemeConfigValue<Str> primary;
    ThemeConfigValue<Str> primaryForeground;
    ThemeConfigValue<Str> secondary;
    ThemeConfigValue<Str> secondaryForeground;
    ThemeConfigValue<Str> muted;
    ThemeConfigValue<Str> mutedForeground;
    ThemeConfigValue<Str> accent;
    ThemeConfigValue<Str> accentForeground;
    ThemeConfigValue<Str> destructive;
    ThemeConfigValue<Str> destructiveForeground;
    ThemeConfigValue<Str> border;
    ThemeConfigValue<Str> input;
    ThemeConfigValue<Str> ring;
};

struct SemanticRadiusConfig {
    ThemeConfigValue<float> none;
    ThemeConfigValue<float> sm;
    ThemeConfigValue<float> md;
    ThemeConfigValue<float> lg;
    ThemeConfigValue<float> xl;
    ThemeConfigValue<float> full;
};

struct SemanticSpacingConfig {
    ThemeConfigValue<float> xxs;
    ThemeConfigValue<float> xs;
    ThemeConfigValue<float> sm;
    ThemeConfigValue<float> md;
    ThemeConfigValue<float> lg;
    ThemeConfigValue<float> xl;
    ThemeConfigValue<float> xxl;
};

struct SemanticTextStyleConfig {
    ThemeConfigValue<float> size;
    ThemeConfigValue<float> lineHeight;
    ThemeConfigValue<FontWeight> weight;
};

struct SemanticTypographyConfig {
    ThemeConfigValue<Str> sans;
    ThemeConfigValue<Str> mono;
    SemanticTextStyleConfig xs;
    SemanticTextStyleConfig sm;
    SemanticTextStyleConfig md;
    SemanticTextStyleConfig lg;
    SemanticTextStyleConfig xl;
    SemanticTextStyleConfig monoMd;
};

struct SemanticShadowConfig {

    const JsonValue* sm = nullptr;
    const JsonValue* md = nullptr;
    const JsonValue* lg = nullptr;
};

struct SemanticThemeConfig {
    SemanticColorConfig colors;
    SemanticRadiusConfig radius;
    SemanticSpacingConfig spacing;
    SemanticTypographyConfig typography;
    SemanticShadowConfig shadow;

    bool ApplyTo(SemanticThemeTokens* tokens) const;
};

struct SemanticThemeConfigFile {
    SemanticThemeConfig tokens;
};

bool SemanticThemeConfigParse(const JsonValue* value, SemanticThemeConfig* out);
bool SemanticThemeConfigFileParse(const JsonValue* value,
                                  SemanticThemeConfigFile* out);

struct ThemeConfigColors {
    const JsonValue* value = nullptr;

    ThemeConfigColors& operator=(const JsonValue* json) {
        value = json;
        return *this;
    }
    operator const JsonValue*() const { return value; }
    explicit operator bool() const { return value != nullptr; }
};

struct ThemeSetConfig {
    Str name = {};
    Str author = {};
    Str url = {};
    const JsonValue* themes = nullptr;

    int Count() const;
};

bool ThemeSetConfigParse(const JsonValue* value, ThemeSetConfig* out);

struct ThemeConfig {
    Str name = {};
    Str author = {};
    Str url = {};
    ThemeMode mode = ThemeMode::Light;
    bool isDefault = false;

    ThemeConfigColors colors;

    float fontSize = 0;
    Str fontFamily = {};
    Str monoFontFamily = {};
    float monoFontSize = 0;
    float radius = -1;
    float radiusLg = -1;
    bool shadow = true;
    bool hasShadow = false;

    const JsonValue* highlight = nullptr;
};

bool ThemeConfigNames(const ThemeConfig* cfg, const char* key);

void ThemeConfigResolve(Theme* out, const ThemeConfig* cfg, const Theme& base);

struct ThemeRegistry {

    Arena* arena = nullptr;

    Arena* names = nullptr;
    Vec<Str> interned;
    Vec<ThemeConfig> themes;
    Vec<Str> loadedDirs;
    Str active[2] = {};

    int watch = 0;
    bool initialized = false;

    ~ThemeRegistry();
    static ThemeRegistry* Global(App* app);
    static const ThemeRegistry* Global(const App* app);
    int Count() const { return len(themes); }
};

void ThemeRegistryInit(App* app);

void ThemeSyncBase(App* app);

int ThemeRegistryLoadStr(App* app, Str json);

int ThemeRegistryLoadDir(App* app, Str dir);

bool ThemeRegistryWatchDir(App* app, Str dir, Func0 onLoad = Func0{});

int ThemeRegistryReload(App* app);

bool ThemeApplySemanticConfigStr(App* app, ThemeMode mode, Str json,
                                 SemanticThemeTokens* out = nullptr);

bool ThemeSemanticConfigApply(const JsonValue* doc, SemanticThemeTokens* io);

int ThemeRegistryCount(const App* app);
const ThemeConfig* ThemeRegistryAt(const App* app, int ix);
const ThemeConfig* ThemeRegistryFind(const App* app, Str name);

Str ThemeRegistryActive(const App* app, ThemeMode mode);

bool ThemeRegistryApply(App* app, const ThemeConfig* cfg);
bool ThemeRegistryApply(App* app, Str name);

void ThemeRegistryReset(App* app);

void ThemeRegistryFree(App* app);

}

#line 1 "src/ui/sizing.h"

namespace gpui {

struct UiSize {
    enum class Kind : uint8_t {
        Size,
        XSmall,
        Small,
        Medium,
        Large
    };

    struct Constant {
        Kind kind;
        constexpr operator Kind() const { return kind; }
    };

    static constexpr Constant Size{Kind::Size};
    static constexpr Constant XSmall{Kind::XSmall};
    static constexpr Constant Small{Kind::Small};
    static constexpr Constant Medium{Kind::Medium};
    static constexpr Constant Large{Kind::Large};

    Kind kind = Kind::Medium;
    float pixels = 0;

    constexpr UiSize() = default;
    constexpr UiSize(Kind value) : kind(value) {}
    constexpr UiSize(Constant value) : kind(value.kind) {}
    static constexpr UiSize Custom(float value) {
        UiSize out(Kind::Size);
        out.pixels = value;
        return out;
    }
    constexpr operator Kind() const { return kind; }
    UiSize& operator=(Kind value) {
        kind = value;
        pixels = 0;
        return *this;
    }
    UiSize& operator=(Constant value) { return *this = value.kind; }
};

constexpr bool operator==(UiSize a, UiSize b) {
    return a.kind == b.kind &&
           (a.kind != UiSize::Kind::Size || a.pixels == b.pixels);
}
constexpr bool operator!=(UiSize a, UiSize b) {
    return !(a == b);
}
constexpr bool operator==(UiSize a, UiSize::Constant b) {
    return a.kind == b.kind;
}
constexpr bool operator==(UiSize::Constant a, UiSize b) {
    return b == a;
}
constexpr bool operator!=(UiSize a, UiSize::Constant b) {
    return !(a == b);
}
constexpr bool operator!=(UiSize::Constant a, UiSize b) {
    return !(a == b);
}
constexpr bool operator<(UiSize a, UiSize::Constant b) {
    return (uint8_t)a.kind < (uint8_t)b.kind;
}
constexpr bool operator<(UiSize::Constant a, UiSize b) {
    return (uint8_t)a.kind < (uint8_t)b.kind;
}
constexpr bool operator>(UiSize a, UiSize::Constant b) {
    return (uint8_t)a.kind > (uint8_t)b.kind;
}
constexpr bool operator>(UiSize::Constant a, UiSize b) {
    return (uint8_t)a.kind > (uint8_t)b.kind;
}

inline float UiSizeAsF32(UiSize s) {
    switch (s) {
        case UiSize::Size:
            return s.pixels;
        case UiSize::XSmall:
            return 0;
        case UiSize::Small:
            return 1;
        case UiSize::Large:
            return 3;
        default:
            return 2;
    }
}

inline Str UiSizeAsStr(UiSize s) {
    switch (s) {
        case UiSize::Size:
            return StrL("custom");
        case UiSize::XSmall:
            return StrL("xs");
        case UiSize::Small:
            return StrL("sm");
        case UiSize::Large:
            return StrL("lg");
        default:
            return StrL("md");
    }
}

inline UiSize UiSizeFromStr(Str text) {
    if (StrEqI(text, "xs") || StrEqI(text, "xsmall")) {
        return UiSize::XSmall;
    }
    if (StrEqI(text, "sm") || StrEqI(text, "small")) {
        return UiSize::Small;
    }
    if (StrEqI(text, "lg") || StrEqI(text, "large")) {
        return UiSize::Large;
    }
    return UiSize::Medium;
}

inline UiSize UiSizeSmaller(UiSize s) {
    switch (s) {
        case UiSize::Size:
            return UiSize::Custom(s.pixels * 0.2f);
        case UiSize::Small:
            return UiSize::XSmall;
        case UiSize::Medium:
            return UiSize::Small;
        case UiSize::Large:
            return UiSize::Medium;
        default:
            return UiSize::XSmall;
    }
}

inline UiSize UiSizeLarger(UiSize s) {
    switch (s) {
        case UiSize::Size:
            return UiSize::Custom(s.pixels * 1.2f);
        case UiSize::XSmall:
            return UiSize::Small;
        case UiSize::Small:
            return UiSize::Medium;
        case UiSize::Medium:
            return UiSize::Large;
        default:
            return UiSize::Large;
    }
}

inline UiSize UiSizeMax(UiSize a, UiSize b) {
    if (a.kind == UiSize::Kind::Size && b.kind == UiSize::Kind::Size) {
        return UiSize::Custom(std::min(a.pixels, b.pixels));
    }
    if (a.kind == UiSize::Kind::Size) {
        return a;
    }
    if (b.kind == UiSize::Kind::Size) {
        return b;
    }
    return UiSizeAsF32(a) < UiSizeAsF32(b) ? a : b;
}

inline UiSize UiSizeMin(UiSize a, UiSize b) {
    if (a.kind == UiSize::Kind::Size && b.kind == UiSize::Kind::Size) {
        return UiSize::Custom(std::max(a.pixels, b.pixels));
    }
    if (a.kind == UiSize::Kind::Size) {
        return a;
    }
    if (b.kind == UiSize::Kind::Size) {
        return b;
    }
    return UiSizeAsF32(a) > UiSizeAsF32(b) ? a : b;
}

inline float UiSizePx(UiSize s) {
    switch (s) {
        case UiSize::Size:
            return s.pixels;
        case UiSize::XSmall:
            return 20;
        case UiSize::Small:
            return 24;
        case UiSize::Large:
            return 36;
        default:
            return 28;
    }
}

inline float UiIconPx(const Ctx* cx, UiSize s) {
    switch (s) {
        case UiSize::Size:
            return s.pixels;
        case UiSize::XSmall:
            return Rems(cx, 0.75f);
        case UiSize::Small:
            return Rems(cx, 0.875f);
        case UiSize::Large:
            return Rems(cx, 1.5f);
        default:
            return Rems(cx, 1.f);
    }
}

inline Edges UiTableCellPadding(UiSize s) {

    switch (s) {
        case UiSize::XSmall:
            return Edges::New(4, 4, 2, 2);
        case UiSize::Small:
            return Edges::New(6, 6, 3, 3);
        case UiSize::Large:
            return Edges::New(12, 12, 8, 8);
        default:
            return Edges::New(8, 8, 4, 4);
    }
}

inline float UiTableRowHeight(UiSize s) {
    switch (s) {
        case UiSize::Size:
            return s.pixels;
        case UiSize::XSmall:
            return 26;
        case UiSize::Small:
            return 30;
        case UiSize::Large:
            return 40;
        default:
            return 32;
    }
}

inline float UiFontPx(UiSize s) {
    switch (s) {
        case UiSize::Size:
            return s.pixels * 0.875f;
        case UiSize::XSmall:
            return 11;
        case UiSize::Small:
            return 12;
        case UiSize::Large:
            return 16;
        default:
            return 14;
    }
}

inline float UiInputPadX(UiSize s) {
    switch (s) {
        case UiSize::Large:
            return 12;
        case UiSize::Medium:
            return 10;
        case UiSize::Small:
            return 8;
        case UiSize::XSmall:
            return 4;
        default:
            return 8;
    }
}

inline float UiInputFontPx(UiSize s) {
    switch (s) {
        case UiSize::Size:
            return s.pixels * 0.875f;
        case UiSize::XSmall:
            return 12;
        case UiSize::Small:
        case UiSize::Medium:
            return 14;
        default:
            return 16;
    }
}

inline float UiInputPadY(UiSize s) {
    switch (s) {
        case UiSize::Large:
            return 10;
        case UiSize::Medium:
            return 8;
        case UiSize::Small:
            return 2;
        case UiSize::XSmall:
            return 0;
        default:
            return 2;
    }
}

inline float UiInputHeight(const Ctx* cx, UiSize s) {
    switch (s) {
        case UiSize::Large:
            return Rems(cx, 2.75f);
        case UiSize::Medium:
            return Rems(cx, 2.f);
        case UiSize::Small:
            return Rems(cx, 1.5f);
        case UiSize::XSmall:
            return Rems(cx, 1.25f);
        default:
            return Rems(cx, 1.5f);
    }
}

constexpr float kDropdownListPadding = 4.f;

inline float UiListPadX(UiSize s) {
    return UiInputPadX(s) + 1.f - kDropdownListPadding;
}

inline float UiListPadY(const Ctx* cx, UiSize s) {
    switch (s) {
        case UiSize::Large:
            return Rems(cx, 0.5f);
        case UiSize::Small:
            return Rems(cx, 0.125f);
        default:
            return Rems(cx, 0.25f);
    }
}

inline float UiSizeWithPx(const Ctx* cx, UiSize s) {
    switch (s) {
        case UiSize::Size:
            return s.pixels;
        case UiSize::Large:
            return Rems(cx, 2.75f);
        case UiSize::Medium:
            return Rems(cx, 2.f);
        case UiSize::Small:
            return Rems(cx, 1.25f);
        default:
            return Rems(cx, 1.f);
    }
}

inline El* UiInputTextSize(El* e, UiSize s) {
    return e->Font(UiInputFontPx(s));
}
inline El* UiInputPadL(El* e, UiSize s) {
    return e->PadL(UiInputPadX(s));
}
inline El* UiInputPadR(El* e, UiSize s) {
    return e->PadR(UiInputPadX(s));
}
inline El* UiInputPadX(El* e, UiSize s) {
    return e->PadX(UiInputPadX(s));
}
inline El* UiInputPadY(El* e, UiSize s) {
    return e->PadY(UiInputPadY(s));
}
inline El* UiInputH(const Ctx* cx, El* e, UiSize s) {
    return e->H(UiInputHeight(cx, s));
}
inline El* UiInputSize(const Ctx* cx, El* e, UiSize s) {
    return UiInputH(cx, UiInputPadY(UiInputPadX(e, s), s), s);
}
inline El* UiListPadX(El* e, UiSize s) {
    return e->PadX(UiListPadX(s));
}
inline El* UiListPadY(const Ctx* cx, El* e, UiSize s) {
    return e->PadY(UiListPadY(cx, s));
}
inline El* UiListSize(const Ctx* cx, El* e, UiSize s) {
    return UiInputTextSize(UiListPadY(cx, UiListPadX(e, s), s), s);
}
inline El* UiSizeWith(const Ctx* cx, El* e, UiSize s) {
    float px = UiSizeWithPx(cx, s);
    return e->W(px)->H(px);
}

inline float UiTableCellFontPx(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return 12;
        case UiSize::Small:
        case UiSize::Medium:
            return 14;
        case UiSize::Large:
            return 16;
        default:
            return 0;
    }
}
inline El* UiTableCellSize(El* e, UiSize s) {
    Edges pad = UiTableCellPadding(s);
    if (float font = UiTableCellFontPx(s)) {
        e->Font(font);
    }
    return e->PadL(pad.left)->PadR(pad.right)->PadT(pad.top)->PadB(pad.bottom);
}

inline float UiButtonTextPx(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return 12;
        case UiSize::Small:
        case UiSize::Medium:
            return 14;
        default:
            return 16;
    }
}
inline El* UiButtonTextSize(El* e, UiSize s) {
    return e->Font(UiButtonTextPx(s));
}

namespace component {

inline El* BindClick(El* e, Str name, Listener onClick) {
    e->PathId(name);
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}

inline El* BindPathClick(El* e, Str name, Listener onClick) {
    e->PathClick(name);
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
}
}

#line 1 "src/ui/accordion.h"

namespace gpui {

namespace component {

struct AccordionStyle {
    float padT = -1;
    float padB = -1;
    float padL = -1;
    float padR = -1;
    Rgba fg = {};
};

struct AccordionItem {
    Ctx* cx = nullptr;
    El* title = nullptr;

    ArenaVec<El*> children;
    bool open = false;
    bool disabled = false;
    IconName icon = IconName::None;
    AccordionStyle titleStyle = {};
    AccordionStyle contentStyle = {};

    ElRefiner refiner = {};

    int index = 0;
    bool last = false;
    UiSize size = UiSize::Medium;

    Listener onToggle;

    static AccordionItem* New(Ctx* cx);
    AccordionItem* Title(El* t);
    AccordionItem* Title(Str s);
    AccordionItem* Icon(IconName i);
    AccordionItem* Open(bool v);
    AccordionItem* Disabled(bool v);
    AccordionItem* Child(El* c);
    AccordionItem* Child(Str s);
    AccordionItem* TitleStyle(const AccordionStyle& s);
    AccordionItem* ContentStyle(const AccordionStyle& s);
    AccordionItem* WithSize(UiSize s);

    El* IntoEl();
};

struct Accordion {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    bool multiple = false;
    bool bordered = true;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    ArenaVec<AccordionItem*> items;
    Listener onToggle;

    static Accordion* New(Ctx* cx, Str id);
    Accordion* Multiple(bool v);
    Accordion* Bordered(bool v);
    Accordion* Disabled(bool v);
    Accordion* WithSize(UiSize s);
    Accordion* Item(AccordionItem* it);
    Accordion* OnToggle(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/alert.h"

namespace gpui {

namespace component {

enum class AlertVariant : uint8_t {
    Default,
    Info,
    Success,
    Warning,
    Error
};

struct Alert {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    AlertVariant variant = AlertVariant::Default;
    IconName icon = IconName::Info;
    Str title = {};
    Str message = {};

    bool markdown = false;

    El* content = nullptr;
    UiSize size = UiSize::Medium;
    bool banner = false;
    bool visible = true;
    Listener onClose;

    static Alert* New(Ctx* cx, Str id, Str message);
    static Alert* Info(Ctx* cx, Str id, Str message);
    static Alert* Success(Ctx* cx, Str id, Str message);
    static Alert* Warning(Ctx* cx, Str id, Str message);
    static Alert* Error(Ctx* cx, Str id, Str message);
    Alert* Title(Str s);
    Alert* Icon(IconName n);
    Alert* Markdown(bool v = true);
    Alert* Content(El* e);
    Alert* Banner();
    Alert* Visible(bool v);
    Alert* OnClose(Listener fn);
    Alert* WithSize(UiSize s);
    El* IntoEl();
};

}
}

#line 1 "src/ui/shimmer.h"

namespace gpui {

namespace component {

const int kShimmerLayerCount = 12;
const float kDefaultShimmerSpread = 0.3f;

const float kMinHighlightLightnessGap = 0.1f;

struct ShimmerSpread {
    enum class Kind : uint8_t {
        Relative,
        Absolute
    };

    Kind kind = Kind::Relative;
    float value = kDefaultShimmerSpread;

    static ShimmerSpread Relative(float fraction);
    static ShimmerSpread Absolute(float length);
};

inline bool operator==(ShimmerSpread a, ShimmerSpread b) {
    return a.kind == b.kind && a.value == b.value;
}
inline bool operator!=(ShimmerSpread a, ShimmerSpread b) {
    return !(a == b);
}

struct ShimmerStyle {
    float durationMs = 2000.f;
    Rgba highlightColor = {};
    bool hasHighlightColor = false;
    ShimmerSpread spread = {};
    bool reverse = false;
    bool once = false;

    static ShimmerStyle New();

    ShimmerStyle Duration(float ms) const;
    ShimmerStyle HighlightColor(Rgba color) const;

    ShimmerStyle Spread(ShimmerSpread value) const;
    ShimmerStyle Spread(float fraction) const;
    ShimmerStyle Reverse(bool value) const;
    ShimmerStyle Once(bool value) const;
};

struct ShimmerAnimation {
    float durationMs = 0;

    bool synced = false;
    bool oneshot = false;
};

ShimmerAnimation ShimmerLoadingAnimation(float durationMs, bool once);
inline ShimmerAnimation ShimmerStyleAnimation(const ShimmerStyle& style) {
    return ShimmerLoadingAnimation(style.durationMs, style.once);
}

float ShimmerPhase(Ctx* cx, uint32_t key, const ShimmerStyle& style);

Rgba ShimmerHighlightColor(Rgba text, Rgba background, Rgba foreground,
                           bool dark, const Rgba* overrideColor);

float ShimmerLayerOpacity(bool dark);

bool ShimmerBandBounds(Bounds bounds, float phase, ShimmerSpread spread,
                       int layer, Bounds* out);

struct ShimmerText {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};
    ShimmerStyle shimmerStyle = {};
    Str id = {};
    Rgba fg = {};
    bool hasFg = false;

    static ShimmerText* New(Ctx* cx, Str text);

    ShimmerText* Id(Str value);
    ShimmerText* WithShimmerStyle(const ShimmerStyle& style);
    ShimmerText* Duration(float ms);
    ShimmerText* HighlightColor(Rgba color);
    ShimmerText* Spread(ShimmerSpread value);
    ShimmerText* Spread(float fraction);
    ShimmerText* Reverse(bool value = true);
    ShimmerText* Once(bool value = true);

    ShimmerText* Fg(Rgba color);
    El* IntoEl();
};

}
}

#line 1 "src/ui/attachment.h"

namespace gpui {

namespace component {

enum class AttachmentStatus : uint8_t {

    Pending,

    Uploading,

    Processing,

    Failed,

    Complete
};

inline bool AttachmentStatusIsPending(AttachmentStatus s) {
    return s == AttachmentStatus::Pending;
}
inline bool AttachmentStatusIsUploading(AttachmentStatus s) {
    return s == AttachmentStatus::Uploading;
}
inline bool AttachmentStatusIsProcessing(AttachmentStatus s) {
    return s == AttachmentStatus::Processing;
}
inline bool AttachmentStatusIsFailed(AttachmentStatus s) {
    return s == AttachmentStatus::Failed;
}
inline bool AttachmentStatusIsComplete(AttachmentStatus s) {
    return s == AttachmentStatus::Complete;
}
inline bool AttachmentStatusIsInProgress(AttachmentStatus s) {
    return s == AttachmentStatus::Uploading ||
           s == AttachmentStatus::Processing;
}

struct AttachmentSlotLayout {
    UiSize size = UiSize::Medium;
    AttachmentStatus status = AttachmentStatus::Complete;
    Axis axis = Axis::Horizontal;

    bool flush = false;

    Listener retry;

    float progress = -1;

    Str id = {};
    bool hasId = false;
};

struct AttachmentMedia {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    ArenaVec<El*> overlays;
    UiSize size = UiSize::Medium;
    bool hasSize = false;
    AttachmentStatus status = AttachmentStatus::Complete;
    Axis axis = Axis::Horizontal;
    bool flush = false;
    Listener retry;
    float progress = -1;
    Str id = {};
    bool hasId = false;
    Str source = {};
    bool hasSource = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentMedia* New(Ctx* cx);
    AttachmentMedia* Src(Str source);

    AttachmentMedia* Overlay(El* overlay);
    AttachmentMedia* Child(El* e);
    AttachmentMedia* WithSize(UiSize value);
    AttachmentMedia* Refine(const Style& s, uint32_t fields);

    AttachmentMedia* Layout(const AttachmentSlotLayout& layout);
    El* IntoEl();
};

struct AttachmentTitle {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};
    AttachmentStatus status = AttachmentStatus::Complete;
    bool hasStatus = false;
    ShimmerStyle shimmerStyle = {};
    bool hasShimmerStyle = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentTitle* New(Ctx* cx, Str text);
    AttachmentTitle* Status(AttachmentStatus value);
    AttachmentTitle* WithShimmerStyle(const ShimmerStyle& value);
    AttachmentTitle* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct AttachmentDescription {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};
    AttachmentStatus status = AttachmentStatus::Complete;
    bool hasStatus = false;

    UiSize size = UiSize::Medium;
    bool hasSize = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentDescription* New(Ctx* cx, Str text);
    AttachmentDescription* Status(AttachmentStatus value);
    AttachmentDescription* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct AttachmentContentChild {
    AttachmentTitle* title = nullptr;
    AttachmentDescription* description = nullptr;
    El* element = nullptr;
};

struct AttachmentContent {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<AttachmentContentChild> children;
    bool verticalLayout = false;
    AttachmentStatus status = AttachmentStatus::Complete;

    Listener retry;
    Str retryId = {};

    float progress = -1;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentContent* New(Ctx* cx);
    AttachmentContent* Title(AttachmentTitle* value);
    AttachmentContent* Description(AttachmentDescription* value);
    AttachmentContent* Child(El* e);
    AttachmentContent* Refine(const Style& s, uint32_t fields);
    AttachmentContent* Layout(const AttachmentSlotLayout& layout);
    El* IntoEl();
};

struct AttachmentActions {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    bool verticalLayout = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentActions* New(Ctx* cx);
    AttachmentActions* Child(El* e);
    AttachmentActions* Refine(const Style& s, uint32_t fields);
    AttachmentActions* LayoutForAxis(Axis axis);
    El* IntoEl();
};

struct Attachment {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    bool hasId = false;
    Style style = {};
    uint32_t styleSet = 0;
    AttachmentStatus status = AttachmentStatus::Complete;
    UiSize size = UiSize::Medium;
    Axis axis = Axis::Horizontal;
    AttachmentMedia* media = nullptr;
    AttachmentContent* content = nullptr;
    AttachmentActions* actions = nullptr;
    Listener onClick;
    Listener onRemove;
    Listener onRetry;

    float progress = -1;
    Str tooltip = {};
    bool hasTooltip = false;

    static Attachment* New(Ctx* cx);

    Attachment* Id(Str value);

    Attachment* OnClick(Listener handler);

    Attachment* OnRemove(Listener handler);

    Attachment* OnRetry(Listener handler);

    Attachment* Progress(float percent);

    Attachment* Tooltip(Str text);
    Attachment* Status(AttachmentStatus value);
    Attachment* WithAxis(Axis value);
    Attachment* Media(AttachmentMedia* value);
    Attachment* Content(AttachmentContent* value);
    Attachment* Actions(AttachmentActions* value);
    Attachment* WithSize(UiSize value);
    Attachment* Refine(const Style& s, uint32_t fields);

    Listener RetryControl() const;

    void LayoutSlots();
    El* IntoEl();
};

struct AttachmentGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ArenaVec<El*> children;
    float scrollX = 0;
    Listener onScroll;
    Rgba edgeFade = {};
    bool hasEdgeFade = false;
    Style style = {};
    uint32_t styleSet = 0;

    static AttachmentGroup* New(Ctx* cx, Str id);
    AttachmentGroup* Child(El* e);

    AttachmentGroup* TrackScroll(float value, Listener onScroll);
    AttachmentGroup* ScrollX(float value);
    AttachmentGroup* OnScroll(Listener fn);

    AttachmentGroup* WithEdgeFade(Rgba color);
    AttachmentGroup* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct AttachmentCardMetrics {

    float height = 0;

    float chipWidth = 0;

    float media = 0;

    float mediaGlyph = 0;
    float paddingStart = 0;
    float paddingEnd = 0;

    float cardPadding = 0;
    float gap = 0;
    float text = 0;
    float description = 0;
};

AttachmentCardMetrics AttachmentMetrics(const Ctx* cx, UiSize size);

const int kAttachmentUploadBarPoints = 15;
int AttachmentUploadBarPath(float w, float h, float percent, float radius,
                            Point* out);

}
}

#line 1 "src/ui/avatar.h"

namespace gpui {

namespace component {

float AvatarSizePx(UiSize s);

TempStr AvatarInitialsTemp(Str name);

struct Avatar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str initials = {};
    Background bg = {};
    bool hasBg = false;
    float size = 48;

    float textPx = -1;

    bool textSemibold = false;
    float radius = -1;
    float borderW = 1;
    Rgba borderC = {};
    bool hasBorderC = false;
    IconName placeholder = IconName::User;

    Str src = {};

    static Avatar* New(Ctx* cx);

    Avatar* Name(Str s);
    Avatar* Src(Str url);
    Avatar* Initials(Str s);
    Avatar* Bg(Background c);
    Avatar* Size(float v);
    Avatar* WithSize(UiSize s);
    Avatar* Radius(float v);
    Avatar* Border(float w, Rgba c);
    Avatar* Placeholder(IconName n);
    El* IntoEl();
};

struct AvatarGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<Avatar*> avatars;
    UiSize size = UiSize::Medium;
    int limit = 3;
    bool ellipsis = false;

    static AvatarGroup* New(Ctx* cx);
    AvatarGroup* Child(Avatar* av);
    AvatarGroup* WithSize(UiSize s);
    AvatarGroup* Limit(int v);
    AvatarGroup* Ellipsis();
    El* IntoEl();
};

}
}

#line 1 "src/ui/badge.h"

namespace gpui {

namespace component {

enum class BadgeKind : uint8_t {
    Number,
    Dot,
    Icon
};

struct Badge {
    Arena* a = nullptr;
    Ctx* cx = nullptr;

    uint64_t count = 0;
    uint64_t max = 99;
    BadgeKind kind = BadgeKind::Number;
    IconName icon = IconName::None;
    Rgba color = {};
    bool hasColor = false;
    UiSize size = UiSize::Medium;

    ArenaVec<El*> children;

    static Badge* New(Ctx* cx);
    Badge* Count(uint64_t n);
    Badge* Max(uint64_t n);
    Badge* Dot();
    Badge* Icon(IconName n);
    Badge* Color(Rgba c);
    Badge* WithSize(UiSize s);
    Badge* Child(El* c);
    El* IntoEl();
};

}
}

#line 1 "src/ui/message.h"

namespace gpui {

namespace component {

struct Bubble;

enum class MessageAlignment : uint8_t {

    Start,

    End
};

struct MessageGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    static MessageGroup* New(Ctx* cx);
    MessageGroup* Child(El* e);
    MessageGroup* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct MessageAvatar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    static MessageAvatar* New(Ctx* cx);
    MessageAvatar* Child(El* e);
    MessageAvatar* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct MessageHeader {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;

    bool contentInset = false;
    bool hasContentInset = false;
    Style style = {};
    uint32_t styleSet = 0;

    static MessageHeader* New(Ctx* cx);
    MessageHeader* ContentInset(bool value);

    MessageHeader* WithInheritedContentInset(bool value);
    MessageHeader* Child(El* e);
    MessageHeader* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct MessageContent {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    MessageAlignment alignment = MessageAlignment::Start;
    bool hasGhostBubble = false;
    Style style = {};
    uint32_t styleSet = 0;

    static MessageContent* New(Ctx* cx);

    MessageContent* WithBubble(Bubble* bubble);
    MessageContent* Aligned(MessageAlignment value);
    MessageContent* Child(El* e);
    MessageContent* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct MessageFooter {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    bool contentInset = false;
    bool hasContentInset = false;
    Style style = {};
    uint32_t styleSet = 0;

    static MessageFooter* New(Ctx* cx);
    MessageFooter* ContentInset(bool value);
    MessageFooter* WithInheritedContentInset(bool value);
    MessageFooter* Child(El* e);
    MessageFooter* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct Message {
    Arena* a = nullptr;
    Ctx* cx = nullptr;

    Str id = {};
    bool hasId = false;
    RoleOverride role = {};
    Style style = {};
    uint32_t styleSet = 0;
    Style stackStyle = {};
    uint32_t stackStyleSet = 0;
    MessageAlignment alignment = MessageAlignment::Start;
    MessageAvatar* avatar = nullptr;
    MessageHeader* header = nullptr;
    MessageContent* content = nullptr;
    MessageFooter* footer = nullptr;

    static Message* New(Ctx* cx);

    Message* Id(Str value);

    Message* Role(RoleOverride value);
    Message* Alignment(MessageAlignment value);
    Message* WithStackStyle(const Style& s, uint32_t fields);

    Message* Avatar(El* avatarEl);
    Message* AvatarSlot(MessageAvatar* value);
    Message* Header(MessageHeader* value);
    Message* Content(MessageContent* value);
    Message* Footer(MessageFooter* value);
    Message* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

}
}

#line 1 "src/ui/bubble.h"

namespace gpui {

namespace component {

struct Button;

enum class BubbleVariant : uint8_t {

    Filled,

    Secondary,

    Muted,

    Tinted,

    Outline,

    Ghost,

    Destructive
};

enum class BubbleReactionSide : uint8_t {

    Top,

    Bottom
};

struct BubbleContent {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;

    BubbleVariant variant = BubbleVariant::Filled;
    MessageAlignment alignment = MessageAlignment::Start;
    bool hasAlignment = false;
    Style style = {};
    uint32_t styleSet = 0;

    static BubbleContent* New(Ctx* cx);
    BubbleContent* Child(El* e);
    BubbleContent* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct BubbleGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    static BubbleGroup* New(Ctx* cx);
    BubbleGroup* Child(El* e);
    BubbleGroup* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct BubbleReactionChild {
    Button* action = nullptr;
    El* element = nullptr;
};

struct BubbleReactions {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<BubbleReactionChild> children;
    BubbleReactionSide side = BubbleReactionSide::Bottom;
    MessageAlignment alignment = MessageAlignment::End;
    Style style = {};
    uint32_t styleSet = 0;

    static BubbleReactions* New(Ctx* cx);
    BubbleReactions* Side(BubbleReactionSide value);
    BubbleReactions* Alignment(MessageAlignment value);

    BubbleReactions* Action(Button* action);
    BubbleReactions* Child(El* e);
    BubbleReactions* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct Bubble {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Style style = {};
    uint32_t styleSet = 0;
    MessageAlignment alignment = MessageAlignment::Start;
    bool hasAlignment = false;
    BubbleVariant variant = BubbleVariant::Filled;
    BubbleContent* content = nullptr;
    BubbleReactions* reactions = nullptr;

    static Bubble* New(Ctx* cx);
    Bubble* Alignment(MessageAlignment value);
    Bubble* WithVariant(BubbleVariant value);
    bool IsGhost() const { return variant == BubbleVariant::Ghost; }

    Bubble* Content(BubbleContent* value);
    Bubble* Reactions(BubbleReactions* value);
    Bubble* Child(El* e);
    Bubble* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

}
}

#line 1 "src/ui/breadcrumb.h"

namespace gpui {

namespace component {

struct BreadcrumbItem {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str label = {};
    Listener onClick;
    bool disabled = false;

    bool isLast = false;
    int ix = 0;

    static BreadcrumbItem* New(Ctx* cx, Str label);
    BreadcrumbItem* Disabled(bool v);
    BreadcrumbItem* OnClick(Listener fn);
    El* IntoEl();
};

struct Breadcrumb {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<BreadcrumbItem*> items;

    static Breadcrumb* New(Ctx* cx);
    Breadcrumb* Child(BreadcrumbItem* item);

    Breadcrumb* Child(Str label);
    El* IntoEl();
};

}
}

#line 1 "src/ui/button.h"

namespace gpui {

namespace component {

const float kFocusLineOpacity = 0.6f;

enum class ButtonRounded : uint8_t {
    None,
    Small,
    Medium,
    Large,
    Size
};

struct ButtonCustomVariant {
    Rgba color = {};
    Rgba foreground = {};
    bool shadow = false;
    Rgba hover = {};
    Rgba active = {};

    static ButtonCustomVariant New(const App* app);
    ButtonCustomVariant Color(Rgba value) const;
    ButtonCustomVariant Foreground(Rgba value) const;
    ButtonCustomVariant Hover(Rgba value) const;
    ButtonCustomVariant Active(Rgba value) const;
    ButtonCustomVariant Shadow(bool value = true) const;
};

enum class ButtonVariant : uint8_t {
    Default,
    Primary,
    Secondary,
    Danger,
    Info,
    Success,
    Warning,
    Ghost,
    Link,
    Text,

    Custom
};

inline bool ButtonVariantIsLink(ButtonVariant value) {
    return value == ButtonVariant::Link;
}
inline bool ButtonVariantIsText(ButtonVariant value) {
    return value == ButtonVariant::Text;
}
inline bool ButtonVariantIsGhost(ButtonVariant value) {
    return value == ButtonVariant::Ghost;
}

enum class ButtonIconVariant : uint8_t {
    Icon,
    Spinner,
    Progress
};

struct Icon;
struct Spinner;
struct ProgressCircle;

struct ButtonIcon {
    Ctx* cx = nullptr;
    ButtonIconVariant variant = ButtonIconVariant::Icon;
    IconName iconName = IconName::None;
    component::Icon* icon = nullptr;
    component::Spinner* spinner = nullptr;
    component::ProgressCircle* progress = nullptr;
    IconName loadingIconName = IconName::None;
    component::Icon* loadingIcon = nullptr;
    bool loading = false;
    UiSize size = UiSize::Medium;
    float sizePx = 0;

    static ButtonIcon* New(Ctx* cx, IconName icon);
    static ButtonIcon* New(Ctx* cx, component::Icon* icon);
    static ButtonIcon* New(Ctx* cx, component::Spinner* spinner);
    static ButtonIcon* New(Ctx* cx, component::ProgressCircle* progress);
    ButtonIcon* LoadingIcon(IconName value);
    ButtonIcon* LoadingIcon(component::Icon* value);
    ButtonIcon* Loading(bool value);
    ButtonIcon* WithSize(UiSize value);
    ButtonIcon* Size(float value);
    bool IsSpinner() const { return variant == ButtonIconVariant::Spinner; }
    bool IsProgress() const { return variant == ButtonIconVariant::Progress; }
    El* IntoEl();
};

struct Button;

struct ButtonVariants {
    static Button* WithVariant(Button* button, ButtonVariant variant);
    static Button* Primary(Button* button);
    static Button* Secondary(Button* button);
    static Button* Danger(Button* button);
    static Button* Warning(Button* button);
    static Button* Success(Button* button);
    static Button* Info(Button* button);
    static Button* Ghost(Button* button);
    static Button* Link(Button* button);
    static Button* Text(Button* button);
    static Button* Custom(Button* button, const ButtonCustomVariant& variant);
};

struct Button {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};
    IconName icon = IconName::None;

    bool hasIconColor = false;
    Rgba iconColor = {};

    IconName iconRight = IconName::None;
    ButtonIcon* buttonIcon = nullptr;
    ButtonVariant variant = ButtonVariant::Default;
    ButtonCustomVariant customVariant = {};
    ButtonRounded rounded = ButtonRounded::Medium;
    float roundedPx = 0;
    UiSize size = UiSize::Medium;
    bool outline = false;
    bool disabled = false;
    bool loading = false;
    bool compact = false;
    bool justifyStart = false;
    bool selected = false;

    bool open = false;
    bool dropdown = false;

    bool hoverGroup = false;

    bool hoverGroupHeld = false;
    bool focusRing = true;
    int tabIndex = 0;
    bool tabStop = true;
    bool hasCustom = false;
    Rgba custom = {};
    Str tooltip = {};

    int8_t tooltipPlacement = -1;
    Str accessibilityLabel = {};
    Str accessibilityId = {};
    AccessibilityRole accessibilityRole = AccessibilityRole::None;
    bool hasAccessibilityRole = false;
    bool accessibilityToggled = false;
    bool hasAccessibilityToggled = false;
    El* extra = nullptr;
    ArenaVec<El*> children;

    float sizePx = 0;

    float contentTextPx = 0;
    float contentLineH = 0;
    float contentGap = 0;
    float contentIconPx = 0;
    IconName loadingIcon = IconName::None;

    bool joined = false;
    bool edgeT = true, edgeB = true, edgeL = true, edgeR = true;
    bool cornerTL = true, cornerTR = true, cornerBL = true, cornerBR = true;
    Listener onClick;
    Listener onHover;
    uint32_t clickAction = 0;
    int64_t clickActionArg = 0;

    StateStyle selectedStyle = {};
    StateStyle disabledStyle = {};

    static Button* New(Ctx* cx, Str id);
    Button* Label(Str s);
    Button* Icon(IconName n);
    Button* Icon(ButtonIcon* value);
    Button* WithVariant(ButtonVariant value);
    Button* IconColor(Rgba c);
    Button* IconRight(IconName n);
    Button* Primary();
    Button* Secondary();
    Button* Danger();
    Button* Warning();
    Button* Success();
    Button* Info();
    Button* Ghost();
    Button* Link();
    Button* Text();
    Button* Outline();
    Button* Rounded(ButtonRounded value);
    Button* Rounded(float px);
    Button* Compact();

    Button* JustifyStart(bool v = true);
    Button* Selected(bool v);

    Button* Open(bool v);
    bool ShowsSelectedStyle() const;
    Button* SelectedStyle(const StateStyle& s);
    Button* DisabledStyle(const StateStyle& s);
    Button* DropdownCaret(bool v = true);

    Button* HoverGroup(bool v = true);
    Button* HoverGroupHeld(bool v);
    Button* Custom(Rgba c);
    Button* Custom(const ButtonCustomVariant& value);
    Button* Extra(El* e);
    Button* Child(El* e);
    Button* Children(El** values, int count);
    Button* Loading(bool v);
    Button* Disabled(bool v);
    Button* WithSize(UiSize s);

    Button* Size(float px);

    Button* LoadingIcon(IconName n);

    Button* TabIndex(int v);
    Button* TabStop(bool v);
    Button* FocusRing(bool v);
    Button* Tooltip(Str s);

    Button* TooltipPlacement(gpui::Placement placement);
    Button* AccessibilityLabel(Str s);
    Button* AccessibilityId(Str s);
    Button* Role(AccessibilityRole role);

    Button* Toggled(bool v = true);
    Button* OnClick(Listener l);
    Button* OnHover(Listener l);
    Button* OnClickAction(uint32_t action, int64_t arg = 0);
    El* IntoEl();
};

enum class ToggleVariant : uint8_t {
    Ghost,
    Outline
};

struct Toggle;
struct ToggleGroup;

struct ToggleVariants {
    static Toggle* WithVariant(Toggle* toggle, ToggleVariant variant);
    static Toggle* Ghost(Toggle* toggle);
    static Toggle* Outline(Toggle* toggle);
    static ToggleGroup* WithVariant(ToggleGroup* group, ToggleVariant variant);
    static ToggleGroup* Ghost(ToggleGroup* group);
    static ToggleGroup* Outline(ToggleGroup* group);
};

struct Toggle {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};
    Str tooltip = {};
    IconName icon = IconName::None;
    ArenaVec<El*> children;
    bool checked = false;
    UiSize size = UiSize::Medium;
    ToggleVariant variant = ToggleVariant::Ghost;
    bool disabled = false;
    bool cornerTL = true, cornerTR = true, cornerBL = true, cornerBR = true;
    bool edgeT = true, edgeB = true, edgeL = true, edgeR = true;
    Listener onClick = {};

    static Toggle* New(Ctx* cx, Str id);
    Toggle* Tooltip(Str value);
    Toggle* Label(Str value);
    Toggle* Icon(IconName value);
    Toggle* Child(El* value);
    Toggle* Checked(bool value);
    Toggle* OnClick(Listener value);
    Toggle* BorderCorners(bool tl, bool tr, bool br, bool bl);
    Toggle* BorderEdges(bool top, bool right, bool bottom, bool left);
    Toggle* WithVariant(ToggleVariant value);
    Toggle* Ghost();
    Toggle* Outline();
    Toggle* Disabled(bool value);
    Toggle* WithSize(UiSize value);
    El* IntoEl();
};

struct ToggleGroupEvent {
    const bool* checked = nullptr;
    int count = 0;
};

struct ToggleGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ArenaVec<Toggle*> items;
    UiSize size = UiSize::Medium;
    ToggleVariant variant = ToggleVariant::Ghost;
    bool disabled = false;
    bool segmented = false;
    Listener onClick = {};

    static ToggleGroup* New(Ctx* cx, Str id);
    ToggleGroup* Child(Toggle* value);
    ToggleGroup* Children(Toggle** values, int count);
    ToggleGroup* OnClick(Listener value);
    ToggleGroup* Segmented(bool value = true);
    ToggleGroup* WithSize(UiSize value);
    ToggleGroup* WithVariant(ToggleVariant value);
    ToggleGroup* Ghost();
    ToggleGroup* Outline();
    ToggleGroup* Disabled(bool value);
    El* IntoEl();
};

struct DropdownMenu;
struct PopupMenu;

struct DropdownButton {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Button* button = nullptr;
    PopupMenu* menu = nullptr;
    bool selected = false;
    bool disabled = false;
    bool outline = false;

    bool hasVariant = false;
    ButtonVariant variant = ButtonVariant::Default;
    ButtonCustomVariant customVariant = {};
    bool hasSize = false;
    UiSize size = UiSize::Medium;

    bool anchorRight = true;
    bool anchorAbove = false;

    static DropdownButton* New(Ctx* cx, Str id);
    DropdownButton* Button_(component::Button* b);
    DropdownButton* Menu(PopupMenu* m);
    DropdownButton* Selected(bool v);
    DropdownButton* Disabled(bool v);
    DropdownButton* Outline();
    DropdownButton* WithVariant(ButtonVariant v);
    DropdownButton* Primary();
    DropdownButton* Secondary();
    DropdownButton* Danger();
    DropdownButton* Warning();
    DropdownButton* Success();
    DropdownButton* Info();
    DropdownButton* Ghost();
    DropdownButton* Link();
    DropdownButton* Text();
    DropdownButton* Custom(const ButtonCustomVariant& value);
    DropdownButton* WithSize(UiSize s);
    El* IntoEl();
};

struct ButtonGroupEvent {
    const int* selected = nullptr;
    int count = 0;

    bool Contains(int index) const {
        for (int i = 0; i < count; i++) {
            if (selected[i] == index) {
                return true;
            }
        }
        return false;
    }
};

struct ButtonGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ArenaVec<Button*> children;
    bool multiple = false;
    bool disabled = false;
    bool vertical = false;
    bool compact = false;
    bool outline = false;
    bool hasVariant = false;
    ButtonVariant variant = ButtonVariant::Default;
    ButtonCustomVariant customVariant = {};
    bool hasSize = false;
    UiSize size = UiSize::Medium;

    Listener onClick;

    static ButtonGroup* New(Ctx* cx, Str id);
    ButtonGroup* Child(Button* b);
    ButtonGroup* Children(Button** values, int count);
    ButtonGroup* Multiple(bool v);
    ButtonGroup* Disabled(bool v);
    ButtonGroup* Vertical(bool v = true);
    ButtonGroup* Layout(Axis value);
    ButtonGroup* Compact();
    ButtonGroup* Outline();
    ButtonGroup* WithVariant(ButtonVariant v);
    ButtonGroup* Primary();
    ButtonGroup* Secondary();
    ButtonGroup* Danger();
    ButtonGroup* Warning();
    ButtonGroup* Success();
    ButtonGroup* Info();
    ButtonGroup* Ghost();
    ButtonGroup* Link();
    ButtonGroup* Text();
    ButtonGroup* Custom(const ButtonCustomVariant& value);
    ButtonGroup* WithSize(UiSize s);
    ButtonGroup* OnClick(Listener l);
    El* IntoEl();
};

}
}

#line 1 "src/ui/carousel.h"

namespace gpui {
namespace component {

struct CarouselEvent {
    int index = 0;
};

struct CarouselPointerGesture {
    Point startPosition = {};
    Point startOffset = {};
    int startIndex = -1;
    float totalDelta = 0;
    bool axisLocked = false;
    bool active = false;
};

struct CarouselScrollGesture {
    Point startOffset = {};
    int startIndex = -1;
    float totalDelta = 0;
    bool active = false;
};

struct CarouselLoopLayout {
    float cycleExtent = 0;
    float trackGap = 0;
    float runwayExtent = 0;
    bool runwayReady = false;
    bool active = false;
};

struct CarouselState {
    Entity<CarouselState> self = {};
    int itemCount = 0;
    int selectedIndex = -1;
    Axis axis = Axis::Horizontal;
    bool looping = false;
    Point offset = {};
    Point maxOffset = {};
    Bounds viewport = {};
    Bounds frame = {};
    Vec<Bounds> items;
    bool hasViewport = false;
    bool hasFrame = false;
    CarouselPointerGesture pointerGesture = {};
    CarouselScrollGesture scrollGesture = {};
    bool ignoreScrollUntilQuiet = false;
    int scrollSettleEpoch = 0;
    bool suppressPointerClick = false;
    int motionRevision = 0;
    int geometryRevision = 0;
    CarouselLoopLayout loopLayout = {};
    bool geometryHasRunway = false;
    bool loopLayoutRemovalPending = false;
    Point loopMotionTarget = {};
    bool hasLoopMotionTarget = false;
    bool wheelBurstActive = false;
    int wheelBurstEpoch = 0;
    FocusHandle focus = {};
    bool focusRingSuppressed = false;
    OngoingScroll wheelLock = {};
    Vec<Bounds> pendingItems;
    Bounds pendingViewport = {};
    Bounds pendingFrame = {};
    bool pendingHasRunway = false;

    static CarouselState New(int itemCount);
    CarouselState& WithSelectedIndex(int index);
    CarouselState& WithAxis(Axis value);
    CarouselState& WithLooping(bool value = true);
    int ItemCount() const { return itemCount; }
    int SelectedIndex() const { return selectedIndex; }
    Axis GetAxis() const { return axis; }
    bool IsLooping() const { return looping; }
    bool HasPrevious() const;
    bool HasNext() const;
    void SetSelectedIndex(int index, Ctx* cx);
    void SetItemCount(int count, Ctx* cx);
    void SetAxis(Axis value, Ctx* cx);
    void SetLooping(bool value, Ctx* cx);
    bool SelectIndex(int index, Ctx* cx);
    bool SelectPrevious(Ctx* cx);
    bool SelectNext(Ctx* cx);
    bool SelectFirst(Ctx* cx);
    bool SelectLast(Ctx* cx);
    FocusHandle Focus(Ctx* cx);
    void SuppressFocusRing(bool suppressed);
    bool IsFocusRingSuppressed() const { return focusRingSuppressed; }
    bool IsInteracting() const;
    bool HasScrollGesture() const { return scrollGesture.active; }
    bool IsPointerDragLocked() const;
    bool ShouldSuppressPointerClick() const { return suppressPointerClick; }
    int MotionRevision() const { return motionRevision; }
    Size FrameSize() const;
    void SetGeometry(Bounds viewport, Bounds frame, const Bounds* itemBounds,
                     int n, bool hasRunway);
    void SetGeometry(Bounds viewport, const Bounds* itemBounds, int n);
    float LoopRunway() const;
    bool IsLoopLayoutTransitioning() const;
    Point LoopItemOffset(int index) const;
    Point MotionTargetFor(int index) const;
    bool HasMotionTargetFor(int index) const;
    bool SettleLoopMotion(Point rendered, Point* out, Ctx* cx);
    bool HasSnapTarget(int index) const;
    Point SnapTargetFor(int index) const;
    int NearestIndex(Point offset) const;
    bool NormalizeLoopCoordinate();
    bool BeginDrag(Point position, Ctx* cx);
    bool UpdateDrag(Point position, Ctx* cx);
    bool FinishDrag(Ctx* cx);
    bool HandleScrollDelta(Axis axis, float delta, TouchPhase phase, Ctx* cx);
    bool FinishScroll(bool cancelled, Ctx* cx);
    void DeferScrollToAncestor(Ctx* cx);
    bool HandleWheelStep(Axis axis, float delta, Ctx* cx);
    Point Offset() const { return offset; }
    void SetOffset(Point value) { offset = value; }
    void IngestPendingGeometry(Ctx* cx);

    static void OnAction(CarouselState* self, Ctx* cx,
                         const ActionEvent* event);
    static void OnPrevious(CarouselState* self, Ctx* cx,
                           const ClickEvent* event);
    static void OnNext(CarouselState* self, Ctx* cx, const ClickEvent* event);
    static void OnSelect(CarouselState* self, Ctx* cx, const ClickEvent* event,
                         int64_t index);
    static void OnPointerDown(CarouselState* self, Ctx* cx,
                              const MouseDownEvent* event);
    static void OnPointerMove(CarouselState* self, Ctx* cx,
                              const DragMoveEvent* event);
    static void OnPointerUp(CarouselState* self, Ctx* cx,
                            const MouseUpEvent* event);
    static void OnWheel(CarouselState* self, Ctx* cx,
                        const ScrollWheelEvent* event);
    static void OnScrollSettle(CarouselState* self, Ctx* cx,
                               const TickEvent* event, int64_t epoch);
    static void OnIgnoredScrollRecovery(CarouselState* self, Ctx* cx,
                                        const TickEvent* event, int64_t epoch);
    static void OnWheelBurstEnd(CarouselState* self, Ctx* cx,
                                const TickEvent* event, int64_t epoch);
    static void OnRootMouseDown(CarouselState* self, Ctx* cx,
                                const MouseDownEvent* event);
};

Entity<CarouselState> CarouselStateNew(App* app, int itemCount);
void CarouselInitKeys();

struct Carousel {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<CarouselState> state = {};
    Str accessibilityLabel = StrL("Carousel");
    bool focusRingEnabled = true;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static Carousel* New(Ctx* cx, Str id, Entity<CarouselState> state);
    Carousel* AccessibilityLabel(Str value);
    Carousel* FocusRing(bool enabled);
    Carousel* Child(El* child);
    Carousel* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct CarouselContent {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Entity<CarouselState> state = {};
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};
    Style trackStyle = {};
    uint32_t trackStyleSet = 0;

    static CarouselContent* New(Ctx* cx, Entity<CarouselState> state);
    CarouselContent* Child(El* child);
    CarouselContent* Refine(const Style& value, uint32_t fields);
    CarouselContent* TrackStyle(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct CarouselItem {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    int index = 0;
    Entity<CarouselState> state = {};
    Str accessibilityLabel = {};
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static CarouselItem* New(Ctx* cx, Str id, int index,
                             Entity<CarouselState> state);
    CarouselItem* AccessibilityLabel(Str value);
    CarouselItem* Child(El* child);
    CarouselItem* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct CarouselControl {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Entity<CarouselState> state = {};
    UiSize size = UiSize::Medium;
    Str accessibilityLabel = {};
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};
    bool next = false;

    CarouselControl* WithSize(UiSize value);
    CarouselControl* AccessibilityLabel(Str value);
    CarouselControl* Child(El* child);
    CarouselControl* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct CarouselPrevious : CarouselControl {
    static CarouselPrevious* New(Ctx* cx, Entity<CarouselState> state);
};

struct CarouselNext : CarouselControl {
    static CarouselNext* New(Ctx* cx, Entity<CarouselState> state);
};

struct CarouselPagination {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str accessibilityLabel = StrL("Carousel pagination");
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static CarouselPagination* New(Ctx* cx);
    CarouselPagination* AccessibilityLabel(Str value);
    CarouselPagination* Child(El* child);
    CarouselPagination* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct CarouselPaginationItem {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    int index = 0;
    Entity<CarouselState> state = {};
    UiSize size = UiSize::XSmall;
    Str accessibilityLabel = {};
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static CarouselPaginationItem* New(Ctx* cx, Str id, int index,
                                       Entity<CarouselState> state);
    CarouselPaginationItem* WithSize(UiSize value);
    CarouselPaginationItem* AccessibilityLabel(Str value);
    CarouselPaginationItem* Child(El* child);
    CarouselPaginationItem* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

}

template <>
struct EventEmitter<component::CarouselState, component::CarouselEvent> {};

}

#line 1 "src/ui/sankey.h"

#line 1 "src/ui/chart.h"

namespace gpui {

namespace component {

namespace plot {
struct Tooltip;
}

struct ChartTooltipSeriesRow {
    Rgba swatch = {};
    Str name = {};
    double value = 0;

    bool f64 = false;
};

plot::Tooltip* ChartTooltipApply(const ChartTooltipContent& content,
                                 plot::Tooltip* tooltip, const void* d,
                                 Str title, bool hasTitle,
                                 const ChartTooltipSeriesRow* rows, int count);

const float kChartMaxBandWidth = 30;

const float kChartHoverDotSize = 8;

const float kChartHoverHaloSize = 20;

uint32_t ChartCallerId(const Ctx* cx, const char* file, int line);

struct ChartAppear {
    bool enabled = true;
    uint64_t generation = 0;

    void SetEnabled(bool v) { enabled = v; }

    void SetKey(Str key);
    void SetKey(uint64_t key) { generation = key; }

    bool Generation(uint64_t* out) const {
        if (enabled && out) {
            *out = generation;
        }
        return enabled;
    }
};

struct PieSlice {
    float value = 0;
    Rgba color = {};

    float outerInset = 0;
    Str label = {};

    Str tooltipName = {};
    Str tooltipValue = {};
};

struct PieChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<PieSlice> slices;

    float outerRadius = 0;
    float innerRadius = 0;
    float padAngle = 0;

    bool hasLabels = false;

    float labelGap = 15;
    bool hasLabelColor = false;
    Rgba labelColor = {};
    Str tooltipName = {};

    uint32_t id = 0;

    bool interactive = true;

    ChartAppear appear = {};

    static PieChart* New(Ctx* cx, const char* file = __builtin_FILE(),
                         int line = __builtin_LINE());
    PieChart* Slice(float value, Rgba color, float outerInset = 0);
    PieChart* Label(Str text);
    PieChart* OuterRadius(float r);
    PieChart* InnerRadius(float r);
    PieChart* PadAngle(float radians);
    PieChart* LabelGap(float gap);
    PieChart* LabelColor(Rgba c);

    PieChart* Tooltip(Str name);

    PieChart* TooltipName(Str name);

    PieChart* TooltipValue(Str value);

    PieChart* Id(Str name);

    PieChart* Interactive(bool v);

    PieChart* Appear(bool v);

    PieChart* AppearKey(Str key);
    PieChart* AppearKey(uint64_t key);

    uint32_t PlotId() const { return id; }

    bool PlotInteractive() const { return interactive; }

    bool AppearGeneration(uint64_t* out) const {
        return appear.Generation(out);
    }

    float ResolveOuterRadius(float height) const;
    El* IntoEl();
};

struct PointAxes {
    bool yAxis = false;
    AxisLabelPlacement placement = AxisLabelPlacement::Outside;
    int yTickCount = 5;
    ChartTickFormatFn tickFormat = nullptr;
    void* tickFormatUser = nullptr;

    int xTickCount = -1;
    int gridColumns = 0;
    bool gridDashed = true;
    float yPaddingTop = 10;
    float yPaddingBottom = 0;
    ArenaVec<double> referenceLines;

    void ApplyTo(Arena* a, ChartSeries* chart) const;
};

struct AreaChart {
    Arena* a = nullptr;
    Str tooltipName = {};

    ChartTooltipContent tooltipContent = {};

    uint32_t id = 0;

    bool interactive = true;

    ChartAppear appear = {};
    Ctx* cx = nullptr;
    const float* ys = nullptr;

    const double* exact = nullptr;
    int n = 0;
    const char* const* labels = nullptr;

    int tickMargin = 1;

    bool overlay = false;
    Rgba stroke = {};
    Rgba fill = {};

    Rgba fillBottom = {};
    ChartStroke strokeStyle = ChartStroke::Natural;

    ArenaVec<ChartSeriesExtra> more;
    bool hasYDomain = false;
    float yDomainMin = 0;
    float yDomainMax = 0;
    int pointCount = 0;
    PointAxes axes;

    bool xAxis = true;
    bool grid = true;

    static AreaChart* New(Ctx* cx, const float* ys, int n,
                          const char* file = __builtin_FILE(),
                          int line = __builtin_LINE());

    static AreaChart* New(Ctx* cx, const double* ys, int n,
                          const char* file = __builtin_FILE(),
                          int line = __builtin_LINE());

    AreaChart* Y(const float* ys);

    AreaChart* Tooltip(Str name);

    AreaChart* TooltipTitle(ChartTooltipTitleFn fn, void* user = nullptr);

    AreaChart* TooltipValue(ChartTooltipValueFn fn, void* user = nullptr);

    AreaChart* TooltipValueColor(ChartTooltipValueColorFn fn,
                                 void* user = nullptr);

    AreaChart* TooltipContent(ChartTooltipContentFn fn, void* user = nullptr);

    AreaChart* Data(const void* items, int stride);
    template <typename T>
    AreaChart* Data(const T* items) {
        return Data((const void*)items, (int)sizeof(T));
    }

    AreaChart* Id(Str name);

    AreaChart* Interactive(bool v);

    AreaChart* Appear(bool v);

    AreaChart* AppearKey(Str key);
    AreaChart* AppearKey(uint64_t key);

    uint32_t PlotId() const { return id; }

    bool PlotInteractive() const { return interactive; }

    bool AppearGeneration(uint64_t* out) const {
        return appear.Generation(out);
    }
    AreaChart* Stroke(Rgba c);
    AreaChart* Fill(Rgba c);

    AreaChart* Fill(Rgba top, Rgba bottom);
    AreaChart* Labels(const char* const* l);
    AreaChart* TickMargin(int n);
    AreaChart* Overlay(bool v = true);

    AreaChart* Natural();
    AreaChart* Linear();

    AreaChart* XAxis(bool v);

    AreaChart* Grid(bool v);
    AreaChart* StepAfter();

    AreaChart* YDomain(float min, float max);

    AreaChart* PointCount(int count);

    AreaChart* YAxis(bool v = true);

    AreaChart* YAxisLabelPlacement(AxisLabelPlacement placement);

    AreaChart* YTickCount(int count);

    AreaChart* YTickFormat(ChartTickFormatFn format, void* user = nullptr);

    AreaChart* XTickCount(int count);

    AreaChart* GridColumns(int count);

    AreaChart* GridDashed(bool dashed);

    AreaChart* ReferenceLine(double value);

    AreaChart* YPadding(float top, float bottom);
    El* IntoEl();
};

struct LineChart {
    Arena* a = nullptr;
    Str tooltipName = {};

    ChartTooltipContent tooltipContent = {};

    uint32_t id = 0;

    bool interactive = true;

    ChartAppear appear = {};
    Ctx* cx = nullptr;
    const float* ys = nullptr;

    const double* exact = nullptr;
    int n = 0;
    const char* const* labels = nullptr;

    int tickMargin = 1;
    Rgba stroke = {};
    bool hasYDomain = false;
    float yDomainMin = 0;
    float yDomainMax = 0;
    int pointCount = 0;
    PointAxes axes;
    ChartStroke strokeStyle = ChartStroke::Natural;
    bool dot = false;

    bool xAxis = true;
    bool grid = true;

    static LineChart* New(Ctx* cx, const float* ys, int n,
                          const char* file = __builtin_FILE(),
                          int line = __builtin_LINE());

    static LineChart* New(Ctx* cx, const double* ys, int n,
                          const char* file = __builtin_FILE(),
                          int line = __builtin_LINE());

    LineChart* Tooltip(Str name);

    LineChart* TooltipTitle(ChartTooltipTitleFn fn, void* user = nullptr);

    LineChart* TooltipValue(ChartTooltipValueFn fn, void* user = nullptr);

    LineChart* TooltipValueColor(ChartTooltipValueColorFn fn,
                                 void* user = nullptr);

    LineChart* TooltipContent(ChartTooltipContentFn fn, void* user = nullptr);

    LineChart* Data(const void* items, int stride);
    template <typename T>
    LineChart* Data(const T* items) {
        return Data((const void*)items, (int)sizeof(T));
    }

    LineChart* Id(Str name);

    LineChart* Interactive(bool v);

    LineChart* Appear(bool v);

    LineChart* AppearKey(Str key);
    LineChart* AppearKey(uint64_t key);

    uint32_t PlotId() const { return id; }

    bool PlotInteractive() const { return interactive; }

    bool AppearGeneration(uint64_t* out) const {
        return appear.Generation(out);
    }
    LineChart* Stroke(Rgba c);
    LineChart* Labels(const char* const* l);
    LineChart* TickMargin(int n);

    LineChart* YDomain(float min, float max);

    LineChart* PointCount(int count);

    LineChart* YAxis(bool v = true);

    LineChart* YAxisLabelPlacement(AxisLabelPlacement placement);

    LineChart* YTickCount(int count);

    LineChart* YTickFormat(ChartTickFormatFn format, void* user = nullptr);

    LineChart* XTickCount(int count);

    LineChart* GridColumns(int count);

    LineChart* GridDashed(bool dashed);

    LineChart* ReferenceLine(double value);

    LineChart* YPadding(float top, float bottom);
    LineChart* Natural();
    LineChart* Linear();
    LineChart* StepAfter();
    LineChart* Dot(bool v = true);

    LineChart* XAxis(bool v);

    LineChart* Grid(bool v);
    El* IntoEl();
};

struct BarChart {
    Arena* a = nullptr;
    Str tooltipName = {};

    ChartTooltipContent tooltipContent = {};

    uint32_t id = 0;

    bool interactive = true;

    ChartAppear appear = {};
    Ctx* cx = nullptr;
    const float* ys = nullptr;

    const double* exact = nullptr;
    int n = 0;
    const char* const* labels = nullptr;
    int tickMargin = 1;
    Rgba fill = {};

    float paddingInner = 0.4f;
    float paddingOuter = 0.2f;

    float maxBandWidth = kChartMaxBandWidth;
    float minLength = 0;
    const Rgba* labelColors = nullptr;

    float radius = 0;
    float domainMin = 0;
    float domainMax = 0;
    BarAlign align = BarAlign::Bottom;

    const float* bases = nullptr;

    bool overlay = false;
    bool labelValues = false;

    bool valueAxis = false;
    int valueTickCount = 5;
    AxisLabelPlacement valueAxisLabelPlacement = AxisLabelPlacement::Outside;
    ChartTickFormatFn valueTickFormat = nullptr;
    void* valueTickFormatUser = nullptr;
    int bandCount = 0;
    int bandTickCount = -1;
    bool gridDashed = true;

    const Rgba* fills = nullptr;
    bool gradient = false;
    bool gradientPerBar = false;
    bool gradientDiagonal = false;
    Rgba gradientFrom = {};
    Rgba gradientTo = {};

    bool labelAxis = true;
    bool grid = true;

    static BarChart* New(Ctx* cx, const float* ys, int n,
                         const char* file = __builtin_FILE(),
                         int line = __builtin_LINE());

    static BarChart* New(Ctx* cx, const double* ys, int n,
                         const char* file = __builtin_FILE(),
                         int line = __builtin_LINE());

    BarChart* Tooltip(Str name);

    BarChart* TooltipTitle(ChartTooltipTitleFn fn, void* user = nullptr);

    BarChart* TooltipValue(ChartTooltipValueFn fn, void* user = nullptr);

    BarChart* TooltipValueColor(ChartTooltipValueColorFn fn,
                                void* user = nullptr);

    BarChart* TooltipContent(ChartTooltipContentFn fn, void* user = nullptr);

    BarChart* Data(const void* items, int stride);
    template <typename T>
    BarChart* Data(const T* items) {
        return Data((const void*)items, (int)sizeof(T));
    }

    BarChart* Id(Str name);

    BarChart* Interactive(bool v);

    BarChart* Appear(bool v);

    BarChart* AppearKey(Str key);
    BarChart* AppearKey(uint64_t key);

    uint32_t PlotId() const { return id; }

    bool PlotInteractive() const { return interactive; }

    bool AppearGeneration(uint64_t* out) const {
        return appear.Generation(out);
    }
    BarChart* Fill(Rgba c);
    BarChart* Labels(const char* const* l);
    BarChart* TickMargin(int n);

    BarChart* ValueAxis(bool on = true);

    BarChart* ValueTickCount(int count);

    BarChart* ValueAxisLabelPlacement(AxisLabelPlacement placement);

    BarChart* ValueTickFormat(ChartTickFormatFn format, void* user = nullptr);

    BarChart* BandCount(int count);

    BarChart* BandTickCount(int count);

    BarChart* GridDashed(bool dashed);

    BarChart* LabelAxis(bool v);

    BarChart* Grid(bool v);

    BarChart* PaddingInner(float v);

    BarChart* PaddingOuter(float v);

    BarChart* MaxBandWidth(float width);

    BarChart* MinLength(float length);
    BarChart* Radius(float v);
    BarChart* Domain(float lo, float hi);
    BarChart* Alignment(BarAlign v);
    BarChart* Base(const float* y0);
    BarChart* Overlay(bool v = true);

    BarChart* LabelValues(bool v = true);

    BarChart* LabelColors(const Rgba* colors);
    BarChart* Fills(const Rgba* colors);

    BarChart* FillGradient(Rgba from, Rgba to, bool perBar = false);

    BarChart* FillGradientDiagonal(Rgba from, Rgba to);
    El* IntoEl();
};

struct CandlestickChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const float* opens = nullptr;
    const float* highs = nullptr;
    const float* lows = nullptr;
    const float* closes = nullptr;
    int n = 0;
    const char* const* labels = nullptr;
    int tickMargin = 1;
    Rgba up = {};
    Rgba down = {};
    float padding = 0.3f;
    float bodyWidthRatio = 0.8f;

    float maxBandWidth = kChartMaxBandWidth;
    Str tooltipName = {};

    ChartTooltipContent tooltipContent = {};

    uint32_t id = 0;

    bool interactive = true;

    ChartAppear appear = {};

    static CandlestickChart* New(Ctx* cx, const float* opens,
                                 const float* highs, const float* lows,
                                 const float* closes, int n,
                                 const char* file = __builtin_FILE(),
                                 int line = __builtin_LINE());
    CandlestickChart* Tooltip(Str name);

    CandlestickChart* TooltipTitle(ChartTooltipTitleFn fn,
                                   void* user = nullptr);

    CandlestickChart* TooltipValue(ChartTooltipValueFn fn,
                                   void* user = nullptr);

    CandlestickChart* TooltipValueColor(ChartTooltipValueColorFn fn,
                                        void* user = nullptr);

    CandlestickChart* TooltipContent(ChartTooltipContentFn fn,
                                     void* user = nullptr);

    CandlestickChart* Data(const void* items, int stride);
    template <typename T>
    CandlestickChart* Data(const T* items) {
        return Data((const void*)items, (int)sizeof(T));
    }

    CandlestickChart* Id(Str name);

    CandlestickChart* Interactive(bool v);

    CandlestickChart* Appear(bool v);

    CandlestickChart* AppearKey(Str key);
    CandlestickChart* AppearKey(uint64_t key);

    uint32_t PlotId() const { return id; }

    bool PlotInteractive() const { return interactive; }

    bool AppearGeneration(uint64_t* out) const {
        return appear.Generation(out);
    }
    CandlestickChart* Colors(Rgba up, Rgba down);
    CandlestickChart* Labels(const char* const* l);
    CandlestickChart* TickMargin(int n);
    CandlestickChart* Padding(float v);
    CandlestickChart* BodyWidthRatio(float v);

    CandlestickChart* MaxBandWidth(float width);
    El* IntoEl();
};

struct RadarLabel {
    enum class Kind : uint8_t {
        Text,
        Element
    };

    Kind kind = Kind::Text;
    Str text = {};
    El* element = nullptr;

    static RadarLabel Text(Str text);
    static RadarLabel Element(El* element);
};

const float kRadarDefaultLabelGap = 10;

int RadarHoveredIndex(int n, float outerRadius, float labelGap, Point position,
                      Size size);

struct RadarChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const float* values = nullptr;

    const double* exact = nullptr;
    int n = 0;
    const RadarLabel* labels = nullptr;

    Rgba stroke = {};
    Rgba fill = {};

    ArenaVec<ChartSeriesExtra> more;

    bool lastFillSet = false;
    float domainMin = 0;
    float domainMax = 0;
    bool dot = false;
    float outerRadius = 0;
    int gridLevels = 4;

    bool grid = true;
    float labelGap = kRadarDefaultLabelGap;
    Rgba labelColor = {};
    bool hasLabelColor = false;

    Str tooltipName = {};

    ChartTooltipContent tooltipContent = {};

    uint32_t id = 0;

    bool interactive = true;

    ChartAppear appear = {};

    El* el = nullptr;

    static RadarChart* New(Ctx* cx, const float* values, int n,
                           const char* file = __builtin_FILE(),
                           int line = __builtin_LINE());

    static RadarChart* New(Ctx* cx, const double* values, int n,
                           const char* file = __builtin_FILE(),
                           int line = __builtin_LINE());

    RadarChart* Value(const float* ys);

    RadarChart* Tooltip(Str name);

    RadarChart* TooltipTitle(ChartTooltipTitleFn fn, void* user = nullptr);

    RadarChart* TooltipValue(ChartTooltipValueFn fn, void* user = nullptr);

    RadarChart* TooltipValueColor(ChartTooltipValueColorFn fn,
                                  void* user = nullptr);

    RadarChart* TooltipContent(ChartTooltipContentFn fn, void* user = nullptr);

    RadarChart* Data(const void* items, int stride);
    template <typename T>
    RadarChart* Data(const T* items) {
        return Data((const void*)items, (int)sizeof(T));
    }

    RadarChart* Id(Str name);

    RadarChart* Interactive(bool v);

    RadarChart* Appear(bool v);

    RadarChart* AppearKey(Str key);
    RadarChart* AppearKey(uint64_t key);

    uint32_t PlotId() const { return id; }

    bool PlotInteractive() const { return interactive; }

    bool AppearGeneration(uint64_t* out) const {
        return appear.Generation(out);
    }

    RadarChart* Stroke(Rgba c);
    RadarChart* Fill(Rgba c);
    RadarChart* Labels(const char* const* l);
    RadarChart* Labels(const RadarLabel* l);
    RadarChart* LabelColor(Rgba c);
    RadarChart* LabelGap(float v);

    RadarChart* MaxValue(float v);
    RadarChart* Domain(float lo, float hi);
    RadarChart* Dot(bool v = true);
    RadarChart* OuterRadius(float v);
    RadarChart* GridLevels(int v);

    RadarChart* Grid(bool v);

    float ResolveOuterRadius(float height) const;
    El* IntoEl();
};

const float kSankeyChartNodeWidth = 10;
const float kSankeyChartNodePadding = 16;
const float kSankeyChartLinkOpacity = 0.3f;
const float kSankeyChartMinLinkWidth = 1;
const float kSankeyChartLabelGap = 6;

const float kSankeyMaxLabelWidthRatio = 0.2f;
const float kSankeyMaxLabelMarginRatio = 0.6f;

struct SankeyLabel {
    Str text = {};
    Rgba color = {};
    float fontSize = 0;
    bool hasColor = false;

    static SankeyLabel New(Str text);
    SankeyLabel Color(Rgba color) const;
    SankeyLabel FontSize(float fontSize) const;
    float LineHeight() const;
};

struct SankeyChartNode {
    Str label = {};

    Str value = {};

    Str note = {};
    Rgba noteColor = {};
    Rgba color = {};
    bool hasColor = false;

    ArenaVec<SankeyLabel> labels;
    bool hasCustomLabels = false;

    Str tooltipName = {};
    Str tooltipValue = {};
};

struct SankeyChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;

    ArenaVec<SankeyChartNode> nodes;
    ArenaVec<SankeyLink> links;
    float nodeWidth = kSankeyChartNodeWidth;
    float nodePadding = kSankeyChartNodePadding;
    SankeyAlign align = SankeyAlign::Justify;
    int iterations = 6;
    SankeyValueScale valueScale = SankeyValueScale::Linear;
    float nodeRadius = 0;
    float linkOpacity = kSankeyChartLinkOpacity;
    float minLinkWidth = kSankeyChartMinLinkWidth;
    float labelGap = kSankeyChartLabelGap;

    bool showValues = false;
    Str tooltipName = {};

    uint32_t id = 0;

    bool interactive = true;

    ChartAppear appear = {};

    static SankeyChart* New(Ctx* cx, const char* file = __builtin_FILE(),
                            int line = __builtin_LINE());
    SankeyChart* Tooltip(Str name);

    SankeyChart* TooltipName(Str name);
    SankeyChart* TooltipValue(Str value);

    SankeyChart* Id(Str name);

    SankeyChart* Interactive(bool v);

    SankeyChart* Appear(bool v);

    SankeyChart* AppearKey(Str key);
    SankeyChart* AppearKey(uint64_t key);

    uint32_t PlotId() const { return id; }

    bool PlotInteractive() const { return interactive; }

    bool AppearGeneration(uint64_t* out) const {
        return appear.Generation(out);
    }

    SankeyChart* Node(Str label);
    SankeyChart* NodeColored(Str label, Rgba color);

    SankeyChart* NodeValue(Str text);
    SankeyChart* NodeNote(Str text, Rgba color);
    SankeyChart* CustomLabel(SankeyLabel label);
    SankeyChart* CustomLabels(const SankeyLabel* labels, int n);
    SankeyChart* Link(int source, int target, double value);
    SankeyChart* NodeWidth(float v);
    SankeyChart* NodePadding(float v);
    SankeyChart* NodeAlign(SankeyAlign v);
    SankeyChart* Iterations(int v);
    SankeyChart* ValueScale(SankeyValueScale v);
    SankeyChart* NodeCornerRadius(float v);
    SankeyChart* LinkOpacity(float v);
    SankeyChart* MinLinkWidth(float v);
    SankeyChart* LabelGap(float v);
    SankeyChart* ShowValues(bool v = true);
    El* IntoEl();
};

void SankeyChartThroughput(const SankeyLink* links, int nLinks, double* out,
                           int n);

}
}

#line 1 "src/ui/checkbox.h"

namespace gpui {

namespace component {

struct Checkbox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};

    Str accessibilityLabel = {};
    Str hint = {};

    El* child = nullptr;
    bool checked = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Str tooltip = {};
    bool focusRing = true;
    AccessibilityRole accessibilityRole = AccessibilityRole::CheckBox;
    int tabIndex = 0;
    bool tabStop = true;
    float w = 0;
    Listener onClick;

    static Checkbox* New(Ctx* cx, Str id);
    Checkbox* Label(Str s);

    Checkbox* AccessibilityLabel(Str s);
    Checkbox* Hint(Str s);
    Checkbox* Child(El* e);
    Checkbox* Checked(bool v);
    Checkbox* Disabled(bool v);
    Checkbox* WithSize(UiSize s);
    Checkbox* W(float v);

    Checkbox* FocusRing(bool v);
    Checkbox* Role(AccessibilityRole role);
    Checkbox* TabIndex(int v);
    Checkbox* TabStop(bool v);
    Checkbox* Tooltip(Str s);
    Checkbox* OnClick(Listener fn);

    Checkbox* OnChange(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/clipboard.h"

namespace gpui {

namespace component {

struct ClipboardEvent {
    Str value = {};
};

struct ClipboardState {
    bool copied = false;
    int timer = 0;
    Str value = {};
    Listener onCopied = {};

    ~ClipboardState();
    static void OnCopy(ClipboardState* self, Ctx* cx, const ClickEvent* ev);
    static void OnReset(ClipboardState* self, Ctx* cx, const TickEvent* ev);
};

struct Clipboard {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str value = {};
    Str tooltipText = {};
    Str accessibilityLabel = {};
    Listener onCopied;
    UiSize size = UiSize::XSmall;

    static Clipboard* New(Ctx* cx, Str id);
    Clipboard* Value(Str v);
    Clipboard* Tooltip(Str t);

    Clipboard* AccessibilityLabel(Str label);
    Clipboard* OnCopied(Listener fn);
    Clipboard* WithSize(UiSize sizeValue);
    El* IntoEl();
};

}
}

#line 1 "src/ui/collapsible.h"

namespace gpui {

namespace component {

struct Collapsible {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    bool open = false;

    Str motionId = {};
    bool hasMotion = false;
    El* trigger = nullptr;

    ArenaVec<El*> children;
    El* content = nullptr;

    int contentAt = 0;

    float width = 0;
    float gap = 0;

    static Collapsible* New(Ctx* cx);
    Collapsible* W(float v);
    Collapsible* Gap(float v);
    Collapsible* Open(bool v);
    Collapsible* MotionId(Str id);
    Collapsible* Trigger(El* e);
    Collapsible* Child(El* e);
    Collapsible* Content(El* e);
    El* IntoEl();
};

}
}

#line 1 "src/ui/color_picker.h"

namespace gpui {

namespace component {

Entity<ColorPickerState> ColorPickerStateFor(Ctx* cx, Str id);

struct ColorPicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};

    Str accessibilityLabel = {};

    IconName icon = IconName::None;
    UiSize size = UiSize::Medium;

    const uint32_t* featured = nullptr;
    int nFeatured = 0;

    Listener onChange;
    Entity<ColorPickerState> state = {};

    bool field = false;
    Str placeholder = {};

    Style style = {};
    uint32_t styleSet = 0;

    static ColorPicker* New(Ctx* cx, Str id);
    static ColorPicker* New(Ctx* cx, Entity<ColorPickerState> state);
    ColorPicker* Label(Str s);

    ColorPicker* AccessibilityLabel(Str s);
    ColorPicker* Icon(IconName v);
    ColorPicker* WithSize(UiSize s);
    ColorPicker* FeaturedColors(const uint32_t* colors, int n);
    ColorPicker* OnChange(Listener fn);
    ColorPicker* Refine(const Style& s, uint32_t fields);

    FocusHandle FocusHandleOf(Ctx* cx) const;
    El* IntoEl();
};

struct ColorSelect {
    ColorPicker* picker = nullptr;

    static ColorSelect* New(Ctx* cx, Entity<ColorPickerState> state);

    ColorSelect* FeaturedColors(const uint32_t* colors, int n);

    ColorSelect* Placeholder(Str s);

    ColorSelect* AccessibilityLabel(Str s);
    ColorSelect* WithSize(UiSize s);
    ColorSelect* Refine(const Style& s, uint32_t fields);
    FocusHandle FocusHandleOf(Ctx* cx) const;
    El* IntoEl();
};

}
}

#line 1 "src/base/list.h"

namespace gpui {

enum class ListEventKind : uint8_t {

    Select,

    Confirm,

    Cancel
};

struct ListEvent {
    ListEventKind kind = ListEventKind::Select;

    int index = -1;

    bool secondary = false;
};

struct ListSelectionChange {
    bool hasIndex = false;
    IndexPath index = {};
};

struct ListSearchRequest {

    Str query = {};
};

struct ListConfirmRequest {
    bool secondary = false;
};

enum class ListAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    Confirm,
    Cancel
};

struct ListKeyAction {
    ListAction action = ListAction::None;
    bool secondary = false;
};

void ListInitKeys();
Str ListContext();

ListKeyAction ListActionOf(uint32_t id, int64_t arg = 0);

enum class ListRowKind : uint8_t {
    Entry,
    SectionHeader,
    SectionFooter
};

struct ListRow {
    ListRowKind kind = ListRowKind::Entry;
    int section = 0;

    int row = 0;
    int entry = -1;

    IndexPath Path() const { return IndexPath{section, row, 0}; }
};

struct ListState {

    int count = 0;

    int selected = -1;

    int rightClicked = -1;
    bool selectable = true;

    bool resetOnCancel = true;

    Vec<int> sectionCounts;
    bool sectionHeaders = false;
    bool sectionFooters = false;

    float rowH = 32;
    float headerH = 0;
    float footerH = 0;

    IndexPath itemToMeasure = {};

    Vec<float> rowHeights;
    float scrollY = 0;
    float viewportH = 0;

    bool loading = false;
    bool hasMore = false;
    int loadMoreThreshold = 20;
    Listener onEvent = {};

    Listener onPerformSearch = {};
    Listener onSetSelectedIndex = {};
    Listener onSetRightClickedIndex = {};
    Listener onConfirm = {};
    Listener onCancel = {};
    Listener onLoadMore = {};

    InputState* queryInput = nullptr;
    Str lastQuery = {};

    Entity<ListState> self = {};

    FocusHandle focus = {};

    ~ListState() {
        StrFree(lastQuery);
        VecReset(sectionCounts);
        VecReset(rowHeights);
    }

    static void OnRowClick(ListState* self, Ctx* cx, const ClickEvent* ev,
                           int64_t ix);
    static void OnRowMouseDown(ListState* self, Ctx* cx,
                               const MouseDownEvent* ev, int64_t ix);
    static void OnScroll(ListState* self, Ctx* cx, const ScrollEvent* ev);
    static void OnQueryInput(ListState* self, Ctx* cx, const InputEvent* ev);
    static void OnMouseDownOut(ListState* self, Ctx* cx,
                               const MouseDownEvent* ev);
};

void ListSetSections(ListState* s, const int* counts, int n, bool headers,
                     bool footers);

void ListSetCount(ListState* s, int count);

int ListRowCount(const ListState* s);

ListRow ListRowAt(const ListState* s, int rowIx);

int ListRowOfEntry(const ListState* s, int entry);

void ListPrepareRowHeights(ListState* s, float itemH, float headerH,
                           float footerH);

const float* ListRowHeights(const ListState* s);

IndexPath ListIndexPathOf(const ListState* s, int entry);
int ListEntryOf(const ListState* s, IndexPath path);

void ListScrollToItem(ListState* s, int entry, ScrollStrategy strategy);

bool ListShouldLoadMore(const ListState* s, int lastVisibleRow);

void ListSetSelectedIndex(ListState* s, Ctx* cx, int entry, bool scroll = false,
                          ScrollStrategy strategy = ScrollStrategy::Top);
void ListSetRightClickedIndex(ListState* s, Ctx* cx, int entry);
bool ListSelectedIndex(const ListState* s, IndexPath* out);
bool ListRightClickedIndex(const ListState* s, IndexPath* out);
void ListSetItemToMeasureIndex(ListState* s, Ctx* cx, IndexPath path);

void ListSetQuery(ListState* s, Ctx* cx, Str query);
void ListRequestLoadMore(ListState* s, Ctx* cx);

int ListNextIndex(const ListState* s);
int ListPrevIndex(const ListState* s);

void ListPerform(ListState* s, Ctx* cx, ListAction act, bool secondary);

void ListOnAction(ListState* self, Ctx* cx, const ActionEvent* ev);

void ListBindKeys(Ctx* cx, El* root, Entity<ListState> state);

void ListClickRow(ListState* s, Ctx* cx, int ix, bool secondary);

void ListRightClickRow(ListState* s, Ctx* cx, int ix);

template <>
struct EventEmitter<ListState, ListEvent> {};

}

#line 1 "src/ui/list.h"

namespace gpui {

namespace component {

El* ListLoadingView(Ctx* cx, float h = 0);

struct ListItem {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    bool selected = false;
    bool secondarySelected = false;
    bool confirmed = false;
    bool disabled = false;

    IconName checkIcon = IconName::None;

    StateStyle style = {};

    Str accessibilityLabel = {};

    static ListItem* New(Ctx* cx, El* child);
    ListItem* Selected(bool v);
    ListItem* SecondarySelected(bool v);
    ListItem* Confirmed(bool v);
    ListItem* CheckIcon(IconName icon);
    ListItem* Disabled(bool v);
    ListItem* Style(const StateStyle& s);
    ListItem* AccessibilityLabel(Str label);
    El* IntoEl(Str id, Listener onClick, Listener onMouseDown);
};

ListItem* ListSeparatorItem(Ctx* cx, El* child = nullptr);

struct ListDelegate {
    void* data = nullptr;
    int (*sectionsCount)(Ctx* cx, void* data) = nullptr;
    int (*itemsCount)(Ctx* cx, void* data, int section) = nullptr;
    ListItem* (*renderItem)(Ctx* cx, void* data, int section, int row,
                            int entry) = nullptr;
    El* (*renderSectionHeader)(Ctx* cx, void* data, int section) = nullptr;
    El* (*renderSectionFooter)(Ctx* cx, void* data, int section) = nullptr;
    El* (*renderEmpty)(Ctx* cx, void* data) = nullptr;
    El* (*renderInitial)(Ctx* cx, void* data) = nullptr;
    bool (*isLoading)(Ctx* cx, void* data) = nullptr;
    El* (*renderLoading)(Ctx* cx, void* data) = nullptr;
    bool (*hasMore)(Ctx* cx, void* data) = nullptr;
    int (*loadMoreThreshold)(void* data) = nullptr;

    Listener performSearch = {};
    Listener setSelectedIndex = {};
    Listener setRightClickedIndex = {};
    Listener confirm = {};
    Listener cancel = {};
    Listener loadMore = {};
};

struct List {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<ListState> state = {};
    ListDelegate delegate = {};

    bool delegateSet = false;

    InputState* search = nullptr;
    Listener onSearchFocus;

    El* loading = nullptr;
    El* initial = nullptr;

    El* empty = nullptr;
    Str searchPlaceholder = {};
    UiSize size = UiSize::Medium;
    bool scrollbarVisible = true;

    float padding = 0;

    float h = 0;

    static List* New(Ctx* cx, Str id, Entity<ListState> state);
    List* WithDelegate(const ListDelegate& value);

    List* Sections(const int* counts, int n);
    List* Count(int n);
    List* Items(void* data,
                ListItem* (*fn)(Ctx*, void*, int section, int row, int entry));
    List* Headers(El* (*headerFn)(Ctx*, void*, int),
                  El* (*footerFn)(Ctx*, void*, int) = nullptr);
    List* Searchable(InputState* search, Listener onFocus);
    List* SearchPlaceholder(Str value);
    List* WithSize(UiSize value);
    List* ScrollbarVisible(bool value);
    List* Padding(float value);
    List* Loading(El* e);
    List* Initial(El* e);
    List* Empty(El* e);
    List* H(float px);
    El* IntoEl();
};

}
}

#line 1 "src/ui/searchable_list.h"

namespace gpui {

namespace component {

struct SearchableListItem {
    Str title = {};
    Str value = {};

    int section = 0;
    bool disabled = false;

    IconName icon = IconName::None;

    bool pinned = false;

    Str badge = {};

    Str display = {};
};

using SearchableItem = SearchableListItem;

struct SearchableGroup;
struct SearchableListState;
struct SearchableListChange;
struct SearchableListDelegate {
    void* user = nullptr;
    const SearchableListItem* items = nullptr;
    int nItems = 0;
    SearchableGroup* const* groups = nullptr;
    int nGroups = 0;
    int (*sectionsCount)(void* user, const App* app) = nullptr;
    Str (*sectionTitle)(void* user, int section) = nullptr;
    int (*itemsCount)(void* user, int section) = nullptr;
    const SearchableListItem* (*item)(void* user, IndexPath path) = nullptr;
    bool (*position)(void* user, Str value, IndexPath* out) = nullptr;
    bool (*matches)(void* user, const SearchableListItem* item,
                    Str query) = nullptr;
    El* (*renderItem)(void* user, Ctx* cx, IndexPath path,
                      const SearchableListItem* item, bool checked) = nullptr;
    El* (*renderSectionHeader)(void* user, Ctx* cx, int section) = nullptr;

    El* (*renderItemContent)(void* user, Ctx* cx, IndexPath path,
                             const SearchableListItem* item) = nullptr;
    bool (*isItemEnabled)(void* user, IndexPath path,
                          const SearchableListItem* item,
                          const App* app) = nullptr;
    bool (*isItemChecked)(void* user, IndexPath path,
                          const SearchableListItem* item,
                          const SearchableListState* state,
                          const App* app) = nullptr;
    void (*onWillChange)(void* user, SearchableListState* state,
                         const SearchableListChange* changes, int n) = nullptr;
    void (*onConfirm)(void* user, const SearchableListState* state,
                      IndexPath path, bool secondary) = nullptr;

    static SearchableListDelegate Items(const SearchableListItem* items,
                                        int nItems);
    static SearchableListDelegate Groups(SearchableGroup* const* groups,
                                         int nGroups);
    int SectionsCount(const App* app = nullptr) const;
    Str SectionTitle(int section) const;
    int ItemsCount(int section) const;
    const SearchableListItem* Item(IndexPath path) const;
    bool Position(Str value, IndexPath* out) const;
    bool Matches(const SearchableListItem* value, Str query) const;
    El* RenderItem(Ctx* cx, IndexPath path, const SearchableListItem* value,
                   bool checked) const;
    El* RenderSectionHeader(Ctx* cx, int section) const;
    bool IsItemEnabled(IndexPath path, const SearchableListItem* value,
                       const App* app) const;
    bool IsItemChecked(IndexPath path, const SearchableListItem* value,
                       const SearchableListState* state, const App* app) const;
    void OnWillChange(SearchableListState* state,
                      const SearchableListChange* changes, int n) const;
    void OnConfirm(const SearchableListState* state, IndexPath path,
                   bool secondary) const;
};

struct SearchableGroup {
    Str title = {};
    Vec<SearchableListItem> items;

    static SearchableGroup* New(Str title);
    SearchableGroup* Item(const SearchableListItem& item);
    SearchableGroup* Items(const SearchableListItem* items, int nItems);
    bool Matches(Str query) const;

    bool MatchedRows(Str query, Vec<int>* rows) const;
    ~SearchableGroup() { VecReset(items); }
};

struct SearchableVec {
    Vec<SearchableListItem> items;
    Vec<int> matched;

    static SearchableVec* New(const SearchableListItem* items, int nItems);
    SearchableVec* Push(const SearchableListItem& item);
    void PerformSearch(Str query);
    int ItemsCount(int section = 0) const;
    const SearchableListItem* Item(IndexPath path) const;
    bool Position(Str value, IndexPath* out) const;
    ~SearchableVec() {
        VecReset(items);
        VecReset(matched);
    }
};

struct SearchableGroupMatch {
    int ix = 0;

    int rowsStart = -1;
    int rowsLen = 0;
};

struct SearchableGroupVec {
    Vec<SearchableGroup*> groups;
    Vec<SearchableGroupMatch> matched;
    Vec<int> rows;

    static SearchableGroupVec* New(SearchableGroup* const* groups, int nGroups);
    void PerformSearch(Str query);
    int SectionsCount() const;
    Str SectionTitle(int section) const;
    int ItemsCount(int section) const;
    const SearchableListItem* Item(IndexPath path) const;
    bool Position(Str value, IndexPath* out) const;
    ~SearchableGroupVec();
};

struct SearchableListItemElement {
    Ctx* cx = nullptr;
    size_t index = 0;
    UiSize size = UiSize::Medium;
    bool selected = false;
    bool checked = false;
    bool disabled = false;
    IconName checkIcon = IconName::Check;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    static SearchableListItemElement* New(Ctx* cx, size_t index);
    SearchableListItemElement* Checked(bool value);
    SearchableListItemElement* CheckIcon(IconName value);
    SearchableListItemElement* Disabled(bool value);
    SearchableListItemElement* Selected(bool value);
    bool IsSelected() const;
    SearchableListItemElement* WithSize(UiSize value);
    SearchableListItemElement* Child(El* child);
    SearchableListItemElement* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

enum class SearchableListMode : uint8_t {
    Single,
    Multi
};

enum class SearchableListChangeKind : uint8_t {
    Select,
    Deselect
};

struct SearchableListChange {
    SearchableListChangeKind kind = SearchableListChangeKind::Select;
    int index = 0;
};

bool SearchableItemMatches(const SearchableItem* it, Str query);

struct SearchableListState {

    ListState list;
    SearchableListMode mode = SearchableListMode::Single;

    Vec<int> selected;
    bool open = false;

    Bounds triggerBounds = {};

    bool closedByConfirm = false;

    bool closeOnSelect = true;

    Vec<int> matches;

    const SearchableItem* items = nullptr;
    int nItems = 0;

    int maxSelected = 0;

    Listener onChange = {};

    Listener onClose = {};

    FocusHandle triggerFocus = {};
    FocusHandle contentFocus = {};
    FocusHandle previousFocus = {};
    SearchableListDelegate delegate = {};
    bool hasDelegate = false;

    bool suppressDelegateConfirm = false;

    const Vec<int>& Selection() const { return selected; }
    void SelectedValues(Vec<Str>* out) const;
    bool IsOpen() const { return open; }
    const FocusHandle* Focus() const { return &triggerFocus; }
    bool AddSelectedIndex(IndexPath index);
    bool RemoveSelectedIndex(IndexPath index);
    void SetSelectedIndices(const IndexPath* indices, int n);

    static void OnRowClick(SearchableListState* self, Ctx* cx,
                           const ClickEvent* ev, int64_t match);

    static void OnAction(SearchableListState* self, Ctx* cx,
                         const ActionEvent* ev);

    static void OnListAction(SearchableListState* self, Ctx* cx,
                             const ActionEvent* ev);

    ~SearchableListState() {
        VecReset(selected);
        VecReset(matches);
    }
};

void SearchableListSelectOnly(SearchableListState* s, int index);

void SearchableListChangesFor(const SearchableListState* s,
                              const SearchableItem* items, int nItems,
                              int index, Vec<SearchableListChange>* out);

void SearchableListApply(SearchableListState* s, const SearchableItem* items,
                         int nItems, const SearchableListChange* changes,
                         int n);

bool SearchableListIsChecked(const SearchableListState* s,
                             const SearchableItem* items, int nItems,
                             int index);

bool SearchableListIsEnabled(const SearchableListState* s,
                             const SearchableItem* items, int nItems,
                             int index);

bool SearchableListClick(SearchableListState* s, int index);

void SearchableListSearch(SearchableListState* s, const SearchableItem* items,
                          int nItems, Str query);

struct SearchableList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SearchableListState> state = {};
    const SearchableItem* items = nullptr;
    int nItems = 0;

    const Str* sections = nullptr;
    int nSections = 0;
    InputState* query = nullptr;
    Listener onQueryFocus = {};
    El* empty = nullptr;

    El* footer = nullptr;
    float w = 240;
    float maxH = 320;

    IconName checkIcon = IconName::Check;
    UiSize size = UiSize::Medium;
    SearchableListDelegate delegate = {};
    bool hasDelegate = false;

    bool inSelect = false;

    static SearchableList* New(Ctx* cx, Str id, Entity<SearchableListState> st,
                               InputState* query);
    SearchableList* Items(const SearchableItem* items, int n);
    SearchableList* Sections(const Str* titles, int n);
    SearchableList* OnQueryFocus(Listener fn);
    SearchableList* Empty(El* e);
    SearchableList* Footer(El* e);
    SearchableList* W(float v);
    SearchableList* MaxH(float v);
    SearchableList* CheckIcon(IconName n);
    SearchableList* WithSize(UiSize value);
    SearchableList* Delegate(const SearchableListDelegate& value);
    SearchableList* InSelect(bool v);
    El* IntoEl();
};

}
}

#line 1 "src/ui/select.h"

namespace gpui {

namespace component {

using SelectGroup = SearchableGroup;
using SelectDelegate = SearchableListDelegate;
using SelectItem = SearchableListItem;
using SelectListItem = SearchableListItemElement;

struct Caret {
    UiSize size = UiSize::Medium;
    Rgba color = {};
    bool hasColor = false;

    static Caret New(UiSize size);
    Caret TextColor(Rgba color) const;

    float IconSize(const Ctx* cx) const;
    El* IntoEl(const Ctx* cx, Arena* a) const;
};

struct SelectEvent {
    bool hasValue = false;
    IndexPath index = {};
    Str value = {};
};

struct SelectState {
    SearchableListState state;
    InputState queryInput;
    InputState* activeQuery = nullptr;
    bool searchable = false;
    IconName icon = IconName::None;
    Str titlePrefix = {};
    bool focusRingEnabled = true;
    Entity<SelectState> self = {};

    Window* blurWin = nullptr;
    int blurQueryFocus = 0;

    static Entity<SelectState> New(App* app);
    SearchableListState* List() { return &state; }
    const SearchableListState* List() const { return &state; }
    void Searchable(bool value);
    void SetItems(const SearchableItem* items, int nItems);
    void SetSelectedIndex(const IndexPath* selected, Ctx* cx);
    void SetSelectedIndex(int flatIndex, Ctx* cx);
    void SetSelectedValue(Str value, Ctx* cx);
    bool SelectedIndex(IndexPath* out) const;
    Str SelectedValue() const;
    void Focus(Window* win) const;
    void SetOpen(bool open, Ctx* cx);
    void ToggleMenu(Ctx* cx);
    void ClearQueryAndRestore(Ctx* cx);
    void Clean(Ctx* cx);
    void WatchBlur(Window* win);

    static void OnToggle(SelectState* self, Ctx* cx, const ClickEvent* event);
    static void OnListClose(SelectState* self, Ctx* cx, const TickEvent* event);
    static void OnListChange(SelectState* self, Ctx* cx,
                             const ListEvent* event);
    static void OnMouseDownOut(SelectState* self, Ctx* cx,
                               const MouseDownEvent* event);
    static void OnBlur(SelectState* self, Ctx* cx, const FocusHandle* lost);
};

Entity<SearchableListState> SelectListEntity(Entity<SelectState> state);

struct Select {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SearchableListState> state = {};
    Entity<SelectState> selectState = {};

    const SearchableItem* items = nullptr;
    int nItems = 0;
    const Str* sections = nullptr;
    int nSections = 0;
    Str placeholder = {};

    Str accessibilityLabel = {};
    Str titlePrefix = {};
    Str empty = {};
    El* emptyEl = nullptr;
    float width = kFill;
    float menuWidth = 0;
    float menuMaxH = 0;
    UiSize size = UiSize::Medium;
    IconName icon = IconName::None;

    IconName checkIcon = IconName::Check;
    bool disabled = false;
    bool cleanable = false;
    bool appearance = true;
    bool focusRing = true;

    InputState* query = nullptr;
    Listener onQueryFocus = {};

    El* trigger = nullptr;

    El* footer = nullptr;
    SearchableListDelegate delegate = {};
    bool hasDelegate = false;
    Listener onToggle;
    Listener onClear;
    Listener onMouseDownOut;
    Bounds* triggerBoundsOut = nullptr;
    Style triggerStyle = {};
    uint32_t triggerStyleSet = 0;

    ElRefiner triggerRefiner = {};

    static Select* New(Ctx* cx, Str id, Entity<SearchableListState> state);
    static Select* New(Ctx* cx, Str id, Entity<SelectState> state);
    Select* Items(const SearchableItem* items, int n);
    Select* Sections(const Str* titles, int n);
    Select* Placeholder(Str s);

    Select* AccessibilityLabel(Str s);
    Select* TitlePrefix(Str s);
    Select* Empty(Str s);
    Select* Empty(El* element);
    Select* W(float v);
    Select* MenuWidth(float v);
    Select* MenuMaxH(float v);
    Select* WithSize(UiSize s);
    Select* Icon(IconName n);
    Select* CheckIcon(IconName n);
    Select* Disabled(bool v);
    Select* Cleanable(bool v = true);
    Select* Appearance(bool v);

    Select* FocusRing(bool v);
    Select* Searchable(InputState* query, Listener onFocus);
    Select* Trigger(El* e);
    Select* Footer(El* e);
    Select* Delegate(const SearchableListDelegate& value);

    Select* Multiple(bool v = true);
    Select* OnToggle(Listener fn);
    Select* OnClear(Listener fn);
    Select* OnMouseDownOut(Listener fn);
    Select* TriggerBoundsOut(Bounds* bounds);
    Select* TriggerRefine(const Style& style, uint32_t fields);
    Select* TriggerRefiner(ElRefiner value);
    El* IntoEl();
};

Str SelectTriggerTitle(const SearchableListState* s, Str placeholder,
                       Str titlePrefix, Arena* a);

void SelectToggleOpen(SearchableListState* s, Ctx* cx);
void SelectToggleOpen(SelectState* s, Ctx* cx);

void SelectBindKeys(Ctx* cx, El* root, Entity<SearchableListState> state);
void SelectClear(SearchableListState* s, Ctx* cx);
void SelectClear(SelectState* s, Ctx* cx);

}

template <>
struct EventEmitter<component::SelectState, component::SelectEvent> {};

template <>
struct EventEmitter<component::SelectState, DismissEvent> {};

}

#line 1 "src/ui/combobox.h"

namespace gpui {

namespace component {

using ComboboxChange = SearchableListChange;

enum class ComboboxEventKind : uint8_t {
    Change,
    Confirm
};

struct ComboboxEvent {
    ComboboxEventKind kind = ComboboxEventKind::Change;
    const Str* values = nullptr;
    int nValues = 0;
};

struct ComboboxTriggerContext {
    const SearchableListState* state = nullptr;
    Str placeholder = {};
    bool hasPlaceholder = false;
    bool open = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;

    int SelectionCount() const;
    int SelectionIndex(int at) const;
    const SearchableListItem* SelectionItem(int at) const;
    Str Placeholder() const;
    bool IsOpen() const { return open; }
    bool IsDisabled() const { return disabled; }
    UiSize Size() const { return size; }
};

struct ComboboxState {
    SearchableListState state;
    InputState queryInput;
    Vec<int> selectionSnapshot;
    bool multiple = false;
    bool searchable = false;
    IconName triggerIcon = IconName::None;
    IconName checkIcon = IconName::Check;
    bool focusRingEnabled = true;
    Bounds bounds = {};
    Entity<ComboboxState> self = {};

    static Entity<ComboboxState> New(App* app);
    SearchableListState* List() { return &state; }
    const SearchableListState* List() const { return &state; }
    ComboboxState* Multiple(bool value);
    ComboboxState* Searchable(bool value);
    void SetItems(const SearchableListItem* items, int nItems);
    void SetDelegate(const SearchableListDelegate& delegate);
    void SelectedValues(Vec<Str>* out) const;
    Str SelectedValue() const;
    const Vec<int>& Selection() const { return state.selected; }
    void SetSelectedValues(const Str* values, int nValues, Ctx* cx);
    void SetSelectedIndices(const IndexPath* indices, int n, Ctx* cx);
    bool AddSelectedIndex(IndexPath index, Ctx* cx);
    bool RemoveSelectedIndex(IndexPath index, Ctx* cx);
    void ClearSelection(Ctx* cx);
    void Focus(Window* win) const;
    const FocusHandle* FocusHandleNow() const;
    Str Query() const { return InputValue(&queryInput); }
    void SetQuery(Str query, Ctx* cx);
    void SetOpen(bool open, Ctx* cx);
    void SyncSnapshot();
    void Emit(Ctx* cx, ComboboxEventKind kind);

    static void OnListChange(ComboboxState* self, Ctx* cx,
                             const ListEvent* event);
    static void OnToggle(ComboboxState* self, Ctx* cx, const ClickEvent* event);
    static void OnClear(ComboboxState* self, Ctx* cx, const ClickEvent* event);
    static void OnMouseDownOut(ComboboxState* self, Ctx* cx,
                               const MouseDownEvent* event);

    ~ComboboxState() { VecReset(selectionSnapshot); }
};

Entity<SearchableListState> ComboboxListEntity(Entity<ComboboxState> state);

struct Combobox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SearchableListState> state = {};
    Entity<ComboboxState> comboboxState = {};
    const SearchableItem* items = nullptr;
    int nItems = 0;
    const Str* sections = nullptr;
    int nSections = 0;
    Str placeholder = {};
    Str searchPlaceholder = {};
    Str empty = {};

    IconName icon = IconName::None;

    IconName checkIcon = IconName::Check;

    float width = kFill;
    float menuWidth = 0;
    float menuMaxH = 0;
    UiSize size = UiSize::Medium;
    bool disabled = false;
    bool cleanable = false;
    bool appearance = true;
    bool focusRing = true;
    InputState* query = nullptr;
    Listener onQueryFocus = {};

    El* trigger = nullptr;
    El* footer = nullptr;
    void* triggerData = nullptr;
    void* footerData = nullptr;
    void* emptyData = nullptr;
    El* (*renderTrigger)(Ctx* cx, void* data,
                         const ComboboxTriggerContext* trigger) = nullptr;
    El* (*renderFooter)(Ctx* cx, void* data) = nullptr;
    El* (*renderEmpty)(Ctx* cx, void* data) = nullptr;
    SearchableListDelegate delegate = {};
    bool hasDelegate = false;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};
    Listener onToggle;
    Listener onClear;

    static Combobox* New(Ctx* cx, Str id, Entity<SearchableListState> state,
                         InputState* query);
    static Combobox* New(Ctx* cx, Str id, Entity<ComboboxState> state);
    Combobox* Items(const SearchableItem* items, int n);
    Combobox* Sections(const Str* titles, int n);
    Combobox* Placeholder(Str s);
    Combobox* SearchPlaceholder(Str s);
    Combobox* Empty(Str s);
    Combobox* Icon(IconName n);
    Combobox* CheckIcon(IconName n);
    Combobox* W(float v);
    Combobox* MenuWidth(float v);
    Combobox* MenuMaxH(float v);
    Combobox* WithSize(UiSize value);
    Combobox* Disabled(bool v);
    Combobox* Cleanable(bool v = true);
    Combobox* Appearance(bool v);

    Combobox* FocusRing(bool v);
    Combobox* Multiple(bool v = true);
    Combobox* Trigger(El* e);
    Combobox* RenderTrigger(void* data,
                            El* (*fn)(Ctx* cx, void* data,
                                      const ComboboxTriggerContext* trigger));
    Combobox* Footer(El* e);
    Combobox* RenderFooter(void* data, El* (*fn)(Ctx* cx, void* data));
    Combobox* RenderEmpty(void* data, El* (*fn)(Ctx* cx, void* data));
    Combobox* Delegate(const SearchableListDelegate& value);
    Combobox* Refine(const Style& value, uint32_t fields);
    Combobox* Refiner(ElRefiner value);

    Combobox* MaxSelected(int n);
    Combobox* OnQueryFocus(Listener fn);
    Combobox* OnToggle(Listener fn);
    Combobox* OnClear(Listener fn);
    El* IntoEl();
};

}

template <>
struct EventEmitter<component::ComboboxState, component::ComboboxEvent> {};

}

#line 1 "src/ui/kbd.h"

namespace gpui {

namespace component {

struct Keystroke {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    bool platform = false;
    Str key = {};
};

int KbdFormat(Keystroke stroke, char* out, int cap);

Str KbdFormatStr(Ctx* cx, Keystroke stroke);

bool KeystrokeParse(Arena* a, Str source, Keystroke* out);
TempStr KeystrokeParseErrorTemp(Str source);

bool KeystrokeForAction(uint32_t action, const char* context, Keystroke* out);

bool KeystrokeForActionAtFocus(Ctx* cx, uint32_t action, FocusHandle focus,
                               Keystroke* out);

struct Kbd {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str stroke = {};
    bool appearance = true;
    bool outline = false;

    static Kbd* New(Ctx* cx, Str stroke);

    static Kbd* New(Ctx* cx, Keystroke stroke);

    static Kbd* ForAction(Ctx* cx, uint32_t action,
                          const char* context = nullptr);
    static Kbd* ForActionAtFocus(Ctx* cx, uint32_t action, FocusHandle focus);
    Kbd* Appearance(bool v);
    Kbd* Outline();
    El* IntoEl();
};

}
}

#line 1 "src/ui/command.h"

namespace gpui {

namespace component {

struct CommandItem {
    Str label = {};

    const Str* keywords = nullptr;
    int nKeywords = 0;
    IconName icon = IconName::None;

    uint32_t action = 0;
    int64_t actionArg = 0;

    const char* actionContext = nullptr;

    bool checked = false;
    bool disabled = false;

    El* (*content)(Ctx* cx, const CommandItem* item) = nullptr;

    int64_t data = 0;
};

struct CommandGroup {
    Str heading = {};
    const CommandItem* items = nullptr;
    int nItems = 0;
};

enum class CommandEntryKind : uint8_t {
    Item,
    Group,

    Separator
};

struct CommandEntry {
    CommandEntryKind kind = CommandEntryKind::Item;
    CommandItem item = {};
    CommandGroup group = {};
};

CommandEntry CommandEntryOf(const CommandItem& item);
CommandEntry CommandEntryOf(const CommandGroup& group);
CommandEntry CommandSeparatorEntry();

bool CommandItemMatches(const CommandItem* item, Str query);

enum class CommandRowKind : uint8_t {
    Heading,
    Item,
    Separator
};

struct CommandRow {
    CommandRowKind kind = CommandRowKind::Item;
    Str heading = {};

    int match = 0;
};

struct CommandMatch {
    int entry = 0;
    int itemIx = 0;

    IndexPath path = {};
    int row = 0;
    bool disabled = false;
    int64_t data = 0;
};

enum class CommandEventKind : uint8_t {
    Query,
    Select,
    Confirm,
    Cancel
};

struct CommandEvent {
    CommandEventKind kind = CommandEventKind::Select;
    IndexPath path = {};
    Str query = {};
    int64_t data = 0;
};

Str CommandContext();
void CommandInitKeys();

struct CommandState {

    InputState query;
    VirtualListScrollHandle scroll = {};

    const CommandEntry* entries = nullptr;
    int nEntries = 0;
    bool searchable = true;

    bool filterable = true;
    Vec<CommandRow> rows;
    Vec<CommandMatch> matched;

    Vec<float> rowSizes;
    bool needsMeasure = true;

    uint64_t layoutKey = 0;

    float listW = -1;
    float listFont = 0;
    float measuredW = -1;
    float measuredFont = 0;

    float measuredRem = 0;

    uint32_t measuredKeymap = 0;

    int measureCount = 0;

    int selected = -1;

    bool preserveNoSelection = false;
    bool loading = false;

    int pendingScroll = -1;

    Vec<char> applied;
    Listener onQuery = {};
    Listener onSelect = {};
    Listener onConfirm = {};
    Listener onCancel = {};

    static void OnRowClick(CommandState* self, Ctx* cx, const ClickEvent* ev,
                           int64_t match);
    static void OnRowHover(CommandState* self, Ctx* cx, const HoverEvent* ev,
                           int64_t match);
    static void OnAction(CommandState* self, Ctx* cx, const ActionEvent* ev);

    ~CommandState() {
        VecReset(rows);
        VecReset(matched);
        VecReset(rowSizes);
        VecReset(applied);
    }
};

void CommandInstall(CommandState* s, Ctx* cx, const CommandEntry* entries,
                    int nEntries, bool searchable, bool filterable = true);

bool CommandSelectedIndex(const CommandState* s, IndexPath* out);

void CommandSetSelectedIndex(CommandState* s, Ctx* cx, const IndexPath* path);

void CommandSelectBy(CommandState* s, Ctx* cx, int step);

int CommandMatchedCount(const CommandState* s);

void CommandSetQuery(CommandState* s, Ctx* cx, Str query);

void CommandSetLoading(CommandState* s, Ctx* cx, bool loading);

struct Command {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<CommandState> state = {};
    const CommandEntry* entries = nullptr;
    int nEntries = 0;
    bool searchable = true;
    bool filterable = true;
    Str placeholder = {};

    El* empty = nullptr;
    El* header = nullptr;
    El* footer = nullptr;

    El* (*headerFn)(Ctx* cx, const CommandState* s) = nullptr;

    float maxH = -1;
    bool bordered = true;
    float w = kFill;

    Style style = {};
    uint32_t styleSet = 0;
    Listener onQuery = {};
    Listener onSelect = {};
    Listener onConfirm = {};
    Listener onCancel = {};

    static Command* New(Ctx* cx, Str id, Entity<CommandState> state);
    Command* Entries(const CommandEntry* entries, int n);

    Command* Items(const CommandItem* items, int n);
    Command* Searchable(bool v);

    Command* Filterable(bool v);
    Command* Placeholder(Str s);
    Command* Empty(El* e);
    Command* Header(El* e);
    Command* Header(El* (*build)(Ctx* cx, const CommandState* s));
    Command* Footer(El* e);
    Command* MaxH(float v);
    Command* Bordered(bool v);
    Command* W(float v);
    Command* Refine(const Style& s, uint32_t fields);
    Command* OnQuery(Listener fn);
    Command* OnSelect(Listener fn);
    Command* OnConfirm(Listener fn);
    Command* OnCancel(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/component_traits.h"

#line 1 "src/ui/time.h"

namespace gpui {

namespace component {

struct Calendar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int year = 2026;
    int month = 1;
    int day = 1;
    int selectedYear = 0;
    int selectedMonth = 0;
    LocalDate rangeEnd = {};
    UiSize size = UiSize::Medium;
    int numberOfMonths = 1;
    CalendarView view = CalendarView::Day;
    int yearMin = 0;
    int yearMax = 0;
    int yearPageStart = 0;
    DateMatcher disabledMatcher = {};
    Entity<CalendarState> state = {};
    int firstDayOfWeek = 0;
    Style style = {};
    uint32_t styleSet = 0;

    bool bare = false;
    Listener onDay;
    Listener onDate;
    Listener onPrev;
    Listener onNext;
    Listener onMonthToggle;
    Listener onYearToggle;
    Listener onMonth;
    Listener onYear;

    static Calendar* New(Ctx* cx);

    static Calendar* New(Ctx* cx, Entity<CalendarState> state);
    Calendar* Year(int y);
    Calendar* Month(int m);
    Calendar* Day(int d);
    Calendar* Selection(int y, int m, int d);
    Calendar* RangeEnd(int y, int m, int d);
    Calendar* WithSize(UiSize s);
    Calendar* NumberOfMonths(int count);
    Calendar* FirstDayOfWeek(int weekday);
    Calendar* View(CalendarView value);
    Calendar* YearRange(int minYear, int maxYear, int pageStart);
    Calendar* DisabledMatcher(DateMatcher matcher);
    Calendar* Bare();
    Calendar* Refine(const Style& value, uint32_t fields);
    Calendar* OnDay(Listener fn);
    Calendar* OnDate(Listener fn);
    Calendar* OnPrev(Listener fn);
    Calendar* OnNext(Listener fn);
    Calendar* OnMonthToggle(Listener fn);
    Calendar* OnYearToggle(Listener fn);
    Calendar* OnMonth(Listener fn);
    Calendar* OnYear(Listener fn);
    El* IntoEl();
};

using ::gpui::HourCycle;
using ::gpui::TimeFieldEvent;
using ::gpui::TimeFieldState;
using ::gpui::TimePrecision;
using ::gpui::TimeSegment;

struct TimeField {
    Ctx* cx = nullptr;
    Str id = {};
    Entity<TimeFieldState> state = {};
    UiSize size = UiSize::Medium;
    Style style = {};
    uint32_t styleSet = 0;
    bool disabled = false;
    bool invalid = false;

    static TimeField* New(Ctx* cx, Entity<TimeFieldState> state);

    TimeField* WithId(Str id);

    TimeField* Invalid(bool v = true);
    TimeField* WithSize(UiSize s);
    TimeField* Disabled(bool v = true);
    TimeField* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct LocalDateTime {
    LocalDate date = {};
    LocalTime time = {};
};

struct DateTime {
    DateKind kind = DateKind::Single;
    LocalDate startDate = {};
    LocalTime startTime = {};
    LocalDate endDate = {};
    LocalTime endTime = {};

    static DateTime Single(LocalDateTime value);
    static DateTime Single();
    static DateTime Range(LocalDateTime start, LocalDateTime end);
    bool IsSome() const;
    bool IsComplete() const;
    bool Start(LocalDateTime* out) const;
    bool End(LocalDateTime* out) const;

    Date DateValue() const;

    Str Format(Arena* a, Str pattern) const;
};

enum class DatePickerEventKind : uint8_t {
    Change
};

struct DatePickerEvent {
    DatePickerEventKind kind = DatePickerEventKind::Change;
    DateTime value = {};
};

enum class DateRangePresetValueKind : uint8_t {
    Single,
    Range,
    DateTime
};

struct DateRangePresetValue {
    DateRangePresetValueKind kind = DateRangePresetValueKind::Single;
    LocalDate start = {};
    LocalDate end = {};

    ::gpui::component::DateTime dateTime = {};

    static DateRangePresetValue Single(LocalDate date);
    static DateRangePresetValue Range(LocalDate start, LocalDate end);
    static DateRangePresetValue WithDateTime(::gpui::component::DateTime value);
    Date IntoDate() const;
};

struct DateRangePreset {
    Str label = {};
    DateRangePresetValue value = {};

    LocalDate start = {};
    LocalDate end = {};
    int64_t arg = 0;

    static DateRangePreset Single(Str label, LocalDate date, int64_t arg = 0);
    static DateRangePreset Range(Str label, LocalDate start, LocalDate end,
                                 int64_t arg = 0);

    static DateRangePreset WithDateTime(Str label,
                                        ::gpui::component::DateTime value);
};

enum class DateFormat : uint8_t {
    Slash,
    Dash
};

struct DatePickerState {
    Entity<DatePickerState> self = {};
    FocusHandle focus = {};
    Date date = {};
    bool open = false;
    Entity<CalendarState> calendar = {};

    Str dateFormat = {};

    bool hasTimePrecision = false;
    TimePrecision timePrecision = TimePrecision::Minute;
    HourCycle hourCycle = HourCycle::H23;
    LocalTime defaultTime = {};
    LocalTime startTime = {};

    LocalTime endTime = {};
    Entity<TimeFieldState> timeField = {};
    bool timeFieldPushed = false;
    Subscription timeFieldSubscription = {};
    int numberOfMonths = 1;
    Matcher disabledMatcher = {};
    Subscription calendarSubscription = {};
    int firstDayOfWeek = 0;
    Bounds bounds = {};

    ~DatePickerState();

    static void OnCalendar(DatePickerState* self, Ctx* cx,
                           const CalendarEvent* ev);
    static void OnTimeField(DatePickerState* self, Ctx* cx,
                            const TimeFieldEvent* ev);
    static void OnToggle(DatePickerState* self, Ctx* cx, const ClickEvent* ev);
    static void OnOpenChange(DatePickerState* self, Ctx* cx,
                             const ClickEvent* ev, int64_t open);
    static void OnDismiss(DatePickerState* self, Ctx* cx,
                          const MouseUpEvent* ev);
    static void OnClear(DatePickerState* self, Ctx* cx, const ClickEvent* ev);
};

Entity<DatePickerState> DatePickerStateNew(Ctx* cx, bool range = false);
inline Entity<DatePickerState> DatePickerStateRange(Ctx* cx) {
    return DatePickerStateNew(cx, true);
}
void DatePickerStateSetDate(DatePickerState* state, Date date, Ctx* cx,
                            bool emit = false);
void DatePickerStateSetDateFormat(DatePickerState* state, Str format,
                                  Ctx* cx = nullptr);
void DatePickerStateSetNumberOfMonths(DatePickerState* state, int count,
                                      Ctx* cx = nullptr);
void DatePickerStateSetFirstDayOfWeek(DatePickerState* state, int weekday,
                                      Ctx* cx = nullptr);
void DatePickerStateSetDisabledMatcher(DatePickerState* state, Matcher matcher,
                                       Ctx* cx = nullptr);
void DatePickerStateSetYearRange(DatePickerState* state, int minYear,
                                 int maxYear, Ctx* cx = nullptr);
void DatePickerStateSelectPreset(DatePickerState* state,
                                 const DateRangePreset& preset, Ctx* cx,
                                 bool emit = true);

void DatePickerStateSetTimePrecision(DatePickerState* state,
                                     TimePrecision precision);

void DatePickerStateSetHourCycle(DatePickerState* state, HourCycle hourCycle);

void DatePickerStateSetDefaultTime(DatePickerState* state, LocalTime time);

DateTime DatePickerStateDateTime(const DatePickerState* state);

void DatePickerStateSetDateTime(DatePickerState* state, DateTime value,
                                Ctx* cx);

Str DatePickerStateDisplayFormat(Arena* a, const DatePickerState* state);

Str DatePickerFormatDate(Arena* a, Str pattern, LocalDate date,
                         LocalTime time = {});
Str DatePickerFormatValue(Arena* a, Str pattern, Date date);

struct DatePicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    int year = 2026;
    int month = 1;
    int day = 1;
    int viewYear = 0;
    int viewMonth = 0;

    int year2 = 0;
    int month2 = 0;
    int day2 = 0;
    Str placeholder = {};
    DateFormat format = DateFormat::Slash;
    UiSize size = UiSize::Medium;
    float width = kFill;

    bool cleanable = false;
    bool appearance = true;
    bool focusRing = true;
    bool disabled = false;
    bool range = false;
    bool open = false;
    int numberOfMonths = 1;
    CalendarView calendarView = CalendarView::Day;
    int yearMin = 0;
    int yearMax = 0;
    int yearPageStart = 0;
    DateMatcher disabledMatcher = {};
    const DateRangePreset* presets = nullptr;
    int presetsCount = 0;
    Listener onToggle;
    Listener onDay;
    Listener onDate;
    Listener onClear;
    Listener onPrev;
    Listener onNext;
    Listener onMonthToggle;
    Listener onYearToggle;
    Listener onMonth;
    Listener onYear;
    Listener onPreset;
    Entity<DatePickerState> state = {};
    Style style = {};
    uint32_t styleSet = 0;

    static DatePicker* New(Ctx* cx);
    static DatePicker* New(Ctx* cx, Entity<DatePickerState> state);
    DatePicker* Id(Str value);
    DatePicker* Year(int y);
    DatePicker* Month(int m);
    DatePicker* Day(int d);
    DatePicker* View(int y, int m);
    DatePicker* RangeEnd(int y, int m, int d);
    DatePicker* Placeholder(Str s);
    DatePicker* Format(DateFormat f);
    DatePicker* WithSize(UiSize s);
    DatePicker* W(float v);
    DatePicker* Cleanable(bool v = true);
    DatePicker* Appearance(bool v);

    DatePicker* FocusRing(bool v);
    DatePicker* Refine(const Style& value, uint32_t fields);
    DatePicker* Disabled(bool v = true);
    DatePicker* Range(bool v = true);
    DatePicker* NumberOfMonths(int count);
    DatePicker* CalendarMode(CalendarView value);
    DatePicker* YearRange(int minYear, int maxYear, int pageStart);
    DatePicker* DisabledMatcher(DateMatcher matcher);
    DatePicker* Presets(const DateRangePreset* values, int count,
                        Listener onSelect = {});
    DatePicker* Open(bool v);
    DatePicker* OnToggle(Listener fn);
    DatePicker* OnDay(Listener fn);
    DatePicker* OnDate(Listener fn);
    DatePicker* OnClear(Listener fn);
    DatePicker* OnPrev(Listener fn);
    DatePicker* OnNext(Listener fn);
    DatePicker* OnMonthToggle(Listener fn);
    DatePicker* OnYearToggle(Listener fn);
    DatePicker* OnMonth(Listener fn);
    DatePicker* OnYear(Listener fn);
    El* IntoEl();
};

}

template <>
struct EventEmitter<component::DatePickerState, component::DatePickerEvent> {};

}

#line 1 "src/ui/description_list.h"

namespace gpui {

namespace component {

enum class DescriptionTextKind : uint8_t {
    String,
    Text,
    AnyElement
};

struct DescriptionText {
    DescriptionTextKind kind = DescriptionTextKind::String;
    Str string = {};
    El* element = nullptr;

    static DescriptionText From(Str text);
    static DescriptionText Text(El* text);
    static DescriptionText AnyElement(El* element);
    El* IntoEl(Ctx* cx) const;
};

struct DescriptionItem {
    DescriptionText label = {};
    DescriptionText value = {};
    int span = 1;
    bool separator = false;

    static DescriptionItem New(DescriptionText label);
    static DescriptionItem Separator();
    DescriptionItem& Value(DescriptionText value);
    DescriptionItem& Span(int spanValue);
};

int DescriptionGroupRows(const DescriptionItem* items, int n, int columns,
                         int* rowCounts = nullptr, int capacity = 0);

struct DescriptionList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<DescriptionItem> items;
    int columns = 3;
    float labelWidth = 120;
    bool bordered = true;

    bool vertical = false;
    UiSize size = UiSize::Medium;

    static DescriptionList* New(Ctx* cx);
    static DescriptionList* Horizontal(Ctx* cx);
    static DescriptionList* Vertical(Ctx* cx);

    DescriptionList* Item(Str label, Str value, int span = 1);
    DescriptionList* Item(DescriptionText label, DescriptionText value,
                          int span = 1);
    DescriptionList* ItemEl(Str label, El* value, int span = 1);
    DescriptionList* Child(const DescriptionItem& item);
    DescriptionList* Separator();
    DescriptionList* Columns(int n);
    DescriptionList* LabelWidth(float w);
    DescriptionList* Bordered(bool v);
    DescriptionList* Layout(Axis axis);
    DescriptionList* Vertical(bool v = true);
    DescriptionList* WithSize(UiSize s);
    El* IntoEl();
};

}
}

#line 1 "src/ui/dialog.h"

namespace gpui {

namespace component {

constexpr float ANIMATION_DURATION = 250.f;

struct DialogButtonProps {
    Str okText = {};
    ButtonVariant okVariant = ButtonVariant::Primary;
    bool hasOkVariant = false;
    Str cancelText = {};
    ButtonVariant cancelVariant = ButtonVariant::Default;
    bool hasCancelVariant = false;
    bool showCancel = false;
    bool hasShowCancel = false;
    Listener onOk = {};
    Listener onCancel = {};
    Listener onClose = {};

    DialogButtonProps* OkText(Str value);
    DialogButtonProps* OkVariant(ButtonVariant value);
    DialogButtonProps* CancelText(Str value);
    DialogButtonProps* CancelVariant(ButtonVariant value);
    DialogButtonProps* ShowCancel(bool value = true);
    DialogButtonProps* OnOk(Listener value);
    DialogButtonProps* OnCancel(Listener value);
    DialogButtonProps* OnClose(Listener value);

    void Merge(const DialogButtonProps& other);

    bool IsCancelShown() const;
    El* RenderOk(Ctx* cx, Str id, bool outline = false) const;
    El* RenderCancel(Ctx* cx, Str id) const;
};

struct DialogContent {
    El* root = nullptr;
    static DialogContent* New(Ctx* cx);
    DialogContent* Child(El* child);
    El* IntoEl();
};

struct DialogHeader {
    El* root = nullptr;
    static DialogHeader* New(Ctx* cx);
    DialogHeader* Child(El* child);
    El* IntoEl();
};

struct DialogTitle {
    El* root = nullptr;
    static DialogTitle* New(Ctx* cx);
    DialogTitle* Child(El* child);
    El* IntoEl();
};

struct DialogDescription {
    El* root = nullptr;
    static DialogDescription* New(Ctx* cx);
    DialogDescription* Child(El* child);
    El* IntoEl();
};

struct DialogFooterButton {
    bool cancel = false;
    bool action = false;
    bool IsCancel() const { return cancel; }
    bool IsAction() const { return action; }
};

struct DialogFooter {
    El* root = nullptr;
    static DialogFooter* New(Ctx* cx);
    DialogFooter* Child(El* child);
    El* IntoEl();
};

struct DialogClose {
    Ctx* cx = nullptr;
    El* root = nullptr;
    El* slot = nullptr;
    DialogFooterButton semantic = {true, false};
    static DialogClose* New(Ctx* cx);
    DialogClose* Child(El* child);

    DialogClose* Trigger(Button* button);
    El* IntoEl();
};

struct DialogAction {
    El* root = nullptr;
    DialogFooterButton semantic = {false, true};
    static DialogAction* New(Ctx* cx);
    DialogAction* Child(El* child);
    El* IntoEl();
};

struct Dialog {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    Str description = {};
    bool open = false;
    El* body = nullptr;

    El* surface = nullptr;
    DialogButtonProps buttonProps = {};

    float width = 448;
    float height = 0;

    bool overlay = true;
    bool overlayClosable = true;

    bool keyboard = true;

    bool alertHost = false;

    int layerIx = 0;
    float radius = 0;
    Background background = {};
    Rgba foreground = {};
    bool hasBackground = false;
    bool hasForeground = false;

    IconName icon = IconName::None;
    Rgba iconColor = {};
    bool hasIconColor = false;
    float iconSize = 16;
    bool headerCentered = false;

    bool okOutline = false;

    bool closeButton = false;

    El* footer = nullptr;
    bool footerVertical = false;

    bool footerStretch = false;
    bool footerMuted = false;
    bool footerDivider = false;

    static Dialog* New(Ctx* cx);
    Dialog* Title(Str s);
    Dialog* Description(Str s);
    Dialog* Open(bool v);
    Dialog* Body(El* e);
    Dialog* Surface(El* e);
    Dialog* W(float px);
    Dialog* H(float px);
    Dialog* Overlay(bool v);
    Dialog* OverlayClosable(bool v);
    Dialog* Keyboard(bool v);
    Dialog* Layer(int ix);
    Dialog* Radius(float px);
    Dialog* Bg(Background color);
    Dialog* Fg(Rgba color);
    Dialog* Icon(IconName n, Rgba color, float size = 16);
    Dialog* HeaderCentered(bool v = true);
    Dialog* OkText(Str s);
    Dialog* CancelText(Str s);
    Dialog* CancelVariant(ButtonVariant v);
    Dialog* OkVariant(ButtonVariant v, bool outline = false);
    Dialog* ShowCancel(bool v);

    Dialog* ButtonProps(const DialogButtonProps& value);

    Dialog* Confirm();
    Dialog* CloseButton(bool v = true);
    Dialog* Footer(El* e);
    Dialog* FooterVertical(bool v = true);
    Dialog* FooterStretch(bool v = true);
    Dialog* FooterMuted(bool v = true);
    Dialog* FooterDivider(bool v = true);
    Dialog* OnClose(Listener fn);
    Dialog* OnCancel(Listener fn);
    Dialog* OnOk(Listener fn);
    El* IntoEl(WinSize size);

  private:

    Str LayerId(Str base) const;
    El* Header();
    El* Actions();
};

struct AlertDialog {
    Dialog* base = nullptr;

    static AlertDialog* New(Ctx* cx);
    AlertDialog* Title(Str value);
    AlertDialog* Description(Str value);
    AlertDialog* Open(bool value);
    AlertDialog* Body(El* value);
    AlertDialog* Surface(El* value);
    AlertDialog* W(float value);
    AlertDialog* H(float value);
    AlertDialog* Overlay(bool value);
    AlertDialog* Keyboard(bool value);
    AlertDialog* Layer(int value);
    AlertDialog* Radius(float value);
    AlertDialog* Bg(Background value);
    AlertDialog* Fg(Rgba value);
    AlertDialog* Icon(IconName value, Rgba color, float size = 16);
    AlertDialog* HeaderCentered(bool value = true);

    AlertDialog* ButtonProps(const DialogButtonProps& value);
    AlertDialog* OkText(Str value);
    AlertDialog* CancelText(Str value);
    AlertDialog* CancelVariant(ButtonVariant value);
    AlertDialog* OkVariant(ButtonVariant value, bool outline = false);
    AlertDialog* ShowCancel(bool value);
    AlertDialog* Confirm();
    AlertDialog* CloseButton(bool value = true);
    AlertDialog* Footer(El* value);
    AlertDialog* FooterVertical(bool value = true);
    AlertDialog* FooterStretch(bool value = true);
    AlertDialog* FooterMuted(bool value = true);
    AlertDialog* FooterDivider(bool value = true);
    AlertDialog* OnClose(Listener value);
    AlertDialog* OnCancel(Listener value);
    AlertDialog* OnOk(Listener value);
    El* IntoEl(WinSize size);
};

}
}

#line 1 "src/ui/dock.h"

namespace gpui {

namespace component {

using Panel = DockPanelDef;
using PanelView = DockPanelDef;
using PanelStyle = DockPanelStyle;
using PanelControl = DockPanelControl;

struct TitleStyle {
    Rgba background = {};
    Rgba foreground = {};
};

struct PanelHandle {
    PanelView view = {};

    static PanelHandle New(const PanelView& panel);
    static PanelHandle FromView(const PanelView& panel);
    static const PanelView* Of(const PanelView* panel);
    const PanelView* Get() const;
    PanelView IntoPanelView() const;
};

PanelHandle panel_handle(const PanelView& panel);

struct DragPanelPreview {
    Ctx* cx = nullptr;
    const PanelView* panel = nullptr;

    static DragPanelPreview* New(Ctx* cx, const PanelView* panel);
    El* IntoEl();
};

struct DockSkin {
    Entity<DockState> state = {};

    static DockSkin New(Entity<DockState> state);
    PanelStyle GetPanelStyle(App* app) const;
    void SetPanelStyle(App* app, Window* win, PanelStyle style);
    bool IsToggleButtonVisible(App* app) const;
    void SetToggleButtonVisible(App* app, Window* win, bool visible);

    bool IsCloseButtonVisible(App* app) const;
    void SetCloseButtonVisible(App* app, Window* win, bool visible);
    const DockRenderer* Renderer() const;
};

El* DockInvalidPanelRender(Ctx* cx, void* data);

const float kDockTabBarH = 30;

struct DockArea {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<DockState> state = {};
    const DockSkin* skin = nullptr;

    static DockArea* New(Ctx* cx, Str id, Entity<DockState> state);
    DockArea* WithSkin(const DockSkin* value);
    El* IntoEl();
};

}
}

#line 1 "src/ui/empty.h"

namespace gpui {
namespace component {

enum class EmptyMediaVariant : uint8_t {
    Default,
    Icon,
};

struct EmptyMedia {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    EmptyMediaVariant variant = EmptyMediaVariant::Default;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static EmptyMedia* New(Ctx* cx);
    EmptyMedia* WithVariant(EmptyMediaVariant value);
    EmptyMedia* Child(El* child);
    EmptyMedia* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct EmptyTitle {
    Arena* a = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static EmptyTitle* New(Ctx* cx);
    EmptyTitle* Child(El* child);
    EmptyTitle* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct EmptyDescription {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static EmptyDescription* New(Ctx* cx);
    EmptyDescription* Child(El* child);
    EmptyDescription* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct EmptyContent {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static EmptyContent* New(Ctx* cx);
    EmptyContent* Child(El* child);
    EmptyContent* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct EmptyHeader {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    EmptyMedia* media = nullptr;
    EmptyTitle* title = nullptr;
    EmptyDescription* description = nullptr;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static EmptyHeader* New(Ctx* cx);
    EmptyHeader* Media(EmptyMedia* value);
    EmptyHeader* Title(EmptyTitle* value);
    EmptyHeader* Description(EmptyDescription* value);
    EmptyHeader* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct Empty {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    EmptyHeader* header = nullptr;
    EmptyContent* content = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static Empty* New(Ctx* cx);
    Empty* Header(EmptyHeader* value);
    Empty* Content(EmptyContent* value);
    Empty* Child(El* child);
    Empty* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

}
}

#line 1 "src/ui/inspector.h"

namespace gpui {

namespace component {

const float kInspectorWidth = 320;

Str StyleToJson(Arena* a, const Style& style);
bool StyleFromJson(Arena* a, Str text, Style* style, uint32_t* fields,
                   Str* error);

struct DivInspector {
    int inspectorId = 0;
    Style initialStyle = {};
    Str applied = {};
    Str error = {};
    InputState jsonInput;

    ~DivInspector();

    void UpdateInspectedElement(const InspectorPick& pick, Ctx* cx);
    bool EditJson(Str code, Ctx* cx);
    void Reset(Ctx* cx);
    El* Render(const InspectorPick& pick, Ctx* cx);

    static void OnReset(DivInspector* self, Ctx* cx, const ClickEvent*);
    static void OnFocus(DivInspector* self, Ctx* cx, const ClickEvent*);
};

struct Inspector {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    float width = kInspectorWidth;

    static Inspector* New(Ctx* cx);
    Inspector* W(float v);

    El* IntoEl();
};

}
}

#line 1 "src/ui/form.h"

namespace gpui {

namespace component {

enum class FieldAlign : uint8_t {
    Center,
    Start,
    End
};

enum class FieldBuilderKind : uint8_t {
    None,
    String,
    Element
};

struct FieldBuilder {
    FieldBuilderKind kind = FieldBuilderKind::None;
    Str string = {};
    El* element = nullptr;

    static FieldBuilder String(Str value);
    static FieldBuilder Element(El* value);
    bool IsSet() const { return kind != FieldBuilderKind::None; }
};

struct Field {
    FieldBuilder label = {};
    El* control = nullptr;

    FieldBuilder description = {};
    bool required = false;
    int colSpan = 1;
    int colStart = -1;
    int colEnd = -1;

    bool visible = true;

    bool labelIndent = true;
    FieldAlign align = FieldAlign::Center;

    El* const* children = nullptr;
    int childCount = 0;

    ElRefiner refiner = {};

    static Field New(El* control = nullptr);
    Field& Label(Str value);
    Field& Label(El* value);
    Field& Description(Str value);
    Field& Description(El* value);
    Field& Required(bool value = true);
    Field& Visible(bool value);
    Field& LabelIndent(bool value);
    Field& Align(FieldAlign value);
    Field& ColSpan(int value);
    Field& ColStart(int value);
    Field& ColEnd(int value);
    Field& Children(El* const* elements, int count);

    El* IntoEl(Ctx* cx) const;
};

using FormField = Field;

struct Form {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<FormField> fields;
    bool horizontal = false;
    int columns = 1;
    float labelWidth = 0;

    UiSize size = UiSize::Medium;

    float labelTextSize = 0;
    El* footer = nullptr;

    static Form* New(Ctx* cx);
    Form* Child(const component::Field& field);

    Form* Field(Str label, El* control);

    Form* FieldEl(El* label, El* control);
    Form* Required(bool v = true);
    Form* Description(Str s);
    Form* DescriptionEl(El* e);
    Form* SpanAll(bool v = true);
    Form* Visible(bool v);
    Form* LabelIndent(bool v);
    Form* Align(FieldAlign v);

    Form* LabelLayout(Axis axis);
    Form* Horizontal(bool v = true);
    Form* Columns(int n);

    Form* Footer(El* content);
    Form* LabelWidth(float w);
    Form* WithSize(UiSize v);
    Form* LabelTextSize(float px);
    El* IntoEl();
};

Form* v_form(Ctx* cx);
Form* h_form(Ctx* cx);
component::Field field(El* control = nullptr);

}
}

#line 1 "src/ui/global_state.h"

namespace gpui {
namespace component {

using UiGlobalState = gpui::BaseGlobalState;

inline UiGlobalState* UiGlobalStateOf(App* app) {
    return BaseGlobalStateOf(app);
}
void UiGlobalStateInit(App* app);
inline void UiSelectionFrameBegin(App* app) {
    BaseSelectionFrameBegin(app);
}
inline uint64_t UiSelectionNextDocumentOrder(App* app) {
    return BaseSelectionNextDocumentOrder(app);
}
inline void UiTextViewStatePush(App* app, EntityId state) {
    BaseTextViewStatePush(app, state);
}
inline void UiTextViewStatePop(App* app) {
    BaseTextViewStatePop(app);
}
inline EntityId UiTextViewStateCurrent(const App* app) {
    return BaseTextViewStateCurrent(app);
}

}
}

#line 1 "src/ui/group_box.h"

namespace gpui {

namespace component {

enum class GroupBoxVariant : uint8_t {
    Normal,
    Fill,
    Outline
};

GroupBoxVariant GroupBoxVariantFromStr(Str text);
Str GroupBoxVariantAsStr(GroupBoxVariant variant);

struct GroupBox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = StrL("group-box");
    Str title = {};

    El* titleEl = nullptr;
    bool hasTitle = false;
    ArenaVec<El*> children;

    El* footer = nullptr;
    GroupBoxVariant variant = GroupBoxVariant::Normal;

    Style rootStyle = {};
    uint32_t rootStyleSet = 0;
    Style titleStyle = {};
    uint32_t titleStyleSet = 0;
    Style contentStyle = {};
    uint32_t contentStyleSet = 0;

    bool titleSemibold = false;
    float titlePadX = 0;
    Background contentBg = {};
    bool hasContentBg = false;
    float contentRadius = -1;
    float contentPad = -1;
    float contentBorder = -1;

    static GroupBox* New(Ctx* cx);
    static GroupBox* New(Ctx* cx, Str title);
    GroupBox* Id(Str value);
    GroupBox* Title(El* e);
    GroupBox* Child(El* e);

    GroupBox* Footer(El* e);
    GroupBox* WithVariant(GroupBoxVariant value);
    GroupBox* Normal();
    GroupBox* Fill();
    GroupBox* Outline();

    GroupBox* Filled(bool v);
    GroupBox* Refine(const Style& style, uint32_t fields);
    GroupBox* TitleStyle(const Style& style, uint32_t fields);
    GroupBox* ContentStyle(const Style& style, uint32_t fields);
    GroupBox* TitleSemibold(bool v = true);
    GroupBox* TitlePadX(float px);
    GroupBox* ContentBg(Background c);
    GroupBox* ContentRadius(float px);
    GroupBox* ContentPad(float px);
    GroupBox* ContentBorder(float px);
    El* IntoEl();
};

}
}

#line 1 "src/ui/syntax.h"

namespace gpui {

namespace component {

enum class SyntaxTok : uint8_t {
    Text,
    Keyword,
    Type,
    Function,
    Property,
    String,
    Number,
    Boolean,
    Comment,
    Tag,
    Attribute,

    Title,
    Literal,
};

using SyntaxLang = int8_t;
constexpr SyntaxLang SyntaxLangNone = -1;

SyntaxLang SyntaxLangFor(Str info);

Str SyntaxLangName(SyntaxLang lang);

struct SyntaxLexer {
    const void* def = nullptr;
    Str src = {};
    int at = 0;
    SyntaxTok tok = SyntaxTok::Text;
    Str text = {};

    bool inTag = false;

    bool tagName = false;

    bool inFence = false;
    bool linkDest = false;
};

void SyntaxLexStart(SyntaxLexer* lx, SyntaxLang lang, Str src);

bool SyntaxLexNext(SyntaxLexer* lx);

Rgba SyntaxTokColor(SyntaxTok tok, ThemeMode mode, Rgba fallback);

}
}

#line 1 "src/ui/highlighter.h"

namespace gpui {

namespace component {

struct Highlighter {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};

    InputState* state = nullptr;

    float h = 0;

    Edges editorPad = {};

    float fontSize = 0;

    Str fontFamily = {};

    SyntaxLang lang = SyntaxLangNone;

    const TextSpan* decorations = nullptr;
    int nDecorations = 0;

    bool activeLine = false;
    bool indentGuides = false;

    bool searchable = true;

    bool folding = false;
    const Diagnostic* diagnostics = nullptr;
    int nDiagnostics = 0;

    static Highlighter* New(Ctx* cx, InputState* state);
    static Highlighter* New(Ctx* cx, Str id, InputState* state);
    Highlighter* H(float v);

    Highlighter* Font(float px);
    Highlighter* FontFamily(Str family);
    Highlighter* Language(Str name);
    Highlighter* Decorations(const TextSpan* runs, int n);
    Highlighter* ActiveLine(bool v = true);
    Highlighter* IndentGuides(bool v = true);
    Highlighter* Searchable(bool v);

    Highlighter* Diagnostics(const Diagnostic* items, int n);
    Highlighter* Folding(bool v = true);
    El* IntoEl();
};

}
}

#line 1 "src/ui/history.h"

#line 1 "src/ui/hover_card.h"

namespace gpui {

namespace component {

using HoverCardAnchor = PopupAnchor;

bool HoverCardOpen(Ctx* cx, Str id);

struct HoverCard {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* trigger = nullptr;
    El* content = nullptr;

    El* (*contentFn)(void* user, Ctx* cx) = nullptr;
    void* contentUser = nullptr;

    bool controlled = false;
    bool open = false;
    int openDelayMs = 600;
    int closeDelayMs = 300;
    Listener onOpenChange = {};

    HoverCardAnchor anchor = HoverCardAnchor::TopCenter;

    static HoverCard* New(Ctx* cx);
    static HoverCard* New(Ctx* cx, Str id);
    HoverCard* Trigger(El* e);
    HoverCard* Content(El* e);
    HoverCard* ContentBuilder(El* (*fn)(void* user, Ctx* cx), void* user);
    HoverCard* Open(bool v);
    HoverCard* OpenDelay(int ms);
    HoverCard* CloseDelay(int ms);
    HoverCard* OnOpenChange(Listener fn);
    HoverCard* Anchor(HoverCardAnchor a);
    El* IntoEl();
};

}
}

#line 1 "src/ui/i18n.h"

namespace gpui {

namespace component {

struct LocaleRow {
    const char* key;
    const char* const* values;
};

Str Tr(const char* key);

bool LocaleSet(Str name);

Str LocaleNow();

int LocaleCount();
Str LocaleAt(int i);

int LocaleIndex(Str name);

int LocaleRowCount();
const LocaleRow* LocaleRowAt(int i);

}
}

#line 1 "src/ui/icon.h"

namespace gpui {

namespace component {

struct Icon;

struct IconNamed {
    Str path = {};

    static IconNamed From(IconName name);
};

struct IconNameExt {
    static Icon* View(Ctx* cx, IconName name);
};

enum class IconSource : uint8_t {

    Path,

    Data,
};

struct Icon {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    IconName name = IconName::None;
    Str path = {};

    Str data = {};
    IconSource source = IconSource::Path;
    float size = 0;
    float rotation = 0;
    Rgba color = {};
    bool hasSize = false;
    bool hasColor = false;

    static Icon* New(Ctx* cx, IconName name);
    static Icon* New(Ctx* cx, IconNamed named);
    static Icon* Empty(Ctx* cx);

    Icon* Path(Str assetPath);

    Icon* Data(Str svg);
    Icon* Size(float v);
    Icon* Size(UiSize v);
    Icon* Color(Rgba c);

    Icon* Transform(float turns);
    Icon* Rotate(float turns);
    El* IntoEl();
};

}
}

#line 1 "src/ui/index_path.h"

#line 1 "src/ui/input.h"

namespace gpui {

namespace component {

namespace input_syntax {
struct Tree {};
}

void InputLanguageInit(App* app);

enum class AnyInputKind : uint8_t {
    None,
    Input,
    Textarea,
    Editor,
    Otp
};

struct AnyInputState {
    AnyInputKind kind = AnyInputKind::None;
    InputState* text = nullptr;
    Entity<OtpState> otp = {};

    static AnyInputState From(InputState* state);
    static AnyInputState FromInput(InputState* state);
    static AnyInputState FromTextarea(InputState* state);
    static AnyInputState FromEditor(InputState* state);
    static AnyInputState FromOtp(Entity<OtpState> state);
    InputState* AsInput() const;
    InputState* AsTextarea() const;
    InputState* AsEditor() const;
    Entity<OtpState> AsOtp() const;
    Str Value(Arena* a, App* app) const;
    FocusHandle FocusHandleOf(const Window* window, App* app) const;
    bool operator==(const AnyInputState& other) const;
    bool operator!=(const AnyInputState& other) const {
        return !(*this == other);
    }
};

enum class InputAlign : uint8_t {
    Left,
    Center,
    Right
};

enum class InputContentType : uint8_t {
    Name,
    NamePrefix,
    GivenName,
    MiddleName,
    FamilyName,
    NameSuffix,
    Nickname,
    JobTitle,
    OrganizationName,
    Location,
    FullStreetAddress,
    StreetAddressLine1,
    StreetAddressLine2,
    AddressCity,
    AddressState,
    AddressCityAndState,
    Sublocality,
    CountryName,
    PostalCode,
    TelephoneNumber,
    EmailAddress,
    Url,
    CreditCardNumber,
    CreditCardName,
    CreditCardGivenName,
    CreditCardMiddleName,
    CreditCardFamilyName,
    CreditCardSecurityCode,
    CreditCardExpiration,
    CreditCardExpirationMonth,
    CreditCardExpirationYear,
    CreditCardType,
    Username,
    Password,
    NewPassword,
    OneTimeCode,
    ShipmentTrackingNumber,
    FlightNumber,
    DateTime,
    Birthdate,
    BirthdateDay,
    BirthdateMonth,
    BirthdateYear,
    CellularEid,
    CellularImei
};

struct NativeMenu;

using EditorContextMenuFn = NativeMenu* (*)(Ctx * cx, NativeMenu* empty,
                                            void* data);

struct Input {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    InputState* state = nullptr;
    Str label = {};
    float width = kFill;
    El* prefix = nullptr;
    El* suffix = nullptr;
    UiSize size = UiSize::Medium;
    InputAlign align = InputAlign::Left;
    bool disabled = false;
    bool cleanable = false;

    bool masked = false;
    bool maskToggle = false;
    bool appearance = true;
    bool focusRing = true;
    bool readonly = false;
    InputContentType contentType = InputContentType::Name;
    bool hasContentType = false;
    AccessibilityRole accessibilityRole = AccessibilityRole::None;
    bool hasAccessibilityRole = false;
    Str accessibilityId = {};
    Str ariaLabel = {};
    Rgba textColor = {};
    bool hasTextColor = false;
    Listener onChange;
    Listener onFocus;
    Listener onClear;
    Listener onToggleMask;
    InputPasteFn onPaste = nullptr;
    void* onPasteData = nullptr;
    InlineTokenRenderer tokenRenderer = nullptr;
    void* tokenRendererUser = nullptr;
    InlineTokenClickListener tokenClick = nullptr;
    void* tokenClickUser = nullptr;
    InlineTokenHoverListener tokenHover = nullptr;
    void* tokenHoverUser = nullptr;
    EditorContextMenuFn contextMenu = nullptr;
    void* contextMenuData = nullptr;

    ElRefiner refiner = {};

    static Input* New(Ctx* cx, Str id, InputState* state);

    Input* ContextMenu(EditorContextMenuFn fn, void* data = nullptr);
    Input* Label(Str s);
    Input* WithSize(UiSize s);
    Input* Align(InputAlign v);
    Input* Disabled(bool v);
    Input* Readonly(bool v = true);
    Input* ContentType(InputContentType value);

    Input* Role(AccessibilityRole role);
    Input* AccessibilityId(Str id);
    Input* AriaLabel(Str label);
    Input* Cleanable(bool v = true);
    Input* Masked(bool v);
    Input* MaskToggle(bool v = true);
    Input* Appearance(bool v);

    Input* FocusRing(bool v);
    Input* TextColor(Rgba c);
    Input* OnClear(Listener fn);
    Input* OnToggleMask(Listener fn);

    Input* Prefix(El* el);
    Input* Suffix(El* el);

    Input* W(float v);
    Input* OnChange(Listener fn);
    Input* OnFocus(Listener fn);
    Input* OnPaste(InputPasteFn fn, void* data = nullptr);

    Input* Token(InlineTokenRenderer fn, void* user = nullptr);
    Input* OnTokenClick(InlineTokenClickListener fn, void* user = nullptr);

    Input* OnTokenHover(InlineTokenHoverListener fn, void* user = nullptr);
    El* IntoEl();
};

struct InputToken {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    InlineTokenContext context = {};
    IconName icon = IconName::None;
    bool hasIcon = false;

    static InputToken* New(Ctx* cx, const InlineTokenContext& context);
    InputToken* Icon(IconName name);
    El* IntoEl();
};

struct SearchPanel {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    InputState* target = nullptr;

    static SearchPanel* New(Ctx* cx, Str id, InputState* target);
    El* IntoEl();
};

struct Editor {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    InputState* state = nullptr;
    float height = 0;
    float fontSize = 0;
    Str fontFamily = {};
    bool appearance = true;
    bool bordered = true;
    bool disabled = false;
    bool readonly = false;
    int tabIndex = 0;
    AccessibilityRole accessibilityRole = AccessibilityRole::MultilineTextInput;
    Str ariaLabel = {};
    EditorContextMenuFn contextMenu = nullptr;
    void* contextMenuData = nullptr;
    InputPasteFn onPaste = nullptr;
    void* onPasteData = nullptr;

    Str language = {};
    const TextSpan* decorations = nullptr;
    int nDecorations = 0;

    bool activeLine = true;
    bool indentGuides = true;
    bool searchable = true;
    bool folding = true;
    const Diagnostic* diagnostics = nullptr;
    int nDiagnostics = 0;
    gpui::Style style = {};
    uint32_t styleFields = 0;

    static Editor* New(Ctx* cx, InputState* state);
    static Editor* New(Ctx* cx, Str id, InputState* state);
    Editor* H(float value);
    Editor* Font(float value);

    Editor* FontFamily(Str family);
    Editor* Appearance(bool value);
    Editor* Bordered(bool value);
    Editor* Disabled(bool value);
    Editor* Readonly(bool value = true);
    Editor* TabIndex(int value);
    Editor* Role(AccessibilityRole value);
    Editor* AriaLabel(Str value);
    Editor* ContextMenu(EditorContextMenuFn fn, void* data = nullptr);
    Editor* OnPaste(InputPasteFn fn, void* data = nullptr);
    Editor* Language(Str value);
    Editor* Decorations(const TextSpan* runs, int n);
    Editor* ActiveLine(bool value = true);
    Editor* IndentGuides(bool value = true);
    Editor* Searchable(bool value);
    Editor* Diagnostics(const Diagnostic* items, int n);
    Editor* Folding(bool value = true);
    Editor* Refine(const gpui::Style& value, uint32_t fields);
    El* IntoEl();
};

struct CompletionMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    InputState* editor = nullptr;
    Str query = {};

    static CompletionMenu* New(Ctx* cx, InputState* editor);
    CompletionMenu* UpdateQuery(int startOffset, Str query);
    CompletionMenu* Show(int offset, const CompletionItem* items, int n);
    void Hide();
    bool HandleAction(InputAction action);
    El* IntoEl();
};

struct CodeActionMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    InputState* state = nullptr;

    static CodeActionMenu* New(Ctx* cx, InputState* state);
    CodeActionMenu* Show(int offset, const CodeActionItem* items, int n);
    void Hide();
    bool HandleAction(InputAction action);
    El* IntoEl();
};

struct DiagnosticPopover {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    InputState* state = nullptr;
    int diagnostic = -1;

    static DiagnosticPopover* New(Ctx* cx, InputState* state, int diagnostic);
    El* IntoEl();
};

struct HoverPopover {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    InputState* editor = nullptr;
    Selection symbolRange = {};
    Str hover = {};

    static HoverPopover* New(Ctx* cx, InputState* editor, Selection symbolRange,
                             Str hover);
    El* IntoEl();
};

struct Textarea {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};

    InputState* state = nullptr;
    int rows = 0;

    UiSize size = UiSize::Medium;

    float height = 0;
    bool softWrap = true;
    AccessibilityRole accessibilityRole = AccessibilityRole::MultilineTextInput;
    Str ariaLabel = {};
    Str accessibilityId = {};
    bool appearance = true;

    bool bordered = true;
    bool disabled = false;
    bool readonly = false;
    bool focusRing = true;
    Listener onFocus;
    InputPasteFn onPaste = nullptr;
    void* onPasteData = nullptr;
    InlineTokenRenderer tokenRenderer = nullptr;
    void* tokenRendererUser = nullptr;
    InlineTokenClickListener tokenClick = nullptr;
    void* tokenClickUser = nullptr;
    InlineTokenHoverListener tokenHover = nullptr;
    void* tokenHoverUser = nullptr;
    EditorContextMenuFn contextMenu = nullptr;
    void* contextMenuData = nullptr;

    ElRefiner refiner = {};

    static Textarea* New(Ctx* cx, Str id, InputState* state);

    Textarea* ContextMenu(EditorContextMenuFn fn, void* data = nullptr);

    Textarea* Rows(int n);
    Textarea* WithSize(UiSize s);
    Textarea* H(float px);
    Textarea* SoftWrap(bool v);
    Textarea* Role(AccessibilityRole role);
    Textarea* AccessibilityId(Str id);
    Textarea* AriaLabel(Str label);
    Textarea* Disabled(bool v);
    Textarea* Readonly(bool v = true);
    Textarea* Appearance(bool v);
    Textarea* Bordered(bool v);
    Textarea* FocusRing(bool v);
    Textarea* OnFocus(Listener fn);
    Textarea* OnPaste(InputPasteFn fn, void* data = nullptr);
    Textarea* Token(InlineTokenRenderer fn, void* user = nullptr);
    Textarea* OnTokenClick(InlineTokenClickListener fn, void* user = nullptr);

    Textarea* OnTokenHover(InlineTokenHoverListener fn, void* user = nullptr);
    El* IntoEl();
};

enum class InputGroupAddonAlignment : uint8_t {
    InlineStart,
    InlineEnd,
    BlockStart,
    BlockEnd
};

struct InputGroupAppearance {
    Rgba background = {};
    Rgba border = {};
    Rgba ring = {};
    bool hasRing = false;

    static InputGroupAppearance New(const Theme& th, bool focused,
                                    bool disabled, bool invalid);
};

struct InputGroupButton {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Button* button = nullptr;
    UiSize size = UiSize::XSmall;
    ElRefiner refiner = {};

    static InputGroupButton* New(Ctx* cx, Str id);
    InputGroupButton* Label(Str s);
    InputGroupButton* Icon(IconName n);
    InputGroupButton* Icon(Str path);
    InputGroupButton* Tooltip(Str s);
    InputGroupButton* AriaLabel(Str s);
    InputGroupButton* WithSize(UiSize s);
    InputGroupButton* WithVariant(ButtonVariant v);
    InputGroupButton* Disabled(bool v);
    InputGroupButton* Loading(bool v);
    InputGroupButton* Outline();
    InputGroupButton* OnClick(Listener fn);
    InputGroupButton* Child(El* el);

    El* RenderInGroup(bool disabled);
    El* IntoEl();
};

struct InputGroupText {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    ElRefiner refiner = {};

    static InputGroupText* New(Ctx* cx);
    InputGroupText* Child(El* el);
    El* IntoEl();
};

struct InputGroupAddonChild {
    El* el = nullptr;
    InputGroupButton* button = nullptr;
};

struct InputGroupAddon {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    InputGroupAddonAlignment alignment = InputGroupAddonAlignment::InlineStart;
    UiSize size = UiSize::Medium;
    ArenaVec<InputGroupAddonChild> children;
    ElRefiner refiner = {};

    static InputGroupAddon* New(Ctx* cx, Str id);
    InputGroupAddon* Align(InputGroupAddonAlignment v);
    InputGroupAddon* Child(El* el);
    InputGroupAddon* Child(InputGroupButton* button);

    El* RenderInGroup(bool disabled);
    El* IntoEl();
};

using InputGroupInput = Input;
using InputGroupTextarea = Textarea;

struct InputGroupControl {
    Input* input = nullptr;
    Textarea* textarea = nullptr;
};

struct InputGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};

    ::gpui::component::Input* input = nullptr;
    Textarea* textarea = nullptr;
    ArenaVec<InputGroupAddon*> addons;
    UiSize size = UiSize::Medium;
    bool disabled = false;
    bool readonly = false;
    bool invalid = false;
    bool focusRing = true;
    Str ariaLabel = {};

    ElRefiner refiner = {};

    El* controlEl = nullptr;

    static InputGroup* New(Ctx* cx, Str id);
    InputGroup* Input(::gpui::component::Input* control);
    InputGroup* Input(Textarea* control);
    InputGroup* Addon(InputGroupAddon* addon);
    InputGroup* Disabled(bool v);
    InputGroup* Readonly(bool v = true);
    InputGroup* Invalid(bool v);
    InputGroup* FocusRing(bool v);
    InputGroup* AriaLabel(Str label);
    InputGroup* WithSize(UiSize s);
    El* IntoEl();
};

struct NumberInput {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    InputState* state = nullptr;
    float width = kFill;
    UiSize size = UiSize::Medium;
    bool disabled = false;
    bool appearance = true;
    bool focusRing = true;
    El* suffix = nullptr;
    Background bg = {};
    bool hasBg = false;
    Rgba textColor = {};
    bool hasTextColor = false;
    NumberStep numberStep = {};
    bool hasNumberStep = true;
    bool hasMin = false;
    double min = 0;
    bool hasMax = false;
    double max = 0;
    Listener onStep;
    Listener onInc;
    Listener onDec;
    Listener onFocus;

    static NumberInput* New(Ctx* cx, InputState* state);
    static NumberInput* New(Ctx* cx, Str id, InputState* state);

    NumberInput* W(float v);
    NumberInput* WithSize(UiSize s);
    NumberInput* Disabled(bool v);
    NumberInput* Appearance(bool v);

    NumberInput* FocusRing(bool v);
    NumberInput* Suffix(El* el);
    NumberInput* Bg(Background c);
    NumberInput* TextColor(Rgba c);

    NumberInput* Step(double value);
    NumberInput* StepBy(NumberStepByValueFn fn, int64_t arg = 0);
    NumberInput* NoStep();
    NumberInput* Min(double value);
    NumberInput* Max(double value);

    NumberInput* OnStep(Listener fn);
    NumberInput* OnFocus(Listener fn);
    NumberInput* OnInc(Listener fn);
    NumberInput* OnDec(Listener fn);
    El* IntoEl();
};

struct OtpInput {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    const char* value = nullptr;
    int len = 0;
    int slots = 6;

    int groups = 2;
    bool masked = false;
    bool disabled = false;
    bool focusRing = true;
    UiSize size = UiSize::Medium;
    float cellPx = 0;
    Listener onFocus;

    Entity<OtpState> state = {};

    static OtpInput* New(Ctx* cx, const char* value, int len);
    static OtpInput* New(Ctx* cx, Str id, Entity<OtpState> state);
    OtpInput* Id(Str s);
    OtpInput* Slots(int n);
    OtpInput* Groups(int n);
    OtpInput* Masked(bool v);
    OtpInput* Disabled(bool v);

    OtpInput* FocusRing(bool v);
    OtpInput* WithSize(UiSize s);
    OtpInput* CellSize(float px);
    OtpInput* OnFocus(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/label.h"

namespace gpui {

namespace component {

enum class HighlightsMatchKind : uint8_t {
    Prefix,
    Full
};

struct HighlightsMatch {
    HighlightsMatchKind kind = HighlightsMatchKind::Full;
    Str text = {};

    static HighlightsMatch Prefix(Str text);
    static HighlightsMatch Full(Str text);
    static HighlightsMatch From(Str text);
    Str AsStr() const;
    bool IsPrefix() const;
};

struct Label {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};
    Str secondary = {};
    bool hasSecondary = false;
    bool masked = false;
    bool semibold = false;
    float font = 0;
    HighlightsMatch highlight = {};
    bool hasHighlight = false;

    int align = 0;
    float lineHeight = 1.25f;

    static Label* New(Ctx* cx, Str text);
    Label* Secondary(Str s);
    Label* Masked(bool v);
    Label* Semibold();
    Label* Font(float px);
    Label* Highlights(HighlightsMatch matched);

    Label* Highlights(Str matched, bool prefix = false);
    Label* TextCenter();
    Label* TextRight();
    Label* LineHeight(float mult);
    Str FullText() const;
    int HighlightRanges(int totalLength, Selection* out, int capacity) const;
    El* IntoEl();
};

}
}

#line 1 "src/ui/link.h"

namespace gpui {

namespace component {

struct Link {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str href = {};
    Str text = {};

    bool disabled = false;
    Listener onOpen;

    ArenaVec<El*> children;

    static Link* New(Ctx* cx, Str id);
    Link* Href(Str s);
    Link* Text(Str s);
    Link* Disabled(bool v);
    Link* OnOpen(Listener fn);
    Link* Child(El* e);
    El* IntoEl();
};

}
}

#line 1 "src/ui/marker.h"

namespace gpui {

namespace component {

enum class MarkerVariant : uint8_t {

    Plain,

    Separator,

    Border
};

enum class MarkerLoadingStyle : uint8_t {

    Spinner,

    Shimmer
};

enum class MarkerAlignment : uint8_t {

    Start,

    Center,

    End
};

struct MarkerIcon {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    static MarkerIcon* New(Ctx* cx);
    MarkerIcon* Child(El* e);
    MarkerIcon* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct MarkerContentChild {
    Str text = {};
    El* element = nullptr;
    bool isText = false;
};

struct MarkerContent {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<MarkerContentChild> children;

    bool shimmer = false;
    ShimmerStyle shimmerStyle = {};
    bool separator = false;
    MarkerAlignment alignment = MarkerAlignment::Start;

    Rgba fg = {};
    bool hasFg = false;
    Style style = {};
    uint32_t styleSet = 0;

    static MarkerContent* New(Ctx* cx);

    MarkerContent* Text(Str text);
    MarkerContent* Child(El* e);
    MarkerContent* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

struct MarkerChild {
    MarkerIcon* icon = nullptr;
    MarkerContent* content = nullptr;
    El* element = nullptr;
};

struct Marker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    bool hasId = false;
    Style style = {};
    uint32_t styleSet = 0;
    Style separatorStyle = {};
    uint32_t separatorStyleSet = 0;
    MarkerVariant variant = MarkerVariant::Plain;

    MarkerAlignment alignment = MarkerAlignment::Start;
    bool hasAlignment = false;
    bool loading = false;
    MarkerLoadingStyle loadingStyle = MarkerLoadingStyle::Spinner;
    ShimmerStyle shimmerStyle = {};
    RoleOverride role = {};
    ArenaVec<MarkerChild> children;

    static Marker* New(Ctx* cx);

    Marker* Id(Str value);

    Marker* Role(RoleOverride value);
    Marker* WithVariant(MarkerVariant value);

    Marker* Alignment(MarkerAlignment value);
    MarkerAlignment ResolvedAlignment() const;
    Marker* Loading(bool value);
    Marker* WithLoadingStyle(MarkerLoadingStyle value);
    Marker* WithShimmerStyle(const ShimmerStyle& value);
    Marker* SeparatorStyle(const Style& s, uint32_t fields);
    Marker* Icon(MarkerIcon* value);
    Marker* Content(MarkerContent* value);
    Marker* Child(El* e);
    Marker* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

}
}

#line 1 "src/base/popup_menu.h"

namespace gpui {

enum class PopupMenuAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    Confirm,
    Cancel,

    OpenSubmenu,
    CloseSubmenu
};

void PopupMenuInitKeys();

Str PopupMenuContext();

PopupMenuAction PopupMenuActionOf(uint32_t id, Side side);

int PopupMenuNextIndex(const bool* clickable, int n, int selected);
int PopupMenuPrevIndex(const bool* clickable, int n, int selected);

struct PopupMenuRow {
    bool clickable = false;
    bool submenu = false;
    bool link = false;
    Str href = {};

    Listener handler = {};
};

struct PopupMenuState {
    bool open = false;

    int selected = -1;

    int openSubmenu = -1;

    Side side = Side::Right;

    float x = 0;
    float y = 0;

    float scrollY = 0;

    FocusHandle focus = {};
    FocusHandle previousFocus = {};

    FocusHandle triggerFocus = {};

    Entity<PopupMenuState> parent = {};

    Bounds bounds = {};

    Listener onConfirm = {};

    Vec<PopupMenuRow> rows;

    static void OnAction(PopupMenuState* self, Ctx* cx, const ActionEvent* ev);

    static void OnItemClick(PopupMenuState* self, Ctx* cx, const ClickEvent* ev,
                            int64_t ix);
    static void OnItemHover(PopupMenuState* self, Ctx* cx, const HoverEvent* ev,
                            int64_t ix);
    static void OnSubmenuClick(PopupMenuState* self, Ctx* cx,
                               const ClickEvent* ev, int64_t ix);
    static void OnSubmenuHover(PopupMenuState* self, Ctx* cx,
                               const HoverEvent* ev, int64_t ix);
    static void OnScroll(PopupMenuState* self, Ctx* cx, const ScrollEvent* ev);

    static void OnTriggerClick(PopupMenuState* self, Ctx* cx,
                               const ClickEvent* ev, int64_t wasOpen);

    static void OnPressOutside(PopupMenuState* self, Ctx* cx,
                               const MouseUpEvent* ev);
    static void OnContextDown(PopupMenuState* self, Ctx* cx,
                              const MouseDownEvent* ev);

    ~PopupMenuState() { VecReset(rows); }
};

void PopupMenuOpen(PopupMenuState* s, Ctx* cx);
void PopupMenuDismiss(PopupMenuState* s, Ctx* cx);
void PopupMenuDismissAll(PopupMenuState* s, Ctx* cx);

void PopupMenuPerform(PopupMenuState* s, Ctx* cx, PopupMenuAction act,
                      const bool* clickable, const bool* hasSubmenu, int n);

void PopupMenuPerformRows(PopupMenuState* s, Ctx* cx, PopupMenuAction act);

void PopupMenuBeginRows(PopupMenuState* s);
void PopupMenuAddRow(PopupMenuState* s, const PopupMenuRow& row);

void PopupMenuConfirm(PopupMenuState* s, Ctx* cx, int ix);

}

#line 1 "src/ui/popup_menu.h"

#line 1 "src/ui/menu.h"

namespace gpui {

namespace component {

enum class PopupMenuItem : uint8_t {
    Item,
    Separator,
    Label,
    ElementItem,
    Submenu
};

using MenuItemKind = PopupMenuItem;

struct PopupMenu;

struct MenuItem {
    MenuItemKind kind = MenuItemKind::Item;
    Str label = {};
    IconName icon = IconName::None;

    Str iconPath = {};
    Str iconSvg = {};

    Str kbd = {};

    uint32_t action = 0;
    int64_t actionArg = 0;

    Listener onClick = {};
    bool checked = false;
    bool disabled = false;
    bool isLink = false;
    Str href = {};

    PopupMenu* submenu = nullptr;

    El* element = nullptr;
};

struct PopupMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<PopupMenuState> state = {};

    ArenaVec<MenuItem> items;
    UiSize size = UiSize::Medium;

    float minW = 0;
    float maxH = 450;
    bool scrollable = false;
    bool externalLinkIcon = true;

    Side checkSide = Side::Left;

    const char* actionContext = nullptr;

    static PopupMenu* New(Ctx* cx, Str id);
    static PopupMenu* New(Ctx* cx, Str id, Entity<PopupMenuState> state);
    PopupMenu* Menu(Str label, IconName icon = IconName::None);

    PopupMenu* Menu(Str label, component::Icon* icon);
    PopupMenu* MenuWithCheck(Str label, bool checked);
    PopupMenu* MenuWithKbd(Str label, Str kbd);

    PopupMenu* MenuWithAction(Str label, uint32_t action, int64_t arg = 0);

    PopupMenu* Action(uint32_t action, int64_t arg = 0);

    PopupMenu* OnClick(Listener l);
    PopupMenu* Link(Str label, Str href, IconName icon = IconName::None);
    PopupMenu* Separator();
    PopupMenu* Label(Str label);
    PopupMenu* Element(El* el);
    PopupMenu* Submenu(Str label, PopupMenu* menu);

    PopupMenu* Disabled(bool v);
    PopupMenu* Checked(bool v);
    PopupMenu* Icon(IconName v);
    PopupMenu* Kbd(Str v);
    PopupMenu* WithSize(UiSize s);
    PopupMenu* MinW(float v);
    PopupMenu* MaxH(float v);
    PopupMenu* Scrollable(bool v = true);
    PopupMenu* CheckSide(Side s);
    PopupMenu* ActionContext(const char* ctx);
    PopupMenu* ExternalLinkIcon(bool v);
    El* IntoEl();
};

Entity<PopupMenuState> PopupMenuStateFor(Ctx* cx, Str id);

struct DropdownMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* trigger = nullptr;
    PopupMenu* menu = nullptr;

    bool anchorRight = false;

    bool anchorAbove = false;
    float gap = 4;

    static DropdownMenu* New(Ctx* cx, Str id);
    DropdownMenu* Trigger(El* e);
    DropdownMenu* Menu(PopupMenu* m);
    DropdownMenu* AnchorRight(bool v = true);
    DropdownMenu* AnchorAbove(bool v = true);
    El* IntoEl();
};

struct DropdownMenuPopover : DropdownMenu {
    static DropdownMenuPopover* New(Ctx* cx, Str id);
    DropdownMenuPopover* Anchor(gpui::Anchor value);
};

struct ContextMenuState {
    Entity<PopupMenuState> menu = {};
    bool open = false;
    Point position = {};
    FocusHandle previousFocus = {};
    FocusHandle triggerFocus = {};

    static void OnMouseDown(ContextMenuState* self, Ctx* cx,
                            const MouseDownEvent* ev);
};

struct ContextMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<ContextMenuState> state = {};
    El* child = nullptr;
    PopupMenu* menu = nullptr;

    static ContextMenu* New(Ctx* cx, Str id);
    ContextMenu* Child(El* e);
    ContextMenu* Menu(PopupMenu* m);
    El* IntoEl();
};

struct ContextMenuExt {
    static ContextMenu* Wrap(Ctx* cx, Str id, El* child, PopupMenu* menu);
};

struct AppMenuBarState {
    int selected = -1;
    int count = 0;
    FocusHandle focus = {};
    FocusHandle previousFocus = {};

    static void OnMenuClick(AppMenuBarState* self, Ctx* cx,
                            const ClickEvent* ev, int64_t ix);

    static void OnMenuHover(AppMenuBarState* self, Ctx* cx,
                            const HoverEvent* ev, int64_t ix);
    static void OnAction(AppMenuBarState* self, Ctx* cx, const ActionEvent* ev);
};

struct AppMenuBarItem {
    Str title = {};
    PopupMenu* menu = nullptr;
};

int AppMenuBarNextIndex(int selected, int count);
int AppMenuBarPrevIndex(int selected, int count);

void AppMenuBarSelect(AppMenuBarState* s, Ctx* cx, int ix);

struct AppMenuBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<AppMenuBarState> state = {};
    ArenaVec<AppMenuBarItem> items;

    static AppMenuBar* New(Ctx* cx, Str id, Entity<AppMenuBarState> state);
    AppMenuBar* Menu(Str title, PopupMenu* menu);
    El* IntoEl();
};

namespace popup_menu {
void init();
}
namespace app_menu_bar {
void init();
}
Str AppMenuBarContext();

}
}

#line 1 "src/ui/scroll.h"

namespace gpui {

namespace component {

using ScrollbarAxis = gpui::ScrollAxis;
using ScrollAxis = ScrollbarAxis;

struct Scrollable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;

    El* element = nullptr;
    Str id = {};
    float scrollY = 0;
    float scrollX = 0;
    float h = 0;
    bool hSet = false;
    ScrollAxis axis = ScrollAxis::Vertical;

    ScrollbarMode mode = ScrollbarMode::Scrolling;
    bool modeSet = false;

    Listener onScroll;

    static Scrollable* New(Ctx* cx);
    static Scrollable* New(Ctx* cx, Str id);
    static Scrollable* New(Ctx* cx, El* element,
                           ScrollAxis axis = ScrollAxis::Both);
    Scrollable* Id(Str v);
    Scrollable* Child(El* e);
    Scrollable* ScrollY(float v);
    Scrollable* ScrollX(float v);
    Scrollable* Axis(ScrollAxis v);
    Scrollable* Mode(ScrollbarMode v);
    Scrollable* H(float v);
    Scrollable* OnScroll(Listener fn);
    El* IntoEl();
};

struct ScrollableElement {
    static El* Scrollbar(Ctx* cx, El* element, Str id, float scrollY,
                         float scrollX, Listener onScroll,
                         ScrollbarAxis axis = ScrollbarAxis::Vertical);
    static El* VerticalScrollbar(Ctx* cx, El* element, Str id, float scrollY,
                                 Listener onScroll);
    static El* HorizontalScrollbar(Ctx* cx, El* element, Str id, float scrollX,
                                   Listener onScroll);
    static Scrollable* OverflowScrollbar(Ctx* cx, El* element);
    static Scrollable* OverflowXScrollbar(Ctx* cx, El* element);
    static Scrollable* OverflowYScrollbar(Ctx* cx, El* element);
};

using ScrollableMask = gpui::ScrollableMask;

}
}

#line 1 "src/ui/message_scroller.h"

namespace gpui {

namespace component {

struct Button;

const float kMessageScrollerOverdraw = 400.f;
const float kMessageScrollerJumpTransitionMs = 200.f;
const float kMessageScrollerBottomFadeTransitionMs = 200.f;

const float kMessageScrollerEstimatedRowHeight = 64.f;

struct MessageScrollerState {
    VirtualListScrollHandle handle;

    Vec<float> heights;
    Vec<uint8_t> needsMeasure;

    bool followTail = true;

    ~MessageScrollerState();

    static void Init(MessageScrollerState* self, int itemCount);

    int ItemCount() const;

    bool IsScrolledUp() const;

    bool IsFollowingTail() const;

    void Reset(Ctx* cx, int itemCount);

    bool Splice(Ctx* cx, int start, int end, int count);

    bool Append(Ctx* cx, int count);

    bool Prepend(Ctx* cx, int count);

    void Remeasure(Ctx* cx);

    bool RemeasureItems(Ctx* cx, int start, int end);

    bool ScrollToItem(Ctx* cx, int index);

    void ScrollToEnd(Ctx* cx);

    static void OnScroll(MessageScrollerState* self, Ctx* cx,
                         const ScrollEvent* ev);
    static void OnJumpToLatest(MessageScrollerState* self, Ctx* cx,
                               const ClickEvent* ev);

    bool ValidRange(int start, int end) const;
};

using MessageScrollerRowFn = El* (*)(void* user, Ctx* cx, int index);

using MessageScrollerButtonFn = void (*)(Button* button);

struct MessageScroller {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<MessageScrollerState> state = {};
    MessageScrollerRowFn renderer = nullptr;
    void* user = nullptr;

    float h = 0;
    Style style = {};
    uint32_t styleSet = 0;
    Style contentStyle = {};
    uint32_t contentStyleSet = 0;
    Style listStyle = {};
    uint32_t listStyleSet = 0;
    Style rowStyle = {};
    uint32_t rowStyleSet = 0;
    Style jumpButtonStyle = {};
    uint32_t jumpButtonStyleSet = 0;
    MessageScrollerButtonFn jumpButtonRenderer = nullptr;
    float jumpButtonTransitionMs = kMessageScrollerJumpTransitionMs;
    Rgba bottomFade = {};
    bool hasBottomFade = false;
    bool scrollbar = true;
    bool jumpButton = true;
    Str jumpButtonLabel = StrL("Jump to latest");

    static MessageScroller* New(Ctx* cx, Str id,
                                Entity<MessageScrollerState> state,
                                MessageScrollerRowFn renderer, void* user);

    MessageScroller* H(float px);
    MessageScroller* Scrollbar(bool value);
    MessageScroller* JumpButton(bool value);
    MessageScroller* WithJumpButtonLabel(Str label);
    MessageScroller* WithContentStyle(const Style& s, uint32_t fields);
    MessageScroller* WithListStyle(const Style& s, uint32_t fields);
    MessageScroller* WithRowStyle(const Style& s, uint32_t fields);
    MessageScroller* WithJumpButtonStyle(const Style& s, uint32_t fields);
    MessageScroller* WithJumpButtonRenderer(MessageScrollerButtonFn fn);

    MessageScroller* WithJumpButtonTransition(float ms);

    MessageScroller* WithBottomFade(Rgba color);
    MessageScroller* Refine(const Style& s, uint32_t fields);
    El* IntoEl();
};

}
}

#line 1 "src/ui/native_menu.h"

namespace gpui {

namespace component {

enum class NativeMenuItemKind : uint8_t {
    Item,
    Separator,
    Submenu
};

struct NativeMenu;

struct NativeMenuItem {
    NativeMenuItemKind kind = NativeMenuItemKind::Item;
    Str label = {};
    bool disabled = false;
    bool checked = false;

    IconName icon = IconName::None;
    Str iconPath = {};
    Str iconSvg = {};

    int64_t id = 0;
    NativeMenu* submenu = nullptr;
};

struct NativeMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;

    ArenaVec<NativeMenuItem> items;

    Listener onSelect = {};

    static NativeMenu* New(Ctx* cx);
    NativeMenu* Menu(Str label, int64_t id);
    NativeMenu* MenuWithDisabled(Str label, bool disabled, int64_t id);
    NativeMenu* MenuWithCheck(Str label, bool checked, int64_t id);
    NativeMenu* MenuWithIcon(Str label, IconName icon, int64_t id);

    NativeMenu* MenuWithIcon(Str label, component::Icon* icon, int64_t id);
    NativeMenu* Separator();
    NativeMenu* Submenu(Str label, NativeMenu* menu);
    NativeMenu* OnSelect(Listener l);
    bool IsEmpty() const { return len(items) == 0; }

    bool Show(float x, float y);

    PopupMenu* IntoPopupMenu(Str id) const;
};

struct NativeMenuFallback {
    Arena* a = nullptr;
    ArenaVec<const NativeMenu*> menus;
    Entity<PopupMenuState>* states = nullptr;
    int nStates = 0;
    Listener onSelect = {};
    Point position = {};

    double openedPress = -1;
    bool open = false;

    ~NativeMenuFallback();
};

NativeMenuFallback* NativeMenuFallbackOf(Window* win);

bool NativeMenuShowFallback(Ctx* cx, const NativeMenu* m, float x, float y);

const NativeMenuItem* NativeMenuFallbackRow(const NativeMenuFallback* f,
                                            EntityId state, int row);

El* NativeMenuFallbackOverlay(Ctx* cx);

int NativeMenuSelectable(const NativeMenu* m, const NativeMenuItem** out,
                         int cap);

}
}

#line 1 "src/ui/notification.h"

namespace gpui {

namespace component {

using NotificationKind = NotificationType;

bool NotificationDeliveryIncludesInApp(NotificationDelivery d);
bool NotificationDeliveryIncludesSystem(NotificationDelivery d);

const float kNotificationWidth = 382;

const int kNotificationMaxItems = 10;

const int kNotificationTickMs = 50;

using NotificationTypeId = uintptr_t;
template <typename T>
inline NotificationTypeId NotificationTypeOf() {
    static uint8_t tag = 0;
    return (NotificationTypeId)&tag;
}

struct Notification {
    int id = 0;

    NotificationTypeId identityType = 0;
    uint32_t identityKey = 0;
    bool identityHasKey = false;
    bool hasType = false;
    NotificationType type = NotificationType::Info;
    Str title = {};
    Str message = {};
    bool hasIcon = false;
    IconName icon = IconName::None;
    bool hasPlacement = false;
    Anchor placement = Anchor::TopRight;
    EntityId action = {};
    EntityId content = {};
    Listener onClick = {};
    Listener onClose = {};

    bool hasDelivery = false;
    NotificationDelivery delivery = NotificationDelivery::InApp;
    bool autohide = true;
    Style style = {};
    uint32_t styleSet = 0;

    Bounds measured = {};
    bool ownsText = false;

    static Notification New();
    static Notification Info(Str message);
    static Notification Success(Str message);
    static Notification Warning(Str message);
    static Notification Error(Str message);
    Notification& Message(Str value);
    Notification& Title(Str value);
    Notification& WithType(NotificationType value);
    Notification& Icon(IconName value);
    Notification& Placement(Anchor value);
    Notification& Delivery(NotificationDelivery value);
    Notification& System();
    Notification& InAppAndSystem();
    Notification& Autohide(bool value = true);
    Notification& Action(EntityId value);
    Notification& Content(EntityId value);
    Notification& OnClick(Listener value);
    Notification& OnClose(Listener value);
    Notification& Refine(const Style& value, uint32_t fields);

    template <typename T>
    Notification& Id() {
        id = 0;
        identityType = NotificationTypeOf<T>();
        identityKey = 0;
        identityHasKey = false;
        return *this;
    }
    template <typename T>
    Notification& Id1(uint32_t key) {
        id = 0;
        identityType = NotificationTypeOf<T>();
        identityKey = key;
        identityHasKey = true;
        return *this;
    }
    template <typename T>
    Notification& Id1(Str key) {
        return Id1<T>((uint32_t)HashClickId(key));
    }
    template <typename T>
    Notification& Action(Entity<T> value) {
        return Action(value.id);
    }
    template <typename T>
    Notification& Content(Entity<T> value) {
        return Content(value.id);
    }
};

using NotificationItem = Notification;

bool NotificationIdentitySame(const Notification& a, const Notification& b);

struct NotificationListState {
    ToastStackState stack;
    Vec<Notification> items;
    int nextId = 1;

    bool useThemeSettings = false;
    Anchor placement = Anchor::TopRight;
    Edges margins = Edges::New(16.f, 16.f, 50.f, 16.f);
    float width = kNotificationWidth;
    int maxItems = kNotificationMaxItems;

    float itemH = 76;

    double lastTickAt = 0;

    NotificationDelivery delivery = NotificationDelivery::InApp;

    bool isAdvancing = false;
    int advanceTimer = 0;

    Window* advanceWin = nullptr;
    bool stackHovered[8] = {};
    bool stackFocused[8] = {};
    FocusHandle stackFocus[8] = {};

    EntityId self = {};

    ~NotificationListState();
    bool IsExpanded() const;

    static void OnCloseClick(NotificationListState* self, Ctx* cx,
                             const ClickEvent* ev, int64_t id);
    static void OnItemClick(NotificationListState* self, Ctx* cx,
                            const ClickEvent* ev, int64_t id);
    static void OnHover(NotificationListState* self, Ctx* cx,
                        const HoverEvent* ev, int64_t anchor);
    static void OnTick(NotificationListState* self, Ctx* cx,
                       const TickEvent* ev);

    static void OnSystemResponse(NotificationListState* self, Ctx* cx,
                                 const ClickEvent* ev, int64_t id);
};

int NotificationPush(NotificationListState* s, Ctx* cx, Notification item,
                     int timeoutMs = -1);

void NotificationDismiss(NotificationListState* s, Ctx* cx, int id);

void NotificationDismissByType(NotificationListState* s, Ctx* cx,
                               NotificationTypeId type);
void NotificationDismissByTypeKey(NotificationListState* s, Ctx* cx,
                                  NotificationTypeId type, uint32_t key);

void NotificationClear(NotificationListState* s, Ctx* cx);

void NotificationStartAdvancing(NotificationListState* s, Ctx* cx);

void NotificationStopAdvancing(NotificationListState* s);

bool NotificationNeedsClock(const NotificationListState* s);

bool NotificationAdvance(NotificationListState* s, int deltaMs);
bool NotificationAdvance(NotificationListState* s, Ctx* cx, int deltaMs);

int NotificationIndexOf(const NotificationListState* s, int id);

const int kNotificationSystemMax = 100;

struct NotificationSystemEntry {
    int id = 0;
    NotificationTypeId identityType = 0;
    uint32_t identityKey = 0;
    bool identityHasKey = false;
    EntityId list = {};
    Window* win = nullptr;
    Listener onClick = {};
};

TempStr NotificationSystemTagTemp(int id);

bool NotificationTagId(Str tag, int* outId);

void NotificationInitSystem(App* app);

void NotificationSystemInsert(const NotificationSystemEntry& e);

void NotificationSystemDismiss(int id, Window* win);
void NotificationSystemDismissByType(NotificationTypeId type, Window* win);
void NotificationSystemDismissByTypeKey(NotificationTypeId type, uint32_t key,
                                        Window* win);

void NotificationSystemDismissAll(Window* win);
const NotificationSystemEntry* NotificationSystemFind(int id, Window* win);
int NotificationSystemCount();
int NotificationSystemCount(const App* app);

void NotificationSystemResponse(Str tag);

struct NotificationList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Entity<NotificationListState> state = {};

    static NotificationList* New(Ctx* cx, Entity<NotificationListState> state);
    El* IntoEl();
};

}
}

#line 1 "src/ui/pagination.h"

namespace gpui {

namespace component {

struct PaginationMenuState {
    int64_t firstPage = 1;
    Listener onChange = {};

    static void OnItem(PaginationMenuState* self, Ctx* cx, const ClickEvent* ev,
                       int64_t ix);
};

constexpr int kMaxEllipsisMenuPages = 100;

void PaginationEllipsisMenuPages(int64_t hiddenStart, int64_t hiddenEnd,
                                 int64_t currentPage, int64_t* start,
                                 int64_t* end);

struct Pagination {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};

    int64_t page = 1;
    int64_t total = 1;

    int64_t visiblePages = 5;
    bool compact = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Listener onChange;

    static Pagination* New(Ctx* cx, int64_t page, int64_t total);
    Pagination* Id(Str s);

    Pagination* CurrentPage(int64_t page);

    Pagination* TotalPages(int64_t pages);
    Pagination* VisiblePages(int64_t n);
    Pagination* Compact(bool v = true);
    Pagination* Disabled(bool v);
    Pagination* WithSize(UiSize s);
    Pagination* OnChange(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/plot.h"

namespace gpui {

struct Path;

namespace component {

using ::gpui::plot::kPlotAxisGap;
using ::gpui::plot::kPlotTextGap;
using ::gpui::plot::kPlotTextHeight;
using ::gpui::plot::kPlotTextSize;
using ::gpui::plot::PathCache;
using ::gpui::plot::PathCaches;
using ::gpui::plot::ScaleBand;
using ::gpui::plot::ScaleLinear;
using ::gpui::plot::ScaleOrdinal;
using ::gpui::plot::ScalePoint;
using ::gpui::plot::ShapeKey;

Point PlotTooltipPlace(Point cursor, Size within, Size box, float gap);

namespace plot {

using namespace ::gpui::plot;

enum class CrossLineAxis : uint8_t {
    Vertical,
    Horizontal,
    Both
};

struct CrossLine {
    Point point = {};
    float verticalStart = 0;
    bool hasVerticalLength = false;
    float verticalLength = 0;
    float horizontalStart = 0;
    bool hasHorizontalLength = false;
    float horizontalLength = 0;
    float thickness = 1;
    bool dashed = true;
    CrossLineAxis direction = CrossLineAxis::Vertical;

    static CrossLine New(Point point);
    CrossLine* Band(float value);
    CrossLine* Horizontal();
    CrossLine* Both();
    CrossLine* Height(float value);
    CrossLine* Width(float value);
    CrossLine* Span(float start, float length);
    CrossLine* HSpan(float start, float length);
    bool ShowVertical() const;
    bool ShowHorizontal() const;
    El* IntoEl(Ctx* cx) const;
};

struct Dot {
    Point point = {};
    float size = 6;
    Rgba stroke = RgbaTransparent();
    Rgba fill = RgbaTransparent();

    float halo = 0;

    static Dot New(Point point);
    Dot* Size(float value);
    Dot* Stroke(Rgba value);
    Dot* Fill(Rgba value);
    Dot* Halo(float value);
    El* IntoEl(Ctx* cx) const;
};

struct TooltipRow {
    Rgba color = {};
    Str label = {};
    Str value = {};
    Rgba valueColor = {};
    bool hasColor = false;
    bool hasValueColor = false;
};

bool TooltipHasSwatches(const ArenaVec<TooltipRow>& rows);

struct Tooltip {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    float gap = 0;
    bool hasCrossLine = false;
    CrossLine crossLine = {};
    ArenaVec<Dot> dots;
    bool appearance = true;
    bool hasTitle = false;
    Str title = {};
    ArenaVec<TooltipRow> rows;
    ArenaVec<El*> children;
    Point cursor = {};
    Size within = {};

    float progress = -1.f;

    bool glide = true;

    static Tooltip* New(Ctx* cx, Point cursor, Size within);
    Tooltip* Title(Str value);
    Tooltip* Row(Rgba color, Str label, Str value);

    Tooltip* PlainRow(Str label, Str value);

    Tooltip* ValueColor(Rgba color);
    Tooltip* Gap(float value);
    Tooltip* Cross(const CrossLine& value);
    Tooltip* Dots(const Dot* values, int count);
    Tooltip* Appearance(bool value);
    Tooltip* Child(El* value);

    Tooltip* Progress(float value);

    Tooltip* Focus(float value);

    Tooltip* Glide(bool value);
    El* IntoEl();
};

void PlotOverlayAttach(PaintCtx* ctx, El* plot, Bounds bounds, El* overlay);

}

int ChartAxisPointCount(int pointCount, int dataLen);

void ChartPointRange(float start, float width, int dataLen, int pointCount,
                     float out[2]);

struct ChartValueExtent {
    double lo = 0;
    double hi = 0;
    float bottom = 0;
    float top = 0;

    double ValueAt(float y) const;

    bool PositionOf(double value, float* out) const;
};

ScaleLinear ChartPointValueScale(const ChartSeries& chart, float height,
                                 ChartValueExtent* extent = nullptr);

const float kChartValueAxisGap = 32;

Str ChartFormatTick(Arena* a, double value);

Str ChartTickLabel(Arena* a, const ChartSeries& chart, double value);

void ChartLabeledItems(int len, int labelCount, int tickMargin, bool* out);

int ChartTickPositions(int count, float height, float* out, int cap);

int ChartBarValueTickLabels(Arena* a, const ChartSeries& chart, Str* out,
                            int cap);

float ChartBarValueAxisGap(const ChartSeries& chart, float measured);

plot::PlotTextAlign ChartPointLabelAlign(int index, int pointCount);

float BarExtendToMinLength(float tick, float zero, bool negative,
                           BarAlign alignment, float min);

int ChartValueTickPositions(float farEdge, float baseline, int count,
                            float* out, int cap);

}
}

#line 1 "src/ui/popover.h"

namespace gpui {

namespace component {

El* PopoverSurface(Ctx* cx, El* e);

const float kDropdownEnterMs = 150.f;

const float kDropdownEnterOffset = -8.f;

El* DropdownOpen(Ctx* cx, El* surface, uint32_t key);

El* DropdownPlaceContent(El* content, float gap = 4);

const float kPopoverOffsetRems = 0.25f;
const float kPopoverArrowRems = 0.375f;

struct PopoverArrowAnchor {
    gpui::Placement side = gpui::Placement::Bottom;
    Point target = {};
};
PopoverArrowAnchor ArrowAnchor(PopupAnchor anchor, Bounds trigger);

void ArrowPoints(Bounds surface, Bounds trigger, gpui::Placement side,
                 float depth, float radius, Point out[3]);

Bounds ArrowJoinBounds(const Point points[3], gpui::Placement side,
                       float stroke);

struct Button;

struct Popover {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* trigger = nullptr;

    component::Button* triggerButton = nullptr;

    El* content = nullptr;
    El* (*contentFn)(void* user, Ctx* cx) = nullptr;
    void* contentUser = nullptr;

    ArenaVec<El*> children;

    bool appearance = true;

    Style style = {};
    uint32_t styleSet = 0;
    ElRefiner refiner = {};

    bool controlled = false;
    bool open = false;
    bool defaultOpen = false;

    PopupAnchor anchor = PopupAnchor::TopLeft;

    float offset = 0;
    bool hasOffset = false;

    bool arrow = false;

    MouseButton button = MouseButton::Left;
    bool overlayClosable = true;
    Listener onOpenChange;
    Listener onClose;

    Style triggerStyle = {};
    uint32_t triggerStyleSet = 0;

    static Popover* New(Ctx* cx);
    static Popover* New(Ctx* cx, Str id);
    Popover* Trigger(El* e);
    Popover* Trigger(component::Button* triggerBtn);
    Popover* Content(El* e);
    Popover* ContentBuilder(El* (*fn)(void* user, Ctx* cx), void* user);

    Popover* Child(El* e);

    Popover* Appearance(bool v);

    Popover* Refine(const Style& s, uint32_t fields);

    Popover* RefineWith(ElRefiner r);
    Popover* Open(bool v);
    Popover* DefaultOpen(bool v);
    Popover* Button(MouseButton b);
    Popover* Anchor(PopupAnchor v);

    Popover* Offset(float v);

    Popover* Arrow(bool v);
    Popover* OverlayClosable(bool v);

    Popover* OnOpenChange(Listener fn);

    Popover* OnClose(Listener fn);

    Popover* TriggerStyle(const Style& s, uint32_t fields);
    El* IntoEl();
};

bool PopoverOpen(Ctx* cx, Str id);

Entity<PopoverState> PopoverStateOf(Ctx* cx, Str id);

}
}

#line 1 "src/ui/progress.h"

namespace gpui {

namespace component {

struct Progress {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    float value = 0;

    float w = kFill;
    float h = 8;

    bool loading = false;

    Str id = {};

    Str accessibilityLabel = {};

    static Progress* New(Ctx* cx);
    Progress* Value(float v);
    Progress* W(float v);
    Progress* H(float v);
    Progress* Loading(bool v);
    Progress* Id(Str v);

    Progress* AccessibilityLabel(Str s);

    Progress* WithSize(UiSize s);
    El* IntoEl();
};

struct ProgressCircle {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    float value = 0;
    float size = 48;

    float startValue = 0;
    Rgba color = {};
    bool hasColor = false;
    bool showLabel = true;
    bool loading = false;
    Str id = {};

    Str accessibilityLabel = {};

    static ProgressCircle* New(Ctx* cx);
    ProgressCircle* Loading(bool v);
    ProgressCircle* Id(Str v);

    ProgressCircle* AccessibilityLabel(Str s);
    ProgressCircle* Value(float v);
    ProgressCircle* Size(float v);
    ProgressCircle* Color(Rgba c);
    ProgressCircle* Label(bool v);
    El* IntoEl();
};

}
}

#line 1 "src/ui/questionnaire.h"

namespace gpui {

namespace component {

struct QuestionnairePart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Entity<QuestionnaireState> state = {};
    Str item = {};
    UiSize size = UiSize::Medium;
    bool hasSize = false;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};
};

#define GPUI_QUESTIONNAIRE_PART(T)                       \
    T* WithSize(UiSize s);                               \
    T* Child(El* e);                                     \
    T* Refine(const Style& refinement, uint32_t fields); \
    El* IntoEl();

struct Questionnaire : QuestionnairePart {
    static Questionnaire* New(Ctx* cx, Entity<QuestionnaireState> state);
    GPUI_QUESTIONNAIRE_PART(Questionnaire)
};

struct QuestionnaireProgress : QuestionnairePart {
    static QuestionnaireProgress* New(Ctx* cx,
                                      Entity<QuestionnaireState> state);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireProgress)
};

struct QuestionnaireTitle : QuestionnairePart {
    static QuestionnaireTitle* New(Ctx* cx, Entity<QuestionnaireState> state,
                                   Str item);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireTitle)
};

struct QuestionnaireDescription : QuestionnairePart {
    static QuestionnaireDescription* New(Ctx* cx,
                                         Entity<QuestionnaireState> state,
                                         Str item);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireDescription)
};

struct QuestionnaireItem : QuestionnairePart {
    static QuestionnaireItem* New(Ctx* cx, Entity<QuestionnaireState> state,
                                  Str item);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireItem)
};

struct QuestionnaireChoices : QuestionnairePart {
    static QuestionnaireChoices* New(Ctx* cx, Entity<QuestionnaireState> state,
                                     Str item);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireChoices)
};

using QuestionnaireChoiceRenderer =
    El* (*)(Ctx * cx, const QuestionnaireChoiceState* choice);

struct QuestionnaireChoice : QuestionnairePart {
    Str value = {};
    Style indicatorStyle = {};
    uint32_t indicatorStyleSet = 0;
    Style contentStyle = {};
    uint32_t contentStyleSet = 0;
    Style shortcutStyle = {};
    uint32_t shortcutStyleSet = 0;
    QuestionnaireChoiceRenderer indicatorRenderer = nullptr;
    QuestionnaireChoiceRenderer shortcutRenderer = nullptr;

    static QuestionnaireChoice* New(Ctx* cx, Entity<QuestionnaireState> state,
                                    Str item, Str value);
    QuestionnaireChoice* IndicatorStyle(const Style& value, uint32_t fields);
    QuestionnaireChoice* ContentStyle(const Style& value, uint32_t fields);
    QuestionnaireChoice* ShortcutStyle(const Style& value, uint32_t fields);
    QuestionnaireChoice* RenderIndicator(QuestionnaireChoiceRenderer fn);
    QuestionnaireChoice* RenderShortcut(QuestionnaireChoiceRenderer fn);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireChoice)
};

struct QuestionnaireChoiceDescription : QuestionnairePart {
    static QuestionnaireChoiceDescription* New(Ctx* cx);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireChoiceDescription)
};

struct QuestionnaireInput : QuestionnairePart {
    static QuestionnaireInput* New(Ctx* cx, Entity<QuestionnaireState> state,
                                   Str item);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireInput)
};

struct QuestionnaireError : QuestionnairePart {
    static QuestionnaireError* New(Ctx* cx, Entity<QuestionnaireState> state,
                                   Str item);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireError)
};

struct QuestionnaireActions : QuestionnairePart {
    static QuestionnaireActions* New(Ctx* cx, Entity<QuestionnaireState> state);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireActions)
};

struct QuestionnairePrevious : QuestionnairePart {
    static QuestionnairePrevious* New(Ctx* cx,
                                      Entity<QuestionnaireState> state);
    GPUI_QUESTIONNAIRE_PART(QuestionnairePrevious)
};

struct QuestionnaireSkip : QuestionnairePart {
    static QuestionnaireSkip* New(Ctx* cx, Entity<QuestionnaireState> state);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireSkip)
};

struct QuestionnaireNext : QuestionnairePart {
    static QuestionnaireNext* New(Ctx* cx, Entity<QuestionnaireState> state);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireNext)
};

struct QuestionnaireSubmit : QuestionnairePart {
    static QuestionnaireSubmit* New(Ctx* cx, Entity<QuestionnaireState> state);
    GPUI_QUESTIONNAIRE_PART(QuestionnaireSubmit)
};

#undef GPUI_QUESTIONNAIRE_PART

Str QuestionnaireErrorText(const QuestionnaireValidationError& error);

UiSize QuestionnaireResolveSize(App* app, const QuestionnairePart* part);
void QuestionnairePublishSize(App* app, Entity<QuestionnaireState> state,
                              UiSize size);

}
}

#line 1 "src/ui/radio.h"

namespace gpui {

namespace component {

struct Radio {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};

    Str accessibilityLabel = {};
    Str hint = {};
    bool checked = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    bool focusRing = true;
    int tabIndex = 0;
    bool tabStop = true;
    Listener onClick;

    ArenaVec<El*> children;

    ElRefiner refiner = {};

    static Radio* New(Ctx* cx, Str id);
    Radio* Child(El* child);
    Radio* Label(Str s);

    Radio* AccessibilityLabel(Str s);
    Radio* Hint(Str s);
    Radio* Checked(bool v);
    Radio* Disabled(bool v);
    Radio* WithSize(UiSize s);

    Radio* FocusRing(bool v);
    Radio* TabIndex(int v);
    Radio* TabStop(bool v);
    Radio* OnClick(Listener fn);

    Radio* OnChange(Listener fn);
    El* IntoEl();
};

struct RadioGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ArenaVec<Radio*> radios;
    bool horizontal = false;
    int selected = -1;
    bool disabled = false;
    UiSize size = UiSize::Medium;

    bool hasSize = false;
    Listener onClick;

    static RadioGroup* New(Ctx* cx, Str id);
    static RadioGroup* Vertical(Ctx* cx, Str id);
    static RadioGroup* Horizontal(Ctx* cx, Str id);
    RadioGroup* Child(Radio* r);

    RadioGroup* Child(Str label);
    RadioGroup* Selected(int ix);
    RadioGroup* Disabled(bool v);
    RadioGroup* WithSize(UiSize s);
    RadioGroup* OnClick(Listener fn);
    RadioGroup* OnChange(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/rating.h"

namespace gpui {

namespace component {

struct RatingState {

    int defaultValue = 0;
    int value = 0;

    int hoveredValue = 0;
    Listener onClick = {};

    static void OnStarHover(RatingState* self, Ctx* cx, const HoverEvent* ev,
                            int64_t ix);
    static void OnStarClick(RatingState* self, Ctx* cx, const ClickEvent* ev,
                            int64_t ix);
};

struct Rating {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    int value = 0;
    int max = 5;
    bool disabled = false;
    Rgba color = {};
    bool hasColor = false;
    UiSize size = UiSize::Medium;
    Listener onClick;

    static Rating* New(Ctx* cx, Str id);
    Rating* Value(int v);
    Rating* Max(int v);
    Rating* Disabled(bool v);
    Rating* Color(Rgba c);
    Rating* WithSize(UiSize s);
    Rating* OnClick(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/resizable.h"

namespace gpui {

namespace component {

using ResizableState = gpui::ResizableState;

const float kResizeIndicatorThickness = 3.f;

struct ResizeIndicator {
    float length = 0;
    float opacity = 0;
};
ResizeIndicator ResizeHandleIndicator(ResizeHandleState state);

El* RenderResizeHandle(void* user, const ResizeHandleContext* handle, Ctx* cx);

inline ResizeHandleRenderer ResizeHandleAppearance() {
    return &RenderResizeHandle;
}

struct Resizable {

    static gpui::Resizable* New(Ctx* cx, Str id,
                                Entity<ResizableState> state = {},
                                Axis axis = Axis::Horizontal);
};

}
}

#line 1 "src/ui/sheet.h"

namespace gpui {

namespace component {

enum class SheetPlacement : uint8_t {
    Left,
    Top,
    Right,
    Bottom
};

struct Sheet {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    El* titleEl = nullptr;
    bool open = false;

    float size = 350;
    SheetPlacement placement = SheetPlacement::Right;

    bool resizable = true;
    bool overlay = true;
    bool overlayClosable = true;
    El* body = nullptr;
    ArenaVec<El*> children;

    El* footer = nullptr;

    float scrollY = 0;
    int scrollId = 0;
    Listener onScroll;
    Listener onClose;
    Style style = {};
    uint32_t styleSet = 0;

    static Sheet* New(Ctx* cx);
    Sheet* Title(Str s);
    Sheet* Title(El* e);
    Sheet* Placement(SheetPlacement p);
    Sheet* Size(float px);
    Sheet* Resizable(bool v = true);
    Sheet* Overlay(bool v);
    Sheet* OverlayClosable(bool v = true);
    Sheet* Open(bool v);
    Sheet* Body(El* e);
    Sheet* Child(El* e);
    Sheet* Footer(El* e);
    Sheet* Refine(const Style& style, uint32_t fields);
    Sheet* Scroll(int id, float y, Listener fn);
    Sheet* OnClose(Listener fn);
    El* IntoEl(WinSize size);
};

}
}

#line 1 "src/ui/window_border.h"

namespace gpui {

namespace component {

inline float WindowShadowSize() {
    return PlatWindowShadowSize();
}
const float kWindowBorderSize = 1;

const float kWindowResizeHitSize = 4;

const float kWindowBorderRadius = 0;

using WindowTiling = gpui::Tiling;

Edges WindowBorderInsets(float shadowSize, WindowTiling tiling);

Edges WindowPaddings(Window* window);

Edges WindowContentInsets(Window* window);

enum class WindowEdge : int8_t {
    None = -1,
    TopLeft = 0,
    Top = 1,
    TopRight = 2,
    Right = 3,
    BottomRight = 4,
    Bottom = 5,
    BottomLeft = 6,
    Left = 7
};

WindowEdge WindowResizeEdge(float x, float y, float w, float h, Edges insets,
                            WindowTiling tiling, float hitSize);

struct WindowBorder {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    float shadowSize = WindowShadowSize();
    float resizeHitSize = kWindowResizeHitSize;
    WindowTiling tiling = {};
    bool hasTiling = false;

    static WindowBorder* New(Ctx* cx);
    WindowBorder* Child(El* e);
    WindowBorder* ShadowSize(float v);
    WindowBorder* ResizeHitSize(float v);
    WindowBorder* Tiling(WindowTiling v);
    El* IntoEl();
};

}
}

#line 1 "src/ui/window_ext.h"

namespace gpui {

struct WindowLayer {
    EntityId view = {};

    bool overlay = true;
    component::SheetPlacement placement = component::SheetPlacement::Right;
    float size = 0;
};

struct WindowLayers {
    App* app = nullptr;
    Window* win = nullptr;

    Vec<WindowLayer> dialogs;
    WindowLayer sheet = {};
    bool hasSheet = false;

    Entity<component::NotificationListState> notifications = {};

    int notifyTimer = 0;

    ~WindowLayers();
};

WindowLayers* WindowLayersOf(Window* win);

void WindowOpenDialog(Ctx* cx, EntityId view, bool overlay = true);
template <typename T>
inline void WindowOpenDialog(Ctx* cx, Entity<T> e, bool overlay = true) {
    WindowOpenDialog(cx, e.id, overlay);
}

inline void WindowOpenAlertDialog(Ctx* cx, EntityId view, bool overlay = true) {
    WindowOpenDialog(cx, view, overlay);
}
template <typename T>
inline void WindowOpenAlertDialog(Ctx* cx, Entity<T> e, bool overlay = true) {
    WindowOpenDialog(cx, e.id, overlay);
}
bool WindowHasActiveDialog(Ctx* cx);
int WindowDialogCount(Ctx* cx);

void WindowCloseDialog(Ctx* cx);
void WindowCloseAllDialogs(Ctx* cx);

void WindowOpenSheetAt(Ctx* cx, EntityId view,
                       component::SheetPlacement placement, float size);
template <typename T>
inline void WindowOpenSheetAt(Ctx* cx, Entity<T> e,
                              component::SheetPlacement placement, float size) {
    WindowOpenSheetAt(cx, e.id, placement, size);
}

inline void WindowOpenSheet(Ctx* cx, EntityId view, float size) {
    WindowOpenSheetAt(cx, view, component::SheetPlacement::Right, size);
}
template <typename T>
inline void WindowOpenSheet(Ctx* cx, Entity<T> e, float size) {
    WindowOpenSheetAt(cx, e.id, component::SheetPlacement::Right, size);
}
bool WindowHasActiveSheet(Ctx* cx);
void WindowCloseSheet(Ctx* cx);

Entity<component::NotificationListState> WindowNotifications(Ctx* cx);

int WindowPushNotification(Ctx* cx, component::Notification item,
                           int timeoutMs = -1);
int WindowPushNotification(Ctx* cx, Str message);

int WindowPushNotification(Ctx* cx, component::NotificationType kind,
                           Str message);
void WindowClearNotifications(Ctx* cx);
int WindowNotificationCount(Ctx* cx);
void WindowRemoveNotifications(Ctx* cx, component::NotificationTypeId type);
void WindowRemoveNotification1(Ctx* cx, component::NotificationTypeId type,
                               uint32_t key);
template <typename T>
inline void WindowRemoveNotification(Ctx* cx) {
    WindowRemoveNotifications(cx, component::NotificationTypeOf<T>());
}
template <typename T>
inline void WindowRemoveNotification1(Ctx* cx, uint32_t key) {
    WindowRemoveNotification1(cx, component::NotificationTypeOf<T>(), key);
}
template <typename T>
inline void WindowRemoveNotification1(Ctx* cx, Str key) {
    WindowRemoveNotification1<T>(cx, (uint32_t)HashClickId(key));
}

InputState* WindowFocusedInput(Ctx* cx);
bool WindowHasFocusedInput(Ctx* cx);

}

#line 1 "src/ui/root.h"

namespace gpui {

namespace component {

using Root = gpui::Root;

int RootDialogOverlayIndex(const bool* wantsOverlay, int n);

Edges RootNotificationInsets(bool hasSheet, SheetPlacement placement,
                             float size);

extern const RootPlugin kWindowStatePlugin;

void RootInit(App* app);

El* WindowStateLayers(Ctx* cx);

}
}

#line 1 "src/ui/separator.h"

namespace gpui {

namespace component {

enum class SeparatorStyle : uint8_t {
    Solid,
    Dashed
};

struct Separator {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    bool vertical = false;
    SeparatorStyle line = SeparatorStyle::Solid;
    Str label = {};
    Rgba color = {};
    bool hasColor = false;

    static Separator* Vertical(Ctx* cx);
    static Separator* Horizontal(Ctx* cx);
    Separator* Dashed();
    Separator* Label(Str s);
    Separator* Color(Rgba c);
    El* IntoEl();
};

}
}

#line 1 "src/ui/setting.h"

namespace gpui {

namespace component {

enum class SettingFieldType : uint8_t {
    Element,
    Switch,
    Checkbox,
    Input,
    NumberInput,
    Dropdown
};

using SettingFieldKind = SettingFieldType;

struct NumberFieldOptions {
    double min = -1e300;
    double max = 1e300;
    double step = 1;
};

struct RenderOptions {
    int pageIx = 0;
    int groupIx = 0;
    int itemIx = 0;
    UiSize size = UiSize::Medium;
    GroupBoxVariant groupVariant = GroupBoxVariant::Normal;
    Axis layout = Axis::Horizontal;
    bool disabled = false;

    static RenderOptions New();
    RenderOptions WithPageIx(int value) const;
    RenderOptions WithGroupIx(int value) const;
    RenderOptions WithItemIx(int value) const;
    RenderOptions WithSize(UiSize value) const;
    RenderOptions WithGroupVariant(GroupBoxVariant value) const;
    RenderOptions WithLayout(Axis value) const;
    RenderOptions WithDisabled(bool value) const;
};

using SettingFieldElementFn = El* (*)(void* user, const RenderOptions* options,
                                      Ctx* cx);

struct SettingFieldElement {
    void* user = nullptr;
    SettingFieldElementFn renderField = nullptr;

    bool IsValid() const { return renderField != nullptr; }
    El* Render(const RenderOptions* options, Ctx* cx) const {
        return renderField ? renderField(user, options, cx) : nullptr;
    }
};

template <typename T>
using SettingValueFn = T (*)(void* user, const App* app);
template <typename T>
using SettingSetValueFn = void (*)(void* user, T value, App* app);
using SettingDirtyFn = bool (*)(void* user, const App* app);
using SettingResetFn = void (*)(void* user, Ctx* cx);

template <typename T>
inline bool SettingValueSame(const T& a, const T& b) {
    return a == b;
}

template <>
inline bool SettingValueSame<Str>(const Str& a, const Str& b) {
    return base::StrEq(a, b);
}

template <typename T>
struct SettingField {
    SettingFieldType fieldType = SettingFieldType::Element;
    void* user = nullptr;
    SettingValueFn<T> value = nullptr;
    SettingSetValueFn<T> setValue = nullptr;
    T defaultValue = {};
    bool hasDefault = false;
    SettingDirtyFn dirty = nullptr;
    SettingResetFn reset = nullptr;
    NumberFieldOptions number = {};
    const SearchableItem* dropdownOptions = nullptr;
    int dropdownOptionsLen = 0;
    bool dropdownScrollable = false;
    SettingFieldElement element = {};

    static SettingField New(SettingFieldType type, void* user,
                            SettingValueFn<T> getValue,
                            SettingSetValueFn<T> putValue) {
        SettingField out;
        out.fieldType = type;
        out.user = user;
        out.value = getValue;
        out.setValue = putValue;
        return out;
    }
    SettingField& DefaultValue(T value_) {
        defaultValue = value_;
        hasDefault = true;
        return *this;
    }
    SettingField& OnReset(SettingDirtyFn isDirty, SettingResetFn doReset) {
        dirty = isDirty;
        reset = doReset;
        return *this;
    }
    bool IsResettable(const App* app) const {
        if (dirty) {
            return dirty(user, app);
        }
        if (fieldType == SettingFieldType::Element || !hasDefault || !value) {
            return false;
        }
        return !SettingValueSame(value(user, app), defaultValue);
    }
    void Reset(Ctx* cx) const {
        if (reset) {
            reset(user, cx);
        } else if (hasDefault && setValue) {
            setValue(user, defaultValue, cx ? cx->app : nullptr);
        }
    }
};

using SettingFieldTypeId = uintptr_t;

template <typename T>
inline SettingFieldTypeId SettingFieldTypeOf() {
    static const uint8_t tag = 0;
    return (SettingFieldTypeId)&tag;
}

struct AnySettingField {
    void* field = nullptr;
    SettingFieldTypeId typeId = 0;
    SettingFieldType fieldType = SettingFieldType::Element;
    bool (*isResettable)(void* field, const App* app) = nullptr;
    void (*reset)(void* field, Ctx* cx) = nullptr;

    bool IsValid() const { return field != nullptr; }
    bool IsResettable(const App* app) const {
        return isResettable && isResettable(field, app);
    }
    void Reset(Ctx* cx) const {
        if (reset) {
            reset(field, cx);
        }
    }
};

template <typename T>
inline AnySettingField EraseSettingField(SettingField<T>* field) {
    AnySettingField out;
    out.field = field;
    out.typeId = SettingFieldTypeOf<T>();
    out.fieldType = field ? field->fieldType : SettingFieldType::Element;
    out.isResettable = [](void* p, const App* app) {
        return ((SettingField<T>*)p)->IsResettable(app);
    };
    out.reset = [](void* p, Ctx* cx) { ((SettingField<T>*)p)->Reset(cx); };
    return out;
}

struct SelectIndex {
    int pageIx = 0;
    int groupIx = -1;
};

struct SettingItem {
    Str title = {};
    Str description = {};

    El* descriptionEl = nullptr;

    bool isElement = false;
    El* control = nullptr;
    SettingFieldElement fieldElement = {};
    ArenaVec<Str> keywords;
    bool disabled = false;

    bool dirty = false;
    Listener onReset = {};

    SettingFieldKind field = SettingFieldKind::Element;

    bool* boolValue = nullptr;
    bool defBool = false;

    Str value = {};
    Str defStr = {};
    NumberFieldOptions num = {};

    Entity<SearchableListState> list = {};
    const SearchableItem* items = nullptr;
    int nItems = 0;
    int defIndex = 0;

    bool hasDefault = false;

    float fieldW = 0;

    Axis layout = Axis::Horizontal;
};

struct SettingGroup {
    Str title = {};
    Str description = {};
    ArenaVec<SettingItem> items;

    El* footer = nullptr;

    GroupBoxVariant variant = GroupBoxVariant::Normal;
    bool hasVariant = false;

    ElRefiner refiner = {};
};

using SettingTitleSuffixFn = El* (*)(void* user, Ctx* cx);

struct SettingPage {
    Str title = {};
    Str description = {};
    IconName icon = IconName::None;
    ArenaVec<SettingGroup> groups;

    El* titleSuffix = nullptr;
    SettingTitleSuffixFn titleSuffixFn = nullptr;
    void* titleSuffixUser = nullptr;

    bool defaultOpen = false;

    bool resettable = true;
};

bool SettingItemMatches(const SettingItem* it, Str query);
bool SettingGroupMatches(const SettingGroup* g, Str query);
bool SettingPageMatches(const SettingPage* p, Str query);

SelectIndex SettingsResolveSelectedIndex(const ArenaVec<SettingPage>& pages,
                                         Str query, SelectIndex selected);

bool SettingGroupIsResettable(const SettingGroup* g, Str query);

struct SettingBinding {
    SettingFieldKind kind = SettingFieldKind::Element;
    bool* boolValue = nullptr;
    bool defBool = false;
    InputState* input = nullptr;
    Str defStr = {};
    NumberFieldOptions num = {};
    Entity<SearchableListState> list = {};
    int defIndex = 0;

    bool hasDefault = false;

    Listener onReset = {};
};

const int64_t kDropdownOptionsMax = 4096;

struct SettingFieldInput {
    InputState input;
    bool seeded = false;
};

struct SettingsState {
    int page = 0;
    int group = -1;
    bool selectionInitialized = false;

    int deferredScrollGroup = -1;

    int listPage = -1;
    uint32_t listQuery = 0;
    float scrollY = 0;
    int pendingScrollGroup = -1;

    float containerWidth = -1;

    InputState search;
    Vec<SettingBinding> fields;

    ~SettingsState() { VecReset(fields); }

    static void OnPageClick(SettingsState* self, Ctx* cx, const ClickEvent* ev,
                            int64_t page);
    static void OnGroupClick(SettingsState* self, Ctx* cx, const ClickEvent* ev,
                             int64_t packed);

    static void OnPageScroll(SettingsState* self, Ctx* cx,
                             const ScrollEvent* ev);

    static void OnFieldClick(SettingsState* self, Ctx* cx, const ClickEvent* ev,
                             int64_t ix);
    static void OnFieldReset(SettingsState* self, Ctx* cx, const ClickEvent* ev,
                             int64_t ix);

    static void OnDropdownPick(SettingsState* self, Ctx* cx,
                               const ClickEvent* ev, int64_t packed);

    static void OnResetPage(SettingsState* self, Ctx* cx, const ClickEvent* ev,
                            int64_t unused);

    static void OnSearchFocus(SettingsState* self, Ctx* cx,
                              const ClickEvent* ev);
};

struct Settings {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SettingsState> state = {};

    ArenaVec<SettingPage> pages;

    float sidebarWidth = 250;
    float sidebarMinWidth = 160;
    float sidebarMaxWidth = 360;

    float h = kFill;
    UiSize size = UiSize::Medium;
    SelectIndex defaultSelectedIndex = {};

    GroupBoxVariant groupVariant = GroupBoxVariant::Normal;

    static Settings* New(Ctx* cx, Str id, Entity<SettingsState> state = {});
    Settings* Page(Str title, IconName icon = IconName::None,
                   Str description = {});
    Settings* Group(Str title, Str description = {});

    Settings* GroupFooter(El* footer);

    Settings* GroupVariant(GroupBoxVariant variant);
    Settings* Item(Str title, Str description, El* control = nullptr);

    Settings* ElementItem(El* content);

    Settings* DescriptionEl(El* e);
    Settings* FieldElement(SettingFieldElement element);

    Settings* SwitchField(bool* value, bool defValue = false,
                          bool hasDefault = false);
    Settings* CheckboxField(bool* value, bool defValue = false,
                            bool hasDefault = false);
    Settings* InputField(Str value = {}, Str defValue = {});
    Settings* NumberField(Str value = {}, NumberFieldOptions opts = {},
                          Str defValue = {});
    Settings* DropdownField(Entity<SearchableListState> list,
                            const SearchableItem* items, int nItems,
                            int defIndex = -1);

    Settings* FieldWidth(float v);

    Settings* PageResettable(bool v);

    Settings* PageDefaultOpen(bool v);

    Settings* PageTitleSuffix(El* e);

    Settings* Keywords(Str a1, Str a2 = {}, Str a3 = {});
    Settings* Keyword(Str keyword);
    Settings* Disabled(bool v = true);
    Settings* Resettable(bool dirty, Listener onReset);
    Settings* Layout(Axis axis);
    Settings* SidebarWidth(float v);
    Settings* SidebarSizeRange(float minWidth, float maxWidth);
    Settings* WithSize(UiSize value);
    Settings* DefaultSelectedIndex(SelectIndex value);
    Settings* H(float v);
    Settings* WithGroupVariant(GroupBoxVariant v);

    Settings* Bordered(bool v);
    El* IntoEl();
};

}
}

#line 1 "src/ui/sidebar.h"

namespace gpui {

namespace component {

enum class SidebarCollapsible : uint8_t {
    Icon,
    Offcanvas,
    None
};

enum class SidebarWrapperKind : uint8_t {

    None,

    Static,

    Animated
};

struct SidebarLayout {
    bool iconCollapsed = false;
    bool offcanvasCollapsed = false;

    bool alignChildToEnd = false;
    SidebarWrapperKind wrapper = SidebarWrapperKind::None;
    float wrapperWidth = 0;
};

SidebarLayout SidebarLayoutFor(SidebarCollapsible collapsible, bool collapsed,
                               float expandedWidth, Side side);

struct SidebarMenuItem;
struct SidebarMenu;
struct SidebarGroup;

using SidebarItemRender = El* (*)(void* value, Ctx* cx, Str id, bool collapsed);

struct SidebarItem {
    void* value = nullptr;
    SidebarItemRender render = nullptr;

    static SidebarItem New(void* value, SidebarItemRender render);
    static SidebarItem From(SidebarMenuItem* item);
    static SidebarItem From(SidebarMenu* menu);
    static SidebarItem From(SidebarGroup* group);
    bool IsValid() const { return value && render; }
    El* Render(Ctx* cx, Str id, bool collapsed) const;
};

const float kSidebarCollapsedWidth = 48;

struct SidebarMenuState {
    bool open = false;

    bool seeded = false;
    bool clickToOpen = false;
    bool clickToToggle = false;
    Listener onClick = {};

    static void OnItemClick(SidebarMenuState* self, Ctx* cx,
                            const ClickEvent* ev);
    static void OnCaretClick(SidebarMenuState* self, Ctx* cx,
                             const ClickEvent* ev);
};

struct SidebarMenuItem {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    IconName icon = IconName::None;
    Str label = {};

    Str accessibilityLabel = {};
    Listener onClick;
    bool active = false;
    bool disabled = false;
    bool defaultOpen = false;
    bool clickToOpen = false;
    bool clickToToggle = false;
    El* suffix = nullptr;
    ArenaVec<SidebarMenuItem*> children;
    PopupMenu* contextMenu = nullptr;

    Style style = {};
    uint32_t styleSet = 0;

    Style labelStyle = {};
    uint32_t labelStyleSet = 0;

    bool collapsed = false;

    static SidebarMenuItem* New(Ctx* cx, Str label);
    SidebarMenuItem* Icon(IconName v);
    SidebarMenuItem* Refine(const Style& v, uint32_t fields);
    SidebarMenuItem* LabelStyle(const Style& v, uint32_t fields);
    SidebarMenuItem* AccessibilityLabel(Str v);
    SidebarMenuItem* Active(bool v);
    SidebarMenuItem* Disabled(bool v);
    SidebarMenuItem* DefaultOpen(bool v);
    SidebarMenuItem* ClickToOpen(bool v);
    SidebarMenuItem* ClickToToggle(bool v);
    SidebarMenuItem* Suffix(El* e);
    SidebarMenuItem* Child(SidebarMenuItem* item);
    SidebarMenuItem* OnClick(Listener fn);

    SidebarMenuItem* ContextMenu(PopupMenu* menu);

    El* IntoEl(Str id);
};

struct SidebarMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<SidebarMenuItem*> items;
    bool collapsed = false;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static SidebarMenu* New(Ctx* cx);
    SidebarMenu* Child(SidebarMenuItem* item);
    SidebarMenu* Refine(const Style& v, uint32_t fields);
    El* IntoEl(Str id);
};

struct SidebarGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str label = {};

    ArenaVec<SidebarMenu*> menus;
    ArenaVec<SidebarItem> children;
    bool collapsed = false;

    static SidebarGroup* New(Ctx* cx, Str label);
    SidebarGroup* Child(SidebarMenu* menu);
    SidebarGroup* Child(SidebarMenuItem* item);
    SidebarGroup* Child(SidebarItem item);
    El* IntoEl(Str id);
};

struct SidebarHeader {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;
    bool selected = false;
    bool collapsed = false;
    Listener onClick = {};
    ElRefiner refiner = {};

    static SidebarHeader* New(Ctx* cx);
    SidebarHeader* Child(El* child);
    SidebarHeader* Selected(bool v);
    SidebarHeader* Collapsed(bool v);
    SidebarHeader* OnClick(Listener fn);
    SidebarHeader* Refine(const Style& v, uint32_t fields);
    El* IntoEl();
};

struct SidebarFooter {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> children;
    Style style = {};
    uint32_t styleSet = 0;
    bool selected = false;
    bool collapsed = false;
    Listener onClick = {};
    ElRefiner refiner = {};

    static SidebarFooter* New(Ctx* cx);
    SidebarFooter* Child(El* child);
    SidebarFooter* Selected(bool v);
    SidebarFooter* Collapsed(bool v);
    SidebarFooter* OnClick(Listener fn);
    SidebarFooter* Refine(const Style& v, uint32_t fields);
    El* IntoEl();
};

struct SidebarToggleButton {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    bool collapsed = false;
    Side side = Side::Left;
    Listener onClick;

    Str accessibilityLabel = {};

    static SidebarToggleButton* New(Ctx* cx);
    SidebarToggleButton* Collapsed(bool v);
    SidebarToggleButton* WithSide(Side v);
    SidebarToggleButton* OnClick(Listener fn);
    SidebarToggleButton* AccessibilityLabel(Str v);
    El* IntoEl();
};

struct Sidebar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* header = nullptr;
    El* footer = nullptr;

    ArenaVec<SidebarGroup*> groups;
    ArenaVec<SidebarItem> content;
    Side side = Side::Left;
    SidebarCollapsible collapsible = SidebarCollapsible::Icon;
    bool collapsed = false;

    float width = 255;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static Sidebar* New(Ctx* cx, Str id);
    Sidebar* WithSide(Side v);
    Sidebar* Collapsible(SidebarCollapsible v);
    Sidebar* Collapsible(bool v);
    Sidebar* Collapsed(bool v);
    Sidebar* Header(El* e);
    Sidebar* Header(SidebarHeader* e);
    Sidebar* Footer(El* e);
    Sidebar* Footer(SidebarFooter* e);
    Sidebar* Child(SidebarGroup* group);
    Sidebar* Child(SidebarMenu* menu);
    Sidebar* Child(SidebarMenuItem* item);
    Sidebar* Child(SidebarItem item);
    Sidebar* W(float px);
    Sidebar* Refine(const Style& v, uint32_t fields);
    El* IntoEl();
};

}
}

#line 1 "src/ui/skeleton.h"

namespace gpui {

namespace component {

struct Skeleton {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    bool secondary = false;
    float w = kFill;
    float h = 16;

    static Skeleton* New(Ctx* cx);
    Skeleton* Secondary();
    Skeleton* W(float v);
    Skeleton* H(float v);
    El* IntoEl();
};

}
}

#line 1 "src/ui/slider.h"

namespace gpui {

namespace component {

struct Slider {
    Arena* a = nullptr;
    Ctx* cx = nullptr;

    Str id = {};

    SliderState* state = nullptr;

    bool reverse = false;
    bool disabled = false;

    Axis axis = Axis::Horizontal;

    float width = kAuto;

    Rgba bar = {};
    bool hasBar = false;
    Listener onChange;

    static Slider* New(Ctx* cx, Str id, SliderState* state);
    Slider* Reverse(bool v = true);
    Slider* Vertical(bool v = true);
    Slider* WithAxis(Axis v);
    Slider* Disabled(bool v = true);
    Slider* W(float px);

    Slider* WFill();
    Slider* Bg(Rgba c);

    Slider* OnChange(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/speech.h"

namespace gpui {

namespace component {

struct AudioFormat {
    uint32_t sampleRate = 16000;
    uint16_t channels = 1;

    static AudioFormat New(uint32_t sampleRate, uint16_t channels) {
        return AudioFormat{sampleRate, channels};
    }

    uint32_t SampleRate() const { return sampleRate; }

    uint16_t Channels() const { return channels; }
};
inline bool operator==(AudioFormat a, AudioFormat b) {
    return a.sampleRate == b.sampleRate && a.channels == b.channels;
}
inline bool operator!=(AudioFormat a, AudioFormat b) {
    return !(a == b);
}

enum class SpeechErrorKind : uint8_t {

    PermissionDenied,

    NoInputDevice,

    Unsupported,

    Input,

    Recognizer
};

struct SpeechError {
    SpeechErrorKind kind = SpeechErrorKind::Unsupported;
    char message[160] = {};

    static SpeechError PermissionDenied();
    static SpeechError NoInputDevice();
    static SpeechError Unsupported();

    static SpeechError Input(Str message);

    static SpeechError Recognizer(Str message);

    Str Display(Arena* a) const;
};

struct SpeechState;

struct SpeechSink {
    Entity<SpeechState> state = {};
    uint32_t session = 0;

    void Ready(App* app) const;

    void Hypothesis(Str text, App* app) const;

    void Phrase(Str text, App* app) const;

    void Finish(App* app) const;

    void Error(const SpeechError& error, App* app) const;
};

struct AudioSink {
    Entity<SpeechState> state = {};
    uint32_t session = 0;

    void Push(const int16_t* samples, int count, App* app) const;

    void Error(const SpeechError& error, App* app) const;
};

struct RecognitionSession {
    void* data = nullptr;

    void (*pushAudio)(void* data, const int16_t* samples, int count,
                      App* app) = nullptr;

    void (*finish)(void* data, App* app) = nullptr;
    void (*drop)(void* data) = nullptr;
};

struct SpeechRecognizer {
    void* data = nullptr;

    AudioFormat (*audioFormat)(void* data) = nullptr;

    bool (*isAvailable)(void* data, const App* app) = nullptr;

    bool (*start)(void* data, SpeechSink sink, App* app,
                  RecognitionSession* out, SpeechError* error) = nullptr;

    bool IsSet() const { return start != nullptr; }
};

struct AudioCapture {
    void* data = nullptr;
    void (*stop)(void* data) = nullptr;
};

struct AudioInput {
    void* data = nullptr;

    bool (*start)(void* data, AudioFormat format, AudioSink sink, App* app,
                  AudioCapture* out, SpeechError* error) = nullptr;

    bool IsSet() const { return start != nullptr; }
};

struct SpeechAudioConverter {

    double step = 1;

    double position = 1;
    float previous = 0;

    bool lowPass = false;
    float alpha = 0;
    float stages[2] = {};
    int channels = 1;

    static SpeechAudioConverter New(uint32_t sourceRate, AudioFormat format);

    void Convert(const float* input, int n, Vec<int16_t>& out);
};

const int kSpeechLevelIntervalMs = 25;

const int kSpeechLevelHistory = 256;

struct LevelMeter {

    int window = 400;
    double sum = 0;
    int count = 0;
    float smoothed = 0;

    float levels[kSpeechLevelHistory] = {};
    int first = 0;
    int nLevels = 0;

    double lastLevelAt = -1;

    void Reset(uint32_t sampleRate, uint16_t channels);

    bool Push(const int16_t* samples, int n);
    int LevelsLen() const { return nLevels; }

    float LevelAt(int ix) const {
        return levels[(first + ix) % kSpeechLevelHistory];
    }
};

int SpeechLevelWindowFor(uint32_t sampleRate, uint16_t channels);

float SpeechLevelOfRms(double rms);

float SpeechLevelSmooth(float previous, float raw);

enum class SpeechStatus : uint8_t {

    Idle,

    Connecting,

    Recording,

    Stopping
};

inline bool SpeechStatusIsActive(SpeechStatus s) {
    return s != SpeechStatus::Idle;
}

inline bool SpeechStatusIsCapturing(SpeechStatus s) {
    return s == SpeechStatus::Connecting || s == SpeechStatus::Recording;
}

enum class SpeechEventKind : uint8_t {

    Started,

    Partial,

    Final,

    Cancelled,

    Error
};

struct SpeechEvent {
    SpeechEventKind kind = SpeechEventKind::Started;
    Str text = {};
    SpeechError error = {};
};

struct SpeechDeferredOp;

struct SpeechState {
    SpeechRecognizer recognizer = {};
    AudioInput input = {};
    bool systemFallback = true;

    int stopTimeoutMs = 3000;
    SpeechStatus status = SpeechStatus::Idle;

    bool hasSession = false;
    uint32_t sessionId = 0;

    AudioCapture capture = {};
    bool hasCapture = false;
    RecognitionSession recognition = {};

    int stopTimer = 0;
    Window* stopTimerWin = nullptr;

    uint32_t nextSession = 0;

    Str committed = {};
    Str hypothesis = {};
    LevelMeter meter = {};

    Vec<SpeechDeferredOp*> deferred;
    bool drainPosted = false;
    int updateDepth = 0;

    Entity<SpeechState> self = {};

    ~SpeechState();

    SpeechState* Recognizer(const SpeechRecognizer& value);

    SpeechState* Input(const AudioInput& value);

    SpeechState* SystemFallback(bool value);

    SpeechState* StopTimeout(int ms);

    bool HasRecognizer() const;

    bool IsAvailable(const App* app) const;

    SpeechStatus Status() const { return status; }

    TempStr TranscriptTemp() const;

    int LevelsLen() const { return meter.LevelsLen(); }
    float LevelAt(int ix) const { return meter.LevelAt(ix); }
    double LastLevelAt() const { return meter.lastLevelAt; }

    void Start(Ctx* cx);

    void Stop(Ctx* cx);

    void Cancel(Ctx* cx);

    void Toggle(Ctx* cx);

    bool IsSession(uint32_t id) const { return hasSession && sessionId == id; }

    static void OnToggle(SpeechState* self, Ctx* cx, const ClickEvent* ev);
    static void OnDrain(SpeechState* self, Ctx* cx, const void*);
    static void OnStopTimeout(SpeechState* self, Ctx* cx, const TickEvent* ev,
                              int64_t session);
};

Entity<SpeechState> SpeechStateNew(App* app);

struct SpeechButton {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SpeechState> state = {};
    UiSize size = UiSize::Medium;
    bool disabled = false;
    bool showWhenUnsupported = false;
    Style style = {};
    uint32_t styleSet = 0;

    static SpeechButton* New(Ctx* cx, Entity<SpeechState> state);

    SpeechButton* ShowWhenUnsupported(bool show);
    SpeechButton* WithSize(UiSize s);
    SpeechButton* Disabled(bool v);

    SpeechButton* Refine(const Style& refinement, uint32_t fields);
    El* IntoEl();
};

struct SpeechWaveform {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Entity<SpeechState> state = {};
    UiSize size = UiSize::Medium;
    Style style = {};
    uint32_t styleSet = 0;

    static SpeechWaveform* New(Ctx* cx, Entity<SpeechState> state);
    SpeechWaveform* WithSize(UiSize s);
    SpeechWaveform* Refine(const Style& refinement, uint32_t fields);
    float Height() const;
    El* IntoEl();
};

int SpeechWaveformBarRects(Bounds bounds, const float* levels, int nLevels,
                           float scroll, float bar, float gap, Bounds* out,
                           int cap);

}

template <>
struct EventEmitter<component::SpeechState, component::SpeechEvent> {};

}

#line 1 "src/ui/spinner.h"

namespace gpui {

namespace component {

struct Spinner {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    UiSize size = UiSize::Medium;
    float px = 0;
    IconName icon = IconName::Loader;

    Str iconSvg = {};
    Rgba color = {};
    bool hasColor = false;

    float speed = 0;
    EaseFn ease = nullptr;

    Str id = {};

    static Spinner* New(Ctx* cx);
    Spinner* Speed(float ms);
    Spinner* Ease(EaseFn fn);
    Spinner* Id(Str v);
    Spinner* WithSize(UiSize s);
    Spinner* Size(float v);
    Spinner* Icon(IconName n);
    Spinner* IconData(Str svg);
    Spinner* Color(Rgba c);
    El* IntoEl();
};

}
}

#line 1 "src/ui/status_bar.h"

namespace gpui {

namespace component {

struct StatusBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> left;
    ArenaVec<El*> center;
    ArenaVec<El*> right;

    static StatusBar* New(Ctx* cx);
    StatusBar* Left(El* e);
    StatusBar* Left(Str s);

    StatusBar* Center(El* e);
    StatusBar* Center(Str s);
    StatusBar* Right(El* e);
    StatusBar* Right(Str s);
    El* IntoEl();
};

}
}

#line 1 "src/ui/styled.h"

namespace gpui::component {

inline El* RaisedShadow(El* element) {
    if (!element) {
        return nullptr;
    }
    Rgba ink = Rgba8(0, 0, 0, 26);
    BoxShadow shadows[] = {
        {0, 1, 1.5f, 0, ink, false},
        {0, 1, 1.f, -1.f, ink, false},
    };
    return element->Shadows(shadows, 2);
}

}

#line 1 "src/ui/stepper.h"

namespace gpui {

namespace component {

struct StepperItem {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    IconName icon = IconName::None;

    ArenaVec<El*> children;
    bool disabled = false;

    ElRefiner refiner = {};

    int step = 0;
    int checkedStep = 0;
    Axis layout = Axis::Horizontal;
    bool isLast = false;
    bool textCenter = false;
    UiSize size = UiSize::Medium;
    Listener onClick;

    static StepperItem* New(Ctx* cx);
    StepperItem* Icon(IconName v);
    StepperItem* Child(El* e);
    StepperItem* Disabled(bool v);
    El* IntoEl();
};

struct Stepper {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    ArenaVec<StepperItem*> items;
    int step = 0;
    Axis layout = Axis::Horizontal;
    bool disabled = false;
    bool textCenter = false;
    bool itemsCenter = false;
    UiSize size = UiSize::Medium;

    float width = kFill;
    Listener onClick;

    static Stepper* New(Ctx* cx, Str id);
    Stepper* Item(StepperItem* item);
    Stepper* SelectedIndex(int i);
    Stepper* Layout(Axis v);
    Stepper* Vertical();
    Stepper* TextCenter(bool v);

    Stepper* ItemsCenter(bool v = true);
    Stepper* Disabled(bool v);
    Stepper* WithSize(UiSize s);
    Stepper* W(float px);
    Stepper* OnClick(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/switch.h"

namespace gpui {

namespace component {

struct Switch {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};

    Str accessibilityLabel = {};
    bool checked = false;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    Rgba color = {};

    Str tooltip = {};
    bool hasColor = false;
    bool focusRing = true;
    int tabIndex = 0;
    bool tabStop = true;
    Listener onClick;

    static Switch* New(Ctx* cx, Str id);
    Switch* Label(Str s);

    Switch* AccessibilityLabel(Str s);
    Switch* Checked(bool v);
    Switch* Disabled(bool v);
    Switch* WithSize(UiSize s);
    Switch* Color(Rgba c);
    Switch* Tooltip(Str s);

    Switch* FocusRing(bool v);
    Switch* TabIndex(int v);
    Switch* TabStop(bool v);
    Switch* OnClick(Listener fn);

    Switch* OnChange(Listener fn);
    El* IntoEl();
};

}
}

#line 1 "src/ui/tab.h"

namespace gpui {

namespace component {

enum class TabVariant : uint8_t {
    Tab,
    Outline,
    Pill,
    Segmented,
    Underline
};

float TabHeight(TabVariant v, UiSize size);
float TabInnerHeight(TabVariant v, UiSize size);

float TabPadX(TabVariant v, UiSize size);

float TabMarginTop(TabVariant v, UiSize size);
float TabMarginBottom(TabVariant v, UiSize size);

float TabBarGap(TabVariant v, UiSize size);

float TabBarPadX(TabVariant v, UiSize size);

float TabBarRadius(TabVariant v, UiSize size, float radius, float radiusLg);
float TabRadius(TabVariant v, UiSize size, float radius, float radiusLg);
float TabInnerRadius(TabVariant v, UiSize size, float radius, float radiusLg);

struct Tab {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str label = {};
    Str ariaLabel = {};
    IconName icon = IconName::None;
    El* prefix = nullptr;
    El* suffix = nullptr;
    ArenaVec<El*> children;
    TabVariant variant = TabVariant::Tab;
    UiSize size = UiSize::Medium;
    bool disabled = false;
    bool selected = false;
    bool tabBarPrefix = true;

    bool flex1 = false;
    float maxWidth = 0;
    Listener onClick;
    Style style = {};
    uint32_t styleSet = 0;

    ElRefiner refiner = {};

    static Tab* New(Ctx* cx);
    static Tab* New(Ctx* cx, Str label);
    Tab* Label(Str value);
    Tab* AriaLabel(Str value);
    Tab* Icon(IconName value);
    Tab* Prefix(El* value);
    Tab* Suffix(El* value);
    Tab* Child(El* value);
    Tab* Disabled(bool value = true);
    Tab* Selected(bool value = true);
    Tab* OnClick(Listener value);
    Tab* WithVariant(TabVariant value);
    Tab* Outline();
    Tab* Pill();
    Tab* Segmented();
    Tab* Underline();
    Tab* WithSize(UiSize value);
    Tab* Flex1();
    Tab* MaxWidth(float value);
    Tab* TabBarPrefix(bool value);
    Tab* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

struct TabBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};

    ArenaVec<component::Tab> items;

    int selected = -1;
    TabVariant variant = TabVariant::Tab;
    UiSize size = UiSize::Medium;

    float maxWidth = 0;

    float width = kAuto;
    El* prefix = nullptr;
    El* suffix = nullptr;
    El* lastEmptySpace = nullptr;

    bool menu = false;
    Listener onChange;
    bool trackScroll = false;
    int scrollId = 0;
    float scrollX = 0;
    Listener onScroll;
    Style style = {};
    uint32_t styleSet = 0;

    static TabBar* New(Ctx* cx);
    static TabBar* New(Ctx* cx, Str id);
    TabBar* Child(component::Tab* child);
    TabBar* Child(Str label);
    TabBar* Tab(Str label);
    TabBar* Tab(Str label, IconName icon, bool disabled = false);

    TabBar* Flex1();

    TabBar* AriaLabel(Str label);
    TabBar* Disabled(int ix, bool v = true);
    TabBar* Selected(int i);
    TabBar* OnChange(Listener fn);
    TabBar* OnClick(Listener fn);
    TabBar* Variant(TabVariant v);
    TabBar* WithVariant(TabVariant v);
    TabBar* Outline();
    TabBar* Pill();
    TabBar* Segmented();
    TabBar* Underline();
    TabBar* Size(UiSize v);
    TabBar* WithSize(UiSize v);
    TabBar* MaxWidth(float v);

    TabBar* W(float v);
    TabBar* WFill();
    TabBar* Prefix(El* e);
    TabBar* Suffix(El* e);
    TabBar* LastEmptySpace(El* e);
    TabBar* Menu(bool v = true);

    TabBar* TrackScroll(int scrollKey, float offset, Listener fn);
    TabBar* Refine(const Style& value, uint32_t fields);
    El* IntoEl();
};

using Tabs = TabBar;

}
}

#line 1 "src/base/data_table.h"

namespace gpui {

enum class TableSelectionMode : uint8_t {
    None,
    Row,
    Column,
    Cell
};

enum class ColumnSort : uint8_t {
    Default,
    Ascending,
    Descending
};

enum class TableAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    SelectPrevColumn,
    SelectNextColumn,
    SelectFirst,
    SelectLast,
    SelectPageUp,
    SelectPageDown,
    Cancel
};

void TableInitKeys();
Str TableContext();
TableAction TableActionOf(uint32_t id);

enum class TableEventKind : uint8_t {
    SelectRow,
    SelectCol,
    SelectCell,
    DoubleClickedRow,
    DoubleClickedCell,

    RightClickedRow,
    RightClickedCell,
    Sort,
    ColumnWidthsChanged,

    MoveColumn,
    Cancel
};

struct TableEvent {
    TableEventKind kind = TableEventKind::SelectRow;
    int row = -1;
    int col = -1;

    ColumnSort sort = ColumnSort::Default;

    const float* widths = nullptr;
    int nWidths = 0;
};

extern const Str kTableResizeDrag;

extern const Str kTableColDrag;

const float kTableResizeHandleW = 2;

const float kTableResizeHandlePadding = 4;

struct TableVisibleRange {
    int rowFirst = 0;
    int rowEnd = 0;
    int colFirst = 0;
    int colEnd = 0;
};

struct TableState {
    int rowCount = 0;
    int colCount = 0;
    TableSelectionMode mode = TableSelectionMode::None;
    int selectedRow = -1;
    int selectedCol = -1;
    int selectedCellRow = -1;
    int selectedCellCol = -1;

    int rightClickedRow = -1;

    int rightClickedCellRow = -1;
    int rightClickedCellCol = -1;
    bool rowSelectable = true;
    bool colSelectable = true;
    bool cellSelectable = false;

    bool rowHeader = true;

    bool loopSelection = false;
    bool sortable = true;

    int sortCol = -1;
    ColumnSort sort = ColumnSort::Default;

    int pageRows = 10;

    bool colResizable = true;

    Vec<float> colWidth;

    Vec<float> colMinWidths;
    Vec<float> colMaxWidths;
    float colMinWidth = 20;
    float colMaxWidth = 0;

    int resizingCol = -1;

    Vec<int> colOrder;
    bool colOrderSeeded = false;

    bool colMovable = true;
    int draggingCol = -1;
    int dropGap = -1;

    Vec<Bounds> colBounds;

    float rowH = 32;
    float scrollY = 0;
    float viewportH = 0;

    float scrollX = 0;
    float viewportW = 0;

    int fixedCols = 0;

    bool colFixed = true;

    Bounds bodyBounds = {};

    TableVisibleRange visibleRange = {};

    bool loading = false;
    bool hasMore = false;
    int loadMoreThreshold = 20;
    Listener onEvent = {};

    Entity<TableState> self = {};

    FocusHandle focus = {};

    ~TableState() {
        VecReset(colWidth);
        VecReset(colMinWidths);
        VecReset(colMaxWidths);
        VecReset(colOrder);
        VecReset(colBounds);
    }
    Listener onLoadMore = {};

    void* delegateData = nullptr;
    void (*delegateSort)(Ctx* cx, void* data, int col,
                         ColumnSort sort) = nullptr;
    void (*delegateMoveColumn)(Ctx* cx, void* data, int from, int to) = nullptr;
    void (*delegateLoadMore)(Ctx* cx, void* data) = nullptr;

    static void OnRowClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                           int64_t row);

    static void OnCellClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                            int64_t packed);
    static void OnRowMouseDown(TableState* self, Ctx* cx,
                               const MouseDownEvent* ev, int64_t row);

    static void OnCellMouseDown(TableState* self, Ctx* cx,
                                const MouseDownEvent* ev, int64_t packed);
    static void OnHeadClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                            int64_t col);
    static void OnSortClick(TableState* self, Ctx* cx, const ClickEvent* ev,
                            int64_t col);

    static void OnResizeDrag(TableState* self, Ctx* cx,
                             const DragMoveEvent* ev);
    static void OnResizeEnd(TableState* self, Ctx* cx, const MouseUpEvent* ev);
    static void OnColDragMove(TableState* self, Ctx* cx,
                              const DragMoveEvent* ev);
    static void OnColDrop(TableState* self, Ctx* cx, const DropEvent* ev);
    static void OnColDragEnd(TableState* self, Ctx* cx, const MouseUpEvent* ev);
    static void OnScroll(TableState* self, Ctx* cx, const ScrollEvent* ev);

    static void OnScrollXY(TableState* self, Ctx* cx, const ScrollEvent* ev);
};

void TableSeedColOrder(TableState* s, int colCount);
int TableColAt(const TableState* s, int display);

int TableDisplayOfCol(const TableState* s, int col);

bool TableMoveColumn(TableState* s, int from, int to);
void TableMoveColumnEvent(TableState* s, Ctx* cx, int from, int to);

int TableDragGapAt(const Bounds* colBounds, int n, float x, int dragCol,
                   int fixedCount = 0);

void TableEnsureCols(TableState* s, int n);

inline int64_t TableCellPack(int row, int col) {
    return ((int64_t)row << 12) | (int64_t)(col & 0xfff);
}
inline int TableCellRow(int64_t packed) {
    return (int)(packed >> 12);
}
inline int TableCellCol(int64_t packed) {
    return (int)(packed & 0xfff);
}

bool TableVisibleRowsChanged(TableState* s, int first, int end);
bool TableVisibleColsChanged(TableState* s, int first, int end);

void TableVisibleCols(const TableState* s, int* first, int* end);

int TableExtraRowsNeeded(float totalH, float actualH, float rowH);

bool TableShouldLoadMore(const TableState* s, int visibleEnd);

void TableScrollToRow(TableState* s, int row, ScrollStrategy strategy);

void TableScrollToCol(TableState* s, int col, ScrollStrategy strategy);

void TableRefreshCols(TableState* s);

float TableColWidth(const TableState* s, int col, float declared);

void TableSeedColWidth(TableState* s, int col, float declared);

float TableClampColWidth(const TableState* s, float width);
float TableClampColWidth(const TableState* s, int col, float width);
void TableSetColConstraints(TableState* s, int col, float minWidth,
                            float maxWidth);

void TableResizeCol(TableState* s, Ctx* cx, int col, float width);

ColumnSort TableNextSort(ColumnSort s);

ColumnSort TableSortOf(const TableState* s, int col);

void TablePerformSort(TableState* s, Ctx* cx, int col);

enum class TableSelectionKind : uint8_t {
    None,
    Row,
    Column,
    Cell
};

struct TableSelection {
    TableSelectionKind kind = TableSelectionKind::None;
    int row = -1;
    int col = -1;
};

inline bool operator==(const TableSelection& a, const TableSelection& b) {
    return a.kind == b.kind && a.row == b.row && a.col == b.col;
}
inline bool operator!=(const TableSelection& a, const TableSelection& b) {
    return !(a == b);
}

TableSelection TableSelectionOf(const TableState* s);

void TableSetSelection(TableState* s, Ctx* cx, TableSelection selection);

int TableSelectedRow(const TableState* s);
int TableSelectedCol(const TableState* s);
bool TableSelectedCell(const TableState* s, int* row, int* col);

void TableSetSelectedRow(TableState* s, Ctx* cx, int row);
void TableSetSelectedCol(TableState* s, Ctx* cx, int col);
void TableSetSelectedCell(TableState* s, Ctx* cx, int row, int col);
void TableClearSelection(TableState* s, Ctx* cx);

bool TableEscalatesToRow(const TableState* s, int row, int col,
                         bool doubleClick);

void TablePerform(TableState* s, Ctx* cx, TableAction act);

void TableOnAction(TableState* self, Ctx* cx, const ActionEvent* ev);
void TableBindKeys(Ctx* cx, El* root, Entity<TableState> state);

template <>
struct EventEmitter<TableState, TableEvent> {};

}

#line 1 "src/ui/data_table.h"

#line 1 "src/ui/table.h"

namespace gpui {

namespace component {

enum class ColumnFixed : uint8_t {
    Left
};

struct TableColumn {
    Str title = {};
    float width = 100;
    bool right = false;
    bool sortable = false;
    bool selectable = true;

    bool resizable = true;

    bool fixed = false;

    Str key = {};
    Str name = {};
    bool center = false;
    bool hasSort = false;
    ColumnSort sort = ColumnSort::Default;
    bool hasPaddings = false;
    Edges paddings = {};
    ColumnFixed fixedSide = ColumnFixed::Left;
    bool movable = true;
    float minWidth = 20;
    float maxWidth = 0;

    static TableColumn New(Str key, Str name);
    TableColumn Sort(ColumnSort value) const;
    TableColumn Sortable() const;
    TableColumn Ascending() const;
    TableColumn Descending() const;
    TableColumn TextCenter() const;
    TableColumn TextRight() const;
    TableColumn Paddings(Edges value) const;
    TableColumn P0() const;
    TableColumn Width(float value) const;
    TableColumn Fixed(ColumnFixed value = ColumnFixed::Left) const;
    TableColumn FixedLeft() const;
    TableColumn Resizable(bool value) const;
    TableColumn Movable(bool value) const;
    TableColumn Selectable(bool value) const;
    TableColumn MinWidth(float value) const;
    TableColumn MaxWidth(float value) const;
};

using Column = TableColumn;

struct ColumnGroup {
    Str label = {};
    int span = 1;

    static ColumnGroup New(Str label, size_t span);
};

using TableGroupCell = ColumnGroup;

struct TableGroupHeader {
    const TableGroupCell* cells = nullptr;
    int n = 0;
};

struct DataTable;

struct TableDelegate {
    void* data = nullptr;
    int (*columnsCount)(Ctx* cx, void* data) = nullptr;
    int (*rowsCount)(Ctx* cx, void* data) = nullptr;
    TableColumn (*column)(Ctx* cx, void* data, int col) = nullptr;
    void (*performSort)(Ctx* cx, void* data, int col,
                        ColumnSort sort) = nullptr;
    El* (*renderHeader)(Ctx* cx, void* data) = nullptr;
    El* (*renderGroupTh)(Ctx* cx, void* data, Str label, int span,
                         float width) = nullptr;
    El* (*renderTh)(Ctx* cx, void* data, int col) = nullptr;
    El* (*renderTr)(Ctx* cx, void* data, int row) = nullptr;
    El* (*renderTd)(Ctx* cx, void* data, int row, int col) = nullptr;
    void (*groupHeaders)(Ctx* cx, void* data, DataTable* table) = nullptr;
    PopupMenu* (*contextMenu)(Ctx* cx, void* data, int row,
                              PopupMenu* menu) = nullptr;
    void (*moveColumn)(Ctx* cx, void* data, int from, int to) = nullptr;
    El* (*renderEmpty)(Ctx* cx, void* data) = nullptr;
    bool (*loading)(Ctx* cx, void* data) = nullptr;
    El* (*renderLoading)(Ctx* cx, void* data, UiSize size) = nullptr;
    bool (*hasMore)(Ctx* cx, void* data) = nullptr;
    int (*loadMoreThreshold)(void* data) = nullptr;
    void (*loadMore)(Ctx* cx, void* data) = nullptr;
    El* (*renderLastEmptyCol)(Ctx* cx, void* data) = nullptr;
    void (*visibleRowsChanged)(Ctx* cx, void* data, int first,
                               int end) = nullptr;
    void (*visibleColumnsChanged)(Ctx* cx, void* data, int first,
                                  int end) = nullptr;
    Str (*cellText)(Ctx* cx, void* data, int row, int col) = nullptr;
};

struct DataTable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<TableState> state = {};
    const TableColumn* columns = nullptr;
    int nColumns = 0;

    El* (*cell)(Ctx* cx, void* data, int row, int col) = nullptr;
    void* data = nullptr;
    int nRows = 0;
    bool stripe = false;

    bool bordered = true;

    bool scrollbarV = true;
    bool scrollbarH = true;

    ArenaVec<TableGroupHeader> groupHeaders;

    float h = 0;

    El* empty = nullptr;

    PopupMenu* (*contextMenu)(Ctx* cx, void* data, int row,
                              PopupMenu* menu) = nullptr;

    El* (*lastEmptyCol)(Ctx* cx, void* data) = nullptr;

    void (*visibleRowsChanged)(Ctx* cx, void* data, int first,
                               int end) = nullptr;
    void (*visibleColsChanged)(Ctx* cx, void* data, int first,
                               int end) = nullptr;

    Str (*cellText)(Ctx* cx, void* data, int row, int col) = nullptr;

    float rowHeight = 32;

    UiSize size = UiSize::Medium;
    TableDelegate delegate = {};
    bool hasDelegate = false;

    static DataTable* New(Ctx* cx, Str id, Entity<TableState> state);
    DataTable* Columns(const TableColumn* cols, int n);
    DataTable* Delegate(const TableDelegate& value);
    DataTable* Rows(int n, void* data,
                    El* (*cell)(Ctx* cx, void* data, int row, int col));
    DataTable* Stripe(bool v);
    DataTable* Bordered(bool v);
    DataTable* ScrollbarVisible(bool vertical, bool horizontal);
    DataTable* WithSize(UiSize s);

    DataTable* RowHeight(float px);
    DataTable* GroupHeader(const TableGroupCell* cells, int n);
    DataTable* H(float px);
    DataTable* Empty(El* e);
    DataTable* ContextMenu(PopupMenu* (*fn)(Ctx*, void*, int, PopupMenu*));
    DataTable* LastEmptyCol(El* (*fn)(Ctx*, void*));
    DataTable* OnVisibleRows(void (*fn)(Ctx*, void*, int, int));
    DataTable* OnVisibleCols(void (*fn)(Ctx*, void*, int, int));
    DataTable* CellText(Str (*fn)(Ctx*, void*, int, int));

    void Dump(Vec<Str>* heads, Vec<Str>* cells);

    void Headers(Vec<Str>* heads);

    void DumpRange(int lo, int hi, Vec<Str>* heads, Vec<Str>* cells);
    El* IntoEl();

    El* BuildEl();

    float ColWidth(const TableState* s, int col) const;
};

enum class TableAlign : uint8_t {
    Left,
    Center,
    Right
};

struct TableCellEl {
    Arena* a = nullptr;
    Ctx* cx = nullptr;

    bool head = false;
    int ix = 0;
    int colSpan = 1;
    TableAlign align = TableAlign::Left;
    UiSize size = UiSize::Medium;

    float width = kAuto;
    ArenaVec<El*> children;

    ElRefiner refiner = {};

    TableCellEl* ColSpan(int n);
    TableCellEl* TextCenter();
    TableCellEl* TextRight();
    TableCellEl* W(float v);
    TableCellEl* WithSize(UiSize s);
    TableCellEl* Child(El* e);
    El* IntoEl();
};

struct TableHead {
    static TableCellEl* New(Ctx* cx);
};
struct TableCell {
    static TableCellEl* New(Ctx* cx);
};

struct TableRow {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int ix = 0;
    UiSize size = UiSize::Medium;
    bool hasBg = false;
    Background bg = {};
    ArenaVec<TableCellEl*> cells;
    ElRefiner refiner = {};

    static TableRow* New(Ctx* cx);
    TableRow* Bg(Background c);
    TableRow* Child(TableCellEl* c);
    El* IntoEl();
};

enum class TableGroupKind : uint8_t {
    Header,
    Body,
    Footer
};

struct TableGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    TableGroupKind kind = TableGroupKind::Body;
    int ix = 0;
    UiSize size = UiSize::Medium;
    ArenaVec<TableRow*> rows;
    ElRefiner refiner = {};

    TableGroup* Child(TableRow* r);
    El* IntoEl();
};

struct TableHeader {
    static TableGroup* New(Ctx* cx);
};
struct TableBody {
    static TableGroup* New(Ctx* cx);
};
struct TableFooter {
    static TableGroup* New(Ctx* cx);
};

struct TableCaption {
    Arena* a = nullptr;
    Ctx* cx = nullptr;

    int ix = 0;
    UiSize size = UiSize::Medium;
    ArenaVec<El*> children;
    ElRefiner refiner = {};

    static TableCaption* New(Ctx* cx);
    TableCaption* Child(El* e);
    El* IntoEl();
};

struct TablePart {
    TableGroup* group = nullptr;
    TableCaption* caption = nullptr;
};

struct Table {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    UiSize size = UiSize::Medium;
    bool bordered = false;
    ArenaVec<TablePart> parts;

    Str id = {};

    Str accessibilityLabel = {};

    static Table* New(Ctx* cx, Str id);
    Table* WithSize(UiSize s);

    Table* AccessibilityLabel(Str s);

    Table* Bordered(bool v = true);
    Table* Child(TableGroup* g);
    Table* Child(TableCaption* c);
    El* IntoEl();
};

}
}

#line 1 "src/ui/tag.h"

namespace gpui {

namespace component {

enum class TagVariant : uint8_t {
    Primary,
    Secondary,
    Danger,
    Success,
    Warning,
    Info
};

struct Tag {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    TagVariant variant = TagVariant::Secondary;
    bool outline = false;
    UiSize size = UiSize::Medium;
    float radius = -1;
    Str text = {};
    Rgba customBg = {};
    Rgba customFg = {};
    Rgba customBorder = {};
    bool hasCustom = false;

    ArenaVec<El*> children;

    static Tag* New(Ctx* cx, Str text);
    Tag* Primary();
    Tag* Secondary();
    Tag* Danger();
    Tag* Success();
    Tag* Warning();
    Tag* Info();
    Tag* Outline();
    Tag* WithSize(UiSize s);
    Tag* Radius(float v);
    Tag* Child(El* e);

    Tag* Custom(Rgba bg, Rgba fg, Rgba border = {});
    El* IntoEl();
};

}
}

#line 1 "src/ui/text.h"

namespace gpui {

namespace component {

using Span = gpui::Span;
using LinkMark = gpui::LinkMark;
using TextMark = gpui::TextMark;
using ImageNode = gpui::ImageNode;
using MarkdownParseContext = gpui::MarkdownParseContext;
using MarkdownNode = gpui::MarkdownNode;
using MarkdownBlockParserFn = gpui::MarkdownBlockParserFn;
using MarkdownBlockRenderFn = gpui::MarkdownBlockRenderFn;
using MarkdownPlugin = gpui::MarkdownPlugin;
using MarkdownBlockParser = gpui::MarkdownBlockParser;
using MarkdownBlockRenderer = gpui::MarkdownBlockRenderer;
using MarkdownExtensions = gpui::MarkdownExtensions;
using TextViewPlugin = gpui::TextViewPlugin;
using TextViewSetupFn = gpui::TextViewSetupFn;
using MdRun = gpui::MdRun;
using MdKind = gpui::MdKind;
using MdNode = gpui::MdNode;
using MdPlugin = gpui::MdPlugin;
using MdPluginNode = gpui::MdPluginNode;
using MdPluginParseFn = gpui::MdPluginParseFn;
using MdPluginRenderFn = gpui::MdPluginRenderFn;
using CodeBlock = gpui::CodeBlock;
using CodeHighlight = gpui::CodeHighlight;
using CodeBlockHighlighterFn = gpui::CodeBlockHighlighterFn;
using CodeBlockActionsFn = gpui::CodeBlockActionsFn;
using TableActionsFn = gpui::TableActionsFn;
using TableData = gpui::TableData;
using RangeHighlight = gpui::RangeHighlight;
using RangeHighlightError = gpui::RangeHighlightError;
using RangeHighlightErrorKind = gpui::RangeHighlightErrorKind;
using RenderedText = gpui::RenderedText;
using HeadingStyleFn = gpui::HeadingStyleFn;

using HeadingFontSizeFn = float (*)(uint8_t level, float base, void* data);
struct TextViewHeadingCompat {
    float headingBaseFontSize = 14;
    HeadingFontSizeFn headingFontSize = nullptr;
    void* data = nullptr;
};

uint32_t TextViewHeadingCompatRefine(uint8_t level, Style* out, void* data);
using TextViewStyle = gpui::TextViewStyle;
using TextViewDefaults = gpui::TextViewDefaults;
using TextViewFormat = gpui::TextViewFormat;
using TextViewMotion = gpui::TextViewMotion;
using TextViewState = gpui::TextViewState;
using TextViewLayoutState = gpui::TextViewLayoutState;
using TextView = gpui::TextView;
using Text = gpui::Text;
using Minifier = gpui::Minifier;
using HtmlInlineTag = gpui::HtmlInlineTag;

using gpui::HtmlAttrValue;
using gpui::HtmlMinify;
using gpui::HtmlParse;
using gpui::HtmlParseInlineTag;
using gpui::HtmlParseInto;
using gpui::MdDecodeEntity;
using gpui::MdParse;
using gpui::MdTableToMarkdown;
using gpui::TextViewInitKeys;

inline TextView* Markdown(Ctx* cx, Str source) {
    return gpui::MarkdownView(cx, source);
}
inline TextView* Html(Ctx* cx, Str source) {
    return gpui::HtmlView(cx, source);
}

TextViewStyle UiTextViewStyle(const Theme& theme);

void UiCodeBlockHighlighter(void* data, const CodeBlock* block, Arena* a,
                            ArenaVec<CodeHighlight>* out);

void TextViewInstallDefaults(App* app);

struct FrontmatterPlugin {
    static MarkdownPlugin New();
};

}
}

#line 1 "src/ui/title_bar.h"

namespace gpui {

namespace component {

constexpr float kTitleBarHeight = 34.f;

inline float TitleBarLeftPad() {
    return PlatSecondaryIsCommand() ? 80.f : 12.f;
}

struct TitleBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* bar = nullptr;
    El* content = nullptr;

    static TitleBar* New(Ctx* cx);
    TitleBar* Child(El* e);
    El* IntoEl();
};

}
}

#line 1 "src/ui/toolbar.h"

namespace gpui {

namespace component {

template <typename T>
inline T* PrepareForToolbar(T* control) {
    return control;
}
inline Button* PrepareForToolbar(Button* button) {
    return button->Ghost()->Compact();
}

inline UiSize ToolbarSize(UiSize size) {
    return size == UiSize::Large ? UiSize(UiSize::Medium) : size;
}

struct ToolbarItem {
    void* control = nullptr;
    El* (*build)(void* control, UiSize size) = nullptr;
    El* content = nullptr;

    El* IntoEl(Ctx* cx, UiSize size) const;
};

template <typename T>
El* ToolbarBuildSized(void* control, UiSize size) {
    return PrepareForToolbar((T*)control)->WithSize(size)->IntoEl();
}

template <typename T>
inline ToolbarItem ToolbarSized(T* control) {
    ToolbarItem item;
    item.control = control;
    item.build = &ToolbarBuildSized<T>;
    return item;
}

struct ToolbarGroup {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    UiSize size = UiSize::Small;
    Str label = {};
    float gap = 0;
    ArenaVec<ToolbarItem> items;

    static ToolbarGroup* New(Ctx* cx, Str id);
    ToolbarGroup* Label(Str value);

    ToolbarGroup* Gap(float value);
    template <typename T>
    ToolbarGroup* Child(T* control) {
        if (control) {
            items.Append(a, ToolbarSized(control));
        }
        return this;
    }

    ToolbarGroup* Content(El* content);
    ToolbarGroup* WithSize(UiSize value);
    El* IntoEl();
};

struct Toolbar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    UiSize size = UiSize::Small;
    bool disabled = false;
    ArenaVec<ToolbarItem> items;

    static Toolbar* New(Ctx* cx, Str id);

    Toolbar* Disabled(bool value);

    template <typename T>
    Toolbar* Child(T* control) {
        if (control) {
            items.Append(a, ToolbarSized(control));
        }
        return this;
    }

    Toolbar* Content(El* content);
    Toolbar* Contents(El* const* contents, int count);

    Toolbar* WithSize(UiSize value);
    El* IntoEl();
};

}
}

#line 1 "src/ui/tooltip.h"

namespace gpui {

namespace component {

struct Tooltip {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};

    static Tooltip* New(Ctx* cx, Str text);
    El* IntoEl();
};

}
}

#line 1 "src/ui/touch_selection.h"

namespace gpui {

namespace component {

struct EditMenuItem {
    Str label = {};
    Listener onClick = {};
};

struct EditMenu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Bounds anchor = {};
    ArenaVec<EditMenuItem> items;

    static EditMenu* New(Ctx* cx, Str id, Bounds anchor);
    EditMenu* Item(Str label, Listener onClick);
    El* IntoEl();
};

struct TouchSelectionOverlay {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    TouchSelectionSnapshot snapshot = {};
    bool hasSnapshot = false;
    ArenaVec<EditMenuItem> items;

    static TouchSelectionOverlay* New(Ctx* cx, Str id);
    TouchSelectionOverlay* Snapshot(const TouchSelectionSnapshot& snap);
    TouchSelectionOverlay* Item(Str label, Listener onClick);
    El* IntoEl();
};

El* WindowTouchSelectionOverlay(Ctx* cx);

}
}

#line 1 "src/ui/tree.h"

namespace gpui {

namespace component {

struct PopupMenu;

using TreeContextMenuFn = PopupMenu* (*)(void* user, Ctx* cx, int ix,
                                         const TreeEntry& entry,
                                         PopupMenu* menu);

struct Tree {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<TreeState> state = {};

    float h = 0;

    bool icons = true;
    TreeContextMenuFn contextMenu = nullptr;
    void* contextMenuUser = nullptr;

    static Tree* New(Ctx* cx, Str id, Entity<TreeState> state);
    Tree* H(float v);
    Tree* Icons(bool v);
    Tree* ContextMenu(TreeContextMenuFn fn, void* user = nullptr);
    El* IntoEl();
};

}
}

#line 1 "src/ui/virtual_list.h"

namespace gpui {

namespace component {

struct VirtualList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = StrL("vlist");
    int count = 0;
    float rowH = 32;

    float viewH = 0;
    float scrollY = 0;

    float scrollX = 0;

    const float* sizes = nullptr;
    uint8_t* needsMeasure = nullptr;

    VirtualListScrollHandle* handle = nullptr;
    Listener onRenderRow;
    El* (*row)(Ctx* cx, int ix) = nullptr;

    VirtualRowFn rowWithUser = nullptr;
    void* rowUser = nullptr;

    int scrollId = 0;
    Listener onScroll = {};

    ScrollAxis axis = ScrollAxis::Both;

    float pad = 0;

    static VirtualList* New(Ctx* cx, int count);
    VirtualList* Id(Str v);
    VirtualList* RowH(float v);
    VirtualList* ViewH(float v);
    VirtualList* ScrollY(float v);
    VirtualList* ScrollX(float v);
    VirtualList* Sizes(const float* v);
    VirtualList* MeasureRows(uint8_t* flags);
    VirtualList* Handle(VirtualListScrollHandle* h);
    VirtualList* Scroll(int id, Listener onScroll);
    VirtualList* Axis(ScrollAxis v);
    VirtualList* Pad(float v);

    VirtualList* Row(El* (*fn)(Ctx*, int));
    VirtualList* Row(VirtualRowFn fn, void* user);
    El* IntoEl();
};

}
}

#line 1 "src/ui/lib.h"

namespace gpui {
namespace component {

void Init(App* app);

}
}

#line 1 "src/sys/sysinfo.h"

namespace base {
int StrToIntUnchecked(Str s);
}

namespace gpui {

struct ProcessInfo {
    uint32_t pid = 0;
    char name[260] = {};
    float cpu = 0;
    uint64_t memory = 0;
};

struct ProcSample {
    uint32_t pid = 0;
    uint64_t cpu100ns = 0;
};

struct DiskInfo {
    float usedPct = 0;
    uint64_t total = 0;
    uint64_t used = 0;
};

struct BatteryInfo {
    bool present = false;
    bool charging = false;
    float pct = 0;
};

struct SysTimes {
    uint64_t idle = 0;
    uint64_t kernel = 0;
    uint64_t user = 0;
    bool valid = false;
};

struct SysState {
    SysTimes prevCpu;
    Vec<ProcSample> prevProcs;
    float cpu;
    float mem;
    uint64_t memTotal;
    uint64_t memUsed;
    DiskInfo disk;
    BatteryInfo battery;
    Vec<ProcessInfo> procs;
    int ncpu;

    SysState() : cpu(0), mem(0), memTotal(0), memUsed(0), ncpu(1) {
        ZeroStruct(&prevCpu);
        ZeroStruct(&disk);
        ZeroStruct(&battery);
    }
};

void SysStateInit(SysState* s);
void SysStateFree(SysState* s);
void SysRefresh(SysState* s);

enum class ProcessSort : uint8_t {
    Pid = 0,
    Name = 1,
    Cpu = 2,
    Memory = 3
};
void SysSortProcesses(SysState* s, ProcessSort field, bool descending,
                      int keepTop);

TempStr FormatBytesTemp(uint64_t bytes);
TempStr FormatPctTemp(float v, int decimals);

bool SysSelfPrivateMemory(uint64_t* bytes);
}

#line 1 "src/shell/error.h"

namespace gpui {

struct ShellError {
    Str message;

    bool IsSet() const { return len(message) > 0; }
};

void ShellErrorClear(ShellError* error);
void ShellErrorSet(ShellError* error, Str message);

}

#line 1 "src/shell/value.h"

namespace gpui::shell {

enum class BridgedKind : uint8_t {
    Nil,
    Bool,
    Number,
    String,
};

struct Bridged {
    BridgedKind kind = BridgedKind::Nil;
    bool boolean = false;
    double number = 0;
    Str string;

    static Bridged Nil();
    static Bridged Bool(bool value);
    static Bridged Number(double value);
    static Bridged String(Str value);
};

bool BridgedAsF32(const Bridged& value, float* out,
                  ShellError* error = nullptr);
bool BridgedAsString(const Bridged& value, Str* out,
                     ShellError* error = nullptr);
bool BridgedAsPixels(const Bridged& value, float* out,
                     ShellError* error = nullptr);
bool BridgedAsColor(const Bridged& value, Hsla* out,
                    ShellError* error = nullptr);
bool BridgedIsTruthy(const Bridged& value);
Str BridgedDescribe(Arena* arena, const Bridged& value);
bool BridgedArg(const Bridged* args, int count, int index, Str method,
                Bridged* out, ShellError* error = nullptr);

}

#line 1 "src/shell/spec.h"

namespace gpui {
struct Policy;
}

namespace gpui::shell {

using SpecId = uint32_t;
using CallbackId = uint64_t;

struct StructureFingerprint {
    uint64_t value = 0;

    bool operator==(const StructureFingerprint& other) const {
        return value == other.value;
    }
    bool operator!=(const StructureFingerprint& other) const {
        return value != other.value;
    }
};

uint64_t StructureMix(uint64_t state, uint64_t value);

enum class BackgroundKind : uint8_t {
    Solid,
    LinearGradient,
    PatternSlash,
    Checkerboard,
};

struct BackgroundSpec {
    BackgroundKind kind = BackgroundKind::Solid;
    Str color;
    float opacity = 1;
    float angle = 0;
    Str fromColor;
    float fromPosition = 0;
    Str toColor;
    float toPosition = 1;
    Str colorSpace;
    float width = 0;
    float interval = 0;
    float size = 0;
};

enum class ComponentKind : uint8_t {
    Div,
    HFlex,
    VFlex,
    ChildView,
    Text,
    TextView,
    Button,
    Link,
    Checkbox,
    Switch,
    Scrollbar,
    Input,
    Textarea,
    NumberInput,
    OtpInput,
    Svg,

    Accordion,

    AccordionItem,
    AccordionHeader,

    AccordionPanel,
    AccordionTrigger,

    Pagination,

    Avatar,

    AvatarImage,
    AvatarFallback,
    Image,
    PathFill,
    PathStroke,
    Tabs,
    Tab,
    Progress,
    ProgressTrack,
    ProgressIndicator,
    FpsMonitor,
    Slider,
    SliderTrack,
    SliderIndicator,
    SliderThumb,
    Radio,
    Toggle,
    RadioGroup,
    ToggleGroup,
    Table,
    TableHeader,
    TableBody,
    TableRow,
    TableHead,
    TableCell,
    TableCaption,
    HResizable,
    VResizable,
    ResizablePanel,
    Collapsible,
    Popover,
    HoverCard,
    Popup,
    Select,
    Combobox,
    DatePicker,

    DockArea,

    DockContent,
    VVirtualList,
    HVirtualList,

    List,
    UniformList,

    Registered,
};

enum class TextViewFormat : uint8_t {
    Html,
    Markdown,
};

struct VirtualListSpec {
    Str id;
    Axis axis = Axis::Vertical;
    Size* sizes = nullptr;
    int sizeCount = 0;
    CallbackId getKey = 0;
    CallbackId renderItems = 0;
};

struct ListSpec {

    Str id;

    int itemCount = 0;

    CallbackId getKey = 0;

    CallbackId renderItems = 0;
};

struct ComponentPayload {
    const void* data = nullptr;
    const void* type = nullptr;
};

struct Component {
    ComponentKind kind = ComponentKind::Div;
    Str text;

    Str value;
    TextViewFormat textViewFormat = TextViewFormat::Markdown;

    Policy* policy = nullptr;
    uint64_t handle = 0;
    uint32_t index = 0;
    BackgroundSpec background;
    float strokeWidth = 0;
    VirtualListSpec* virtualList = nullptr;
    ListSpec* list = nullptr;
    ComponentPayload payload;
};

const char* ComponentName(const Component& component);

enum class SpecOpKind : uint8_t {
    NullaryStyle,
    ParamStyle,
    Method,
    Callback,

    ActionCallback,
    StateStyle,
    Slot,

    RegisteredMethod,
};

struct SpecOp {
    SpecOpKind kind = SpecOpKind::Method;
    Str name;
    uint16_t styleIndex = 0;
    CallbackId callback = 0;
    SpecId node = 0;
    Bridged* args = nullptr;
    int argCount = 0;
    ComponentPayload payload;
};

struct SpecNode {
    Component component;
    ArenaVec<SpecOp> ops;
    ArenaVec<SpecId> children;
};

enum class SpecErrorKind : uint8_t {
    None,
    Claimed,
    Expired,
    AlreadyParented,
    SelfParent,
    DuplicateChildView,
};

struct SpecError {
    SpecErrorKind kind = SpecErrorKind::None;
    Str component;
};

Str SpecErrorMessage(Arena* arena, const SpecError& error);

enum class SlotSiteKind : uint8_t {

    Text,

    Argument,

    Handler,
};

struct SlotSite {
    SlotSiteKind kind = SlotSiteKind::Text;
    uint16_t op = 0;
    uint8_t argument = 0;

    bool operator==(const SlotSite& other) const {
        return kind == other.kind && op == other.op &&
               argument == other.argument;
    }
};

struct Slot {
    SpecId node = 0;
    SlotSite site;

    uint16_t argument = 0;
};

enum class SlotValueKind : uint8_t {
    Text,
    Value,
    Handler
};

struct SlotValue {
    SlotValueKind kind = SlotValueKind::Text;
    Str text;
    Bridged value;
    CallbackId handler = 0;
};

class SpecArena;

struct Template {
    SpecArena* arena = nullptr;
    SpecId root = 0;
    Vec<Slot> slots;
    int arity = 0;

    void* application = nullptr;

    ~Template();
};

class SpecArena {
  public:
    SpecArena();

    explicit SpecArena(Arena* borrowed);
    SpecArena(const SpecArena&) = delete;
    SpecArena& operator=(const SpecArena&) = delete;
    ~SpecArena();

    void Reset();
    int Len() const { return nodes.len; }
    bool IsEmpty() const { return nodes.len == 0; }
    SpecId Push(const Component& component);
    bool PushChildView(const Component& component, SpecId* out,
                       SpecError* error = nullptr);

    bool PushDockArea(uint64_t handle, SpecId* out, SpecError* error = nullptr);
    const SpecNode* Node(SpecId id) const;
    bool PushOp(SpecId id, const SpecOp& op, SpecError* error = nullptr);
    bool Claim(SpecId id, SpecError* error = nullptr);

    bool CanClaim(SpecId id, SpecError* error = nullptr) const;

    bool IsLive(SpecId id, SpecError* error = nullptr) const {
        return CheckLive(id, error);
    }
    bool Attach(SpecId parent, SpecId child, SpecError* error = nullptr);
    bool ClaimVirtualItems(uint64_t count, uint64_t limit);
    Str DebugTree(Arena* into, SpecId root) const;

    StructureFingerprint Structure() const { return {structure}; }

    SpecId Graft(const Template& tmpl);

    bool WriteSlot(SpecId base, const Slot& slot, const SlotValue& value,
                   SpecError* error = nullptr);

    bool MountsAnEntity() const { return mountedViews.len > 0; }

    bool HasRegistered() const;

    Arena* Storage() const { return arena; }

  private:
    Arena* arena = nullptr;
    bool ownsArena = true;
    Vec<SpecNode*> nodes;
    Vec<uint8_t> parented;
    Vec<uint8_t> claimed;
    Vec<uint64_t> mountedViews;

    Vec<Policy*> policies;
    uint64_t virtualItems = 0;

    uint64_t structure = 0;

    bool CheckLive(SpecId id, SpecError* error) const;
    void ReleasePolicies();
    Component CopyComponent(const Component& component);
    SpecOp CopyOp(const SpecOp& op);
    void WriteTree(StrBuilder* out, SpecId id, int depth) const;
};

}

#line 1 "src/shell/component_registry.h"

namespace gpui {
class ShellRuntime;
struct ScriptView;
}

namespace gpui::shell {

constexpr uint32_t kComponentRegistryApiVersion = 1;

extern const char kDefaultComponentModule[];

template <class T>
struct Slice {
    const T* items = nullptr;
    int count = 0;

    constexpr Slice() = default;
    constexpr Slice(const T* items, int count) : items(items), count(count) {}
    template <int N>
    constexpr Slice(const T (&array)[N]) : items(array), count(N) {}

    const T* begin() const { return items; }
    const T* end() const { return items + count; }
    const T& operator[](int at) const { return items[at]; }
};

enum class SchemaKind : uint8_t {
    String,
    Number,
    Boolean,
    Element,
    Entity,
    Callback,
    Enum,
    Array,
    Optional,
};

struct ArgumentSchema {
    SchemaKind kind = SchemaKind::String;
    const char* text = nullptr;
    Slice<const char*> values = {};
    const ArgumentSchema* item = nullptr;
};

constexpr ArgumentSchema SchemaString() {
    return {SchemaKind::String, nullptr, {}, nullptr};
}
constexpr ArgumentSchema SchemaNumber() {
    return {SchemaKind::Number, nullptr, {}, nullptr};
}
constexpr ArgumentSchema SchemaBoolean() {
    return {SchemaKind::Boolean, nullptr, {}, nullptr};
}
constexpr ArgumentSchema SchemaElement() {
    return {SchemaKind::Element, nullptr, {}, nullptr};
}
constexpr ArgumentSchema SchemaEntity(const char* kind) {
    return {SchemaKind::Entity, kind, {}, nullptr};
}
constexpr ArgumentSchema SchemaCallback(const char* signature) {
    return {SchemaKind::Callback, signature, {}, nullptr};
}
constexpr ArgumentSchema SchemaEnum(Slice<const char*> values) {
    return {SchemaKind::Enum, nullptr, values, nullptr};
}
constexpr ArgumentSchema SchemaArray(const ArgumentSchema* item) {
    return {SchemaKind::Array, nullptr, {}, item};
}
constexpr ArgumentSchema SchemaOptional(const ArgumentSchema* item) {
    return {SchemaKind::Optional, nullptr, {}, item};
}

struct ArgumentDescriptor {
    const char* name = nullptr;
    ArgumentSchema schema = {};
};

enum class ComponentArgumentKind : uint8_t {
    String,
    Number,
    Boolean,
    Element,
    Entity,
    Callback,
    Enum,
    Array,
    Optional,
};

struct ComponentArgument {
    ComponentArgumentKind kind = ComponentArgumentKind::String;
    Str string;
    double number = 0;
    bool boolean = false;
    SpecId element = 0;
    const char* entityKind = nullptr;
    uint64_t handle = 0;
    CallbackId callback = 0;
    const ComponentArgument* items = nullptr;
    int count = 0;

    const ComponentArgument* Some() const;
};

enum class DataKind : uint8_t {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object,
};

struct ComponentDataValue {
    DataKind kind = DataKind::Null;
    bool boolean = false;
    double number = 0;
    Str string;
    const ComponentDataValue* items = nullptr;

    const Str* keys = nullptr;
    int count = 0;

    static ComponentDataValue Null();
    static ComponentDataValue Boolean(bool value);
    static ComponentDataValue Number(double value);
    static ComponentDataValue String(Str value);
    static ComponentDataValue Array(const ComponentDataValue* items, int count);

    const ComponentDataValue* Get(Str key) const;
};

using ComponentCallbackArgument = ComponentDataValue;
using ComponentCallbackValue = ComponentDataValue;

template <class T>
const void* PayloadTag() {
    static const char tag = 0;
    return &tag;
}

struct EmptyPayload {};

struct PayloadBuild;

using PayloadFactory = bool (*)(PayloadBuild* build,
                                const ComponentArgument* arguments, int count);

struct PayloadBuild {
    Arena* a = nullptr;
    ComponentPayload out = {};
    Str error;

    template <class T>
    T* New() {
        T* value = ArenaNew<T>(a);
        out.data = value;
        out.type = PayloadTag<T>();
        return value;
    }

    template <class T>
    bool Mark() {
        out.data = nullptr;
        out.type = PayloadTag<T>();
        return true;
    }
    bool Fail(Str message);
    Str Dup(Str value) const { return StrDup(a, value); }
};

template <class T>
const T* PayloadAs(const ComponentPayload& payload) {
    return payload.type == PayloadTag<T>() ? (const T*)payload.data : nullptr;
}
template <class T>
bool PayloadIs(const ComponentPayload& payload) {
    return payload.type == PayloadTag<T>();
}

struct ConstructorDescriptor {
    const char* exportName = nullptr;
    Slice<ArgumentDescriptor> arguments = {};
    PayloadFactory factory = nullptr;

    const char* deprecationReplacement = nullptr;
    const char* deprecationMessage = nullptr;
};

struct MethodDescriptor {
    const char* name = nullptr;
    Slice<ArgumentDescriptor> arguments = {};
    const char* documentation = nullptr;
    PayloadFactory recorder = nullptr;
};

struct MaterializeRequest;

using ComponentMaterializer = El* (*)(MaterializeRequest * request);

struct ComponentDescriptor {
    const char* name = nullptr;
    Slice<ConstructorDescriptor> constructors = {};
    Slice<MethodDescriptor> methods = {};
    const char* documentation = nullptr;
    ComponentMaterializer materialize = nullptr;
};

struct StateBuild {
    Window* window = nullptr;
    App* app = nullptr;

    Arena* a = nullptr;
    void* value = nullptr;
    void (*destroy)(void* value) = nullptr;
    Str error;

    template <class T>
    T* New() {
        T* state = new T();
        value = state;
        destroy = [](void* held) { delete (T*)held; };
        return state;
    }
    bool Fail(Str message);
};

using StateFactory = bool (*)(StateBuild* build,
                              const ComponentArgument* arguments, int count);

struct StateCall {
    Window* window = nullptr;
    App* app = nullptr;
    Arena* a = nullptr;
    ComponentDataValue out = {};
    Str error;

    Str code;

    bool Fail(Str message);
};

using StateMethod = bool (*)(StateCall* call, void* state,
                             const ComponentDataValue* arguments, int count);

struct StateMethodDescriptor {
    const char* name = nullptr;

    const char* signature = nullptr;
    bool readonly = false;
    StateMethod call = nullptr;
};

struct StateDescriptor {
    const char* exportName = nullptr;
    const char* kind = nullptr;
    Slice<ArgumentDescriptor> arguments = {};
    const char* documentation = nullptr;
    StateFactory factory = nullptr;
    Slice<StateMethodDescriptor> methods = {};
};

enum class RegistryErrorKind : uint8_t {
    None,
    IncompatibleApiVersion,
    DuplicateComponent,
    InvalidComponent,
    DuplicateExport,
    InvalidExport,
    InvalidMethod,
    InvalidArgument,
    DuplicateArgument,
    InvalidArgumentSchema,
    RequiredArgumentAfterOptional,
    DuplicateMethod,
    UndocumentedMethod,
    UnreachableMethodVocabulary,
    InvalidDeprecationReplacement,
    EmptyConstructorList,
    InvalidStateKind,
    DuplicateStateKind,
    InvalidModuleSpecifier,
};

struct RegistryError {
    RegistryErrorKind kind = RegistryErrorKind::None;
    uint32_t expected = 0;
    uint32_t actual = 0;
    const char* component = nullptr;
    const char* callable = nullptr;
    const char* argument = nullptr;
    const char* reason = nullptr;
    const char* literal = nullptr;

    bool IsSet() const { return kind != RegistryErrorKind::None; }
    bool operator==(const RegistryError& other) const;
};

Str RegistryErrorMessage(Arena* into, const RegistryError& error);

using ComponentInitializer = void (*)(App* app);

struct ComponentWindowOptions {
    Str title;
    int dipW = 880;
    int dipH = 720;
    WinOpts opts = {};
};

using ComponentWindowBuild = EntityId (*)(Window* window, App* app, void* data);

using ComponentWindowOpener = Window* (*)(App * app,
                                          const ComponentWindowOptions& options,
                                          ComponentWindowBuild build,
                                          void* data);

class FrozenComponentRegistry;

class ComponentRegistry {
  public:
    ComponentRegistry() = default;
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;
    ~ComponentRegistry();

    bool Open(uint32_t apiVersion, const char* moduleSpecifier,
              RegistryError* error);
    void WithInitializer(ComponentInitializer initializer);

    void WithWindowOpener(ComponentWindowOpener opener);

    bool Register(const ComponentDescriptor* descriptor, RegistryError* error,
                  uint32_t* id = nullptr);
    bool RegisterState(const StateDescriptor* descriptor, RegistryError* error);

    void Freeze(FrozenComponentRegistry* out);

  private:
    const char* moduleSpecifier = nullptr;
    ComponentInitializer initializer = nullptr;
    ComponentWindowOpener windowOpener = nullptr;
    Vec<const ComponentDescriptor*> descriptors;
    Vec<const StateDescriptor*> states;
    Vec<const char*> exports;
    Vec<const char*> stateKinds;

    bool HasExport(const char* name) const;
};

class FrozenComponentRegistry {
  public:
    FrozenComponentRegistry() = default;
    FrozenComponentRegistry(const FrozenComponentRegistry&) = delete;
    FrozenComponentRegistry& operator=(const FrozenComponentRegistry&) = delete;
    ~FrozenComponentRegistry();

    const char* ModuleSpecifier() const { return moduleSpecifier; }
    ComponentInitializer Initializer() const { return initializer; }

    ComponentWindowOpener WindowOpener() const { return windowOpener; }
    int DescriptorCount() const { return len(descriptors); }
    const ComponentDescriptor* Descriptor(uint32_t id) const;
    int StateCount() const { return len(states); }
    const StateDescriptor* State(int at) const;
    const StateDescriptor* StateOfKind(Str kind) const;

    const ComponentDescriptor* Find(Str name, uint32_t* id = nullptr) const;
    const MethodDescriptor* Method(uint32_t component, Str name) const;

    Str JavaScriptModuleSource(Str stateProof) const;

  private:
    friend class ComponentRegistry;
    const char* moduleSpecifier = nullptr;
    ComponentInitializer initializer = nullptr;
    ComponentWindowOpener windowOpener = nullptr;
    Vec<const ComponentDescriptor*> descriptors;
    Vec<const StateDescriptor*> states;
};

bool IsJavaScriptIdentifier(Str name);

void AppendComponentDeclarations(StrBuilder* out,
                                 const FrozenComponentRegistry* components,
                                 Str inlineTokenTypes);
void AppendComponentElementUnion(StrBuilder* out,
                                 const FrozenComponentRegistry* components);

extern const char* const kRegisteredCommonBehaviors[3];

bool IsRegisteredCommonSlot(Str name);

struct CheckedVocabulary {
    const char* method;
    Slice<const char*> accepted;
};
Slice<CheckedVocabulary> PreludeCheckedVocabularies();

constexpr int kMaxRetainedComponentStates = 4096;

class ComponentStateStore {
  public:
    ComponentStateStore() = default;
    ComponentStateStore(const ComponentStateStore&) = delete;
    ComponentStateStore& operator=(const ComponentStateStore&) = delete;
    ~ComponentStateStore();

    int Len() const { return len(entries); }

    bool Insert(const char* kind, void* owner, void* value,
                void (*destroy)(void*), uint64_t* handle, Str* error, Arena* a);
    const char* Kind(uint64_t handle) const;

    void* Get(uint64_t handle, const char* kind, Str* error, Arena* a) const;
    void ReleaseApplication(void* application);
    void Clear();

  private:
    struct Entry {
        uint64_t handle = 0;
        const char* kind = nullptr;
        void* owner = nullptr;
        void* value = nullptr;
        void (*destroy)(void*) = nullptr;
    };
    uint64_t nextHandle = 0;
    Vec<Entry> entries;
};

struct ComponentWindowEffects;

struct ComponentCallback {
    CallbackId id = 0;

    bool IsSet() const { return id != 0; }

    bool InvokeWith(ShellRuntime* runtime, const ComponentDataValue* arguments,
                    int count, Window* window, App* app,
                    ComponentDataValue* result = nullptr, Str* error = nullptr,
                    Arena* a = nullptr) const;

    void InvokeAndReport(ShellRuntime* runtime, const char* context,
                         const ComponentDataValue* arguments, int count,
                         Window* window, App* app) const;

    bool SnapshotWith(ShellRuntime* runtime,
                      const ComponentDataValue* arguments, int count, Ctx* cx,
                      ComponentDataValue* out, Arena* a,
                      Str* error = nullptr) const;

    bool SnapshotRowsWith(ShellRuntime* runtime,
                          const ComponentDataValue* arguments, int count,
                          Ctx* cx, const ComponentDataValue** rows,
                          int* rowCount, Arena* a, Str* error = nullptr) const;

    El* BuildWith(ShellRuntime* runtime, const ComponentDataValue* arguments,
                  int count, Ctx* cx, Str* error = nullptr) const;

    El* BuildInteractiveWith(ShellRuntime* runtime,
                             const ComponentDataValue* arguments, int count,
                             Ctx* cx, Str* error = nullptr) const;

    ComponentWindowEffects WindowEffects(ShellRuntime* runtime, Arena* a) const;
};

using ComponentEffectBody = bool (*)(void* user, Window* window, App* app,
                                     Str* error, Arena* a);

struct ComponentEventEffects {
    Window* window = nullptr;
    App* app = nullptr;
    Arena* a = nullptr;
    Vec<Str> completed;
    Vec<Str> inProgress;

    ComponentEventEffects() = default;
    ComponentEventEffects(const ComponentEventEffects&) = delete;
    ComponentEventEffects& operator=(const ComponentEventEffects&) = delete;
    ~ComponentEventEffects();

    bool RunOnce(Str key, ComponentEffectBody body, void* user, bool* executed,
                 Str* error);
};

using ComponentEventBody = bool (*)(ComponentEventEffects* effects, void* user,
                                    Str* error);

struct ComponentWindowEffects {
    ShellRuntime* runtime = nullptr;
    ComponentCallback reporter = {};
    bool* active = nullptr;

    bool IsActive() const { return active && *active; }

    bool Event(Window* window, App* app, ComponentEventBody body, void* user,
               Str* error, Arena* a) const;
};

struct ComponentAppEffectCleanup {
    void (*run)(App* app, void* user) = nullptr;
    void (*drop)(void* user) = nullptr;
    void* user = nullptr;
};

struct ComponentAppEffectInstall {
    ComponentAppEffectCleanup (*run)(App* app, void* user) = nullptr;
    void (*drop)(void* user) = nullptr;
    void* user = nullptr;
};

struct ComponentAppEffects {
    ShellRuntime* runtime = nullptr;

    void* application = nullptr;
    EntityId view = {};

    bool Replace(Str key, Str revision, Window* window, App* app,
                 ComponentAppEffectInstall install, Str* error, Arena* a) const;
};

struct PendingAppEffect {
    Str key;
    Str revision;
};
struct InstalledAppEffect {
    Str key;
    Str revision;
    ComponentAppEffectCleanup cleanup = {};
};
struct ComponentAppEffectQueue {
    Vec<PendingAppEffect> pending;
    Vec<InstalledAppEffect> installed;

    ComponentAppEffectQueue() = default;
    ComponentAppEffectQueue(const ComponentAppEffectQueue&) = delete;
    ComponentAppEffectQueue& operator=(const ComponentAppEffectQueue&) = delete;

    ~ComponentAppEffectQueue();

    const Str* Pending(Str key) const;
    InstalledAppEffect* Installed(Str key);
    void RemovePending(Str key);
};

bool QueueComponentAppEffect(ComponentAppEffectQueue* queue, Str key,
                             Str revision);

struct ComponentEventBinding;
using ComponentEventRun = void (*)(const ComponentEventBinding* binding,
                                   ScriptView* view, Ctx* cx,
                                   const void* event);
struct ComponentEventBinding {
    ComponentEventRun run = nullptr;
    ComponentCallback callback = {};
    void* user = nullptr;
    int64_t value = 0;
};

Listener ComponentListener(Ctx* cx, ComponentEventRun run,
                           ComponentCallback callback, void* user = nullptr,
                           int64_t value = 0);

Listener ComponentValueListener(Ctx* cx, Str key, ComponentEventRun run,
                                ComponentCallback callback,
                                void* user = nullptr);

struct ComponentChild {
    SpecId id = 0;

    const char* componentName = nullptr;
    const ComponentPayload* payload = nullptr;
    bool consumed = false;
};

struct ComponentElementFactory {
    SpecId id = 0;
    bool IsSet() const { return id != 0 || set; }
    bool set = false;
};

struct RecordedComponentMethod {
    Str name;
    ComponentPayload payload;
};

struct MaterializeRequest {
    Ctx* cx = nullptr;
    ShellRuntime* runtime = nullptr;
    const SpecArena* specs = nullptr;
    const SpecNode* node = nullptr;
    SpecId id = 0;
    const ComponentDescriptor* descriptor = nullptr;
    ComponentPayload payload = {};

    Str elementId;
    bool disabled = false;
    bool selected = false;
    CallbackId onClick = 0;
    ShellError* error = nullptr;

    const char* ComponentName() const {
        return descriptor ? descriptor->name : "";
    }
    template <class T>
    const T* PayloadAs() const {
        return shell::PayloadAs<T>(payload);
    }

    int MethodCount() const;
    RecordedComponentMethod Method(int at) const;

    bool HasStyle() const;

    El* ApplyStyle(El* target);

    ElRefiner TakeStyle();

    int ChildrenLen() const;

    bool TakeChildren(El*** out, int* count);

    bool AppendChildren(El* parent);
    bool TakeTypedChildren(ComponentChild** out, int* count);
    El* MaterializeChild(ComponentChild* child);

    El* TakeSlot(const char* name);

    int TakeSlots(const char* name, El*** out);
    ComponentElementFactory TakeSlotFactory(const char* name);
    El* BuildFactory(ComponentElementFactory factory);

    El* Finish(El* element);

    El* ResolveElement(const ComponentArgument& argument);
    ComponentCallback ResolveCallback(const ComponentArgument& argument) const;

    void* State(const ComponentArgument& argument, const char* kind);
    template <class T>
    T* StateAs(const ComponentArgument& argument, const char* kind) {
        return (T*)State(argument, kind);
    }
    ComponentCallback OnClick() const { return {onClick}; }

    bool AppEffects(ComponentAppEffects* out);

    El* Fail(Str message);

    bool styleTaken = false;
    uint8_t lane = 0;
    int childrenTaken = 0;
    ComponentChild* typed = nullptr;
    int typedCount = 0;

    bool* slotTaken = nullptr;
    Str failure;
};

}

#line 1 "src/component_shell/lib.h"

namespace gpui::component_shell {

void Init(App* app);

const shell::FrozenComponentRegistry* Components();

Window* OpenWindowWithRoot(App* app,
                           const shell::ComponentWindowOptions& options,
                           shell::ComponentWindowBuild build, void* data);

void MountWindowRoot(App* app, Window* window,
                     shell::ComponentWindowBuild build, void* data);

bool Register(shell::ComponentRegistry* registry, shell::RegistryError* error);

}

#line 1 "src/component_shell/support.h"

namespace gpui::component_shell {

using shell::ArgumentDescriptor;
using shell::ArgumentSchema;
using shell::ComponentArgument;
using shell::ComponentDescriptor;
using shell::ComponentPayload;
using shell::ConstructorDescriptor;
using shell::MaterializeRequest;
using shell::MethodDescriptor;
using shell::PayloadBuild;
using shell::SchemaArray;
using shell::SchemaBoolean;
using shell::SchemaCallback;
using shell::SchemaElement;
using shell::SchemaEntity;
using shell::SchemaEnum;
using shell::SchemaNumber;
using shell::SchemaOptional;
using shell::SchemaString;
using shell::Slice;

struct Empty {};

struct CommonBehavior {};

bool RecordEmpty(PayloadBuild* build, const ComponentArgument*, int);
bool RecordCommonBehavior(PayloadBuild* build, const ComponentArgument*, int);

inline constexpr ArgumentDescriptor kOnClickArguments[] = {
    {"callback", SchemaCallback("(event: ClickEvent, cx: Context) => void")}};
inline constexpr ArgumentDescriptor kDisabledArguments[] = {
    {"disabled", SchemaBoolean()}};
inline constexpr ArgumentDescriptor kSelectedArguments[] = {
    {"selected", SchemaBoolean()}};

inline constexpr MethodDescriptor kOnClickMethod = {
    "on_click", kOnClickArguments,
    "Invokes the callback when this component is activated.",
    &RecordCommonBehavior};

inline constexpr MethodDescriptor kDisabledMethod = {
    "disabled", kDisabledArguments,
    "Controls whether this component accepts interaction.",
    &RecordCommonBehavior};

inline constexpr MethodDescriptor kSelectedMethod = {
    "selected", kSelectedArguments,
    "Marks this component as the selected one among its siblings.",
    &RecordCommonBehavior};

inline constexpr const char* kSizeLiterals[] = {"xsmall", "small", "medium",
                                                "large"};

UiSize SizeOfLiteral(Str literal);

bool RejectStyle(MaterializeRequest* request, const char* name);

bool RequireChild(MaterializeRequest* request, const char* parent,
                  const char* actual, Slice<const char*> allowed);

El* Carrier(Ctx* cx, const void* tag, void* value);

void* TakeCarried(MaterializeRequest* request, El* element, const void* tag,
                  const char* name);

template <class T>
El* CarrierOf(Ctx* cx, T* value) {
    return Carrier(cx, shell::PayloadTag<T>(), value);
}
template <class T>
T* TakeCarriedAs(MaterializeRequest* request, El* element, const char* name) {
    return (T*)TakeCarried(request, element, shell::PayloadTag<T>(), name);
}

template <class T>
struct EntityState {
    App* app = nullptr;
    Entity<T> entity = {};

    ~EntityState() {
        if (app && entity.IsValid()) EntityDrop(app, entity.id);
    }
};

template <class T>
Entity<T> UseKeyedState(Ctx* cx, Str key, Str kind) {
    return KeyedEntity<T>(
        cx, KeyedKey((uint32_t)HashClickId(key), (uint32_t)HashClickId(kind)));
}

struct DeferredSlot {
    ShellRuntime* runtime = nullptr;
    const shell::SpecArena* specs = nullptr;
    ShellError* error = nullptr;
    shell::ComponentElementFactory factory = {};

    const char* failure = nullptr;
};
DeferredSlot* NewDeferredSlot(MaterializeRequest* request,
                              shell::ComponentElementFactory factory,
                              const char* failure);

El* BuildDeferredSlot(const DeferredSlot* slot, Ctx* cx);

template <class T, class F>
void EachMethod(const MaterializeRequest* request, F&& visit) {
    for (const shell::SpecOp& op : request->node->ops) {
        if (op.kind != shell::SpecOpKind::RegisteredMethod) continue;
        if (const T* value = shell::PayloadAs<T>(op.payload)) visit(*value);
    }
}

bool ParseColorArgument(PayloadBuild* build, const char* component, Str text,
                        Rgba* out);

TempStr F64DisplayTemp(double value);

}

#line 1 "src/component_shell/compound/common.h"

namespace gpui::component_shell::compound::common {

bool NonemptyId(Str id, const char* component, Str* error);

bool NonnegativeUsize(double value, Str label, uint64_t* out, Str* error);

bool FiniteF32(double value, Str label, float* out, Str* error);

int UsizeInt(uint64_t value);

int64_t UsizeInt64(uint64_t value);

}

#line 1 "src/component_shell/controls/support.h"

namespace gpui::component_shell::controls::support {

struct CommonOp {
    enum Kind : uint8_t {
        Size,
        Label,
        Tooltip,
        Checked,
        Outline,
        Change,
    } kind = Size;
    UiSize size = UiSize::Medium;
    Str text;
    bool checked = false;

    shell::ComponentArgument change = {};
};

bool RecordLabel(PayloadBuild* build, const ComponentArgument* args, int);
bool RecordTooltip(PayloadBuild* build, const ComponentArgument* args, int);
bool RecordChecked(PayloadBuild* build, const ComponentArgument* args, int);
bool RecordSize(PayloadBuild* build, const ComponentArgument* args, int);
bool RecordOutline(PayloadBuild* build, const ComponentArgument*, int);
bool RecordChange(PayloadBuild* build, const ComponentArgument* args, int);

inline constexpr ArgumentDescriptor kChangeArguments[] = {
    {"on_change", SchemaCallback("(checked: boolean, cx: Context) => void")}};
inline constexpr ArgumentDescriptor kSizeArguments[] = {
    {"size", SchemaEnum(kSizeLiterals)}};

inline constexpr MethodDescriptor kChangeMethod = {
    "on_change", kChangeArguments,
    "Reports the new checked state after a click.", &RecordChange};
inline constexpr MethodDescriptor kSizeMethod = {
    "size", kSizeArguments, "Sets the semantic control size.", &RecordSize};
inline constexpr MethodDescriptor kOutlineMethod = {
    "outline",
    {},
    "Uses the component's outline presentation.",
    &RecordOutline};

}

#line 1 "src/component_shell/display/common.h"

namespace gpui::component_shell::display::common {

bool NonEmptyId(PayloadBuild* build, const char* component, Str id);

bool EnsureNoChildren(MaterializeRequest* request, const char* component);

}

#line 1 "src/component_shell/families.h"

namespace gpui {
struct QuestionnaireState;
}
namespace gpui::component {
struct NativeMenu;
}

namespace gpui::component_shell {

using RegisterFamily = bool (*)(shell::ComponentRegistry* registry,
                                shell::RegistryError* error);

bool RegisterSpinner(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterSeparator(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterSkeleton(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterChat(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterEmpty(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterInputGroup(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterControls(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDelegateCollections(shell::ComponentRegistry*,
                                 shell::RegistryError*);
bool RegisterDelegateCombobox(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDelegateSelect(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDataTable(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDisplay(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterCompound(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterTypedCompound(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterLifecycle(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterCollections(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterCommand(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterWindowEffects(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterOverlays(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterRetainedForms(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterLayout(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterMedia(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterScroll(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterSettings(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterStructured(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterNavigation(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterBasic(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterChart(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterCarousel(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterQuestionnaire(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterControlsAction(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterControlsDisplay(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterControlsText(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterDelegateCollectionsList(shell::ComponentRegistry*,
                                     shell::RegistryError*);

using ListRowProbe = void (*)(Str id);
void SetListRowProbe(ListRowProbe probe);

using ComboboxEventProbe = void (*)(bool confirm, const Str* values, int count);
void SetComboboxEventProbe(ComboboxEventProbe probe);

using SelectProbe = void (*)(Str value);
void SetSelectProbe(SelectProbe probe);

using DataTableProbe = void (*)(bool built);
void SetDataTableProbe(DataTableProbe probe);

bool RegisterDisplayAlert(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDisplayBreadcrumb(shell::ComponentRegistry*,
                               shell::RegistryError*);
bool RegisterDisplayClipboard(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDisplayGroupBox(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDisplayRating(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDisplayStatusBar(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterDisplayToolbar(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterCompoundAvatar(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterCompoundCollapsible(shell::ComponentRegistry*,
                                 shell::RegistryError*);
bool RegisterCompoundPagination(shell::ComponentRegistry*,
                                shell::RegistryError*);
bool RegisterCompoundProgress(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterCompoundRadio(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterLifecycleTooltip(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterLifecycleMenu(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterCollectionsTree(shell::ComponentRegistry*, shell::RegistryError*);

using TreeRowProbe = void (*)(Str id, Str label, bool selected);
void SetTreeRowProbe(TreeRowProbe probe);

bool RegisterCommandCommand(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterCommandNativeMenu(shell::ComponentRegistry*,
                               shell::RegistryError*);

using NativeMenuShowProbe = bool (*)(const component::NativeMenu* menu,
                                     Str* error, Arena* a);
void SetNativeMenuShowProbe(NativeMenuShowProbe probe);

using WindowEffectsReporterFailureProbe = void (*)(Str diagnosis);
void SetWindowEffectsReporterFailureProbe(
    WindowEffectsReporterFailureProbe probe);

bool RegisterOverlaysHoverCard(shell::ComponentRegistry*,
                               shell::RegistryError*);
bool RegisterOverlaysPopover(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterOverlaysDropdownMenu(shell::ComponentRegistry*,
                                  shell::RegistryError*);

bool RegisterLayoutTextarea(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterLayoutResizable(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterMediaImage(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterMediaEditor(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterScrollScroll(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterStructuredDescriptionList(shell::ComponentRegistry*,
                                       shell::RegistryError*);
bool RegisterStructuredForm(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterStructuredTable(shell::ComponentRegistry*, shell::RegistryError*);

bool RegisterNavigationIcon(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterNavigationSidebar(shell::ComponentRegistry*,
                               shell::RegistryError*);

bool RegisterBasicText(shell::ComponentRegistry*, shell::RegistryError*);
bool RegisterBasicDropdownButton(shell::ComponentRegistry*,
                                 shell::RegistryError*);

using ChartErrorProbe = void (*)(Str error);
void SetChartErrorProbe(ChartErrorProbe probe);

bool CarouselNonnegativeUsize(const shell::ComponentArgument& argument,
                              double* out);
bool CarouselNonemptyId(const shell::ComponentArgument& argument);

Entity<QuestionnaireState> QuestionnaireStateFor(Ctx* cx, Str id);

}

#line 1 "src/component_shell/input_group/mod.h"

namespace gpui::component_shell::input_group {

struct TextareaLayout {
    bool autoGrow = false;
    int rows = 0;
    int minRows = 0;
    int maxRows = 0;

    bool operator==(const TextareaLayout& other) const {
        return autoGrow == other.autoGrow && rows == other.rows &&
               minRows == other.minRows && maxRows == other.maxRows;
    }
    bool operator!=(const TextareaLayout& other) const {
        return !(*this == other);
    }
};

struct Op {
    enum Kind : uint8_t {
        Addon,
        Align,
        Size,
        Variant,
        Outline,
        Readonly,
        Invalid,
        FocusRing,
        Masked,
        Loading,
        Label,
        Icon,
        AriaLabel,
        AccessibilityId,
        Tooltip,
        Value,
        Placeholder,
        ContentType,
        Layout,
        OnChange,
    } kind = Addon;

    ComponentArgument argument = {};
    Str text;
    bool flag = false;
    UiSize size = UiSize::Medium;
    component::ButtonVariant variant = component::ButtonVariant::Default;
    component::InputGroupAddonAlignment align =
        component::InputGroupAddonAlignment::InlineStart;
    component::InputContentType contentType = component::InputContentType::Name;
    TextareaLayout layout = {};
};

}

namespace gpui::component_shell::input_group::binding {

bool RecordRows(PayloadBuild* build, const ComponentArgument* args, int count);
bool RecordAutoGrow(PayloadBuild* build, const ComponentArgument* args,
                    int count);

bool Rows(const ComponentArgument& argument, int* out, Str* error);

inline constexpr ArgumentDescriptor kRowsArguments[] = {
    {"rows", SchemaNumber()}};
inline constexpr ArgumentDescriptor kAutoGrowArguments[] = {
    {"min_rows", SchemaNumber()},
    {"max_rows", SchemaNumber()}};

inline constexpr MethodDescriptor kRowsMethod = {
    "rows", kRowsArguments,
    "Sets a fixed row count on TextareaState. Use h(...) for an explicit "
    "viewport height.",
    &RecordRows};
inline constexpr MethodDescriptor kAutoGrowMethod = {
    "auto_grow", kAutoGrowArguments,
    "Grows the native textarea between the minimum and maximum row counts.",
    &RecordAutoGrow};

struct Binding {
    InputState* state = nullptr;
    uint64_t handle = 0;

    Str elementId;
    bool textarea = false;
    shell::ComponentCallback change = {};
    bool hasValue = false;
    Str value;
    bool hasPlaceholder = false;
    Str placeholder;
    bool hasLayout = false;
    TextareaLayout layout = {};
    bool hasMasked = false;
    bool masked = false;
};

bool Prepare(MaterializeRequest* request, InputState* state, uint64_t handle,
             bool textarea, Binding* out);

void Apply(const Binding& binding, Ctx* cx);

}

namespace gpui::component_shell::input_group::content_type {

bool Record(PayloadBuild* build, const ComponentArgument* args, int count);

extern const char* const kLiterals[45];

inline constexpr ArgumentDescriptor kArguments[] = {
    {"content_type", SchemaEnum(kLiterals)}};

inline constexpr MethodDescriptor kMethod = {
    "content_type", kArguments,
    "Sets the native content type; names follow InputContentType in "
    "snake_case.",
    &Record};

}

#line 1 "src/shell/input_tokens.h"

namespace gpui::shell {

struct TextStateEntity {
    App* app = nullptr;
    InputState input;

    TextStateEntity() = default;
    TextStateEntity(const TextStateEntity&) = delete;
    TextStateEntity& operator=(const TextStateEntity&) = delete;
    ~TextStateEntity();
};

void TextStateInit(TextStateEntity* state, App* app, bool textarea,
                   Str placeholder, Str value);

ComponentDataValue InlineTokenContextData(Arena* a,
                                          const InlineTokenContext& token,
                                          Str text);
ComponentDataValue InlineTokenClickData(Arena* a,
                                        const InlineTokenClickEvent& event,
                                        Str text);

ComponentDataValue InlineTokenHoverData(Arena* a,
                                        const InlineTokenHoverEvent& event,
                                        Str text);

extern const StateMethodDescriptor kInputTokenStateMethods[8];
extern const StateMethodDescriptor kTextareaTokenStateMethods[8];

struct InlineTokenCallbacks {
    ShellRuntime* runtime = nullptr;
    InputState* state = nullptr;
    ComponentCallback renderer = {};
    ComponentCallback listener = {};
    ComponentCallback hoverListener = {};

    static InlineTokenCallbacks* New(Ctx* cx, ShellRuntime* runtime,
                                     InputState* state,
                                     ComponentCallback renderer,
                                     ComponentCallback listener);

    InlineTokenRenderer Renderer() const;
    InlineTokenClickListener Listener() const;

    InlineTokenCallbacks* WithHover(ComponentCallback hover);

    InlineTokenHoverListener HoverListener() const;
};

}

#line 1 "src/component_shell/input_tokens.h"

namespace gpui::component_shell::input_tokens {

struct Op {
    enum Kind : uint8_t {
        Render,
        Click,
        Hover,
        Change,
    } kind = Render;
    ComponentArgument argument = {};
};

bool RecordRender(PayloadBuild* build, const ComponentArgument* args, int);
bool RecordClick(PayloadBuild* build, const ComponentArgument* args, int);
bool RecordHover(PayloadBuild* build, const ComponentArgument* args, int);
bool RecordChange(PayloadBuild* build, const ComponentArgument* args, int);

inline constexpr ArgumentDescriptor kRenderArguments[] = {
    {"render", SchemaCallback("(token: InlineTokenContext, cx: Context) => "
                              "Element | null")}};
inline constexpr ArgumentDescriptor kClickArguments[] = {
    {"listener",
     SchemaCallback("(event: InlineTokenClickEvent, cx: Context) => void")}};
inline constexpr ArgumentDescriptor kHoverArguments[] = {
    {"listener",
     SchemaCallback("(event: InlineTokenHoverEvent, cx: Context) => void")}};
inline constexpr ArgumentDescriptor kChangeArguments[] = {
    {"listener", SchemaCallback("(text: string, cx: Context) => void")}};

inline constexpr MethodDescriptor kTokenMethod = {
    "token", kRenderArguments,
    "Renders an atomic token from its current UTF-16 range and read-only "
    "context.",
    &RecordRender};
inline constexpr MethodDescriptor kTokenClickMethod = {
    "on_token_click", kClickArguments,
    "Activates a reference after a completed unconsumed click, outside the "
    "editing borrow.",
    &RecordClick};
inline constexpr MethodDescriptor kTokenHoverMethod = {
    "on_token_hover", kHoverArguments,
    "Reports pointer presence over a token; hover never selects or edits.",
    &RecordHover};
inline constexpr MethodDescriptor kChangeMethod = {
    "on_change", kChangeArguments,
    "Reports user text or token identity changes; explicit draft restoration "
    "remains silent.",
    &RecordChange};

struct Binding {
    InputState* state = nullptr;

    uint64_t handle = 0;

    Str elementId;
    shell::InlineTokenCallbacks* callbacks = nullptr;
    shell::ComponentCallback change = {};
};

void SubscribeChange(Ctx* cx, InputState* state, uint64_t handle, Str key,
                     shell::ComponentEventRun run,
                     shell::ComponentCallback callback);

bool Prepare(MaterializeRequest* request, InputState* state, uint64_t handle,
             Binding* out);

void ApplyInput(const Binding& binding, component::Input* input);
void ApplyTextarea(const Binding& binding, component::Textarea* textarea);

El* Wrap(const Binding& binding, Ctx* cx, El* element);

}

#line 1 "src/component_shell/layout/mod.h"

namespace gpui::component_shell::layout::textarea {

bool RequireLeaf(int children, Str* error);

}

namespace gpui::component_shell::layout::resizable {

bool FinitePositive(double value, const char* label, float* out, Str* error);

bool RequireGroupStyle(bool styled, Str* error);

bool RequirePanelChild(const char* actual, Str* error);

}

#line 1 "src/component_shell/media/mod.h"

namespace gpui::component_shell::media::image {

bool AssetPath(Str path, Str* out, Str* error);

}

namespace gpui::component_shell::media::editor {

struct EditorStateValue {
    shell::TextStateEntity text;

    const char* language = nullptr;
};

bool RequireLeaf(int children, Str* error);

}

#line 1 "src/component_shell/navigation/mod.h"

namespace gpui::component_shell::navigation {

bool IconSize(Str value, UiSize* out, Str* error);

bool IconRotation(double value, float* out, Str* error);

bool IconPath(Str path, Str* error);

bool RequireRegisteredChild(const char* parent, const char* expected,
                            const char* actual, Str* error);

bool RequireDefaultItemStyle(bool styled, Str* error);

}

#line 1 "src/component_shell/retained_forms/mod.h"

namespace gpui::component_shell::retained_forms {

bool PositiveUsize(const ComponentArgument* args, int count,
                   const char* callable, uint64_t* out, Str* error);

bool EnsureLeaf(int childrenLen, const char* component, Str* error);

}

#line 1 "src/component_shell/scroll/mod.h"

namespace gpui::component_shell::scroll::scroll {

struct ScrollHandleState {
    float offsetX = 0;
    float offsetY = 0;

    int renders = 0;
    int barAt = -2;
    ScrollbarAxis barAxis = ScrollbarAxis::Vertical;
    bool hasBarAxis = false;
    ScrollbarMode barMode = ScrollbarMode::Scrolling;
    bool hasBarMode = false;

    static void OnScroll(ScrollHandleState* self, Ctx* cx,
                         const ScrollEvent* event);
};

struct Op {
    enum Kind : uint8_t {
        Axis,
        Mode,
        ViewportFromLayout,
    } kind = Axis;
    ScrollbarAxis axis = ScrollbarAxis::Vertical;
    ScrollbarMode mode = ScrollbarMode::Scrolling;
    bool flag = false;
};

struct ResolvedOps {
    bool hasAxis = false;
    ScrollbarAxis axis = ScrollbarAxis::Vertical;
    bool hasMode = false;
    ScrollbarMode mode = ScrollbarMode::Scrolling;
    bool viewportFromLayout = false;

    void Fold(const Op& op);
};

bool RequireLeaf(int children, bool styled, Str* error);

}

#line 1 "src/component_shell/settings/mod.h"

namespace gpui::component_shell::settings {

bool Positive(double value, const char* label, float* out, Str* error);

}

#line 1 "src/component_shell/structured/mod.h"

namespace gpui::component_shell::structured {

bool PositiveUsize(double value, Str label, uint64_t* out, Str* error);

bool PositiveU16(double value, Str label, uint16_t* out, Str* error);

bool NonnegativeF32(double value, Str label, float* out, Str* error);

struct ListOp {
    enum Kind : uint8_t {
        Vertical,
        Bordered,
        Columns,
        Size,
    } kind = Vertical;
    bool bordered = false;
    int columns = 0;
    UiSize size = UiSize::Medium;
};

struct ListConfig {
    bool vertical = false;
    bool bordered = true;
    int columns = 3;
    UiSize size = UiSize::Medium;

    void Apply(const ListOp& op);
};

bool RecordItemSpan(PayloadBuild* build, const ComponentArgument* args,
                    int count);
bool RecordListColumns(PayloadBuild* build, const ComponentArgument* args,
                       int count);

bool EnsureItemSurface(int children, bool styled, Str* error);

bool RecordFormColumns(PayloadBuild* build, const ComponentArgument* args,
                       int count);

struct TableHeaderPart {};
struct TableBodyPart {};
struct TableFooterPart {};
struct TableHeadPart {};
struct TableCellPart {};

bool RecordCellSpan(PayloadBuild* build, const ComponentArgument* args,
                    int count);

}

#line 1 "src/component_shell/typed_compound/mod.h"

namespace gpui::component_shell::typed_compound {

El* TypedChildElement(Ctx* cx, const void* tag, void* value, El* rendered);

void* TakeElement(MaterializeRequest* request, El* element, const void* tag,
                  const char* name);

bool IsTypedElement(El* element, const void* tag);

template <class T>
bool IsTypedElementOf(El* element) {
    return IsTypedElement(element, shell::PayloadTag<T>());
}

template <class T>
El* TypedChildElementOf(Ctx* cx, T* value) {
    return TypedChildElement(cx, shell::PayloadTag<T>(), value,
                             value->IntoEl());
}

template <class T>
T* TakeElementAs(MaterializeRequest* request, El* element, const char* name) {
    return (T*)TakeElement(request, element, shell::PayloadTag<T>(), name);
}

template <class T>
El* FinishPart(MaterializeRequest* request, T* element) {
    element->refiner = request->TakeStyle();
    El** children = nullptr;
    int count = 0;
    if (!request->TakeChildren(&children, &count)) return nullptr;
    for (int i = 0; i < count; i++) {
        if (children[i]) element->Child(children[i]);
    }
    return TypedChildElementOf(request->cx, element);
}

template <class T>
El* FinishTypedChildren(MaterializeRequest* request, T* element,
                        const char* parent, Slice<const char*> allowed) {
    shell::ComponentChild* children = nullptr;
    int count = 0;
    if (!request->TakeTypedChildren(&children, &count)) return nullptr;
    for (int i = 0; i < count; i++) {
        if (!RequireChild(request, parent, children[i].componentName, allowed))
            return nullptr;
    }
    for (int i = 0; i < count; i++) {
        El* child = request->MaterializeChild(&children[i]);
        if (!child) return nullptr;
        element->Child(child);
    }
    element->refiner = request->TakeStyle();
    return TypedChildElementOf(request->cx, element);
}

bool IndexCallbackPayload(PayloadBuild* build, const ComponentArgument* args,
                          int count, const char* component);

bool RadioGroupOnChangeCallback(const ComponentPayload& payload,
                                shell::CallbackId* callback);

}

#line 1 "src/fps/fps.h"

namespace gpui {

struct FpsStyle {
    Rgba background;
    Rgba foreground;
    Rgba muted;
    Rgba good;
    Rgba warn;
    Rgba bad;
};

const FpsStyle& FpsStyleDark();

Rgba FpsLevelColor(const FpsStyle& style, float frameSecs, float budgetSecs);

enum : uint16_t {

    kFpsCapacity = 120,

    kFpsPresents = 512,

    kResourceHistoryCap = 32,

    kFpsWarmupFrames = 8,
};

struct FrameSample {

    float drawSecs = 0;

    uint64_t invalidations = 0;
};

struct FrameSampler {
    FrameSample samples[kFpsCapacity] = {};
    int n = 0;
    int capacity = kFpsCapacity;

    double presents[kFpsPresents] = {};
    int nPresents = 0;
    uint64_t cursor = 0;

    uint32_t warmup = kFpsWarmupFrames;

    bool drainedBacklog = false;

    double ownFrameAt = -1;
    uint64_t ownFrameNotifies = 0;
};

void FrameSamplerTick(FrameSampler* s, Window* win);
void FrameSamplerExpectOwnFrame(FrameSampler* s, double at);

void FrameSamplerReset(FrameSampler* s);

void FrameSamplerIngest(FrameSampler* s, const FrameTiming* frames, int n,
                        double now);

void FrameSamplerIngestDraws(FrameSampler* s, const FrameSample* samples,
                             int n);

void FrameSamplerIngestPresents(FrameSampler* s, const double* presentAt, int n,
                                double now);
void FrameSamplerSetCapacity(FrameSampler* s, int capacity);

float FrameSamplerFps(const FrameSampler* s);

float FrameSamplerPresentInterval(const FrameSampler* s);

float FpsSustainableRate(float meanDrawSecs, double displayPeriod);
float FrameSamplerMeanDraw(const FrameSampler* s);

float FrameSamplerPercentileDraw(const FrameSampler* s, float percentile);

float FrameSamplerMeanInvalidations(const FrameSampler* s);

float FrameSamplerPeakDraw(const FrameSampler* s);

float FrameSamplerOverBudget(const FrameSampler* s, float budgetSecs);

struct ResourceSample {

    float cpuPercent = 0;

    uint64_t memoryBytes = 0;

    float gpuPercent = -1.f;
};

struct ResourceHistory {
    double at[kResourceHistoryCap] = {};
    ResourceSample samples[kResourceHistoryCap] = {};
    int n = 0;

    float windowSecs = 3.f;
};

void ResourceHistoryPush(ResourceHistory* h, ResourceSample sample, double now);

bool ResourceHistoryMean(const ResourceHistory* h, ResourceSample* out);

struct ResourceProbe {
    uint64_t prevCpu100ns = 0;
    double prevAt = 0;
    bool primed = false;
    ResourceHistory history;
};

bool ResourceProbeSample(ResourceProbe* probe, ResourceSample* out);

struct FpsReadout {

    float maxFps = 0;

    float fps = 0;

    float intervalMillis = 0;

    float frameMillis = 0;

    float percentileMillis = 0;
    float droppedPercent = 0;

    float invalidations = 0;
};

struct FpsResourceJob;

enum class FpsHeadline : uint8_t {

    Max,

    Observed,
};

struct FpsMonitor {
    FrameSampler sampler;
    FpsReadout readout;
    double readoutAt = -1;

    float frameBudget = 1.f / 60.f;
    bool budgetExplicit = false;
    FpsHeadline headline = FpsHeadline::Max;

    bool displayAsked = false;
    uint64_t display = 0;
    double displayPeriod = 0;
    bool showResources = true;
    float resourceInterval = 0.5f;
    ResourceProbe probe;
    ResourceSample resources;
    bool hasResources = false;

    Window* clockWindow = nullptr;
    int clockTimer = 0;
    uint64_t framesRendered = 0;
    uint64_t clockSeenFrames = 0;
    uint32_t clockStillTicks = 0;
    bool sleeping = false;
    int resourceTask = 0;
    uint64_t resourceGeneration = 0;
    FpsResourceJob* resourceJob = nullptr;
    bool compact = false;

    float axisMax = (1.f / 60.f) * 2.f;

    ~FpsMonitor();
    static El* Render(FpsMonitor* self, Ctx* cx);
    static void OnToggleCompact(FpsMonitor* self, Ctx* cx, const ClickEvent*);

    static void OnToggleHeadline(FpsMonitor* self, Ctx* cx,
                                 const MouseDownEvent* event);
    static void OnClockTick(FpsMonitor* self, Ctx* cx, const TickEvent*);
};

void FpsMonitorSetFrameBudget(FpsMonitor* self, float budgetSecs);

void FpsMonitorAdoptDisplayPeriod(FpsMonitor* self, double periodSecs);

bool FpsRenderWatchTick(uint64_t framesRendered, uint64_t* seen,
                        uint32_t* stillTicks);

TempStr FpsFormatCpuTemp(float percent);

TempStr FpsFormatBytesTemp(uint64_t bytes);

enum class FpsAnchor : uint8_t {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    TopCenter,
    BottomCenter,
    LeftCenter,
    RightCenter,
};

struct FpsOverlayOpts {

    FpsAnchor anchor = FpsAnchor::TopRight;

    float frameBudget = 0;
};

El* FpsOverlayEl(Ctx* cx, Entity<FpsMonitor> monitor,
                 FpsOverlayOpts opts = FpsOverlayOpts{});

El* FpsMonitorEl(Ctx* cx, FpsOverlayOpts opts = FpsOverlayOpts{});

}

#line 1 "src/gpui/accessibility_linux.h"

namespace gpui {

#if GPUI_OS_LINUX

void AccessibilityLinuxInit(App* app, Str busAddress);
void AccessibilityLinuxShutdown();
int AccessibilityLinuxFd();
void AccessibilityLinuxPump();
void AccessibilityLinuxTreeChanged(Window* win);
void AccessibilityLinuxFocusChanged(Window* win, int focusId);

Point AccessibilityLinuxWindowOrigin(Window* win);
#endif

}

#line 1 "src/gpui/accessibility_win.h"

namespace gpui {

#if GPUI_OS_WINDOWS
struct WinAccessibility;

WinAccessibility* AccessibilityWinNew(Window* win, void* hwnd);
void AccessibilityWinClose(WinAccessibility* accessibility);
intptr_t AccessibilityWinGetObject(WinAccessibility* accessibility,
                                   uintptr_t wParam, intptr_t lParam);
void AccessibilityWinTreeChanged(WinAccessibility* accessibility);
void AccessibilityWinFocusChanged(WinAccessibility* accessibility, int focusId);
#endif

}

#line 1 "src/gpui/asset_icons.h"

namespace gpui {

struct AssetIcon {
    int offset;
    int len;
};

extern const uint8_t kAssetIconsData[];
extern const int kAssetIconsDataLen;

extern const char kAssetIconNames[];
extern const AssetIcon kAssetIcons[];
extern const int kAssetIconsCount;

const uint8_t* AssetIconFind(Str name, int* lenOut);

const uint8_t* AssetIconForPath(Str assetPath, int* lenOut);

}

#line 1 "src/gpui/image.h"

namespace gpui {

struct RenderImage;
struct ImageStore;

struct ImageLookup {
    App* app = nullptr;
    Window* win = nullptr;
    PaintApp* pa = nullptr;
    EntityId cache = {};

    static ImageLookup Of(PaintApp* pa) {
        ImageLookup l;
        l.pa = pa;
        return l;
    }
    static ImageLookup Of(PaintCtx* ctx, EntityId cache = {}) {
        ImageLookup l;
        if (ctx) {
            l.app = ctx->app;
            l.win = ctx->window;
            l.pa = ctx->pa;
        }
        l.cache = cache;
        return l;
    }
};

struct ImageCache {
    ImageStore* store = nullptr;

    ImageCache();
    ImageCache(const ImageCache&) = delete;
    ImageCache& operator=(const ImageCache&) = delete;
    ~ImageCache();

    void Clear();
    void Remove(Str src);
    int Len() const;
    bool Empty() const { return Len() == 0; }
};

bool ImageDropEncoded(App* app, const uint8_t* bytes, int len);

ImageStore* ImageStoreNew();
void ImageStoreFree(ImageStore* s);

RenderImage* ImageForSrc(const ImageLookup& cx, Str src);
RenderImage* ImageForSrc(PaintApp* pa, Str src);
RenderImage* ImageForSource(const ImageLookup& cx, const ImageSource& source);
RenderImage* ImageForSource(PaintApp* pa, const ImageSource& source);

ImageLoadState ImageSrcState(const ImageLookup& cx, Str src,
                             double* loadingSeconds = nullptr);
ImageLoadState ImageSrcState(PaintApp* pa, Str src,
                             double* loadingSeconds = nullptr);
ImageLoadState ImageSourceState(const ImageLookup& cx,
                                const ImageSource& source,
                                double* loadingSeconds = nullptr);
ImageLoadState ImageSourceState(PaintApp* pa, const ImageSource& source,
                                double* loadingSeconds = nullptr);

int ImageFrameIndex(RenderImage* image, bool reducedMotion,
                    bool* wantsAnimation);

const uint8_t* ImageVectorForSrc(const ImageLookup& cx, Str src, int* lenOut);
const uint8_t* ImageVectorForSrc(Str src, int* lenOut);
const uint8_t* ImageVectorForSource(const ImageLookup& cx,
                                    const ImageSource& source, int* lenOut);
const uint8_t* ImageVectorForSource(PaintApp* pa, const ImageSource& source,
                                    int* lenOut);

bool ImageSrcIsLocal(Str src);

Str ImageAssetFor(Arena* a, Str src);

void ImageCacheClear();
void ImageCacheClear(App* app);

int ImageCacheResourceCount(App* app = nullptr);
int ImageCacheEncodedCount(App* app = nullptr);

}

#line 1 "src/gpui/paint.h"

#include <math.h>

#if GPUI_OS_WINDOWS
#ifndef WIN_BACKEND_ALL
#define WIN_BACKEND_ALL 0
#endif
#if WIN_BACKEND_ALL
#undef WIN_BACKEND_DIRECT2D
#undef WIN_BACKEND_D3D11
#undef WIN_BACKEND_D3D12
#define WIN_BACKEND_DIRECT2D 1
#define WIN_BACKEND_D3D11 1
#define WIN_BACKEND_D3D12 1
#else
#if !defined(WIN_BACKEND_DIRECT2D) && !defined(WIN_BACKEND_D3D11) && \
    !defined(WIN_BACKEND_D3D12)
#define WIN_BACKEND_DIRECT2D 1
#endif
#ifndef WIN_BACKEND_DIRECT2D
#define WIN_BACKEND_DIRECT2D 0
#endif
#ifndef WIN_BACKEND_D3D11
#define WIN_BACKEND_D3D11 0
#endif
#ifndef WIN_BACKEND_D3D12
#define WIN_BACKEND_D3D12 0
#endif
#if WIN_BACKEND_DIRECT2D + WIN_BACKEND_D3D11 + WIN_BACKEND_D3D12 != 1
#error Define exactly one WIN_BACKEND_DIRECT2D, WIN_BACKEND_D3D11 or WIN_BACKEND_D3D12, or define WIN_BACKEND_ALL
#endif
#endif
#else
#undef WIN_BACKEND_ALL
#undef WIN_BACKEND_DIRECT2D
#undef WIN_BACKEND_D3D11
#undef WIN_BACKEND_D3D12
#define WIN_BACKEND_ALL 0
#define WIN_BACKEND_DIRECT2D 0
#define WIN_BACKEND_D3D11 0
#define WIN_BACKEND_D3D12 0
#endif

#define WIN_BACKEND_GPU (WIN_BACKEND_D3D11 || WIN_BACKEND_D3D12)

namespace gpui {

#if GPUI_OS_WINDOWS
enum class WinPaintBackend : uint8_t {
    Direct2D,
    D3D11,
    D3D12,
};

enum class WinPaintMsaa : uint8_t {
    X1 = 1,
    X2 = 2,
    X4 = 4,
    X8 = 8,
};

enum class WinSceneMode : uint8_t {
    Off,
    Replay,
    Cache,
    Skip,
    Damage,
};

struct WinPaintOptions {
    WinPaintBackend backend = WinPaintBackend::Direct2D;
    WinPaintMsaa msaa = WinPaintMsaa::X4;
    WinSceneMode scene = WinSceneMode::Skip;

    int gpuResetEvery = 0;
};

const WinPaintOptions& WinPaintOptionsGet();
bool WinPaintOptionsTakeArg(Str arg);
#endif

enum : uint8_t {
    kFontWeightMask = 15,
    kFontWeightNormal = 0,
    kFontWeightThin = 1,
    kFontWeightExtraLight = 2,
    kFontWeightLight = 3,
    kFontWeightExplicitNormal = 4,
    kFontWeightMedium = 5,
    kFontWeightSemibold = 6,
    kFontWeightBold = 7,
    kFontWeightExtraBold = 8,
    kFontWeightBlack = 9,
    kFontMono = 16,
    kFontUnderline = 32,
    kFontItalic = 64,

    kFontStrike = 128
};

enum : uint16_t {
    kFontTabularNums = 256
};

enum : uint16_t {
    kFontFamilyShift = 9,
    kFontFamilyMask = 0xFE00
};

uint8_t FontFamilyIntern(Str name);

Str FontFamilyName(uint8_t id);
inline uint16_t FontFamilyBits(uint8_t id) {
    return (uint16_t)((uint16_t)(id & 0x7F) << kFontFamilyShift);
}
inline uint8_t FontFamilyOf(uint16_t weight) {
    return (uint8_t)((weight & kFontFamilyMask) >> kFontFamilyShift);
}

const float kLineHeight = 1.618034f;

PaintApp* PaintAppNew();
void PaintAppFree(PaintApp* pa);

const Str* PaintInstalledFontNames(PaintApp* pa, int* n);

Str PaintSystemUIFontMappedFamily();

bool PaintTargetBegin(PaintCtx* ctx, void* native, int pxW, int pxH);

bool PaintTargetBeginOffscreen(PaintCtx* ctx, int pxW, int pxH);

bool PaintTargetEndOffscreen(PaintCtx* ctx, uint8_t* outBgra);

bool PaintTargetEnd(PaintCtx* ctx);

void PaintTargetFree(PaintCtx* ctx);

inline Rgba PaintFade(const PaintCtx* ctx, Rgba c) {
    if (!ctx || ctx->opacity >= 1.f) {
        return c;
    }
    float a = (float)c.a * (ctx->opacity < 0 ? 0 : ctx->opacity);
    c.a = (uint8_t)(a <= 0 ? 0 : (a >= 255 ? 255 : lroundf(a)));
    return c;
}

void CanvasClear(PaintCtx* ctx, Rgba c);
void CanvasFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c);
void CanvasFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                     Rgba c);
inline float ClampRadius(float r, float w, float h) {
    float lim = (w < h ? w : h) * 0.5f;
    if (lim < 0) lim = 0;
    return r > lim ? lim : r;
}
inline void FillRound(PaintCtx* ctx, float x, float y, float w, float h,
                      float r, Rgba c) {
    CanvasFillRound(ctx, x, y, w, h, ClampRadius(r, w, h), c);
}

void CanvasStrokeRound(PaintCtx* ctx, float x, float y, float w, float h,
                       float r, float stroke, Rgba c,
                       const float* dash = nullptr);
inline void DrawRoundStroke(PaintCtx* ctx, float x, float y, float w, float h,
                            float r, float stroke, Rgba c) {
    CanvasStrokeRound(ctx, x, y, w, h, ClampRadius(r, w, h), stroke, c);
}
void CanvasLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                float stroke, Rgba c, const float* dash = nullptr);

void CanvasEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                   float stroke, Rgba c);

void CanvasEllipseGradient(PaintCtx* ctx, float cx, float cy, float rx,
                           float ry, float x0, float y0, float x1, float y1,
                           Rgba from, Rgba to);
void CanvasPushClip(PaintCtx* ctx, float x, float y, float w, float h);
void CanvasPopClip(PaintCtx* ctx);

struct Path;

Path* PathNew(PaintCtx* ctx, bool winding);
void PathFree(Path* p);
void PathMoveTo(Path* p, float x, float y);
void PathLineTo(Path* p, float x, float y);
void PathCubicTo(Path* p, float x1, float y1, float x2, float y2, float x,
                 float y);

void PathArcTo(Path* p, float cx, float cy, float r, float a0, float a1,
               bool clockwise);
void PathClose(Path* p);

void PathFill(PaintCtx* ctx, Path* p, Rgba c, float dx = 0, float dy = 0);

void PathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                      float y1, Rgba from, Rgba to, float dx = 0, float dy = 0);
void PathFillGradientV(PaintCtx* ctx, Path* p, float y0, float y1, Rgba top,
                       Rgba bot);
void PathStroke(PaintCtx* ctx, Path* p, float stroke, Rgba c,
                bool roundCaps = false, float dx = 0, float dy = 0);

void PathStrokeGradient(PaintCtx* ctx, Path* p, float stroke, float x0,
                        float y0, float x1, float y1, Rgba from, Rgba to,
                        bool roundCaps = false, float dx = 0, float dy = 0);

void PathRealize(PaintCtx* ctx, Path* p);

struct RenderImage;

enum class RenderImageStatus : uint8_t {
    Loading,
    Ready,
    Failed,
};

RenderImage* RenderImageDecode(PaintApp* pa, const uint8_t* bytes, int len);

RenderImage* RenderImageFromBgra(PaintApp* pa, const uint8_t* bgra, int w,
                                 int h);

RenderImage* RenderImageNewLoading();
void RenderImageComplete(RenderImage* img, RenderImage* decoded);

void RenderImageRetain(RenderImage* img);
void RenderImageRelease(RenderImage* img);

uint64_t RenderImageGeneration(const RenderImage* img);
RenderImageStatus RenderImageStatusGet(const RenderImage* img);

Size RenderImageSizePx(const RenderImage* img, int frameIndex = 0);
int RenderImageFrameCount(const RenderImage* img);
int RenderImageFrameDurationMs(const RenderImage* img, int frameIndex);

void RenderImageDraw(PaintCtx* ctx, RenderImage* img, Bounds bounds,
                     Bounds imageBounds, int frameIndex, float radius = 0,
                     bool grayscale = false);
inline void RenderImageDraw(PaintCtx* ctx, RenderImage* img, Bounds bounds,
                            float radius = 0) {
    RenderImageDraw(ctx, img, bounds, bounds, 0, radius, false);
}

struct TextLayout;

TextLayout* TextLayoutNew(PaintCtx* ctx, Str s, float fontSize, float maxW,
                          bool wrap, uint16_t weight, float lineH,
                          Size* outSize, TextAlign align = TextAlign::Left);

Size TextLayoutSize(TextLayout* tl);
void TextLayoutAddRef(TextLayout* tl);
void TextLayoutRelease(TextLayout* tl);
uint64_t TextLayoutGeneration(const TextLayout* tl);

int TextLayoutTextLen(const TextLayout* tl);

void TextLayoutDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                    bool clip, float clipW = 0);

bool PaintTextLayoutSpans(PaintCtx* ctx, TextLayout* tl, Str text, float x,
                          float y, Rgba base, const TextSpan* spans, int n);

void TextLayoutDrawSpans(PaintCtx* ctx, TextLayout* tl, Str text, float x,
                         float y, Rgba base, const TextSpan* spans, int n);
void DrawTextAt(PaintCtx* ctx, Str s, float x, float y, float w, float h,
                float fontSize, Rgba c, bool truncate, bool wrap = false,
                float measMaxW = -1.f, int weight = 0, float lineH = 0);

int TextLayoutHitPoint(TextLayout* tl, Str s, float relX, float relY);

int TextLayoutRangeRects(TextLayout* tl, Str s, int u8a, int u8b, Bounds* out,
                         int max);

float TextLayoutBaseline(TextLayout* tl);

}

#line 1 "src/gpui/paintgpu.h"

#if GPUI_OS_WINDOWS

namespace gpui {

bool PaintGpuOn();

bool PaintD3d12On();

int PaintGpuSamples();

void* PaintSharedD3dDevice(PaintApp* pa);

void* PaintSharedDxgiFactory(PaintApp* pa);

void PaintSharedD3dDeviceReset(PaintApp* pa);

void* PaintSharedDwrite(PaintApp* pa);

bool PaintImagePixels(const RenderImage* img, const uint8_t** bgra, int* w,
                      int* h, int frameIndex);

void* PaintTextLayoutNative(TextLayout* tl);

namespace gpuw {

void PaintAppFree();
bool PaintTargetBegin(PaintCtx* ctx, void* native, int pxW, int pxH);
bool PaintTargetBeginOffscreen(PaintCtx* ctx, int pxW, int pxH);
bool PaintTargetEndOffscreen(PaintCtx* ctx, uint8_t* outBgra);
bool PaintTargetEnd(PaintCtx* ctx);
void PaintTargetFree(PaintCtx* ctx);

void CanvasClear(PaintCtx* ctx, Rgba c);
void CanvasFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c);
void CanvasFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                     Rgba c);
void CanvasStrokeRound(PaintCtx* ctx, float x, float y, float w, float h,
                       float r, float stroke, Rgba c, const float* dash);
void CanvasLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                float stroke, Rgba c, const float* dash);
void CanvasEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                   float stroke, Rgba c);
void CanvasPushClip(PaintCtx* ctx, float x, float y, float w, float h);
void CanvasPopClip(PaintCtx* ctx);

Path* PathNew(PaintCtx* ctx, bool winding);
void PathFree(Path* p);
void PathMoveTo(Path* p, float x, float y);
void PathLineTo(Path* p, float x, float y);
void PathCubicTo(Path* p, float x1, float y1, float x2, float y2, float x,
                 float y);
void PathArcTo(Path* p, float cx, float cy, float r, float a0, float a1,
               bool clockwise);
void PathClose(Path* p);
void PathFill(PaintCtx* ctx, Path* p, Rgba c, float dx, float dy);
void PathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                      float y1, Rgba from, Rgba to, float dx, float dy);
void PathStroke(PaintCtx* ctx, Path* p, float stroke, Rgba c, bool roundCaps,
                float dx, float dy);
void PathStrokeGradient(PaintCtx* ctx, Path* p, float stroke, float x0,
                        float y0, float x1, float y1, Rgba from, Rgba to,
                        bool roundCaps, float dx, float dy);
void PathRealize(PaintCtx* ctx, Path* p);

void RenderImageDraw(PaintCtx* ctx, RenderImage* img, Bounds bounds,
                     Bounds imageBounds, int frameIndex, float radius,
                     bool grayscale);

void RenderImageFree(uint64_t imageGeneration);
int RenderImageCacheCountForTest(uint64_t imageGeneration);
void TextLayoutDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                    bool clip, float clipW);

struct FrameStats {
    int instances = 0;
    int draws = 0;
    int glyphsRasterized = 0;
    int pathTriangles = 0;
};
const FrameStats& LastFrameStats();

}

}

#endif

#line 1 "src/gpui/platform.h"

namespace gpui {

void WindowDrawFrame(Window* win, void* native, int pxW, int pxH, float dipW,
                     float dipH);

void WindowDrawFrameHeadless(Window* win, float dipW, float dipH);

bool PlatReduceMotion();

#if GPUI_OS_IOS || GPUI_OS_ANDROID
inline bool PlatReduceMotionKnown(bool* out) {
    if (out) {
        *out = PlatReduceMotion();
    }
    return true;
}
inline void PlatReduceMotionFollow(void (*onChange)(bool reduce)) {
    (void)onChange;
}
#else
bool PlatReduceMotionKnown(bool* out);
void PlatReduceMotionFollow(void (*onChange)(bool reduce));
#endif

struct KeyDownFlags {
    bool held = false;
    bool preferCharacterInput = false;
    bool imeInProgress = false;
};
bool WindowKeyDown(Window* win, int key, bool shift, bool ctrl, bool alt,
                   bool platform = false, bool function = false,
                   KeyDownFlags flags = {});

void WindowKeyUp(Window* win, int key, bool shift, bool ctrl, bool alt,
                 bool platform = false, bool function = false);

void WindowChar(Window* win, uint32_t ch, bool ctrl, bool alt);

void WindowDispatchInput(Window* win, const PlatformInput* input);

PlatformInput InputMouseDown(MouseButton button, float x, float y,
                             Modifiers modifiers, int clickCount,
                             bool firstMouse);
PlatformInput InputMouseUp(MouseButton button, float x, float y,
                           Modifiers modifiers, int clickCount);
PlatformInput InputMouseMove(float x, float y, bool pressed,
                             MouseButton pressedButton, Modifiers modifiers);
PlatformInput InputMouseExited(float x, float y, bool pressed,
                               MouseButton pressedButton, Modifiers modifiers);
PlatformInput InputScrollWheel(float x, float y, float deltaX, float deltaY,
                               bool precise, Modifiers modifiers,
                               TouchPhase phase);
PlatformInput InputTouchDrag(TouchPhase phase, Point start, Point position);
PlatformInput InputLongPress(TouchPhase phase, Point start, Point position);

void WindowTouchBegin(Window* win, float x, float y);
void WindowTouchMove(Window* win, float x, float y);
void WindowTouchEnd(Window* win, float x, float y);
void WindowTouchCancel(Window* win);
void WindowTouchPoll(Window* win, double now);

bool WindowScrollApply(Window* win, float x, float y, float dx, float dy);

int WindowClickCount(Window* win, float x, float y, MouseButton button);

int WindowCurrentClickCount(Window* win);

void WindowTimerTick(Window* win);

int WindowChromeHit(Window* win, float x, float y);

int WindowTimerMs(Window* win);

void WindowClosed(Window* win);
bool WindowShouldClose(Window* win);
bool AppAnyWindowOpen(App* app);

Window* WindowAlloc(App* app, WinOpts opts);

App* AppNewHeadless();
void AppFreeHeadless(App* app);

void WindowClampToDisplay(int* dipW, int* dipH, int screenW, int screenH);

bool WindowGeomRequested(int* x, int* y, int* w, int* h);

int GpuiTakeRuntimeArgs(int argc, char** argv);

bool PlatInit(App* app);
void PlatShutdown(App* app);

void PlatSetTimer(Window* win, int ms);

void PlatWake(App* app);

struct PlatPollFd {
    int fd;
    uint16_t events;
    uint16_t revents;
};
using PlatLoopPrepare = int (*)(PlatPollFd** fds, int* timeoutMs);
using PlatLoopDispatch = void (*)();
#if GPUI_OS_LINUX
void PlatAddLoopSource(PlatLoopPrepare prepare, PlatLoopDispatch dispatch);
#else
inline void PlatAddLoopSource(PlatLoopPrepare prepare,
                              PlatLoopDispatch dispatch) {
    (void)prepare, (void)dispatch;
}
#endif

void PlatSetCursor(Window* win, CursorKind kind);

void PlatSetMouseCapture(Window* win, bool capture);

int PlatDoubleClickMs();

uint64_t PlatWindowDisplay(Window* win);

double PlatDisplayRefreshPeriod(uint64_t display);

void* PlatWindowHandle(Window* win);

void PlatInstallAccessibilityHitTest(Window* win);

void PlatAccessibilityTreeChanged(Window* win);
void PlatAccessibilityFocusChanged(Window* win, int focusId);

struct PlatMenuItem {
    const char* label = nullptr;

    int id = 0;
    bool separator = false;
    bool disabled = false;
    bool checked = false;

    const char* iconPath = nullptr;

    const char* iconSvg = nullptr;
    int iconSvgLen = 0;
    const PlatMenuItem* submenu = nullptr;
    int submenuN = 0;

    const char* key = nullptr;
    Modifiers keyMods = {};
};

#if GPUI_OS_IOS || GPUI_OS_ANDROID
inline bool PlatHasMenu() {
    return false;
}
inline int PlatShowMenu(Window* win, const PlatMenuItem* items, int n, float x,
                        float y, bool dark) {
    (void)win, (void)items, (void)n, (void)x, (void)y, (void)dark;
    return 0;
}
#else
bool PlatHasMenu();
int PlatShowMenu(Window* win, const PlatMenuItem* items, int n, float x,
                 float y, bool dark);
#endif

bool PlatHasAppMenu();

void PlatSetAppMenu(App* app, const PlatMenuItem* items, int n);

void PlatSetMenuBarVisible(bool visible);

void AppMenuChosen(int id);

}

#line 1 "src/gpui/scene.h"

namespace gpui {

enum SceneLevel : uint8_t {
    kSceneOff = 0,
    kSceneReplay = 1,
    kSceneCache = 2,
    kSceneSkip = 3,
    kSceneDamage = 4
};

int SceneLevelOn();
inline bool SceneOn() {
    return SceneLevelOn() > kSceneOff;
}

bool SceneTakeArg(Str arg);

namespace scene {

struct State;

void Free(PaintCtx* ctx);

bool Recording();

int ContextPush(PaintCtx* ctx, int z);
void ContextPop(PaintCtx* ctx, int parent);
int CurrentContext(PaintCtx* ctx);

void FrameBegin(PaintCtx* ctx);

bool FrameEnd(PaintCtx* ctx, Bounds* damage);

void Replay(PaintCtx* ctx, const Bounds* damage);

bool SuspendBegin();
void SuspendEnd(bool prev);

bool SkipPresent(PaintCtx* ctx);

void Invalidate(PaintCtx* ctx);

void Reset(PaintCtx* ctx);

bool RecTextDrawSpans(PaintCtx* ctx, TextLayout* tl, Str text, float x, float y,
                      Rgba base, const TextSpan* spans, int n);
void RecClear(PaintCtx* ctx, Rgba c);
void RecFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c);
void RecFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                  Rgba c);
void RecStrokeRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                    float stroke, Rgba c, const float* dash);
void RecLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
             float stroke, Rgba c, const float* dash);
void RecEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                float stroke, Rgba c);
void RecPushClip(PaintCtx* ctx, float x, float y, float w, float h);
void RecPopClip(PaintCtx* ctx);

Path* RecPathNew(PaintCtx* ctx, bool winding);
void RecPathFree(Path* p);
void RecPathMoveTo(Path* p, float x, float y);
void RecPathLineTo(Path* p, float x, float y);
void RecPathCubicTo(Path* p, float x1, float y1, float x2, float y2, float x,
                    float y);
void RecPathArcTo(Path* p, float cx, float cy, float r, float a0, float a1,
                  bool clockwise);
void RecPathClose(Path* p);
void RecPathFill(PaintCtx* ctx, Path* p, Rgba c);
void RecPathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                         float y1, Rgba from, Rgba to);
void RecPathStroke(PaintCtx* ctx, Path* p, float stroke, Rgba c,
                   bool roundCaps);
void RecPathStrokeGradient(PaintCtx* ctx, Path* p, float stroke, float x0,
                           float y0, float x1, float y1, Rgba from, Rgba to,
                           bool roundCaps);

void RecImageDraw(PaintCtx* ctx, RenderImage* img, Bounds bounds,
                  Bounds imageBounds, int frameIndex, float radius,
                  bool grayscale);
inline void RecImageDraw(PaintCtx* ctx, RenderImage* img, Bounds bounds,
                         float radius) {
    RecImageDraw(ctx, img, bounds, bounds, 0, radius, false);
}
void RecTextDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                 bool clip, float clipW);

struct SceneStats {

    int prims = 0;
    int contexts = 0;
    int layers = 0;

    int maskChanges = 0;

    int culled = 0;

    int clipPushes = 0;
    int pathPrims = 0;
    int pathVerbs = 0;

    int pathCacheHits = 0;
    int pathCacheMisses = 0;

    int framePathCacheHits = 0;
    int framePathCacheMisses = 0;
    float framePathBuildMs = 0;
    int pathCacheLive = 0;
    int maskCacheHits = 0;
    int maskCacheMisses = 0;
    int maskCacheLive = 0;

    int frames = 0;
    int framesUnchanged = 0;
    int framesPartial = 0;

    float damageFracSum = 0;

    int primsChanged = 0;

    float damageFraction = 1;
};
const SceneStats& Stats(PaintCtx* ctx);

}

}

#line 1 "src/gpui/test_app.h"

namespace gpui {

App* TestAppNew();
void TestAppFree(App* app);

Window* TestWindowOpen(App* app, EntityId root, float w = 1920, float h = 1080,
                       float scale = 2);
template <typename T>
Window* TestWindowOpen(App* app, Entity<T> root, float w = 1920, float h = 1080,
                       float scale = 2) {
    return TestWindowOpen(app, root.id, w, h, scale);
}

void TestDraw(Window* win);

void TestFlushEffects(App* app);

void TestSimulateResize(Window* win, float w, float h);

void TestRunUntilParked(App* app);

void TestAdvanceClock(App* app, double ms);

double TestClockNow();

void TestSimulateKeystrokes(Window* win, const char* keystrokes);

void TestSimulateInput(Window* win, Str text);

void TestSimulateMouseMove(Window* win, Point position, bool pressed = false,
                           MouseButton button = MouseButton::Left,
                           Modifiers modifiers = {});
void TestSimulateMouseDown(Window* win, Point position,
                           MouseButton button = MouseButton::Left,
                           Modifiers modifiers = {});
void TestSimulateMouseUp(Window* win, Point position,
                         MouseButton button = MouseButton::Left,
                         Modifiers modifiers = {});

void TestSimulateClick(Window* win, Point position, Modifiers modifiers = {});

void TestSimulateScrollWheel(Window* win, Point position, Point delta,
                             Modifiers modifiers = {});

bool TestDispatchAction(Window* win, uint32_t action, int64_t arg = 0);

void TestWriteToClipboard(Str text);

Str TestReadFromClipboard(Arena* a);

void TestFocus(Window* win, FocusHandle handle);

}

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/html5ever/html5ever.h"

namespace html5ever {

using base::Arena;
using base::ArenaPtr;
using base::ArenaPtrGet;
using base::ArenaStr;
using base::ArenaStrGet;
using base::ArenaVec;
using base::Str;

enum class Namespace : uint8_t {
    Html,
    MathMl,
    Svg,
};

enum class NodeKind : uint8_t {
    Document,
    Doctype,
    Text,
    Comment,
    Element,
};

struct Attribute {
    ArenaStr name = {};
    ArenaStr value = {};
    Namespace ns = Namespace::Html;
    ArenaPtr<Attribute> next = {};
};

struct Node {
    NodeKind kind = NodeKind::Document;
    Namespace ns = Namespace::Html;
    ArenaStr name = {};

    ArenaStr data = {};

    ArenaStr systemId = {};
    ArenaPtr<Attribute> attrs = {};
    ArenaPtr<Node> parent = {};
    ArenaPtr<Node> first = {};
    ArenaPtr<Node> last = {};
    ArenaPtr<Node> next = {};

    bool implicit = false;
};

enum class TokenKind : uint8_t {
    Eof,
    ParseError,
    Character,
    NullCharacter,
    Comment,
    Doctype,
    StartTag,
    EndTag,
};

struct Token {
    int line = 1;
    ArenaStr name = {};
    ArenaStr data = {};
    ArenaStr systemId = {};
    ArenaPtr<Attribute> attrs = {};
    TokenKind kind = TokenKind::Eof;
    bool selfClosing = false;
    bool forceQuirks = false;
};

using TokenSink = void (*)(void* user, const Token* token);

struct TokenizerOptions {
    bool exactErrors = false;
    bool discardBom = true;
};

struct ParseOptions {
    TokenizerOptions tokenizer = {};
    bool exactErrors = false;
    bool scriptingEnabled = true;
    bool iframeSrcdoc = false;
    bool dropDoctype = false;
};

struct SerializeOptions {
    bool includeNode = false;
    bool createMissingParent = true;
};

void Tokenize(Arena* a, Str source, TokenSink sink, void* user = nullptr,
              TokenizerOptions options = {});
Node* ParseDocument(Arena* a, Str source, ParseOptions options = {});
Node* ParseFragment(Arena* a, Str source, Str context = Str{},
                    ParseOptions options = {});
Str Serialize(Arena* a, const Node* node, SerializeOptions options = {});

struct Parser {
    Arena* a = nullptr;
    ParseOptions options = {};
    void* impl = nullptr;
};

Parser* ParserNew(Arena* a, ParseOptions options = {});
Parser* ParserNewFragment(Arena* a, Str context, ParseOptions options = {});
void ParserProcess(Parser* parser, Str chunk);
bool ParserIsPaused(const Parser* parser);
void ParserResumeAfterCurrentScript(Parser* parser);
Node* ParserFinish(Parser* parser);

inline Str AttributeName(Arena* a, const Attribute* attr) {
    return attr ? ArenaStrGet(a, attr->name) : Str{};
}
inline Str AttributeValue(Arena* a, const Attribute* attr) {
    return attr ? ArenaStrGet(a, attr->value) : Str{};
}
inline Attribute* AttributeNext(Arena* a, Attribute* attr) {
    return attr ? ArenaPtrGet(a, attr->next) : nullptr;
}
inline const Attribute* AttributeNext(Arena* a, const Attribute* attr) {
    return attr ? ArenaPtrGet(a, attr->next) : nullptr;
}

inline Str NodeName(Arena* a, const Node* node) {
    return node ? ArenaStrGet(a, node->name) : Str{};
}
inline Str NodeData(Arena* a, const Node* node) {
    return node ? ArenaStrGet(a, node->data) : Str{};
}
inline Str NodeSystemId(Arena* a, const Node* node) {
    return node ? ArenaStrGet(a, node->systemId) : Str{};
}
inline Attribute* NodeAttrs(Arena* a, Node* node) {
    return node ? ArenaPtrGet(a, node->attrs) : nullptr;
}
inline const Attribute* NodeAttrs(Arena* a, const Node* node) {
    return node ? ArenaPtrGet(a, node->attrs) : nullptr;
}
#define GPUI_HTML5EVER_NODE_LINK(Name, field)                   \
    inline Node* Node##Name(Arena* a, Node* node) {             \
        return node ? ArenaPtrGet(a, node->field) : nullptr;    \
    }                                                           \
    inline const Node* Node##Name(Arena* a, const Node* node) { \
        return node ? ArenaPtrGet(a, node->field) : nullptr;    \
    }
GPUI_HTML5EVER_NODE_LINK(Parent, parent)
GPUI_HTML5EVER_NODE_LINK(First, first)
GPUI_HTML5EVER_NODE_LINK(Last, last)
GPUI_HTML5EVER_NODE_LINK(Next, next)
#undef GPUI_HTML5EVER_NODE_LINK

inline Str TokenName(Arena* a, const Token* token) {
    return token ? ArenaStrGet(a, token->name) : Str{};
}
inline Str TokenData(Arena* a, const Token* token) {
    return token ? ArenaStrGet(a, token->data) : Str{};
}
inline Str TokenSystemId(Arena* a, const Token* token) {
    return token ? ArenaStrGet(a, token->systemId) : Str{};
}
inline const Attribute* TokenAttrs(Arena* a, const Token* token) {
    return token ? ArenaPtrGet(a, token->attrs) : nullptr;
}

const Attribute* Attr(Arena* a, const Node* node, Str name);
Str AttrValue(Arena* a, const Node* node, Str name);

}

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/html5ever-mini/html5ever.h"

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/markdown/constant.h"

namespace markdown {

using base::SeqStrFirst;
using base::SeqStrings;
using base::SeqStrNext;
using base::Str;

constexpr int kAutolinkSchemeSizeMax = 32;

constexpr int kAutolinkDomainSizeMax = 63;

constexpr int kCharacterReferenceDecimalSizeMax = 7;

constexpr int kCharacterReferenceHexadecimalSizeMax = 6;

constexpr int kCharacterReferenceNamedSizeMax = 31;

constexpr int kCodeFencedSequenceSizeMin = 3;

constexpr int kFrontmatterSequenceSize = 3;

constexpr int kHardBreakPrefixSizeMin = 2;

constexpr int kHeadingAtxOpeningFenceSizeMax = 6;

extern const Str kHtmlCdataPrefix;

constexpr int kHtmlRawSizeMax = 8;

constexpr int kLinkReferenceSizeMax = 999;

constexpr int kListItemValueSizeMax = 10;

constexpr int kMathFlowSequenceSizeMin = 2;

constexpr int kResourceDestinationBalanceMax = 32;

constexpr int kTabSize = 4;

constexpr int kThematicBreakMarkerCountMin = 3;

extern const char kHtmlBlockNames[];
extern const char kHtmlRawNames[];

struct CharacterReference {
    int32_t nameOff;
    int32_t valueOff;
};
extern const char kCharacterReferenceNames[];
extern const char kCharacterReferenceValues[];
extern const CharacterReference kCharacterReferences[2125];

}

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/markdown/state.h"

namespace markdown {

struct Tokenizer;

enum class StateName : uint16_t {
    AttentionStart,
    AttentionInside,
    AutolinkStart,
    AutolinkOpen,
    AutolinkSchemeOrEmailAtext,
    AutolinkSchemeInsideOrEmailAtext,
    AutolinkUrlInside,
    AutolinkEmailAtSignOrDot,
    AutolinkEmailAtext,
    AutolinkEmailValue,
    AutolinkEmailLabel,
    BlankLineStart,
    BlankLineAfter,
    BlockQuoteStart,
    BlockQuoteContStart,
    BlockQuoteContBefore,
    BlockQuoteContAfter,
    BomStart,
    BomInside,
    CharacterEscapeStart,
    CharacterEscapeInside,
    CharacterReferenceStart,
    CharacterReferenceOpen,
    CharacterReferenceNumeric,
    CharacterReferenceValue,
    CodeIndentedStart,
    CodeIndentedAtBreak,
    CodeIndentedAfter,
    CodeIndentedFurtherStart,
    CodeIndentedInside,
    CodeIndentedFurtherBegin,
    CodeIndentedFurtherAfter,
    ContentChunkStart,
    ContentChunkInside,
    ContentDefinitionBefore,
    ContentDefinitionAfter,
    DataStart,
    DataInside,
    DataAtBreak,
    DefinitionStart,
    DefinitionBefore,
    DefinitionLabelAfter,
    DefinitionLabelNok,
    DefinitionMarkerAfter,
    DefinitionDestinationBefore,
    DefinitionDestinationAfter,
    DefinitionDestinationMissing,
    DefinitionTitleBefore,
    DefinitionAfter,
    DefinitionAfterWhitespace,
    DefinitionTitleBeforeMarker,
    DefinitionTitleAfter,
    DefinitionTitleAfterOptionalWhitespace,
    DestinationStart,
    DestinationEnclosedBefore,
    DestinationEnclosed,
    DestinationEnclosedEscape,
    DestinationRaw,
    DestinationRawEscape,
    DocumentStart,
    DocumentBeforeFrontmatter,
    DocumentContainerExistingBefore,
    DocumentContainerExistingAfter,
    DocumentContainerNewBefore,
    DocumentContainerNewBeforeNotBlockQuote,
    DocumentContainerNewBeforeNotList,
    DocumentContainerNewBeforeNotGfmFootnoteDefinition,
    DocumentContainerNewAfter,
    DocumentContainersAfter,
    DocumentFlowInside,
    DocumentFlowEnd,
    FlowStart,
    FlowBeforeGfmTable,
    FlowBeforeCodeIndented,
    FlowBeforeRaw,
    FlowBeforeHtml,
    FlowBeforeHeadingAtx,
    FlowBeforeHeadingSetext,
    FlowBeforeThematicBreak,
    FlowAfter,
    FlowBlankLineBefore,
    FlowBlankLineAfter,
    FlowBeforeContent,
    FrontmatterStart,
    FrontmatterOpenSequence,
    FrontmatterOpenAfter,
    FrontmatterAfter,
    FrontmatterContentStart,
    FrontmatterContentInside,
    FrontmatterContentEnd,
    FrontmatterCloseStart,
    FrontmatterCloseSequence,
    FrontmatterCloseAfter,
    GfmAutolinkLiteralProtocolStart,
    GfmAutolinkLiteralProtocolAfter,
    GfmAutolinkLiteralProtocolPrefixInside,
    GfmAutolinkLiteralProtocolSlashesInside,
    GfmAutolinkLiteralWwwStart,
    GfmAutolinkLiteralWwwAfter,
    GfmAutolinkLiteralWwwPrefixInside,
    GfmAutolinkLiteralWwwPrefixAfter,
    GfmAutolinkLiteralDomainInside,
    GfmAutolinkLiteralDomainAtPunctuation,
    GfmAutolinkLiteralDomainAfter,
    GfmAutolinkLiteralPathInside,
    GfmAutolinkLiteralPathAtPunctuation,
    GfmAutolinkLiteralPathAfter,
    GfmAutolinkLiteralTrail,
    GfmAutolinkLiteralTrailCharRefInside,
    GfmAutolinkLiteralTrailCharRefStart,
    GfmAutolinkLiteralTrailBracketAfter,
    GfmFootnoteDefinitionStart,
    GfmFootnoteDefinitionLabelBefore,
    GfmFootnoteDefinitionLabelAtMarker,
    GfmFootnoteDefinitionLabelInside,
    GfmFootnoteDefinitionLabelEscape,
    GfmFootnoteDefinitionLabelAfter,
    GfmFootnoteDefinitionWhitespaceAfter,
    GfmFootnoteDefinitionContStart,
    GfmFootnoteDefinitionContBlank,
    GfmFootnoteDefinitionContFilled,
    GfmLabelStartFootnoteStart,
    GfmLabelStartFootnoteOpen,
    GfmTaskListItemCheckStart,
    GfmTaskListItemCheckInside,
    GfmTaskListItemCheckClose,
    GfmTaskListItemCheckAfter,
    GfmTaskListItemCheckAfterSpaceOrTab,
    GfmTableStart,
    GfmTableHeadRowBefore,
    GfmTableHeadRowStart,
    GfmTableHeadRowBreak,
    GfmTableHeadRowData,
    GfmTableHeadRowEscape,
    GfmTableHeadDelimiterStart,
    GfmTableHeadDelimiterBefore,
    GfmTableHeadDelimiterCellBefore,
    GfmTableHeadDelimiterValueBefore,
    GfmTableHeadDelimiterLeftAlignmentAfter,
    GfmTableHeadDelimiterFiller,
    GfmTableHeadDelimiterRightAlignmentAfter,
    GfmTableHeadDelimiterCellAfter,
    GfmTableHeadDelimiterNok,
    GfmTableBodyRowStart,
    GfmTableBodyRowBreak,
    GfmTableBodyRowData,
    GfmTableBodyRowEscape,
    HardBreakEscapeStart,
    HardBreakEscapeAfter,
    HeadingAtxStart,
    HeadingAtxBefore,
    HeadingAtxSequenceOpen,
    HeadingAtxAtBreak,
    HeadingAtxSequenceFurther,
    HeadingAtxData,
    HeadingSetextStart,
    HeadingSetextBefore,
    HeadingSetextInside,
    HeadingSetextAfter,
    HtmlFlowStart,
    HtmlFlowBefore,
    HtmlFlowOpen,
    HtmlFlowDeclarationOpen,
    HtmlFlowCommentOpenInside,
    HtmlFlowCdataOpenInside,
    HtmlFlowTagCloseStart,
    HtmlFlowTagName,
    HtmlFlowBasicSelfClosing,
    HtmlFlowCompleteClosingTagAfter,
    HtmlFlowCompleteEnd,
    HtmlFlowCompleteAttributeNameBefore,
    HtmlFlowCompleteAttributeName,
    HtmlFlowCompleteAttributeNameAfter,
    HtmlFlowCompleteAttributeValueBefore,
    HtmlFlowCompleteAttributeValueQuoted,
    HtmlFlowCompleteAttributeValueQuotedAfter,
    HtmlFlowCompleteAttributeValueUnquoted,
    HtmlFlowCompleteAfter,
    HtmlFlowBlankLineBefore,
    HtmlFlowContinuation,
    HtmlFlowContinuationDeclarationInside,
    HtmlFlowContinuationAfter,
    HtmlFlowContinuationStart,
    HtmlFlowContinuationBefore,
    HtmlFlowContinuationCommentInside,
    HtmlFlowContinuationRawTagOpen,
    HtmlFlowContinuationRawEndTag,
    HtmlFlowContinuationClose,
    HtmlFlowContinuationCdataInside,
    HtmlFlowContinuationStartNonLazy,
    HtmlTextStart,
    HtmlTextOpen,
    HtmlTextDeclarationOpen,
    HtmlTextTagCloseStart,
    HtmlTextTagClose,
    HtmlTextTagCloseBetween,
    HtmlTextTagOpen,
    HtmlTextTagOpenBetween,
    HtmlTextTagOpenAttributeName,
    HtmlTextTagOpenAttributeNameAfter,
    HtmlTextTagOpenAttributeValueBefore,
    HtmlTextTagOpenAttributeValueQuoted,
    HtmlTextTagOpenAttributeValueQuotedAfter,
    HtmlTextTagOpenAttributeValueUnquoted,
    HtmlTextCdata,
    HtmlTextCdataOpenInside,
    HtmlTextCdataClose,
    HtmlTextCdataEnd,
    HtmlTextCommentOpenInside,
    HtmlTextComment,
    HtmlTextCommentClose,
    HtmlTextCommentEnd,
    HtmlTextDeclaration,
    HtmlTextEnd,
    HtmlTextInstruction,
    HtmlTextInstructionClose,
    HtmlTextLineEndingBefore,
    HtmlTextLineEndingAfter,
    HtmlTextLineEndingAfterPrefix,
    LabelStart,
    LabelAtBreak,
    LabelEolAfter,
    LabelEscape,
    LabelInside,
    LabelNok,
    LabelEndStart,
    LabelEndAfter,
    LabelEndResourceStart,
    LabelEndResourceBefore,
    LabelEndResourceOpen,
    LabelEndResourceDestinationAfter,
    LabelEndResourceDestinationMissing,
    LabelEndResourceBetween,
    LabelEndResourceTitleAfter,
    LabelEndResourceEnd,
    LabelEndOk,
    LabelEndNok,
    LabelEndReferenceFull,
    LabelEndReferenceFullAfter,
    LabelEndReferenceFullMissing,
    LabelEndReferenceNotFull,
    LabelEndReferenceCollapsed,
    LabelEndReferenceCollapsedOpen,
    LabelStartImageStart,
    LabelStartImageOpen,
    LabelStartImageAfter,
    LabelStartLinkStart,
    ListItemStart,
    ListItemBefore,
    ListItemBeforeOrdered,
    ListItemBeforeUnordered,
    ListItemValue,
    ListItemMarker,
    ListItemMarkerAfter,
    ListItemAfter,
    ListItemMarkerAfterFilled,
    ListItemWhitespace,
    ListItemPrefixOther,
    ListItemWhitespaceAfter,
    ListItemContStart,
    ListItemContBlank,
    ListItemContFilled,
    NonLazyContinuationStart,
    NonLazyContinuationAfter,
    ParagraphStart,
    ParagraphLineStart,
    ParagraphInside,
    RawFlowStart,
    RawFlowBeforeSequenceOpen,
    RawFlowSequenceOpen,
    RawFlowInfoBefore,
    RawFlowInfo,
    RawFlowMetaBefore,
    RawFlowMeta,
    RawFlowAtNonLazyBreak,
    RawFlowCloseStart,
    RawFlowBeforeSequenceClose,
    RawFlowSequenceClose,
    RawFlowAfterSequenceClose,
    RawFlowContentBefore,
    RawFlowContentStart,
    RawFlowBeforeContentChunk,
    RawFlowContentChunk,
    RawFlowAfter,
    RawTextStart,
    RawTextSequenceOpen,
    RawTextBetween,
    RawTextData,
    RawTextSequenceClose,
    SpaceOrTabStart,
    SpaceOrTabInside,
    SpaceOrTabAfter,
    SpaceOrTabEolStart,
    SpaceOrTabEolAfterFirst,
    SpaceOrTabEolAfterEol,
    SpaceOrTabEolAtEol,
    SpaceOrTabEolAfterMore,
    StringStart,
    StringBefore,
    StringBeforeData,
    TextStart,
    TextBefore,
    TextBeforeHtml,
    TextBeforeHardBreakEscape,
    TextBeforeLabelStartLink,
    TextBeforeData,
    ThematicBreakStart,
    ThematicBreakBefore,
    ThematicBreakSequence,
    ThematicBreakAtBreak,
    TitleStart,
    TitleBegin,
    TitleAfterEol,
    TitleAtBreak,
    TitleEscape,
    TitleInside,
    TitleNok,
    Count,
};

struct State {
    enum class Kind : uint8_t {
        Ok,
        Nok,
        Next,
        Retry,
    };

    Kind kind = Kind::Ok;
    StateName name = StateName::AttentionStart;

    bool operator==(const State& other) const {
        if (kind != other.kind) {
            return false;
        }
        if (kind == Kind::Next || kind == Kind::Retry) {
            return name == other.name;
        }
        return true;
    }
    bool operator!=(const State& other) const { return !(*this == other); }
};

inline State StateNext(StateName name) {
    return State{State::Kind::Next, name};
}
inline State StateRetry(StateName name) {
    return State{State::Kind::Retry, name};
}
inline State StateOk() {
    return State{State::Kind::Ok, StateName::AttentionStart};
}
inline State StateNok() {
    return State{State::Kind::Nok, StateName::AttentionStart};
}

State Call(Tokenizer* tokenizer, StateName name);

}

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/markdown/event.h"

namespace markdown {

using base::Str;
using base::Vec;

enum class Name : uint8_t {
    AttentionSequence,
    Autolink,
    AutolinkEmail,
    AutolinkMarker,
    AutolinkProtocol,
    BlankLineEnding,
    BlockQuote,
    BlockQuoteMarker,
    BlockQuotePrefix,
    ByteOrderMark,
    CharacterEscape,
    CharacterEscapeMarker,
    CharacterEscapeValue,
    CharacterReference,
    CharacterReferenceMarker,
    CharacterReferenceMarkerHexadecimal,
    CharacterReferenceMarkerNumeric,
    CharacterReferenceMarkerSemi,
    CharacterReferenceValue,
    CodeFenced,
    CodeFencedFence,
    CodeFencedFenceInfo,
    CodeFencedFenceMeta,
    CodeFencedFenceSequence,
    CodeFlowChunk,
    CodeIndented,
    CodeText,
    CodeTextData,
    CodeTextSequence,
    Content,
    Data,
    Definition,
    DefinitionDestination,
    DefinitionDestinationLiteral,
    DefinitionDestinationLiteralMarker,
    DefinitionDestinationRaw,
    DefinitionDestinationString,
    DefinitionLabel,
    DefinitionLabelMarker,
    DefinitionLabelString,
    DefinitionMarker,
    DefinitionTitle,
    DefinitionTitleMarker,
    DefinitionTitleString,
    Emphasis,
    EmphasisSequence,
    EmphasisText,
    Frontmatter,
    FrontmatterChunk,
    FrontmatterFence,
    FrontmatterSequence,
    GfmAutolinkLiteralEmail,
    GfmAutolinkLiteralMailto,
    GfmAutolinkLiteralProtocol,
    GfmAutolinkLiteralWww,
    GfmAutolinkLiteralXmpp,
    GfmFootnoteCall,
    GfmFootnoteCallLabel,
    GfmFootnoteCallMarker,
    GfmFootnoteDefinition,
    GfmFootnoteDefinitionPrefix,
    GfmFootnoteDefinitionLabel,
    GfmFootnoteDefinitionLabelMarker,
    GfmFootnoteDefinitionLabelString,
    GfmFootnoteDefinitionMarker,
    GfmStrikethrough,
    GfmStrikethroughSequence,
    GfmStrikethroughText,
    GfmTable,
    GfmTableBody,
    GfmTableCell,
    GfmTableCellText,
    GfmTableCellDivider,
    GfmTableDelimiterRow,
    GfmTableDelimiterMarker,
    GfmTableDelimiterCell,
    GfmTableDelimiterCellValue,
    GfmTableDelimiterFiller,
    GfmTableHead,
    GfmTableRow,
    GfmTaskListItemCheck,
    GfmTaskListItemMarker,
    GfmTaskListItemValueChecked,
    GfmTaskListItemValueUnchecked,
    HardBreakEscape,
    HardBreakTrailing,
    HeadingAtx,
    HeadingAtxSequence,
    HeadingAtxText,
    HeadingSetext,
    HeadingSetextText,
    HeadingSetextUnderline,
    HeadingSetextUnderlineSequence,
    HtmlFlow,
    HtmlFlowData,
    HtmlText,
    HtmlTextData,
    Image,
    Label,
    LabelEnd,
    LabelImage,
    LabelImageMarker,
    LabelLink,
    LabelMarker,
    LabelText,
    LineEnding,
    Link,
    ListItem,
    ListItemMarker,
    ListItemPrefix,
    ListItemValue,
    ListOrdered,
    ListUnordered,
    MathFlow,
    MathFlowFence,
    MathFlowFenceMeta,
    MathFlowFenceSequence,
    MathFlowChunk,
    MathText,
    MathTextData,
    MathTextSequence,
    Paragraph,
    Reference,
    ReferenceMarker,
    ReferenceString,
    Resource,
    ResourceDestination,
    ResourceDestinationLiteral,
    ResourceDestinationLiteralMarker,
    ResourceDestinationRaw,
    ResourceDestinationString,
    ResourceMarker,
    ResourceTitle,
    ResourceTitleMarker,
    ResourceTitleString,
    SpaceOrTab,
    Strong,
    StrongSequence,
    StrongText,
    ThematicBreak,
    ThematicBreakSequence,
    LinePrefix,
};

bool IsVoidEvent(Name name);

enum class ContentKind : uint8_t {
    Flow,
    Content,
    String,
    Text,
};

struct Link {
    int32_t previous = -1;
    int32_t next = -1;
    ContentKind content = ContentKind::Flow;
};

struct Point {
    int32_t line = 1;
    int32_t column = 1;
    int32_t index = 0;
    int32_t vs = 0;
};

Point PointShiftTo(const Point& from, Str bytes, int32_t index);

enum class Kind : uint8_t {
    Enter,
    Exit,
};

struct Event {
    Kind kind = Kind::Enter;
    Name name = Name::Data;
    bool hasLink = false;
    Point point = {};
    Link link = {};
};

}

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/markdown/util.h"

namespace markdown {

using base::Arena;
using base::ArenaVec;

Str StrOwn(Arena* a, Str s);
Str StrOwn(Arena* a, const char* s, int32_t len);

enum class CharKind : uint8_t {
    Whitespace,
    Punctuation,
    Other,
};

bool IsUnicodePunctuation(uint32_t cp);

int32_t CharBeforeIndex(Str bytes, int32_t index);
int32_t CharAfterIndex(Str bytes, int32_t index);

CharKind Classify(int32_t cp);
CharKind KindAfterIndex(Str bytes, int32_t index);

inline bool IsAsciiWhitespace(uint8_t b) {
    return b == ' ' || b == '\t' || b == '\n' || b == '\r' || b == 0x0c;
}
inline bool IsAsciiPunctuation(uint8_t b) {
    return (b >= '!' && b <= '/') || (b >= ':' && b <= '@') ||
           (b >= '[' && b <= '`') || (b >= '{' && b <= '~');
}
inline bool IsAsciiDigit(uint8_t b) {
    return b >= '0' && b <= '9';
}
inline bool IsAsciiHexDigit(uint8_t b) {
    return IsAsciiDigit(b) || (b >= 'a' && b <= 'f') || (b >= 'A' && b <= 'F');
}
inline bool IsAsciiAlpha(uint8_t b) {
    return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z');
}
inline bool IsAsciiAlphanumeric(uint8_t b) {
    return IsAsciiAlpha(b) || IsAsciiDigit(b);
}
inline bool IsAsciiControl(uint8_t b) {
    return b < 0x20 || b == 0x7f;
}

int32_t Utf8Encode(char* out, uint32_t cp);

struct Position {
    Point start = {};
    Point end = {};
};

Position PositionFromExitEvent(const Vec<Event>& events, int32_t index);

struct Slice {
    Str bytes = {};
    int32_t before = 0;
    int32_t after = 0;

    int32_t Len() const { return len(bytes) + before + after; }
};

Slice SliceFromPosition(Str bytes, const Position& position);
Slice SliceFromIndices(Str bytes, int32_t start, int32_t end);

Str SliceSerialize(Arena* a, const Slice& slice);

struct EditMap {
    struct Entry {
        int32_t at = 0;
        int32_t remove = 0;
        ArenaVec<Event> add{};
    };

    Arena* a = nullptr;
    Vec<Entry> map;

    Vec<int32_t> buckets;
};

void EditMapAdd(EditMap& map, int32_t index, int32_t remove, const Event* add,
                int32_t addLen);
void EditMapAddBefore(EditMap& map, int32_t index, int32_t remove,
                      const Event* add, int32_t addLen);
inline bool EditMapEmpty(const EditMap& map) {
    return map.map.len == 0;
}
void EditMapConsume(EditMap& map, Vec<Event>& events);

int32_t SkipOpt(const Vec<Event>& events, int32_t index, const Name* names,
                int32_t namesLen);
int32_t SkipOptBack(const Vec<Event>& events, int32_t index, const Name* names,
                    int32_t namesLen);
int32_t SkipTo(const Vec<Event>& events, int32_t index, const Name* names,
               int32_t namesLen);
int32_t SkipToBack(const Vec<Event>& events, int32_t index, const Name* names,
                   int32_t namesLen);

Str NormalizeIdentifier(Arena* a, Str value);

bool ListLoose(const Vec<Event>& events, int32_t index, bool includeItems);
bool ListItemLoose(const Vec<Event>& events, int32_t index);

ArenaAlign GfmTableAlign(const Vec<Event>& events, int32_t index, Arena* a);

int32_t CharacterReferenceValueMax(uint8_t marker);
bool CharacterReferenceValueTest(uint8_t marker, uint8_t byte);

Str CharacterReferenceDecode(Arena* a, Str value, uint8_t marker);

base::TempStr CharacterReferenceDecodeTemp(Str value, uint8_t marker);

}

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/markdown/tokenizer.h"

namespace markdown {

enum class Container : uint8_t {
    BlockQuote,
    ListItem,
    GfmFootnoteDefinition,
};

struct ContainerState {
    Container kind = Container::BlockQuote;
    bool blankInitial = false;
    int32_t size = 0;
};

enum class LabelKind : uint8_t {
    Image,
    Link,
    GfmFootnote,
    GfmUndefinedFootnote,
};

struct LabelStartMark {
    LabelKind kind = LabelKind::Image;
    int32_t startA = 0;
    int32_t startB = 0;
    bool inactive = false;
};

struct Label {
    LabelKind kind = LabelKind::Image;
    int32_t startA = 0;
    int32_t startB = 0;
    int32_t endA = 0;
    int32_t endB = 0;
};

enum class ResolveName : uint8_t {
    Label,
    Attention,
    GfmTable,
    HeadingAtx,
    HeadingSetext,
    ListItem,
    Content,
    Data,
    String,
    Text,
};

struct Subresult {
    bool done = false;
    Vec<Str> gfmFootnoteDefinitions;
    Vec<Str> definitions;
};

void SubresultAppend(Subresult& dst, Subresult& src);

struct Tokenizer;

struct ParseState {

    Arena* a = nullptr;

    Arena* scratch = nullptr;
    const ParseOptions* options = nullptr;
    Str bytes = {};
    Vec<Str> definitions;
    Vec<Str> gfmFootnoteDefinitions;
};

struct TokenizeState {
    Tokenizer* documentChild = nullptr;
    State documentChildState = {};
    bool documentChildStateSome = false;
    Vec<ContainerState> documentContainerStack;
    int32_t documentContinued = 0;

    int32_t documentDataIndex = -1;

    Vec<ArenaVec<Event>> documentExits;
    bool documentLazyAcceptingBefore = false;
    bool documentAtFirstParagraphOfListItem = false;

    ContentKind spaceOrTabEolContent = ContentKind::Flow;
    bool spaceOrTabEolContentSome = false;
    bool spaceOrTabEolConnect = false;
    bool spaceOrTabEolOk = false;
    bool spaceOrTabConnect = false;
    ContentKind spaceOrTabContent = ContentKind::Flow;
    bool spaceOrTabContentSome = false;
    int32_t spaceOrTabMin = 0;
    int32_t spaceOrTabMax = 0;
    int32_t spaceOrTabSize = 0;
    Name spaceOrTabToken = Name::SpaceOrTab;

    Vec<LabelStartMark> labelStarts;
    Vec<LabelStartMark> labelStartsLoose;
    Vec<Label> labels;
    Vec<Str> definitions;
    Vec<Str> gfmFootnoteDefinitions;

    bool connect = false;
    uint8_t marker = 0;
    uint8_t markerB = 0;
    const uint8_t* markers = nullptr;
    int32_t markersLen = 0;
    bool seen = false;
    int32_t size = 0;
    int32_t sizeB = 0;
    int32_t sizeC = 0;
    int32_t start = 0;
    int32_t end = 0;
    Name token1 = Name::Data;
    Name token2 = Name::Data;
    Name token3 = Name::Data;
    Name token4 = Name::Data;
    Name token5 = Name::Data;
    Name token6 = Name::Data;
};

struct IndexVs {
    int32_t index = 0;
    int32_t vs = 0;
};

struct Progress {
    int32_t eventsLen = 0;
    int32_t stackLen = 0;
    int32_t previous = -1;
    int32_t current = -1;
    Point point = {};
};

struct Attempt {
    State ok = {};
    State nok = {};

    bool check = false;
    bool hasProgress = false;
    Progress progress = {};
};

struct Tokenizer {

    Vec<IndexVs> columnStart;
    int32_t firstLine = 1;
    Point lineStart = {};
    bool consumed = true;
    Vec<Attempt> attempts;

    int32_t current = -1;
    int32_t previous = -1;
    Point point = {};
    Vec<Event> events;
    Vec<Name> stack;
    EditMap map;
    Vec<ResolveName> resolvers;
    ParseState* parseState = nullptr;
    TokenizeState tokenizeState;
    bool interrupt = false;
    bool concrete = false;
    bool pierce = false;
    bool lazy = false;
};

Tokenizer* TokenizerNew(Point point, ParseState* parseState);
void TokenizerFree(Tokenizer* tokenizer);

void RegisterResolver(Tokenizer* t, ResolveName name);
void RegisterResolverBefore(Tokenizer* t, ResolveName name);
void DefineSkip(Tokenizer* t, Point point);
void Consume(Tokenizer* t);
void Enter(Tokenizer* t, Name name);
void EnterLink(Tokenizer* t, Name name, Link link);
void Exit(Tokenizer* t, Name name);

void TokenizerCheck(Tokenizer* t, State ok, State nok);
void TokenizerAttempt(Tokenizer* t, State ok, State nok);

State Push(Tokenizer* t, int32_t fromIndex, int32_t fromVs, int32_t toIndex,
           int32_t toVs, State state);
Subresult Flush(Tokenizer* t, State state, bool resolve);

bool ResolveCall(Tokenizer* t, ResolveName name, Subresult* out);

void SubtokenizeLink(Vec<Event>& events, int32_t index);
void SubtokenizeLinkTo(Vec<Event>& events, int32_t previous, int32_t next);

Subresult Subtokenize(Vec<Event>& events, ParseState* parseState,
                      bool hasFilter, ContentKind filter);
void DivideEvents(EditMap& map, const Vec<Event>& events, int32_t linkIndex,
                  Vec<Event>& childEvents, int32_t* accA, int32_t* accB);

Vec<Event> Parse(ParseState* parseState);

Node* ToMdastCompile(const Vec<Event>& events, ParseState* parseState,
                     NodePositions* positions);

}

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/markdown/construct.h"

namespace markdown {

State AttentionStart(Tokenizer* t);
State AttentionInside(Tokenizer* t);
State AutolinkStart(Tokenizer* t);
State AutolinkOpen(Tokenizer* t);
State AutolinkSchemeOrEmailAtext(Tokenizer* t);
State AutolinkSchemeInsideOrEmailAtext(Tokenizer* t);
State AutolinkUrlInside(Tokenizer* t);
State AutolinkEmailAtSignOrDot(Tokenizer* t);
State AutolinkEmailAtext(Tokenizer* t);
State AutolinkEmailValue(Tokenizer* t);
State AutolinkEmailLabel(Tokenizer* t);
State BlankLineStart(Tokenizer* t);
State BlankLineAfter(Tokenizer* t);
State BlockQuoteStart(Tokenizer* t);
State BlockQuoteContStart(Tokenizer* t);
State BlockQuoteContBefore(Tokenizer* t);
State BlockQuoteContAfter(Tokenizer* t);
State BomStart(Tokenizer* t);
State BomInside(Tokenizer* t);
State CharacterEscapeStart(Tokenizer* t);
State CharacterEscapeInside(Tokenizer* t);
State CharacterReferenceStart(Tokenizer* t);
State CharacterReferenceOpen(Tokenizer* t);
State CharacterReferenceNumeric(Tokenizer* t);
State CharacterReferenceValue(Tokenizer* t);
State CodeIndentedStart(Tokenizer* t);
State CodeIndentedAtBreak(Tokenizer* t);
State CodeIndentedAfter(Tokenizer* t);
State CodeIndentedFurtherStart(Tokenizer* t);
State CodeIndentedInside(Tokenizer* t);
State CodeIndentedFurtherBegin(Tokenizer* t);
State CodeIndentedFurtherAfter(Tokenizer* t);
State ContentChunkStart(Tokenizer* t);
State ContentChunkInside(Tokenizer* t);
State ContentDefinitionBefore(Tokenizer* t);
State ContentDefinitionAfter(Tokenizer* t);
State DataStart(Tokenizer* t);
State DataInside(Tokenizer* t);
State DataAtBreak(Tokenizer* t);
State DefinitionStart(Tokenizer* t);
State DefinitionBefore(Tokenizer* t);
State DefinitionLabelAfter(Tokenizer* t);
State DefinitionLabelNok(Tokenizer* t);
State DefinitionMarkerAfter(Tokenizer* t);
State DefinitionDestinationBefore(Tokenizer* t);
State DefinitionDestinationAfter(Tokenizer* t);
State DefinitionDestinationMissing(Tokenizer* t);
State DefinitionTitleBefore(Tokenizer* t);
State DefinitionAfter(Tokenizer* t);
State DefinitionAfterWhitespace(Tokenizer* t);
State DefinitionTitleBeforeMarker(Tokenizer* t);
State DefinitionTitleAfter(Tokenizer* t);
State DefinitionTitleAfterOptionalWhitespace(Tokenizer* t);
State DestinationStart(Tokenizer* t);
State DestinationEnclosedBefore(Tokenizer* t);
State DestinationEnclosed(Tokenizer* t);
State DestinationEnclosedEscape(Tokenizer* t);
State DestinationRaw(Tokenizer* t);
State DestinationRawEscape(Tokenizer* t);
State DocumentStart(Tokenizer* t);
State DocumentBeforeFrontmatter(Tokenizer* t);
State DocumentContainerExistingBefore(Tokenizer* t);
State DocumentContainerExistingAfter(Tokenizer* t);
State DocumentContainerNewBefore(Tokenizer* t);
State DocumentContainerNewBeforeNotBlockQuote(Tokenizer* t);
State DocumentContainerNewBeforeNotList(Tokenizer* t);
State DocumentContainerNewBeforeNotGfmFootnoteDefinition(Tokenizer* t);
State DocumentContainerNewAfter(Tokenizer* t);
State DocumentContainersAfter(Tokenizer* t);
State DocumentFlowInside(Tokenizer* t);
State DocumentFlowEnd(Tokenizer* t);
State FlowStart(Tokenizer* t);
State FlowBeforeGfmTable(Tokenizer* t);
State FlowBeforeCodeIndented(Tokenizer* t);
State FlowBeforeRaw(Tokenizer* t);
State FlowBeforeHtml(Tokenizer* t);
State FlowBeforeHeadingAtx(Tokenizer* t);
State FlowBeforeHeadingSetext(Tokenizer* t);
State FlowBeforeThematicBreak(Tokenizer* t);
State FlowAfter(Tokenizer* t);
State FlowBlankLineBefore(Tokenizer* t);
State FlowBlankLineAfter(Tokenizer* t);
State FlowBeforeContent(Tokenizer* t);
State FrontmatterStart(Tokenizer* t);
State FrontmatterOpenSequence(Tokenizer* t);
State FrontmatterOpenAfter(Tokenizer* t);
State FrontmatterAfter(Tokenizer* t);
State FrontmatterContentStart(Tokenizer* t);
State FrontmatterContentInside(Tokenizer* t);
State FrontmatterContentEnd(Tokenizer* t);
State FrontmatterCloseStart(Tokenizer* t);
State FrontmatterCloseSequence(Tokenizer* t);
State FrontmatterCloseAfter(Tokenizer* t);
State GfmAutolinkLiteralProtocolStart(Tokenizer* t);
State GfmAutolinkLiteralProtocolAfter(Tokenizer* t);
State GfmAutolinkLiteralProtocolPrefixInside(Tokenizer* t);
State GfmAutolinkLiteralProtocolSlashesInside(Tokenizer* t);
State GfmAutolinkLiteralWwwStart(Tokenizer* t);
State GfmAutolinkLiteralWwwAfter(Tokenizer* t);
State GfmAutolinkLiteralWwwPrefixInside(Tokenizer* t);
State GfmAutolinkLiteralWwwPrefixAfter(Tokenizer* t);
State GfmAutolinkLiteralDomainInside(Tokenizer* t);
State GfmAutolinkLiteralDomainAtPunctuation(Tokenizer* t);
State GfmAutolinkLiteralDomainAfter(Tokenizer* t);
State GfmAutolinkLiteralPathInside(Tokenizer* t);
State GfmAutolinkLiteralPathAtPunctuation(Tokenizer* t);
State GfmAutolinkLiteralPathAfter(Tokenizer* t);
State GfmAutolinkLiteralTrail(Tokenizer* t);
State GfmAutolinkLiteralTrailCharRefInside(Tokenizer* t);
State GfmAutolinkLiteralTrailCharRefStart(Tokenizer* t);
State GfmAutolinkLiteralTrailBracketAfter(Tokenizer* t);
State GfmFootnoteDefinitionStart(Tokenizer* t);
State GfmFootnoteDefinitionLabelBefore(Tokenizer* t);
State GfmFootnoteDefinitionLabelAtMarker(Tokenizer* t);
State GfmFootnoteDefinitionLabelInside(Tokenizer* t);
State GfmFootnoteDefinitionLabelEscape(Tokenizer* t);
State GfmFootnoteDefinitionLabelAfter(Tokenizer* t);
State GfmFootnoteDefinitionWhitespaceAfter(Tokenizer* t);
State GfmFootnoteDefinitionContStart(Tokenizer* t);
State GfmFootnoteDefinitionContBlank(Tokenizer* t);
State GfmFootnoteDefinitionContFilled(Tokenizer* t);
State GfmLabelStartFootnoteStart(Tokenizer* t);
State GfmLabelStartFootnoteOpen(Tokenizer* t);
State GfmTaskListItemCheckStart(Tokenizer* t);
State GfmTaskListItemCheckInside(Tokenizer* t);
State GfmTaskListItemCheckClose(Tokenizer* t);
State GfmTaskListItemCheckAfter(Tokenizer* t);
State GfmTaskListItemCheckAfterSpaceOrTab(Tokenizer* t);
State GfmTableStart(Tokenizer* t);
State GfmTableHeadRowBefore(Tokenizer* t);
State GfmTableHeadRowStart(Tokenizer* t);
State GfmTableHeadRowBreak(Tokenizer* t);
State GfmTableHeadRowData(Tokenizer* t);
State GfmTableHeadRowEscape(Tokenizer* t);
State GfmTableHeadDelimiterStart(Tokenizer* t);
State GfmTableHeadDelimiterBefore(Tokenizer* t);
State GfmTableHeadDelimiterCellBefore(Tokenizer* t);
State GfmTableHeadDelimiterValueBefore(Tokenizer* t);
State GfmTableHeadDelimiterLeftAlignmentAfter(Tokenizer* t);
State GfmTableHeadDelimiterFiller(Tokenizer* t);
State GfmTableHeadDelimiterRightAlignmentAfter(Tokenizer* t);
State GfmTableHeadDelimiterCellAfter(Tokenizer* t);
State GfmTableHeadDelimiterNok(Tokenizer* t);
State GfmTableBodyRowStart(Tokenizer* t);
State GfmTableBodyRowBreak(Tokenizer* t);
State GfmTableBodyRowData(Tokenizer* t);
State GfmTableBodyRowEscape(Tokenizer* t);
State HardBreakEscapeStart(Tokenizer* t);
State HardBreakEscapeAfter(Tokenizer* t);
State HeadingAtxStart(Tokenizer* t);
State HeadingAtxBefore(Tokenizer* t);
State HeadingAtxSequenceOpen(Tokenizer* t);
State HeadingAtxAtBreak(Tokenizer* t);
State HeadingAtxSequenceFurther(Tokenizer* t);
State HeadingAtxData(Tokenizer* t);
State HeadingSetextStart(Tokenizer* t);
State HeadingSetextBefore(Tokenizer* t);
State HeadingSetextInside(Tokenizer* t);
State HeadingSetextAfter(Tokenizer* t);
State HtmlFlowStart(Tokenizer* t);
State HtmlFlowBefore(Tokenizer* t);
State HtmlFlowOpen(Tokenizer* t);
State HtmlFlowDeclarationOpen(Tokenizer* t);
State HtmlFlowCommentOpenInside(Tokenizer* t);
State HtmlFlowCdataOpenInside(Tokenizer* t);
State HtmlFlowTagCloseStart(Tokenizer* t);
State HtmlFlowTagName(Tokenizer* t);
State HtmlFlowBasicSelfClosing(Tokenizer* t);
State HtmlFlowCompleteClosingTagAfter(Tokenizer* t);
State HtmlFlowCompleteEnd(Tokenizer* t);
State HtmlFlowCompleteAttributeNameBefore(Tokenizer* t);
State HtmlFlowCompleteAttributeName(Tokenizer* t);
State HtmlFlowCompleteAttributeNameAfter(Tokenizer* t);
State HtmlFlowCompleteAttributeValueBefore(Tokenizer* t);
State HtmlFlowCompleteAttributeValueQuoted(Tokenizer* t);
State HtmlFlowCompleteAttributeValueQuotedAfter(Tokenizer* t);
State HtmlFlowCompleteAttributeValueUnquoted(Tokenizer* t);
State HtmlFlowCompleteAfter(Tokenizer* t);
State HtmlFlowBlankLineBefore(Tokenizer* t);
State HtmlFlowContinuation(Tokenizer* t);
State HtmlFlowContinuationDeclarationInside(Tokenizer* t);
State HtmlFlowContinuationAfter(Tokenizer* t);
State HtmlFlowContinuationStart(Tokenizer* t);
State HtmlFlowContinuationBefore(Tokenizer* t);
State HtmlFlowContinuationCommentInside(Tokenizer* t);
State HtmlFlowContinuationRawTagOpen(Tokenizer* t);
State HtmlFlowContinuationRawEndTag(Tokenizer* t);
State HtmlFlowContinuationClose(Tokenizer* t);
State HtmlFlowContinuationCdataInside(Tokenizer* t);
State HtmlFlowContinuationStartNonLazy(Tokenizer* t);
State HtmlTextStart(Tokenizer* t);
State HtmlTextOpen(Tokenizer* t);
State HtmlTextDeclarationOpen(Tokenizer* t);
State HtmlTextTagCloseStart(Tokenizer* t);
State HtmlTextTagClose(Tokenizer* t);
State HtmlTextTagCloseBetween(Tokenizer* t);
State HtmlTextTagOpen(Tokenizer* t);
State HtmlTextTagOpenBetween(Tokenizer* t);
State HtmlTextTagOpenAttributeName(Tokenizer* t);
State HtmlTextTagOpenAttributeNameAfter(Tokenizer* t);
State HtmlTextTagOpenAttributeValueBefore(Tokenizer* t);
State HtmlTextTagOpenAttributeValueQuoted(Tokenizer* t);
State HtmlTextTagOpenAttributeValueQuotedAfter(Tokenizer* t);
State HtmlTextTagOpenAttributeValueUnquoted(Tokenizer* t);
State HtmlTextCdata(Tokenizer* t);
State HtmlTextCdataOpenInside(Tokenizer* t);
State HtmlTextCdataClose(Tokenizer* t);
State HtmlTextCdataEnd(Tokenizer* t);
State HtmlTextCommentOpenInside(Tokenizer* t);
State HtmlTextComment(Tokenizer* t);
State HtmlTextCommentClose(Tokenizer* t);
State HtmlTextCommentEnd(Tokenizer* t);
State HtmlTextDeclaration(Tokenizer* t);
State HtmlTextEnd(Tokenizer* t);
State HtmlTextInstruction(Tokenizer* t);
State HtmlTextInstructionClose(Tokenizer* t);
State HtmlTextLineEndingBefore(Tokenizer* t);
State HtmlTextLineEndingAfter(Tokenizer* t);
State HtmlTextLineEndingAfterPrefix(Tokenizer* t);
State LabelStart(Tokenizer* t);
State LabelAtBreak(Tokenizer* t);
State LabelEolAfter(Tokenizer* t);
State LabelEscape(Tokenizer* t);
State LabelInside(Tokenizer* t);
State LabelNok(Tokenizer* t);
State LabelEndStart(Tokenizer* t);
State LabelEndAfter(Tokenizer* t);
State LabelEndResourceStart(Tokenizer* t);
State LabelEndResourceBefore(Tokenizer* t);
State LabelEndResourceOpen(Tokenizer* t);
State LabelEndResourceDestinationAfter(Tokenizer* t);
State LabelEndResourceDestinationMissing(Tokenizer* t);
State LabelEndResourceBetween(Tokenizer* t);
State LabelEndResourceTitleAfter(Tokenizer* t);
State LabelEndResourceEnd(Tokenizer* t);
State LabelEndOk(Tokenizer* t);
State LabelEndNok(Tokenizer* t);
State LabelEndReferenceFull(Tokenizer* t);
State LabelEndReferenceFullAfter(Tokenizer* t);
State LabelEndReferenceFullMissing(Tokenizer* t);
State LabelEndReferenceNotFull(Tokenizer* t);
State LabelEndReferenceCollapsed(Tokenizer* t);
State LabelEndReferenceCollapsedOpen(Tokenizer* t);
State LabelStartImageStart(Tokenizer* t);
State LabelStartImageOpen(Tokenizer* t);
State LabelStartImageAfter(Tokenizer* t);
State LabelStartLinkStart(Tokenizer* t);
State ListItemStart(Tokenizer* t);
State ListItemBefore(Tokenizer* t);
State ListItemBeforeOrdered(Tokenizer* t);
State ListItemBeforeUnordered(Tokenizer* t);
State ListItemValue(Tokenizer* t);
State ListItemMarker(Tokenizer* t);
State ListItemMarkerAfter(Tokenizer* t);
State ListItemAfter(Tokenizer* t);
State ListItemMarkerAfterFilled(Tokenizer* t);
State ListItemWhitespace(Tokenizer* t);
State ListItemPrefixOther(Tokenizer* t);
State ListItemWhitespaceAfter(Tokenizer* t);
State ListItemContStart(Tokenizer* t);
State ListItemContBlank(Tokenizer* t);
State ListItemContFilled(Tokenizer* t);
State NonLazyContinuationStart(Tokenizer* t);
State NonLazyContinuationAfter(Tokenizer* t);
State ParagraphStart(Tokenizer* t);
State ParagraphLineStart(Tokenizer* t);
State ParagraphInside(Tokenizer* t);
State RawFlowStart(Tokenizer* t);
State RawFlowBeforeSequenceOpen(Tokenizer* t);
State RawFlowSequenceOpen(Tokenizer* t);
State RawFlowInfoBefore(Tokenizer* t);
State RawFlowInfo(Tokenizer* t);
State RawFlowMetaBefore(Tokenizer* t);
State RawFlowMeta(Tokenizer* t);
State RawFlowAtNonLazyBreak(Tokenizer* t);
State RawFlowCloseStart(Tokenizer* t);
State RawFlowBeforeSequenceClose(Tokenizer* t);
State RawFlowSequenceClose(Tokenizer* t);
State RawFlowAfterSequenceClose(Tokenizer* t);
State RawFlowContentBefore(Tokenizer* t);
State RawFlowContentStart(Tokenizer* t);
State RawFlowBeforeContentChunk(Tokenizer* t);
State RawFlowContentChunk(Tokenizer* t);
State RawFlowAfter(Tokenizer* t);
State RawTextStart(Tokenizer* t);
State RawTextSequenceOpen(Tokenizer* t);
State RawTextBetween(Tokenizer* t);
State RawTextData(Tokenizer* t);
State RawTextSequenceClose(Tokenizer* t);
State SpaceOrTabStart(Tokenizer* t);
State SpaceOrTabInside(Tokenizer* t);
State SpaceOrTabAfter(Tokenizer* t);
State SpaceOrTabEolStart(Tokenizer* t);
State SpaceOrTabEolAfterFirst(Tokenizer* t);
State SpaceOrTabEolAfterEol(Tokenizer* t);
State SpaceOrTabEolAtEol(Tokenizer* t);
State SpaceOrTabEolAfterMore(Tokenizer* t);
State StringStart(Tokenizer* t);
State StringBefore(Tokenizer* t);
State StringBeforeData(Tokenizer* t);
State TextStart(Tokenizer* t);
State TextBefore(Tokenizer* t);
State TextBeforeHtml(Tokenizer* t);
State TextBeforeHardBreakEscape(Tokenizer* t);
State TextBeforeLabelStartLink(Tokenizer* t);
State TextBeforeData(Tokenizer* t);
State ThematicBreakStart(Tokenizer* t);
State ThematicBreakBefore(Tokenizer* t);
State ThematicBreakSequence(Tokenizer* t);
State ThematicBreakAtBreak(Tokenizer* t);
State TitleStart(Tokenizer* t);
State TitleBegin(Tokenizer* t);
State TitleAfterEol(Tokenizer* t);
State TitleAtBreak(Tokenizer* t);
State TitleEscape(Tokenizer* t);
State TitleInside(Tokenizer* t);
State TitleNok(Tokenizer* t);

constexpr int32_t kSizeMax = 0x7fffffff;

struct SpaceOrTabOptions {
    int32_t min = 0;
    int32_t max = 0;
    Name kind = Name::SpaceOrTab;
    bool connect = false;
    ContentKind content = ContentKind::Flow;
    bool contentSome = false;
};

StateName SpaceOrTab(Tokenizer* t);
StateName SpaceOrTabMinMax(Tokenizer* t, int32_t min, int32_t max);
StateName SpaceOrTabWithOptions(Tokenizer* t, const SpaceOrTabOptions& options);

struct SpaceOrTabEolOptions {
    bool connect = false;
    ContentKind content = ContentKind::Flow;
    bool contentSome = false;
};

StateName SpaceOrTabEol(Tokenizer* t);
StateName SpaceOrTabEolWithOptions(Tokenizer* t,
                                   const SpaceOrTabEolOptions& options);

void ResolveWhitespace(Tokenizer* t, bool hardBreak, bool trimWhole);

void GfmAutolinkLiteralResolve(Tokenizer* t);

bool LabelEndResolve(Tokenizer* t, Subresult* out);
bool AttentionResolve(Tokenizer* t, Subresult* out);
bool GfmTableResolve(Tokenizer* t, Subresult* out);
bool HeadingAtxResolve(Tokenizer* t, Subresult* out);
bool HeadingSetextResolve(Tokenizer* t, Subresult* out);
bool ListItemResolve(Tokenizer* t, Subresult* out);
bool ContentResolve(Tokenizer* t, Subresult* out);
bool DataResolve(Tokenizer* t, Subresult* out);
bool StringResolve(Tokenizer* t, Subresult* out);
bool TextResolve(Tokenizer* t, Subresult* out);

}

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/markdown-mini/markdown.h"

#endif

#line 1 "src/shell/a11y.h"

namespace gpui::shell {

AccessibilityRole AccessibilityRoleFromName(Str name);
int AccessibilityRoleNameCount();

}

#line 1 "src/shell/action.h"

namespace gpui::shell {

uint32_t ShellActionOf(Str id);

Str ShellActionScriptId(uint32_t action);

const char* ShellActionInternText(Str value);

}

#line 1 "src/shell/filesystem.h"

namespace gpui::shell {

constexpr int kFsMaxReadBytes = 64 * 1024 * 1024;
constexpr int kFsMaxWriteBytes = 8 * 1024 * 1024;
constexpr int kFsMaxDirectoryEntries = 10000;
constexpr int kFsMaxDirectoryNameBytes = 1024 * 1024;

enum class FsOperation : uint8_t {
    Read,
    Write,
    ReadDirectory,
    Exists,
    RemoveFile,
    RemoveDirectory,
    MakeDirectory,
};

struct FsEntry {
    Str name;
    bool isDirectory = false;
};

struct FsResult {
    Str bytes;
    Vec<FsEntry> entries;
    bool exists = false;

    void Free();
};

bool FsRun(FsOperation operation, Str root, Str relative, Str input,
           bool recursive, FsResult* result, Str* error);

}

#line 1 "src/shell/assets.h"

namespace gpui {

constexpr int kShellMaxAssetBytes = 16 * 1024 * 1024;
constexpr int kShellMaxReportedMissingAssets = 256;

struct AppAssets {
    Str root;
    Vec<Str> missing;
    int source = 0;

    explicit AppAssets(Str root);
    AppAssets(const AppAssets&) = delete;
    AppAssets& operator=(const AppAssets&) = delete;
    ~AppAssets();

    bool Install();
    void Uninstall();
    bool Load(Str path, Vec<uint8_t>* out, Str* error = nullptr);
    bool Exists(Str path);
    bool List(Str path, Vec<Str>* out, Str* error = nullptr);
    bool Resolve(Str path, Str* relative, Str* error = nullptr) const;
};

}

#line 1 "src/shell/capability.h"

namespace gpui {

enum class ExecuteGrantKind : uint8_t {
    Denied,
    Allowed,
    Unrestricted,
};

struct ExecuteGrant {
    ExecuteGrantKind kind = ExecuteGrantKind::Denied;
    Vec<Str> commands;

    ExecuteGrant();
    ExecuteGrant(const ExecuteGrant& other);
    ExecuteGrant& operator=(const ExecuteGrant& other);
    ~ExecuteGrant();

    static ExecuteGrant Denied();
    static ExecuteGrant Allowed(const Str* commands, int count);
    static ExecuteGrant Unrestricted();
    bool Allows(Str command) const;
};

struct HttpRequestGrant {
    Str scheme;
    Str host;
    uint16_t port = 0;
    bool hasPort = false;
    Vec<Str> methods;
    Vec<Str> paths;
    Vec<Str> pathPrefixes;

    HttpRequestGrant();
    explicit HttpRequestGrant(Str host);
    HttpRequestGrant(const HttpRequestGrant& other);
    HttpRequestGrant& operator=(const HttpRequestGrant& other);
    ~HttpRequestGrant();

    HttpRequestGrant& Scheme(Str value);
    HttpRequestGrant& Port(uint16_t value);
    HttpRequestGrant& AddMethod(Str value);
    HttpRequestGrant& AddPath(Str value);
    HttpRequestGrant& AddPathPrefix(Str value);
    bool Allows(Str requestScheme, Str requestHost, uint16_t requestPort,
                bool requestHasPort, Str method, Str path) const;
};

enum class CapabilityAccess : uint8_t {
    Read,
    Write,
};

enum class CapabilityErrorKind : uint8_t {
    None,
    NotGranted,
    OutsideRoots,
    ExecuteDenied,
    StorageDenied,
};

struct CapabilityError {
    CapabilityErrorKind kind = CapabilityErrorKind::None;
    CapabilityAccess access = CapabilityAccess::Read;
    Str subject;
};

void CapabilityErrorFree(CapabilityError* error);
Str CapabilityErrorMessage(Arena* arena, const CapabilityError& error);

bool IsOpenableUrl(Str url);

struct CapabilityPath {
    Str root;
    Str relative;

    void Free();
};

class Capabilities {
  public:
    Capabilities();
    Capabilities(const Capabilities& other);
    Capabilities& operator=(const Capabilities& other);
    ~Capabilities();

    Capabilities& AddReadRoot(Str root);
    Capabilities& AddWriteRoot(Str root);
    Capabilities& SetExecute(const ExecuteGrant& grant);
    Capabilities& AddNetworkHost(Str host);
    Capabilities& AddHttpRequest(const HttpRequestGrant& grant);
    Capabilities& Storage(bool allowed);
    Capabilities& ClipboardRead(bool allowed);
    Capabilities& ClipboardWrite(bool allowed);
    Capabilities& Exit(bool allowed);

    bool HasReadAccess() const;
    bool HasWriteAccess() const;
    bool HasStorage() const;
    bool IsClipboardReadable() const;
    bool IsClipboardWritable() const;
    bool MayExit() const;
    bool MayRun(Str command) const;
    bool MayReach(Str host) const;
    bool MayRequest(Str scheme, Str host, uint16_t port, bool hasPort,
                    Str method, Str path) const;

    bool ResolvePath(Str path, CapabilityAccess access, CapabilityPath* out,
                     CapabilityError* error = nullptr) const;

  private:
    Vec<Str> readRoots;
    Vec<Str> writeRoots;
    ExecuteGrant execute;
    Vec<Str> networkHosts;
    Vec<HttpRequestGrant*> httpRequests;
    bool storage = false;
    bool clipboardRead = false;
    bool clipboardWrite = false;
    bool exit = false;

    void Clear();
    void CopyFrom(const Capabilities& other);
};

}

#line 1 "src/shell/metrics.h"

namespace gpui {

struct RuntimeMetrics {
    uint64_t scriptRenders = 0;
    uint64_t scriptRenderNanos = 0;
    uint64_t slowestScriptRenderNanos = 0;
    uint64_t nativeNanos = 0;

    uint64_t frameScriptCalls = 0;
    uint64_t materializations = 0;
    uint64_t materializeNanos = 0;

    uint64_t structureRepeats = 0;

    uint64_t structureChanges = 0;

    bool StructureRepeatRate(double* out) const;
    uint64_t MeanScriptOnlyNanos() const;
    uint64_t MeanNativeNanos() const;
    uint64_t MeanScriptRenderNanos() const;
    uint64_t MeanMaterializeNanos() const;
    RuntimeMetrics Since(const RuntimeMetrics& earlier) const;
};

namespace shell {

struct Metrics {
    RuntimeMetrics value;
};

enum class MetricsTimerKind : uint8_t {
    ScriptRender,
    Native,

    FrameScript,
    Materialize,
};

struct MetricsTimer {
    Metrics* metrics = nullptr;
    MetricsTimerKind kind = MetricsTimerKind::ScriptRender;
    double started = 0;
};

MetricsTimer MetricsBegin(Metrics* metrics, MetricsTimerKind kind);
void MetricsEnd(MetricsTimer* timer);
void MetricsAdd(Metrics* metrics, MetricsTimerKind kind, uint64_t nanos);
RuntimeMetrics MetricsRead(const Metrics* metrics);

void MetricsRecordStructure(Metrics* metrics, bool repeated);

}
}

#line 1 "src/shell/host_modules.h"

namespace gpui {

enum class HostValueKind : uint8_t {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
};

struct HostValue;

struct HostField {
    Str name;
    HostValue* value = nullptr;
};

struct HostValue {
    HostValueKind kind = HostValueKind::Null;
    bool boolean = false;
    double number = 0;
    Str string;
    Vec<HostValue*> array;
    Vec<HostField> object;

    void Free();
    bool CopyFrom(const HostValue& other);
    void SetNull();
    void SetBool(bool value);
    void SetNumber(double value);
    bool SetString(Str value);
    bool Append(const HostValue& value);
    bool SetField(Str name, const HostValue& value);
    const HostValue* Get(Str name) const;
    const char* Describe() const;
};

struct HostError {
    Str message;

    bool IsSet() const { return message.s != nullptr; }
    void Set(Str value);
    void Clear();
};

struct HostArguments {
    Vec<HostValue*> values;

    void Free();
    int Len() const { return len(values); }
    const HostValue* Get(int index) const;
    bool Value(int index, const HostValue** value, HostError* error) const;
    bool String(int index, Str* value, HostError* error) const;
    bool Number(int index, double* value, HostError* error) const;
    bool Integer(int index, int64_t* value, HostError* error) const;
    bool Boolean(int index, bool* value, HostError* error) const;
};

struct HostCall {
    const HostArguments* arguments = nullptr;
    HostValue result;
    HostError error;

    void Fail(Str message) { error.Set(message); }
};

struct HostModules;

struct HostAsyncRequest {
    const HostArguments* arguments = nullptr;
    Func1<HostCall*> work;
    Func0 release;
    HostModules* registry = nullptr;
    HostError error;
};

class HostModule {
  public:
    static HostModule* New(Str name);
    HostModule* Retain();
    void Release();

    HostModule* Function(Str name, Func1<HostCall*> body, Func0 release = {});
    HostModule* AsyncFunction(Str name, Func1<HostCall*> work,
                              Func0 release = {});
    HostModule* AsyncFunction(Str name, Func1<HostAsyncRequest*> begin,
                              Func0 release = {});
    HostModule* Declarations(Str typescript);

    Str Name() const { return name; }
    Str Declared() const { return declarations; }
    int FunctionCount() const { return functions.len; }
    Str FunctionName(int index) const;
    bool Has(Str function) const;
    bool IsAsync(Str function) const;
    bool Validate(HostError* error = nullptr) const;
    bool Call(Str function, HostCall* call) const;
    bool Begin(Str function, HostAsyncRequest* request) const;

  private:
    struct FunctionEntry;
    uint32_t refs = 1;
    Str name;
    Str declarations;
    Vec<FunctionEntry*> functions;

    explicit HostModule(Str name);
    ~HostModule();
    FunctionEntry* Find(Str function) const;
    HostModule* SetFunction(Str name, bool async, Func1<HostCall*> body,
                            Func1<HostAsyncRequest*> begin, Func0 release);
};

HostModules* HostModulesNew();
HostModules* HostModulesRetain(HostModules* modules);
void HostModulesRelease(HostModules* modules);
HostModules* HostModulesClone(HostModules* modules);
uint64_t HostModulesGeneration(const HostModules* modules);
int HostModulesCount(const HostModules* modules);
HostModule* HostModulesAt(const HostModules* modules, int index);
HostModule* HostModulesGet(const HostModules* modules, Str name);
bool HostModulesInsert(HostModules* modules, HostModule* module);

bool ShellExportModule(HostModule* module, HostError* error = nullptr);
void ShellClearExportedModules();

bool HostDispatch(Str module, Str function, HostCall* call);
bool HostDispatchBegin(Str module, Str function, HostAsyncRequest* request);
bool HostIsIdentifier(Str name);
bool HostIsReservedSpecifier(Str name);

}

#line 1 "src/shell/storage.h"

namespace gpui::shell {

constexpr int kMaxStorageBytes = 8 * 1024 * 1024;
constexpr int kMaxStorageKeys = 4096;
constexpr int kMaxStorageValueBytes = 1024 * 1024;
constexpr int kMaxStorageWaiters = 1024;

bool StorageReplaceFile(Str temporary, Str path, Str* error);

struct StorageEntry {
    Str key;
    Str value;
};

struct StorageWrite {
    uint64_t revision = 0;
    Str path;
    Str body;

    void Free();
};

struct StorageOutcome {
    bool ok = false;
    Str error;
};

struct StorageWaiter {
    uint64_t revision = 0;
    Func1<StorageOutcome> settle;
};

class Storage {
  public:
    explicit Storage(bool persisted = false);
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;
    ~Storage();

    bool SetPath(Str path, Str* error = nullptr);
    Str Get(Str key) const;
    bool Set(Str key, Str value, Str* error = nullptr);
    bool Remove(Str key, Str* error = nullptr);
    bool Clear(Str* error = nullptr);
    int Len() const { return entries.len; }
    Str Key(int index) const;
    bool HasPath() const { return path.s != nullptr; }

    uint64_t Revision() const { return revision; }
    uint64_t WrittenRevision() const { return written; }
    bool IsDirty() const;
    bool HasWriteInFlight() const { return inFlight != 0; }
    bool BeginWrite(StorageWrite* write, Str* error = nullptr);
    void FinishWrite(uint64_t writeRevision, bool ok,
                     Vec<StorageWaiter*>* ready);
    void AbortWrite(uint64_t writeRevision);
    bool Wait(Func1<StorageOutcome> settle, StorageWaiter** waiter,
              bool* immediate, Str* error = nullptr);
    void CancelWaiter(StorageWaiter* waiter);

  private:
    Vec<StorageEntry*> entries;
    Vec<StorageWaiter*> waiters;
    Str path;
    bool persisted = false;
    uint64_t revision = 0;
    uint64_t written = 0;
    uint64_t inFlight = 0;
    uint64_t failed = 0;

    bool Load(Str* error);
    bool Encode(Str* body, Str* error) const;
    bool ValidateEncoded(Str* error) const;
    void Touch();
    void ResetEntries();
    void ReadyThrough(uint64_t through, Vec<StorageWaiter*>* ready);
};

bool StoragePersist(const StorageWrite& write, Str* error = nullptr);

}

#line 1 "src/shell/policy.h"

namespace gpui {

struct Policy;

Policy* PolicyNew();
Policy* PolicyNew(const Capabilities& capabilities);
Policy* PolicyRetain(Policy* policy);
void PolicyRelease(Policy* policy);
const Capabilities& PolicyCapabilities(const Policy* policy);

Str PolicyApplication(const Policy* policy);
void PolicySetApplication(Policy* policy, Str name);

void PolicyUpdateDefaultApplication(Str name);

Policy* PolicyDefault();
void PolicySetDefault(Policy* policy);
void PolicyUpdateDefaultCapabilities(const Capabilities& capabilities);
shell::Storage* PolicyStorage(Policy* policy, bool session);
bool PolicySetStoragePath(Policy* policy, Str path, Str* error = nullptr);
bool ShellSetStoragePath(Str path, Str* error = nullptr);
HostModules* PolicyHostModules(Policy* policy);
bool PolicyAddHostModule(Policy* policy, HostModule* module,
                         HostError* error = nullptr);
void PolicyClearHostModules(Policy* policy);

}

#line 1 "src/shell/retained.h"

namespace gpui::shell {

using EntityHandle = uint64_t;

const int kMaxLiveEntities = 10000;

struct ScriptDockSkin;

enum class RetainedKind : uint8_t {
    Input,
    Textarea,
    Slider,
    Otp,

    Calendar,
    Focus,

    Dock,
    VirtualScroll,
};

enum class RetainedEvent : uint8_t {
    InputChange,
    InputSubmit,
    InputFocus,
    InputBlur,
    SliderChange,
    SliderRelease,
    OtpChange,
    OtpComplete,
    OtpFocus,
    OtpBlur,
    CalendarChange,

    DockLayoutChanged,
};

enum class DockChromeSlot : uint8_t {
    TabBar,
    EmptyGroup,

    DropIndicator,
    Dock,
};

struct DockChromeHooks {
    CallbackId tabBar = 0;
    CallbackId emptyGroup = 0;
    CallbackId dropIndicator = 0;
    CallbackId dock = 0;
};

struct RetainedCallback {
    RetainedEvent event = RetainedEvent::InputChange;
    CallbackId callback = 0;
};

struct NumberInputConfig {
    bool hasStep = false;
    double step = 1;
    bool hasMin = false;
    double min = 0;
    bool hasMax = false;
    double max = 0;
};

struct RetainedEntry {
    uint32_t id = 0;
    RetainedKind kind = RetainedKind::Input;
    EntityId owner = {};
    void* application = nullptr;
    App* app = nullptr;
    InputState* input = nullptr;
    SliderState* slider = nullptr;
    Entity<OtpState> otp = {};
    FocusHandle focus = {};
    VirtualListScrollHandle scroll = {};
    Entity<CalendarState> calendar = {};
    Entity<DockState> dock = {};

    ScriptDockSkin* dockSkin = nullptr;

    Arena* dockArena = nullptr;

    Subscription subscription = {};
    DockChromeHooks dockHooks = {};
    NumberInputConfig number = {};
    Vec<RetainedCallback> callbacks;
};

class RetainedStore {
  public:
    RetainedStore();
    RetainedStore(const RetainedStore&) = delete;
    RetainedStore& operator=(const RetainedStore&) = delete;
    ~RetainedStore();

    int Len() const { return entries.len; }
    uint32_t Checkpoint() const { return nextId; }

    EntityHandle CreateInput(bool textarea, Str placeholder, Str value,
                             int rows, App* app, EntityId owner,
                             void* application);
    EntityHandle CreateSlider(float min, float max, float step,
                              SliderScale scale, SliderValue value, App* app,
                              EntityId owner, void* application);
    EntityHandle CreateOtp(int length, Str value, bool masked, App* app,
                           EntityId owner, void* application);
    EntityHandle CreateCalendar(Ctx* cx, EntityId owner, void* application);

    EntityHandle CreateDock(Str id, bool hasVersion, int version, Ctx* cx,
                            EntityId owner, void* application);
    EntityHandle CreateFocus(App* app, EntityId owner, void* application);
    EntityHandle CreateVirtualScroll(App* app, EntityId owner,
                                     void* application);

    RetainedEntry* Find(EntityHandle handle) const;
    RetainedEntry* FindLocal(uint32_t id) const;
    bool AddCallback(EntityHandle handle, RetainedEvent event,
                     CallbackId callback, bool replace,
                     CallbackId* replaced = nullptr);
    bool Release(EntityHandle handle, Vec<CallbackId>* callbacks = nullptr);
    void ReleaseOwner(EntityId owner, Vec<CallbackId>* callbacks = nullptr);
    void ReleaseApplication(void* application,
                            Vec<CallbackId>* callbacks = nullptr);
    void Rollback(uint32_t checkpoint, Vec<CallbackId>* callbacks = nullptr);
    void Clear(Vec<CallbackId>* callbacks = nullptr);

  private:
    uint32_t storeId = 0;
    uint32_t nextId = 1;
    Vec<RetainedEntry*> entries;

    EntityHandle Push(RetainedEntry* entry);
    bool Belongs(EntityHandle handle, uint32_t* id) const;
    static void Destroy(RetainedEntry* entry, Vec<CallbackId>* callbacks);
};

}

#line 1 "src/shell/snapshot.h"

namespace gpui {

struct SnapshotRuntimeLease {
    void* state = nullptr;
    void (*retain)(void* state) = nullptr;
    void (*release)(void* state) = nullptr;
    void (*retireCallbacks)(void* state, uint64_t generation) = nullptr;
};

class RenderSnapshot {
  public:
    RenderSnapshot(uint64_t generation, shell::SpecId root,
                   shell::SpecArena* arena, SnapshotRuntimeLease runtime = {});
    RenderSnapshot(const RenderSnapshot&) = delete;
    RenderSnapshot& operator=(const RenderSnapshot&) = delete;
    ~RenderSnapshot();

    uint64_t Generation() const { return generation; }
    shell::SpecId Root() const { return root; }
    const shell::SpecArena* Specs() const { return arena; }
    int Len() const;

    shell::StructureFingerprint Structure() const;
    bool IsEmpty() const;
    Str DebugTree(Arena* into) const;

  private:
    uint64_t generation = 0;
    shell::SpecId root = 0;
    shell::SpecArena* arena = nullptr;
    SnapshotRuntimeLease runtime;
};

}

#line 1 "src/shell/runtime.h"

namespace gpui {

struct ViewType;
struct ViewObject;
struct ScriptView;
struct LoadedApplication;
struct ShellRuntimeImpl;
struct ShellRuntimeControl;
struct ShellRuntimeAccess;
}
namespace gpui::shell {
struct MaterializedDependencies;
class FrozenComponentRegistry;
struct ComponentAppEffectInstall;
}
namespace gpui {
struct ShellTaskDriver;
struct InlineTokenContext;
struct InlineTokenClickEvent;
struct InlineTokenHoverEvent;

class ShellRuntime {
  public:

    static ShellRuntime* New(
        App* app = nullptr, ShellError* error = nullptr,
        const shell::FrozenComponentRegistry* components = nullptr);
    const shell::FrozenComponentRegistry* Components() const;

    void* ComponentState(uint64_t handle, const char* kind,
                         Str* error = nullptr, Arena* a = nullptr) const;

    Str LastComponentFailure() const;
    void NoteComponentFailure(Str message);

    uint64_t ComponentFailureCount() const;
    ShellRuntime* Retain();
    void Release();

    LoadedApplication* LoadApplication(Str directory,
                                       Str entry = StrL("main.js"),
                                       ShellError* error = nullptr);

    Entity<ScriptView> MountApplication(LoadedApplication* application,
                                        Window* window, App* app,
                                        ShellError* error = nullptr);

    ViewType* LoadSource(Str name, Str source, ShellError* error = nullptr);
    ViewType* LoadSource(Str name, Str source, Policy* policy,
                         ShellError* error = nullptr);
    ViewType* LoadApp(Str directory, Str entry = StrL("main.js"),
                      ShellError* error = nullptr);
    ViewType* LoadApp(Str directory, Str entry, Policy* policy,
                      ShellError* error = nullptr);

    ViewType* ReloadApp(Str directory, Str entry, Policy* policy,
                        const shell::MaterializedDependencies* reuse,
                        ShellError* error = nullptr);

    void SetDependencyCacheRoot(Str root);
    ViewObject* Instantiate(ViewType* type, Window* window, App* app,
                            Policy* policy = nullptr,
                            ShellError* error = nullptr, EntityId view = {});
    RenderSnapshot* BuildSnapshot(ViewObject* object, Window* window, App* app,
                                  EntityId view = {}, Policy* policy = nullptr,
                                  ShellError* error = nullptr);
    Str RenderToSpec(Arena* into, ViewObject* object, Window* window, App* app,
                     EntityId view = {}, Policy* policy = nullptr,
                     ShellError* error = nullptr);

    bool Eval(Str source, Str name = StrL("<eval>"),
              ShellError* error = nullptr);
    bool DrainJobs(int limit = 1024, ShellError* error = nullptr);
    RuntimeMetrics ReadMetrics() const;
    void RecordMaterialize(uint64_t nanos);
    void RecordStructure(bool repeated);
    int LiveCallbacks() const;
    int LiveEntities() const;
    int LiveNestedViews() const;
    int LiveTasks() const;
    int LiveTemplates() const;
    shell::RetainedEntry* Retained(shell::EntityHandle handle) const;
    EntityId NestedView(shell::EntityHandle handle, App* app) const;

    void RegisterScriptView(EntityId view, bool* dirty);
    void UnregisterScriptView(EntityId view, bool* dirty);
    void InvalidateScriptView(EntityId view);
    void ReleaseOwnedEntities(EntityId view);
    void ReleaseApplicationState(ViewObject* object);

    void* ScriptViewApplication(EntityId view, App* app) const;
    bool ScheduleComponentAppEffect(
        void* application, EntityId view, Str key, Str revision, Window* window,
        App* app, const shell::ComponentAppEffectInstall& install, Str* error,
        Arena* a);

    void ApplyComponentAppEffect(uint64_t token, App* app);

    void CleanupComponentAppEffects(EntityId view);

    void DispatchClick(shell::CallbackId callback, const ClickEvent& event,
                       Window* window, App* app);
    void DispatchMouseMove(shell::CallbackId callback,
                           const MouseMoveEvent& event, Window* window,
                           App* app);
    void DispatchChange(shell::CallbackId callback, bool value, Window* window,
                        App* app);
    void DispatchIndex(shell::CallbackId callback, uint32_t value,
                       Window* window, App* app);
    void DispatchNumbers(shell::CallbackId callback, const float* values,
                         int count, Window* window, App* app);
    void DispatchString(shell::CallbackId callback, Str value, Window* window,
                        App* app);
    void DispatchSignal(shell::CallbackId callback, Window* window, App* app);

    void DispatchKey(shell::CallbackId callback, const KeyEvent& event,
                     bool* propagate, Window* window, App* app);
    void DispatchMouseButton(shell::CallbackId callback, MouseButton button,
                             float x, float y, int clickCount,
                             Modifiers modifiers, Bounds bounds, bool hasBounds,
                             Window* window, App* app);
    void DispatchScrollWheel(shell::CallbackId callback,
                             const ScrollWheelEvent& event, Bounds bounds,
                             bool hasBounds, bool* propagate, Window* window,
                             App* app);
    void DispatchAction(shell::CallbackId callback, Str action, bool* propagate,
                        Window* window, App* app);
    void DispatchCalendarEvent(shell::EntityHandle handle,
                               const CalendarEvent& event, Window* window,
                               App* app);

    El* DescribeDockChrome(Ctx* cx, shell::EntityHandle dock,
                           shell::DockChromeSlot slot, uint64_t key,
                           shell::CallbackId handler, Str payload);
    void DispatchItemSecondaryClick(shell::CallbackId callback, Str key,
                                    const MouseDownEvent& event, Window* window,
                                    App* app);
    void DispatchInputEvent(shell::EntityHandle handle, const InputEvent& event,
                            Window* window, App* app);
    void DispatchSliderEvent(shell::EntityHandle handle,
                             const SliderEvent& event, Window* window,
                             App* app);
    void DispatchOtpEvent(shell::EntityHandle handle, const OtpEvent& event,
                          Window* window, App* app);
    void RenderVirtualItems(shell::CallbackId render, shell::CallbackId getKey,
                            shell::CallbackId onItemClick,
                            shell::CallbackId onItemSecondaryClick, int first,
                            int end, Ctx* cx, El** out);

    void BeginTokenFrame();
    El* RenderInlineToken(shell::CallbackId render,
                          const InlineTokenContext* ctx, Str text, Ctx* cx);
    void DispatchTokenClick(shell::CallbackId click,
                            const InlineTokenClickEvent* ev, Str text, Ctx* cx);
    void DispatchTokenHover(shell::CallbackId hover,
                            const InlineTokenHoverEvent* ev, Str text, Ctx* cx);

  private:
    friend struct ShellRuntimeAccess;
    friend struct ShellTaskDriver;
    friend void ShellRuntimeRetireSnapshot(void*, uint64_t);
    void ResumeTask(uint32_t id, Ctx* cx);
    ShellRuntime();
    ~ShellRuntime();

    uint32_t refs = 1;
    ShellRuntimeImpl* impl = nullptr;
    ShellRuntimeControl* control = nullptr;
};

void LoadedApplicationFree(LoadedApplication* application);

ViewType* ViewTypeRetain(ViewType* type);
void ViewTypeRelease(ViewType* type);

const shell::MaterializedDependencies* ViewTypeDependencies(ViewType* type);
ViewObject* ViewObjectRetain(ViewObject* object);
void ViewObjectRelease(ViewObject* object);
void ShellSetDevelopmentMode(bool enabled);
bool ShellDevelopmentMode();

struct ShellExitRequest {
    int code = 0;
    EntityId view = {};
};
using ShellExitHandler = void (*)(const ShellExitRequest&, Ctx*);
void ShellOnExitRequest(ShellExitHandler handler);

void ShellInit(App* app);

void ShellInitWithComponents(App* app,
                             const shell::FrozenComponentRegistry* components);

}

#line 1 "src/shell/materialize.h"

namespace gpui {

El* ShellMaterialize(Ctx* cx, ShellRuntime* runtime,
                     const RenderSnapshot* snapshot,
                     ShellError* error = nullptr);
El* ShellMaterializeSpec(Ctx* cx, ShellRuntime* runtime,
                         const shell::SpecArena* specs, shell::SpecId root,
                         ShellError* error = nullptr);

El* ShellTryMaterialize(Ctx* cx, ShellRuntime* runtime,
                        const RenderSnapshot* snapshot, ShellError* error);

void ShellApplyNodeStyle(Ctx* cx, const shell::SpecArena* specs,
                         shell::SpecId id, El* target,
                         ShellError* error = nullptr);

namespace shell {

bool TextViewImageUrl(const Capabilities& capabilities, Str url);

struct TextViewImageResponse {
    bool ok = false;

    Str bytes = {};
};

constexpr double kTextViewImageTimeoutSecs = 30;

bool TextViewImageRequest(const Capabilities& capabilities, Str url,
                          Func1<TextViewImageResponse> done,
                          double deadline = 0);

bool TextViewSvgReferencesFile(Str bytes);

}
}

#line 1 "src/shell/view.h"

namespace gpui {

struct ResizablePanelEvent;

struct ShellBoolBinding {
    shell::CallbackId callback = 0;
    bool value = false;
};

struct ShellStringBinding {
    shell::CallbackId callback = 0;
    Str value;
};

struct ShellSelectBinding {
    shell::CallbackId onOpenChange = 0;
    shell::CallbackId onConfirm = 0;
    shell::CallbackId onDismiss = 0;
    bool open = false;
    bool disabled = false;
    FocusHandle triggerFocus = {};
    FocusHandle contentFocus = {};
};

struct ShellNumberBinding {
    InputState* state = nullptr;
    NumberStep step = {};
    bool hasStep = false;
    bool hasMin = false;
    double min = 0;
    bool hasMax = false;
    double max = 0;
    bool disabled = false;
    shell::CallbackId onStep = 0;
};

struct ShellMouseButtonBinding {
    shell::CallbackId left = 0;
    shell::CallbackId right = 0;
    shell::CallbackId middle = 0;
};

struct ShellActionBinding {
    uint32_t action = 0;
    shell::CallbackId callback = 0;
};

struct ShellScrollPosition {
    float x = 0;
    float y = 0;
};

struct ScriptView {
    ShellRuntime* runtime = nullptr;
    ViewType* type = nullptr;
    ViewObject* object = nullptr;
    RenderSnapshot* snapshot = nullptr;
    Policy* policy = nullptr;
    ShellError error = {};
    EntityId self = {};

    uint32_t themeRevision = 0;
    bool dirty = true;

    ~ScriptView();

    static Entity<ScriptView> New(App* app, ShellRuntime* runtime,
                                  ViewType* type, Policy* policy = nullptr);
    static El* Render(ScriptView* self, Ctx* cx);
    static void Refresh(ScriptView* self, Ctx* cx);
    static bool Reload(ScriptView* self, Ctx* cx, Str directory, Str entry,
                       ShellError* error = nullptr);

    static void OnClick(ScriptView* self, Ctx* cx, const ClickEvent* event,
                        int64_t callback);
    static void OnTextLink(ScriptView* self, Ctx* cx, const ClickEvent* event,
                           int64_t binding);

    static void OnImageDeadline(ScriptView* self, Ctx* cx,
                                const TickEvent* event);
    static void OnChange(ScriptView* self, Ctx* cx, const ClickEvent* event,
                         int64_t value);
    static void OnHover(ScriptView* self, Ctx* cx, const HoverEvent* event,
                        int64_t callback);
    static void OnMouseMove(ScriptView* self, Ctx* cx,
                            const MouseMoveEvent* event, int64_t callback);
    static void OnOpenChange(ScriptView* self, Ctx* cx,
                             const PopoverOpenChangeEvent* event,
                             int64_t callback);
    static void OnResize(ScriptView* self, Ctx* cx,
                         const ResizablePanelEvent* event, int64_t callback);
    static void OnBoundBool(ScriptView* self, Ctx* cx, const void* event,
                            int64_t binding);
    static void OnBoundString(ScriptView* self, Ctx* cx,
                              const ClickEvent* event, int64_t binding);
    static void OnItemSecondaryPress(ScriptView* self, Ctx* cx,
                                     const MouseDownEvent* event,
                                     int64_t binding);
    static void OnSelectAction(ScriptView* self, Ctx* cx,
                               const ActionEvent* event, int64_t binding);

    static void OnSelectActivate(ScriptView* self, Ctx* cx,
                                 const ClickEvent* event, int64_t binding);
    static void OnNumberStep(ScriptView* self, Ctx* cx,
                             const NumberInputEvent* event, int64_t callback);
    static void OnNumberKey(ScriptView* self, Ctx* cx, const KeyEvent* event,
                            int64_t binding);
    static void OnInputEvent(ScriptView* self, Ctx* cx, const InputEvent* event,
                             int64_t handle);
    static void OnSliderEvent(ScriptView* self, Ctx* cx,
                              const SliderEvent* event, int64_t handle);
    static void OnOtpEvent(ScriptView* self, Ctx* cx, const OtpEvent* event,
                           int64_t handle);
    static void OnCalendarEvent(ScriptView* self, Ctx* cx,
                                const CalendarEvent* event, int64_t handle);

    static void OnDockEvent(ScriptView* self, Ctx* cx, const DockEvent* event,
                            int64_t callback);

    static void OnScriptKey(ScriptView* self, Ctx* cx, const KeyEvent* event,
                            int64_t callback);
    static void OnScriptMouseDown(ScriptView* self, Ctx* cx,
                                  const MouseDownEvent* event, int64_t binding);
    static void OnScriptMouseUp(ScriptView* self, Ctx* cx,
                                const MouseUpEvent* event, int64_t binding);

    static void OnScriptMouseDownOut(ScriptView* self, Ctx* cx,
                                     const MouseDownEvent* event,
                                     int64_t callback);

    static void OnScrollPosition(ScriptView* self, Ctx* cx,
                                 const ScrollEvent* event, int64_t position);
    static void OnScriptScrollWheel(ScriptView* self, Ctx* cx,
                                    const ScrollWheelEvent* event,
                                    int64_t callback);
    static void OnScriptAction(ScriptView* self, Ctx* cx,
                               const ActionEvent* event, int64_t binding);

    static void OnComponentEvent(ScriptView* self, Ctx* cx, const void* event,
                                 int64_t binding);

    static void OnComponentAppEffect(ScriptView* self, Ctx* cx,
                                     const void* event, int64_t token);

    static void OnDispatchAction(ScriptView* self, Ctx* cx,
                                 const ClickEvent* event, int64_t action);
};

}

#line 1 "src/shell/root.h"

namespace gpui {

struct FpsHudRequest {

    FpsAnchor anchor = FpsAnchor::TopRight;

    bool hasFrameBudget = false;
    float frameBudget = 0;
};

bool FpsAnchorFromName(Str name, FpsAnchor* out);
SeqStrings FpsAnchorNames();

struct DialogOptions {
    bool escapeDismissable = true;
    bool backdropDismissable = true;

    DialogOptions& EscapeDismissable(bool value) {
        escapeDismissable = value;
        return *this;
    }
    DialogOptions& BackdropDismissable(bool value) {
        backdropDismissable = value;
        return *this;
    }
};

enum class ToastLevel : uint8_t {
    Info,
    Success,
    Warning,
    Error,
};

const char* ToastLevelName(ToastLevel level);
bool ToastLevelFromName(Str name, ToastLevel* out);

constexpr int kToastDefaultTimeoutMs = 5000;

struct ToastRequest {
    Str title;
    Str description;
    ToastLevel level = ToastLevel::Info;

    bool hasTimeout = true;
    int timeoutMs = kToastDefaultTimeoutMs;

    bool hasId = false;
    Str id;
};

struct ShellToastId {
    uint32_t key = 0;
    Str text;

    bool operator==(const ShellToastId& other) const {
        return key == other.key && StrEq(text, other.text);
    }
};

struct ShellToastValue {
    Str title;
    Str description;
    ToastLevel level = ToastLevel::Info;
};

struct ShellDialogEntry {
    EntityId layer = {};
};

struct ShellRoot {
    App* app = nullptr;
    EntityId content = {};

    Vec<ShellDialogEntry> dialogs;

    EntityId tooltipOverlay = {};

    EntityId sheet = {};
    ToastManager<ShellToastId, ShellToastValue> toasts;
    ToastStackState toastState;
    FocusHandle toastFocus = {};

    int toastTimer = 0;
    Window* toastWindow = nullptr;

    uint64_t nextToastOrdinal = 0;

    bool fpsHudVisible = false;
    FpsHudRequest fpsHud = {};

    ~ShellRoot();

    static Entity<ShellRoot> New(App* app, EntityId content);
    static El* Render(ShellRoot* self, Ctx* cx);
    static void OnToastTick(ShellRoot* self, Ctx* cx, const TickEvent* event);

    static void BlurOnBackgroundPress(ShellRoot* self, Ctx* cx,
                                      const MouseDownEvent* event);
    static void OnToastClick(ShellRoot* self, Ctx* cx, const ClickEvent* event,
                             int64_t key);
};

ShellRoot* ShellRootOf(Window* window, App* app);

int ShellRootOpenDialog(Ctx* cx, Entity<ScriptView> content,
                        DialogOptions options = {});
int ShellRootOpenDialogView(Ctx* cx, EntityId content,
                            DialogOptions options = {});

bool ShellRootCloseDialog(Ctx* cx);

int ShellRootCloseAllDialogs(Ctx* cx);
bool ShellRootHasDialog(Ctx* cx);
int ShellRootDialogCount(Ctx* cx);

EntityId ShellRootTopmostDialog(Ctx* cx);

bool ShellRootOpenSheet(Ctx* cx, Entity<ScriptView> content,
                        Placement placement = Placement::Right);
bool ShellRootOpenSheetView(Ctx* cx, EntityId content,
                            Placement placement = Placement::Right);
bool ShellRootCloseSheet(Ctx* cx);
bool ShellRootHasSheet(Ctx* cx);

EntityId ShellRootSheet(Ctx* cx);

bool ShellRootShowFpsMonitor(Ctx* cx, const FpsHudRequest& request);

bool ShellRootHideFpsMonitor(Ctx* cx);
bool ShellRootFpsMonitorVisible(Ctx* cx);

bool ShellRootPushToast(Ctx* cx, const ToastRequest& toast);

bool ShellRootRemoveToast(Ctx* cx, Str id);

void ShellRootClearToasts(Ctx* cx);

int ShellRootToastCount(Ctx* cx);

void ShellRootAdvanceToasts(Ctx* cx, int64_t nowMs);
int64_t ShellRootNowMs();

}

#line 1 "src/shell/plugin.h"

namespace gpui::shell {

constexpr const char* kShellVersion = "0.6.0";
constexpr const char* kShellManifestFile = "gpui-shell.json";
constexpr int kShellMaxManifestBytes = 1024 * 1024;

constexpr const char* kGitDependencyDefaultEntry = "index.js";

struct PluginHttpGrant {
    Str scheme;
    Str host;
    uint16_t port = 0;
    bool hasPort = false;
    Vec<Str> methods;
    Vec<Str> paths;
    Vec<Str> pathPrefixes;
};

struct GitDependency {
    Str name;
    Str git;
    Str branch;
    Str tag;
    Str entry;
    Str reference;
    bool packageEntry = false;
};

struct PluginManifest {
    Arena* arena = nullptr;
    Str id;
    Str name;
    Str version;
    Str shellVersion;
    Str entry;

    Vec<GitDependency> dependencies;
    Vec<Str> readRoots;
    Vec<Str> writeRoots;
    Vec<Str> execute;
    Vec<Str> networkHosts;
    Vec<PluginHttpGrant*> http;
    bool executeUnrestricted = false;
    bool storage = true;
    bool clipboardRead = false;
    bool clipboardWrite = false;
    bool exit = false;

    PluginManifest();
    PluginManifest(const PluginManifest&) = delete;
    PluginManifest& operator=(const PluginManifest&) = delete;
    ~PluginManifest();

    Capabilities Grant(Str pluginDirectory, Str dataDirectory) const;
};

bool PluginManifestParse(Str source, PluginManifest* out,
                         ShellError* error = nullptr);
bool PluginManifestRead(Str directory, PluginManifest* out,
                        ShellError* error = nullptr);
void PluginManifestSchema(StrBuilder* out);

Str ShellDataHome();
Str ShellBundleIdForPath(Str root);
Str ShellAppDataDirectory(Str id, Arena* arena, ShellError* error = nullptr);

struct PluginDiscovery {
    PluginManifest* manifest = nullptr;
    Str root;
    Str error;
};

struct Plugin {
    PluginManifest* manifest = nullptr;
    Str root;
    Str dataDirectory;
    Str storePath;
    Policy* policy = nullptr;
    ShellRuntime* runtime = nullptr;
    Entity<ScriptView> view = {};
    AppAssets* assets = nullptr;
    App* app = nullptr;
};

using PluginAuthorizeFn = bool (*)(const PluginManifest* manifest, void* data);

class PluginManager {
  public:
    PluginManager();
    explicit PluginManager(Str directory);
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    ~PluginManager();

    PluginManager& AddDirectory(Str directory);
    PluginManager& DataHome(Str directory);
    const Vec<PluginDiscovery>& Discover();
    bool Load(ShellRuntime* runtime, Str id, PluginAuthorizeFn authorize,
              void* authorizeData, Window* window, App* app,
              ShellError* error = nullptr);
    bool Unload(Str id, App* app);
    const Plugin* Loaded(Str id) const;
    Str DataDirectory(Str id, Arena* arena) const;

  private:
    Vec<Str> directories;
    Str dataHome;
    Vec<PluginDiscovery> catalog;
    Vec<Plugin*> loaded;
    bool discovered = false;

    void ClearCatalog();
};

Entity<ShellRoot> ShellLoadApplication(ShellRuntime* runtime, Str directory,
                                       Window* window, App* app,
                                       Policy* policy = nullptr,
                                       ShellError* error = nullptr,
                                       Str* resolvedEntry = nullptr);
Str ShellCheckApplication(Arena* arena, ShellRuntime* runtime, Str directory,
                          Window* window, App* app, Policy* policy = nullptr,
                          ShellError* error = nullptr);

}

#line 1 "src/shell/dependencies.h"

namespace gpui::shell {

constexpr double kGitDependencyTimeout = 30.0;
constexpr double kGitDependencyLockTimeout = 2 * 60.0;

constexpr const char* kEditorModuleDirectory = "node_modules";
constexpr const char* kEditorLinkMarker = ".gpui-shell-link";

constexpr int kEditorPruneMaxEntries = 4096;

struct MaterializedDependency {
    Str name;
    Str root;
    Str entry;

    void Free();
};

struct MaterializedDependencies {

    Vec<MaterializedDependency> items;

    const MaterializedDependency* Find(Str name) const;

    bool CopyFrom(const MaterializedDependencies& other);
    void Free();
};

Str GitDependencyCacheRoot(Str home);

bool GitDependencyUserCacheRoot(Str home, Str userProfile, Str* out,
                                Str* error);

Str GitDependencyRemoteKey(Str git);

class GitDependencyStore {
  public:

    GitDependencyStore();
    explicit GitDependencyStore(Str root);
    GitDependencyStore(const GitDependencyStore&) = delete;
    GitDependencyStore& operator=(const GitDependencyStore&) = delete;
    ~GitDependencyStore();

    bool IsValid() const { return root.s != nullptr; }
    Str Root() const { return root; }
    Str Error() const { return initError; }

    bool Materialize(Str name, const GitDependency& dependency,
                     MaterializedDependency* out, Str* error);

    bool MaterializeAll(const PluginManifest& manifest,
                        MaterializedDependencies* out, Str* error);

    bool LinkForEditor(Str applicationRoot,
                       const MaterializedDependencies& dependencies,
                       int* linked, Str* error);

  private:
    void Prune(Str modules, const Vec<Str>& declared);

    Str root;
    Str initError;
};

struct DependencyLock {

    int64_t handle = 0;
};

bool DependencyMakeDirectories(Str path, Str* error);
void DependencyRemoveTree(Str path);

bool DependencyRenameDirectory(Str from, Str to);

bool DependencyLockAcquire(Str path, Str name, DependencyLock* out, Str* error);
void DependencyLockRelease(DependencyLock* lock);
bool DependencySymlinkDirectory(Str target, Str link);
bool DependencyRemoveDirectoryLink(Str link);

bool DependencyReadDirectoryLink(Str link, Str* target);
uint32_t DependencyProcessId();

bool ShellWriteDependencyLinks(Str applicationRoot, int* linked, Str* error);

}

#line 1 "src/shell/dock.h"

namespace gpui::shell {

using ShellPanelBuildFn = Entity<ScriptView> (*)(Window* window, App* app,
                                                 void* data);
using ShellPanelSerializeFn = bool (*)(Entity<ScriptView> view, App* app,
                                       void* data, StrBuilder* out);
using ShellPanelDeserializeFn = void (*)(Entity<ScriptView> view, Str json,
                                         Window* window, App* app, void* data);

struct ShellPanelScript {
    void* data = nullptr;
    ShellPanelBuildFn build = nullptr;
    ShellPanelSerializeFn serialize = nullptr;
    ShellPanelDeserializeFn deserialize = nullptr;
    void (*release)(void* data) = nullptr;
    bool closable = true;
    bool zoomable = true;
    bool visible = true;
};

Str ShellPanelName(Str application, Str panel);

DockPanelDef ScriptPanelNew(App* app, Str name, Entity<ScriptView> view,
                            const ShellPanelScript* script = nullptr);
Str ShellRegisterPanel(App* app, Str application, Str panel,
                       const ShellPanelScript& script);

bool ShellIsScriptPanel(const DockPanelDef& def);

struct ShellDockChrome {
    void* data = nullptr;
    El* (*tabBar)(Ctx* cx, void* data, const DockTabGroup* group) = nullptr;
    El* (*emptyGroup)(Ctx* cx, void* data, const DockTabGroup* group) = nullptr;
    El* (*dropIndicator)(Ctx* cx, void* data, Bounds bounds) = nullptr;
    El* (*dock)(Ctx* cx, void* data, const DockCtx* dock,
                El* content) = nullptr;
};

struct ShellDockChromeFrame {
    EntityHandle dock = 0;
    const DockTabGroup* group = nullptr;
    const DockCtx* dockCtx = nullptr;
};

const ShellDockChromeFrame* ShellDockCurrentChrome();

El* ShellDockTakeContent();

struct ScriptDockSkin {
    ShellDockChrome chrome = {};
    DockRenderer renderer = {};

    Str id = {};
    EntityHandle dock = 0;
    DockChromeHooks* hooks = nullptr;
    ShellRuntime* runtime = nullptr;

    ScriptDockSkin();
    explicit ScriptDockSkin(ShellDockChrome value);
    const DockRenderer* Renderer();
};

inline El* ShellDockDrawChrome(Ctx* cx, ShellRuntime* runtime,
                               EntityHandle dock, DockChromeSlot slot,
                               uint64_t key, CallbackId handler, Str payload) {
    return runtime ? runtime->DescribeDockChrome(cx, dock, slot, key, handler,
                                                 payload)
                   : nullptr;
}

void ShellTabGroupData(const DockTabGroup* group, StrBuilder* out);
void ShellDockData(const DockCtx* dock, StrBuilder* out);

void ShellDropIndicatorData(const DockState* state, Bounds to, StrBuilder* out);

}

#line 1 "src/sys/http.h"

namespace gpui {

struct HttpRsp {

    int status = 0;
    Vec<uint8_t> body;

    Str contentType;

    Str redirectUrl;
};

void HttpRspFree(HttpRsp* r);

struct HttpHeader {
    Str name;
    Str value;
};

struct HttpReq {
    Str url;
    Str method;
    const HttpHeader* headers = nullptr;
    int nHeaders = 0;
    Str body;

    bool noRedirect = false;
};

bool HttpSend(const HttpReq& req, HttpRsp* out);

bool HttpGet(Str url, HttpRsp* out);
bool HttpGetNoRedirect(Str url, HttpRsp* out);

constexpr int kHttpMaxBody = 16 * 1024 * 1024;
constexpr int kHttpTimeoutMs = 15000;

bool HttpUrlIsRemote(Str url);

struct HttpAsyncResult {
    bool ok = false;

    HttpRsp* response = nullptr;
};

bool HttpSendAsync(const HttpReq& req, Func1<HttpAsyncResult> done);

struct HttpAsyncJob {
    HttpReq req;
    Str url;
    Str method;
    Str body;
    Vec<HttpHeader> headers;
    HttpRsp response;
    Func1<HttpAsyncResult> done;
    bool ok = false;
};

void HttpAsyncJobFree(HttpAsyncJob* job);

bool HttpAsyncLaunchHosted(HttpAsyncJob* job);

bool HttpAsyncLaunch(HttpAsyncJob* job);

enum class FetchState : uint8_t {

    None = 0,

    Pending,

    Done,

    Failed
};

FetchState HttpFetch(Str url, const uint8_t** bytes, int* len);

int HttpFetchPending();

void HttpSetOnFetchDone(Func0 f);

void HttpFetchClear();

void HttpFetchDrop(Str url);

void HttpSetEnabled(bool on);
bool HttpEnabled();

}

#line 1 "src/shell/fetch.h"

namespace gpui::shell {

constexpr int kFetchMaxBody = 8 * 1024 * 1024;
constexpr int kFetchMaxRedirects = 10;

struct FetchResult {
    int status = 0;
    Str url;
    Str body;
    Str error;

    void Free();
};

bool FetchIsHttpMethod(Str method);

bool FetchHeaderIsProhibited(Str name);

struct FetchHeader {
    Str name;
    Str value;
};

struct FetchRequest {
    Str url;

    Str method;
    Vec<FetchHeader> headers;
    Str body;

    double deadline = 0;

    void Free();
};

constexpr int kFetchMaxRequestBody = 8 * 1024 * 1024;

bool FetchAuthorize(Str url, Str method, const Capabilities& capabilities,
                    Str* error = nullptr);

bool FetchSend(const FetchRequest& request, const Capabilities& capabilities,
               FetchResult* out);

struct FetchAsyncResult {
    bool ok = false;

    FetchResult* result = nullptr;
};

bool FetchSendAsync(const FetchRequest& request,
                    const Capabilities& capabilities,
                    Func1<FetchAsyncResult> done);

bool FetchFollowsLocation(int status);

void FetchRewriteRedirect(int status, Str* method, Vec<FetchHeader>* headers,
                          Str* body);

bool FetchSameOrigin(Str left, Str right);

bool FetchAuthorizeRedirect(const Capabilities& capabilities, Str method,
                            Str current, Str next,
                            const Vec<FetchHeader>& headers, Str* error);

using FetchHttpSend = bool (*)(const HttpReq& req, HttpRsp* out);
void FetchSetHttpSendForTests(FetchHttpSend send);

}

#line 1 "src/shell/host.h"

namespace gpui::shell {

enum class InvocationKind : uint8_t {
    Run,
    Check,
    Types,
    Help,
    Version,
};

struct Invocation {
    InvocationKind kind = InvocationKind::Run;

    Str directory;
    bool watch = false;
    bool development = false;
    bool printSpec = false;
};

bool ShellParseInvocation(const char* const* arguments, int count,
                          Invocation* out, Str* error);

Window* ShellOpenWindow(App* app, const FrozenComponentRegistry* components,
                        const ComponentWindowOptions& options,
                        ComponentWindowBuild build, void* data);

}

#line 1 "src/shell/process.h"

namespace gpui::shell {

struct ProcessCancellation {
    Mutex mutex;
    bool cancelled = false;

    void Cancel();
    bool IsCancelled();
};

struct ProcessOutput {
    int code = -1;
    Str out;
    Str err;

    void Free();
};

struct ProcessOptions {

    Str workingDirectory;

    const Str* environment = nullptr;
    int environmentCount = 0;

    bool inheritEnvironment = false;
};

bool ProcessRunBounded(Str command, const Str* args, int count,
                       ProcessCancellation* cancellation, ProcessOutput* out,
                       Str* error, const ProcessOptions* options = nullptr);

}

#line 1 "src/shell/scope.h"

namespace gpui {

class ShellRuntime;

enum class ScopePhase : uint8_t {
    Render,
    Event,
    Task,
    Layout,
};

const char* ScopePhaseName(ScopePhase phase);
bool ScopePhaseAllowsNotify(ScopePhase phase);

namespace shell {

class CallScopeGuard {
  public:
    CallScopeGuard(CallScopeGuard&& other) noexcept;
    CallScopeGuard& operator=(CallScopeGuard&& other) noexcept;
    CallScopeGuard(const CallScopeGuard&) = delete;
    CallScopeGuard& operator=(const CallScopeGuard&) = delete;
    ~CallScopeGuard();

    uint64_t Generation() const { return generation; }
    bool IsActive() const { return active; }

  private:
    friend CallScopeGuard ScopeEnter(Window*, App*, ScopePhase, EntityId,
                                     Policy*, ShellRuntime*, void*);
    explicit CallScopeGuard(uint64_t generation);
    void Leave();

    uint64_t generation = 0;
    bool active = false;
};

CallScopeGuard ScopeEnter(Window* window, App* app, ScopePhase phase,
                          EntityId view = {}, Policy* policy = nullptr,
                          ShellRuntime* runtime = nullptr,
                          void* application = nullptr);
void ScopeAdopt(uint64_t generation);
uint64_t ScopeCurrentGeneration();
ScopePhase ScopeCurrentPhase();
bool ScopeHasCurrent();
Policy* ScopeCurrentPolicy();
ShellRuntime* ScopeCurrentRuntime();
EntityId ScopeCurrentView();
void* ScopeCurrentApplication();

class ScopeHostContext {
  public:
    ScopeHostContext(ScopeHostContext&& other) noexcept;
    ScopeHostContext& operator=(ScopeHostContext&& other) noexcept;
    ScopeHostContext(const ScopeHostContext&) = delete;
    ScopeHostContext& operator=(const ScopeHostContext&) = delete;
    ~ScopeHostContext();

    bool IsSet() const { return held; }
    Window* GetWindow() const { return window; }
    App* GetApp() const { return app; }

  private:
    friend ScopeHostContext ScopeCurrentHost();
    friend ScopeHostContext ScopeHostForGeneration(uint64_t, ShellError*);
    ScopeHostContext(Window* window, App* app, bool held);
    static ScopeHostContext Acquire(Window* window, App* app);
    void Release();

    Window* window = nullptr;
    App* app = nullptr;
    bool held = false;
};

ScopeHostContext ScopeCurrentHost();
ScopeHostContext ScopeHostForGeneration(uint64_t generation,
                                        ShellError* error = nullptr);
Str ScopeStaleContextMessage();

}
}

#line 1 "src/shell/watch.h"

namespace gpui::shell {

constexpr int kSourceWatchMaxDepth = 8;
constexpr int kSourceWatchMaxFiles = 4096;
constexpr int kSourceWatchDebounceMs = 200;
constexpr int kSourceWatchPollMs = 250;

struct SourceTreeStamp {
    uint64_t newest = 0;
    uint64_t bytes = 0;
    uint32_t files = 0;

    bool operator==(const SourceTreeStamp& other) const {
        return newest == other.newest && bytes == other.bytes &&
               files == other.files;
    }
    bool operator!=(const SourceTreeStamp& other) const {
        return !(*this == other);
    }
};

bool ScanSourceTree(Str directory, SourceTreeStamp* stamp,
                    ShellError* error = nullptr,
                    int maxFiles = kSourceWatchMaxFiles);

class SourceWatcher {
  public:
    SourceWatcher() = default;
    SourceWatcher(const SourceWatcher&) = delete;
    SourceWatcher& operator=(const SourceWatcher&) = delete;
    ~SourceWatcher();

    bool Init(Str directory, ShellError* error = nullptr,
              int debounceMs = kSourceWatchDebounceMs);
    bool Poll(bool* changed, ShellError* error = nullptr);
    bool PollAt(double now, bool* changed, ShellError* error = nullptr);
    bool Observe(const SourceTreeStamp& next, double now, bool* changed);

  private:
    Str directory;
    SourceTreeStamp stamp;
    double changedAt = 0;
    int debounceMs = kSourceWatchDebounceMs;
    bool pending = false;
};

}

namespace gpui {

struct ShellWatcher {
    ShellRuntime* runtime = nullptr;
    EntityId view = {};
    Window* window = nullptr;
    Str directory;
    Str entry;
    shell::SourceWatcher source;
    ShellError error = {};
    int timer = 0;
    int scanTask = 0;
    void* scanJob = nullptr;

    ~ShellWatcher();

    static Entity<ShellWatcher> Start(ShellRuntime* runtime,
                                      Entity<ScriptView> view, Str directory,
                                      Str entry, Window* window, App* app,
                                      ShellError* error = nullptr);
    static void OnPoll(ShellWatcher* self, Ctx* cx, const TickEvent*);
};

}

#line 1 "src/shell/standard.h"

namespace gpui::shell {

constexpr int kStandardDataLimit = 64 * 1024 * 1024;

void Sha256(Str data, uint8_t digest[32]);
bool SecureRandom(uint8_t* bytes, int count);

bool ZlibDeflate(Str input, bool gzip, Str* output, Str* error = nullptr);
bool ZlibInflate(Str input, bool gzip, Str* output, Str* error = nullptr);

}

#line 1 "src/shell/style.h"

namespace gpui::shell {

bool IsParamStyleName(Str name);

bool IsNullaryStyleName(Str name);

bool ApplyNullaryStyle(El* element, Str name, float remSize = 16);

bool ApplyParamStyle(El* element, const SpecOp& op, ShellError* error,
                     float remSize = 16);

uint32_t StyleFieldsOf(Str name);

TempStr StyleSuggestTemp(Str name);

TempStr UnknownElementMethodTemp(Str name);

}

#line 1 "src/shell/theme_tokens.h"

namespace gpui {
struct App;

namespace shell {

uint32_t ThemeTokensSync(const App* app);
uint32_t ThemeTokensRevision();
bool ThemeTokenColor(Str name, Hsla* out);
bool ThemeTokenSpacing(Str name, float* out);
bool ThemeTokenRadius(Str name, float* out);

bool ThemeTypographyTokens(TypographyTokens* out);
SeqStrings ThemeColorTokenNames();
SeqStrings ThemeSpacingTokenNames();
SeqStrings ThemeRadiusTokenNames();

}
}

#line 1 "src/shell/typings.h"

namespace gpui::shell {

constexpr const char* kShellTypesFile = "gpui-kit.d.ts";

constexpr const char* kShellConfigFile = "jsconfig.json";
constexpr const char* kShellTypeScriptConfigFile = "tsconfig.json";
constexpr int kShellTypesMaxDepth = 8;
constexpr int kShellTypesMaxFiles = 4096;

class FrozenComponentRegistry;

void AppendBuiltinTypeDeclarations(StrBuilder* out);

void ShellTypeDeclarations(StrBuilder* out,
                           const HostModules* modules = nullptr,
                           const FrozenComponentRegistry* components = nullptr);

bool ShellWriteTypeDeclarations(
    Str directory, const HostModules* modules = nullptr, int* written = nullptr,
    ShellError* error = nullptr,
    const FrozenComponentRegistry* components = nullptr);

}

#line 1 "src/sys/dir_watch.h"

namespace gpui {

using DirWatchId = int;

enum class DirWatchError {
    None = 0,

    Unsupported,

    Failed,

    Limit,
};

const char* DirWatchErrorName(DirWatchError err);

DirWatchId DirWatchStart(Str dir, Func0 onChange, bool create = false,
                         DirWatchError* err = nullptr);

void DirWatchStop(DirWatchId id);

int DirWatchCount();

DirWatchId DirWatchAdd(Func0 onChange);

void DirWatchSignal(DirWatchId id);

struct DirWatchPlat;

DirWatchPlat* DirWatchPlatOpen(DirWatchId id, const char* dir, bool create,
                               DirWatchError* err);

void DirWatchPlatClose(DirWatchPlat* plat);

}

#line 1 "src/sys/executor.h"

namespace gpui {

using TaskId = int;

void ExecInit();

void ExecShutdown();

bool ExecOnMainThread();

void ExecSetWake(Func0 wake);

void ExecPost(Func0 f);

void ExecPostNow(Func0 f);

int ExecDrain();

int ExecQueued();

TaskId ExecSpawn(Func0 work, Func0 done = Func0{});

bool ExecCancel(TaskId id);

int ExecPending();

bool ExecWaitIdle(int timeoutMs);

int ExecWorkerCount();

bool ExecHasThreads();

constexpr int kExecMaxWorkers = 8;

}

#line 1 "src/sys/gpu.h"

namespace gpui {

bool GpuAvailable();

float GpuUsagePercent();

void GpuProbeFree();

}

#line 1 "src/sys/notify.h"

namespace gpui {

using SysNotifyResponseFn = Func1<Str>;

bool SysNotifyAvailable();

void SysNotifySetAppIdentity(Str appId, Str appName);

bool SysNotifyShow(Str tag, Str title, Str body);

void SysNotifyDismiss(Str tag);

void SysNotifyOnResponse(SysNotifyResponseFn fn);

void SysNotifyShutdown();

}

#line 1 "src/sys/task.h"

#include <coroutine>

namespace gpui {

struct TaskGuard {
    bool (*alive)(void* user) = nullptr;
    void* user = nullptr;

    bool IsAlive() const { return !alive || alive(user); }
};

struct TaskHandle {
    int32_t index = -1;
    uint32_t gen = 0;

    bool IsValid() const { return index >= 0 && gen != 0; }
};

inline bool operator==(TaskHandle a, TaskHandle b) {
    return a.index == b.index && a.gen == b.gen;
}
inline bool operator!=(TaskHandle a, TaskHandle b) {
    return !(a == b);
}

struct TaskPromise;

struct Task {
    using promise_type = TaskPromise;

    TaskHandle id;

    bool IsRunning() const;
};

bool TaskCancel(TaskHandle id);

bool TaskLive(TaskHandle id);

int TaskCount();

int TaskCancelAll();

struct TaskFinal {
    TaskPromise* promise = nullptr;

    bool await_ready() noexcept;
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    void await_resume() const noexcept {}
};

struct TaskPromise {
    TaskGuard guard;
    TaskHandle id;

    bool awaiting = false;
    bool cancelled = false;

    TaskPromise() = default;

    template <typename... Rest>
    explicit TaskPromise(TaskGuard g, Rest&&...) : guard(g) {}

    Task get_return_object();

    std::suspend_never initial_suspend() const noexcept { return {}; }

    TaskFinal final_suspend() noexcept { return TaskFinal{this}; }
    void return_void() const noexcept {}

    void unhandled_exception() const noexcept {}
};

struct BackgroundSpawn {
    Func0 work;
    std::coroutine_handle<TaskPromise> waiting{};
    TaskHandle id;

    explicit BackgroundSpawn(Func0 w) : work(w) {}

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<TaskPromise> h);
    void await_resume() const noexcept {}
};

}

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/taffy/taffy_math.h"

#if defined(_MSC_VER)
#define TAFFY_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define TAFFY_INLINE inline __attribute__((always_inline))
#else
#define TAFFY_INLINE inline
#endif

namespace taffy {

inline Optf MaybeMin(Optf a, Optf b) {
    if (IsNone(a)) {
        return None();
    }
    return IsSome(b) ? F32Min(a, b) : a;
}

inline Optf MaybeMax(Optf a, Optf b) {
    if (IsNone(a)) {
        return None();
    }
    return IsSome(b) ? F32Max(a, b) : a;
}

inline Optf MaybeAdd(Optf a, Optf b) {
    if (IsNone(a)) {
        return None();
    }
    return IsSome(b) ? a + b : a;
}

inline Optf MaybeSub(Optf a, Optf b) {
    if (IsNone(a)) {
        return None();
    }
    return IsSome(b) ? a - b : a;
}

inline Optf MaybeClamp(Optf a, Optf lo, Optf hi) {
    if (IsNone(a)) {
        return None();
    }
    float v = a;
    if (IsSome(hi)) {
        v = F32Min(v, hi);
    }
    if (IsSome(lo)) {
        v = F32Max(v, lo);
    }
    return v;
}

inline AvailableSpace MaybeMin(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite) {
        return IsSome(b) ? AvailableSpace::Definite(F32Min(a.value, b)) : a;
    }
    return IsSome(b) ? AvailableSpace::Definite(b) : a;
}

inline AvailableSpace MaybeMax(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && IsSome(b)) {
        return AvailableSpace::Definite(F32Max(a.value, b));
    }
    return a;
}

inline AvailableSpace MaybeAdd(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && IsSome(b)) {
        return AvailableSpace::Definite(a.value + b);
    }
    return a;
}

inline AvailableSpace MaybeSub(AvailableSpace a, Optf b) {
    if (a.kind == AvailableSpace::Kind::Definite && IsSome(b)) {
        return AvailableSpace::Definite(a.value - b);
    }
    return a;
}

inline AvailableSpace MaybeClamp(AvailableSpace a, Optf lo, Optf hi) {
    if (a.kind != AvailableSpace::Kind::Definite) {
        return a;
    }
    float v = a.value;
    if (IsSome(hi)) {
        v = F32Min(v, hi);
    }
    if (IsSome(lo)) {
        v = F32Max(v, lo);
    }
    return AvailableSpace::Definite(v);
}

TAFFY_INLINE SizeFOpt MaybeMin(SizeFOpt a, SizeFOpt b) {
    return {MaybeMin(a.w, b.w), MaybeMin(a.h, b.h)};
}

TAFFY_INLINE SizeFOpt MaybeMax(SizeFOpt a, SizeFOpt b) {
    return {MaybeMax(a.w, b.w), MaybeMax(a.h, b.h)};
}

TAFFY_INLINE SizeFOpt MaybeAdd(SizeFOpt a, SizeFOpt b) {
    return {MaybeAdd(a.w, b.w), MaybeAdd(a.h, b.h)};
}

TAFFY_INLINE SizeFOpt MaybeSub(SizeFOpt a, SizeFOpt b) {
    return {MaybeSub(a.w, b.w), MaybeSub(a.h, b.h)};
}

TAFFY_INLINE SizeFOpt MaybeClamp(SizeFOpt a, SizeFOpt lo, SizeFOpt hi) {
    return {MaybeClamp(a.w, lo.w, hi.w), MaybeClamp(a.h, lo.h, hi.h)};
}

TAFFY_INLINE SizeAvail MaybeSub(SizeAvail a, SizeFOpt b) {
    return {MaybeSub(a.width, b.w), MaybeSub(a.height, b.h)};
}

TAFFY_INLINE SizeAvail MaybeClamp(SizeAvail a, SizeFOpt lo, SizeFOpt hi) {
    return {MaybeClamp(a.width, lo.w, hi.w), MaybeClamp(a.height, lo.h, hi.h)};
}

TAFFY_INLINE SizeAvail MaybeSet(SizeAvail a, SizeFOpt v) {
    return a.MaybeSet(v);
}

constexpr SizeFOpt AsOptional(SizeF s) {
    return s;
}

}

#endif

#if GPUI_INCLUDE_PRIVATE_API
#line 1 "src/taffy/compute.h"

namespace taffy {

void ComputeRootLayout(TaffyTree* tree, NodeId root, SizeAvail availableSpace);

LayoutOutput ComputeHiddenLayout(TaffyTree* tree, NodeId node);

void RoundLayout(TaffyTree* tree, NodeId node);

using LeafMeasureFn = SizeF (*)(SizeFOpt knownDimensions,
                                SizeAvail availableSpace, void* ctx);

LayoutOutput ComputeLeafLayout(const LayoutInput& inputs, const Style& style,
                               CalcResolver calc, LeafMeasureFn measure,
                               void* measureCtx);

LayoutOutput ComputeFlexboxLayout(TaffyTree* tree, NodeId node,
                                  const LayoutInput& inputs);

LayoutOutput ComputeGridLayout(TaffyTree* tree, NodeId node,
                               const LayoutInput& inputs);

LayoutOutput ComputeBlockLayout(TaffyTree* tree, NodeId node,
                                const LayoutInput& inputs,
                                BlockContext* blockCtx);

inline AlignItemsKeyword ResolveSelfAlignmentSafety(AlignItems alignment,
                                                    bool overflows) {
    if (alignment.safety == AlignmentSafety::Safe && overflows) {
        return AlignItemsKeyword::Start;
    }
    return alignment.keyword;
}

AlignContentKeyword ApplyAlignmentFallback(float freeSpace, int numItems,
                                           AlignContent alignmentMode);

float ComputeAlignmentOffset(float freeSpace, int numItems, float gap,
                             AlignContentKeyword alignmentMode,
                             bool layoutIsFlexReversed, bool isFirst);

void GridExplicitSizeForTest(const Style& style, Optf autoFitContainerSize,
                             bool maxRepetitions, AbsoluteAxis axis,
                             CalcResolver calc, uint16_t* outAutoRepetitions,
                             uint16_t* outTrackCount);

void GridChildMinMaxSpanForTest(LinePlacement line, uint16_t explicitTrackCount,
                                int16_t* outMinLine, int16_t* outMaxLine,
                                uint16_t* outSpan);

void GridSizeEstimateForTest(uint16_t explicitColCount,
                             uint16_t explicitRowCount, Direction direction,
                             const LinePlacement* columns,
                             const LinePlacement* rows, int n,
                             uint16_t* outColCounts, uint16_t* outRowCounts);

struct GridTrackForTest {
    bool isGutter = false;
    bool isCollapsed = false;
    CompactLength min;
    CompactLength max;
};

int GridInitTracksForTest(const Style& style, AbsoluteAxis axis,
                          uint16_t negativeImplicit, uint16_t explicitCount,
                          uint16_t positiveImplicit, GridTrackForTest* out,
                          int cap);

struct GridPlacementForTest {
    int16_t columnStart = 0;
    int16_t columnEnd = 0;
    int16_t rowStart = 0;
    int16_t rowEnd = 0;
};

int GridPlaceForTest(TaffyTree* tree, NodeId parent, uint16_t explicitColCount,
                     uint16_t explicitRowCount, GridAutoFlow flow,
                     GridPlacementForTest* out, int cap, uint16_t* outColCounts,
                     uint16_t* outRowCounts);

SizeF ComputeContentSizeContribution(PointF location, SizeF size,
                                     SizeF contentSize, PointOverflow overflow);

}

#endif

#line 1 "src/ui/element_ext.h"

#line 1 "src/wry/wry.h"

namespace wry {

using base::Str;
using base::Vec;

struct Position {
    double x = 0;
    double y = 0;
    bool logical = true;
};

struct Size {
    double width = 0;
    double height = 0;
    bool logical = true;
};

struct Rect {
    Position position;
    Size size;
};

inline Position LogicalPosition(double x, double y) {
    return Position{x, y, true};
}
inline Position PhysicalPosition(double x, double y) {
    return Position{x, y, false};
}
inline Size LogicalSize(double w, double h) {
    return Size{w, h, true};
}
inline Size PhysicalSize(double w, double h) {
    return Size{w, h, false};
}

struct Rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

enum class Theme : uint8_t {
    Dark,
    Light,
    Auto,
};

enum class PageLoadEvent : uint8_t {
    Started,
    Finished,
};

enum class ScrollBarStyle : uint8_t {
    Default,
    FluentOverlay,
};

enum class MemoryUsageLevel : uint8_t {
    Normal,
    Low,
};

enum class BackgroundThrottlingPolicy : uint8_t {
    Disabled,
    Suspend,
    Throttle,
};

enum class ProxyKind : uint8_t {
    None,
    Http,
    Socks5,
};

struct ProxyConfig {
    ProxyKind kind = ProxyKind::None;
    Str host;
    Str port;
};

struct InitializationScript {
    Str script;

    bool forMainFrameOnly = true;
};

struct Header {
    Str name;
    Str value;
};

struct Request {
    Str method;
    Str uri;
    const Header* headers = nullptr;
    int headerCount = 0;
    const uint8_t* body = nullptr;
    int bodyLen = 0;
};

struct Response {
    int status = 200;
    const Header* headers = nullptr;
    int headerCount = 0;
    const uint8_t* body = nullptr;
    int bodyLen = 0;
};

struct RequestResponder;
void Respond(RequestResponder* responder, const Response* response);

struct CustomProtocol {
    Str name;
    void* ctx = nullptr;
    void (*handler)(void* ctx, Str id, const Request* request,
                    RequestResponder* responder) = nullptr;
};

struct WebView;

enum class NewWindowResponse : uint8_t {
    Allow,
    Create,
    Deny,
};

struct NewWindowFeatures {
    bool hasPosition = false;
    double x = 0;
    double y = 0;
    bool hasSize = false;
    double width = 0;
    double height = 0;
    WebView* opener = nullptr;

    void* targetConfiguration = nullptr;
};

using DownloadStartedHandler = bool (*)(void* ctx, Str url, Str* path);

using DownloadCompletedHandler = void (*)(void* ctx, Str url, const Str* path,
                                          bool success);

enum class DragDropKind : uint8_t {
    Enter,
    Over,
    Drop,
    Leave,
};

struct DragDropEvent {
    DragDropKind kind = DragDropKind::Leave;
    const Str* paths = nullptr;
    int pathCount = 0;
    int32_t x = 0;
    int32_t y = 0;
};

using DragDropHandler = bool (*)(void* ctx, const DragDropEvent* event);

enum class CookieSameSite : uint8_t {
    None,
    Lax,
    Strict,
};

struct Cookie {
    Str name;
    Str value;
    Str domain;
    Str path;
    bool hasHttpOnly = false;
    bool httpOnly = false;
    bool hasSecure = false;
    bool secure = false;
    bool hasSameSite = false;
    CookieSameSite sameSite = CookieSameSite::Lax;
    bool session = true;
    bool hasExpires = false;
    int64_t expiresUnixSeconds = 0;
    bool hasMaxAge = false;
    int64_t maxAgeSeconds = 0;
};

void CookieListFree(Vec<Cookie>* cookies);

struct WebViewAttributes {
    static bool AllowDownload(void*, Str, Str*) { return true; }

    Str id;

    Str dataDirectory;
    Str userAgent;
    bool visible = true;
    bool transparent = false;
    bool hasBackgroundColor = false;
    Rgba backgroundColor;

    Str url;

    const Header* headers = nullptr;
    int headerCount = 0;

    Str html;
    bool zoomHotkeysEnabled = false;
    const InitializationScript* initializationScripts = nullptr;
    int initializationScriptCount = 0;
    const CustomProtocol* customProtocols = nullptr;
    int customProtocolCount = 0;

    void* ctx = nullptr;

    void (*ipcHandler)(void* ctx, Str url, Str body) = nullptr;

    bool (*navigationHandler)(void* ctx, Str url) = nullptr;
    void (*documentTitleChangedHandler)(void* ctx, Str title) = nullptr;
    void (*onPageLoadHandler)(void* ctx, PageLoadEvent event,
                              Str url) = nullptr;

    DownloadStartedHandler downloadStartedHandler = AllowDownload;
    DownloadCompletedHandler downloadCompletedHandler = nullptr;
    DragDropHandler dragDropHandler = nullptr;

    NewWindowResponse (*newWindowReqHandler)(
        void* ctx, Str url, const NewWindowFeatures* features,
        WebView** createdWebView) = nullptr;

    bool clipboard = false;
#if defined(DEBUG) || defined(_DEBUG)
    bool devtools = true;
#else
    bool devtools = false;
#endif

    bool acceptFirstMouse = false;
    bool backForwardNavigationGestures = false;
    bool incognito = false;
    bool autoplay = true;
    ProxyConfig proxyConfig;
    bool focused = true;

    bool hasBounds = true;

    Rect bounds = {{0, 0, true}, {200, 200, true}};

    bool hasBackgroundThrottling = false;
    BackgroundThrottlingPolicy backgroundThrottling =
        BackgroundThrottlingPolicy::Disabled;
    bool javascriptDisabled = false;

    bool hasDataStoreIdentifier = false;
    uint8_t dataStoreIdentifier[16] = {};

    bool hasTrafficLightInset = false;
    Position trafficLightInset;

    bool allowLinkPreview = true;

    void* webviewConfiguration = nullptr;

    Str additionalBrowserArgs;
    bool browserAcceleratorKeys = true;
    bool defaultContextMenus = true;
    bool hasTheme = false;
    Theme theme = Theme::Auto;

    bool useHttpsScheme = false;
    ScrollBarStyle scrollBarStyle = ScrollBarStyle::Default;
    bool browserExtensionsEnabled = false;

    Str extensionPath;

    void* webviewEnvironment = nullptr;
};

WebView* WebViewNew(void* parentWindow, const WebViewAttributes* attrs,
                    bool asChild);

void WebViewFree(WebView* webview);

Str WebViewId(WebView* webview);

bool WebViewEval(WebView* webview, Str js);

bool WebViewEvalWithCallback(WebView* webview, Str js, void* ctx,
                             void (*callback)(void* ctx, Str result));

Str WebViewUrlTemp(WebView* webview);

bool WebViewLoadUrl(WebView* webview, Str url);

bool WebViewLoadUrlWithHeaders(WebView* webview, Str url, const Header* headers,
                               int headerCount);

bool WebViewLoadHtml(WebView* webview, Str html);

bool WebViewReload(WebView* webview);

bool WebViewBounds(WebView* webview, Rect* out);

bool WebViewSetBounds(WebView* webview, Rect bounds);

bool WebViewSetVisible(WebView* webview, bool visible);

bool WebViewFocus(WebView* webview);

bool WebViewFocusParent(WebView* webview);

bool WebViewZoom(WebView* webview, double scaleFactor);

bool WebViewSetBackgroundColor(WebView* webview, Rgba color);

bool WebViewSetTheme(WebView* webview, Theme theme);

bool WebViewSetMemoryUsageLevel(WebView* webview, MemoryUsageLevel level);

bool WebViewReparent(WebView* webview, void* parentWindow);

bool WebViewSetTrafficLightInset(WebView* webview, Position position);

bool WebViewPrint(WebView* webview);

bool WebViewClearAllBrowsingData(WebView* webview);

bool WebViewCookies(WebView* webview, Vec<Cookie>* out);
bool WebViewCookiesForUrl(WebView* webview, Str url, Vec<Cookie>* out);
bool WebViewSetCookie(WebView* webview, const Cookie* cookie);
bool WebViewDeleteCookie(WebView* webview, const Cookie* cookie);

void WebViewOpenDevtools(WebView* webview);
void WebViewCloseDevtools(WebView* webview);
bool WebViewIsDevtoolsOpen(WebView* webview);

#if GPUI_OS_WINDOWS

void* WebViewControllerRaw(WebView* webview);
void* WebViewEnvironmentRaw(WebView* webview);
void* WebViewNativeRaw(WebView* webview);
#endif

Str WorkAroundUriPrefix(Str httpOrHttps, Str protocol);

bool IsWorkAroundUri(Str uri, Str httpOrHttps, Str protocol);

Str ApplyUriWorkAround(Str uri, Str httpOrHttps, Str protocol);
Str RevertUriWorkAround(Str uri, Str httpOrHttps, Str protocol);

Str ProxyUriTemp(const ProxyConfig* proxy);

Str PathFromFileUriTemp(Str uri);

void DownloadFileNameParts(Str uri, Str suggested, Str* stem, Str* ext);

double ScaleFactorFromScreen(int widthPx, int widthMm);

struct SyntheticMouseEvent {
    bool pressed = false;

    int button = 8;
    int x = 0;
    int y = 0;
    int buttons = 0;
    int detail = 1;
    bool ctrlKey = false;
    bool altKey = false;
    bool shiftKey = false;
    bool metaKey = false;
};
Str SyntheticMouseEventJsTemp(const SyntheticMouseEvent* ev);

struct PollFd {
    int fd;
    uint16_t events;
    uint16_t revents;
};

int EventLoopPrepare(PollFd** fds, int* timeoutMs);

void EventLoopDispatch();

Str WebViewVersionTemp();

bool WebViewAvailable();

}

#line 1 "src/webview/webview.h"

namespace gpui {

struct WebView;
struct WebViewHandleState;

struct WebViewHandle {
    WebViewHandle() = default;
    WebViewHandle(const WebViewHandle& other);
    WebViewHandle& operator=(const WebViewHandle& other);
    ~WebViewHandle();

    wry::WebView* Raw() const;
    bool IsValid() const { return Raw() != nullptr; }

  private:
    WebViewHandleState* state = nullptr;

    explicit WebViewHandle(WebViewHandleState* state);
    friend Entity<WebView> WebViewNew(Ctx* cx,
                                      const wry::WebViewAttributes* attrs);
};

struct WebView {
    WebViewHandle owned;
    bool visible = true;

    Bounds bounds = {};

    Bounds applied = {};
    bool hasApplied = false;

    bool subscribed = false;

    ~WebView();

    static void OnWindowMouseDown(WebView* self, Ctx* cx,
                                  const MouseDownEvent* ev);
};

Entity<WebView> WebViewNew(Ctx* cx, const wry::WebViewAttributes* attrs);

void WebViewShow(WebView* self);
void WebViewHide(WebView* self);
bool WebViewVisible(const WebView* self);

Bounds WebViewBounds(const WebView* self);

void WebViewLoadUrl(WebView* self, Str url);

void WebViewBack(WebView* self);

wry::WebView* WebViewRaw(const WebView* self);

WebViewHandle WebViewGetHandle(const WebView* self);

El* WebViewEl(Entity<WebView> view, Ctx* cx);

}

#endif

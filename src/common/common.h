#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <ole2.h>
#include <tlhelp32.h>

// shlwapi.h defines StrDup as a macro that aliases to StrDupW when UNICODE
// is set, which collides with our StrDup(Arena*, Str) function.
#undef StrDup

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>
#include <new>     // for placement new
#include <utility> // for std::forward

typedef unsigned __int64 u64;

using AtomicBool = volatile LONG;
using AtomicInt = volatile LONG;

// Atomic bool operations (base.cpp)
bool AtomicBoolGet(AtomicBool* p);
void AtomicBoolSet(AtomicBool* p, bool v);

// Atomic int operations (base.cpp)
int AtomicIntGet(AtomicInt* p);
void AtomicIntSet(AtomicInt* p, int v);
int AtomicIntAdd(AtomicInt* p, int v);
int AtomicIntInc(AtomicInt* p);
int AtomicIntDec(AtomicInt* p);


// arena.cpp

// Standalone reserve/commit arena
static const u64 ARENA_HEADER_SIZE = 128;

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
    SRWLOCK lock;

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
void* Realloc(struct Arena* arena, void* mem, size_t size);
void* MemDup(struct Arena* arena, const void* mem, size_t size, size_t extraBytes = 0);

template <typename T>
inline T* AllocArray(struct Arena* arena, size_t n = 1) {
    return (T*)AllocZero(arena, n * sizeof(T));
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

// works on any struct with len member (Str, WStr, *Vec)
template <typename T>
int len(const T& v) {
    return v.len;
}
template <typename T>
int len(const T* v) {
    return v->len;
}

template<typename T>
void VecExpandTo(Arena* arena, T& v, int wantedSize) {
    if (wantedSize <= v.cap) {
        return;
    }
    v.els = (decltype(v.els))ReallocToWantedSize(arena, v.els, &v.cap, wantedSize, (int)sizeof(*v.els));
}

template <typename T>
void VecExpand(Arena* arena, T& v, int n) {
    int wantedSize = v.len + n;
    VecExpandTo(arena, v, wantedSize);
}

template <typename T, typename E>
void VecPush(Arena* arena, T& v, E el) {
    VecExpand(arena, v, 1);
    v.els[v.len] = el;
    v.len++;
}

// Iterator wrapper for range-based for loops over Vec types (structs with len/els)
template <typename Vec>
class VecIterator {
    Vec* vec;

  public:
    VecIterator(Vec* v) : vec(v) {}
    auto begin() { return vec ? vec->els : nullptr; }
    auto end() { return vec ? vec->els + vec->len : nullptr; }
};

// Helper functions for type deduction (works with both Vec& and Vec*)
template <typename Vec>
VecIterator<Vec> VecIter(Vec& v) {
    return VecIterator<Vec>(&v);
}
template <typename Vec>
VecIterator<Vec> VecIter(Vec* v) {
    return VecIterator<Vec>(v);
}

// str_util.cpp

struct Str {
    char* s;
    int len;

    Str() : s(nullptr), len(0) {}
    explicit Str(char* s_) : s(s_), len(0) {
        len = (int)strlen(s);
    }
    explicit Str(char* s_, int len_) : s(s_), len(len_) {}
};

// Create Str from string literal with compile-time length
#define StrL(lit) Str((char*)(lit), (int)(sizeof(lit) - 1))

Str AllocStrTemp(int size);

struct WStr {
    wchar_t* s;
    int len;

    WStr() : s(nullptr), len(0) {}
    explicit WStr(wchar_t* s_) : s(s_), len(0) {
        while (s && s[len]) len++;
    }
    explicit WStr(wchar_t* s_, int len_) : s(s_), len(len_) {}
};

// Create WStr from wide string literal with compile-time length
#define WStrL(lit) WStr((wchar_t*)(lit), (int)(sizeof(lit) / sizeof(wchar_t) - 1))

bool WStrEq(WStr a, WStr b);
void WStrCopy(wchar_t* dst, const wchar_t* src, int maxLen);
wchar_t ToLowerW(wchar_t c);
int WStrFindSubstr(WStr str, WStr substr);
int WStrCmpNoCase(WStr a, WStr b);

WStr ToWStrTemp(Str s);
Str ToUtf8(Arena* arena, WStr wide);
Str ToUtf8Temp(WStr wide);

// Str utilities
Str StrDup(Arena* arena, Str s);
Str StrDupTemp(Str s);
bool StrEq(Str a, Str b);
bool StrContains(Str str, Str substr);
bool StrHasPrefix(Str s, Str prefix);
bool StrHasSuffix(Str s, Str suffix);
Str StrTrimSuffix(Str s, Str suffix);
bool StrHasPrefixNoCase(Str s, Str prefix);
Str FormatFileSize(Arena* arena, u64 size);
void FormatFileSizeToWstrBuf(u64 size, WStr buf);
int FormatSizeHumanIntoBuf(u64 size, Str buf);
void FormatSizeHumanIntoWBuf(u64 size, WStr wbuf);
Str PathJoinTemp(Str dir, Str name);
Str StrFmt(Arena* arena, const char* fmt, ...);
#define StrFmtTemp(fmt, ...) StrFmt(GetTempArena(), fmt, __VA_ARGS__)
int StrLastIndexOfChar(Str s, char c);

// UTF-8 string utilities (legacy, for null-terminated strings)
void StrCopyUtf8(char* dst, const char* src, int maxBytes);
Str StrTrimSuffixWhitespace(Str s);

// Counters for StrFmt optimization tracking
extern AtomicInt gStrFmtFirstAlloc;  // Formatted into available space
extern AtomicInt gStrFmtSecondAlloc; // Needed separate allocation

struct VecStr {
    int len;
    int cap;
    Str* els;
};

void SplitStrByWhitespace(Arena* arena, const Str& s, VecStr& vecOut);

// file_util.cpp
bool FileSystemEntryExists(Str s);
bool DirectoryExists(Str s);
bool FileExists(Str s);
Str FindFirstValidParentDir(Str path);
Str PathGetDirTemp(Str path);
Str PathGetNameTemp(Str path);
Str SmartResolveDirectory(Str dir);

// Works for any struct with int len member (Str, WStr, *Vec, etc.)
template <typename T>
inline bool IsEmpty(const T& v) {
    return v.len <= 0;
}
template <typename T>
inline bool IsEmpty(const T* v) {
    return !v || v->len <= 0;
}

// specialized for Str and WStr
inline bool IsEmpty(const Str& v) {
    return !v.s || v.len <= 0;
}
inline bool IsEmpty(const Str* v) {
    return !v || !v->s || v->len <= 0;
}
inline bool IsEmpty(const WStr& v) {
    return !v.s || v.len <= 0;
}
inline bool IsEmpty(const WStr* v) {
    return !v || !v->s || v->len <= 0;
}

// dir_util.cpp

// Forward declaration for DirEntry
struct DirEntries;

// Sentinel value indicating directory is still being scanned
#define kStillScanningDir ((DirEntries*)(uintptr_t)-2)

// Check if DirEntry is a directory (dv != nullptr means it's a dir)
inline bool IsDir(DirEntries* dv) {
    return dv != nullptr;
}

struct DirEntry {
    Str name;
    u64 size;
    DirEntries* dv; // nullptr=file, kStillScanningDir=dir not yet scanned, else=scanned dir
    FILETIME createTime;
    FILETIME modTime;
};

struct DirEntries {
    Str fullDir; // Full path of this directory
    int len;
    DirEntry* els;
    char* err; // Error message if directory couldn't be read, nullptr otherwise
};

struct DirEntriesNode {
    DirEntriesNode* next;
    DirEntries* dv;
    bool nonRecursive; // If true, don't queue subdirectories for scanning
};

// Callback type for when a directory scan completes
typedef void (*OnScannedDirCallback)(DirEntries* dv, void* userData);

// Background directory reader thread context
struct DirScanCtx {
    Arena* a; // Permanent data arena
    OnScannedDirCallback onScannedDir;
    void* userData;
    CRITICAL_SECTION cs;         // Protect queue access
    HANDLE hSemaphore;           // Counting semaphore for work items
    HANDLE hQueueEmptyEvent;     // Signaled when all work is done (queue empty + no in-flight)
    HANDLE hThreadExitedEvent;   // Signaled when thread has exited
    DirEntriesNode* dirsToVisit; // Queue of directories to scan
    AtomicBool shouldExit;       // Signal thread to exit
    AtomicInt inFlightCount;     // Number of directories currently being processed
};

DirScanCtx* CreateDirScanCtx(Arena* arena, OnScannedDirCallback callback, void* userData);
void AskDirScanThreadToQuit(DirScanCtx* ctx);
DirEntries* RequestDirScan(DirScanCtx* ctx, Str dir);
void QueueDirScan(DirScanCtx* ctx, DirEntries* dv, bool nonRecursive = false);
void RequestDirRescan(DirScanCtx* ctx, DirEntries* dv);

// Directory utilities (paths are UTF-8)
DirEntry* FindEntryByName(DirEntries* dv, Str name);
DWORD WINAPI DirScanThread(LPVOID param);

// Allocate a DirEntries with fullDir set
DirEntries* AllocDirEntries(Arena* arena, Str fullDir);
void ReadDirectory(Arena* arena, DirEntries* dv, AtomicBool* shouldExit);

// win_util.cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DC state management
struct SavedDCState {
    HWND hwnd;
    HDC hdc;
    HFONT oldFont;
};

SavedDCState SaveDCState(HWND hwnd);
void RestoreDCState(SavedDCState* state);
int MeasureStringWidth(HDC hdc, const wchar_t* str);
Str GetWindowTextTemp(HWND hwnd);
void SetHwndText(HWND hwnd, Str s);
Str GetLastErrorAsStr(Arena* arena);
bool WasLaunchedByPowershellWithPipeRedirect();
Str GetAppLocalDataDirTemp();

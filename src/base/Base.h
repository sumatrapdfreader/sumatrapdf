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
#elif defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
#define IS_INTEL_64 0
#define IS_INTEL_32 0
#define IS_ARM_64 1
#else
#error "unsupported arch"
#endif

/* OS_POSIX - Any POSIX-like system */
#if OS_DARWIN || OS_LINUX || defined(unix) || defined(__unix) || defined(__unix__)
#define OS_POSIX 1
#else
#define OS_POSIX 0
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
#define COMPILER_CLANG 0
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

// C/C++ standard headers  we use often
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>
#include <new>       // for placement new
#include <algorithm> // for std::min, std::max
#include <utility>   // for std::forward
#if OS_POSIX
#include <pthread.h>
#include <strings.h>
#endif

#define _USE_MATH_DEFINES
#include <math.h>

#if OS_WIN
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
#include <shellapi.h>
#include <ole2.h>

// nasty but necessary
#if defined(min) || defined(max)
#error "min or max defined"
#endif
// mingw's gdiplus.h includes <math.h> which in C++ pulls in <cmath>/<limits>
// that use min/max as identifiers; pre-include them before defining macros
#ifdef __GNUC__
#include <cmath>
#include <algorithm>
#include <limits>
#endif
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#include <gdiplus.h>
#undef NOMINMAX
#undef min
#undef max

// some GDI+ PropertyTag* are missing from mingw headers (but present in MS SDK)
#ifndef PropertyTagExifFocalLengthIn35mmFilm
#define PropertyTagExifFocalLengthIn35mmFilm ((PROPID)0xA405)
#endif
#ifndef PropertyTagExifWhiteBalance
#define PropertyTagExifWhiteBalance ((PROPID)0xA403)
#endif
#else
using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using DWORD64 = uint64_t;
using UINT = unsigned int;
using LONG = int32_t;
using BOOL = int;
using WCHAR = wchar_t;
using COLORREF = uint32_t;
using LCID = uint32_t;
using HWND = void*;
using HDC = void*;

struct EXCEPTION_POINTERS;
struct MINIDUMP_EXCEPTION_INFORMATION;

struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

#define CP_ACP 0
#define CP_UTF8 65001
#define LOCALE_USER_DEFAULT 0
constexpr int MAX_PATH = 4096;
constexpr int URLZONE_INVALID = -1;
constexpr int URLZONE_INTERNET = 3;

struct POINT {
    LONG x;
    LONG y;
};

struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};

struct SIZE {
    LONG cx;
    LONG cy;
};

namespace Gdiplus {
struct Point {
    int X;
    int Y;

    Point() = default;
    Point(int x, int y) : X(x), Y(y) {}
};
struct PointF {
    float X;
    float Y;

    PointF() = default;
    PointF(float x, float y) : X(x), Y(y) {}
};
struct Rect {
    int X;
    int Y;
    int Width;
    int Height;

    Rect() = default;
    Rect(int x, int y, int width, int height) : X(x), Y(y), Width(width), Height(height) {}
};
struct RectF {
    float X;
    float Y;
    float Width;
    float Height;

    RectF() = default;
    RectF(float x, float y, float width, float height) : X(x), Y(y), Width(width), Height(height) {}
};
struct Color {
    uint32_t argb = 0;

    Color() = default;
    explicit Color(uint32_t argb) : argb(argb) {}
    Color(uint8_t r, uint8_t g, uint8_t b) : argb((uint32_t)0xff << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | b) {}
    Color(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        : argb((uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | b) {}
};
} // namespace Gdiplus

#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#endif

using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using uint = unsigned int;

#if OS_WIN
using AtomicBool = volatile LONG;
using AtomicInt = volatile LONG;
using AtomicRefCount = volatile LONG;
#else
using AtomicBool = volatile int;
using AtomicInt = volatile int;
using AtomicRefCount = volatile int;
#endif

bool AtomicBoolGet(AtomicBool* p);
void AtomicBoolSet(AtomicBool* p, bool v);
int AtomicIntGet(AtomicInt* p);
void AtomicIntSet(AtomicInt* p, int v);
int AtomicIntAdd(AtomicInt* p, int v);
int AtomicIntInc(AtomicInt* p);
int AtomicIntDec(AtomicInt* p);
int AtomicRefCountAdd(AtomicRefCount* v);
int AtomicRefCountDec(AtomicRefCount* v);

struct Arena;

struct Str {
    char* s;
    int len;

    Str() : s(nullptr), len(0) {}
    Str(const char* s_) : s((char*)s_), len(0) { len = s_ ? (int)strlen(s_) : 0; }
    explicit Str(const char* s_, int len_) : s((char*)s_), len(len_) {}
    explicit Str(char* s_) : s(s_), len(0) { len = s ? (int)strlen(s) : 0; }
    explicit Str(char* s_, int len_) : s(s_), len(len_) {}

    explicit operator bool() const { return len > 0 && s; }
};

// exists just to mark the intent, needed by both Str.h and TempAllocator.h
using TempStr = Str;

// Create Str from string literal with compile-time length
#define StrL(lit) Str((char*)(lit), (int)(sizeof(lit) - 1))

// Compile-time length (as int) of a string literal (or char[]/WCHAR[] array);
// faster than len() which does a runtime strlen. Works for both narrow
// and wide literals since dimof counts elements. Not for decayed pointers.
#define LenL(lit) (dimofi(lit) - 1)

Str AllocStrTemp(int size);

struct WStr {
    wchar_t* s;
    int len;

    WStr() : s(nullptr), len(0) {}
    WStr(const wchar_t* s_) : s((wchar_t*)s_), len(0) {
        while (s_ && s_[len]) len++;
    }
    explicit WStr(const wchar_t* s_, int len_) : s((wchar_t*)s_), len(len_) {}
    explicit WStr(wchar_t* s_) : s(s_), len(0) {
        while (s && s[len]) len++;
    }
    explicit WStr(wchar_t* s_, int len_) : s(s_), len(len_) {}

    explicit operator bool() const { return len > 0 && s; }
};

// exists just to mark the intent, needed by both Str.h and TempAllocator.h
using TempWStr = WStr;

// Create WStr from wide string literal with compile-time length
#define WStrL(lit) WStr((wchar_t*)(lit), (int)(sizeof(lit) / sizeof(wchar_t) - 1))

// length of a Str / WStr as int. Also accepts a C string (char* / wchar_t*) via
// Str/WStr's implicit ctor, like the former str::Leni / wstr::Leni it replaces.
inline int len(Str s) {
    return s.len;
}
inline int len(WStr s) {
    return s.len;
}

struct VecStr {
    int len;
    int cap;
    Str* els;
};

#if COMPILER_MSVC
#define NO_INLINE __declspec(noinline)
#define FORCEINLINE __forceinline
#else
// assuming gcc or similar
#define NO_INLINE __attribute__((noinline))
#define FORCEINLINE inline __attribute__((always_inline))
#endif

#define NoOp() ((void)0)

template <typename T, size_t N>
char (&DimofSizeHelper(T (&array)[N]))[N];
#define dimof(array) (sizeof(DimofSizeHelper(array)))
#define dimofi(array) (int)(sizeof(DimofSizeHelper(array)))

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

extern void _uploadDebugReport(Str, Str, bool, bool);

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define FILE_LINE __FILE__ ":" STRINGIZE(__LINE__)

#define ReportIfCond(cond, condStr, fileLine, isCrash, captureCallstack)      \
    __analysis_assume(!(cond));                                               \
    do {                                                                      \
        if (cond) {                                                           \
            _uploadDebugReport(condStr, fileLine, isCrash, captureCallstack); \
        }                                                                     \
    } while (0)

#define ReportIf(cond) ReportIfCond(cond, #cond, FILE_LINE, false, true)
#define ReportIfFast(cond) ReportIfCond(cond, #cond, FILE_LINE, false, false)
#if defined(DEBUG)
#define ReportDebugIf(cond) ReportIfCond(cond, #cond, FILE_LINE, false, true)
#else
#define ReportDebugIf(cond)
#endif

void* AllocZero(int count, int size);

template <typename T>
FORCEINLINE T* AllocArray(int n) {
    return (T*)AllocZero(n, (int)sizeof(T));
}

template <typename T>
FORCEINLINE T* AllocStruct() {
    return (T*)AllocZero(1, (int)sizeof(T));
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
    if (n == 0 || val == 0) {
        return true;
    }
    ReportIf(n < 0);
    ReportIf(val < 0);
    T res = val + n;
    return val > res;
}

// return false if adding n to val overflows. Only valid for n > 0
template <typename T>
inline bool addSafe(T* valInOut, T n) {
    if (n == 0 || *valInOut == 0) {
        valInOut = 0;
        return true;
    }
    ReportIf(n < 0);
    ReportIf(*valInOut < 0);
    T res = *valInOut + n;
    if (res < *valInOut) {
        return false;
    }
    *valInOut = res;
    return true;
}

// return false if multiplying val by n overflows. Only valid for n > 0
template <typename T>
inline bool mulSafe(T* valInOut, T n) {
    if (n == 0 || *valInOut == 0) {
        *valInOut = 0;
        return true;
    }
    ReportIf(n < 0);
    ReportIf(*valInOut < 0);
    T res = *valInOut * n;
    if (res < *valInOut || res < n) {
        // multiplication overflowed
        return false;
    }
    *valInOut = res;
    return true;
}

void* memdup(const void* data, int n, int extraBytes = 0);
bool memeq(const void* s1, const void* s2, int n);

int RoundToPowerOf2(int size);
u32 MurmurHash2(const void* key, int n);
u32 MurmurHash2(Str s);
u32 MurmurHash2(WStr s);
u32 MurmurHashWStrI(WStr str);
u32 MurmurHashStrI(Str s);

int RoundUp(int n, int rounding);
void* RoundUp(void* d, int rounding);

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
    Kind kind = nullptr;

    Kind GetKind() const { return kind; }
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
    ~ExitScope() { lambda(); }
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

    bool IsEmpty() const { return fn == nullptr; }
    bool IsValid() const { return fn != nullptr; }
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

template <typename T, void (T::*Method)()>
static void MethodTrampoline(void* obj) {
    (static_cast<T*>(obj)->*Method)();
}

template <typename T, void (T::*Method)()>
Func0 MkMethod0(T* obj) {
    auto res = Func0{};
    res.fn = (void*)&MethodTrampoline<T, Method>;
    res.userData = (void*)obj;
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

    bool IsValid() const { return fn != nullptr; }
    bool IsEmpty() const { return fn == nullptr; }
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

template <typename T, typename TArg, void (T::*Method)(TArg)>
static void MethodTrampoline1(void* obj, TArg arg) {
    (static_cast<T*>(obj)->*Method)(arg);
}

template <typename T, typename TArg, void (T::*Method)(TArg)>
Func1<TArg> MkMethod1(T* obj) {
    auto res = Func1<TArg>{};
    using fptr = void (*)(void*, TArg);
    res.fn = (fptr)&MethodTrampoline1<T, TArg, Method>;
    res.userData = (void*)obj;
    return res;
}

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

int setMinMax(int& v, int minVal, int maxVal);

#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

extern AtomicInt gAllowAllocFailure;

/* How to use:
AutoCall freeToolsFileName(free, (void*)tools_filename);
AutoCall closeFile(fclose, f);
defer { instance->Release(); };
*/

#include "Geom.h"
#include "Vec.h"
#include "Str.h"
#include "StrUtf8.h"
#include "StrFormatParse.h"
#include "StrVec.h"
#include "Strconv.h"
#include "Thread.h"
#include "Arena.h"
#include "Scoped.h"
#include "Color.h"

// lstrcpy is dangerous so forbid using it
#ifdef lstrcpy
#undef lstrcpy
#define lstrcpy dont_use_lstrcpy
#endif

#include "Log.h"

#endif

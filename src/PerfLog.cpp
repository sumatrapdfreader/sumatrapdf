/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Timer.h"
#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "PerfLog.h"

#if !IS_PERF_LOG

void InitPerfLog() {}
void StartPerfLog() {}
void StopPerfLog() {}
void SetPerfLogPath(Str) {}
void SavePerfLog() {}
void DestroyPerfLog() {}

#else

constexpr int kMaxPerfDepth = 256;
constexpr int kMaxPerfLogBytes = 256 * 1024 * 1024;
// skip deep frames so a restore profile is not 1M str::IsNull lines
constexpr int kMaxPerfLogDepth = 10;
constexpr int kSymCap = 64 * 1024;
constexpr int kLineBuf = 1024;

struct PerfSym {
    const void* addr;
    const char* name;
};

static Arena* gPerfArena = nullptr;
static Mutex gSymMutex;
static AtomicInt gPerfOn = 0;
static DWORD gMainThreadId = 0;
static LARGE_INTEGER gQpcFreq = {};
static Str gPerfLogPath;
static bool gDbgHelpOk = false;
static bool gPerfFull = false;
static char* gRaw = nullptr;
static LONG gRawUsed = 0;

static PerfSym* gSyms = nullptr;
static int gSymCap = 0;
static int gSymN = 0;

static thread_local int gInHook = 0;
static thread_local int gPerfDepth = 0;
static thread_local LARGE_INTEGER gStartStack[kMaxPerfDepth];

static char HexDigit(u32 v) {
    return "0123456789abcdef"[v & 15];
}

static int AppendHex(char* d, u64 v) {
    char tmp[16];
    int n = 0;
    do {
        tmp[n++] = HexDigit((u32)v);
        v >>= 4;
    } while (v);
    for (int i = 0; i < n; i++) {
        d[i] = tmp[n - 1 - i];
    }
    return n;
}

static u32 HashPtr(const void* p) {
    u64 x = (u64)(uintptr_t)p;
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ull;
    x ^= x >> 27;
    return (u32)x;
}

static PerfSym* FindSymSlot(const void* addr, bool forInsert) {
    u32 capMask = (u32)gSymCap - 1;
    u32 slot = HashPtr(addr) & capMask;
    for (int n = 0; n < gSymCap; n++) {
        PerfSym& e = gSyms[slot];
        if (!e.addr) {
            return forInsert ? &e : nullptr;
        }
        if (e.addr == addr) {
            return &e;
        }
        slot = (slot + 1) & capMask;
    }
    return nullptr;
}

static int FormatLine(char* d, int depth, DWORD tid, const void* addr, bool isExit, u64 us) {
    int n = 0;
    int indent = depth * 2;
    if (indent > 200) {
        indent = 200;
    }
    for (int i = 0; i < indent; i++) {
        d[n++] = ' ';
    }
    if (tid != gMainThreadId) {
        n += AppendHex(d + n, tid);
        d[n++] = ' ';
    }
    d[n++] = '0';
    d[n++] = 'x';
    n += AppendHex(d + n, (u64)(uintptr_t)addr);
    if (isExit) {
        d[n++] = ' ';
        d[n++] = ' ';
        n += AppendHex(d + n, us);
    }
    d[n++] = '\n';
    d[n] = 0;
    return n;
}

static void AppendLine(const char* s, int n) {
    if (gPerfFull || !gRaw || n <= 0) {
        return;
    }
    LONG end = InterlockedAdd(&gRawUsed, n);
    LONG start = end - n;
    if (start < 0 || end > kMaxPerfLogBytes) {
        gPerfFull = true;
        return;
    }
    memcpy(gRaw + start, s, (size_t)n);
}

static void EnsurePerfLog() {
    if (gRaw) {
        return;
    }
    gRaw = (char*)VirtualAlloc(nullptr, (SIZE_T)kMaxPerfLogBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    gRawUsed = 0;
    QueryPerformanceFrequency(&gQpcFreq);
    gPerfArena = ArenaNew();
    gSymCap = kSymCap;
    gSyms = (PerfSym*)gPerfArena->Push((u64)gSymCap * sizeof(PerfSym), 8, true);
}

extern "C" void PerfEnterImpl(void* addr) {
    if (gInHook) {
        return;
    }
    gInHook = 1;

    if (!AtomicIntGet(&gPerfOn)) {
        gInHook = 0;
        return;
    }

    int depth = gPerfDepth;
    if (gPerfDepth < kMaxPerfDepth) {
        gStartStack[gPerfDepth] = TimeGet();
    }
    gPerfDepth++;

    if (depth <= kMaxPerfLogDepth) {
        char buf[kLineBuf];
        int n = FormatLine(buf, depth, GetCurrentThreadId(), addr, false, 0);
        AppendLine(buf, n);
    }

    gInHook = 0;
}

extern "C" void PerfExitImpl(void* addr) {
    if (gInHook) {
        return;
    }
    gInHook = 1;

    if (gPerfDepth <= 0) {
        gInHook = 0;
        return;
    }
    gPerfDepth--;

    u64 us = 0;
    if (gPerfDepth < kMaxPerfDepth && gQpcFreq.QuadPart) {
        LARGE_INTEGER now = TimeGet();
        u64 delta = (u64)(now.QuadPart - gStartStack[gPerfDepth].QuadPart);
        us = (delta * 1000000ull) / (u64)gQpcFreq.QuadPart;
    }

    if (!AtomicIntGet(&gPerfOn)) {
        gInHook = 0;
        return;
    }

    if (gPerfDepth <= kMaxPerfLogDepth) {
        char buf[kLineBuf];
        int n = FormatLine(buf, gPerfDepth, GetCurrentThreadId(), addr, true, us);
        AppendLine(buf, n);
    }

    gInHook = 0;
}

void InitPerfLog() {
    gMainThreadId = GetCurrentThreadId();
}

void StartPerfLog() {
    EnsurePerfLog();
    if (len(gPerfLogPath) == 0) {
        gPerfLogPath = str::Dup(GetPathInExeDirTemp(StrL("sumperf.txt")));
    }
    AtomicIntSet(&gPerfOn, 1);
    AppendLine("perf log start\n", LenL("perf log start\n"));
}

void StopPerfLog() {
    AtomicIntSet(&gPerfOn, 0);
}

void SetPerfLogPath(Str path) {
    str::FreePtr(&gPerfLogPath);
    gPerfLogPath = str::Dup(path);
}

static void IndexLogAddrs(Str src) {
    int i = 0;
    while (i + 2 < src.len) {
        if (src.s[i] != '0' || src.s[i + 1] != 'x') {
            i++;
            continue;
        }
        int n = 0;
        u64 v = 0;
        bool any = false;
        for (n = 2; i + n < src.len; n++) {
            char c = src.s[i + n];
            int d = -1;
            if (c >= '0' && c <= '9') {
                d = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                d = c - 'a' + 10;
            } else {
                break;
            }
            v = (v << 4) | (u64)d;
            any = true;
        }
        if (any && gSymN * 2 < gSymCap) {
            const void* addr = (const void*)(uintptr_t)v;
            PerfSym* e = FindSymSlot(addr, true);
            if (e && !e->addr) {
                e->addr = addr;
                gSymN++;
            }
        }
        i += n > 0 ? n : 1;
    }
}

static void FillSymNames() {
    if (!gDbgHelpOk || !DynSymFromAddr || !gSyms) {
        return;
    }

    char symBuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* info = (SYMBOL_INFO*)symBuf;
    for (int i = 0; i < gSymCap; i++) {
        PerfSym& e = gSyms[i];
        if (!e.addr) {
            continue;
        }
        memset(symBuf, 0, sizeof(symBuf));
        info->SizeOfStruct = sizeof(SYMBOL_INFO);
        info->MaxNameLen = 512;
        DWORD64 disp = 0;
        if (!DynSymFromAddr(GetCurrentProcess(), (DWORD64)e.addr, &disp, info) || !info->Name[0]) {
            continue;
        }
        e.name = str::Dup(gPerfArena, Str(info->Name)).s;
    }
}

static const char* NameForHexAddr(Str hex, int* nOut) {
    if (hex.len < 3 || hex.s[0] != '0' || hex.s[1] != 'x') {
        return nullptr;
    }
    u64 v = 0;
    int n = 2;
    for (; n < hex.len; n++) {
        char c = hex.s[n];
        int d = -1;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else {
            break;
        }
        v = (v << 4) | (u64)d;
    }
    if (n == 2) {
        return nullptr;
    }
    *nOut = n;
    PerfSym* e = FindSymSlot((const void*)(uintptr_t)v, false);
    if (e && e->name) {
        return e->name;
    }
    return nullptr;
}

static Str RawLog() {
    LONG n = gRawUsed;
    if (n > kMaxPerfLogBytes) {
        n = kMaxPerfLogBytes;
    }
    if (n < 0 || !gRaw) {
        return {};
    }
    return Str(gRaw, (int)n);
}

static Str RewriteLogTemp() {
    Str src = RawLog();
    str::Builder dst(gPerfArena);
    dst.Reserve(src.len + 16);
    int i = 0;
    while (i < src.len) {
        if (i + 2 < src.len && src.s[i] == '0' && src.s[i + 1] == 'x') {
            int n = 0;
            const char* name = NameForHexAddr(Str(src.s + i, src.len - i), &n);
            if (name) {
                dst.Append(Str(name));
                i += n;
                continue;
            }
        }
        dst.AppendChar(src.s[i]);
        i++;
    }
    return str::Dup(gPerfArena, ToStr(dst));
}

void SavePerfLog() {
    if (!gRaw) {
        return;
    }
    Str path = gPerfLogPath;
    if (len(path) == 0) {
        path = GetPathInExeDirTemp(StrL("sumperf.txt"));
    }
    path = str::Dup(path);

    StopPerfLog();

    Str raw = RawLog();
    file::WriteFile(path, raw);

    if (!gDbgHelpOk) {
        gDbgHelpOk = dbghelp::Initialize(ToWStrTemp(GetSelfExeDirTemp()), false);
    }
    if (gDbgHelpOk) {
        gSymMutex.Lock();
        IndexLogAddrs(raw);
        FillSymNames();
        Str named = RewriteLogTemp();
        gSymMutex.Unlock();
        file::WriteFile(path, named);
    }

    str::Free(path);
}

void DestroyPerfLog() {
    AtomicIntSet(&gPerfOn, 0);
    gSyms = nullptr;
    ArenaDelete(gPerfArena);
    gPerfArena = nullptr;
    if (gRaw) {
        VirtualFree(gRaw, 0, MEM_RELEASE);
        gRaw = nullptr;
    }
    gRawUsed = 0;
    str::FreePtr(&gPerfLogPath);
}

#endif

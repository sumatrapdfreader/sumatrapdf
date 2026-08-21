/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Detects a blocked UI thread and logs what every one of our threads was doing.
//
// A watchdog thread pings the UI thread with a message and times the round
// trip. If the UI thread doesn't pump messages within kHangThresholdMs, all
// other threads are frozen, a raw callstack is captured from each, they're
// resumed, and only then are the addresses symbolized and logged (symbolizing
// while threads are frozen could block on a heap or loader lock one of them
// holds).

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "base/File.h"
#include "base/Timer.h"
#include "base/Win.h"

#include "SumatraConfig.h"
#include "CrashHandler.h"
#include "HangDetector.h"

// how long the UI thread can ignore our ping before we call it blocked
constexpr int kHangThresholdMs = 3000;
// pause between pings; the ping itself blocks for up to kHangThresholdMs
constexpr int kPingIntervalMs = 1000;
// callstacks are noisy and a hang tends to repeat, so cap them per session
constexpr int kMaxHangReports = 10;
constexpr int kMaxThreads = 128;
constexpr int kMaxFrames = 64;

struct ThreadStack {
    ThreadId tid;
    int nAddrs;
    u64 addrs[kMaxFrames];
};

static HWND gHwndPing = nullptr;
static HANDLE gStopEvent = nullptr;
static ThreadHandle gWatchdogThread = nullptr;
static ThreadId gUiThreadId = 0;
static ThreadId gWatchdogThreadId = 0;
static int gHangReportsLeft = kMaxHangReports;
// buffers are allocated up front: nothing may allocate while threads are frozen
static ThreadStack* gStacks = nullptr;
static ThreadHandle* gThreadHandles = nullptr;

bool IsUiHangDetectorRunning() {
    return gWatchdogThread != nullptr;
}

// Symbols: prefer a .pdb next to the .exe (what a local build has), then
// whatever the crash handler downloaded into gSymbolsDir.
static bool EnsureSymbols() {
    str::Builder symPath(1024);
    symPath.a = GetTempArena();
    symPath.Append(GetSelfExeDirTemp());
    if (len(gSymbolsDir) > 0) {
        symPath.Append(";");
        symPath.Append(gSymbolsDir);
    }
    TempWStr ws = ToWStrTemp(ToStrTemp(symPath));
    if (!dbghelp::Initialize(ws, false)) {
        return false;
    }
    if (dbghelp::HasSymbols()) {
        return true;
    }
    // dbghelp was already initialized with a path that doesn't find our .pdb
    // (e.g. by the crash handler), so re-initialize with ours
    if (!dbghelp::Initialize(ws, true)) {
        return false;
    }
    return dbghelp::HasSymbols();
}

// Only log threads that are ours: the UI thread, or one started by
// StartThread() / RunAsync() (they all run under ThreadFunc0). Threads from the
// system or a third-party dll tell us nothing about our hang.
static bool IsOurThread(ThreadId tid, Str callstack) {
    if (tid == gUiThreadId) {
        // the UI thread's stack can be deeper than kMaxFrames while blocked, so
        // don't rely on WinMain still being in the captured part
        return true;
    }
    static Str validThreads[] = {
        StrL("WinMain"),
        StrL("ThreadFunc0"),
        StrL("RenderCacheThread"),
    };
    for (auto& s : validThreads) {
        if (str::Contains(callstack, s)) return true;
    }
    return false;
}

// Suspends every thread of this process except the watchdog, captures raw
// callstack addresses, then resumes them. Returns how many threads were
// captured. Allocates nothing between the first suspend and the last resume.
static int CollectFrozenStacks() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD pid = GetCurrentProcessId();
    DWORD access = THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME;
    int n = 0;
    THREADENTRY32 te32;
    te32.dwSize = sizeof(te32);
    BOOL ok = Thread32First(snap, &te32);
    while (ok && n < kMaxThreads) {
        bool skip = te32.th32OwnerProcessID != pid || te32.th32ThreadID == gWatchdogThreadId;
        if (!skip) {
            ThreadHandle h = OpenThread(access, false, te32.th32ThreadID);
            if (h) {
                gThreadHandles[n] = h;
                gStacks[n].tid = te32.th32ThreadID;
                gStacks[n].nAddrs = 0;
                n++;
            }
        }
        ok = Thread32Next(snap, &te32);
    }
    CloseHandle(snap);

    for (int i = 0; i < n; i++) {
        SuspendThread(gThreadHandles[i]);
    }
    for (int i = 0; i < n; i++) {
        gStacks[i].nAddrs = dbghelp::GetSuspendedThreadCallstackAddrs(gThreadHandles[i], gStacks[i].addrs, kMaxFrames);
    }
    for (int i = 0; i < n; i++) {
        ResumeThread(gThreadHandles[i]);
    }

    for (int i = 0; i < n; i++) {
        SafeCloseThreadHandle(&gThreadHandles[i]);
    }
    return n;
}

static void ReportHang(double blockedMs) {
    if (gHangReportsLeft <= 0) {
        return;
    }
    if (!EnsureSymbols()) {
        // a callstack of bare addresses helps no one
        return;
    }
    // warm up dbghelp (module list, our .pdb) before anything is frozen, so
    // the frozen window doesn't have to wait on a first-time symbol load
    dbghelp::GetCurrentThreadCallstackTemp();

    int nThreads = CollectFrozenStacks();
    if (nThreads == 0) {
        return;
    }

    gHangReportsLeft--;
    str::Builder s(8 * 1024);
    int nSkipped = 0;
    for (int i = 0; i < nThreads; i++) {
        ThreadStack& ts = gStacks[i];
        if (ts.nAddrs == 0) {
            nSkipped++;
            continue;
        }
        str::Builder cs(2048);
        for (int j = 0; j < ts.nAddrs; j++) {
            dbghelp::GetAddressInfo(cs, (DWORD64)ts.addrs[j], false);
        }
        Str csStr = ToStr(cs);
        if (!IsOurThread(ts.tid, csStr)) {
            nSkipped++;
            continue;
        }
        Str what = (ts.tid == gUiThreadId) ? StrL(" (UI thread)") : StrL("");
        s.Append(fmt("\nThread %x%s\n", (int)ts.tid, what));
        s.Append(csStr);
    }
    logf("UI thread blocked for at least %d ms (report %d of %d, %d threads not ours skipped)\n%s\n", (int)blockedMs,
         kMaxHangReports - gHangReportsLeft, kMaxHangReports, nSkipped, ToStr(s));
}

static void HangDetectorThreadFunc() {
    gWatchdogThreadId = GetCurrentThreadId();
    bool inHang = false;
    for (;;) {
        DWORD res = WaitForSingleObject(gStopEvent, kPingIntervalMs);
        if (res != WAIT_TIMEOUT) {
            return; // stopped (or the event went away)
        }
        // WM_NULL is handled by DefWindowProc: this only measures how long
        // until the UI thread gets around to pumping messages
        auto t = TimeGet();
        DWORD_PTR unused = 0;
        LRESULT ok = SendMessageTimeoutW(gHwndPing, WM_NULL, 0, 0, SMTO_NORMAL, kHangThresholdMs, &unused);
        double ms = TimeSinceInMs(t);
        if (ok != 0) {
            if (inHang) {
                logf("UI thread is responsive again (ping took %.0f ms)\n", ms);
                inHang = false;
            }
            continue;
        }
        if (GetLastError() != ERROR_TIMEOUT) {
            continue; // window is gone, we're shutting down
        }
        if (inHang) {
            continue; // same hang, already reported
        }
        inHang = true;
        ReportHang(ms);
    }
}

// Call from the UI thread. Enabled by default in debug builds (see WinMain);
// release builds can turn it on programmatically by calling this.
void StartUiHangDetector() {
    if (IsUiHangDetectorRunning()) {
        return;
    }
    // message-only window owned by the UI thread: a ping to it is answered by
    // the UI thread's message loop, which is exactly what we want to time
    gHwndPing = CreateWindowExW(0, WC_STATICW, L"SumatraPDF hang detector", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (!gHwndPing) {
        log("StartUiHangDetector: failed to create the ping window\n");
        return;
    }
    gStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    gStacks = AllocArray<ThreadStack>(nullptr, kMaxThreads);
    gThreadHandles = AllocArray<ThreadHandle>(nullptr, kMaxThreads);
    if (!gStopEvent || !gStacks || !gThreadHandles) {
        StopUiHangDetector();
        return;
    }
    gUiThreadId = GetCurrentThreadId();
    auto fn = MkFunc0Void(HangDetectorThreadFunc);
    gWatchdogThread = StartThread(fn, "HangDetector");
    if (!gWatchdogThread) {
        StopUiHangDetector();
        return;
    }
    logf("StartUiHangDetector: threshold %d ms, up to %d reports\n", kHangThresholdMs, kMaxHangReports);
}

void StopUiHangDetector() {
    if (gStopEvent) {
        SetEvent(gStopEvent);
    }
    if (gWatchdogThread) {
        // the ping blocks for up to kHangThresholdMs, so give it that long
        WaitForSingleObject(gWatchdogThread, kHangThresholdMs + 1000);
        SafeCloseThreadHandle(&gWatchdogThread);
    }
    SafeCloseHandle(&gStopEvent);
    if (gHwndPing) {
        DestroyWindow(gHwndPing);
        gHwndPing = nullptr;
    }
    Free(nullptr, gStacks);
    gStacks = nullptr;
    Free(nullptr, (void*)gThreadHandles);
    gThreadHandles = nullptr;
}

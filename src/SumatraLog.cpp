/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/FileWatcher.h"
#include "SumatraLog.h"

constexpr const WCHAR* kPipeName = L"\\\\.\\pipe\\LOCAL\\ArsLexis-Logger";

Str gLogAppName = StrL("SumatraPDF");

Mutex gLogMutex;

// we use a dedicated Arena so we can do logging during crash handling
// where we want to avoid allocator deadlocks by calling malloc()
Arena* gLogAllocator = nullptr;

str::Builder* gLogBuf = nullptr;
bool gLogToConsole = false;
// we always log if IsDebuggerPresent()
// this forces logging to debuger always
bool gLogToDebugger = false;
// meant to avoid doing stuff during crash reporting
// will log to debugger (if no need for formatting)
bool gReducedLogging = false;
// when main thread exists other threads might still
// try to log. when true, this stops logging
bool gDestroyedLogging = false;

// if true, doesn't log if the same text has already been logged
// reduces logging but also can be confusing i.e. log lines are not showing up
static bool gSkipDuplicateLines = false;

bool gLogToPipe = true;
static HANDLE hLogPipe = INVALID_HANDLE_VALUE;
static Mutex gPipeMutex;

Str gLogFilePath;

// 1 MB - 128 to stay under 1 MB even after appending (an estimate)
constexpr int kMaxLogBuf = (1024 * 1024) - 128;

static LARGE_INTEGER lastPipeOpenTryTime = {};

static void maybeOpenLogPipe() {
    // only re-try every 10 secs to minimize cost because pipe is rarely
    // opened and logging is frequent
    if (lastPipeOpenTryTime.QuadPart != 0) {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double diffSecs =
            static_cast<double>(now.QuadPart - lastPipeOpenTryTime.QuadPart) / static_cast<double>(freq.QuadPart);
        if (diffSecs < 10.0f) {
            return;
        }
    }
    QueryPerformanceCounter(&lastPipeOpenTryTime);
    hLogPipe = CreateFileW(kPipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    // TODO: retry if ERROR_PIPE_BUSY?
}

static void logToPipe(Str s) {
    if (!gLogToPipe) {
        return;
    }
    if (!s) {
        return;
    }
    size_t n = (size_t)s.len;

    gPipeMutex.Lock();

    DWORD cbWritten = 0;
    bool didConnect = false;
    if (!IsValidHandle(hLogPipe)) {
        maybeOpenLogPipe();
        if (!IsValidHandle(hLogPipe)) {
            gPipeMutex.Unlock();
            return;
        }
        didConnect = true;
    }

    if (didConnect) {
        // logview accepts logging from anyone, so announce ourselves
        TempStr initialMsg = fmt("app: %s\n", gLogAppName);
        WriteFile(hLogPipe, initialMsg.s, (DWORD)initialMsg.len, &cbWritten, nullptr);
    }

    DWORD cb = (DWORD)n;
    BOOL ok = WriteFile(hLogPipe, s.s, cb, &cbWritten, nullptr);
    if (!ok) {
        CloseHandle(hLogPipe);
        hLogPipe = INVALID_HANDLE_VALUE;
    }

    gPipeMutex.Unlock();
}

static void log2(Str s, bool always) {
    bool skipLog = !always && gSkipDuplicateLines && gLogBuf && str::Contains(*gLogBuf, s);

    if (!skipLog) {
        // in reduced logging mode, we do want to log to at least the debugger
        if (gLogToDebugger || IsDebuggerPresent() || gReducedLogging) {
            OutputDebugStringA(s.s);
        }
    }
    if (gDestroyedLogging) {
        return;
    }
    if (gReducedLogging) {
        // if the pipe already connected, do log to it even if disabled
        // we do want easy logging, just want to reduce doing stuff
        // that can break crash handling
        if (gLogToPipe && IsValidHandle(hLogPipe)) {
            logToPipe(s);
        }
        return;
    }
    gLogMutex.Lock();

    AtomicIntInc(&gAllowAllocFailure);
    AutoCall decAllowAlloc(AtomicIntDec, &gAllowAllocFailure);

    if (!gLogBuf) {
        gLogAllocator = ArenaNew();
        gLogBuf = new str::Builder(32 * 1024);
        gLogBuf->a = gLogAllocator;
    } else {
        if (len(*gLogBuf) > kMaxLogBuf) {
            // TODO: use gLogBuf->Clear(), which doesn't free the allocated space
            gLogBuf->Reset();
        }
    }

    size_t n = (size_t)s.len;

    // when skipping, we skip buf (crash reports) and console
    // but write to file and logview
    if (!skipLog) {
        gLogBuf->Append(s);
    }

    if (!skipLog && gLogToConsole) {
        LogConsole(s);
    }

    if (gLogFilePath) {
        auto* f = fopen(gLogFilePath.s, "a");
        if (f != nullptr) {
            fwrite(s.s, 1, n, f);
            fflush(f);
            fclose(f);
        }
    }
    logToPipe(s);
    gLogMutex.Unlock();
}

void log(Str s) {
    log2(s, false);
}

void loga(Str s) {
    if (gDestroyedLogging) {
        return;
    }
    log2(s, true);
}

void StartLogToFile(Str path, bool removeIfExists) {
    ReportIf(gLogFilePath);
    gLogFilePath = str::Dup(path);
    FileWatcherSetSkipPath(gLogFilePath);
    if (removeIfExists) {
        file::Delete(path);
    }
}

bool WriteCurrentLogToFile(Str path) {
    if (!gLogBuf) return false;
    Str slice = ToStr(*gLogBuf);
    if (len(slice) == 0) {
        return false;
    }
    bool ok = dir::CreateForFile(path);
    if (!ok) {
        logf("WriteCurrentLogToFile: dir::CreateForFile('%s') failed\n", path);
        return false;
    }
    ok = file::WriteFile(path, slice);
    if (!ok) {
        logf("WriteCurrentLogToFile: file::WriteFile('%s') failed\n", path);
    }
    return ok;
}

void DestroyLogging() {
    gDestroyedLogging = true;
    gLogMutex.Lock();
    delete gLogBuf;
    gLogBuf = nullptr;
    ArenaDelete(gLogAllocator);
    gLogAllocator = nullptr;
    gLogMutex.Unlock();
    str::FreePtr(&gLogFilePath);
    FileWatcherSetSkipPath(Str());
}

// --- parent process chain (startup diagnostics) --------------------------------

// Parent PID of process `pid` via Toolhelp snapshot, or 0 on failure.
static DWORD GetProcessParentPid(DWORD pid) {
    if (pid == 0) {
        return 0;
    }
    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snap) {
        return 0;
    }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(snap, &pe)) {
        return 0;
    }
    do {
        if (pe.th32ProcessID == pid) {
            return pe.th32ParentProcessID;
        }
    } while (Process32NextW(snap, &pe));
    return 0;
}

static TempStr GetProcessImagePathTemp(DWORD pid) {
    if (pid == 0) {
        return {};
    }
    AutoCloseHandle hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc.IsValid()) {
        return {};
    }
    // Long paths can exceed MAX_PATH; grow if needed.
    DWORD cap = MAX_PATH;
    for (int attempt = 0; attempt < 3; attempt++) {
        WCHAR* buf = AllocArrayTemp<WCHAR>((int)cap);
        DWORD n = cap;
        if (QueryFullProcessImageNameW(hProc, 0, buf, &n)) {
            return ToUtf8Temp(WStr(buf, (int)n));
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return {};
        }
        cap *= 2;
    }
    return {};
}

// Process command line via NtQueryInformationProcess ProcessCommandLineInformation
// (Windows 10+). Returns empty if unavailable or access denied.
static TempStr GetProcessCommandLineTemp(DWORD pid) {
    if (pid == 0) {
        return {};
    }
    using NtQueryInformationProcessFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static NtQueryInformationProcessFn ntQip = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll) {
            ntQip = (NtQueryInformationProcessFn)GetProcAddress(ntdll, "NtQueryInformationProcess");
        }
    }
    if (!ntQip) {
        return {};
    }

    AutoCloseHandle hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc.IsValid()) {
        return {};
    }

    // ProcessCommandLineInformation = 60 (Windows 10+)
    constexpr ULONG kProcessCommandLineInformation = 60;
    ULONG retLen = 0;
    LONG status = ntQip(hProc, kProcessCommandLineInformation, nullptr, 0, &retLen);
    // STATUS_INFO_LENGTH_MISMATCH (0xC0000004) expected when sizing
    (void)status;
    if (retLen == 0 || retLen > 64 * 1024) {
        return {};
    }
    void* buf = malloc(retLen);
    if (!buf) {
        return {};
    }
    status = ntQip(hProc, kProcessCommandLineInformation, buf, retLen, &retLen);
    if (status < 0) {
        free(buf);
        return {};
    }
    // Buffer is a UNICODE_STRING (Length, MaximumLength, Buffer*) with data following.
    struct {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    }* us = (decltype(us))buf;
    TempStr out{};
    if (us->Buffer && us->Length >= sizeof(WCHAR)) {
        int nChars = (int)(us->Length / sizeof(WCHAR));
        out = ToUtf8Temp(WStr(us->Buffer, nChars));
    }
    free(buf);
    return out;
}

// Log parent → grandparent → … (path + command line when readable).
// Walk parent PIDs and log path + command line for each (startup diagnostics).
void LogParentProcessChain() {
    log("Parent process chain:\n");
    DWORD pid = GetCurrentProcessId();
    DWORD seen[16]{};
    int nSeen = 0;
    for (int depth = 0; depth < 16; depth++) {
        DWORD parentPid = GetProcessParentPid(pid);
        if (parentPid == 0 || parentPid == pid) {
            if (depth == 0) {
                log("  (none / unknown)\n");
            }
            break;
        }
        bool cycle = false;
        for (int i = 0; i < nSeen; i++) {
            if (seen[i] == parentPid) {
                cycle = true;
                break;
            }
        }
        if (cycle) {
            logf("  [%d] pid=%u (cycle, stop)\n", depth, parentPid);
            break;
        }
        seen[nSeen++] = parentPid;

        TempStr path = GetProcessImagePathTemp(parentPid);
        TempStr cmd = GetProcessCommandLineTemp(parentPid);
        if (path && cmd) {
            logf("  [%d] pid=%u path='%s'\n      cmdline='%s'\n", depth, parentPid, path, cmd);
        } else if (path) {
            logf("  [%d] pid=%u path='%s'\n      cmdline=(unavailable)\n", depth, parentPid, path);
        } else if (cmd) {
            logf("  [%d] pid=%u path=(unavailable)\n      cmdline='%s'\n", depth, parentPid, cmd);
        } else {
            logf("  [%d] pid=%u path=(unavailable) cmdline=(unavailable)\n", depth, parentPid);
        }
        pid = parentPid;
    }
}

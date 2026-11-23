/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// sumatrapdf-tool.exe: a console front end for SumatraPDF.exe. SumatraPDF.exe
// is a GUI (Windows subsystem) app that can't write to a console or be piped
// reliably; this console app forwards its args to SumatraPDF.exe next to it
// over a named pipe and relays SumatraPDF.exe's stdout / stderr back, so
// everything SumatraPDF.exe's command line does works from cmd.exe /
// PowerShell. Protocol: ToolPipe.h.
//
// Usage: sumatrapdf-tool.exe <args...>   e.g. sumatrapdf-tool.exe info file.pdf

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

#include "ToolPipe.h"

constexpr DWORD kPipeBufSize = 64 * 1024;
// how often a pipe nobody will connect to anymore is unblocked, after the child exited
constexpr DWORD kUnblockPollMs = 50;

static void PrintUsage() {
    fprintf(stderr, "sumatrapdf-tool: run SumatraPDF.exe's command line from a console\n");
    fprintf(stderr, "usage: sumatrapdf-tool <args...>\n");
    fprintf(stderr, "  e.g. sumatrapdf-tool info file.pdf, sumatrapdf-tool draw -o page.png file.pdf 1\n");
    fprintf(stderr,
            "  the args are SumatraPDF.exe's: the mupdf tools (draw, convert, info, ...), cli, -print-to, ...\n");
}

// prints a message and the Windows error to stderr
static void PrintLastError(const char* what) {
    DWORD err = GetLastError();
    char msg[512] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, msg, sizeof(msg),
                   nullptr);
    fprintf(stderr, "sumatrapdf-tool: %s: %s", what, msg[0] ? msg : "unknown error\n");
}

// args as [u32 byte count][each arg as UTF-8 + '\0'], from argv[1] on
static char* BuildArgsPayload(int argc, WCHAR** argv, DWORD* sizeOut) {
    DWORD total = 0;
    for (int i = 1; i < argc; i++) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (n <= 0) {
            return nullptr;
        }
        total += (DWORD)n;
    }
    if (total > kToolPipeMaxArgsBytes) {
        return nullptr;
    }
    char* buf = (char*)malloc(sizeof(DWORD) + total);
    if (!buf) {
        return nullptr;
    }
    memcpy(buf, &total, sizeof(DWORD));
    char* p = buf + sizeof(DWORD);
    for (int i = 1; i < argc; i++) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, p, (int)total, nullptr, nullptr);
        p += n;
    }
    *sizeOut = (DWORD)sizeof(DWORD) + total;
    return buf;
}

static HANDLE CreateServerPipe(const WCHAR* base, const WCHAR* suffix, DWORD access) {
    WCHAR name[MAX_PATH];
    swprintf_s(name, L"%s%s", base, suffix);
    DWORD mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    return CreateNamedPipeW(name, access | FILE_FLAG_FIRST_PIPE_INSTANCE, mode, 1, kPipeBufSize, kPipeBufSize, 0,
                            nullptr);
}

// connects a server pipe. A client that connected first, or even already left,
// still counts: what it wrote can be read.
static bool ConnectPipe(HANDLE h) {
    if (ConnectNamedPipe(h, nullptr)) {
        return true;
    }
    DWORD err = GetLastError();
    return err == ERROR_PIPE_CONNECTED || err == ERROR_NO_DATA;
}

// one of our standard handles that receives a child stream
struct StreamSink {
    HANDLE h = nullptr;
    // a console gets UTF-8 decoded and written with WriteConsoleW: the CRT
    // and WriteFile convert with the console code page and break on DBCS (#6276)
    bool isConsole = false;
    // a UTF-8 sequence cut by a read
    char pend[4] = {};
    int npend = 0;
};

static int Utf8SeqLen(unsigned char lead) {
    if (lead < 0xC0) {
        return 1;
    }
    if (lead < 0xE0) {
        return 2;
    }
    if (lead < 0xF0) {
        return 3;
    }
    return 4;
}

static void WriteToConsole(StreamSink* sink, const char* data, DWORD n) {
    // prepend what the last read cut, keep a sequence this one cuts
    char buf[kPipeBufSize + 4];
    int len = 0;
    memcpy(buf, sink->pend, sink->npend);
    len = sink->npend;
    memcpy(buf + len, data, n);
    len += (int)n;
    sink->npend = 0;
    for (int back = 1; back <= 3 && back <= len; back++) {
        unsigned char c = (unsigned char)buf[len - back];
        if ((c & 0xC0) == 0x80) {
            continue;
        }
        if (c >= 0xC0 && Utf8SeqLen(c) > back) {
            memcpy(sink->pend, buf + len - back, back);
            sink->npend = back;
            len -= back;
        }
        break;
    }
    if (len == 0) {
        return;
    }
    // per call: stdout and stderr are relayed on two threads
    WCHAR* wbuf = (WCHAR*)malloc(sizeof(WCHAR) * len);
    if (!wbuf) {
        return;
    }
    int wn = MultiByteToWideChar(CP_UTF8, 0, buf, len, wbuf, len);
    DWORD written = 0;
    WriteConsoleW(sink->h, wbuf, (DWORD)wn, &written, nullptr);
    free(wbuf);
}

static void WriteToSink(StreamSink* sink, const char* data, DWORD n) {
    if (sink->isConsole) {
        WriteToConsole(sink, data, n);
        return;
    }
    while (n > 0) {
        DWORD written = 0;
        if (!WriteFile(sink->h, data, n, &written, nullptr) || written == 0) {
            return;
        }
        data += written;
        n -= written;
    }
}

struct Relay {
    HANDLE pipe = nullptr;
    StreamSink sink;
    volatile LONG connected = 0;
};

// copies a child stream to our std handle until the child closes it
static DWORD WINAPI RelayThread(void* arg) {
    Relay* r = (Relay*)arg;
    if (!ConnectPipe(r->pipe)) {
        return 0;
    }
    InterlockedExchange(&r->connected, 1);
    char* buf = (char*)malloc(kPipeBufSize);
    for (;;) {
        DWORD n = 0;
        if (!ReadFile(r->pipe, buf, kPipeBufSize, &n, nullptr) || n == 0) {
            break;
        }
        WriteToSink(&r->sink, buf, n);
    }
    free(buf);
    return 0;
}

struct ArgsSender {
    HANDLE pipe = nullptr;
    const char* data = nullptr;
    DWORD size = 0;
    volatile LONG connected = 0;
};

static DWORD WINAPI SendArgsThread(void* arg) {
    ArgsSender* a = (ArgsSender*)arg;
    if (!ConnectPipe(a->pipe)) {
        return 0;
    }
    InterlockedExchange(&a->connected, 1);
    DWORD written = 0;
    WriteFile(a->pipe, a->data, a->size, &written, nullptr);
    // returns once the child read it all, so closing doesn't drop data
    FlushFileBuffers(a->pipe);
    return 0;
}

struct StdinSender {
    HANDLE pipe = nullptr;
    HANDLE src = nullptr;
};

// copies our stdin (a file or a pipe) to the child until it ends
static DWORD WINAPI SendStdinThread(void* arg) {
    StdinSender* s = (StdinSender*)arg;
    if (!ConnectPipe(s->pipe)) {
        return 0;
    }
    char* buf = (char*)malloc(kPipeBufSize);
    for (;;) {
        DWORD n = 0;
        if (!ReadFile(s->src, buf, kPipeBufSize, &n, nullptr) || n == 0) {
            break;
        }
        DWORD written = 0;
        if (!WriteFile(s->pipe, buf, n, &written, nullptr)) {
            break;
        }
    }
    free(buf);
    // returns once the child read it all; then closing the pipe is its EOF
    FlushFileBuffers(s->pipe);
    CloseHandle(s->pipe);
    return 0;
}

// a console stays the child's: it reads it directly, line editing included
static bool StdinIsFileOrPipe(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD type = GetFileType(h);
    return type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE;
}

// waits for a pipe thread after the child exited. One still waiting for the
// child to connect never will: cancel its wait.
static void FinishPipeThread(HANDLE thread, volatile LONG* connected) {
    while (WaitForSingleObject(thread, kUnblockPollMs) == WAIT_TIMEOUT) {
        if (!*connected) {
            CancelSynchronousIo(thread);
        }
    }
    CloseHandle(thread);
}

static StreamSink SinkFor(DWORD stdId) {
    StreamSink s;
    s.h = GetStdHandle(stdId);
    DWORD mode = 0;
    s.isConsole = s.h && s.h != INVALID_HANDLE_VALUE && GetConsoleMode(s.h, &mode);
    return s;
}

// the child dies with us (Ctrl+C, a killed console)
static HANDLE CreateKillOnCloseJob() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) {
        return nullptr;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
    return job;
}

int main() {
    int argc = 0;
    WCHAR** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        PrintLastError("CommandLineToArgvW() failed");
        return 1;
    }
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    // SumatraPDF.exe next to us
    WCHAR exePath[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    WCHAR* lastSep = (n > 0 && n < MAX_PATH) ? wcsrchr(exePath, L'\\') : nullptr;
    if (!lastSep) {
        PrintLastError("GetModuleFileNameW() failed");
        return 1;
    }
    wcscpy_s(lastSep + 1, MAX_PATH - (lastSep + 1 - exePath), L"SumatraPDF.exe");
    if (GetFileAttributesW(exePath) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "sumatrapdf-tool: SumatraPDF.exe not found next to sumatrapdf-tool.exe\n");
        return 1;
    }

    DWORD payloadSize = 0;
    char* payload = BuildArgsPayload(argc, argv, &payloadSize);
    if (!payload) {
        fprintf(stderr, "sumatrapdf-tool: invalid or too long arguments\n");
        return 1;
    }

    WCHAR base[128];
    swprintf_s(base, L"\\\\.\\pipe\\sumatrapdf-tool-%lu-%lu", GetCurrentProcessId(), GetTickCount());
    ArgsSender args;
    args.pipe = CreateServerPipe(base, kToolPipeArgsSuffix, PIPE_ACCESS_OUTBOUND);
    args.data = payload;
    args.size = payloadSize;
    Relay out;
    out.pipe = CreateServerPipe(base, kToolPipeOutSuffix, PIPE_ACCESS_INBOUND);
    out.sink = SinkFor(STD_OUTPUT_HANDLE);
    Relay err;
    err.pipe = CreateServerPipe(base, kToolPipeErrSuffix, PIPE_ACCESS_INBOUND);
    err.sink = SinkFor(STD_ERROR_HANDLE);
    if (args.pipe == INVALID_HANDLE_VALUE || out.pipe == INVALID_HANDLE_VALUE || err.pipe == INVALID_HANDLE_VALUE) {
        PrintLastError("CreateNamedPipeW() failed");
        return 1;
    }
    StdinSender in;
    in.src = GetStdHandle(STD_INPUT_HANDLE);
    if (StdinIsFileOrPipe(in.src)) {
        in.pipe = CreateServerPipe(base, kToolPipeInSuffix, PIPE_ACCESS_OUTBOUND);
        if (in.pipe == INVALID_HANDLE_VALUE) {
            PrintLastError("CreateNamedPipeW() failed");
            return 1;
        }
    }

    WCHAR cmdLine[MAX_PATH * 2 + 160];
    swprintf_s(cmdLine, L"\"%s\" %s %s", exePath, kToolPipeFlag, base);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(exePath, cmdLine, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        PrintLastError("starting SumatraPDF.exe failed");
        return 1;
    }
    HANDLE job = CreateKillOnCloseJob();
    if (job) {
        AssignProcessToJobObject(job, pi.hProcess);
    }
    HANDLE threads[3] = {
        CreateThread(nullptr, 0, SendArgsThread, &args, 0, nullptr),
        CreateThread(nullptr, 0, RelayThread, &out, 0, nullptr),
        CreateThread(nullptr, 0, RelayThread, &err, 0, nullptr),
    };
    HANDLE inThread = in.pipe ? CreateThread(nullptr, 0, SendStdinThread, &in, 0, nullptr) : nullptr;
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);

    FinishPipeThread(threads[0], &args.connected);
    FinishPipeThread(threads[1], &out.connected);
    FinishPipeThread(threads[2], &err.connected);
    // the child is gone: what's left of our stdin has no reader
    if (inThread) {
        while (WaitForSingleObject(inThread, kUnblockPollMs) == WAIT_TIMEOUT) {
            CancelSynchronousIo(inThread);
        }
        CloseHandle(inThread);
    }
    if (job) {
        CloseHandle(job);
    }
    return (int)exitCode;
}

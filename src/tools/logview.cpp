#include "utils/BaseUtil.h"

constexpr const WCHAR* kPipeName = L"\\\\.\\pipe\\SumatraPDFLogger";
constexpr DWORD kBufSize = 1024 * 16;

void log(const char* s, __unused int cb) {
    OutputDebugStringA(s);
    printf("%s", s);
}

void log(const char* s) {
    int cb = (int)str::Len(s);
    log(s, cb);
}

bool IsValidHandle(HANDLE h) {
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    return h != nullptr;
}

static DWORD WINAPI PipeHandlingThread(void* param) {
    HANDLE hHeap = GetProcessHeap();
    char bufRequest[kBufSize]{0};
    DWORD cbBytesRead{0};
    HANDLE hPipe = (HANDLE)param;
    if (!IsValidHandle(hPipe)) {
        log("PipeHandlingThread: not a valid handle!\n");
        return (DWORD)-1;
    }
    BOOL ok{false};
    DWORD err{0};
    while (err == 0) {
        ok = ReadFile(hPipe, bufRequest, sizeof(bufRequest) - 1, &cbBytesRead, nullptr);
        if (ok && cbBytesRead > 0) {
            bufRequest[cbBytesRead] = 0;
            log(bufRequest, (int)cbBytesRead);
            continue;
        }
        err = GetLastError();
        // TODO: handle ERROR_MORE_DATA ?
        if (err == ERROR_BROKEN_PIPE) {
            // log("broken pipe\n");
        } else {
            // TODO: log the error
            // TODO: could this be non-error?
        }
    }

    // we don't need to flush because we don't write
    // FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    log("client exited\n");
    return 0;
}

int main(__unused int argc, __unused char** argv) {
    HANDLE hPipe{INVALID_HANDLE_VALUE};
    HANDLE hThread{nullptr};
    BOOL ok{FALSE};
    DWORD threadId{0};

    log("Starting logview\n");
    for (;;) {
        DWORD openMode = PIPE_ACCESS_INBOUND;
        DWORD mode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;
        DWORD maxInstances = PIPE_UNLIMITED_INSTANCES;
        hPipe = CreateNamedPipeW(kPipeName, openMode, mode, maxInstances, kBufSize, kBufSize, 0, nullptr);
        if (!IsValidHandle(hPipe)) {
            log("couldn't open pipe\n");
            return 1;
        }
        ok = ConnectNamedPipe(hPipe, nullptr);
        if (!ok) {
            ok = GetLastError() == ERROR_PIPE_CONNECTED;
        }
        if (!ok) {
            log("client couldn't connect to our pipe\n");
            CloseHandle(hPipe);
            continue;
        }
        hThread = CreateThread(nullptr, 0, PipeHandlingThread, (void*)hPipe, 0, &threadId);
    }
    return 0;
}

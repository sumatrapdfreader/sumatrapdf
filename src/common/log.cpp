#include "common.h"
#include "log.h"

static char* gLogFilePath = nullptr;
static bool gAllocatedConsole = false;
static bool gConsoleInitialized = false;
static bool gLoggedToConsole = false;
static bool gStdoutRedirected = false;
static HANDLE gOriginalStdout = INVALID_HANDLE_VALUE;
static DWORD gStdoutFileType = FILE_TYPE_UNKNOWN;
static HWND gStartupForegroundWindow = nullptr;

Str fileTypeToStr(DWORD fileType) {
    switch (fileType) {
        case FILE_TYPE_DISK:
            return StrL("FILE_TYPE_DISK");
        case FILE_TYPE_CHAR:
            return StrL("FILE_TYPE_CHAR");
        case FILE_TYPE_PIPE:
            return StrL("FILE_TYPE_PIPE");
        case FILE_TYPE_REMOTE:
            return StrL("FILE_TYPE_REMOTE");
        case FILE_TYPE_UNKNOWN:
            return StrL("FILE_TYPE_UNKNOWN");
        default:
            return StrL("unknown fileType");
    }
}

void LogInit(Str logFilePath) {
    if (gLogFilePath) {
        free(gLogFilePath);
    }
    gLogFilePath = (char*)malloc(logFilePath.len + 1);
    memcpy(gLogFilePath, logFilePath.s, logFilePath.len);
    gLogFilePath[logFilePath.len] = 0;

    DeleteFileA(gLogFilePath);
    // Remember the foreground window at startup (likely the console we were launched from)
    gStartupForegroundWindow = GetForegroundWindow();

    if (WasLaunchedByPowershellWithPipeRedirect()) return;

    // Capture original stdout handle before any console manipulation
    // This detects if we were launched with redirection (e.g., app.exe >out.txt)
    gOriginalStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (gOriginalStdout != INVALID_HANDLE_VALUE && gOriginalStdout != nullptr) {
        gStdoutFileType = GetFileType(gOriginalStdout);
        if (gStdoutFileType == FILE_TYPE_DISK || gStdoutFileType == FILE_TYPE_PIPE) {
            gStdoutRedirected = true;
        }
    }
}

// Ensure console exists, allocating one if needed
static void EnsureConsole() {
    if (gConsoleInitialized) return;
    gConsoleInitialized = true;

    // If stdout is redirected (detected in LogInit), don't mess with console
    if (gStdoutRedirected) {
        return;
    }

    // GUI apps don't automatically attach to parent console.
    // Try to attach to parent's console first (e.g., PowerShell, cmd.exe)
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // Successfully attached to parent console
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONIN$", "r", stdin);
        return;
    }

    // No parent console - allocate our own
    if (AllocConsole()) {
        gAllocatedConsole = true;
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONIN$", "r", stdin);
    }
}

void logConsole(const char* fmt, ...) {
    EnsureConsole();

    char buf[4096];
    va_list args;
    va_start(args, fmt);
    int len = wvsprintfA(buf, fmt, args);
    va_end(args);

    if (len <= 0) return;

    DWORD written;
    if (gStdoutRedirected && gOriginalStdout != INVALID_HANDLE_VALUE) {
        // Redirected to file (cmd.exe style) - WriteFile works directly
        WriteFile(gOriginalStdout, buf, len, &written, nullptr);
        BOOL ok = WriteFile(gOriginalStdout, buf, len, &written, nullptr);
        if (!ok) {
            logf("error: %s\n", GetLastErrorAsStr(GetTempArena()).s);
        }
    } else {
        // Writing to console
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            WriteConsoleA(hConsole, buf, len, &written, nullptr);
            gLoggedToConsole = true;
        }
    }
}

void WaitForConsoleClose() {
    SendEnterIfLoggedToConsole();
    if (!gAllocatedConsole) return;

    // Print message and wait for Enter
    const char* msg = "press Enter to exit";
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hConsole, msg, (DWORD)strlen(msg), &written, nullptr);
    }

    // Wait for Enter key
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput != INVALID_HANDLE_VALUE) {
        FlushConsoleInputBuffer(hInput);
        char c;
        DWORD read;
        ReadConsoleA(hInput, &c, 1, &read, nullptr);
    }
}

void SendEnterIfLoggedToConsole() {
    // Only if we logged to console, attached to parent (didn't allocate), and startup window is valid
    if (!gLoggedToConsole) return;
    if (gAllocatedConsole) return;
    if (!gStartupForegroundWindow) return;
    if (!IsWindow(gStartupForegroundWindow)) return;

    // Bring the startup window to foreground so it receives the input
    SetForegroundWindow(gStartupForegroundWindow);
    // simulate Enter key press
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_RETURN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void LogDestroy() {
    if (gLogFilePath) {
        free(gLogFilePath);
        gLogFilePath = nullptr;
    }
}

void logStr(Str s) {
    if (IsEmpty(s)) return;
    if (!gLogFilePath) return;

    // Append to log file
    HANDLE hFile = CreateFileA(gLogFilePath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, s.s, s.len, &written, nullptr);
        CloseHandle(hFile);
    }

    // If running under debugger, output there too
    if (IsDebuggerPresent()) {
        Str s2 = StrDupTemp(s);
        OutputDebugStringA(s2.s);
    }
}

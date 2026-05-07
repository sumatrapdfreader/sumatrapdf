#include "common.h"

SavedDCState SaveDCState(HWND hwnd) {
    SavedDCState state = {};
    state.hwnd = hwnd;
    state.hdc = GetDC(hwnd);
    HFONT hFont = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    if (hFont) {
        state.oldFont = (HFONT)SelectObject(state.hdc, hFont);
    }
    return state;
}

void RestoreDCState(SavedDCState* state) {
    if (state->oldFont) {
        SelectObject(state->hdc, state->oldFont);
    }
    ReleaseDC(state->hwnd, state->hdc);
}

int MeasureStringWidth(HDC hdc, const wchar_t* str) {
    SIZE size;
    int len = 0;
    const wchar_t* p = str;
    while (*p++) len++;
    GetTextExtentPoint32W(hdc, str, len, &size);
    return size.cx;
}

Str GetWindowTextTemp(HWND hwnd) {
    int wideLen = GetWindowTextLengthW(hwnd);
    if (wideLen == 0) {
        return Str();
    }
    wchar_t* wide = (wchar_t*)AllocTemp((wideLen + 1) * sizeof(wchar_t));
    GetWindowTextW(hwnd, wide, wideLen + 1);
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    char* utf8 = (char*)AllocTemp(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Len, nullptr, nullptr);
    return Str(utf8, utf8Len - 1); // Exclude null terminator from length
}

void SetHwndText(HWND hwnd, Str s) {
    WStr wide = ToWStrTemp(s);
    SetWindowTextW(hwnd, wide.s);
}

Str GetLastErrorAsStr(Arena* arena) {
    DWORD err = GetLastError();
    if (!err) {
        return StrDup(arena, StrL("no error"));
    }
    wchar_t* msgBuf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msgBuf, 0, nullptr);
    if (!msgBuf) {
        return StrDup(arena, StrL("FormatMessageW() failed"));
    }
    auto ws = WStr(msgBuf);
    Str temp = ToUtf8(GetTempArena(), WStr(msgBuf));
    temp = StrTrimSuffixWhitespace(temp);
    Str result = StrFmt(arena, "0x%08lX '%s'", err, temp.s);
    LocalFree(msgBuf);
    return result;
}

// Check if we were launched by PowerShell with stdout redirected to a pipe.
// PowerShell's pipe redirection has known issues with GUI apps using WriteFile.
bool WasLaunchedByPowershellWithPipeRedirect() {
    // Check if stdout is a pipe
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdout == INVALID_HANDLE_VALUE || hStdout == nullptr) {
        return false;
    }
    if (GetFileType(hStdout) != FILE_TYPE_PIPE) {
        return false;
    }

    // Get our parent process ID
    DWORD parentPid = 0;
    DWORD myPid = GetCurrentProcessId();

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == myPid) {
                parentPid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    if (parentPid == 0) {
        CloseHandle(hSnapshot);
        return false;
    }

    // Find parent process name
    Str parentName;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == parentPid) {
                parentName = ToUtf8Temp(WStr(pe.szExeFile));
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);

    return StrHasPrefixNoCase(parentName, StrL("pwsh.exe")) || StrHasPrefixNoCase(parentName, StrL("powershell"));
}

Str GetAppLocalDataDirTemp() {
    wchar_t* path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);
    if (FAILED(hr) || !path) {
        return Str();
    }
    Str result = ToUtf8Temp(WStr(path));
    CoTaskMemFree(path);
    return result;
}

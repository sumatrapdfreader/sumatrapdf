#include "common.h"

bool FileSystemEntryExists(Str s) {
    if (IsEmpty(s)) return false;
    WStr wide = ToWStrTemp(s);
    DWORD attrs = GetFileAttributesW(wide.s);
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool DirectoryExists(Str s) {
    if (IsEmpty(s)) return false;
    WStr wide = ToWStrTemp(s);
    DWORD attrs = GetFileAttributesW(wide.s);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

Str FindFirstValidParentDir(Str path) {
    Str current = path;
    while (current.len > 0) {
        if (DirectoryExists(current)) {
            return current;
        }
        Str parent = PathGetDirTemp(current);
        if (parent.len >= current.len) {
            break; // Reached root or can't go higher
        }
        current = parent;
    }
    return current;
}

static Str GetHomeDir() {
    wchar_t buf[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, len));
    }
    // Fallback to HOMEDRIVE + HOMEPATH
    wchar_t drive[MAX_PATH];
    wchar_t path[MAX_PATH];
    DWORD driveLen = GetEnvironmentVariableW(L"HOMEDRIVE", drive, MAX_PATH);
    DWORD pathLen = GetEnvironmentVariableW(L"HOMEPATH", path, MAX_PATH);
    if (driveLen > 0 && pathLen > 0) {
        wchar_t combined[MAX_PATH * 2];
        int pos = 0;
        for (DWORD i = 0; i < driveLen && pos < MAX_PATH * 2 - 1; i++) {
            combined[pos++] = drive[i];
        }
        for (DWORD i = 0; i < pathLen && pos < MAX_PATH * 2 - 1; i++) {
            combined[pos++] = path[i];
        }
        combined[pos] = 0;
        return ToUtf8Temp(WStr(combined, pos));
    }
    return Str();
}

static Str ExpandEnvVar(Str varName) {
    WStr wideVar = ToWStrTemp(varName);
    wchar_t buf[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(wideVar.s, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, len));
    }
    return Str();
}

static Str ToAbsolutePath(Str path) {
    WStr widePath = ToWStrTemp(path);
    wchar_t buf[MAX_PATH];
    DWORD len = GetFullPathNameW(widePath.s, MAX_PATH, buf, nullptr);
    if (len > 0 && len < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, len));
    }
    return path;
}

Str PathGetDirTemp(Str path) {
    if (IsEmpty(path)) return Str();
    path = StrTrimSuffix(path, StrL("\\"));
    int idx = StrLastIndexOfChar(path, '\\');
    if (idx < 0) return Str();
    // Keep trailing backslash for root (e.g., "C:\")
    int len = (idx <= 2) ? idx + 1 : idx;
    return StrDupTemp(Str(path.s, len));
}

Str PathGetNameTemp(Str path) {
    if (IsEmpty(path)) return Str();
    path = StrTrimSuffix(path, StrL("\\"));
    int idx = StrLastIndexOfChar(path, '\\');
    if (idx < 0) return StrDupTemp(path);
    return StrDupTemp(Str(path.s + idx + 1, path.len - idx - 1));
}

bool FileExists(Str s) {
    if (IsEmpty(s)) return false;
    WStr wide = ToWStrTemp(s);
    DWORD attrs = GetFileAttributesW(wide.s);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

Str SmartResolveDirectory(Str dir) {
    if (IsEmpty(dir)) return dir;

    auto* ta = GetTempArena();

    // Replace / with backslash
    char* normalized = (char*)Alloc(ta, dir.len + 1);
    for (int i = 0; i < dir.len; i++) {
        normalized[i] = (dir.s[i] == '/') ? '\\' : dir.s[i];
    }
    normalized[dir.len] = 0;
    Str result = Str(normalized, dir.len);

    // If it's already a valid directory, just convert to absolute
    if (DirectoryExists(result)) {
        return ToAbsolutePath(result);
    }

    // Try expanding ~ to home directory
    if (result.len > 0 && result.s[0] == '~') {
        Str home = GetHomeDir();
        if (!IsEmpty(home)) {
            int newLen = home.len + result.len - 1;
            char* expanded = (char*)Alloc(ta, newLen + 1);
            int pos = 0;
            for (int i = 0; i < home.len; i++) {
                expanded[pos++] = home.s[i];
            }
            for (int i = 1; i < result.len; i++) {
                expanded[pos++] = result.s[i];
            }
            expanded[pos] = 0;
            result = Str(expanded, pos);

            if (DirectoryExists(result)) {
                return ToAbsolutePath(result);
            }
        }
    }

    // Expand environment variables: $FOO and %FOO%
    // Look for $VAR or %VAR% patterns
    char* expanded = (char*)Alloc(ta, MAX_PATH);
    int outPos = 0;
    int i = 0;
    while (i < result.len && outPos < MAX_PATH - 1) {
        if (result.s[i] == '$' && i + 1 < result.len) {
            // $VAR format - read until non-alphanumeric/underscore
            int varStart = i + 1;
            int varEnd = varStart;
            while (varEnd < result.len) {
                char c = result.s[varEnd];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
                    varEnd++;
                } else {
                    break;
                }
            }
            if (varEnd > varStart) {
                Str varName = Str(result.s + varStart, varEnd - varStart);
                Str value = ExpandEnvVar(varName);
                if (!IsEmpty(value)) {
                    for (int j = 0; j < value.len && outPos < MAX_PATH - 1; j++) {
                        expanded[outPos++] = value.s[j];
                    }
                    i = varEnd;
                    continue;
                }
            }
            expanded[outPos++] = result.s[i++];
        } else if (result.s[i] == '%') {
            // %VAR% format
            int varStart = i + 1;
            int varEnd = varStart;
            while (varEnd < result.len && result.s[varEnd] != '%') {
                varEnd++;
            }
            if (varEnd < result.len && varEnd > varStart) {
                Str varName = Str(result.s + varStart, varEnd - varStart);
                Str value = ExpandEnvVar(varName);
                if (!IsEmpty(value)) {
                    for (int j = 0; j < value.len && outPos < MAX_PATH - 1; j++) {
                        expanded[outPos++] = value.s[j];
                    }
                    i = varEnd + 1;
                    continue;
                }
            }
            expanded[outPos++] = result.s[i++];
        } else {
            expanded[outPos++] = result.s[i++];
        }
    }
    expanded[outPos] = 0;
    result = Str(expanded, outPos);

    return ToAbsolutePath(result);
}

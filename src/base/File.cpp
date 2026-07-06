/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include <limits.h>

#include "base/File.h"

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
#define ZERO_PADDING_COUNT 3

TempStr GetHomeDirTemp();
TempStr ExpandEnvVarTemp(Str varName);
TempStr ToAbsolutePathTemp(Str path);

namespace path {

bool IsSep(char c) {
    return c == PATH_SEP_CHAR || (OS_WIN && c == '/');
}

static bool IsSep(WCHAR c) {
    return c == PATH_SEP_WCHAR || (OS_WIN && c == L'/');
}

static void SkipLeadingPathSep(Str& path) {
    if (path && IsSep(path.s[0])) {
        path.s++;
        path.len--;
    }
}

static void SkipLeadingPathSep(WStr& path) {
    if (path && IsSep(path.s[0])) {
        path.s++;
        path.len--;
    }
}

TempStr GetBaseNameTemp(Str path) {
    int end = path.len;
    int start = end;
    while (start > 0 && !IsSep(path.s[start - 1])) {
        start--;
    }
    return Str(path.s + start, end - start);
}

static WStr GetBaseNameTemp(WStr path) {
    int end = path.len;
    int start = end;
    while (start > 0 && !IsSep(path.s[start - 1])) {
        start--;
    }
    return WStr(path.s + start, end - start);
}

static int GetExtPos(Str path) {
    int ext = -1;
    for (int i = 0; i < path.len; i++) {
        char c = path.s[i];
        if (c == '.') {
            ext = i;
        } else if (IsSep(c)) {
            ext = -1;
        }
    }
    return ext;
}

TempStr GetExtTemp(Str path) {
    int ext = GetExtPos(path);
    if (ext < 0) {
        return StrL("");
    }
    return str::DupTemp(Str(path.s + ext, path.len - ext));
}

TempStr GetPathNoExtTemp(Str path) {
    int ext = GetExtPos(path);
    if (ext < 0) {
        return str::DupTemp(path);
    }
    return str::DupTemp(Str(path.s, ext));
}

TempStr JoinTemp(Str path, Str fileName, Str fileName2) {
    SkipLeadingPathSep(fileName);
    Str sepStr = {};
    if (len(path) > 0 && !IsSep(path.s[path.len - 1])) {
        sepStr = StrL(PATH_SEP);
    }
    TempStr res = str::JoinTemp(path, sepStr, fileName);
    if (fileName2) {
        res = JoinTemp(res, fileName2);
    }
    return res;
}

Str Join(Arena* allocator, Str path, Str fileName) {
    SkipLeadingPathSep(fileName);
    Str sepStr = {};
    if (len(path) > 0 && !IsSep(path.s[path.len - 1])) {
        sepStr = StrL(PATH_SEP);
    }
    return str::Join(allocator, path, sepStr, fileName);
}

Str Join(Str path, Str fileName) {
    return Join(nullptr, path, fileName);
}

TempWStr JoinTemp(WStr path, WStr fileName, WStr fileName2) {
    SkipLeadingPathSep(fileName);
    WStr sepStr;
    if (len(path) > 0 && !IsSep(path.s[path.len - 1])) {
        sepStr = PATH_SEP_WSTR;
    }
    TempWStr res = str::JoinTemp(path, sepStr, fileName);
    if (fileName2) {
        res = JoinTemp(res, fileName2);
    }
    return res;
}

WStr Join(WStr path, WStr fileName, WStr fileName2) {
    TempWStr res = JoinTemp(path, fileName, fileName2);
    return wstr::Dup(res);
}

TempWStr GetDirTemp(WStr path) {
    WStr baseName = GetBaseNameTemp(path);
    if (baseName.s == path.s) {
        return str::DupTemp(L".");
    }
    if (baseName.s == path.s + 1) {
        return str::DupTemp(WStr(path.s, 1));
    }
    if (baseName.s == path.s + 3 && path.s[1] == L':') {
        return str::DupTemp(WStr(path.s, 3));
    }
    if (baseName.s == path.s + 2 && path.len >= 2 && IsSep(path.s[0]) && IsSep(path.s[1])) {
        return str::DupTemp(path);
    }
    return str::DupTemp(WStr(path.s, (int)(baseName.s - path.s - 1)));
}

TempStr GetDirTemp(Str path) {
    Str baseName = GetBaseNameTemp(path);
    if (baseName.s == path.s) {
        return str::DupTemp(".");
    }
    if (baseName.s == path.s + 1) {
        return str::DupTemp(Str(path.s, 1));
    }
    if (baseName.s == path.s + 3 && path.s[1] == ':') {
        return str::DupTemp(Str(path.s, 3));
    }
    if (baseName.s == path.s + 2 && path.len >= 2 && IsSep(path.s[0]) && IsSep(path.s[1])) {
        return str::DupTemp(path);
    }
    return str::DupTemp(Str(path.s, (int)(baseName.s - path.s - 1)));
}

static Str AdvanceUntilWildcardMatch(Str fileName, Str filter);

static bool MatchWildcardsRec(Str fileName, Str filter) {
    if (len(filter) == 0) {
        return len(fileName) == 0;
    }
    switch (filter.s[0]) {
        case '\0':
        case ';':
            return len(fileName) == 0;
        case '*': {
            Str filterRest(filter.s + 1, filter.len - 1);
            fileName = AdvanceUntilWildcardMatch(fileName, filterRest);
            return len(fileName) > 0 || len(filterRest) == 0 || filterRest.s[0] == ';';
        }
        case '?':
            return len(fileName) > 0 &&
                   MatchWildcardsRec(Str(fileName.s + 1, fileName.len - 1), Str(filter.s + 1, filter.len - 1));
        default:
            return tolower(fileName.s[0]) == tolower(filter.s[0]) &&
                   MatchWildcardsRec(Str(fileName.s + 1, fileName.len - 1), Str(filter.s + 1, filter.len - 1));
    }
}

static Str AdvanceUntilWildcardMatch(Str fileName, Str filter) {
    while (len(fileName) > 0 && !MatchWildcardsRec(fileName, filter)) {
        fileName.s++;
        fileName.len--;
    }
    return fileName;
}

bool Match(Str path, Str filter) {
    Str baseName = GetBaseNameTemp(path);
    if (!baseName) {
        return false;
    }
    while (str::ContainsChar(filter, ';')) {
        if (MatchWildcardsRec(baseName, filter)) {
            return true;
        }
        int semiIdx = str::IndexOfChar(filter, ';');
        filter = Str(filter.s + semiIdx + 1, filter.len - semiIdx - 1);
    }
    return MatchWildcardsRec(baseName, filter);
}

bool IsWslUnc(Str path) {
    return str::StartsWithI(path, "\\\\wsl.localhost\\") || str::StartsWithI(path, "\\\\wsl$\\");
}

bool IsWslMount(Str path) {
    if (!path || !str::StartsWithI(path, "/mnt/")) {
        return false;
    }
    if (path.len < 6) {
        return false;
    }
    char drive = (char)tolower((unsigned char)path.s[5]);
    return drive >= 'a' && drive <= 'z' && (IsSep(path.s[6]) || path.s[6] == '\0');
}

TempStr WslUncToUnixTemp(Str path) {
    if (!path) {
        return {};
    }

    int off = 0;
    if (str::StartsWithI(path, "\\\\wsl.localhost\\")) {
        off = LenL("\\\\wsl.localhost\\");
    } else if (str::StartsWithI(path, "\\\\wsl$\\")) {
        off = LenL("\\\\wsl$\\");
    } else {
        return {};
    }

    for (; off < path.len && path.s[off] && !IsSep(path.s[off]); off++) {
        ;
    }
    if (off >= path.len || !IsSep(path.s[off]) || off + 1 >= path.len) {
        return {};
    }

    TempStr unixPath = str::JoinTemp(StrL("/"), Str(path.s + off + 1, path.len - off - 1));
    str::TransCharsInPlace(unixPath, StrL("\\"), StrL("/"));
    return unixPath;
}

TempStr WindowsToWslMountTemp(Str path) {
    if (!path || path.len < 3) {
        return {};
    }
    char drive = (char)tolower((unsigned char)path.s[0]);
    if (!(drive >= 'a' && drive <= 'z' && path.s[1] == ':' && IsSep(path.s[2]))) {
        return {};
    }

    TempStr rest = str::DupTemp(Str(path.s + 3, path.len - 3));
    str::TransCharsInPlace(rest, StrL("\\"), StrL("/"));
    return fmt("/mnt/%c/%s", drive, rest);
}

} // namespace path

TempStr MakeUniqueFilePathTemp(Str path) {
    if (!file::Exists(path)) {
        return str::DupTemp(path);
    }
    TempStr noExt = path::GetPathNoExtTemp(path);
    TempStr ext = path::GetExtTemp(path);
    for (int i = 1; i < 10000; i++) {
        TempStr candidate = fmt("%s.%d%s", noExt, i, ext);
        if (!file::Exists(candidate)) {
            return candidate;
        }
    }
    return str::DupTemp(path);
}

namespace file {

thread_local CopyProgressCb gFileCopyProgressCb;

Str ReadFileWithArena(Str filePath, Arena* allocator) {
    char* d = nullptr;
    int res;
    int size = 0;
    FILE* fp = OpenFILE(filePath);
    if (!fp) {
        return {};
    }
    AutoCall closeFile(fclose, fp);
    res = fseek(fp, 0, SEEK_END);
    if (res != 0) {
        return {};
    }
    long fileSize = ftell(fp);
    size_t nRead = 0;
    if (fileSize < 0 || fileSize > INT_MAX - ZERO_PADDING_COUNT) {
        goto Error;
    }
    size = (int)fileSize;
    d = AllocArray<char>(allocator, size + ZERO_PADDING_COUNT);
    if (!d) {
        goto Error;
    }
    res = fseek(fp, 0, SEEK_SET);
    if (res != 0) {
        goto Error;
    }

    nRead = fread((void*)d, 1, size, fp);
    if (nRead != (size_t)size) {
        int err = ferror(fp);
        int isEof = feof(fp);
        logf("ReadFileWithArena: fread() failed, path: '%s', size: %d, nRead: %d, err: %d, isEof: %d\n", filePath,
             (int)size, (int)nRead, err, isEof);
        ReportIf(!(isEof || (err != 0)));
        goto Error;
    }

    return Str(d, size);
Error:
    Free(allocator, (void*)d);
    return {};
}

Str ReadFile(Str path) {
    return ReadFileWithArena(path, nullptr);
}

int ReadN(Str path, u8* buf, size_t toRead) {
    FILE* fp = OpenFILE(path);
    if (!fp) {
        return -1;
    }
    AutoCall closeFile(fclose, fp);
    ZeroMemory(buf, toRead);
    size_t nRead = fread((void*)buf, 1, toRead, fp);
    if (nRead == 0 && ferror(fp)) {
        return -1;
    }
    return (int)nRead;
}

bool StartsWithN(Str path, Str s) {
    u8* buf = AllocArrayTemp<u8>(s.len);
    if (!buf) {
        return false;
    }
    if (ReadN(path, buf, s.len) != s.len) {
        return false;
    }
    return memeq(buf, s.s, s.len);
}

bool StartsWith(Str path, Str s) {
    return file::StartsWithN(path, s);
}

} // namespace file

namespace dir {

bool CreateAll(Str dir) {
    TempStr parent = path::GetDirTemp(dir);
    if (!str::Eq(parent, dir) && !Exists(parent)) {
        CreateAll(parent);
    }
    return Create(dir);
}

bool CreateForFile(Str path) {
    TempStr dir = path::GetDirTemp(path);
    return CreateAll(dir);
}

} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

bool FileSystemEntryExists(Str s) {
    return path::GetType(s) != path::Type::None;
}

Str FindFirstValidParentDir(Str path) {
    Str current = path;
    while (len(current) > 0) {
        if (dir::Exists(current)) {
            return current;
        }
        Str parent = PathGetDirTemp(current);
        if (parent.len >= current.len) {
            break;
        }
        current = parent;
    }
    return current;
}

Str PathGetDirTemp(Str path) {
    if (len(path) == 0) {
        return Str();
    }
    while (path.len > 1 && path::IsSep(path.s[path.len - 1])) {
        path.len--;
    }
    int idx = -1;
    for (int i = 0; i < path.len; i++) {
        if (path::IsSep(path.s[i])) {
            idx = i;
        }
    }
    if (idx < 0) {
        return Str();
    }
    int n = idx;
    if (idx == 0) {
        n = 1;
    } else if (idx == 2 && path.s[1] == ':') {
        n = 3;
    }
    return str::DupTemp(Str(path.s, n));
}

Str PathGetNameTemp(Str path) {
    if (len(path) == 0) {
        return Str();
    }
    while (path.len > 1 && path::IsSep(path.s[path.len - 1])) {
        path.len--;
    }
    int idx = -1;
    for (int i = 0; i < path.len; i++) {
        if (path::IsSep(path.s[i])) {
            idx = i;
        }
    }
    if (idx < 0) {
        return str::DupTemp(path);
    }
    return str::DupTemp(Str(path.s + idx + 1, path.len - idx - 1));
}

Str SmartResolveDirectory(Str dir) {
    if (len(dir) == 0) {
        return dir;
    }

    auto* ta = GetTempArena();
    char* normalized = (char*)Alloc(ta, dir.len + 1);
    for (int i = 0; i < dir.len; i++) {
        normalized[i] = path::IsSep(dir.s[i]) ? PATH_SEP_CHAR : dir.s[i];
    }
    normalized[dir.len] = 0;
    Str result = Str(normalized, dir.len);

    if (dir::Exists(result)) {
        return ToAbsolutePathTemp(result);
    }

    if (len(result) > 0 && result.s[0] == '~') {
        Str home = GetHomeDirTemp();
        if (len(home) > 0) {
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
            if (dir::Exists(result)) {
                return ToAbsolutePathTemp(result);
            }
        }
    }

    char* expanded = (char*)Alloc(ta, MAX_PATH);
    int outPos = 0;
    int i = 0;
    while (i < result.len && outPos < MAX_PATH - 1) {
        if (result.s[i] == '$' && i + 1 < result.len) {
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
                Str value = ExpandEnvVarTemp(varName);
                if (len(value) > 0) {
                    for (int j = 0; j < value.len && outPos < MAX_PATH - 1; j++) {
                        expanded[outPos++] = value.s[j];
                    }
                    i = varEnd;
                    continue;
                }
            }
            expanded[outPos++] = result.s[i++];
        } else if (result.s[i] == '%') {
            int varStart = i + 1;
            int varEnd = varStart;
            while (varEnd < result.len && result.s[varEnd] != '%') {
                varEnd++;
            }
            if (varEnd < result.len && varEnd > varStart) {
                Str varName = Str(result.s + varStart, varEnd - varStart);
                Str value = ExpandEnvVarTemp(varName);
                if (len(value) > 0) {
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

    return ToAbsolutePathTemp(result);
}

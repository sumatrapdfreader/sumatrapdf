/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/WinDynCalls.h"

#include "base/Log.h"

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
#define ZERO_PADDING_COUNT 3

namespace path {

Type GetType(Str pathA) {
    if (!pathA) {
        return Type::None;
    }

    WCHAR* path = CWStrTemp(pathA);
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(path, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        // path doesn't exist
        return Type::None;
    }
    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return Type::Dir;
    }
    // TODO: not sure if that is that simple, but whatevs
    // https://learn.microsoft.com/en-us/windows/win32/fileio/file-attribute-constants
    return Type::File;
}

bool IsSep(char c) {
    return '\\' == c || '/' == c;
}

static void SkipLeadingPathSep(Str& path) {
    if (path && IsSep(path.s[0])) {
        path.s++;
        path.len--;
    }
}

// do not free, returns view inside <path>
TempStr GetBaseNameTemp(Str path) {
    int end = path.len;
    int start = end;
    while (start > 0 && !IsSep(path.s[start - 1])) {
        start--;
    }
    return Str(path.s + start, end - start);
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
    // TODO: not sure if should allow null path
    SkipLeadingPathSep(fileName);
    Str sepStr = {};
    if (len(path) > 0) {
        if (!IsSep(path.s[path.len - 1])) {
            sepStr = StrL("\\");
        }
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
    if (len(path) > 0) {
        if (!IsSep(path.s[path.len - 1])) {
            sepStr = StrL("\\");
        }
    }
    return str::Join(allocator, path, sepStr, fileName);
}

Str Join(Str path, Str fileName) {
    return Join(nullptr, path, fileName);
}

WStr Join(WStr path, WStr fileName, WStr fileName2) {
    TempWStr res = JoinTemp(path, fileName, fileName2);
    return wstr::Dup(res);
}

bool IsDirectory(Str path) {
    WCHAR* pathW = CWStrTemp(path);
    DWORD attrs = GetFileAttributesW(pathW);
    if (INVALID_FILE_ATTRIBUTES == attrs) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool IsSep(WCHAR c) {
    return '\\' == c || '/' == c;
}

// do not free, returns view inside <path>
static WStr GetBaseNameTemp(WStr path) {
    int end = path.len;
    int start = end;
    while (start > 0 && !IsSep(path.s[start - 1])) {
        start--;
    }
    return WStr(path.s + start, end - start);
}

TempWStr GetDirTemp(WStr path) {
    WStr baseName = GetBaseNameTemp(path);
    if (baseName.s == path.s) {
        // relative directory
        return str::DupTemp(L".");
    }
    if (baseName.s == path.s + 1) {
        // relative root
        return str::DupTemp(WStr(path.s, 1));
    }
    if (baseName.s == path.s + 3 && path.s[1] == ':') {
        // local drive root
        return str::DupTemp(WStr(path.s, 3));
    }
    if (baseName.s == path.s + 2 && wstr::StartsWith(path, L"\\\\")) {
        // server root
        return str::DupTemp(path);
    }
    // any subdirectory
    return str::DupTemp(WStr(path.s, (int)(baseName.s - path.s - 1)));
}

TempStr GetDirTemp(Str path) {
    Str baseName = GetBaseNameTemp(path);
    if (baseName.s == path.s) {
        // relative directory
        return str::DupTemp(".");
    }
    if (baseName.s == path.s + 1) {
        // relative root
        return str::DupTemp(Str(path.s, 1));
    }
    if (baseName.s == path.s + 3 && path.s[1] == ':') {
        // local drive root
        return str::DupTemp(Str(path.s, 3));
    }
    if (baseName.s == path.s + 2 && str::StartsWith(path, "\\\\")) {
        // server root
        return str::DupTemp(path);
    }
    // any subdirectory
    return str::DupTemp(Str(path.s, (int)(baseName.s - path.s - 1)));
}

TempWStr JoinTemp(WStr path, WStr fileName, WStr fileName2) {
    // TODO: not sure if should allow null path
    if (fileName && IsSep(fileName.s[0])) {
        fileName = WStr(fileName.s + 1, fileName.len - 1);
    }
    WStr sepStr;
    if (len(path) > 0) {
        if (!IsSep(path.s[path.len - 1])) {
            sepStr = L"\\";
        }
    }
    TempWStr res = str::JoinTemp(path, sepStr, fileName);
    if (fileName2) {
        res = JoinTemp(res, fileName2);
    }
    return res;
}

// Normalize a file path.
//  remove relative path component (..\ and .\),
//  replace slashes by backslashes,
//  convert to long form.
//
// Returns a pointer to a memory allocated block containing the normalized string.
//   The caller is responsible for freeing the block.
//   Returns nullptr if the file does not exist or if a memory allocation fails.
//
// Precondition: the file must exist on the file system.
//
// Note:
//   - the case of the root component is preserved
//   - the case of rest is set to the way it is stored on the file system
//
// suppose file "C:\foo\Bar.Pdf" exists on the file system then
//    "c:\foo\bar.pdf" becomes "c:\foo\Bar.Pdf"
//    "C:\foo\BAR.PDF" becomes "C:\foo\Bar.Pdf"
static TempWStr NormalizeTemp(WStr path) {
    // win32 path APIs read a NUL-terminated string; path (a WStr) might be a
    // non-terminated substring, so use a terminated copy
    WCHAR* pathZ = CWStrTemp(path);
    // convert to absolute path, change slashes into backslashes
    DWORD cch = GetFullPathNameW(pathZ, 0, nullptr, nullptr);
    if (!cch) {
        return str::DupTemp(path);
    }

    // GetFullPathNameW with a 0 buffer returns the size *including* the
    // terminating null; the fill call returns the count *excluding* it, which
    // is the real string length. Using cch as the WStr len leaves it one too
    // long, so str::Eq() against a correctly-sized path fails to match.
    WCHAR* fullPathBuf = AllocArrayTemp<WCHAR>(cch);
    DWORD nChars = GetFullPathNameW(pathZ, cch, fullPathBuf, nullptr);
    TempWStr fullPath = WStr(fullPathBuf, (int)nChars);

    TempWStr normPath = fullPath;
    // convert to long form
    cch = GetLongPathNameW(fullPath.s, nullptr, 0);
    if (cch > 0) {
        // this sometimes fails for valid long paths
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4940
        WCHAR* longBuf = AllocArrayTemp<WCHAR>(cch);
        DWORD nLong = GetLongPathNameW(fullPath.s, longBuf, cch);
        normPath = WStr(longBuf, (int)nLong);
        if (cch <= MAX_PATH) {
            return normPath;
        }
    }

    // handle overlong paths: first, try to shorten the path
    cch = GetShortPathNameW(fullPath.s, nullptr, 0);
    if (cch && cch <= MAX_PATH) {
        WCHAR* shortBuf = AllocArrayTemp<WCHAR>(cch);
        DWORD nShort = GetShortPathNameW(fullPath.s, shortBuf, cch);
        TempWStr shortPath = WStr(shortBuf, (int)nShort);
        WStr shortPathName = GetBaseNameTemp(shortPath);
        WStr normPathName = GetBaseNameTemp(normPath);
        if (normPathName.len + (int)(shortPathName.s - shortPath.s) < MAX_PATH) {
            // keep the long filename if possible
            *shortPathName.s = 0;
            return str::JoinTemp(shortPath, GetBaseNameTemp(normPath));
        }
        return shortPath;
    }
    // only add \\?\ prefix for paths that are actually overlong
    if (wstr::StartsWith(normPath.s, L"\\\\?\\")) {
        return normPath;
    }
    if (len(normPath) >= MAX_PATH) {
        return str::JoinTemp(WStrL(L"\\\\?\\"), normPath);
    }
    return normPath;
}

TempStr NormalizeTemp(Str path) {
    TempWStr s = ToWStrTemp(path);
    TempWStr ws = NormalizeTemp(s);
    return ToUtf8Temp(ws);
}

// Normalizes the file path and the converts it into a short form that
// can be used for interaction with non-UNICODE aware applications
TempStr ShortPathTemp(Str path) {
    TempWStr pathW = ToWStrTemp(path);
    TempWStr normPath = NormalizeTemp(pathW);
    DWORD cch = GetShortPathNameW(normPath.s, nullptr, 0);
    if (!cch) {
        return ToUtf8Temp(normPath);
    }
    TempWStr shortPath = WStr(AllocArrayTemp<WCHAR>(cch + 1), (int)cch + 1);
    GetShortPathNameW(normPath.s, shortPath.s, cch);
    return ToUtf8Temp(shortPath);
}

static bool IsSameFileHandleInformation(BY_HANDLE_FILE_INFORMATION& fi1, BY_HANDLE_FILE_INFORMATION fi2) {
    if (fi1.dwVolumeSerialNumber != fi2.dwVolumeSerialNumber) {
        return false;
    }
    if (fi1.nFileIndexLow != fi2.nFileIndexLow) {
        return false;
    }
    if (fi1.nFileIndexHigh != fi2.nFileIndexHigh) {
        return false;
    }
    if (fi1.nFileSizeLow != fi2.nFileSizeLow) {
        return false;
    }
    if (fi1.nFileSizeHigh != fi2.nFileSizeHigh) {
        return false;
    }
    if (fi1.dwFileAttributes != fi2.dwFileAttributes) {
        return false;
    }
    if (fi1.nNumberOfLinks != fi2.nNumberOfLinks) {
        return false;
    }
    if (!FileTimeEq(fi1.ftLastWriteTime, fi2.ftLastWriteTime)) {
        return false;
    }
    if (!FileTimeEq(fi1.ftCreationTime, fi2.ftCreationTime)) {
        return false;
    }
    return true;
}

// Code adapted from
// http://stackoverflow.com/questions/562701/best-way-to-determine-if-two-path-reference-to-same-file-in-c-c/562830#562830
// Determine if 2 paths point ot the same file...
bool IsSame(Str path1, Str path2) {
    if (str::IsNull(path1) || str::IsNull(path2)) {
        return false;
    }
    if (str::EqI(path1, path2)) {
        return true;
    }

    // we assume that if the last part doesn't match, they can't be the same
    TempStr base1 = path::GetBaseNameTemp(path1);
    TempStr base2 = path::GetBaseNameTemp(path2);
    if (!str::EqI(base1, base2)) {
        return false;
    }

    // pass the Str (length-aware), not .s: path1/path2 may be non-terminated
    // substrings, and ToWStrTemp(const char*) would read past their end
    WCHAR* path1W = CWStrTemp(path1);
    WCHAR* path2W = CWStrTemp(path2);
    bool isSame = false;
    bool needFallback = true;
    // CreateFile might fail for already opened files
    HANDLE h1 = CreateFileW(path1W, 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    HANDLE h2 = CreateFileW(path2W, 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

    if (h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION fi1, fi2;
        if (GetFileInformationByHandle(h1, &fi1) && GetFileInformationByHandle(h2, &fi2)) {
            isSame = IsSameFileHandleInformation(fi1, fi2);
            needFallback = false;
        }
    }

    CloseHandle(h1);
    CloseHandle(h2);

    if (!needFallback) {
        return isSame;
    }

    TempStr npath1 = NormalizeTemp(path1);
    TempStr npath2 = NormalizeTemp(path2);
    // consider the files different, if their paths can't be normalized
    return npath1 && str::EqI(npath1, npath2);
}

bool HasVariableDriveLetter(Str path) {
    char root[] = R"(?:\)";
    root[0] = (char)toupper(path.s[0]);
    if (root[0] < 'A' || 'Z' < root[0]) {
        return false;
    }

    uint driveType = GetDriveTypeA(root);
    switch (driveType) {
        case DRIVE_REMOVABLE:
        case DRIVE_CDROM:
        case DRIVE_NO_ROOT_DIR:
            return true;
    }
    return false;
}

bool IsOnNetworkDrive(Str path) {
    WCHAR* ws = CWStrTemp(path);
    return PathIsNetworkPathW(ws);
}

// OneDrive / iCloud / Dropbox "Files On-Demand" placeholders — cloud-only
// stubs that hydrate on first read. File I/O is slow and/or bursty, so
// callers may prefer to slurp the whole file into RAM once.
bool IsCloudPlaceholder(Str path) {
    WCHAR* ws = CWStrTemp(path);
    DWORD attrs = GetFileAttributesW(ws);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    // Any of these signal that the bytes aren't all local yet:
    //   FILE_ATTRIBUTE_OFFLINE                = 0x00001000
    //   FILE_ATTRIBUTE_RECALL_ON_OPEN         = 0x00040000
    //   FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS  = 0x00400000
    const DWORD cloudBits =
        FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS;
    return (attrs & cloudBits) != 0;
}

bool IsOnFixedDrive(Str path) {
    WCHAR* ws = CWStrTemp(path);
    if (PathIsNetworkPathW(ws)) {
        return false;
    }

    uint type;
    WCHAR root[MAX_PATH];
    if (GetVolumePathNameW(ws, root, dimof(root))) {
        type = GetDriveType(root);
    } else {
        type = GetDriveType(ws);
    }
    return DRIVE_FIXED == type;
}

// ReadDirectoryChangesW() only works reliably on NTFS and ReFS.
// Network paths don't support it either (SMB doesn't relay notifications).
// For other file systems (FAT32, exFAT etc.) we need manual polling.
bool SupportsChangeNotifications(Str pathA) {
    WCHAR* path = CWStrTemp(pathA);
    if (PathIsNetworkPathW(path)) {
        return false;
    }

    WCHAR root[MAX_PATH];
    if (!GetVolumePathNameW(path, root, dimof(root))) {
        return false;
    }

    WCHAR fsName[MAX_PATH];
    if (!GetVolumeInformationW(root, nullptr, 0, nullptr, nullptr, nullptr, fsName, dimof(fsName))) {
        return false;
    }
    if (wstr::EqI(fsName, L"NTFS")) {
        return true;
    }
    if (wstr::EqI(fsName, L"ReFS")) {
        return true;
    }
    return false;
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

/* matches the filename of a path against a list of semicolon
   separated filters as used by the common file dialogs
   (e.g. "*.pdf;*.xps;?.*" will match all PDF and XPS files and
   all filenames consisting of only a single character and
   having any extension) */
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

bool IsAbsolute(Str path) {
    WCHAR* ws = CWStrTemp(path);
    return !PathIsRelativeW(ws);
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
    return drive >= 'a' && drive <= 'z' && (path.s[6] == '/' || path.s[6] == '\0');
}

// Converts a WSL UNC path to its equivalent Unix path by stripping the
// \\wsl.localhost\<distro>\ or \\wsl$\<distro>\ prefix.
// e.g. "\\wsl.localhost\Ubuntu\home\user\file.tex" -> "/home/user/file.tex"
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

    // Skip the distribution name, e.g. "Ubuntu" in "\\wsl.localhost\Ubuntu\home\..."
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

// Converts a Windows absolute path to its WSL mount-path equivalent.
// e.g. "C:\project\file.tex" -> "/mnt/c/project/file.tex"
TempStr WindowsToWslMountTemp(Str path) {
    if (!path) {
        return {};
    }

    if (path.len < 3) {
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

// When running in App Store, Windows virtualizes %APPDATA% etc., so to get a real path
// for settings etc., we need to un-virtualize
TempStr GetNonVirtualTemp(Str virtualPath) {
    if (!DynGetFinalPathNameByHandleW) {
        return virtualPath;
    }
    WCHAR* pathW = CWStrTemp(virtualPath);
    HANDLE hFile = CreateFileW(pathW, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return virtualPath;
    }

    WCHAR realPath[MAX_PATH * 4];
    DWORD ret = DynGetFinalPathNameByHandleW(hFile, realPath, sizeof(realPath) / sizeof(WCHAR), FILE_NAME_NORMALIZED);

    CloseHandle(hFile);
    if (ret <= 0) {
        return virtualPath;
    }

    TempStr res = ToUtf8Temp(realPath);
    // Remove the "\\?\" prefix if present
    if (str::StartsWith(res, "\\\\?\\")) {
        res = Str(res.s + 4, res.len - 4);
    }
    return res;
}

} // namespace path

// returns the path to either the %TEMP% directory or a
// non-existing file inside whose name starts with filePrefix
TempStr GetTempFilePathTemp(Str filePrefix) {
    WCHAR tempDir[MAX_PATH]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    if (!res || res >= dimof(tempDir)) {
        return {};
    }
    if (!filePrefix) {
        return ToUtf8Temp(tempDir);
    }
    WCHAR path[MAX_PATH]{};
    WCHAR* filePrefixW = CWStrTemp(filePrefix);
    if (!GetTempFileNameW(tempDir, filePrefixW, 0, path)) {
        return {};
    }
    return ToUtf8Temp(path);
}

// returns a path to the application module's directory
// with either the given fileName or the module's name
// (module is the EXE or DLL in which path::GetPathOfFileInAppDir resides)
TempStr GetPathInExeDirTemp(Str fileName) {
    TempStr dir = GetSelfExeDirTemp();
    TempStr path = path::JoinTemp(dir, fileName);
    path = path::NormalizeTemp(path);
    return path;
}

// If path doesn't exist, returns it as-is.
// Otherwise generates unique path by inserting ".1", ".2" etc. before extension.
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

FILE* OpenFILE(Str path) {
    ReportIf(!path);
    if (!path) {
        return nullptr;
    }
    WCHAR* pathW = CWStrTemp(path);
    return _wfopen(pathW, L"rb");
}

Str ReadFileWithArena(Str filePath, Arena* allocator) {
#if 0 // OS_WIN
    WCHAR buf[512];
    strconv::Utf8ToWcharBuf(filePath, len(filePath), buf, dimof(buf));
    return ReadFileWithArena(buf, fileSizeOut, allocator);
#else
    char* d = nullptr;
    int res;
    FILE* fp = OpenFILE(filePath);
    if (!fp) {
        return {};
    }
    defer {
        fclose(fp);
    };
    res = fseek(fp, 0, SEEK_END);
    if (res != 0) {
        return {};
    }
    size_t size = ftell(fp);
    size_t nRead = 0;
    if (addOverflows<size_t>(size, ZERO_PADDING_COUNT)) {
        goto Error;
    }
    d = AllocArray<char>(allocator, size + ZERO_PADDING_COUNT);
    if (!d) {
        goto Error;
    }
    res = fseek(fp, 0, SEEK_SET);
    if (res != 0) {
        return {};
    }

    nRead = fread((void*)d, 1, size, fp);
    if (nRead != size) {
        int err = ferror(fp);
        int isEof = feof(fp);
        logf("ReadFileWithArena: fread() failed, path: '%s', size: %d, nRead: %d, err: %d, isEof: %d\n", filePath,
             (int)size, (int)nRead, err, isEof);
        // we should either get eof or err
        // either way shouldn't happen because we're reading the exact size of file
        // I've seen this in crash reports so maybe the files are over-written
        // between the time I do fseek() and fread()
        ReportIf(!(isEof || (err != 0)));
        goto Error;
    }

    return Str(d, (int)size);
Error:
    Free(allocator, (void*)d);
    return {};
#endif
}

Str ReadFile(Str path) {
    return ReadFileWithArena(path, nullptr);
}

bool WriteFile(Str path, Str d) {
    WCHAR* pathW = CWStrTemp(path);
    const void* data = (const void*)d.s;
    size_t dataLen = (size_t)d.len;
    DWORD access = GENERIC_WRITE;
    DWORD share = FILE_SHARE_READ;
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    auto fh = CreateFileW(pathW, access, share, nullptr, CREATE_ALWAYS, flags, nullptr);
    if (INVALID_HANDLE_VALUE == fh) {
        return false;
    }
    AutoCloseHandle h(fh);

    DWORD size = 0;
    BOOL ok = WriteFile(h, data, (DWORD)dataLen, &size, nullptr);
    ReportIf(ok && (dataLen != (size_t)size));
    return ok && dataLen == (size_t)size;
}

HANDLE OpenReadOnly(Str path) {
    WCHAR* filePath = CWStrTemp(path);
    return CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool Exists(Str path) {
    if (!path) {
        return false;
    }

    WCHAR* pathW = CWStrTemp(path);
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(pathW, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }
    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }
    return true;
}

i64 GetSize(HANDLE h) {
    if (h == nullptr || h == INVALID_HANDLE_VALUE) {
        return -1;
    }
    LARGE_INTEGER size{};
    BOOL ok = GetFileSizeEx(h, &size);
    if (!ok) {
        return -1;
    }
    return size.QuadPart;
}

// Query filesystem metadata without opening the file content. Opening a file
// (even read-only) forces a slow, possibly multi-minute hydration of a cloud
// placeholder (OneDrive "Files On-Demand", issue #5756) and can trigger a
// Windows Defender / network re-scan. GetFileAttributesEx avoids all of that.
static bool GetInfo(Str path, WIN32_FILE_ATTRIBUTE_DATA& fileInfo) {
    if (!path) {
        return false;
    }
    WCHAR* pathW = CWStrTemp(path);
    BOOL ok = GetFileAttributesEx(pathW, GetFileExInfoStandard, &fileInfo);
    return ok != 0;
}

// returns -1 on error (can't use INVALID_FILE_SIZE because it won't cast right)
i64 GetSize(Str path) {
    ReportIf(!path);
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetInfo(path, fileInfo)) {
        return -1;
    }
    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return -1;
    }
    LARGE_INTEGER size;
    size.HighPart = (LONG)fileInfo.nFileSizeHigh;
    size.LowPart = fileInfo.nFileSizeLow;
    return size.QuadPart;
}

// buf must be at least toRead in size (note: it won't be zero-terminated)
// returns -1 for error
int ReadN(Str path, u8* buf, size_t toRead) {
    AutoCloseHandle h = OpenReadOnly(path);
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }

    ZeroMemory(buf, toRead);
    DWORD nRead = 0;
    BOOL ok = ReadFile(h, (void*)buf, (DWORD)toRead, &nRead, nullptr);
    if (!ok) {
        return -1;
    }
    return (int)nRead;
}

// Return true if the file wasn't there or was successfully deleted
bool Delete(Str filePath) {
    if (!filePath) {
        return false;
    }
    WCHAR* filePathW = CWStrTemp(filePath);
    BOOL ok = DeleteFileW(filePathW);
    ok |= (GetLastError() == ERROR_FILE_NOT_FOUND);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

bool DeleteFileToTrash(Str path) {
    TempWStr pathW = ToWStrTemp(path);
    int n = len(pathW) + 2;
    TempWStr pathDoubleTerminated = WStr(AllocArrayTemp<WCHAR>(n), n);
    wstr::BufSet(pathDoubleTerminated, pathW);
    FILEOP_FLAGS flags = FOF_NO_UI | FOF_ALLOWUNDO;
    uint op = FO_DELETE;
    SHFILEOPSTRUCTW shfo = {nullptr, op, pathDoubleTerminated.s, nullptr, flags, FALSE, nullptr, nullptr};
    int res = SHFileOperationW(&shfo);
    return res == 0;
}

bool Copy(Str dst, Str src, bool dontOverwrite) {
    WCHAR* dstW = CWStrTemp(dst);
    WCHAR* srcW = CWStrTemp(src);
    BOOL ok = CopyFileW(srcW, dstW, (BOOL)dontOverwrite);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

thread_local CopyProgressCb gFileCopyProgressCb;

static DWORD CALLBACK CopyProgressRoutine(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
                                          LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE, LPVOID lpData) {
    auto* cb = (const CopyProgressCb*)lpData;
    CopyProgress p;
    p.bytesCopied = TotalBytesTransferred.QuadPart;
    p.bytesTotal = TotalFileSize.QuadPart;
    cb->Call(&p);
    return PROGRESS_CONTINUE;
}

bool Copy(Str dst, Str src, bool dontOverwrite, const CopyProgressCb& cbProgress) {
    if (cbProgress.IsEmpty()) {
        return Copy(dst, src, dontOverwrite);
    }
    WCHAR* dstW = CWStrTemp(dst);
    WCHAR* srcW = CWStrTemp(src);
    BOOL cancel = FALSE;
    DWORD flags = dontOverwrite ? COPY_FILE_FAIL_IF_EXISTS : 0;
    BOOL ok = CopyFileExW(srcW, dstW, CopyProgressRoutine, (LPVOID)&cbProgress, &cancel, flags);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

FILETIME GetAccessTime(Str path) {
    FILETIME t{};
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetInfo(path, fileInfo)) {
        t = fileInfo.ftLastAccessTime;
    }
    return t;
}

bool SetAccessTime(Str path, FILETIME accessTime) {
    WCHAR* pathW = CWStrTemp(path);
    DWORD access = FILE_WRITE_ATTRIBUTES;
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
    AutoCloseHandle h(CreateFileW(pathW, access, share, nullptr, OPEN_EXISTING, 0, nullptr));
    if (INVALID_HANDLE_VALUE == h) {
        return false;
    }
    return SetFileTime(h, nullptr, &accessTime, nullptr);
}

FILETIME GetModificationTime(Str filePath) {
    FILETIME lastMod{};
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetInfo(filePath, fileInfo)) {
        lastMod = fileInfo.ftLastWriteTime;
    }
    return lastMod;
}

DWORD GetAttributes(Str path) {
    WCHAR* pathW = CWStrTemp(path);
    return GetFileAttributesW(pathW);
}

bool SetAttributes(Str path, DWORD attrs) {
    WCHAR* pathW = CWStrTemp(path);
    return SetFileAttributesW(pathW, attrs);
}

bool SetModificationTime(Str path, FILETIME lastMod) {
    WCHAR* pathW = CWStrTemp(path);
    DWORD access = GENERIC_READ | GENERIC_WRITE;
    DWORD disp = OPEN_EXISTING;
    AutoCloseHandle h(CreateFileW(pathW, access, 0, nullptr, disp, 0, nullptr));
    if (INVALID_HANDLE_VALUE == h) {
        return false;
    }
    return SetFileTime(h, nullptr, nullptr, &lastMod);
}

// return true if a file starts with string s of size len
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

// return true if a file starts with null-terminated string s
bool StartsWith(Str path, Str s) {
    return file::StartsWithN(path, s);
}

int GetZoneIdentifier(Str filePath) {
    TempStr path = str::JoinTemp(filePath, StrL(":Zone.Identifier"));
    WCHAR* pathW = CWStrTemp(path);
    return GetPrivateProfileIntW(L"ZoneTransfer", L"ZoneId", URLZONE_INVALID, pathW);
}

bool SetZoneIdentifier(Str filePath, int zoneId) {
    TempStr path = str::JoinTemp(filePath, StrL(":Zone.Identifier"));
    TempStr id = fmt("%d", zoneId);
    WCHAR* idw = CWStrTemp(id);
    WCHAR* pathW = CWStrTemp(path);
    return WritePrivateProfileStringW(L"ZoneTransfer", L"ZoneId", idw, pathW);
}

bool DeleteZoneIdentifier(Str filePath) {
    TempStr path = str::JoinTemp(filePath, StrL(":Zone.Identifier"));
    return Delete(path);
}

bool Rename(Str newPath, Str oldPath) {
    if (!newPath || !oldPath) {
        return false;
    }
    WCHAR* newPathW = CWStrTemp(newPath);
    WCHAR* oldPathW = CWStrTemp(oldPath);
    BOOL ok = MoveFileW(oldPathW, newPathW);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

} // namespace file

namespace dir {

// TODO: duplicate with path::IsDirectory()
bool Exists(WStr dir) {
    if (!dir) {
        return false;
    }

    // GetFileAttributesEx reads a NUL-terminated string; dir might be a
    // non-terminated substring, so use a terminated copy
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(CWStrTemp(dir), GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }

    return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
bool Exists(Str dir) {
    if (!dir) {
        return false;
    }
    WCHAR* dirW = CWStrTemp(dir);

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(dirW, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }

    return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Return true if a directory already exists or has been successfully created
bool Create(Str dir) {
    WCHAR* dirW = CWStrTemp(dir);
    BOOL ok = CreateDirectoryW(dirW, nullptr);
    if (ok) {
        return true;
    }
    return ERROR_ALREADY_EXISTS == GetLastError();
}

// creates a directory and all its parent directories that don't exist yet
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

// remove directory and all its children
bool RemoveAll(Str dir) {
    TempWStr dirW = ToWStrTemp(dir);
    // path must be doubly terminated
    // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shfileopstructa#fo_rename
    int n = len(dirW) + 2;
    TempWStr dirDoubleTerminated = WStr(AllocArrayTemp<WCHAR>(n), n);
    wstr::BufSet(dirDoubleTerminated, dirW);
    FILEOP_FLAGS flags = FOF_NO_UI;
    uint op = FO_DELETE;
    SHFILEOPSTRUCTW shfo = {nullptr, op, dirDoubleTerminated.s, nullptr, flags, FALSE, nullptr, nullptr};
    int res = SHFileOperationW(&shfo);
    return res == 0;
}

// Check if the process can create files/directories in the given directory
// by attempting to create and immediately remove a temporary file.
bool HasWriteAccess(Str dir) {
    if (!dir) {
        return false;
    }
    TempStr path = path::JoinTemp(dir, StrL("__sumatra_write_test__.tmp"));
    WCHAR* pathW = CWStrTemp(path);
    HANDLE h = CreateFileW(pathW, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(h);
    return true;
}

} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

// --- begin: merged from former src/common/file_util.cpp ---

bool FileSystemEntryExists(Str s) {
    if (IsEmpty(s)) return false;
    WCHAR* wide = CWStrTemp(s);
    DWORD attrs = GetFileAttributesW(wide);
    return attrs != INVALID_FILE_ATTRIBUTES;
}

Str FindFirstValidParentDir(Str path) {
    Str current = path;
    while (!IsEmpty(current)) {
        if (dir::Exists(current)) {
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
    DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, n));
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
    WCHAR* wideVar = CWStrTemp(varName);
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(wideVar, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, n));
    }
    return Str();
}

static Str ToAbsolutePath(Str path) {
    WCHAR* widePath = CWStrTemp(path);
    wchar_t buf[MAX_PATH];
    DWORD n = GetFullPathNameW(widePath, MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, n));
    }
    return path;
}

Str PathGetDirTemp(Str path) {
    if (IsEmpty(path)) return Str();
    path = str::TrimSuffix(path, StrL("\\"));
    int idx = str::LastIndexOfChar(path, '\\');
    if (idx < 0) return Str();
    // Keep trailing backslash for root (e.g., "C:\")
    int n = (idx <= 2) ? idx + 1 : idx;
    return str::DupTemp(Str(path.s, n));
}

Str PathGetNameTemp(Str path) {
    if (IsEmpty(path)) return Str();
    path = str::TrimSuffix(path, StrL("\\"));
    int idx = str::LastIndexOfChar(path, '\\');
    if (idx < 0) return str::DupTemp(path);
    return str::DupTemp(Str(path.s + idx + 1, path.len - idx - 1));
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
    if (dir::Exists(result)) {
        return ToAbsolutePath(result);
    }

    // Try expanding ~ to home directory
    if (!IsEmpty(result) && result.s[0] == '~') {
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

            if (dir::Exists(result)) {
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
// --- end: merged from former src/common/file_util.cpp ---

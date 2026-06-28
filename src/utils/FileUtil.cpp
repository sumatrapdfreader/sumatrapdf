/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/WinDynCalls.h"

#include "utils/Log.h"

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
#define ZERO_PADDING_COUNT 3

namespace path {

Type GetType(Str pathA) {
    if (!pathA) {
        return Type::None;
    }

    TempWStr path = ToWStrTemp(pathA);
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(path.s, GetFileExInfoStandard, &fileInfo);
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
    return str::DupTemp(path.s + ext, path.len - ext);
}

TempStr GetPathNoExtTemp(Str path) {
    int ext = GetExtPos(path);
    if (ext < 0) {
        return str::DupTemp(path);
    }
    return str::DupTemp(path.s, ext);
}

TempStr JoinTemp(Str path, Str fileName, Str fileName2) {
    // TODO: not sure if should allow null path
    SkipLeadingPathSep(fileName);
    Str sepStr = {};
    if (!str::IsEmpty(path)) {
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
    if (!str::IsEmpty(path)) {
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
    return WStr(str::Dup(res));
}

bool IsDirectory(Str path) {
    auto pathW = ToWStrTemp(path);
    DWORD attrs = GetFileAttributesW(pathW.s);
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
        return str::DupTemp(path.s, 1);
    }
    if (baseName.s == path.s + 3 && path.s[1] == ':') {
        // local drive root
        return str::DupTemp(path.s, 3);
    }
    if (baseName.s == path.s + 2 && str::StartsWith(path, L"\\\\")) {
        // server root
        return str::DupTemp(path);
    }
    // any subdirectory
    return str::DupTemp(path.s, baseName.s - path.s - 1);
}

TempStr GetDirTemp(Str path) {
    Str baseName = GetBaseNameTemp(path);
    if (baseName.s == path.s) {
        // relative directory
        return str::DupTemp(".");
    }
    if (baseName.s == path.s + 1) {
        // relative root
        return str::DupTemp(path.s, 1);
    }
    if (baseName.s == path.s + 3 && path.s[1] == ':') {
        // local drive root
        return str::DupTemp(path.s, 3);
    }
    if (baseName.s == path.s + 2 && str::StartsWith(path, "\\\\")) {
        // server root
        return str::DupTemp(path);
    }
    // any subdirectory
    return str::DupTemp(path.s, baseName.s - path.s - 1);
}

TempWStr JoinTemp(WStr path, WStr fileName, WStr fileName2) {
    // TODO: not sure if should allow null path
    if (fileName && IsSep(fileName.s[0])) {
        fileName = WStr(fileName.s + 1, fileName.len - 1);
    }
    WStr sepStr;
    if (!str::IsEmpty(path)) {
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
// e.g. suppose the a file "C:\foo\Bar.Pdf" exists on the file system then
//    "c:\foo\bar.pdf" becomes "c:\foo\Bar.Pdf"
//    "C:\foo\BAR.PDF" becomes "C:\foo\Bar.Pdf"
static TempWStr NormalizeTemp(WStr path) {
    // convert to absolute path, change slashes into backslashes
    DWORD cch = GetFullPathNameW(path.s, 0, nullptr, nullptr);
    if (!cch) {
        return str::DupTemp(path);
    }

    TempWStr fullPath = WStr(AllocArrayTemp<WCHAR>(cch), (int)cch);
    GetFullPathNameW(path.s, cch, fullPath.s, nullptr);

    TempWStr normPath = fullPath;
    // convert to long form
    cch = GetLongPathNameW(fullPath.s, nullptr, 0);
    if (cch > 0) {
        // this sometimes fails for valid long paths
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4940
        normPath = WStr(AllocArrayTemp<WCHAR>(cch), (int)cch);
        GetLongPathNameW(fullPath.s, normPath.s, cch);
        if (cch <= MAX_PATH) {
            return normPath;
        }
    }

    // handle overlong paths: first, try to shorten the path
    cch = GetShortPathNameW(fullPath.s, nullptr, 0);
    if (cch && cch <= MAX_PATH) {
        TempWStr shortPath = WStr(AllocArrayTemp<WCHAR>(cch), (int)cch);
        GetShortPathNameW(fullPath.s, shortPath.s, cch);
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
    if (str::StartsWith(normPath.s, L"\\\\?\\")) {
        return normPath;
    }
    if (str::Len(normPath) >= MAX_PATH) {
        return str::JoinTemp(L"\\\\?\\", normPath);
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

    TempWStr path1W = ToWStrTemp(path1.s);
    TempWStr path2W = ToWStrTemp(path2.s);
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
    TempWStr ws = ToWStrTemp(path);
    return PathIsNetworkPathW(ws.s);
}

bool IsCloudPlaceholder(Str path) {
    TempWStr ws = ToWStrTemp(path);
    DWORD attrs = GetFileAttributesW(ws.s);
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
    TempWStr ws = ToWStrTemp(path);
    if (PathIsNetworkPathW(ws.s)) {
        return false;
    }

    uint type;
    WCHAR root[MAX_PATH];
    if (GetVolumePathNameW(ws.s, root, dimof(root))) {
        type = GetDriveType(root);
    } else {
        type = GetDriveType(ws.s);
    }
    return DRIVE_FIXED == type;
}

// ReadDirectoryChangesW() only works reliably on NTFS and ReFS.
// Network paths don't support it either (SMB doesn't relay notifications).
// For other file systems (FAT32, exFAT etc.) we need manual polling.
bool SupportsChangeNotifications(Str pathA) {
    TempWStr path = ToWStrTemp(pathA);
    if (PathIsNetworkPathW(path.s)) {
        return false;
    }

    WCHAR root[MAX_PATH];
    if (!GetVolumePathNameW(path.s, root, dimof(root))) {
        return false;
    }

    WCHAR fsName[MAX_PATH];
    if (!GetVolumeInformationW(root, nullptr, 0, nullptr, nullptr, nullptr, fsName, dimof(fsName))) {
        return false;
    }
    if (str::EqI(fsName, L"NTFS")) {
        return true;
    }
    if (str::EqI(fsName, L"ReFS")) {
        return true;
    }
    return false;
}

static Str AdvanceUntilWildcardMatch(Str fileName, Str filter);

static bool MatchWildcardsRec(Str fileName, Str filter) {
    if (str::IsEmpty(filter)) {
        return str::IsEmpty(fileName);
    }
    switch (filter.s[0]) {
        case '\0':
        case ';':
            return str::IsEmpty(fileName);
        case '*': {
            Str filterRest(filter.s + 1, filter.len - 1);
            fileName = AdvanceUntilWildcardMatch(fileName, filterRest);
            return !str::IsEmpty(fileName) || str::IsEmpty(filterRest) || filterRest.s[0] == ';';
        }
        case '?':
            return !str::IsEmpty(fileName) &&
                   MatchWildcardsRec(Str(fileName.s + 1, fileName.len - 1), Str(filter.s + 1, filter.len - 1));
        default:
            return tolower(fileName.s[0]) == tolower(filter.s[0]) &&
                   MatchWildcardsRec(Str(fileName.s + 1, fileName.len - 1), Str(filter.s + 1, filter.len - 1));
    }
}

static Str AdvanceUntilWildcardMatch(Str fileName, Str filter) {
    while (!str::IsEmpty(fileName) && !MatchWildcardsRec(fileName, filter)) {
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
    while (str::FindChar(filter, ';')) {
        if (MatchWildcardsRec(baseName, filter)) {
            return true;
        }
        filter = Str(str::FindChar(filter, ';').s + 1);
    }
    return MatchWildcardsRec(baseName, filter);
}

bool IsAbsolute(Str path) {
    TempWStr ws = ToWStrTemp(path);
    return !PathIsRelativeW(ws.s);
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
        off = str::Leni("\\\\wsl.localhost\\");
    } else if (str::StartsWithI(path, "\\\\wsl$\\")) {
        off = str::Leni("\\\\wsl$\\");
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

    TempStr unixPath = str::JoinTemp("/", Str(path.s + off + 1, path.len - off - 1));
    str::TransCharsInPlace(unixPath, "\\", "/");
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

    TempStr rest = str::DupTemp(path.s + 3, path.len - 3);
    str::TransCharsInPlace(rest, "\\", "/");
    return str::FormatTemp("/mnt/%c/%s", drive, rest.s);
}

// When running in App Store, Windows virtualizes %APPDATA% etc., so to get a real path
// for settings etc., we need to un-virtualize
TempStr GetNonVirtualTemp(Str virtualPath) {
    if (!DynGetFinalPathNameByHandleW) {
        return Str(virtualPath);
    }
    TempWStr pathW = ToWStrTemp(virtualPath);
    HANDLE hFile = CreateFileW(pathW, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return Str(virtualPath);
    }

    WCHAR realPath[MAX_PATH * 4];
    DWORD ret = DynGetFinalPathNameByHandleW(hFile, realPath, sizeof(realPath) / sizeof(WCHAR), FILE_NAME_NORMALIZED);

    CloseHandle(hFile);
    if (ret <= 0) {
        return Str(virtualPath);
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
    TempWStr filePrefixW = ToWStrTemp(filePrefix);
    if (!GetTempFileNameW(tempDir, filePrefixW.s, 0, path)) {
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
        TempStr candidate = str::FormatTemp("%s.%d%s", noExt.s, i, ext.s);
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
    TempWStr pathW = ToWStrTemp(path);
    return _wfopen(pathW.s, L"rb");
}

ByteSlice ReadFileWithArena(Str filePath, Arena* allocator) {
#if 0 // OS_WIN
    WCHAR buf[512];
    strconv::Utf8ToWcharBuf(filePath, str::Len(filePath), buf, dimof(buf));
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
        logf("ReadFileWithArena: fread() failed, path: '%s', size: %d, nRead: %d, err: %d, isEof: %d\n", filePath.s,
             (int)size, (int)nRead, err, isEof);
        // we should either get eof or err
        // either way shouldn't happen because we're reading the exact size of file
        // I've seen this in crash reports so maybe the files are over-written
        // between the time I do fseek() and fread()
        ReportIf(!(isEof || (err != 0)));
        goto Error;
    }

    return {(u8*)d, size};
Error:
    Free(allocator, (void*)d);
    return {};
#endif
}

ByteSlice ReadFile(Str path) {
    return ReadFileWithArena(path, nullptr);
}

bool WriteFile(Str path, const ByteSlice& d) {
    TempWStr pathW = ToWStrTemp(path);
    const void* data = d.data();
    size_t dataLen = d.size();
    DWORD access = GENERIC_WRITE;
    DWORD share = FILE_SHARE_READ;
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    auto fh = CreateFileW(pathW.s, access, share, nullptr, CREATE_ALWAYS, flags, nullptr);
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
    TempWStr filePath = ToWStrTemp(path);
    return CreateFileW(filePath.s, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}

bool Exists(Str path) {
    if (!path) {
        return false;
    }

    TempWStr pathW = ToWStrTemp(path);
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(pathW.s, GetFileExInfoStandard, &fileInfo);
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
    // Don't use GetFileAttributesEx to retrieve the file size, as
    // that function doesn't interact well with symlinks, etc.
    LARGE_INTEGER size{};
    BOOL ok = GetFileSizeEx(h, &size);
    if (!ok) {
        return -1;
    }
    return size.QuadPart;
}

// returns -1 on error (can't use INVALID_FILE_SIZE because it won't cast right)
i64 GetSize(Str path) {
    ReportIf(!path);
    if (!path) {
        return -1;
    }

    AutoCloseHandle h = OpenReadOnly(path);
    return GetSize(h);
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
    TempWStr filePathW = ToWStrTemp(filePath);
    BOOL ok = DeleteFileW(filePathW.s);
    ok |= (GetLastError() == ERROR_FILE_NOT_FOUND);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

bool DeleteFileToTrash(Str path) {
    TempWStr pathW = ToWStrTemp(path);
    auto n = str::Len(pathW) + 2;
    TempWStr pathDoubleTerminated = WStr(AllocArrayTemp<WCHAR>(n), (int)n);
    str::BufSet(pathDoubleTerminated, (int)n, pathW);
    FILEOP_FLAGS flags = FOF_NO_UI | FOF_ALLOWUNDO;
    uint op = FO_DELETE;
    SHFILEOPSTRUCTW shfo = {nullptr, op, pathDoubleTerminated, nullptr, flags, FALSE, nullptr, nullptr};
    int res = SHFileOperationW(&shfo);
    return res == 0;
}

bool Copy(Str dst, Str src, bool dontOverwrite) {
    TempWStr dstW = ToWStrTemp(dst);
    TempWStr srcW = ToWStrTemp(src);
    BOOL ok = CopyFileW(srcW.s, dstW.s, (BOOL)dontOverwrite);
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
    TempWStr dstW = ToWStrTemp(dst);
    TempWStr srcW = ToWStrTemp(src);
    BOOL cancel = FALSE;
    DWORD flags = dontOverwrite ? COPY_FILE_FAIL_IF_EXISTS : 0;
    BOOL ok = CopyFileExW(srcW.s, dstW.s, CopyProgressRoutine, (LPVOID)&cbProgress, &cancel, flags);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

FILETIME GetAccessTime(Str path) {
    FILETIME t{};
    AutoCloseHandle h(OpenReadOnly(path));
    if (h.IsValid()) {
        GetFileTime(h, nullptr, &t, nullptr);
    }
    return t;
}

bool SetAccessTime(Str path, FILETIME accessTime) {
    TempWStr pathW = ToWStrTemp(path);
    DWORD access = FILE_WRITE_ATTRIBUTES;
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
    AutoCloseHandle h(CreateFileW(pathW.s, access, share, nullptr, OPEN_EXISTING, 0, nullptr));
    if (INVALID_HANDLE_VALUE == h) {
        return false;
    }
    return SetFileTime(h, nullptr, &accessTime, nullptr);
}

FILETIME GetModificationTime(Str filePath) {
    FILETIME lastMod{};
    AutoCloseHandle h(OpenReadOnly(filePath));
    if (h.IsValid()) {
        GetFileTime(h, nullptr, nullptr, &lastMod);
    }
    return lastMod;
}

DWORD GetAttributes(Str path) {
    TempWStr pathW = ToWStrTemp(path);
    return GetFileAttributesW(pathW.s);
}

bool SetAttributes(Str path, DWORD attrs) {
    TempWStr pathW = ToWStrTemp(path);
    return SetFileAttributesW(pathW.s, attrs);
}

bool SetModificationTime(Str path, FILETIME lastMod) {
    TempWStr pathW = ToWStrTemp(path);
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
    TempStr path = str::JoinTemp(filePath, ":Zone.Identifier");
    TempWStr pathW = ToWStrTemp(path);
    return GetPrivateProfileIntW(L"ZoneTransfer", L"ZoneId", URLZONE_INVALID, pathW.s);
}

bool SetZoneIdentifier(Str filePath, int zoneId) {
    TempStr path = str::JoinTemp(filePath, ":Zone.Identifier");
    TempStr id = str::FormatTemp("%d", zoneId);
    TempWStr idw = ToWStrTemp(id);
    TempWStr pathW = ToWStrTemp(path);
    return WritePrivateProfileStringW(L"ZoneTransfer", L"ZoneId", idw.s, pathW.s);
}

bool DeleteZoneIdentifier(Str filePath) {
    TempStr path = str::JoinTemp(filePath, ":Zone.Identifier");
    return Delete(path);
}

bool Rename(Str newPath, Str oldPath) {
    if (!newPath || !oldPath) {
        return false;
    }
    TempWStr newPathW = ToWStrTemp(newPath);
    TempWStr oldPathW = ToWStrTemp(oldPath);
    BOOL ok = MoveFileW(oldPathW.s, newPathW.s);
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

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(dir.s, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }

    return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
bool Exists(Str dir) {
    if (!dir) {
        return false;
    }
    TempWStr dirW = ToWStrTemp(dir);

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(dirW.s, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }

    return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Return true if a directory already exists or has been successfully created
bool Create(Str dir) {
    TempWStr dirW = ToWStrTemp(dir);
    BOOL ok = CreateDirectoryW(dirW.s, nullptr);
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
    auto n = str::Len(dirW) + 2;
    TempWStr dirDoubleTerminated = WStr(AllocArrayTemp<WCHAR>(n), (int)n);
    str::BufSet(dirDoubleTerminated, (int)n, dirW);
    FILEOP_FLAGS flags = FOF_NO_UI;
    uint op = FO_DELETE;
    SHFILEOPSTRUCTW shfo = {nullptr, op, dirDoubleTerminated, nullptr, flags, FALSE, nullptr, nullptr};
    int res = SHFileOperationW(&shfo);
    return res == 0;
}

// Check if the process can create files/directories in the given directory
// by attempting to create and immediately remove a temporary file.
bool HasWriteAccess(Str dir) {
    if (!dir) {
        return false;
    }
    TempStr path = path::JoinTemp(dir, "__sumatra_write_test__.tmp");
    TempWStr pathW = ToWStrTemp(path);
    HANDLE h = CreateFileW(pathW.s, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
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

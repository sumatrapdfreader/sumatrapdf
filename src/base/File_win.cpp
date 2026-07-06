/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/WinDynCalls.h"

#include "base/File.h"

namespace path {

Type GetType(Str pathA) {
    if (!pathA) {
        return Type::None;
    }

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(CWStrTemp(pathA), GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return Type::None;
    }
    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return Type::Dir;
    }
    return Type::File;
}

bool IsDirectory(Str path) {
    DWORD attrs = GetFileAttributesW(CWStrTemp(path));
    if (INVALID_FILE_ATTRIBUTES == attrs) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static TempWStr NormalizeTemp(WStr path) {
    WCHAR* pathZ = CWStrTemp(path);
    DWORD cch = GetFullPathNameW(pathZ, 0, nullptr, nullptr);
    if (!cch) {
        return str::DupTemp(path);
    }

    WCHAR* fullPathBuf = AllocArrayTemp<WCHAR>(cch);
    DWORD nChars = GetFullPathNameW(pathZ, cch, fullPathBuf, nullptr);
    TempWStr fullPath = WStr(fullPathBuf, (int)nChars);

    TempWStr normPath = fullPath;
    cch = GetLongPathNameW(fullPath.s, nullptr, 0);
    if (cch > 0) {
        WCHAR* longBuf = AllocArrayTemp<WCHAR>(cch);
        DWORD nLong = GetLongPathNameW(fullPath.s, longBuf, cch);
        normPath = WStr(longBuf, (int)nLong);
        if (cch <= MAX_PATH) {
            return normPath;
        }
    }

    cch = GetShortPathNameW(fullPath.s, nullptr, 0);
    if (cch && cch <= MAX_PATH) {
        WCHAR* shortBuf = AllocArrayTemp<WCHAR>(cch);
        DWORD nShort = GetShortPathNameW(fullPath.s, shortBuf, cch);
        return WStr(shortBuf, (int)nShort);
    }
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

bool IsSame(Str path1, Str path2) {
    if (str::IsNull(path1) || str::IsNull(path2)) {
        return false;
    }
    if (str::EqI(path1, path2)) {
        return true;
    }

    TempStr base1 = path::GetBaseNameTemp(path1);
    TempStr base2 = path::GetBaseNameTemp(path2);
    if (!str::EqI(base1, base2)) {
        return false;
    }

    WCHAR* path1W = CWStrTemp(path1);
    WCHAR* path2W = CWStrTemp(path2);
    bool isSame = false;
    bool needFallback = true;
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
    return PathIsNetworkPathW(CWStrTemp(path));
}

bool IsCloudPlaceholder(Str path) {
    DWORD attrs = GetFileAttributesW(CWStrTemp(path));
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
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
    return wstr::EqI(fsName, L"NTFS") || wstr::EqI(fsName, L"ReFS");
}

bool IsAbsolute(Str path) {
    return !PathIsRelativeW(CWStrTemp(path));
}

TempStr GetNonVirtualTemp(Str virtualPath) {
    if (!DynGetFinalPathNameByHandleW) {
        return virtualPath;
    }
    HANDLE hFile = CreateFileW(CWStrTemp(virtualPath), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return virtualPath;
    }

    WCHAR realPath[MAX_PATH * 4];
    DWORD ret = DynGetFinalPathNameByHandleW(hFile, realPath, dimof(realPath), FILE_NAME_NORMALIZED);

    CloseHandle(hFile);
    if (ret <= 0) {
        return virtualPath;
    }

    TempStr res = ToUtf8Temp(realPath);
    if (str::StartsWith(res, "\\\\?\\")) {
        res = Str(res.s + 4, res.len - 4);
    }
    return res;
}

} // namespace path

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

TempStr GetPathInExeDirTemp(Str fileName) {
    TempStr dir = GetSelfExeDirTemp();
    TempStr path = path::JoinTemp(dir, fileName);
    path = path::NormalizeTemp(path);
    return path;
}

namespace file {

FILE* OpenFILE(Str path) {
    ReportIf(!path);
    if (!path) {
        return nullptr;
    }
    return _wfopen(CWStrTemp(path), L"rb");
}

bool WriteFile(Str path, Str d) {
    const void* data = (const void*)d.s;
    size_t dataLen = (size_t)d.len;
    HANDLE fh = CreateFileW(CWStrTemp(path), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (INVALID_HANDLE_VALUE == fh) {
        return false;
    }
    AutoCloseHandle h(fh);

    DWORD size = 0;
    BOOL ok = ::WriteFile(h, data, (DWORD)dataLen, &size, nullptr);
    ReportIf(ok && (dataLen != (size_t)size));
    return ok && dataLen == (size_t)size;
}

FileHandle OpenReadOnly(Str path) {
    return CreateFileW(CWStrTemp(path), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}

bool Exists(Str path) {
    if (!path) {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(CWStrTemp(path), GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }
    return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

i64 GetSize(FileHandle h) {
    if (h == nullptr || h == kInvalidFileHandle) {
        return -1;
    }
    LARGE_INTEGER size{};
    BOOL ok = GetFileSizeEx(h, &size);
    if (!ok) {
        return -1;
    }
    return size.QuadPart;
}

static bool GetInfo(Str path, WIN32_FILE_ATTRIBUTE_DATA& fileInfo) {
    if (!path) {
        return false;
    }
    BOOL ok = GetFileAttributesEx(CWStrTemp(path), GetFileExInfoStandard, &fileInfo);
    return ok != 0;
}

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

bool Delete(Str filePath) {
    if (!filePath) {
        return false;
    }
    BOOL ok = DeleteFileW(CWStrTemp(filePath));
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
    BOOL ok = CopyFileW(CWStrTemp(src), CWStrTemp(dst), (BOOL)dontOverwrite);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

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
    BOOL cancel = FALSE;
    DWORD flags = dontOverwrite ? COPY_FILE_FAIL_IF_EXISTS : 0;
    BOOL ok = CopyFileExW(CWStrTemp(src), CWStrTemp(dst), CopyProgressRoutine, (LPVOID)&cbProgress, &cancel, flags);
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
    AutoCloseHandle h(CreateFileW(CWStrTemp(path), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_EXISTING, 0, nullptr));
    if (!h.IsValid()) {
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
    return GetFileAttributesW(CWStrTemp(path));
}

bool SetAttributes(Str path, DWORD attrs) {
    return SetFileAttributesW(CWStrTemp(path), attrs);
}

bool SetModificationTime(Str path, FILETIME lastMod) {
    AutoCloseHandle h(
        CreateFileW(CWStrTemp(path), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
    if (!h.IsValid()) {
        return false;
    }
    return SetFileTime(h, nullptr, nullptr, &lastMod);
}

int GetZoneIdentifier(Str filePath) {
    TempStr path = str::JoinTemp(filePath, StrL(":Zone.Identifier"));
    return GetPrivateProfileIntW(L"ZoneTransfer", L"ZoneId", URLZONE_INVALID, CWStrTemp(path));
}

bool SetZoneIdentifier(Str filePath, int zoneId) {
    TempStr path = str::JoinTemp(filePath, StrL(":Zone.Identifier"));
    TempStr id = fmt("%d", zoneId);
    return WritePrivateProfileStringW(L"ZoneTransfer", L"ZoneId", CWStrTemp(id), CWStrTemp(path));
}

bool DeleteZoneIdentifier(Str filePath) {
    TempStr path = str::JoinTemp(filePath, StrL(":Zone.Identifier"));
    return Delete(path);
}

bool Rename(Str newPath, Str oldPath) {
    if (!newPath || !oldPath) {
        return false;
    }
    BOOL ok = MoveFileW(CWStrTemp(oldPath), CWStrTemp(newPath));
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

} // namespace file

static ULARGE_INTEGER FileTimeToLargeInteger(const FILETIME& ft) {
    ULARGE_INTEGER res;
    res.LowPart = ft.dwLowDateTime;
    res.HighPart = ft.dwHighDateTime;
    return res;
}

int FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2) {
    ULARGE_INTEGER t1 = FileTimeToLargeInteger(ft1);
    ULARGE_INTEGER t2 = FileTimeToLargeInteger(ft2);
    LONGLONG diff = t1.QuadPart - t2.QuadPart;
    diff = diff / (LONGLONG)10000000L;
    return (int)diff;
}

namespace dir {

bool Exists(WStr dir) {
    if (!dir) {
        return false;
    }
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
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(CWStrTemp(dir), GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }
    return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool Create(Str dir) {
    BOOL ok = CreateDirectoryW(CWStrTemp(dir), nullptr);
    if (ok) {
        return true;
    }
    return ERROR_ALREADY_EXISTS == GetLastError();
}

bool RemoveAll(Str dir) {
    TempWStr dirW = ToWStrTemp(dir);
    int n = len(dirW) + 2;
    TempWStr dirDoubleTerminated = WStr(AllocArrayTemp<WCHAR>(n), n);
    wstr::BufSet(dirDoubleTerminated, dirW);
    FILEOP_FLAGS flags = FOF_NO_UI;
    uint op = FO_DELETE;
    SHFILEOPSTRUCTW shfo = {nullptr, op, dirDoubleTerminated.s, nullptr, flags, FALSE, nullptr, nullptr};
    int res = SHFileOperationW(&shfo);
    return res == 0;
}

bool HasWriteAccess(Str dir) {
    if (!dir) {
        return false;
    }
    TempStr path = path::JoinTemp(dir, StrL("__sumatra_write_test__.tmp"));
    HANDLE h = CreateFileW(CWStrTemp(path), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(h);
    return true;
}

} // namespace dir

TempStr GetHomeDirTemp() {
    WCHAR buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, n));
    }

    WCHAR drive[MAX_PATH];
    WCHAR path[MAX_PATH];
    DWORD driveLen = GetEnvironmentVariableW(L"HOMEDRIVE", drive, MAX_PATH);
    DWORD pathLen = GetEnvironmentVariableW(L"HOMEPATH", path, MAX_PATH);
    if (driveLen > 0 && pathLen > 0) {
        WCHAR combined[MAX_PATH * 2];
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
    return {};
}

TempStr ExpandEnvVarTemp(Str varName) {
    WCHAR buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(CWStrTemp(varName), buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, n));
    }
    return {};
}

TempStr ToAbsolutePathTemp(Str path) {
    WCHAR buf[MAX_PATH];
    DWORD n = GetFullPathNameW(CWStrTemp(path), MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, n));
    }
    return path;
}

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"

#include "base/File.h"

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
constexpr int kZeroPaddingCount = 3;

// Defined in Win.cpp; avoid pulling all of Win.h into this file.
void LogLastError(DWORD err = 0);
Str GetLastErrorAsStr(Arena* arena);

// Same value as HINSTANCE in WinMain for this image (exe or DLL).
// Using __ImageBase (not GetModuleHandle(nullptr)) so DLL builds report the
// DLL path, not the host process path.
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace path {

Type GetType(Str pathA) {
    if (len(pathA) == 0) {
        return Type::None;
    }

    DWORD attrs = GetCachedAttributes(pathA);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return Type::None;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        return Type::Dir;
    }
    return Type::File;
}

// Network-drive attribute cache: GetFileAttributesExW on UNC/mapped drives is
// slow and menu rebuild / Exists / GetSize can hit the same path repeatedly.
// One entry stores full WIN32_FILE_ATTRIBUTE_DATA for both GetCachedAttributes
// and GetCachedAttributesEx.
struct AttrsCacheEntry {
    char* path = nullptr; // owned heap string
    bool ok = false;      // last GetFileAttributesEx result
    WIN32_FILE_ATTRIBUTE_DATA data{};
    u64 tickMs = 0;
};

static Mutex gAttrsCacheMutex;
static Vec<AttrsCacheEntry> gAttrsCache;
constexpr u64 kAttrsCacheTtlMs = 60ull * 60ull * 1000ull; // 1 hour
constexpr int kAttrsCacheMaxEntries = 512;

static void FreeAttrsCacheEntry(AttrsCacheEntry& e) {
    free(e.path);
    e.path = nullptr;
    e.ok = false;
    e.data = {};
    e.tickMs = 0;
}

// Look up a non-expired cache entry. Caller holds gAttrsCacheMutex.
// Returns true if a live entry was found (ok or failed query).
static bool LookupAttrsCache(Str path, u64 now, bool* okOut, WIN32_FILE_ATTRIBUTE_DATA* dataOut) {
    for (int i = 0; i < len(gAttrsCache); i++) {
        AttrsCacheEntry& e = gAttrsCache[i];
        if (!e.path || !str::EqI(Str(e.path), path)) {
            continue;
        }
        if (now - e.tickMs > kAttrsCacheTtlMs) {
            FreeAttrsCacheEntry(e);
            return false;
        }
        *okOut = e.ok;
        *dataOut = e.data;
        return true;
    }
    return false;
}

// Insert or update cache entry. Caller holds gAttrsCacheMutex.
static void StoreAttrsCache(Str path, u64 now, bool ok, const WIN32_FILE_ATTRIBUTE_DATA& data) {
    int freeIdx = -1;
    int oldestIdx = -1;
    u64 oldestTick = UINT64_MAX;
    for (int i = 0; i < len(gAttrsCache); i++) {
        AttrsCacheEntry& e = gAttrsCache[i];
        if (!e.path) {
            if (freeIdx < 0) {
                freeIdx = i;
            }
            continue;
        }
        if (str::EqI(Str(e.path), path)) {
            e.ok = ok;
            e.data = data;
            e.tickMs = now;
            return;
        }
        if (e.tickMs < oldestTick) {
            oldestTick = e.tickMs;
            oldestIdx = i;
        }
    }
    if (freeIdx < 0 && len(gAttrsCache) < kAttrsCacheMaxEntries) {
        freeIdx = len(gAttrsCache);
        VecAppendBlanks(gAttrsCache, 1);
    }
    if (freeIdx < 0) {
        freeIdx = oldestIdx;
    }
    if (freeIdx < 0) {
        return;
    }
    AttrsCacheEntry& e = gAttrsCache[freeIdx];
    FreeAttrsCacheEntry(e);
    Str owned = str::Dup(path);
    e.path = owned.s;
    e.ok = ok;
    e.data = data;
    e.tickMs = now;
}

// Non-fixed drive availability (network / removable / …). When a mapped or UNC
// drive is offline, GetFileAttributesEx is very slow; after the first failure
// we cache "unavailable" and fail subsequent paths on that drive for a few
// minutes without touching the network again.
struct DriveAvailEntry {
    DriveAvailEntry* next = nullptr;
    char driveName[128]; // "X" or "\\server\share"
    bool isAvailable = false;
    u64 lastCheckMs = 0;
};

static DriveAvailEntry* gDriveAvailHead = nullptr;
constexpr u64 kDriveAvailTtlMs = 4ull * 60ull * 1000ull; // 4 minutes

// Fill keyOut with a non-fixed drive key ("X" or "\\server\share"). Returns
// false for fixed drives, relative paths, and paths we do not guard.
static bool GetNonFixedDriveKey(Str path, char* keyOut, int keyCap) {
    if (len(path) == 0 || !keyOut || keyCap < 2) {
        return false;
    }
    keyOut[0] = 0;
    int n = len(path);
    if (n < 2) {
        return false;
    }
    const char* s = path.s;

    bool sep0 = s[0] == '\\' || s[0] == '/';
    bool sep1 = s[1] == '\\' || s[1] == '/';
    if (sep0 && sep1) {
        // \\?\UNC\server\share\... or \\?\C:\... or \\server\share\...
        int start = 2;
        if (n >= 4 && (s[2] == '?' || s[2] == '.') && (s[3] == '\\' || s[3] == '/')) {
            if (s[2] == '.') {
                return false; // \\.\device
            }
            Str rest((char*)s + 4, n - 4);
            if (str::StartsWithI(rest, StrL("UNC\\")) || str::StartsWithI(rest, StrL("UNC/"))) {
                // skip "UNC\"
                start = 4 + 4;
            } else {
                // \\?\C:\... → treat rest as the real path
                return GetNonFixedDriveKey(rest, keyOut, keyCap);
            }
        }
        // Parse \\server\share from s[start..]
        if (start >= n) {
            return false;
        }
        int i = start;
        while (i < n && s[i] != '\\' && s[i] != '/') {
            i++;
        }
        if (i >= n || i == start) {
            return false; // no server or no share
        }
        int shareStart = i + 1;
        int j = shareStart;
        while (j < n && s[j] != '\\' && s[j] != '/') {
            j++;
        }
        if (j == shareStart) {
            return false;
        }
        // key = "\\server\share" (normalize seps to '\')
        int need = 2 + (i - start) + 1 + (j - shareStart) + 1;
        if (need > keyCap) {
            return false;
        }
        int p = 0;
        keyOut[p++] = '\\';
        keyOut[p++] = '\\';
        for (int k = start; k < i; k++) {
            keyOut[p++] = s[k];
        }
        keyOut[p++] = '\\';
        for (int k = shareStart; k < j; k++) {
            keyOut[p++] = s[k];
        }
        keyOut[p] = 0;
        return true;
    }

    if (s[1] != ':') {
        return false; // relative
    }
    char drive = (char)toupper((u8)s[0]);
    if (drive < 'A' || drive > 'Z') {
        return false;
    }
    WCHAR root[] = LR"(?:\)";
    root[0] = (WCHAR)drive;
    UINT type = GetDriveTypeW(root);
    // Guard everything that is not a local fixed disk. DRIVE_NO_ROOT_DIR is a
    // disconnected mapping — still use the availability cache.
    if (type == DRIVE_FIXED || type == DRIVE_UNKNOWN) {
        return false;
    }
    keyOut[0] = drive;
    keyOut[1] = 0;
    return true;
}

// Caller holds gAttrsCacheMutex. Drops expired nodes while scanning.
static DriveAvailEntry* FindDriveAvailLocked(Str driveName, u64 now) {
    DriveAvailEntry** pp = &gDriveAvailHead;
    while (*pp) {
        DriveAvailEntry* e = *pp;
        if (now - e->lastCheckMs > kDriveAvailTtlMs) {
            *pp = e->next;
            free(e);
            continue;
        }
        if (str::EqI(Str(e->driveName), driveName)) {
            return e;
        }
        pp = &e->next;
    }
    return nullptr;
}

// Caller holds gAttrsCacheMutex.
static void StoreDriveAvailLocked(Str driveName, bool isAvailable, u64 now) {
    DriveAvailEntry* e = FindDriveAvailLocked(driveName, now);
    if (e) {
        e->isAvailable = isAvailable;
        e->lastCheckMs = now;
        return;
    }
    e = AllocStruct<DriveAvailEntry>();
    if (!e) {
        return;
    }
    str::BufSet(Str(e->driveName, dimof(e->driveName)), driveName);
    e->isAvailable = isAvailable;
    e->lastCheckMs = now;
    e->next = gDriveAvailHead;
    gDriveAvailHead = e;
}

// Probe the drive root ("X:\" or "\\server\share\") without holding the mutex.
static bool ProbeDriveRootAccessible(Str driveKey) {
    TempStr root;
    if (len(driveKey) == 1) {
        root = fmt("%c:\\", driveKey.s[0]);
    } else {
        root = str::JoinTemp(driveKey, StrL("\\"));
    }
    WIN32_FILE_ATTRIBUTE_DATA data{};
    return GetFileAttributesExW(CWStrTemp(root), GetFileExInfoStandard, &data) != 0;
}

// Like GetFileAttributesExW(..., GetFileExInfoStandard, ...). Network paths
// share the same 1-hour path cache as GetCachedAttributes; unavailable
// non-fixed drives use a short drive-level availability cache.
bool GetCachedAttributesEx(Str path, WIN32_FILE_ATTRIBUTE_DATA* out) {
    if (out) {
        *out = {};
    }
    if (len(path) == 0 || !out) {
        return false;
    }

    const bool network = IsOnNetworkDrive(path);

    char driveKeyBuf[128];
    const bool hasDriveKey = GetNonFixedDriveKey(path, driveKeyBuf, dimof(driveKeyBuf));
    Str driveKey = hasDriveKey ? Str(driveKeyBuf) : Str();

    // Non-fixed drive availability. Network paths check this first (before the
    // per-path attrs cache and GetFileAttributesEx): an offline share would
    // otherwise hang on every history/menu path. On cache miss for network we
    // probe the drive root once and remember the result for ~4 minutes.
    if (hasDriveKey) {
        const u64 now = GetTickCount64();
        bool needProbe = false;
        {
            ScopedMutex lock(&gAttrsCacheMutex);
            DriveAvailEntry* e = FindDriveAvailLocked(driveKey, now);
            if (e) {
                if (!e->isAvailable) {
                    return false; // known offline — fail fast
                }
                // known available: continue to path cache / attribute query
            } else if (network) {
                needProbe = true;
            }
        }
        if (needProbe) {
            bool avail = ProbeDriveRootAccessible(driveKey);
            ScopedMutex lock(&gAttrsCacheMutex);
            StoreDriveAvailLocked(driveKey, avail, GetTickCount64());
            if (!avail) {
                return false;
            }
        }
    }

    if (network) {
        const u64 now = GetTickCount64();
        bool ok = false;
        WIN32_FILE_ATTRIBUTE_DATA data{};
        {
            ScopedMutex lock(&gAttrsCacheMutex);
            if (LookupAttrsCache(path, now, &ok, &data)) {
                // logf("path::GetCachedAttributesEx: network path='%s' ok=%d attrs=0x%x cache=hit\n", path, (int)ok,
                //      data.dwFileAttributes);
                if (ok) {
                    *out = data;
                }
                return ok;
            }
        }
    }

    WIN32_FILE_ATTRIBUTE_DATA data{};
    BOOL ok = GetFileAttributesExW(CWStrTemp(path), GetFileExInfoStandard, &data);
    if (!ok) {
        data = {};
    }

    if (ok) {
        if (hasDriveKey) {
            ScopedMutex lock(&gAttrsCacheMutex);
            StoreDriveAvailLocked(driveKey, true, GetTickCount64());
        }
        if (network) {
            ScopedMutex lock(&gAttrsCacheMutex);
            StoreAttrsCache(path, GetTickCount64(), true, data);
        }
        *out = data;
        return true;
    }

    // Attribute query failed. For non-network non-fixed drives (e.g. removable
    // with no media), learn availability after the first failure. Network was
    // already probed above on miss.
    if (hasDriveKey && !network) {
        const u64 now = GetTickCount64();
        bool needProbe = false;
        {
            ScopedMutex lock(&gAttrsCacheMutex);
            DriveAvailEntry* e = FindDriveAvailLocked(driveKey, now);
            if (!e) {
                needProbe = true;
            }
        }
        if (needProbe) {
            bool avail = ProbeDriveRootAccessible(driveKey);
            ScopedMutex lock(&gAttrsCacheMutex);
            StoreDriveAvailLocked(driveKey, avail, GetTickCount64());
        }
    }

    if (network) {
        // logf("path::GetCachedAttributesEx: network path='%s' ok=%d attrs=0x%x cache=miss\n", path, (int)(ok != 0),
        //      data.dwFileAttributes);
        ScopedMutex lock(&gAttrsCacheMutex);
        StoreAttrsCache(path, GetTickCount64(), false, data);
    }
    return false;
}

// Like GetFileAttributesW: returns attributes or INVALID_FILE_ATTRIBUTES.
// On Windows, network-drive path results are cached for 1 hour (shared with
// GetCachedAttributesEx). Offline non-fixed drives (mapped/UNC/removable) are
// also remembered for ~4 minutes so later queries fail fast.
// Non-Windows: no cache (same as an uncached attribute query).
DWORD GetCachedAttributes(Str path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetCachedAttributesEx(path, &data)) {
        return INVALID_FILE_ATTRIBUTES;
    }
    return data.dwFileAttributes;
}

bool IsDirectory(Str path) {
    DWORD attrs = GetCachedAttributes(path);
    if (INVALID_FILE_ATTRIBUTES == attrs) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static TempWStr NormalizeTemp(WStr path) {
    WCHAR* pathZ = CWStrTemp(path);
    // GetFullPathNameW is path-string math only (relative→absolute, collapse
    // . / ..). It does not require the path to exist and does not hit the
    // network for UNC/mapped paths the way GetLongPathNameW does.
    DWORD cch = GetFullPathNameW(pathZ, 0, nullptr, nullptr);
    if (!cch) {
        return str::DupTemp(path);
    }

    WCHAR* fullPathBuf = AllocArrayTemp<WCHAR>((int)cch);
    DWORD nChars = GetFullPathNameW(pathZ, cch, fullPathBuf, nullptr);
    TempWStr fullPath = WStr(fullPathBuf, (int)nChars);

    // GetLongPathNameW / GetShortPathNameW query the filesystem (each path
    // component) and are very slow on network drives when called on the UI
    // thread (open, tab switch, menu rebuild). Skip them for network paths —
    // absolute form from GetFullPathNameW is enough.
    if (PathIsNetworkPathW(fullPath.s) || PathIsNetworkPathW(pathZ)) {
        if (wstr::StartsWith(fullPath.s, WStrL(L"\\\\?\\"))) {
            return fullPath;
        }
        if (len(fullPath) >= MAX_PATH) {
            return str::JoinTemp(WStrL(L"\\\\?\\"), fullPath);
        }
        return fullPath;
    }

    TempWStr normPath = fullPath;
    cch = GetLongPathNameW(fullPath.s, nullptr, 0);
    if (cch > 0) {
        WCHAR* longBuf = AllocArrayTemp<WCHAR>((int)cch);
        DWORD nLong = GetLongPathNameW(fullPath.s, longBuf, cch);
        normPath = WStr(longBuf, (int)nLong);
        if (cch <= MAX_PATH) {
            return normPath;
        }
    }

    cch = GetShortPathNameW(fullPath.s, nullptr, 0);
    if (cch && cch <= MAX_PATH) {
        WCHAR* shortBuf = AllocArrayTemp<WCHAR>((int)cch);
        DWORD nShort = GetShortPathNameW(fullPath.s, shortBuf, cch);
        return WStr(shortBuf, (int)nShort);
    }
    if (wstr::StartsWith(normPath.s, WStrL(L"\\\\?\\"))) {
        return normPath;
    }
    if (len(normPath) >= MAX_PATH) {
        return str::JoinTemp(WStrL(L"\\\\?\\"), normPath);
    }
    return normPath;
}

// Absolute path form. Local drives also expand 8.3 names via GetLongPathNameW;
// network paths skip that (too slow on the UI thread) and only use GetFullPathNameW.
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
    TempWStr shortPath = WStr(AllocArrayTemp<WCHAR>((int)cch + 1), (int)cch + 1);
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

// Drive-letter -> is-network cache. Mappings change rarely and
// IsOnNetworkDrive() runs in front of every cached attribute query, so keep a
// short TTL rather than querying the drive for each call.
constexpr u64 kDriveIsNetCacheTtlMs = 60ull * 1000ull;
static Mutex gDriveIsNetMutex;
static u64 gDriveIsNetTick[26];
static bool gDriveIsNet[26];

// drive is an upper-case letter 'A'..'Z'
static bool IsNetworkDriveLetter(char drive) {
    int idx = drive - 'A';
    u64 now = GetTickCount64();
    {
        ScopedMutex lock(&gDriveIsNetMutex);
        u64 tick = gDriveIsNetTick[idx];
        if (tick != 0 && (now - tick) <= kDriveIsNetCacheTtlMs) {
            return gDriveIsNet[idx];
        }
    }
    WCHAR root[] = LR"(?:\)";
    root[0] = (WCHAR)drive;
    // resolved locally from the mount point, unlike WNetGetConnection
    bool isNet = GetDriveTypeW(root) == DRIVE_REMOTE;
    ScopedMutex lock(&gDriveIsNetMutex);
    gDriveIsNetTick[idx] = now;
    gDriveIsNet[idx] = isNet;
    return isNet;
}

// PathIsNetworkPathW() answers this for a drive-letter path by going through
// WNetGetConnection -> NetWkstaGetInfo, i.e. a synchronous RPC to the
// LanmanWorkstation service: ~0.3 ms per call on a mapped drive here, and it
// can block for seconds when the server behind a mapping is unreachable.
// Decide from the path shape (UNC is a pure string question) plus a cached
// GetDriveType (local) instead.
bool IsOnNetworkDrive(Str path) {
    int n = len(path);
    if (n < 2) {
        return false;
    }
    const char* s = path.s;
    bool sep0 = s[0] == '\\' || s[0] == '/';
    bool sep1 = s[1] == '\\' || s[1] == '/';
    if (sep0 && sep1) {
        // \\?\UNC\server\share is UNC; \\?\C:\... is a drive-letter path;
        // \\.\ names a device and is never a network path
        if (n >= 4 && (s[2] == '?' || s[2] == '.') && (s[3] == '\\' || s[3] == '/')) {
            if (s[2] == '.') {
                return false;
            }
            Str rest((char*)s + 4, n - 4);
            if (str::StartsWithI(rest, StrL("UNC\\")) || str::StartsWithI(rest, StrL("UNC/"))) {
                return true;
            }
            return IsOnNetworkDrive(rest);
        }
        return true; // \\server\share
    }
    if (s[1] != ':') {
        return false; // relative path
    }
    char drive = (char)toupper((u8)s[0]);
    if (drive < 'A' || drive > 'Z') {
        return false;
    }
    return IsNetworkDriveLetter(drive);
}

bool IsCloudPlaceholder(Str path) {
    DWORD attrs = GetCachedAttributes(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    const DWORD cloudBits =
        FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS;
    return (attrs & cloudBits) != 0;
}

// True if this directory name is one used by OneNote / Outlook / IE to extract
// an attachment that the host still needs to rewrite or delete.
static bool IsEphemeralHostDirName(Str name) {
    if (str::EqI(name, StrL("OneNote"))) {
        return true;
    }
    if (str::StartsWithI(name, StrL("Microsoft.Office.OneNote"))) {
        return true;
    }
    if (str::EqI(name, StrL("Content.Outlook"))) {
        return true;
    }
    if (str::EqI(name, StrL("INetCache"))) {
        return true;
    }
    if (str::EqI(name, StrL("Temporary Internet Files"))) {
        return true;
    }
    return false;
}

// Files extracted by OneNote, Outlook, and similar hosts into a cache folder.
// Opening those in place keeps a handle (or a directory watch) on the host's
// file, and the host then fails to sync with "denied access to the file"
// (issue #4705). Detection is by path component only; the file need not exist.
bool IsEphemeralHostFile(Str path) {
    if (len(path) == 0) {
        return false;
    }
    int start = 0;
    int nPath = len(path);
    for (int i = 0; i <= nPath; i++) {
        bool sep = (i == nPath) || IsSep(path.s[i]);
        if (!sep) {
            continue;
        }
        int n = i - start;
        if (n > 0 && IsEphemeralHostDirName(Str(path.s + start, n))) {
            return true;
        }
        start = i + 1;
    }
    return false;
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

// True if the volume behind `path` is reachable right now. Used when the user
// explicitly asks to drop deleted history entries: a present USB stick or
// mapped share with a missing file should be cleaned up, but an unplugged
// drive / offline share must not — we can't tell those files from deleted.
bool IsOnAvailableDrive(Str path) {
    if (len(path) == 0) {
        return false;
    }

    char keyBuf[128];
    if (GetNonFixedDriveKey(path, keyBuf, dimof(keyBuf))) {
        Str key(keyBuf);
        const u64 now = GetTickCount64();
        {
            ScopedMutex lock(&gAttrsCacheMutex);
            DriveAvailEntry* e = FindDriveAvailLocked(key, now);
            if (e) {
                return e->isAvailable;
            }
        }
        bool avail = ProbeDriveRootAccessible(key);
        ScopedMutex lock(&gAttrsCacheMutex);
        StoreDriveAvailLocked(key, avail, GetTickCount64());
        return avail;
    }

    WCHAR* ws = CWStrTemp(path);
    WCHAR root[MAX_PATH];
    if (GetVolumePathNameW(ws, root, dimof(root))) {
        return GetFileAttributesW(root) != INVALID_FILE_ATTRIBUTES;
    }

    int n = len(path);
    if (n >= 2 && path.s[1] == ':') {
        WCHAR driveRoot[] = L"X:\\";
        driveRoot[0] = (WCHAR)toupper((u8)path.s[0]);
        if (driveRoot[0] < L'A' || driveRoot[0] > L'Z') {
            return false;
        }
        UINT type = GetDriveTypeW(driveRoot);
        if (type == DRIVE_NO_ROOT_DIR || type == DRIVE_UNKNOWN) {
            return false;
        }
        return GetFileAttributesW(driveRoot) != INVALID_FILE_ATTRIBUTES;
    }
    return false;
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
    return wstr::EqI(fsName, WStrL(L"NTFS")) || wstr::EqI(fsName, WStrL(L"ReFS"));
}

bool IsAbsolute(Str path) {
    return !PathIsRelativeW(CWStrTemp(path));
}

TempStr GetNonVirtualTemp(Str virtualPath) {
    HANDLE hFile = CreateFileW(CWStrTemp(virtualPath), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return virtualPath;
    }

    WCHAR realPath[MAX_PATH * 4];
    DWORD ret = GetFinalPathNameByHandleW(hFile, realPath, dimof(realPath), FILE_NAME_NORMALIZED);

    CloseHandle(hFile);
    if (ret <= 0) {
        return virtualPath;
    }

    TempStr res = ToUtf8Temp(realPath);
    str::TrimPrefix(res, StrL("\\\\?\\"));
    return res;
}

} // namespace path

TempStr GetTempFilePathTemp(Str filePrefix) {
    WCHAR tempDir[MAX_PATH]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    if (!res || res >= dimof(tempDir)) {
        return {};
    }
    if (len(filePrefix) == 0) {
        return ToUtf8Temp(tempDir);
    }
    WCHAR path[MAX_PATH]{};
    WCHAR* filePrefixW = CWStrTemp(filePrefix);
    if (!GetTempFileNameW(tempDir, filePrefixW, 0, path)) {
        DWORD err = GetLastError();
        logf("GetTempFilePathTemp: GetTempFileNameW failed tempDir='%s' prefix='%s' lastError=%u\n", WStr(tempDir),
             filePrefix, err);
        LogLastError(err);
        return {};
    }
    return ToUtf8Temp(path);
}

TempWStr GetSelfExePathW() {
    WCHAR buf[MAX_PATH + 2]{};
    DWORD nChars = dimof(buf) - 1;
    // TODO: GetModuleFileNameW() truncates if too big but doesn't return the needed size
    GetModuleFileNameW((HINSTANCE)&__ImageBase, buf, nChars);
    return str::DupTemp(WStr(buf));
}

// Path of this process image (exe or DLL that contains this code).
TempStr GetSelfExePathTemp() {
    WCHAR buf[MAX_PATH + 2]{};
    DWORD nChars = dimof(buf) - 1;
    GetModuleFileNameW((HINSTANCE)&__ImageBase, buf, nChars);
    return ToUtf8Temp(buf);
}

// Directory containing GetSelfExePathTemp().
TempStr GetSelfExeDirTemp() {
    TempStr path = GetSelfExePathTemp();
    return path::GetDirTemp(path);
}

TempStr GetPathInExeDirTemp(Str fileName) {
    TempStr dir = GetSelfExeDirTemp();
    TempStr path = path::JoinTemp(dir, fileName);
    path = path::NormalizeTemp(path);
    return path;
}

namespace file {

FILE* OpenFILE(Str path) {
    ReportIf(len(path) == 0);
    if (len(path) == 0) {
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
        // keep GetLastError for callers; also log for diagnostics (installer upgrades)
        DWORD err = GetLastError();
        logf("file::WriteFile: CreateFileW failed for '%s' lastError=%u\n", path, err);
        SetLastError(err);
        return false;
    }
    AutoCloseHandle h(fh);

    DWORD size = 0;
    BOOL ok = ::WriteFile(h, data, (DWORD)dataLen, &size, nullptr);
    if (!ok || dataLen != (size_t)size) {
        DWORD err = GetLastError();
        logf("file::WriteFile: WriteFile failed for '%s' ok=%d wrote=%u want=%zu lastError=%u\n", path, (int)ok, size,
             dataLen, err);
        SetLastError(err);
        return false;
    }
    return true;
}

FileHandle OpenReadOnly(Str path) {
    return CreateFileW(CWStrTemp(path), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}

// Opens path for reading and writing, creating it when createIfMissing.
// Shares the file with other readers/writers so a store can stay open while
// someone else inspects it.
FileHandle OpenReadWrite(Str path, bool createIfMissing) {
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD disposition = createIfMissing ? OPEN_ALWAYS : OPEN_EXISTING;
    return CreateFileW(CWStrTemp(path), GENERIC_READ | GENERIC_WRITE, share, nullptr, disposition,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
}

void Close(FileHandle h) {
    if (h != kInvalidFileHandle && h != nullptr) {
        CloseHandle(h);
    }
}

// Moves the file position to the end and returns it, i.e. the current file
// size, or -1 on failure. That offset is where the next write lands.
i64 SeekEnd(FileHandle h) {
    LARGE_INTEGER zero = {};
    LARGE_INTEGER pos = {};
    if (!SetFilePointerEx(h, zero, &pos, FILE_END)) {
        return -1;
    }
    return pos.QuadPart;
}

// Writes all of data at the current file position, looping over partial writes.
bool WriteAll(FileHandle h, Str data) {
    int written = 0;
    while (written < data.len) {
        DWORD n = 0;
        if (!::WriteFile(h, data.s + written, (DWORD)(data.len - written), &n, nullptr) || n == 0) {
            return false;
        }
        written += (int)n;
    }
    return true;
}

// Reads exactly size bytes at offset; a short read (e.g. hitting the end of
// the file) is a failure. Leaves the file position unspecified, so callers
// that also write must seek first.
bool ReadAt(FileHandle h, i64 offset, void* buf, int size) {
    LARGE_INTEGER pos;
    pos.QuadPart = offset;
    if (!SetFilePointerEx(h, pos, nullptr, FILE_BEGIN)) {
        return false;
    }
    int total = 0;
    while (total < size) {
        DWORD n = 0;
        if (!::ReadFile(h, (char*)buf + total, (DWORD)(size - total), &n, nullptr) || n == 0) {
            return false;
        }
        total += (int)n;
    }
    return true;
}

bool Flush(FileHandle h) {
    return FlushFileBuffers(h) != 0;
}

// Text of the error left behind by the last failed call, for error messages.
TempStr LastErrorTemp() {
    return GetLastErrorAsStr(GetTempArena());
}

// Reads up to toRead bytes from the front of the file, zero-filling the rest of
// buf. Returns the number of bytes read or -1 on failure.
//
// Goes to CreateFileW directly instead of the portable fopen()/fread() version:
// the CRT allocates a FILE and its buffer, takes the lowio handle-table lock and
// copies through that buffer before calling CreateFileW anyway. This runs once
// per page when opening an image directory, so that overhead is worth skipping.
int ReadN(Str path, u8* buf, size_t toRead) {
    ReportIf(len(path) == 0);
    if (len(path) == 0 || !buf || toRead > (size_t)UINT32_MAX) {
        return -1;
    }
    // share flags match what the CRT's "rb" mode uses, plus DELETE so we don't
    // block someone else from replacing the file while we peek at its header
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
    HANDLE h = CreateFileW(CWStrTemp(path), GENERIC_READ, share, nullptr, OPEN_EXISTING, flags, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }
    AutoCloseHandle hf(h);
    ZeroMemory(buf, toRead);
    DWORD nRead = 0;
    // a short read at end of file is not an error, same as fread()
    if (!::ReadFile(h, buf, (DWORD)toRead, &nRead, nullptr)) {
        return -1;
    }
    return (int)nRead;
}

static i64 GetSizeFromHandle(FileHandle h) {
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

// Reads the whole file into memory allocated from a (heap if a is nullptr),
// followed by kZeroPaddingCount zero bytes so the result is also a valid
// NUL-terminated char* / WCHAR*.
//
// Skips the CRT for the same reason ReadN() does, plus two wins that matter
// more here because this is used for whole (potentially large) files:
//  - the portable version sizes the file with fseek(END)/ftell(), which is a
//    32-bit long on Windows, so it can't read files >= 2GB at all
//  - we allocate without zeroing: the buffer is immediately overwritten with
//    file data, so pre-zeroing it is a second full-size memset per file
Str ReadFileWithArena(Str filePath, Arena* a) {
    ReportIf(len(filePath) == 0);
    if (len(filePath) == 0) {
        return {};
    }
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
    HANDLE h = CreateFileW(CWStrTemp(filePath), GENERIC_READ, share, nullptr, OPEN_EXISTING, flags, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return {};
    }
    AutoCloseHandle hf(h);

    i64 fileSize = GetSizeFromHandle(h);
    if (fileSize < 0 || fileSize > (i64)(INT_MAX - kZeroPaddingCount)) {
        return {};
    }
    int size = (int)fileSize;
    char* d = (char*)Alloc(a, (size_t)size + kZeroPaddingCount);
    if (!d) {
        return {};
    }
    memset(d + size, 0, kZeroPaddingCount);

    int nTotal = 0;
    while (nTotal < size) {
        DWORD nRead = 0;
        // ::ReadFile is allowed to return less than asked for; loop until done
        if (!::ReadFile(h, d + nTotal, (DWORD)(size - nTotal), &nRead, nullptr)) {
            DWORD err = GetLastError();
            logf("ReadFileWithArena: ReadFile() failed, path: '%s', size: %d, nRead: %d, lastError: %u\n", filePath,
                 size, nTotal, err);
            Free(a, (void*)d);
            return {};
        }
        if (nRead == 0) {
            // unexpected end of file (someone truncated it while we were reading)
            logf("ReadFileWithArena: unexpected eof, path: '%s', size: %d, nRead: %d\n", filePath, size, nTotal);
            Free(a, (void*)d);
            return {};
        }
        nTotal += (int)nRead;
    }
    return Str(d, size);
}

bool Exists(Str path) {
    if (len(path) == 0) {
        return false;
    }
    // GetCachedAttributes: network paths cached 1hr (avoids UI-thread stalls in
    // menu/toolbar rebuild → CanViewExternally → file::Exists).
    DWORD attrs = path::GetCachedAttributes(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// Maps the whole file at path into memory as a read-only view backed by the
// OS file cache. Unlike ReadFile() this doesn't commit private memory for the
// file content: pages are faulted in from disk on first access and can be
// discarded by the OS under memory pressure. Caveats: the file stays locked
// against writers for the lifetime of the mapping, and if the backing file
// becomes unreadable while mapped (e.g. a network drive disconnects),
// touching a mapped page raises EXCEPTION_IN_PAGE_ERROR instead of returning
// an error, so avoid mapping files on unreliable media.
bool MemoryMap(Str path, Mapping* res) {
    HANDLE hFile = OpenReadOnly(path);
    if (hFile == kInvalidFileHandle) {
        return false;
    }
    i64 size = GetSizeFromHandle(hFile);
    if (size <= 0) {
        CloseHandle(hFile);
        return false;
    }
    HANDLE hMapping = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) {
        CloseHandle(hFile);
        return false;
    }
    // mapping the full file can fail in 32-bit builds for files larger than
    // the available address space
    void* data = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!data) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }
    res->data = (u8*)data;
    res->size = size;
    res->hFile = hFile;
    res->hMapping = hMapping;
    return true;
}

void MemoryUnmap(Mapping* m) {
    if (m->data) {
        UnmapViewOfFile(m->data);
    }
    if (m->hMapping) {
        CloseHandle(m->hMapping);
    }
    if (m->hFile != kInvalidFileHandle && m->hFile != nullptr) {
        CloseHandle(m->hFile);
    }
    *m = {};
}

static bool GetInfo(Str path, WIN32_FILE_ATTRIBUTE_DATA& fileInfo) {
    return path::GetCachedAttributesEx(path, &fileInfo);
}

i64 GetSize(Str path) {
    ReportIf(len(path) == 0);
    if (len(path) == 0) {
        return -1;
    }
    WIN32_FILE_ATTRIBUTE_DATA fileInfo{};
    if (!GetInfo(path, fileInfo)) {
        // Cache can fail (e.g. "drive offline" short-circuit or a cached
        // negative attrs result). Probe directly and recover when the path is fine.
        WIN32_FILE_ATTRIBUTE_DATA direct{};
        BOOL ok = GetFileAttributesExW(CWStrTemp(path), GetFileExInfoStandard, &direct);
        if (!ok || (direct.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            return -1;
        }
        u64 sz = ((u64)direct.nFileSizeHigh << 32) | (u64)direct.nFileSizeLow;
        return (i64)sz;
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
    if (len(filePath) == 0) {
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
                                          LARGE_INTEGER /*StreamSize*/, LARGE_INTEGER /*StreamBytesTransferred*/,
                                          DWORD /*dwStreamNumber*/, DWORD /*dwCallbackReason*/, HANDLE /*hSourceFile*/,
                                          HANDLE /*hDestinationFile*/, LPVOID lpData) {
    const auto* cb = (const CopyProgressCb*)lpData;
    CopyProgress p;
    p.bytesCopied = TotalBytesTransferred.QuadPart;
    p.bytesTotal = TotalFileSize.QuadPart;
    cb->Call(&p);
    return PROGRESS_CONTINUE;
}

bool Copy(Str dst, Str src, bool dontOverwrite, const CopyProgressCb& cbProgress) {
    if (!cbProgress.IsValid()) {
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
    return path::GetCachedAttributes(path);
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
    return (int)GetPrivateProfileIntW(L"ZoneTransfer", L"ZoneId", URLZONE_INVALID, CWStrTemp(path));
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
    if (len(newPath) == 0 || len(oldPath) == 0) {
        return false;
    }
    BOOL ok = MoveFileW(CWStrTemp(oldPath), CWStrTemp(newPath));
    if (!ok) {
        DWORD err = GetLastError();
        logf("file::Rename: MoveFileW failed old='%s' new='%s' lastError=%u\n", oldPath, newPath, err);
        LogLastError(err);
        return false;
    }
    return true;
}

// Like Rename() but overwrites newPath if it exists, and doesn't return until
// the rename is on disk. Used to publish a file written to a temp path.
bool RenameReplace(Str newPath, Str oldPath) {
    if (len(newPath) == 0 || len(oldPath) == 0) {
        return false;
    }
    DWORD flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
    return MoveFileExW(CWStrTemp(oldPath), CWStrTemp(newPath), flags) != 0;
}

bool OverwriteAtomicRetry(Str dst, Str src, int retryCount, int retrySleepMs) {
    if (len(dst) == 0 || len(src) == 0) {
        return false;
    }

    TempStr dstDir = path::GetDirTemp(dst);
    TempStr dstName = path::GetBaseNameTemp(dst);
    WCHAR* dstDirW = CWStrTemp(dstDir);
    WCHAR* prefixW = CWStrTemp(dstName);
    WCHAR prefix[4] = L"tmp";
    int prefixLen = (int)wcslen(prefixW);
    prefixLen = std::min(prefixLen, 3);
    for (int i = 0; i < prefixLen; i++) {
        prefix[i] = prefixW[i];
    }
    if (prefixLen > 0) {
        prefix[prefixLen] = 0;
    }

    WCHAR tempPathW[MAX_PATH]{};
    if (!GetTempFileNameW(dstDirW, prefix, 0, tempPathW)) {
        LogLastError();
        return false;
    }

    TempStr tempPath = ToUtf8Temp(tempPathW);
    if (!Copy(tempPath, src, false)) {
        Delete(tempPath);
        return false;
    }

    retryCount = std::max(retryCount, 1);
    for (int i = 0; i < retryCount; i++) {
        BOOL ok = MoveFileExW(CWStrTemp(tempPath), CWStrTemp(dst), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (ok) {
            return true;
        }
        if (i + 1 < retryCount && retrySleepMs > 0) {
            Sleep((DWORD)retrySleepMs);
        }
    }

    LogLastError();
    Delete(tempPath);
    return false;
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
    LONGLONG diff = (LONGLONG)t1.QuadPart - (LONGLONG)t2.QuadPart;
    diff = diff / (LONGLONG)10000000L;
    return (int)diff;
}

namespace dir {

bool Exists(WStr dir) {
    if (len(dir) == 0) {
        return false;
    }
    return Exists(ToUtf8Temp(dir));
}

bool Exists(Str dir) {
    if (len(dir) == 0) {
        return false;
    }
    DWORD attrs = path::GetCachedAttributes(dir);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool Create(Str dir) {
    BOOL ok = CreateDirectoryW(CWStrTemp(dir), nullptr);
    if (ok) {
        return true;
    }
    return ERROR_ALREADY_EXISTS == GetLastError();
}

// Create dir and all missing parents (like mkdir -p). Uses SHCreateDirectoryExW
// so intermediates are created in one call instead of recursive CreateDirectory.
bool CreateAll(Str dir, int* errOut) {
    if (errOut) {
        *errOut = 0;
    }
    if (len(dir) == 0) {
        return false;
    }
    if (Exists(dir)) {
        return true;
    }
    int err = SHCreateDirectoryExW(nullptr, CWStrTemp(dir), nullptr);
    if (errOut) {
        *errOut = err;
    }
    // ALREADY_EXISTS / FILE_EXISTS can mean a file is in the way; require a directory.
    // A false here with err == ERROR_SUCCESS means the create reported success but the
    // directory was gone by the time we looked (e.g. removed by another thread).
    if (err == ERROR_SUCCESS || err == ERROR_ALREADY_EXISTS || err == ERROR_FILE_EXISTS) {
        return Exists(dir);
    }
    return false;
}

// SHFileOperation wants a double-NUL-terminated path list
static bool ShDelete(Str path) {
    TempWStr pathW = ToWStrTemp(path);
    int n = len(pathW) + 2;
    TempWStr doubleTerminated = WStr(AllocArrayTemp<WCHAR>(n), n);
    wstr::BufSet(doubleTerminated, pathW);
    FILEOP_FLAGS flags = FOF_NO_UI;
    uint op = FO_DELETE;
    SHFILEOPSTRUCTW shfo = {nullptr, op, doubleTerminated.s, nullptr, flags, FALSE, nullptr, nullptr};
    int res = SHFileOperationW(&shfo);
    return res == 0;
}

bool RemoveAll(Str dir) {
    return ShDelete(dir);
}

// Delete everything inside dir but keep dir itself, so code that races with us
// still finds the directory there (see SaveThumbnail / dir::CreateAll).
// A "dir\*" wildcard is how SHFileOperation spells "contents but not the dir".
bool Empty(Str dir) {
    if (!Exists(dir)) {
        return false;
    }
    return ShDelete(path::JoinTemp(dir, StrL("*")));
}

bool HasWriteAccess(Str dir) {
    if (len(dir) == 0) {
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
        return ToUtf8Temp(WStr(buf, (int)n));
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
        return ToUtf8Temp(WStr(buf, (int)n));
    }
    return {};
}

TempStr ToAbsolutePathTemp(Str path) {
    WCHAR buf[MAX_PATH];
    DWORD n = GetFullPathNameW(CWStrTemp(path), MAX_PATH, buf, nullptr);
    if (n > 0 && n < MAX_PATH) {
        return ToUtf8Temp(WStr(buf, (int)n));
    }
    return path;
}

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#if OS_WIN
#define PATH_SEP "\\"
#define PATH_SEP_CHAR '\\'
#define PATH_SEP_WSTR L"\\"
#define PATH_SEP_WCHAR L'\\'
#else
#define PATH_SEP "/"
#define PATH_SEP_CHAR '/'
#define PATH_SEP_WSTR L"/"
#define PATH_SEP_WCHAR L'/'
#endif

namespace path {

bool IsSep(char c);

TempStr GetExtTemp(Str path);
TempStr GetBaseNameTemp(Str path);
TempStr GetPathNoExtTemp(Str path);

TempStr GetDirTemp(Str path);
TempWStr GetDirTemp(WStr path);

TempStr GetNonVirtualTemp(Str virtualPath);

Str Join(Arena* a, Str path, Str fileName);
Str Join(Str path, Str fileName);
WStr Join(WStr path, WStr fileName, WStr fileName2 = WStr());
TempStr JoinTemp(Str path, Str fileName, Str fileName2 = Str());
TempWStr JoinTemp(WStr path, WStr fileName, WStr fileName2 = WStr());

bool IsDirectory(Str path);

TempStr NormalizeTemp(Str path);
TempStr ToOSTemp(Str path);

TempStr ShortPathTemp(Str path);
bool IsSame(Str path1, Str path2);
bool HasVariableDriveLetter(Str path);
bool IsOnFixedDrive(Str path);
bool IsOnNetworkDrive(Str path);
bool IsCloudPlaceholder(Str path);
bool SupportsChangeNotifications(Str path);
bool IsAbsolute(Str path);

bool IsWslUnc(Str path);
bool IsWslMount(Str path);
TempStr WslUncToUnixTemp(Str path);
TempStr WindowsToWslMountTemp(Str path);

bool Match(Str path, Str filter);

enum Type {
    None, // path doesn't exist
    File,
    Dir,
};
Type GetType(Str path);

} // namespace path

TempStr GetTempFilePathTemp(Str filePrefix = Str());
TempStr GetPathInExeDirTemp(Str fileName = Str());
TempStr MakeUniqueFilePathTemp(Str path);

namespace file {

#if OS_WIN
using FileHandle = HANDLE;
inline const FileHandle kInvalidFileHandle = INVALID_HANDLE_VALUE;
#else
using FileHandle = int;
constexpr FileHandle kInvalidFileHandle = -1;
#endif

bool Exists(Str path);

FILE* OpenFILE(Str path);
FileHandle OpenReadOnly(Str path);
Str ReadFileWithArena(Str path, Arena*);
Str ReadFile(Str path);
int ReadN(Str path, u8* buf, size_t toRead);
bool WriteFile(Str path, Str);

i64 GetSize(FileHandle h);
i64 GetSize(Str path);

// read-only memory-mapped view of an entire file
struct Mapping {
    u8* data = nullptr;
    i64 size = 0;
    FileHandle hFile = kInvalidFileHandle;
    void* hMapping = nullptr;
};
bool MemoryMap(Str path, Mapping*);
void MemoryUnmap(Mapping*);
bool Delete(Str path);
bool DeleteFileToTrash(Str path);

FILETIME GetModificationTime(Str path);

bool SetModificationTime(Str path, FILETIME lastMod);

DWORD GetAttributes(Str path);
bool SetAttributes(Str path, DWORD attrs);

bool StartsWithN(Str path, Str s);
bool StartsWith(Str path, Str s);

int GetZoneIdentifier(Str path);
bool SetZoneIdentifier(Str path, int zoneId = URLZONE_INTERNET);
bool DeleteZoneIdentifier(Str path);

// Progress reported by Copy(). bytesTotal == 0 means "total not known".
struct CopyProgress {
    i64 bytesCopied;
    i64 bytesTotal;
};
using CopyProgressCb = Func1<CopyProgress*>;

// Thread-local progress callback honored by long-running copies (e.g.
// caching a network-drive cbx locally). Caller sets it before triggering
// an operation that may copy large files; clears it afterwards.
extern thread_local CopyProgressCb gFileCopyProgressCb;

bool Copy(Str dst, Str src, bool dontOverwrite);
bool Copy(Str dst, Str src, bool dontOverwrite, const CopyProgressCb& cbProgress);
bool Rename(Str newPath, Str oldPath);

bool SetAccessTime(Str path, FILETIME accessTime);
FILETIME GetAccessTime(Str path);

} // namespace file

namespace dir {

bool Exists(WStr dir);
bool Exists(Str dir);

bool Create(Str dir);
bool CreateForFile(Str path);
bool CreateAll(Str dir);
bool RemoveAll(Str dir);
bool HasWriteAccess(Str dir);

} // namespace dir

// global file utilities (paths are UTF-8); moved here from Base.h
// (formerly src/common/file_util.cpp)
bool FileSystemEntryExists(Str s);
Str FindFirstValidParentDir(Str path);
Str PathGetDirTemp(Str path);
Str PathGetNameTemp(Str path);
Str SmartResolveDirectory(Str dir);

bool FileTimeEq(const FILETIME& a, const FILETIME& b);
int FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2);

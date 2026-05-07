/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace path {

bool IsSep(char c);

TempStr GetExtTemp(const char* path);
TempStr GetBaseNameTemp(const char* path);
TempStr GetPathNoExtTemp(const char* path);

TempStr GetDirTemp(const char* path);
TempWStr GetDirTemp(const WCHAR* path);

TempStr GetNonVirtualTemp(const char* virtualPath);

char* Join(Arena* allocator, const char* path, const char* fileName);
char* Join(const char* path, const char* fileName);
WCHAR* Join(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);
TempStr JoinTemp(const char* path, const char* fileName, const char* fileName2 = nullptr);
TempWStr JoinTemp(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);

bool IsDirectory(const char*);

TempStr NormalizeTemp(const char* path);

TempStr ShortPathTemp(const char* pathA);
bool IsSame(const char* path1, const char* path2);
bool HasVariableDriveLetter(const char* path);
bool IsOnFixedDrive(const char* path);
bool IsOnNetworkDrive(const char* path);
// OneDrive / iCloud / Dropbox "Files On-Demand" placeholders — cloud-only
// stubs that hydrate on first read. File I/O is slow and/or bursty, so
// callers may prefer to slurp the whole file into RAM once.
bool IsCloudPlaceholder(const char* path);
bool SupportsChangeNotifications(const char* path);
bool IsAbsolute(const char* path);

bool Match(const char* path, const char* filter);

enum Type {
    None, // path doesn't exist
    File,
    Dir,
};
Type GetType(const char* path);

} // namespace path

TempStr GetTempFilePathTemp(const char* filePrefix = nullptr);
TempStr GetPathInExeDirTemp(const char* fileName = nullptr);
TempStr MakeUniqueFilePathTemp(const char* path);

namespace file {

bool Exists(const char* path);

FILE* OpenFILE(const char* path);
HANDLE OpenReadOnly(const char*);
ByteSlice ReadFileWithAllocator(const char* path, Arena*);
ByteSlice ReadFile(const char* path);
int ReadN(const char* path, char* buf, size_t toRead);
bool WriteFile(const char* path, const ByteSlice&);

i64 GetSize(HANDLE h);
i64 GetSize(const char*);
bool Delete(const char* path);
bool DeleteFileToTrash(const char* path);

FILETIME GetModificationTime(const char* path);

bool SetModificationTime(const char* path, FILETIME lastMod);

DWORD GetAttributes(const char* path);
bool SetAttributes(const char* path, DWORD attrs);

bool StartsWithN(const char* path, const char* s, size_t len);
bool StartsWith(const char* path, const char* s);

int GetZoneIdentifier(const char* path);
bool SetZoneIdentifier(const char* path, int zoneId = URLZONE_INTERNET);
bool DeleteZoneIdentifier(const char* path);

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

bool Copy(const char* dst, const char* src, bool dontOverwrite);
bool Copy(const char* dst, const char* src, bool dontOverwrite, const CopyProgressCb& cbProgress);
bool Rename(const char* newPath, const char* oldPath);

bool SetAccessTime(const char* path, FILETIME accessTime);
FILETIME GetAccessTime(const char* path);

} // namespace file

namespace dir {

bool Exists(const WCHAR* dir);
bool Exists(const char*);

bool Create(const char* dir);
bool CreateForFile(const char* path);
bool CreateAll(const char* dir);
bool RemoveAll(const char* dir);
bool HasWriteAccess(const char* dir);

} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b);

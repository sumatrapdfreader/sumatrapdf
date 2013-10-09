--setfenv(1, require'winapi')
--require'winapi.wintypes'
require'winapi.wintypes'

local ffi = require'ffi'

ffi.cdef[[
static const int MAX_PATH = 260;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES,  *PSECURITY_ATTRIBUTES,  *LPSECURITY_ATTRIBUTES;

typedef enum _GET_FILEEX_INFO_LEVELS {
    GetFileExInfoStandard,
    GetFileExMaxInfoLevel
} GET_FILEEX_INFO_LEVELS;

typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR   cFileName[ MAX_PATH ];
    CHAR   cAlternateFileName[ 14 ];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;
typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR  cFileName[ MAX_PATH ];
    WCHAR  cAlternateFileName[ 14 ];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;
]]

ffi.cdef[[
typedef enum _FINDEX_INFO_LEVELS {
    FindExInfoStandard,
    FindExInfoBasic,
    FindExInfoMaxInfoLevel
} FINDEX_INFO_LEVELS;

typedef enum _FINDEX_SEARCH_OPS {
    FindExSearchNameMatch,
    FindExSearchLimitToDirectories,
    FindExSearchLimitToDevices,
    FindExSearchMaxSearchOp
} FINDEX_SEARCH_OPS;

static const int FIND_FIRST_EX_CASE_SENSITIVE  = 0x00000001;
static const int FIND_FIRST_EX_LARGE_FETCH     = 0x00000002;
]]
ffi.cdef[[
LONG
CompareFileTime(
     const FILETIME *lpFileTime1,
     const FILETIME *lpFileTime2
    );

BOOL
CreateDirectoryA(
         LPCSTR lpPathName,
     LPSECURITY_ATTRIBUTES lpSecurityAttributes
    );
BOOL
CreateDirectoryW(
         LPCWSTR lpPathName,
     LPSECURITY_ATTRIBUTES lpSecurityAttributes
    );

HANDLE
CreateFileA(
         LPCSTR lpFileName,
         DWORD dwDesiredAccess,
         DWORD dwShareMode,
     LPSECURITY_ATTRIBUTES lpSecurityAttributes,
         DWORD dwCreationDisposition,
         DWORD dwFlagsAndAttributes,
     HANDLE hTemplateFile
    );
HANDLE
CreateFileW(
         LPCWSTR lpFileName,
         DWORD dwDesiredAccess,
         DWORD dwShareMode,
     LPSECURITY_ATTRIBUTES lpSecurityAttributes,
         DWORD dwCreationDisposition,
         DWORD dwFlagsAndAttributes,
     HANDLE hTemplateFile
    );

BOOL
DefineDosDeviceW(
         DWORD dwFlags,
         LPCWSTR lpDeviceName,
     LPCWSTR lpTargetPath
    );

BOOL
DeleteFileA(
     LPCSTR lpFileName
    );

BOOL
DeleteFileW(
     LPCWSTR lpFileName
    );

BOOL
DeleteVolumeMountPointW(
     LPCWSTR lpszVolumeMountPoint
    );

BOOL
FileTimeToLocalFileTime(
      const FILETIME *lpFileTime,
     LPFILETIME lpLocalFileTime
    );

BOOL
FindClose(HANDLE hFindFile);

BOOL
FindCloseChangeNotification(
     HANDLE hChangeHandle
    );

HANDLE
FindFirstChangeNotificationA(
     LPCSTR lpPathName,
     BOOL bWatchSubtree,
     DWORD dwNotifyFilter
    );

HANDLE
FindFirstChangeNotificationW(
     LPCWSTR lpPathName,
     BOOL bWatchSubtree,
     DWORD dwNotifyFilter
    );

HANDLE
FindFirstFileA(
      LPCSTR lpFileName,
     LPWIN32_FIND_DATAA lpFindFileData
    );

HANDLE
FindFirstFileExA(
           LPCSTR lpFileName,
           FINDEX_INFO_LEVELS fInfoLevelId,
          LPVOID lpFindFileData,
           FINDEX_SEARCH_OPS fSearchOp,
     LPVOID lpSearchFilter,
           DWORD dwAdditionalFlags
    );

HANDLE
FindFirstFileExW(
  LPCWSTR lpFileName,
  FINDEX_INFO_LEVELS fInfoLevelId,
  LPVOID lpFindFileData,
  FINDEX_SEARCH_OPS fSearchOp,
  LPVOID lpSearchFilter,
  DWORD dwAdditionalFlags);

HANDLE
FindFirstFileW(
      LPCWSTR lpFileName,
     LPWIN32_FIND_DATAW lpFindFileData
    );

HANDLE
FindFirstVolumeW(
     LPWSTR lpszVolumeName,
     DWORD cchBufferLength
    );

BOOL
FindNextChangeNotification(
     HANDLE hChangeHandle
    );

BOOL
FindNextFileA(
      HANDLE hFindFile,
     LPWIN32_FIND_DATAA lpFindFileData
    );

BOOL
FindNextFileW(
      HANDLE hFindFile,
     LPWIN32_FIND_DATAW lpFindFileData
    );

BOOL
FindNextVolumeW(
     HANDLE hFindVolume,
    LPWSTR lpszVolumeName,
        DWORD cchBufferLength
    );

BOOL
FindVolumeClose(
     HANDLE hFindVolume
    );

BOOL
FlushFileBuffers(
     HANDLE hFile
    );

BOOL
GetDiskFreeSpaceA(
      LPCSTR lpRootPathName,
     LPDWORD lpSectorsPerCluster,
     LPDWORD lpBytesPerSector,
     LPDWORD lpNumberOfFreeClusters,
     LPDWORD lpTotalNumberOfClusters
    );
BOOL
GetDiskFreeSpaceW(
      LPCWSTR lpRootPathName,
     LPDWORD lpSectorsPerCluster,
     LPDWORD lpBytesPerSector,
     LPDWORD lpNumberOfFreeClusters,
     LPDWORD lpTotalNumberOfClusters
    );

BOOL
GetDiskFreeSpaceExA(
      LPCSTR lpDirectoryName,
     PULARGE_INTEGER lpFreeBytesAvailableToCaller,
     PULARGE_INTEGER lpTotalNumberOfBytes,
     PULARGE_INTEGER lpTotalNumberOfFreeBytes
    );
BOOL
GetDiskFreeSpaceExW(
      LPCWSTR lpDirectoryName,
     PULARGE_INTEGER lpFreeBytesAvailableToCaller,
     PULARGE_INTEGER lpTotalNumberOfBytes,
     PULARGE_INTEGER lpTotalNumberOfFreeBytes
    );

UINT
GetDriveTypeA(
     LPCSTR lpRootPathName
    );
UINT
GetDriveTypeW(
     LPCWSTR lpRootPathName
    );

DWORD
GetFileAttributesA(
     LPCSTR lpFileName
    );

DWORD
GetFileAttributesW(
     LPCWSTR lpFileName
    );

BOOL
GetFileAttributesExA(
      LPCSTR lpFileName,
      GET_FILEEX_INFO_LEVELS fInfoLevelId,
     LPVOID lpFileInformation
    );

BOOL
GetFileAttributesExW(
      LPCWSTR lpFileName,
      GET_FILEEX_INFO_LEVELS fInfoLevelId,
     LPVOID lpFileInformation
    );

BOOL
GetFileInformationByHandle(
    HANDLE hFile,
    LPBY_HANDLE_FILE_INFORMATION lpFileInformation
    );

DWORD
GetFileSize(
          HANDLE hFile,
     LPDWORD lpFileSizeHigh
    );

BOOL
GetFileSizeEx(
      HANDLE hFile,
     PLARGE_INTEGER lpFileSize
    );

BOOL
GetFileTime(
          HANDLE hFile,
     LPFILETIME lpCreationTime,
     LPFILETIME lpLastAccessTime,
     LPFILETIME lpLastWriteTime
    );

DWORD
GetFileType(
     HANDLE hFile
    );

DWORD
GetFinalPathNameByHandleA (
     HANDLE hFile,
    LPSTR lpszFilePath,
     DWORD cchFilePath,
     DWORD dwFlags
);

DWORD
GetFinalPathNameByHandleW (
     HANDLE hFile,
    LPWSTR lpszFilePath,
     DWORD cchFilePath,
     DWORD dwFlags
);

DWORD
GetFullPathNameA(
                LPCSTR lpFileName,
                DWORD nBufferLength,
    LPSTR lpBuffer,
     LPSTR *lpFilePart
    );
DWORD
GetFullPathNameW(
                LPCWSTR lpFileName,
                DWORD nBufferLength,
    LPWSTR lpBuffer,
     LPWSTR *lpFilePart
    );

DWORD
GetLogicalDrives(void);

DWORD
GetLogicalDriveStringsW(
     DWORD nBufferLength,
    LPWSTR lpBuffer
    );


DWORD
GetLongPathNameA(
     LPCSTR lpszShortPath,
    LPSTR  lpszLongPath,
     DWORD cchBuffer
    );
DWORD
GetLongPathNameW(
     LPCWSTR lpszShortPath,
    LPWSTR  lpszLongPath,
     DWORD cchBuffer
    );

DWORD
GetShortPathNameW(
     LPCWSTR lpszLongPath,
    LPWSTR  lpszShortPath,
     DWORD cchBuffer
    );

UINT
GetTempFileNameW(
     LPCWSTR lpPathName,
     LPCWSTR lpPrefixString,
     UINT uUnique,
    LPWSTR lpTempFileName
    );

DWORD
GetTempPathW(
     DWORD nBufferLength,
    LPWSTR lpBuffer
    );

BOOL
GetVolumeInformationByHandleW(
          HANDLE hFile,
    LPWSTR lpVolumeNameBuffer,
          DWORD nVolumeNameSize,
     LPDWORD lpVolumeSerialNumber,
     LPDWORD lpMaximumComponentLength,
     LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
          DWORD nFileSystemNameSize
    );

BOOL
GetVolumeInformationW(
      LPCWSTR lpRootPathName,
    LPWSTR lpVolumeNameBuffer,
          DWORD nVolumeNameSize,
     LPDWORD lpVolumeSerialNumber,
     LPDWORD lpMaximumComponentLength,
     LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
          DWORD nFileSystemNameSize
    );

BOOL
GetVolumeNameForVolumeMountPointW(
     LPCWSTR lpszVolumeMountPoint,
    LPWSTR lpszVolumeName,
     DWORD cchBufferLength
    );

BOOL
GetVolumePathNamesForVolumeNameW(
      LPCWSTR lpszVolumeName,
     LPWCH lpszVolumePathNames,
      DWORD cchBufferLength,
     PDWORD lpcchReturnLength
    );

BOOL
GetVolumePathNameW(
     LPCWSTR lpszFileName,
    LPWSTR lpszVolumePathName,
     DWORD cchBufferLength
    );

BOOL
LocalFileTimeToFileTime(
      const FILETIME *lpLocalFileTime,
     LPFILETIME lpFileTime
    );

BOOL
LockFile(
     HANDLE hFile,
     DWORD dwFileOffsetLow,
     DWORD dwFileOffsetHigh,
     DWORD nNumberOfBytesToLockLow,
     DWORD nNumberOfBytesToLockHigh
    );

BOOL
LockFileEx(
           HANDLE hFile,
           DWORD dwFlags,
     DWORD dwReserved,
           DWORD nNumberOfBytesToLockLow,
           DWORD nNumberOfBytesToLockHigh,
        LPOVERLAPPED lpOverlapped
    );

DWORD
QueryDosDeviceW(
     LPCWSTR lpDeviceName,
    LPWSTR lpTargetPath,
         DWORD ucchMax
    );

BOOL
ReadFile(
            HANDLE hFile,
     LPVOID lpBuffer,
            DWORD nNumberOfBytesToRead,
       LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
    );

BOOL
ReadFileEx(
         HANDLE hFile,
     LPVOID lpBuffer,
         DWORD nNumberOfBytesToRead,
      LPOVERLAPPED lpOverlapped,
     LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    );

BOOL
ReadFileScatter(
           HANDLE hFile,
           FILE_SEGMENT_ELEMENT aSegmentArray[],
           DWORD nNumberOfBytesToRead,
     LPDWORD lpReserved,
        LPOVERLAPPED lpOverlapped
    );

BOOL
RemoveDirectoryA(
     LPCSTR lpPathName
    );

BOOL
RemoveDirectoryW(
     LPCWSTR lpPathName
    );

BOOL
SetEndOfFile(
     HANDLE hFile
    );

BOOL
SetFileAttributesA(
     LPCSTR lpFileName,
     DWORD dwFileAttributes
    );


BOOL
SetFileAttributesW(
     LPCWSTR lpFileName,
     DWORD dwFileAttributes
    );

BOOL
SetFileInformationByHandle(
      HANDLE hFile,
      FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
    LPVOID lpFileInformation,
      DWORD dwBufferSize
);

DWORD
SetFilePointer(
            HANDLE hFile,
            LONG lDistanceToMove,
    PLONG lpDistanceToMoveHigh,
            DWORD dwMoveMethod
    );

BOOL
SetFilePointerEx(
          HANDLE hFile,
          LARGE_INTEGER liDistanceToMove,
     PLARGE_INTEGER lpNewFilePointer,
          DWORD dwMoveMethod
    );

BOOL
SetFileTime(
         HANDLE hFile,
     const FILETIME *lpCreationTime,
     const FILETIME *lpLastAccessTime,
     const FILETIME *lpLastWriteTime
    );

BOOL
SetFileValidData(
     HANDLE hFile,
     LONGLONG ValidDataLength
    );

BOOL
UnlockFile(
     HANDLE hFile,
     DWORD dwFileOffsetLow,
     DWORD dwFileOffsetHigh,
     DWORD nNumberOfBytesToUnlockLow,
     DWORD nNumberOfBytesToUnlockHigh
    );

BOOL
UnlockFileEx(
           HANDLE hFile,
     DWORD dwReserved,
           DWORD nNumberOfBytesToUnlockLow,
           DWORD nNumberOfBytesToUnlockHigh,
        LPOVERLAPPED lpOverlapped
    );

BOOL
WriteFile(
            HANDLE hFile,
    LPCVOID lpBuffer,
            DWORD nNumberOfBytesToWrite,
       LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
    );


BOOL
WriteFileEx(
         HANDLE hFile,
    LPCVOID lpBuffer,
         DWORD nNumberOfBytesToWrite,
      LPOVERLAPPED lpOverlapped,
     LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    );

BOOL
WriteFileGather(
           HANDLE hFile,
           FILE_SEGMENT_ELEMENT aSegmentArray[],
           DWORD nNumberOfBytesToWrite,
     LPDWORD lpReserved,
        LPOVERLAPPED lpOverlapped
    );
]]

INVALID_HANDLE_VALUE = ffi.cast("HANDLE",ffi.cast("LONG_PTR",-1));

function IsValidFileHandle(h)
    if h == nil or h == INVALID_HANDLE_VALUE then return false end
    return true
end

function FindFiles(filePattern)
    filePattern = filePattern or "\\*"
    local filePatternW = wcs32(filePattern)
    local findFileData = ffi.new("WIN32_FIND_DATAW")
    local files = {}
    local h = C.FindFirstFileW(filePatternW, findFileData)
    if not IsValidFileHandle(h) then return files end

    local ok = 1
    while ok ~= 0 do
        local file = mbcs(findFileData.cFileName)
        table.insert(files, file)
        ok = c.FindNextFileW(h, findFileData)
    end
    C.FindClose(h)
    return files
end

--[[
CompareFileTime = Lib.CompareFileTime,
CreateDirectoryW = Lib.CreateDirectoryW,
CreateFileW = Lib.CreateFileW,
DefineDosDeviceW = Lib.DefineDosDeviceW,
DeleteFileW = Lib.DeleteFileW,
DeleteVolumeMountPointW = Lib.DeleteVolumeMountPointW,
FileTimeToLocalFileTime = Lib.FileTimeToLocalFileTime,
FindClose = Lib.FindClose,
FindCloseChangeNotification = Lib.FindCloseChangeNotification,
FindFirstChangeNotificationA = Lib.FindFirstChangeNotificationA,
FindFirstChangeNotificationW = Lib.FindFirstChangeNotificationW,
FindFirstFileA = Lib.FindFirstFileA,
FindFirstFileExA = Lib.FindFirstFileExA,
FindFirstFileExW = Lib.FindFirstFileExW,
FindFirstFileW = Lib.FindFirstFileW,
FindFirstVolumeW = Lib.FindFirstVolumeW,
FindNextChangeNotification = Lib.FindNextChangeNotification,
FindNextFileA = Lib.FindNextFileA,
FindNextFileW = Lib.FindNextFileW,
FindNextVolumeW = Lib.FindNextVolumeW,
FindVolumeClose = Lib.FindVolumeClose,
FlushFileBuffers = Lib.FlushFileBuffers,
GetDiskFreeSpaceA = Lib.GetDiskFreeSpaceA,
GetDiskFreeSpaceExA = Lib.GetDiskFreeSpaceExA,
GetDiskFreeSpaceExW = Lib.GetDiskFreeSpaceExW,
GetDiskFreeSpaceW = Lib.GetDiskFreeSpaceW,
GetDriveTypeA = Lib.GetDriveTypeA,
GetDriveTypeW = Lib.GetDriveTypeW,
GetFileAttributesA = Lib.GetFileAttributesA,
GetFileAttributesExA = Lib.GetFileAttributesExA,
GetFileAttributesExW = Lib.GetFileAttributesExW,
GetFileAttributesW = Lib.GetFileAttributesW,
GetFileInformationByHandle = Lib.GetFileInformationByHandle,
GetFileSize = Lib.GetFileSize,
GetFileSizeEx = Lib.GetFileSizeEx,
GetFileTime = Lib.GetFileTime,
GetFileType = Lib.GetFileType,
GetFinalPathNameByHandleA = Lib.GetFinalPathNameByHandleA,
GetFinalPathNameByHandleW = Lib.GetFinalPathNameByHandleW,
GetFullPathNameA = Lib.GetFullPathNameA,
GetFullPathNameW = Lib.GetFullPathNameW,
GetLogicalDrives = Lib.GetLogicalDrives,
GetLogicalDriveStringsW = Lib.GetLogicalDriveStringsW,
GetLongPathNameA = Lib.GetLongPathNameA,
GetLongPathNameW = Lib.GetLongPathNameW,
GetShortPathNameW = Lib.GetShortPathNameW,
GetTempFileNameW = Lib.GetTempFileNameW,
GetTempPathW = Lib.GetTempPathW,
GetVolumeInformationByHandleW = Lib.GetVolumeInformationByHandleW,
GetVolumeInformationW = Lib.GetVolumeInformationW,
GetVolumeNameForVolumeMountPointW = Lib.GetVolumeNameForVolumeMountPointW,
GetVolumePathNamesForVolumeNameW = Lib.GetVolumePathNamesForVolumeNameW,
GetVolumePathNameW = Lib.GetVolumePathNameW,
LocalFileTimeToFileTime = Lib.LocalFileTimeToFileTime,
LockFile = Lib.LockFile,
LockFileEx = Lib.LockFileEx,
QueryDosDeviceW = Lib.QueryDosDeviceW,
ReadFile = Lib.ReadFile,
ReadFileEx = Lib.ReadFileEx,
ReadFileScatter = Lib.ReadFileScatter,
RemoveDirectoryA = Lib.RemoveDirectoryA,
RemoveDirectoryW = Lib.RemoveDirectoryW,
SetEndOfFile = Lib.SetEndOfFile,
SetFileAttributesA = Lib.SetFileAttributesA,
SetFileAttributesW = Lib.SetFileAttributesW,
SetFileInformationByHandle = Lib.SetFileInformationByHandle,
SetFilePointer = Lib.SetFilePointer,
SetFilePointerEx = Lib.SetFilePointerEx,
SetFileTime = Lib.SetFileTime,
SetFileValidData = Lib.SetFileValidData,
UnlockFile = Lib.UnlockFile,
UnlockFileEx = Lib.UnlockFileEx,
WriteFile = Lib.WriteFile,
WriteFileEx = Lib.WriteFileEx,
WriteFileGather = Lib.WriteFileGather,
--]]

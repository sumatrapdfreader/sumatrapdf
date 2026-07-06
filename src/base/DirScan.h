/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Background directory scanning (implemented in DirScan.cpp).
// Not self-sufficient: include after utils/Base.h.

// Forward declaration for DirEntry
struct DirEntries;

// Sentinel value indicating directory is still being scanned
#define kStillScanningDir ((DirEntries*)(uintptr_t)-2)

// Check if DirEntry is a directory (dv != nullptr means it's a dir)
inline bool IsDir(DirEntries* dv) {
    return dv != nullptr;
}

struct DirEntry {
    Str name;
    u64 size;
    DirEntries* dv; // nullptr=file, kStillScanningDir=dir not yet scanned, else=scanned dir
    FILETIME createTime;
    FILETIME modTime;
};

struct DirEntries {
    Str fullDir; // Full path of this directory
    int len;
    DirEntry* els;
    Str err; // Error message if directory couldn't be read, empty if none
};

struct DirEntriesNode {
    DirEntriesNode* next;
    DirEntries* dv;
    bool nonRecursive; // If true, don't queue subdirectories for scanning
};

// Callback type for when a directory scan completes
typedef void (*OnScannedDirCallback)(DirEntries* dv, void* userData);

// Background directory reader thread context
struct DirScanCtx {
    Arena* a; // Permanent data arena
    OnScannedDirCallback onScannedDir;
    void* userData;
    Mutex cs;                    // Protect queue access
    HANDLE hSemaphore;           // Counting semaphore for work items
    HANDLE hQueueEmptyEvent;     // Signaled when all work is done (queue empty + no in-flight)
    HANDLE hThreadExitedEvent;   // Signaled when thread has exited
    DirEntriesNode* dirsToVisit; // Queue of directories to scan
    AtomicBool shouldExit;       // Signal thread to exit
    AtomicInt inFlightCount;     // Number of directories currently being processed
};

DirScanCtx* CreateDirScanCtx(Arena* arena, OnScannedDirCallback callback, void* userData);
void AskDirScanThreadToQuit(DirScanCtx* ctx);
DirEntries* RequestDirScan(DirScanCtx* ctx, Str dir);
void QueueDirScan(DirScanCtx* ctx, DirEntries* dv, bool nonRecursive = false);
void RequestDirRescan(DirScanCtx* ctx, DirEntries* dv);

// Directory utilities (paths are UTF-8)
DirEntry* FindEntryByName(DirEntries* dv, Str name);
DWORD WINAPI DirScanThread(LPVOID param);

DirEntries* AllocDirEntries(Arena* arena, Str fullDir);
void ReadDirectory(Arena* arena, DirEntries* dv, AtomicBool* shouldExit);

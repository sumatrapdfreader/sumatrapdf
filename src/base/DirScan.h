/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Directory iteration and background scanning (implemented in DirScan*.cpp).
// Not self-sufficient: include after base/Base.h.

struct StrQueue;

struct DirIterEntry {
#if OS_WIN
    WIN32_FIND_DATAW* fd = nullptr;
#endif
    Str name;
    Str filePath;
    i64 size = 0;
    FILETIME accessTime{};
    FILETIME modificationTime{};
    bool isDir = false;
    bool isFile = false;
    bool stopTraversal = false;
    bool fileMatches = false;
};

struct DirIter {
    Str dir;
    bool includeFiles = true;
    bool includeDirs = false;
    bool recurse = false;

    DirIter() = default;
    explicit DirIter(Str dir);

    struct iterator {
        const DirIter* di;
        bool didFinish = false;

        StrVec dirsToVisit;
        TempStr currDir;
#if OS_WIN
        WStr pattern;
        WIN32_FIND_DATAW fd{};
        HANDLE h = nullptr;
#else
        void* dirHandle = nullptr;
#endif
        DirIterEntry data;

        iterator(const DirIter*, bool);
        iterator(const iterator&);
        iterator& operator=(const iterator&);
        ~iterator();

        DirIterEntry* operator*();
        iterator& operator++();   // ++it
        iterator operator++(int); // it++
        iterator& operator+(int); // it += n
        friend bool operator==(const iterator& a, const iterator& b);
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    iterator begin() const;
    iterator end() const;
};

void StartDirTraverseAsync(StrQueue* queue, Str dir, bool recurse);

i64 GetFileSize(DirIterEntry*);
bool IsDirectory(DirIterEntry*);
bool IsRegularFile(DirIterEntry*);

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
    // Stands for something elsewhere rather than being it: a symlink or, on
    // Windows, a junction. Recursive scanning stops here, so it keeps its
    // kStillScanningDir until someone asks for it by name.
    bool isLink;
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

// Callback type for when a directory scan completes.
//
// wasRequested tells the two kinds of result apart. A requested one comes from
// RequestDirScan and its DirEntries is freshly made, so the caller may already
// have one for the same directory. A result that isn't requested was found by
// walking, and its DirEntries is the one the caller was handed by
// QueueDirScan, so there is nothing it can collide with.
typedef void (*OnScannedDirCallback)(DirEntries* dv, bool wasRequested, void* userData);

struct DirScanCtx;

// One scanning thread per drive, so a slow drive doesn't hold up the others.
// Workers are created on demand as directories on new drives get queued.
struct DirScanWorker {
    DirScanWorker* next;
    DirScanCtx* ctx;
    Str drive; // "C:\" or "\\server\share\", whatever this worker owns

    Mutex cs;                  // protects everything below
    ConditionVariable hasWork; // signaled when work is queued or the thread should exit
    // Directories the caller asked for explicitly, always taken before the
    // ones discovered by walking. Without this a directory the user just
    // navigated to waits behind everything a recursive scan has queued up.
    DirEntriesNode* priorityDirs;
    // Those found by walking that fall under the directory the caller says it
    // is showing. Their sizes are the ones somebody is looking at, so they go
    // before the rest of the walk. See SetDirScanPriorityDir.
    DirEntriesNode* preferredDirs;
    DirEntriesNode* preferredDirsLast;
    DirEntriesNode* dirsToVisit;
    DirEntriesNode* dirsToVisitLast; // tail, so appending doesn't walk the queue
    int inFlightCount;               // directories currently being read
    bool threadExited;

    // progress, so the caller can show what the scan is doing
    int nFiles;
    int nDirs;
    u64 totalSize;
    u64 busySinceMs;  // when the current stretch of scanning began, 0 if idle
    u64 scannedForMs; // time spent scanning before the current stretch
};

// Background directory reader
struct DirScanCtx {
    Arena* a; // Permanent data arena
    OnScannedDirCallback onScannedDir;
    void* userData;
    Mutex cs;              // protects the worker list
    AtomicBool shouldExit; // signal all threads to exit
    DirScanWorker* workers;
    Str priorityDir; // what SetDirScanPriorityDir was last given
};

// A worker's progress, copied out so the caller doesn't hold a lock while
// using it.
struct DirScanProgress {
    Str drive;
    int nFiles;
    int nDirs;
    u64 totalSize;
    u64 scanningForMs;
    bool scanning;
};

DirScanCtx* CreateDirScanCtx(Arena* arena, OnScannedDirCallback callback, void* userData);
void AskDirScanThreadToQuit(DirScanCtx* ctx);
DirEntries* RequestDirScan(DirScanCtx* ctx, Str dir, bool nonRecursive = false);
void QueueDirScan(DirScanCtx* ctx, DirEntries* dv, bool nonRecursive = false);
void RequestDirRescan(DirScanCtx* ctx, DirEntries* dv);
void SetDirScanPriorityDir(DirScanCtx* ctx, Str dir);

bool DirScanIsIdle(DirScanCtx* ctx);
int GetDirScanProgress(DirScanCtx* ctx, DirScanProgress* out, int maxOut);

DirEntry* FindEntryByName(DirEntries* dv, Str name);

DirEntries* AllocDirEntries(Arena* arena, Str fullDir);
void ReadDirectory(Arena* arena, DirEntries* dv, AtomicBool* shouldExit);

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
        TempStr currDir = {};
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
    Mutex cs;                  // Protect queue access
    ConditionVariable hasWork; // Signaled when work is queued or thread should exit
    bool threadExited;
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
void DirScanThread(DirScanCtx* ctx);

DirEntries* AllocDirEntries(Arena* arena, Str fullDir);
void ReadDirectory(Arena* arena, DirEntries* dv, AtomicBool* shouldExit);

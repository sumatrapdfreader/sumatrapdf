/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/StrQueue.h"

#if OS_WIN
#include "base/Win.h"
#endif

#include "base/DirScan.h"

void AdvanceDirIter(DirIter::iterator* it, int n);
void CloseDirIter(DirIter::iterator* it);

DirIter::DirIter(Str dir) : dir(dir) {}

DirIter::iterator::iterator(const DirIter* di, bool didFinish) {
    this->di = di;
    this->dirsToVisit.Append(di->dir);
    this->didFinish = didFinish;
#if OS_WIN
    this->data.fd = &this->fd;
#endif
    AdvanceDirIter(this, 1);
}

DirIter::iterator::iterator(const iterator& that) {
    *this = that;
}

DirIter::iterator& DirIter::iterator::operator=(const iterator& that) {
    if (this == &that) {
        return *this;
    }
    CloseDirIter(this);
    this->di = that.di;
    this->didFinish = that.didFinish;
    this->dirsToVisit = that.dirsToVisit;
    this->currDir = that.currDir;
    this->data = that.data;
#if OS_WIN
    this->fd = that.fd;
    this->data.fd = &this->fd;
#endif
    return *this;
}

DirIter::iterator::~iterator() {
    CloseDirIter(this);
}

DirIter::iterator DirIter::begin() const {
    return {this, false};
}

DirIter::iterator DirIter::end() const {
    return {this, true};
}

DirIterEntry* DirIter::iterator::operator*() {
    if (didFinish) {
        return nullptr;
    }
    return &data;
}

// postfix increment
DirIter::iterator DirIter::iterator::operator++(int) {
    auto res = *this;
    AdvanceDirIter(this, 1);
    return res;
}

DirIter::iterator& DirIter::iterator::operator++() {
    AdvanceDirIter(this, 1);
    return *this;
}

DirIter::iterator& DirIter::iterator::operator+(int n) {
    AdvanceDirIter(this, n);
    return *this;
}

bool operator==(const DirIter::iterator& a, const DirIter::iterator& b) {
    return (a.di == b.di) && (a.didFinish == b.didFinish);
};

bool operator!=(const DirIter::iterator& a, const DirIter::iterator& b) {
    return (a.di != b.di) || (a.didFinish != b.didFinish);
};

i64 GetFileSize(DirIterEntry* de) {
    return de ? de->size : 0;
}

bool IsDirectory(DirIterEntry* de) {
    return de && de->isDir;
}

bool IsRegularFile(DirIterEntry* de) {
    return de && de->isFile;
}

struct DirTraverseThreadData {
    StrQueue* queue = nullptr; // we don't own it
    Str dir;
    bool recurse = false;
    ~DirTraverseThreadData() { str::Free(dir); }
};

static void DirTraverseThread(DirTraverseThreadData* td) {
    DirIter di(td->dir);
    di.includeFiles = true;
    di.includeDirs = false;
    di.recurse = td->recurse;
    for (DirIterEntry* de : di) {
        td->queue->append(de->filePath);
    }
    td->queue->MarkFinished();
    delete td;
}

void StartDirTraverseAsync(StrQueue* queue, Str dir, bool recurse) {
    auto* td = new DirTraverseThreadData{queue, str::Dup(dir), recurse};
    auto fn = MkFunc0(DirTraverseThread, td);
    RunAsync(fn, StrL("DirTraverseThread"));
}

// Find entry by name in a DirEntries
// Directory utilities (paths are UTF-8)
DirEntry* FindEntryByName(DirEntries* dv, Str name) {
    if (!dv) return nullptr;
    for (int i = 0; i < dv->len; i++) {
        // Skip entries still being scanned
        if (dv->els[i].dv == kStillScanningDir) continue;
        if (str::Eq(dv->els[i].name, name)) {
            return &dv->els[i];
        }
    }
    return nullptr;
}

// Allocate a DirEntries with fullDir set
DirEntries* AllocDirEntries(Arena* arena, Str fullDir) {
    DirEntries* dv = (DirEntries*)Alloc(arena, sizeof(DirEntries));
    *dv = {};
    dv->fullDir = str::Dup(arena, fullDir);
    return dv;
}

// Check if path is already in dirsToVisit list (must hold cs)
// Returns the DirEntries* if found, nullptr otherwise
static DirEntries* FindDirInList(DirEntriesNode* list, Str dir) {
    while (list) {
        if (str::Eq(list->dv->fullDir, dir)) {
            return list->dv;
        }
        list = list->next;
    }
    return nullptr;
}

// Allocate a DirEntriesNode using given allocator
static DirEntriesNode* AllocDirEntriesNode(Arena* arena, DirEntries* dv, bool nonRecursive = false) {
    DirEntriesNode* node = (DirEntriesNode*)Alloc(arena, sizeof(DirEntriesNode));
    node->next = nullptr;
    node->dv = dv;
    node->nonRecursive = nonRecursive;
    return node;
}

// The part of a path that decides which worker owns it: the drive for a local
// path, the share for a UNC one. Anything else gets a worker to itself.
static Str DriveOfPath(Arena* a, Str path) {
    if (path.len >= 2 && path.s[0] == '\\' && path.s[1] == '\\') {
        // "\\server\share\dir" -> "\\server\share\"
        int nSeps = 0;
        int i = 2;
        while (i < path.len) {
            if (path.s[i] == '\\') {
                nSeps++;
                if (nSeps == 2) {
                    break;
                }
            }
            i++;
        }
        if (i < path.len) {
            i++; // include the trailing separator
        }
        return str::Dup(a, Str(path.s, i));
    }
    if (path.len >= 2 && path.s[1] == ':') {
        char drive[4] = {(char)toupper((u8)path.s[0]), ':', '\\', 0};
        return str::Dup(a, Str(drive, 3));
    }
    return str::Dup(a, path);
}

static void DirScanWorkerThread(DirScanWorker* w);

// Must hold ctx->cs. Returns null once we're shutting down: a worker queuing
// subdirectories could otherwise start a thread after teardown has collected
// the worker list, and that thread would outlive the context.
static DirScanWorker* FindOrCreateWorker(DirScanCtx* ctx, Str dir) {
    if (AtomicBoolGet(&ctx->shouldExit)) {
        return nullptr;
    }
    Str drive = DriveOfPath(ctx->a, dir);
    for (DirScanWorker* w = ctx->workers; w; w = w->next) {
        if (str::EqI(w->drive, drive)) {
            return w;
        }
    }

    auto* w = new DirScanWorker();
    w->ctx = ctx;
    w->drive = drive;
    w->next = ctx->workers;
    ctx->workers = w;

    ThreadHandle hThread = StartThread(MkFunc0(DirScanWorkerThread, w), StrL("DirScanThread"));
    if (hThread) {
        SafeCloseThreadHandle(&hThread);
    } else {
        w->threadExited = true;
    }
    return w;
}

// Must hold w->cs. Starts the clock if this is the first work in a while.
static void WorkerNoteBusy(DirScanWorker* w) {
    if (w->busySinceMs == 0) {
        w->busySinceMs = GetTickCount64();
    }
}

// Must hold w->cs. Banks the time spent in this stretch of scanning.
static void WorkerNoteIdle(DirScanWorker* w) {
    if (w->busySinceMs != 0) {
        w->scannedForMs += GetTickCount64() - w->busySinceMs;
        w->busySinceMs = 0;
    }
}

// Create and initialize directory reader context
DirScanCtx* CreateDirScanCtx(Arena* arena, OnScannedDirCallback callback, void* userData) {
    DirScanCtx* ctx = new DirScanCtx();
    ctx->a = arena;
    ctx->onScannedDir = callback;
    ctx->userData = userData;
    ctx->shouldExit = 0;
    ctx->workers = nullptr;
    return ctx;
}

// Signal all worker threads to exit and wait for them
void AskDirScanThreadToQuit(DirScanCtx* ctx) {
    if (!ctx) return;

    AtomicBoolSet(&ctx->shouldExit, true);

    ctx->cs.Lock();
    DirScanWorker* workers = ctx->workers;
    ctx->workers = nullptr;
    ctx->cs.Unlock();

    DirScanWorker* w = workers;
    while (w) {
        w->cs.Lock();
        w->hasWork.WakeAll();
        while (!w->threadExited) {
            w->hasWork.Wait(&w->cs);
        }
        w->cs.Unlock();
        DirScanWorker* next = w->next;
        delete w;
        w = next;
    }
    delete ctx;
}

// Is path dir itself, or something below it? Compared the way the file system
// compares them, and only at a separator, so "C:\foo" doesn't swallow
// "C:\foobar".
static bool IsUnderDir(Str path, Str dir) {
    if (len(dir) == 0 || path.len < dir.len) {
        return false;
    }
    if (!str::StartsWithI(path, dir)) {
        return false;
    }
    if (path.len == dir.len) {
        return true;
    }
    char last = dir.s[dir.len - 1];
    if (last == '\\' || last == '/') {
        // a drive root already ends in a separator
        return true;
    }
    char next = path.s[dir.len];
    return next == '\\' || next == '/';
}

// Appends node to a singly linked list kept with a tail pointer.
static void AppendNode(DirEntriesNode** head, DirEntriesNode** last, DirEntriesNode* node) {
    node->next = nullptr;
    if (*last) {
        (*last)->next = node;
    } else {
        *head = node;
    }
    *last = node;
}

// Request a directory scan - adds to FRONT of list (priority for user requests)
// Returns DirEntries* (either existing from queue or newly allocated)
// nonRecursive scans just this directory, without walking into it
DirEntries* RequestDirScan(DirScanCtx* ctx, Str dir, bool nonRecursive) {
    ctx->cs.Lock();
    DirScanWorker* w = FindOrCreateWorker(ctx, dir);
    ctx->cs.Unlock();
    if (!w) {
        return AllocDirEntries(ctx->a, dir);
    }

    w->cs.Lock();

    // Only the priority list is searched. It holds just what the caller asked
    // for, so it stays short, while dirsToVisit can hold tens of thousands of
    // directories during a recursive scan and walking it would stall whoever
    // is navigating.
    DirEntries* dv = FindDirInList(w->priorityDirs, dir);
    if (dv) {
        w->cs.Unlock();
        return dv;
    }

    // Allocate new DirEntries and add to queue
    // Use arena allocator for queue nodes (thread-safe)
    dv = AllocDirEntries(ctx->a, dir);
    DirEntriesNode* node = AllocDirEntriesNode(ctx->a, dv, nonRecursive);
    node->next = w->priorityDirs;
    w->priorityDirs = node;

    WorkerNoteBusy(w);
    w->hasWork.Wake();
    w->cs.Unlock();
    return dv;
}

// Queue a directory scan - adds to end of list (breadth-first scanning)
// If nonRecursive is true, subdirectories won't be queued for scanning
void QueueDirScan(DirScanCtx* ctx, DirEntries* dv, bool nonRecursive) {
    ctx->cs.Lock();
    DirScanWorker* w = FindOrCreateWorker(ctx, dv->fullDir);
    // the string it points at lives in the arena, so it stays valid once we
    // let go of the lock
    Str priorityDir = ctx->priorityDir;
    ctx->cs.Unlock();
    if (!w) {
        return;
    }

    w->cs.Lock();

    // Every caller hands us a freshly allocated DirEntries, so there's nothing
    // to deduplicate against.
    // Use arena allocator for queue nodes (thread-safe)
    DirEntriesNode* node = AllocDirEntriesNode(ctx->a, dv, nonRecursive);

    // Add to the end of one queue or the other, breadth first within each
    if (IsUnderDir(dv->fullDir, priorityDir)) {
        AppendNode(&w->preferredDirs, &w->preferredDirsLast, node);
    } else {
        AppendNode(&w->dirsToVisit, &w->dirsToVisitLast, node);
    }

    WorkerNoteBusy(w);
    w->hasWork.Wake();
    w->cs.Unlock();
}

// Must hold w->cs. Re-sorts everything queued by walking into the ones under
// dir and the ones that aren't, keeping the relative order within each.
static void RepartitionWorkerQueues(DirScanWorker* w, Str dir) {
    DirEntriesNode* nodes = w->preferredDirs;
    if (nodes) {
        w->preferredDirsLast->next = w->dirsToVisit;
    } else {
        nodes = w->dirsToVisit;
    }
    w->preferredDirs = nullptr;
    w->preferredDirsLast = nullptr;
    w->dirsToVisit = nullptr;
    w->dirsToVisitLast = nullptr;

    while (nodes) {
        DirEntriesNode* node = nodes;
        nodes = nodes->next;
        if (IsUnderDir(node->dv->fullDir, dir)) {
            AppendNode(&w->preferredDirs, &w->preferredDirsLast, node);
        } else {
            AppendNode(&w->dirsToVisit, &w->dirsToVisitLast, node);
        }
    }
}

// Scan what's under dir before the rest of the walk, so the sizes filling in
// are the ones being looked at. Re-orders what's already queued. An empty dir
// goes back to plain breadth-first order.
void SetDirScanPriorityDir(DirScanCtx* ctx, Str dir) {
    if (!ctx) {
        return;
    }
    ctx->cs.Lock();
    if (str::EqI(ctx->priorityDir, dir)) {
        ctx->cs.Unlock();
        return;
    }
    ctx->priorityDir = str::Dup(ctx->a, dir);
    // Walking the queues is O(what's queued), but this only runs when the
    // shown directory changes, not per directory scanned.
    for (DirScanWorker* w = ctx->workers; w; w = w->next) {
        w->cs.Lock();
        RepartitionWorkerQueues(w, ctx->priorityDir);
        w->cs.Unlock();
    }
    ctx->cs.Unlock();
}

// Request a refresh of a directory (non-recursive scan).
// A rescan is something the caller just asked for, so it goes on the priority
// list like any other request: behind a recursive scan's queue it would be
// tens of thousands of directories away.
void RequestDirRescan(DirScanCtx* ctx, DirEntries* dv) {
    RequestDirScan(ctx, dv->fullDir, true);
}

// true when no worker has anything left to do
bool DirScanIsIdle(DirScanCtx* ctx) {
    if (!ctx) return true;
    bool idle = true;
    ctx->cs.Lock();
    for (DirScanWorker* w = ctx->workers; w && idle; w = w->next) {
        w->cs.Lock();
        idle = !w->priorityDirs && !w->preferredDirs && !w->dirsToVisit && w->inFlightCount == 0;
        w->cs.Unlock();
    }
    ctx->cs.Unlock();
    return idle;
}

// Fills up to maxOut entries, one per worker, and returns how many were filled.
int GetDirScanProgress(DirScanCtx* ctx, DirScanProgress* out, int maxOut) {
    if (!ctx) return 0;
    int n = 0;
    ctx->cs.Lock();
    for (DirScanWorker* w = ctx->workers; w && n < maxOut; w = w->next) {
        w->cs.Lock();
        DirScanProgress* p = &out[n++];
        p->drive = w->drive;
        p->nFiles = w->nFiles;
        p->nDirs = w->nDirs;
        p->totalSize = w->totalSize;
        p->scanning = w->busySinceMs != 0;
        p->scanningForMs = w->scannedForMs;
        if (p->scanning) {
            p->scanningForMs += GetTickCount64() - w->busySinceMs;
        }
        w->cs.Unlock();
    }
    ctx->cs.Unlock();
    return n;
}

static void DirScanWorkerThread(DirScanWorker* w) {
    DirScanCtx* ctx = w->ctx;
    auto* tempAlloc = GetTempArena();

    while (true) {
        w->cs.Lock();
        while (!w->priorityDirs && !w->preferredDirs && !w->dirsToVisit && !AtomicBoolGet(&ctx->shouldExit)) {
            w->hasWork.Wait(&w->cs);
        }
        if (AtomicBoolGet(&ctx->shouldExit)) {
            w->cs.Unlock();
            break;
        }
        // What the caller asked for comes first, then what's under the
        // directory it says it's showing, then the rest of the walk.
        DirEntriesNode* node = w->priorityDirs;
        bool wasRequested = node != nullptr;
        if (node) {
            w->priorityDirs = node->next;
        } else if (w->preferredDirs) {
            node = w->preferredDirs;
            w->preferredDirs = node->next;
            if (!w->preferredDirs) {
                w->preferredDirsLast = nullptr;
            }
        } else {
            node = w->dirsToVisit;
            if (node) {
                w->dirsToVisit = node->next;
                if (!w->dirsToVisit) {
                    w->dirsToVisitLast = nullptr;
                }
            }
        }
        if (!node) {
            // Spurious wake with empty queue: wait again.
            w->cs.Unlock();
            continue;
        }
        w->inFlightCount++;
        DirEntries* dv = node->dv;
        bool nonRecursive = node->nonRecursive;
        w->cs.Unlock();

        ReadDirectory(ctx->a, dv, &ctx->shouldExit);

        if (AtomicBoolGet(&ctx->shouldExit)) {
            break;
        }

        int nFiles = 0;
        u64 filesSize = 0;
        for (int i = 0; i < dv->len; i++) {
            if (!IsDir(dv->els[i].dv)) {
                nFiles++;
                filesSize += dv->els[i].size;
            }
        }

        if (!nonRecursive) {
            for (int i = 0; i < dv->len; i++) {
                if (AtomicBoolGet(&ctx->shouldExit)) {
                    break;
                }
                DirEntry* e = &dv->els[i];
                if (e->isLink) {
                    // Following it would walk the target twice, or forever if
                    // it points at an ancestor of itself.
                    continue;
                }
                if (e->dv == kStillScanningDir && !str::Eq(e->name, StrL(".."))) {
                    Str subPath = path::JoinTemp(dv->fullDir, e->name);
                    DirEntries* subDv = AllocDirEntries(ctx->a, subPath);
                    e->dv = subDv;
                    QueueDirScan(ctx, subDv);
                }
            }
        }

        if (AtomicBoolGet(&ctx->shouldExit)) {
            break;
        }

        if (ctx->onScannedDir) {
            ctx->onScannedDir(dv, wasRequested, ctx->userData);
        }

        w->cs.Lock();
        w->nFiles += nFiles;
        w->nDirs++;
        w->totalSize += filesSize;
        w->inFlightCount--;
        bool allDone = !w->priorityDirs && !w->preferredDirs && !w->dirsToVisit && w->inFlightCount == 0;
        if (allDone) {
            WorkerNoteIdle(w);
            w->hasWork.WakeAll();
        }
        w->cs.Unlock();

        tempAlloc->Reset();
    }

    w->cs.Lock();
    WorkerNoteIdle(w);
    w->threadExited = true;
    w->hasWork.WakeAll();
    w->cs.Unlock();

    if (gTempArena) {
        ArenaDelete(gTempArena);
        gTempArena = nullptr;
    }
}

#if OS_WIN

static i64 GetWinFileSize(WIN32_FIND_DATAW* fd) {
    ULARGE_INTEGER ul;
    ul.HighPart = fd->nFileSizeHigh;
    ul.LowPart = fd->nFileSizeLow;
    return (i64)ul.QuadPart;
}

// try to filter out things that are not files
// or not meant to be used by other applications
//
// Takes the whole find data because the reparse tag is in dwReserved0, and
// only there when the entry is a reparse point.
static bool IsRegularFile(const WIN32_FIND_DATAW& fd) {
    DWORD fileAttr = fd.dwFileAttributes;
    if (fileAttr & FILE_ATTRIBUTE_DEVICE) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_TEMPORARY) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT) {
        // A symlink stands for a file somewhere else rather than being one. A
        // cloud provider's placeholder is the file: OneDrive puts a reparse
        // point on everything it syncs, and reading one fetches the contents.
        return !IsReparseTagNameSurrogate(fd.dwReserved0);
    }
    // Offline with no reparse point is the older kind of archived storage,
    // where reading can block for a very long time. A cloud placeholder is
    // marked offline as well, but it was already let through above.
    if (fileAttr & FILE_ATTRIBUTE_OFFLINE) {
        return false;
    }
    return true;
}

static bool IsDirectoryAttr(DWORD fileAttr) {
    return (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Whether the entry stands for something elsewhere rather than being it.
// Symlinks and junctions are such name surrogates, and descending into one can
// loop or count the same files twice: a profile has
// AppData\Local\"Application Data" pointing back at AppData\Local. A cloud
// provider's reparse point is not a surrogate: OneDrive marks every folder it
// syncs with one, and those are ordinary directories that only look like
// something else. Treating them as surrogates hides all of OneDrive.
//
// dwReserved0 holds the reparse tag, but only when the entry is a reparse
// point, which is why this takes the whole find data. FindExInfoBasic still
// fills it in; it only leaves out cAlternateFileName.
static bool IsNameSurrogate(const WIN32_FIND_DATAW& fd) {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return false;
    }
    return IsReparseTagNameSurrogate(fd.dwReserved0);
}

// Hidden and system together is what Windows calls a protected operating system
// file. Explorer keeps these out of sight even when it's been told to show
// hidden files, behind a second setting of its own, and it's a good rule: what
// it covers is pagefile.sys, System Volume Information, $Recycle.Bin, a
// profile's registry hives, and the legacy junctions ("Moje dokumenty",
// PrintHood, "Ustawienia lokalne") that exist only so pre-Vista programs keep
// working. None of it is a person's own files.
//
// Hidden on its own still shows: AppData, ProgramData and NTUSER.DAT are things
// someone browsing might actually be after.
static bool IsProtectedOSFile(DWORD fileAttr) {
    DWORD both = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    return (fileAttr & both) == both;
}

static bool IsSpecialDir(Str s) {
    return str::Eq(s, StrL(".")) || str::Eq(s, StrL(".."));
}

// Store apps put 0-byte execution aliases under WindowsApps with tag
// IO_REPARSE_TAG_APPEXECLINK. FindFirstFile reports size 0; the reparse
// buffer names the real package executable, whose size is what to show.
#ifndef IO_REPARSE_TAG_APPEXECLINK
constexpr ULONG IO_REPARSE_TAG_APPEXECLINK = 0x8000001BL;
#endif

static bool IsAbsolutePathW(const WCHAR* s, int cch) {
    if (cch >= 3 && s[1] == L':' && (s[2] == L'\\' || s[2] == L'/')) {
        return true;
    }
    if (cch >= 2 && s[0] == L'\\' && s[1] == L'\\') {
        return true;
    }
    return false;
}

static u64 SizeOfAppExecLinkTarget(Str path) {
    TempWStr wpath = ToWStrTemp(path);
    HANDLE h = CreateFileW(wpath.s, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                           FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (!IsValidHandle(h)) {
        return 0;
    }

    // Header is 8 bytes (tag, data length, reserved); body starts with a
    // ULONG version, then NUL-terminated WCHAR strings. Version 3 has package
    // id, entry point, application id, executable path, application type.
    BYTE buf[16 * 1024];
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, nullptr, 0, buf, sizeof(buf), &bytes, nullptr);
    CloseHandle(h);
    if (!ok || bytes < 12) {
        return 0;
    }

    DWORD tag = *(DWORD*)buf;
    WORD dataLen = *(WORD*)(buf + 4);
    if (tag != IO_REPARSE_TAG_APPEXECLINK || dataLen < 4 || (DWORD)dataLen + 8 > bytes) {
        return 0;
    }

    const BYTE* data = buf + 8;
    const WCHAR* p = (const WCHAR*)(data + 4);
    const WCHAR* end = (const WCHAR*)(data + dataLen);
    while (p < end && *p) {
        const WCHAR* s = p;
        while (p < end && *p) {
            p++;
        }
        int cch = (int)(p - s);
        if (p < end) {
            p++; // skip the NUL between strings
        }
        if (!IsAbsolutePathW(s, cch)) {
            continue;
        }
        // GetFileAttributesEx needs a NUL-terminated path; DupTemp adds one.
        TempWStr target = str::DupTemp(WStr((WCHAR*)s, cch));
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExW(target.s, GetFileExInfoStandard, &fad)) {
            return 0;
        }
        return ((u64)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    }
    return 0;
}

static void SetDirIterData(DirIter::iterator* it, TempStr name, TempStr path, bool isFile, bool isDir) {
    it->data.fd = &it->fd;
    it->data.name = name;
    it->data.filePath = path;
    it->data.size = GetWinFileSize(&it->fd);
    it->data.accessTime = it->fd.ftLastAccessTime;
    it->data.modificationTime = it->fd.ftLastWriteTime;
    it->data.isFile = isFile;
    it->data.isDir = isDir;
}

void CloseDirIter(DirIter::iterator* it) {
    wstr::FreePtr(&it->pattern);
    SafeFindClose(&it->h);
}

void AdvanceDirIter(DirIter::iterator* it, int n) {
    ReportIf(n != 1);
    if (it->didFinish) {
        return;
    }
    if (it->data.stopTraversal) {
        // could have been set by user accessing prev traversal
        it->didFinish = true;
        return;
    }

    bool includeFiles = it->di->includeFiles;
    bool includeDirs = it->di->includeDirs;
    bool recur = it->di->recurse;

    bool ok;
    bool isFile;
    bool isDir;
    TempStr name;
    TempStr path;

NextDir:
    if (len(it->pattern) == 0) {
        int nDirs = len(it->dirsToVisit);
        if (nDirs == 0) {
            goto DidFinish;
        }
        it->currDir = it->dirsToVisit.RemoveAt(nDirs - 1);
        TempWStr ws = ToWStrTemp(it->currDir);
        it->pattern = path::Join(ws, WStrL(L"*"));
        it->h = FindFirstFileW(it->pattern.s, &it->fd);
        if (!IsValidHandle(it->h)) {
            goto DidFinish;
        }
    } else {
        ok = FindNextFileW(it->h, &it->fd);
        if (!ok) {
            CloseDirIter(it);
            goto NextDir;
        }
    }
    while (true) {
        isFile = IsRegularFile(it->fd);
        isDir = IsDirectoryAttr(it->fd.dwFileAttributes);
        name = ToUtf8Temp(it->fd.cFileName);
        path = path::JoinTemp(it->currDir, name);
        SetDirIterData(it, name, path, isFile, isDir);
        if (isFile && includeFiles) {
            return;
        }
        if (isDir && !IsSpecialDir(name)) {
            if (recur && !IsNameSurrogate(it->fd)) {
                it->dirsToVisit.Append(path);
            }
            if (includeDirs) {
                return;
            }
        }
        ok = FindNextFileW(it->h, &it->fd);
        if (!ok) {
            CloseDirIter(it);
            goto NextDir;
        }
    };
DidFinish:
    CloseDirIter(it);
    it->didFinish = true;
}

// field order matches Vec<T> so VecPush() can hand it to the VecNonTemplated helpers
struct TempEntryVec {
    int len;
    int cap;
    DirEntry* els;
};

static const WStr wdot = WStrL(L".");
static const WStr wdotdot = WStrL(L"..");

void ReadDirectory(Arena* arena, DirEntries* dv, AtomicBool* shouldExit) {
    if (shouldExit && AtomicBoolGet(shouldExit)) {
        return;
    }

    TempEntryVec temp = {};

    DirEntry dotdot = {};
    dotdot.name = StrL("..");
    dotdot.size = 0;
    dotdot.dv = kStillScanningDir;
    VecPush(GetTempArena(), temp, dotdot);

    WStr widePath = ToWStrTemp(dv->fullDir);

    wchar_t searchPath[MAX_PATH + 2];
    int wideLen = 0;
    while (wideLen < widePath.len && wideLen < MAX_PATH - 2) {
        searchPath[wideLen] = widePath.s[wideLen];
        wideLen++;
    }
    if (wideLen > 0 && searchPath[wideLen - 1] != L'\\') {
        searchPath[wideLen++] = L'\\';
    }
    searchPath[wideLen++] = L'*';
    searchPath[wideLen] = 0;

    WIN32_FIND_DATAW fd;
    HANDLE hFind =
        FindFirstFileExW(searchPath, FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (hFind == INVALID_HANDLE_VALUE) {
        dv->err = GetLastErrorAsStr(arena);
        return;
    }
    do {
        if (shouldExit && AtomicBoolGet(shouldExit)) {
            FindClose(hFind);
            return;
        }

        if (wstr::Eq(WStr(fd.cFileName), wdot) || wstr::Eq(WStr(fd.cFileName), wdotdot)) {
            continue;
        }

        if (IsProtectedOSFile(fd.dwFileAttributes)) {
            continue;
        }

        Str utf8Name = ToUtf8Temp(WStr(fd.cFileName));

        DirEntry e = {};
        e.name = utf8Name;
        e.createTime = fd.ftCreationTime;
        e.modTime = fd.ftLastWriteTime;
        e.isLink = IsNameSurrogate(fd);
        // A junction is still a directory to show and to let the user step
        // into. It's only the walking that stops here, which the caller does by
        // looking at isLink.
        if (IsDirectoryAttr(fd.dwFileAttributes)) {
            e.size = 0;
            e.dv = kStillScanningDir;
        } else {
            e.size = ((u64)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            e.dv = nullptr;
            // App execution aliases are 0-byte reparse points; size the target.
            if (e.size == 0 && (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                fd.dwReserved0 == IO_REPARSE_TAG_APPEXECLINK) {
                e.size = SizeOfAppExecLinkTarget(path::JoinTemp(dv->fullDir, utf8Name));
            }
        }
        VecPush(GetTempArena(), temp, e);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    DirEntry* els = (DirEntry*)Alloc(arena, temp.len * sizeof(DirEntry));
    for (int i = 0; i < temp.len; i++) {
        els[i].name = str::Dup(arena, temp.els[i].name);
        els[i].size = temp.els[i].size;
        els[i].dv = temp.els[i].dv;
        els[i].createTime = temp.els[i].createTime;
        els[i].modTime = temp.els[i].modTime;
        els[i].isLink = temp.els[i].isLink;
    }
    dv->els = els;
    MemoryBarrier();
    dv->len = temp.len;
}

#endif

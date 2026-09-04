/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/StrQueue.h"

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

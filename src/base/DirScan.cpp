/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"

#include "base/DirScan.h"

// Find entry by name in a DirEntries
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

// Check if DirEntries is already in dirsToVisit list (must hold cs)
static bool IsDirInList(DirEntriesNode* list, DirEntries* dv) {
    while (list) {
        if (list->dv == dv) {
            return true;
        }
        list = list->next;
    }
    return false;
}

// Allocate a DirEntriesNode using given allocator
static DirEntriesNode* AllocDirEntriesNode(Arena* arena, DirEntries* dv, bool nonRecursive = false) {
    DirEntriesNode* node = (DirEntriesNode*)Alloc(arena, sizeof(DirEntriesNode));
    node->next = nullptr;
    node->dv = dv;
    node->nonRecursive = nonRecursive;
    return node;
}

// Create and initialize directory reader context
DirScanCtx* CreateDirScanCtx(Arena* arena, OnScannedDirCallback callback, void* userData) {
    DirScanCtx* ctx = new DirScanCtx();
    ctx->a = arena;
    ctx->onScannedDir = callback;
    ctx->userData = userData;
    ctx->threadExited = false;
    ctx->dirsToVisit = nullptr;
    ctx->shouldExit = 0;
    ctx->inFlightCount = 0;

    ThreadHandle hThread = StartThread(MkFunc0(DirScanThread, ctx), StrL("DirScanThread"));
    if (hThread) {
        SafeCloseThreadHandle(&hThread);
    } else {
        ctx->threadExited = true;
    }

    return ctx;
}

// Signal directory reader thread to exit and wait for it
void AskDirScanThreadToQuit(DirScanCtx* ctx) {
    if (!ctx) return;

    ctx->cs.Lock();
    AtomicBoolSet(&ctx->shouldExit, true);
    ctx->hasWork.WakeAll();
    while (!ctx->threadExited) {
        ctx->hasWork.Wait(&ctx->cs);
    }
    ctx->cs.Unlock();
    delete ctx;
}

// Request a directory scan - adds to FRONT of list (priority for user requests)
// Returns DirEntries* (either existing from queue or newly allocated)
DirEntries* RequestDirScan(DirScanCtx* ctx, Str dir) {
    ctx->cs.Lock();

    // Check if already in list
    DirEntries* dv = FindDirInList(ctx->dirsToVisit, dir);
    if (dv) {
        ctx->cs.Unlock();
        return dv;
    }

    // Allocate new DirEntries and add to queue
    // Use arena allocator for queue nodes (thread-safe)
    dv = AllocDirEntries(ctx->a, dir);
    DirEntriesNode* node = AllocDirEntriesNode(ctx->a, dv);
    node->next = ctx->dirsToVisit;
    ctx->dirsToVisit = node;

    ctx->hasWork.Wake();
    ctx->cs.Unlock();
    return dv;
}

// Queue a directory scan - adds to end of list (breadth-first scanning)
// If nonRecursive is true, subdirectories won't be queued for scanning
void QueueDirScan(DirScanCtx* ctx, DirEntries* dv, bool nonRecursive) {
    ctx->cs.Lock();

    // Skip if already in list
    if (IsDirInList(ctx->dirsToVisit, dv)) {
        ctx->cs.Unlock();
        return;
    }

    // Use arena allocator for queue nodes (thread-safe)
    DirEntriesNode* node = AllocDirEntriesNode(ctx->a, dv, nonRecursive);

    // Add to end of queue
    if (!ctx->dirsToVisit) {
        ctx->dirsToVisit = node;
    } else {
        DirEntriesNode* last = ctx->dirsToVisit;
        while (last->next) {
            last = last->next;
        }
        last->next = node;
    }

    ctx->hasWork.Wake();
    ctx->cs.Unlock();
}

// Request a refresh of a directory (non-recursive scan)
// Allocates a new DirEntries and queues it for scanning
void RequestDirRescan(DirScanCtx* ctx, DirEntries* dv) {
    DirEntries* newDv = AllocDirEntries(ctx->a, dv->fullDir);
    QueueDirScan(ctx, newDv, true);
}

void DirScanThread(DirScanCtx* ctx) {
    auto* tempAlloc = GetTempArena();

    while (true) {
        ctx->cs.Lock();
        while (!ctx->dirsToVisit && !AtomicBoolGet(&ctx->shouldExit)) {
            ctx->hasWork.Wait(&ctx->cs);
        }
        if (AtomicBoolGet(&ctx->shouldExit)) {
            ctx->cs.Unlock();
            break;
        }
        DirEntriesNode* node = ctx->dirsToVisit;
        ctx->dirsToVisit = node->next;
        ctx->inFlightCount++;
        DirEntries* dv = node->dv;
        bool nonRecursive = node->nonRecursive;
        ctx->cs.Unlock();

        ReadDirectory(ctx->a, dv, &ctx->shouldExit);

        if (AtomicBoolGet(&ctx->shouldExit)) {
            break;
        }

        if (!nonRecursive) {
            for (int i = 0; i < dv->len; i++) {
                if (AtomicBoolGet(&ctx->shouldExit)) {
                    break;
                }
                DirEntry* e = &dv->els[i];
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
            ctx->onScannedDir(dv, ctx->userData);
        }

        ctx->cs.Lock();
        ctx->inFlightCount--;
        bool allDone = !ctx->dirsToVisit && ctx->inFlightCount == 0;
        if (allDone) {
            ctx->hasWork.WakeAll();
        }
        ctx->cs.Unlock();

        tempAlloc->Reset();
    }

    ctx->cs.Lock();
    ctx->threadExited = true;
    ctx->hasWork.WakeAll();
    ctx->cs.Unlock();

    if (gTempArena) {
        ArenaDelete(gTempArena);
        gTempArena = nullptr;
    }
}

#include "common.h"

// Find entry by name in a DirEntries
DirEntry* FindEntryByName(DirEntries* dv, Str name) {
    if (!dv) return nullptr;
    for (int i = 0; i < dv->len; i++) {
        // Skip entries still being scanned
        if (dv->els[i].dv == kStillScanningDir) continue;
        if (StrEq(dv->els[i].name, name)) {
            return &dv->els[i];
        }
    }
    return nullptr;
}

// Allocate a DirEntries with fullDir set
DirEntries* AllocDirEntries(Arena* arena, Str fullDir) {
    DirEntries* dv = (DirEntries*)Alloc(arena, sizeof(DirEntries));
    *dv = {};
    dv->fullDir = StrDup(arena, fullDir);
    return dv;
}

// Temporary collection struct for building entry list
struct TempEntryVec {
    DirEntry* els;
    int len;
    int cap;
};

static const WStr wdot = WStrL(L".");
static const WStr wdotdot = WStrL(L"..");

// Read a directory into an existing DirEntries (dv->fullDir must be set)
// If shouldExit is not null and becomes true, returns early
void ReadDirectory(Arena* arena, DirEntries* dv, AtomicBool* shouldExit) {
    if (shouldExit && AtomicBoolGet(shouldExit)) {
        return;
    }

    // Collect entries using temp allocator
    TempEntryVec temp = {};

    // Add ".." entry
    DirEntry dotdot = {};
    dotdot.name = StrL("..");
    dotdot.size = 0;
    dotdot.dv = kStillScanningDir;
    VecPush(GetTempArena(), temp, dotdot);

    // Convert path to wide string for Win32 API
    WStr widePath = ToWStrTemp(dv->fullDir);

    // Build search pattern: path\*
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
        dv->err = GetLastErrorAsStr(arena).s;
        return;
    }
    do {
        // Check for early exit
        if (shouldExit && AtomicBoolGet(shouldExit)) {
            FindClose(hFind);
            return;
        }

        // Skip "." and ".."
        if (WStrEq(WStr(fd.cFileName), wdot) || WStrEq(WStr(fd.cFileName), wdotdot)) {
            continue;
        }

        // Convert filename to UTF-8
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nullptr, 0, nullptr, nullptr);
        char* utf8Name = (char*)AllocTemp(utf8Len);
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, utf8Name, utf8Len, nullptr, nullptr);
        int nameLen = utf8Len - 1; // Exclude null terminator

        DirEntry e = {};
        e.name = Str(utf8Name, nameLen); // Temp allocator, will be duped below
        e.createTime = fd.ftCreationTime;
        e.modTime = fd.ftLastWriteTime;
        // Check for real directories, not reparse points (symlinks, junctions)
        // Following links could cause infinite loops
        bool isRealDir =
            (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
        if (isRealDir) {
            e.size = 0;
            e.dv = kStillScanningDir;
        } else {
            e.size = ((u64)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            e.dv = nullptr;
        }
        VecPush(GetTempArena(), temp, e);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    // Allocate and fill array BEFORE setting len (for thread safety)
    // Main thread checks len first, so els must be valid before len is non-zero
    DirEntry* els = (DirEntry*)Alloc(arena, temp.len * sizeof(DirEntry));
    for (int i = 0; i < temp.len; i++) {
        els[i].name = StrDup(arena, temp.els[i].name);
        els[i].size = temp.els[i].size;
        els[i].dv = temp.els[i].dv;
        els[i].createTime = temp.els[i].createTime;
        els[i].modTime = temp.els[i].modTime;
    }
    dv->els = els;
    MemoryBarrier();
    dv->len = temp.len;
}

// Check if path is already in dirsToVisit list (must hold cs)
// Returns the DirEntries* if found, nullptr otherwise
static DirEntries* FindDirInList(DirEntriesNode* list, Str dir) {
    while (list) {
        if (StrEq(list->dv->fullDir, dir)) {
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
    DirScanCtx* ctx = (DirScanCtx*)malloc(sizeof(DirScanCtx));
    ctx->a = arena;
    ctx->onScannedDir = callback;
    ctx->userData = userData;
    InitializeCriticalSection(&ctx->cs);
    ctx->hSemaphore = CreateSemaphoreW(nullptr, 0, LONG_MAX, nullptr);
    ctx->hQueueEmptyEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ctx->hThreadExitedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ctx->dirsToVisit = nullptr;
    ctx->shouldExit = 0;
    ctx->inFlightCount = 0;

    HANDLE hThread = CreateThread(nullptr, 0, DirScanThread, ctx, 0, nullptr);
    if (hThread) {
        SetThreadDescription(hThread, L"DirScanThread");
        CloseHandle(hThread);
    }

    return ctx;
}

// Signal directory reader thread to exit and wait for it
void AskDirScanThreadToQuit(DirScanCtx* ctx) {
    if (!ctx) return;

    AtomicBoolSet(&ctx->shouldExit, true);
    ReleaseSemaphore(ctx->hSemaphore, 1, nullptr);

    WaitForSingleObject(ctx->hThreadExitedEvent, 5000);

    DeleteCriticalSection(&ctx->cs);
    CloseHandle(ctx->hSemaphore);
    CloseHandle(ctx->hQueueEmptyEvent);
    CloseHandle(ctx->hThreadExitedEvent);
    free(ctx);
}

// Request a directory scan - adds to FRONT of list (priority for user requests)
// Returns DirEntries* (either existing from queue or newly allocated)
DirEntries* RequestDirScan(DirScanCtx* ctx, Str dir) {
    EnterCriticalSection(&ctx->cs);

    // Check if already in list
    DirEntries* dv = FindDirInList(ctx->dirsToVisit, dir);
    if (dv) {
        LeaveCriticalSection(&ctx->cs);
        return dv;
    }

    // Allocate new DirEntries and add to queue
    // Use arena allocator for queue nodes (thread-safe)
    dv = AllocDirEntries(ctx->a, dir);
    DirEntriesNode* node = AllocDirEntriesNode(ctx->a, dv);
    node->next = ctx->dirsToVisit;
    ctx->dirsToVisit = node;

    LeaveCriticalSection(&ctx->cs);
    ReleaseSemaphore(ctx->hSemaphore, 1, nullptr); // Signal one worker
    return dv;
}

// Queue a directory scan - adds to end of list (breadth-first scanning)
// If nonRecursive is true, subdirectories won't be queued for scanning
void QueueDirScan(DirScanCtx* ctx, DirEntries* dv, bool nonRecursive) {
    EnterCriticalSection(&ctx->cs);

    // Skip if already in list
    if (IsDirInList(ctx->dirsToVisit, dv)) {
        LeaveCriticalSection(&ctx->cs);
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

    LeaveCriticalSection(&ctx->cs);
    ReleaseSemaphore(ctx->hSemaphore, 1, nullptr); // Signal one worker
}

// Request a refresh of a directory (non-recursive scan)
// Allocates a new DirEntries and queues it for scanning
void RequestDirRescan(DirScanCtx* ctx, DirEntries* dv) {
    DirEntries* newDv = AllocDirEntries(ctx->a, dv->fullDir);
    QueueDirScan(ctx, newDv, true);
}

// Background thread function to read directories
DWORD WINAPI DirScanThread(LPVOID param) {
    DirScanCtx* ctx = (DirScanCtx*)param;
    auto* tempAlloc = GetTempArena();

    while (true) {
        WaitForSingleObject(ctx->hSemaphore, INFINITE);

        if (AtomicBoolGet(&ctx->shouldExit)) {
            break;
        }

        EnterCriticalSection(&ctx->cs);
        DirEntriesNode* node = ctx->dirsToVisit;
        if (!node) {
            bool allDone = (ctx->inFlightCount == 0);
            LeaveCriticalSection(&ctx->cs);
            if (allDone) {
                SetEvent(ctx->hQueueEmptyEvent);
            }
            continue;
        }

        ctx->dirsToVisit = node->next;
        ctx->inFlightCount++;
        DirEntries* dv = node->dv;
        bool nonRecursive = node->nonRecursive;
        LeaveCriticalSection(&ctx->cs);

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
                if (e->dv == kStillScanningDir && !StrEq(e->name, StrL(".."))) {
                    Str subPath = PathJoinTemp(dv->fullDir, e->name);
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

        EnterCriticalSection(&ctx->cs);
        ctx->inFlightCount--;
        bool allDone = (ctx->dirsToVisit == nullptr && ctx->inFlightCount == 0);
        LeaveCriticalSection(&ctx->cs);
        if (allDone) {
            SetEvent(ctx->hQueueEmptyEvent);
        }

        tempAlloc->Reset();
    }

    SetEvent(ctx->hThreadExitedEvent);

    if (gTempArena) {
        ArenaDelete(gTempArena);
        gTempArena = nullptr;
    }
    return 0;
}

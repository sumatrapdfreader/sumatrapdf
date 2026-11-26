/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Content hashes of history files, to match them with SumatraPDF Web files by
// content rather than by name. A hash is stored in FileState.ContentSha1 as
// "<40 hex> <size> <mtime>": it holds while the file still has that size and
// modification time.
//
// This session's state per path is kept in memory: a hash checked against the
// file (or just computed), or why there is none. A file that isn't in the
// history yet (a download being opened) gets its hash written to FileState
// once it is.
//
// Hashing runs off the UI thread and must not get stuck on a share that went
// away:
//
//   UI: QueueContentHashes ──> queue ──> coordinator thread (no file I/O)
//                                             │ one I/O thread per file
//                                             ▼
//        stat; size no server file has? done; stored hash still valid? done;
//        else read and hash in 1 MiB pieces
//                                             │
//   no progress for kHashStallMs: cancel the I/O, abandon the thread, skip
//   that share for kShareRetryMs
//                                             │
//   UI <── result (uitask::Post) ── store in FileState, redraw
//
// Wasm doesn't hash: its documents live in /uploads under their server names.

#include "base/Base.h"
#include "base/File.h"
#include "base/Crypto.h"
#include "base/UITask.h"

#include "Settings.h"
#include "AppSettings.h"
#include "FileHistory.h"
#include "ContentHash.h"

#include "SumatraLog.h"

enum class HashState {
    // queued, no hash yet
    Pending,
    // h matches the file
    Hashed,
    // h matched the file; queued to check it still does
    Recheck,
    // its size is no server file's, so its content can't match one
    SizeNotOnServer,
    Missing,
    // its share stopped answering
    Unreachable,
    Failed,
};

static const char* kHashStateNames[] = {"pending", "hashed",      "recheck", "size-not-on-server",
                                        "missing", "unreachable", "failed"};

struct PathHash {
    Str path;
    HashState st = HashState::Pending;
    ContentHash h;
    u64 revision = 0;
    // a job for it is in the queue
    bool queued = false;
};

// UI thread only
static Vec<PathHash> gPathHashes;
static Func0 gOnHashed;

constexpr int kHashChunk = 1 << 20;
constexpr int kHashStallMs = 5 * 1000;
constexpr u64 kShareRetryMs = 10ULL * 60 * 1000;
constexpr int kHashPollMs = 100;

bool ParseSha1Hex(Str hex, u8 out[kSha1Len]) {
    if (len(hex) != kSha1Len * 2) {
        return false;
    }
    return str::HexToMem(hex, Str((char*)out, kSha1Len));
}

TempStr Sha1HexTemp(const u8 sha1[kSha1Len]) {
    return str::MemToHexTemp(Str((const char*)sha1, kSha1Len));
}

// "<40 hex> <size> <mtime>"
bool ParseContentHash(Str s, ContentHash& out) {
    StrVec parts;
    Split(&parts, s, StrL(" "), true);
    if (len(parts) != 3 || !ParseSha1Hex(parts[0], out.sha1)) {
        return false;
    }
    out.size = ParseInt64(parts[1]);
    out.mtime = (u64)ParseInt64(parts[2]);
    return out.size >= 0;
}

TempStr FormatContentHashTemp(const ContentHash& h) {
    return fmt("%s %lld %llu", Sha1HexTemp(h.sha1), h.size, h.mtime);
}

static PathHash* FindPathHash(Str path) {
    for (PathHash& p : gPathHashes) {
        if (str::EqI(p.path, path)) {
            return &p;
        }
    }
    return nullptr;
}

static PathHash* GetPathHash(Str path) {
    PathHash* p = FindPathHash(path);
    if (!p) {
        p = VecAppendBlanks(gPathHashes, 1);
        p->path = str::Dup(path);
    }
    return p;
}

// Records h as path's current content, in memory and in its history entry.
void RememberContentHash(Str path, const ContentHash& h) {
    PathHash* p = GetPathHash(path);
    p->revision++;
    p->st = HashState::Hashed;
    p->h = h;

    FileState* fs = FileHistoryFindByPath(path);
    if (!fs) {
        return;
    }
    TempStr s = FormatContentHashTemp(h);
    if (str::Eq(fs->contentSha1, s)) {
        return;
    }
    str::ReplaceWithCopy(&fs->contentSha1, s);
    ScheduleSaveSettings();
}

// path's hash, known this session to match its content (or, while it's being
// checked again, to have matched it)
bool KnownContentHash(Str path, ContentHash& out) {
    PathHash* p = FindPathHash(path);
    if (!p || (p->st != HashState::Hashed && p->st != HashState::Recheck)) {
        return false;
    }
    out = p->h;
    return true;
}

// Writes this session's hashes to history entries added since (a download
// joins the history once it opens).
void SaveKnownContentHashes() {
    for (PathHash& p : gPathHashes) {
        if (p.st != HashState::Hashed) {
            continue;
        }
        FileState* fs = FileHistoryFindByPath(p.path);
        if (fs && len(fs->contentSha1) == 0) {
            str::ReplaceWithCopy(&fs->contentSha1, FormatContentHashTemp(p.h));
            ScheduleSaveSettings();
        }
    }
}

//--- the worker: everything below runs off the UI thread unless noted

struct HashJob {
    Str path;
    Str shareKey;
    u64 revision = 0;
    ContentHash stored;
    bool hasStored = false;
    Vec<i64> sizes;
    int testDelayMs = 0;

    // owned by the coordinator and the I/O thread; the last to let go frees it
    AtomicInt refs = 1;
    // bumped by the I/O thread as it gets anywhere; the coordinator watches it
    AtomicInt progress = 0;
    AtomicInt abandoned = 0;

    // written by the I/O thread, read by the coordinator once it's done
    HashState st = HashState::Failed;
    ContentHash h;

    ~HashJob() {
        str::Free(path);
        str::Free(shareKey);
    }
};

struct HashResult {
    Str path;
    u64 revision = 0;
    HashState st = HashState::Failed;
    ContentHash h;
};

struct UnreachableShare {
    Str key;
    u64 untilMs = 0;
};

// guards everything below
static Mutex gHashMutex;
static Vec<HashJob*> gHashQueue;
static bool gCoordinatorRunning = false;
static Vec<UnreachableShare> gUnreachableShares;
static int gTestStallMs = 0;
static int gTestDelayMs = 0;
static Str gTestDelayMatch;

static void ReleaseJob(HashJob* job) {
    if (AtomicIntAdd(&job->refs, -1) == 0) {
        delete job;
    }
}

// what goes away with a share: "X:", "\\server\share" or, elsewhere, the first
// two parts ("/mnt/nas")
static TempStr ShareKeyTemp(Str path) {
#if OS_WIN
    if (str::StartsWith(path, StrL("\\\\"))) {
        int slashes = 0;
        for (int i = 2; i < len(path); i++) {
            if (path.s[i] == '\\' || path.s[i] == '/') {
                slashes++;
                if (slashes == 2) {
                    return str::DupTemp(Str(path.s, i));
                }
            }
        }
        return str::DupTemp(path);
    }
    if (len(path) >= 2 && path.s[1] == ':') {
        return str::DupTemp(Str(path.s, 2));
    }
    return {};
#else
    int slashes = 0;
    for (int i = 1; i < len(path); i++) {
        if (path.s[i] == '/') {
            slashes++;
            if (slashes == 2) {
                return str::DupTemp(Str(path.s, i));
            }
        }
    }
    return str::DupTemp(path);
#endif
}

// caller holds gHashMutex
static bool IsShareUnreachableLocked(Str key) {
    u64 now = GetTickCount64();
    for (UnreachableShare& u : gUnreachableShares) {
        if (str::EqI(u.key, key) && now < u.untilMs) {
            return true;
        }
    }
    return false;
}

static void MarkShareUnreachable(Str key) {
    ScopedMutex lock(&gHashMutex);
    for (UnreachableShare& u : gUnreachableShares) {
        if (str::EqI(u.key, key)) {
            u.untilMs = GetTickCount64() + kShareRetryMs;
            return;
        }
    }
    UnreachableShare* u = VecAppendBlanks(gUnreachableShares, 1);
    u->key = str::Dup(key);
    u->untilMs = GetTickCount64() + kShareRetryMs;
}

static void BumpProgress(HashJob* job) {
    AtomicIntAdd(&job->progress, 1);
}

// Hashes the file in pieces; false if it can't be read in full or the job
// was given up on.
static bool HashFileContent(HashJob* job, FILE* f, i64 size, u8 digest[kSha1Len]) {
    char* buf = AllocArray<char>(kHashChunk);
    Sha1Hasher* hasher = NewSha1Hasher();
    i64 total = 0;
    for (;;) {
        size_t n = fread(buf, 1, kHashChunk, f);
        BumpProgress(job);
        if (n == 0 || AtomicIntGet(&job->abandoned)) {
            break;
        }
        Sha1Update(hasher, Str(buf, (int)n));
        total += (i64)n;
    }
    bool ok = !ferror(f) && total == size && !AtomicIntGet(&job->abandoned);
    free(buf);
    Sha1Finish(hasher, digest);
    return ok;
}

static void HashIoThread(HashJob* job) {
    defer {
        ReleaseJob(job);
    };
    if (job->testDelayMs > 0) {
        // stands in for a share that stopped answering
        SleepInMs(job->testDelayMs);
    }
    BumpProgress(job);
    FILE* f = file::OpenFILE(job->path);
    BumpProgress(job);
    if (!f) {
        job->st = HashState::Missing;
        return;
    }
    AutoCall closeFile(fclose, f);
    file::ReadStamp stamp;
    if (!file::GetReadStamp(f, stamp)) {
        return;
    }
    BumpProgress(job);
    job->h.size = stamp.size;
    job->h.mtime = stamp.mtime;

    if (job->hasStored && job->stored.size == stamp.size && job->stored.mtime == stamp.mtime) {
        job->h = job->stored;
        job->st = HashState::Hashed;
        return;
    }
    bool sizeOnServer = false;
    for (i64 sz : job->sizes) {
        sizeOnServer |= sz == stamp.size;
    }
    if (!sizeOnServer) {
        job->st = HashState::SizeNotOnServer;
        return;
    }
    if (!HashFileContent(job, f, stamp.size, job->h.sha1)) {
        return;
    }
    file::ReadStamp after;
    if (file::GetReadStamp(f, after) && stamp == after && file::ReadStampMatchesPath(job->path, stamp)) {
        job->st = HashState::Hashed;
    }
}

static void PostResult(HashJob* job, HashState st);

// Waits for the I/O thread while it makes progress. Returns false if it
// stalled; the thread is then cancelled (Windows) and abandoned.
static bool WaitForIo(HashJob* job, ThreadHandle h, int stallMs) {
    int lastProgress = AtomicIntGet(&job->progress);
    u64 lastChangeMs = GetTickCount64();
    while (!WaitThread(h, kHashPollMs)) {
        int p = AtomicIntGet(&job->progress);
        if (p != lastProgress) {
            lastProgress = p;
            lastChangeMs = GetTickCount64();
            continue;
        }
        if (GetTickCount64() - lastChangeMs < (u64)stallMs) {
            continue;
        }
        AtomicIntSet(&job->abandoned, 1);
#if OS_WIN
        CancelSynchronousIo(h);
#endif
        return false;
    }
    return true;
}

static void HashCoordinator() {
    for (;;) {
        HashJob* job = nullptr;
        int stallMs = kHashStallMs;
        bool skip = false;
        {
            ScopedMutex lock(&gHashMutex);
            if (len(gHashQueue) == 0) {
                gCoordinatorRunning = false;
                return;
            }
            job = gHashQueue[0];
            VecRemoveAt(gHashQueue, 0);
            skip = len(job->shareKey) > 0 && IsShareUnreachableLocked(job->shareKey);
            if (gTestStallMs > 0) {
                stallMs = gTestStallMs;
            }
        }
        if (skip) {
            PostResult(job, HashState::Unreachable);
            ReleaseJob(job);
            continue;
        }

        AtomicIntAdd(&job->refs, 1);
        ThreadHandle h = StartThread(MkFunc0(HashIoThread, job), StrL("ContentHashIo"));
        bool done = WaitForIo(job, h, stallMs);
        SafeCloseThreadHandle(&h);
        if (!done) {
            logf("ContentHash: '%s' stalled, skipping its share for a while\n", job->path);
            if (len(job->shareKey) > 0) {
                MarkShareUnreachable(job->shareKey);
            }
            PostResult(job, HashState::Unreachable);
        } else {
            PostResult(job, job->st);
        }
        ReleaseJob(job);
    }
}

static void HashCoordinatorThread(void*) {
    HashCoordinator();
}

// UI thread: a result is in
static void HashResultDone(HashResult* res) {
    PathHash* path = GetPathHash(res->path);
    path->queued = false;
    if (path->revision == res->revision) {
        if (res->st == HashState::Hashed) {
            RememberContentHash(res->path, res->h);
        } else {
            path->st = res->st;
        }
    }
    str::Free(res->path);
    delete res;
    gOnHashed.Call();
}

static void PostResult(HashJob* job, HashState st) {
    auto* res = new HashResult();
    res->path = str::Dup(job->path);
    res->revision = job->revision;
    res->st = st;
    // after a stall the I/O thread may still be writing job->h
    if (st == HashState::Hashed) {
        res->h = job->h;
    }
    if (!uitask::Post(MkFunc0(HashResultDone, res), "ContentHashDone")) {
        str::Free(res->path);
        delete res;
    }
}

// UI thread. Queues the history files whose content isn't known yet, to be
// hashed if their size is one of sizes (the server's). onHashed is called on
// the UI thread after each result.
void QueueContentHashes(const Vec<i64>& sizes, const Func0& onHashed) {
    if (OS_WASM) {
        return;
    }
    gOnHashed = onHashed;
    Vec<FileState*> history;
    FileHistoryGetRecentlyOpenedOrder(history);

    ScopedMutex lock(&gHashMutex);
    for (FileState* fs : history) {
        if (!fs || len(fs->filePath) == 0) {
            continue;
        }
        // known, or being looked at: only a Recheck not queued yet goes again
        PathHash* p = FindPathHash(fs->filePath);
        if (p && (p->queued || p->st != HashState::Recheck)) {
            continue;
        }
        p = GetPathHash(fs->filePath);
        if (p->st != HashState::Recheck) {
            p->st = HashState::Pending;
        }
        p->queued = true;

        auto* job = new HashJob();
        job->path = str::Dup(fs->filePath);
        job->revision = p->revision;
        job->shareKey = str::Dup(ShareKeyTemp(fs->filePath));
        job->hasStored = len(fs->contentSha1) > 0 && ParseContentHash(fs->contentSha1, job->stored);
        for (i64 sz : sizes) {
            VecAppend(job->sizes, sz);
        }
        if (len(gTestDelayMatch) > 0 && str::Contains(fs->filePath, gTestDelayMatch)) {
            job->testDelayMs = gTestDelayMs;
        }
        VecAppend(gHashQueue, job);
    }
    if (len(gHashQueue) > 0 && !gCoordinatorRunning) {
        gCoordinatorRunning = true;
        RunAsync(MkFunc0<void>(HashCoordinatorThread, nullptr), StrL("ContentHash"));
    }
}

// UI thread: the server list changed. What we learned about each file is
// looked at again: a known hash is checked against the file (keeping it in
// the meantime), the rest start over.
void ForgetContentHashChecks() {
    for (int i = len(gPathHashes) - 1; i >= 0; i--) {
        PathHash& p = gPathHashes[i];
        if (p.queued) {
            continue;
        }
        if (p.st == HashState::Hashed || p.st == HashState::Recheck) {
            // the stored hash makes the check a stat
            p.st = HashState::Recheck;
            continue;
        }
        str::Free(p.path);
        VecRemoveAt(gPathHashes, i);
    }
}

// test hooks: stall timeout; a delay before touching files whose path contains
// match, standing in for a share that doesn't answer
void SetContentHashTestHooks(int stallMs, int delayMs, Str match) {
    ScopedMutex lock(&gHashMutex);
    gTestStallMs = stallMs;
    gTestDelayMs = delayMs;
    str::ReplaceWithCopy(&gTestDelayMatch, match);
}

// one "status=<state> path=<path>" line per file looked at
TempStr ContentHashStatusTemp() {
    str::Builder out;
    for (PathHash& p : gPathHashes) {
        out.Append(fmt("status=%s path=%s\n", Str(kHashStateNames[(int)p.st]), p.path));
    }
    return ToStrTemp(out);
}

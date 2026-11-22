/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Content hashes of history files, to match them with SumatraPDF Web files by
// content rather than by name. A hash is stored in FileState.ContentSha1 as
// "<40 hex> <size> <mtime>": it holds while the file still has that size and
// modification time.
//
// This session's hashes (checked against the file, or just computed) are kept
// in memory too: a file that isn't in the history yet (a download being opened)
// gets its hash written to FileState once it is.

#include "base/Base.h"

#include "Settings.h"
#include "AppSettings.h"
#include "FileHistory.h"
#include "ContentHash.h"

struct KnownHash {
    Str path;
    ContentHash h;
};

// UI thread only
static Vec<KnownHash> gKnownHashes;

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

// the hash saved in path's history entry, not checked against the file
bool StoredContentHash(Str path, ContentHash& out) {
    FileState* fs = FileHistoryFindByPath(path);
    return fs && len(fs->contentSha1) > 0 && ParseContentHash(fs->contentSha1, out);
}

static KnownHash* FindKnownHash(Str path) {
    for (KnownHash& k : gKnownHashes) {
        if (str::EqI(k.path, path)) {
            return &k;
        }
    }
    return nullptr;
}

// Records h as path's current content, in memory and in its history entry.
void RememberContentHash(Str path, const ContentHash& h) {
    KnownHash* k = FindKnownHash(path);
    if (!k) {
        k = VecAppendBlanks(gKnownHashes, 1);
        k->path = str::Dup(path);
    }
    k->h = h;

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

// path's hash, known this session to match its content
bool KnownContentHash(Str path, ContentHash& out) {
    KnownHash* k = FindKnownHash(path);
    if (!k) {
        return false;
    }
    out = k->h;
    return true;
}

// Writes this session's hashes to history entries added since (a download
// joins the history once it opens).
void SaveKnownContentHashes() {
    for (KnownHash& k : gKnownHashes) {
        FileState* fs = FileHistoryFindByPath(k.path);
        if (fs && len(fs->contentSha1) == 0) {
            str::ReplaceWithCopy(&fs->contentSha1, FormatContentHashTemp(k.h));
            ScheduleSaveSettings();
        }
    }
}

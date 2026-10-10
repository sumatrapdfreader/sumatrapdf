/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "Settings.h"
#include "ImageReader.h"
#include "AppTools.h"
#include "FileThumbnails.h"
#include "FileThumbnailsCommon.h"

TempStr GetThumbnailPathTemp(Str filePath) {
    // create a fingerprint of a (normalized) path for the file name
    // I'd have liked to also include the file's last modification time
    // in the fingerprint (much quicker than hashing the entire file's
    // content), but that's too expensive for files on slow drives
    u8 digest[16]{};
    // Null/empty paths show up when a FileState has no path (corrupt settings
    // or a race while closing); never hash an empty path (crash e.g. 35043).
    if (len(filePath) == 0) {
        return {};
    }
    TempStr path = str::DupTemp(filePath);
    if (path::HasVariableDriveLetter(path)) {
        // ignore the drive letter, if it might change
        path.s[0] = '?';
    }
    CalcMD5Digest(path, digest);
    TempStr fingerPrint = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));

    TempStr thumbsDir = GetThumbnailCacheDirTemp();
    if (len(thumbsDir) == 0) {
        return {};
    }

    TempStr res = path::JoinTemp(thumbsDir, str::JoinTemp(fingerPrint, StrL(".png")));
    return res;
}

TempStr GetThumbnailCacheDirTemp() {
    TempStr thumbsDir = GetPathInAppDataDirTemp(StrL("sumatrapdfcache"));
    return thumbsDir;
}

// Empty rather than remove: SaveThumbnail runs on the UI thread and re-creates
// this directory, so deleting it out from under a save in flight made
// dir::CreateAll fail (crash 2026-08-05/8c3bf9e1f000001).
void EmptyThumbnailCacheDirectory() {
    TempStr thumbsDir = GetThumbnailCacheDirTemp();
    dir::Empty(thumbsDir);
}

void DeleteThumbnailForFile(Str filePath) {
    TempStr thumbPath = GetThumbnailPathTemp(filePath);
    if (len(thumbPath) == 0) {
        return;
    }
    bool ok = file::Delete(thumbPath);
    const auto* status = ok ? "ok" : "failed";
    logf("DeleteThumbnailForFile: file::Remove('%s') %s\n", thumbPath, Str(status));
}

bool PixmapIsEmpty(const Pixmap* px) {
    return !px || px->width <= 0 || px->height <= 0 || !px->data;
}

Pixmap* LoadThumbnail(FileState* fs) {
    if (!fs || len(fs->filePath) == 0) {
        return nullptr;
    }
    if (fs->thumbnail) {
        return fs->thumbnail;
    }
    TempStr bmpPath = GetThumbnailPathTemp(fs->filePath);
    if (len(bmpPath) == 0) {
        return nullptr;
    }

    Str data = file::ReadFile(bmpPath);
    if (len(data) == 0) {
        return nullptr;
    }
    // 24bpp: thumbnails stay in memory for every Home entry shown
    Pixmap* px = PixmapToBgr(PixmapFromData(data));
    str::Free(data);
    if (PixmapIsEmpty(px)) {
        FreePixmap(px);
        return nullptr;
    }

    fs->thumbnail = px;
    return fs->thumbnail;
}

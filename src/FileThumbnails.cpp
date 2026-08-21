/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/GdiPlusUtil.h"
#include "base/Pixmap.h"

#include "Settings.h"
#include "ImageReader.h"

#include "AppTools.h"
#include "FileThumbnails.h"

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
    if (!thumbsDir) {
        return {};
    }

    TempStr res = path::JoinTemp(thumbsDir, str::JoinTemp(fingerPrint, StrL(".png")));
    return res;
}

TempStr GetThumbnailCacheDirTemp() {
    TempStr thumbsDir = GetPathInAppDataDirTemp("sumatrapdfcache");
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
    if (!thumbPath) {
        return;
    }
    bool ok = file::Delete(thumbPath);
    const auto* status = ok ? "ok" : "failed";
    logf("DeleteThumbnailForFile: file::Remove('%s') %s\n", thumbPath, Str(status));
}

static bool PixmapIsEmpty(const Pixmap* px) {
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
    if (!bmpPath) {
        return nullptr;
    }

    Str data = file::ReadFile(bmpPath);
    if (!data) {
        return nullptr;
    }
    Pixmap* px = PixmapFromData(data);
    str::Free(data);
    if (PixmapIsEmpty(px)) {
        FreePixmap(px);
        return nullptr;
    }

    fs->thumbnail = px;
    return fs->thumbnail;
}

bool HasThumbnail(FileState* fs) {
    if (!fs || len(fs->filePath) == 0) {
        return false;
    }
    // Prefer the in-memory thumbnail; only hit disk when missing.
    if (!fs->thumbnail && !LoadThumbnail(fs)) {
        return false;
    }

    TempStr bmpPath = GetThumbnailPathTemp(fs->filePath);
    if (!bmpPath) {
        return fs->thumbnail != nullptr;
    }
    FILETIME bmpTime = file::GetModificationTime(bmpPath);
    FILETIME fileTime = file::GetModificationTime(fs->filePath);
    // delete the thumbnail if the file is newer than the thumbnail
    if (FileTimeDiffInSecs(fileTime, bmpTime) > 0) {
        FreePixmap(fs->thumbnail);
        fs->thumbnail = nullptr;
    }

    return fs->thumbnail != nullptr;
}

// takes ownership of bmp
void SetThumbnail(FileState* fs, Pixmap* bmp) {
    ReportIf(bmp && PixmapIsEmpty(bmp));
    if (!fs || len(fs->filePath) == 0 || PixmapIsEmpty(bmp)) {
        FreePixmap(bmp);
        return;
    }
    FreePixmap(fs->thumbnail);
    fs->thumbnail = bmp;
    SaveThumbnail(fs);
}

void SaveThumbnail(FileState* fs) {
    if (!fs || !fs->thumbnail || len(fs->filePath) == 0) {
        return;
    }

    TempStr thumbnailPath = GetThumbnailPathTemp(fs->filePath);
    if (!thumbnailPath) {
        return;
    }
    // failing to create the cache dir is environmental (antivirus, ACLs, disk full,
    // a file occupying the name) rather than a bug, so log and skip the thumbnail -
    // same as the other dir::CreateForFile callers. err == 0 means the create
    // reported success but the directory was gone when we looked.
    int err = 0;
    if (!dir::CreateForFile(thumbnailPath, &err)) {
        logf("SaveThumbnail: dir::CreateForFile('%s') failed, err=%d, file path: '%s'\n", thumbnailPath, err,
             fs->filePath);
        return;
    }
    ReportIfFast(!str::EndsWithI(thumbnailPath, StrL(".png")));

    Pixmap* thumbnail = fs->thumbnail;
    if (PixmapIsEmpty(thumbnail)) {
        return;
    }
    // the engine renders pages to an 8-bit palette DIB when it can, and those
    // pixels can only be read through the platform bitmap
    Pixmap* converted = nullptr;
    defer {
        FreePixmap(converted);
    };
    if (thumbnail->format == PixmapFormat::Native) {
        converted = PixmapCopyAs32bppDIB(thumbnail);
        if (!converted) {
            logf("SaveThumbnail: PixmapCopyAs32bppDIB() failed for '%s'\n", fs->filePath);
            return;
        }
        thumbnail = converted;
    }
    // Wrap (don't take ownership) so we can encode the in-memory thumbnail as PNG.
    Gdiplus::Bitmap* bmp = WrapPixmapGdiplus(thumbnail);
    if (!bmp) {
        return;
    }
    CLSID tmpClsid = GetGdiPlusEncoderClsid(L"image/png");
    WCHAR* pathW = CWStrTemp(thumbnailPath);
    Gdiplus::Status st = bmp->Save(pathW, &tmpClsid, nullptr);
    delete bmp;
    if (st != Gdiplus::Ok) {
        // gdi+ creates the file before it encodes, so a failure leaves a 0-byte
        // png behind, which reads back as a blank thumbnail (issue #5932)
        logf("SaveThumbnail: Save('%s') failed with %d\n", thumbnailPath, (int)st);
        file::Delete(thumbnailPath);
    }
}

void RemoveThumbnail(FileState* fs) {
    if (!fs || len(fs->filePath) == 0) {
        return;
    }
    if (!HasThumbnail(fs)) {
        return;
    }

    TempStr bmpPath = GetThumbnailPathTemp(fs->filePath);
    if (bmpPath) {
        file::Delete(bmpPath);
    }
    FreePixmap(fs->thumbnail);
    fs->thumbnail = nullptr;
}

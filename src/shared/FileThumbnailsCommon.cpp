/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "Settings.h"
#include "AppSettings.h"
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

#if defined(SUMATRA_NG)
constexpr bool kThumbnailsFullPageDefault = true;
#else
constexpr bool kThumbnailsFullPageDefault = false;
#endif

// ThumbnailsType: full (the whole first page) or legacy (the top of it);
// anything else is the app's default
bool ThumbnailsAreFullPage() {
    Str type = gSettings ? gSettings->thumbnailsType : Str();
    if (str::EqI(type, StrL("full"))) {
        return true;
    }
    if (str::EqI(type, StrL("legacy"))) {
        return false;
    }
    return kThumbnailsFullPageDefault;
}

Size ThumbnailFrameSize() {
    if (ThumbnailsAreFullPage()) {
        return Size(kThumbnailFullDx, kThumbnailFullDy);
    }
    return Size(kThumbnailLegacyDx, kThumbnailLegacyDy);
}

// the size a thumbnail is drawn at: scaled to fit the frame, keeping its aspect
Size ThumbnailDrawSize(Size thumb, Size frame) {
    if (thumb.dx <= 0 || thumb.dy <= 0) {
        return frame;
    }
    int dx = frame.dx;
    int dy = thumb.dy * dx / thumb.dx;
    if (dy > frame.dy) {
        dy = frame.dy;
        dx = thumb.dx * dy / thumb.dy;
    }
    return Size(dx, dy);
}

// a legacy thumbnail is as wide as its frame, a full one fits inside its frame
bool ThumbnailSizeMatchesType(Size sz) {
    if (sz.dx <= 0 || sz.dy <= 0) {
        return false;
    }
    if (ThumbnailsAreFullPage()) {
        return sz.dx <= kThumbnailFullDx && sz.dy <= kThumbnailFullDy;
    }
    return sz.dx == kThumbnailLegacyDx && sz.dy <= kThumbnailLegacyDy;
}

// width and height from the IHDR chunk that follows the 8-byte signature;
// empty if the data isn't a PNG
Size PngSizeFromData(Str data) {
    constexpr int kIhdrEnd = 24;
    static const u8 kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (len(data) < kIhdrEnd || memcmp(data.s, kSignature, sizeof(kSignature)) != 0) {
        return {};
    }
    auto be32 = [](const u8* p) -> u32 {
        return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
    };
    const u8* s = (const u8*)data.s;
    u32 width = be32(s + 16);
    u32 height = be32(s + 20);
    // PNG dimensions must be positive and fit within a signed int.
    if (width == 0 || height == 0 || width > 0x7fffffffU || height > 0x7fffffffU) {
        return {};
    }
    return Size((int)width, (int)height);
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
    // made for the other ThumbnailsType: as good as missing, re-created the
    // next time the document is opened
    if (!ThumbnailSizeMatchesType(Size(px->width, px->height))) {
        FreePixmap(px);
        file::Delete(bmpPath);
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
    if (len(bmpPath) == 0) {
        return fs->thumbnail != nullptr;
    }
    FILETIME bmpTime = file::GetModificationTime(bmpPath);
    FILETIME fileTime = file::GetModificationTime(fs->filePath);
    // delete the thumbnail if the file is newer than the thumbnail
    if (FileTimeDiffInSecs(fileTime, bmpTime) > 0) {
        FreePixmap(fs->thumbnail);
        fs->thumbnail = nullptr;
        HomePageThumbnailChanged(fs);
    }

    return fs->thumbnail != nullptr;
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
    HomePageThumbnailChanged(fs);
}

// takes ownership of bmp
void SetThumbnail(FileState* fs, Pixmap* bmp) {
    ReportIf(bmp && PixmapIsEmpty(bmp));
    if (!fs || len(fs->filePath) == 0 || PixmapIsEmpty(bmp)) {
        FreePixmap(bmp);
        return;
    }
    FreePixmap(fs->thumbnail);
    fs->thumbnail = PixmapToBgr(bmp);
    SaveThumbnail(fs);
    HomePageThumbnailChanged(fs);
}

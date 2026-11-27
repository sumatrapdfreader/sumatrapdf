/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GdiPlusUtil.h"
#include "base/Pixmap.h"
#include "base/Win.h"

#include "Settings.h"

#include "FileThumbnails.h"
#include "FileThumbnailsCommon.h"

void SaveThumbnail(FileState* fs) {
    if (!fs || !fs->thumbnail || len(fs->filePath) == 0) {
        return;
    }

    TempStr thumbnailPath = GetThumbnailPathTemp(fs->filePath);
    if (len(thumbnailPath) == 0) {
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
    ReportIf(!str::EndsWithI(thumbnailPath, StrL(".png")));

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
    // encode next to the final name and rename: a reader never sees a partial
    // png, and a failed encode leaves no 0-byte png that reads back blank
    // (issue #5932)
    TempStr tmpPath = str::JoinTemp(thumbnailPath, StrL(".tmp"));
    WCHAR* pathW = CWStrTemp(tmpPath);
    Gdiplus::Status st = bmp->Save(pathW, &tmpClsid, nullptr);
    delete bmp;
    if (st != Gdiplus::Ok) {
        logf("SaveThumbnail: Save('%s') failed with %d\n", tmpPath, (int)st);
        file::Delete(tmpPath);
        return;
    }
    if (!file::RenameReplace(thumbnailPath, tmpPath)) {
        logf("SaveThumbnail: RenameReplace('%s', '%s') failed\n", thumbnailPath, tmpPath);
        file::Delete(tmpPath);
    }
}

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/Pixmap.h"

#include "Settings.h"
#include "ImageReader.h"

#include "AppTools.h"
#include "gui/WasmBridge.h"
#include "FileThumbnails.h"
#include "FileThumbnailsCommon.h"

extern "C" {
#include "mupdf/fitz.h"
}

#include "SumatraLog.h"

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

// ng: orig encodes the thumbnail with GDI+ (WrapPixmapGdiplus + Save). mupdf's
// PNG writer is already linked and portable, so the cache file has the same
// name and format on every platform.
static bool SavePixmapAsPng(Pixmap* pixmap, Str path) {
    Pixmap* owned = nullptr;
    if (pixmap->format == PixmapFormat::Native) {
#if OS_WIN
        owned = PixmapCopyAs32bppDIB(pixmap);
#endif
        if (!owned) {
            logf("SavePixmapAsPng: can't read a native pixmap\n");
            return false;
        }
        pixmap = owned;
    }
    int w = pixmap->width;
    int h = pixmap->height;
    int bpp = PixmapBytesPerPixel(pixmap->format);
    bool isRgb = pixmap->format == PixmapFormat::RGBA8;

    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        FreePixmap(owned);
        return false;
    }
    bool ok = false;
    fz_pixmap* fzPix = nullptr;
    fz_try(ctx) {
        fzPix = fz_new_pixmap(ctx, fz_device_rgb(ctx), w, h, nullptr, 0);
        for (int y = 0; y < h; y++) {
            const u8* src = pixmap->data + ((size_t)y * pixmap->stride);
            u8* dst = fzPix->samples + ((size_t)y * fzPix->stride);
            for (int x = 0; x < w; x++) {
                dst[0] = isRgb ? src[0] : src[2];
                dst[1] = src[1];
                dst[2] = isRgb ? src[2] : src[0];
                src += bpp;
                dst += 3;
            }
        }
        fz_save_pixmap_as_png(ctx, fzPix, CStrTemp(path));
        ok = true;
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, fzPix);
    }
    fz_catch(ctx) {
        logf("SavePixmapAsPng: %s\n", Str(fz_caught_message(ctx)));
    }
    fz_drop_context(ctx);
    FreePixmap(owned);
    return ok;
}

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
    if (!SavePixmapAsPng(thumbnail, thumbnailPath)) {
        // a failed encode can leave a truncated png behind, which reads back as
        // a blank thumbnail (issue #5932)
        logf("SaveThumbnail: Save('%s') failed\n", thumbnailPath);
        file::Delete(thumbnailPath);
        return;
    }
    logf("SaveThumbnail: '%s' -> '%s'\n", fs->filePath, thumbnailPath);
#if OS_WASM
    // the cache lives under the app data directory, which is stored in OPFS
    WasmPersistSettings();
#endif
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

void CreateChmThumbnail(Str, const Size&, const OnBitmapRendered* saveThumbnail) {
    delete saveThumbnail;
}

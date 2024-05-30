/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CryptoUtil.h"
#include "utils/FileUtil.h"
#include "utils/DirIter.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WinUtil.h"

#include "Settings.h"
#include "DocController.h"
#include "FzImgReader.h"
#include "FileHistory.h"

#include "AppTools.h"
#include "FileThumbnails.h"

#include "utils/Log.h"

char* GetThumbnailPathTemp(const char* filePath) {
    // create a fingerprint of a (normalized) path for the file name
    // I'd have liked to also include the file's last modification time
    // in the fingerprint (much quicker than hashing the entire file's
    // content), but that's too expensive for files on slow drives
    u8 digest[16]{};
    // TODO: why is this happening? Seen in crash reports e.g. 35043
    if (!filePath) {
        return nullptr;
    }
    TempStr path = str::DupTemp(filePath);
    if (path::HasVariableDriveLetter(path)) {
        // ignore the drive letter, if it might change
        path[0] = '?';
    }
    CalcMD5Digest((u8*)path, str::Leni(path), digest);
    AutoFreeStr fingerPrint = str::MemToHex(digest, dimof(digest));

    TempStr thumbsDir = GetThumbnailCacheDirTemp();
    if (!thumbsDir) {
        return nullptr;
    }

    TempStr res = path::JoinTemp(thumbsDir, str::JoinTemp(fingerPrint, ".png"));
    return res;
}

TempStr GetThumbnailCacheDirTemp() {
    TempStr thumbsDir = GetPathInAppDataDirTemp("sumatrapdfcache");
    return thumbsDir;
}

void DeleteThumbnailCacheDirectory() {
    TempStr thumbsDir = GetThumbnailCacheDirTemp();
    dir::RemoveAll(thumbsDir);
}

void DeleteThumbnailForFile(const char* filePath) {
    TempStr thumbPath = GetThumbnailPathTemp(filePath);
    bool ok = file::Delete(thumbPath);
    auto status = ok ? "ok" : "failed";
    logf("DeleteThumbnailForFile: file::Remove('%s') %s\n", thumbPath, status);
}

bool LoadThumbnail(FileState* ds) {
    delete ds->thumbnail;
    ds->thumbnail = nullptr;

    char* bmpPath = GetThumbnailPathTemp(ds->filePath);
    if (!bmpPath) {
        return false;
    }

    RenderedBitmap* bmp = LoadRenderedBitmap(bmpPath);
    if (!bmp || bmp->GetSize().IsEmpty()) {
        delete bmp;
        return false;
    }

    ds->thumbnail = bmp;
    return true;
}

bool HasThumbnail(FileState* ds) {
    if (!ds->thumbnail && !LoadThumbnail(ds)) {
        return false;
    }

    char* bmpPath = GetThumbnailPathTemp(ds->filePath);
    if (!bmpPath) {
        return true;
    }
    FILETIME bmpTime = file::GetModificationTime(bmpPath);
    FILETIME fileTime = file::GetModificationTime(ds->filePath);
    // delete the thumbnail if the file is newer than the thumbnail
    if (FileTimeDiffInSecs(fileTime, bmpTime) > 0) {
        delete ds->thumbnail;
        ds->thumbnail = nullptr;
    }

    return ds->thumbnail != nullptr;
}

// takes ownership of bmp
void SetThumbnail(FileState* ds, RenderedBitmap* bmp) {
    ReportIf(bmp && bmp->GetSize().IsEmpty());
    if (!ds || !bmp || bmp->GetSize().IsEmpty()) {
        delete bmp;
        return;
    }
    delete ds->thumbnail;
    ds->thumbnail = bmp;
    SaveThumbnail(ds);
}

void SaveThumbnail(FileState* ds) {
    if (!ds->thumbnail) {
        return;
    }

    TempStr thumbnailPath = GetThumbnailPathTemp(ds->filePath);
    if (!thumbnailPath) {
        return;
    }
    if (!dir::CreateForFile(thumbnailPath)) {
        logf("SaveThumbnail: dir::CreateForFile('%s') failed, file path: '%s'\n", thumbnailPath, ds->filePath);
        ReportIfQuick(true);
    }
    ReportIfQuick(!str::EndsWithI(thumbnailPath, ".png"));

    Gdiplus::Bitmap bmp(ds->thumbnail->GetBitmap(), nullptr);
    CLSID tmpClsid = GetEncoderClsid(L"image/png");
    TempWStr pathW = ToWStrTemp(thumbnailPath);
    bmp.Save(pathW, &tmpClsid, nullptr);
}

void RemoveThumbnail(FileState* ds) {
    if (!HasThumbnail(ds)) {
        return;
    }

    char* bmpPath = GetThumbnailPathTemp(ds->filePath);
    if (bmpPath) {
        file::Delete(bmpPath);
    }
    delete ds->thumbnail;
    ds->thumbnail = nullptr;
}

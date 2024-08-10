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

RenderedBitmap* LoadThumbnail(FileState* fs) {
    if (fs->thumbnail) {
        return fs->thumbnail;
    }
    TempStr bmpPath = GetThumbnailPathTemp(fs->filePath);
    if (!bmpPath) {
        return nullptr;
    }

    RenderedBitmap* bmp = LoadRenderedBitmap(bmpPath);
    if (!bmp || bmp->GetSize().IsEmpty()) {
        delete bmp;
        return nullptr;
    }

    fs->thumbnail = bmp;
    return fs->thumbnail;
}

bool HasThumbnail(FileState* fs) {
    // TODO: optimize, LoadThumbnail() is probably not necessary
    if (!fs->thumbnail && !LoadThumbnail(fs)) {
        return false;
    }

    TempStr bmpPath = GetThumbnailPathTemp(fs->filePath);
    if (!bmpPath) {
        return true;
    }
    FILETIME bmpTime = file::GetModificationTime(bmpPath);
    FILETIME fileTime = file::GetModificationTime(fs->filePath);
    // delete the thumbnail if the file is newer than the thumbnail
    if (FileTimeDiffInSecs(fileTime, bmpTime) > 0) {
        delete fs->thumbnail;
        fs->thumbnail = nullptr;
    }

    return fs->thumbnail != nullptr;
}

// takes ownership of bmp
void SetThumbnail(FileState* fs, RenderedBitmap* bmp) {
    ReportIf(bmp && bmp->GetSize().IsEmpty());
    if (!fs || !bmp || bmp->GetSize().IsEmpty()) {
        delete bmp;
        return;
    }
    delete fs->thumbnail;
    fs->thumbnail = bmp;
    SaveThumbnail(fs);
}

void SaveThumbnail(FileState* fs) {
    if (!fs->thumbnail) {
        return;
    }

    TempStr thumbnailPath = GetThumbnailPathTemp(fs->filePath);
    if (!thumbnailPath) {
        return;
    }
    if (!dir::CreateForFile(thumbnailPath)) {
        logf("SaveThumbnail: dir::CreateForFile('%s') failed, file path: '%s'\n", thumbnailPath, fs->filePath);
        ReportIfQuick(true);
    }
    ReportIfQuick(!str::EndsWithI(thumbnailPath, ".png"));

    Gdiplus::Bitmap bmp(fs->thumbnail->GetBitmap(), nullptr);
    CLSID tmpClsid = GetEncoderClsid(L"image/png");
    TempWStr pathW = ToWStrTemp(thumbnailPath);
    bmp.Save(pathW, &tmpClsid, nullptr);
}

void RemoveThumbnail(FileState* fs) {
    if (!HasThumbnail(fs)) {
        return;
    }

    char* bmpPath = GetThumbnailPathTemp(fs->filePath);
    if (bmpPath) {
        file::Delete(bmpPath);
    }
    delete fs->thumbnail;
    fs->thumbnail = nullptr;
}

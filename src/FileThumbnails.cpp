/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CryptoUtil.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "FzImgReader.h"
#include "SettingsStructs.h"
#include "FileHistory.h"

#include "AppTools.h"
#include "FileThumbnails.h"

constexpr const char* kThumbnailsDirName = "sumatrapdfcache";
constexpr const char* kPngExt = "*.png";

static char* GetThumbnailPathTemp(const char* filePath) {
    // create a fingerprint of a (normalized) path for the file name
    // I'd have liked to also include the file's last modification time
    // in the fingerprint (much quicker than hashing the entire file's
    // content), but that's too expensive for files on slow drives
    u8 digest[16]{0};
    // TODO: why is this happening? Seen in crash reports e.g. 35043
    if (!filePath) {
        return nullptr;
    }
    if (path::HasVariableDriveLetter(filePath)) {
        // ignore the drive letter, if it might change
        char* tmp = (char*)filePath;
        tmp[0] = '?';
    }
    CalcMD5Digest((u8*)filePath, str::Len(filePath), digest);
    AutoFree fingerPrint(_MemToHex(&digest));

    char* thumbsPath = AppGenDataFilenameTemp(kThumbnailsDirName);
    if (!thumbsPath) {
        return nullptr;
    }

    char* tmp = str::Format(R"(%s\%s.png)", thumbsPath, fingerPrint.Get());
    char* res = str::DupTemp(tmp);
    str::Free(tmp);
    return res;
}

// removes thumbnails that don't belong to any frequently used item in file history
void CleanUpThumbnailCache(const FileHistory& fileHistory) {
    char* thumbsPath = AppGenDataFilenameTemp(kThumbnailsDirName);
    AutoFreeStr pattern(path::Join(thumbsPath, kPngExt, nullptr));

    WStrVec files;
    WIN32_FIND_DATA fdata;

    WCHAR* pw = ToWstrTemp(pattern);
    HANDLE hfind = FindFirstFileW(pw, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return;
    }
    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files.Append(str::Dup(fdata.cFileName));
        }
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    // remove files that should not be deleted
    Vec<FileState*> list;
    fileHistory.GetFrequencyOrder(list);
    int n{0};
    for (auto& fs : list) {
        if (n++ > kFileHistoryMaxFrequent * 2) {
            break;
        }
        char* bmpPath = GetThumbnailPathTemp(fs->filePath);
        if (!bmpPath) {
            continue;
        }
        WCHAR* fileName = ToWstrTemp(path::GetBaseNameTemp(bmpPath));
        int idx = files.Find(fileName);
        if (idx < 0) {
            continue;
        }
        WCHAR* path = files.PopAt(idx);
        str::Free(path);
    }

    for (auto& pathW : files) {
        char* pathA = ToUtf8Temp(pathW);
        char* bmpPath = path::Join(thumbsPath, pathA, nullptr);
        file::Delete(bmpPath);
        str::Free(bmpPath);
    }
}

bool LoadThumbnail(FileState& ds) {
    delete ds.thumbnail;
    ds.thumbnail = nullptr;

    char* bmpPath = GetThumbnailPathTemp(ds.filePath);
    if (!bmpPath) {
        return false;
    }

    RenderedBitmap* bmp = LoadRenderedBitmap(bmpPath);
    if (!bmp || bmp->Size().IsEmpty()) {
        delete bmp;
        return false;
    }

    ds.thumbnail = bmp;
    return true;
}

bool HasThumbnail(FileState& ds) {
    if (!ds.thumbnail && !LoadThumbnail(ds)) {
        return false;
    }

    char* bmpPath = GetThumbnailPathTemp(ds.filePath);
    if (!bmpPath) {
        return true;
    }
    FILETIME bmpTime = file::GetModificationTime(bmpPath);
    FILETIME fileTime = file::GetModificationTime(ds.filePath);
    // delete the thumbnail if the file is newer than the thumbnail
    if (FileTimeDiffInSecs(fileTime, bmpTime) > 0) {
        delete ds.thumbnail;
        ds.thumbnail = nullptr;
    }

    return ds.thumbnail != nullptr;
}

void SetThumbnail(FileState* ds, RenderedBitmap* bmp) {
    CrashIf(bmp && bmp->Size().IsEmpty());
    if (!ds || !bmp || bmp->Size().IsEmpty()) {
        delete bmp;
        return;
    }
    delete ds->thumbnail;
    ds->thumbnail = bmp;
    SaveThumbnail(*ds);
}

void SaveThumbnail(FileState& ds) {
    if (!ds.thumbnail) {
        return;
    }

    WCHAR* fp = ToWstrTemp(ds.filePath);
    char* bmpPathA = GetThumbnailPathTemp(ds.filePath);
    if (!bmpPathA) {
        return;
    }
    WCHAR* bmpPath = ToWstrTemp(bmpPathA);
    AutoFreeWstr thumbsPath(path::GetDir(bmpPath));
    if (dir::Create(thumbsPath)) {
        CrashIf(!str::EndsWithI(bmpPath, L".png"));
        Gdiplus::Bitmap bmp(ds.thumbnail->GetBitmap(), nullptr);
        CLSID tmpClsid = GetEncoderClsid(L"image/png");
        bmp.Save(bmpPath, &tmpClsid, nullptr);
    }
}

void RemoveThumbnail(FileState& ds) {
    if (!HasThumbnail(ds)) {
        return;
    }

    char* bmpPath = GetThumbnailPathTemp(ds.filePath);
    if (bmpPath) {
        file::Delete(bmpPath);
    }
    delete ds.thumbnail;
    ds.thumbnail = nullptr;
}

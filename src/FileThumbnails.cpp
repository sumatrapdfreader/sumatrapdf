/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "CryptoUtil.h"
#include "FileUtil.h"
#include "GdiPlusUtil.h"
#include "WinUtil.h"
// layout controllers
#include "BaseEngine.h"
#include "SettingsStructs.h"
#include "FileHistory.h"
// ui
#include "AppTools.h"
#include "FileThumbnails.h"

#define THUMBNAILS_DIR_NAME L"sumatrapdfcache"

// TODO: create in TEMP directory instead?
static WCHAR *GetThumbnailPath(const WCHAR *filePath)
{
    // create a fingerprint of a (normalized) path for the file name
    // I'd have liked to also include the file's last modification time
    // in the fingerprint (much quicker than hashing the entire file's
    // content), but that's too expensive for files on slow drives
    unsigned char digest[16];
    // TODO: why is this happening? Seen in crash reports e.g. 35043
    if (!filePath)
        return nullptr;
    ScopedMem<char> pathU(str::conv::ToUtf8(filePath));
    if (!pathU)
        return nullptr;
    if (path::HasVariableDriveLetter(filePath))
        pathU[0] = '?'; // ignore the drive letter, if it might change
    CalcMD5Digest((unsigned char *)pathU.Get(), str::Len(pathU), digest);
    ScopedMem<char> fingerPrint(_MemToHex(&digest));

    AutoFreeW thumbsPath(AppGenDataFilename(THUMBNAILS_DIR_NAME));
    if (!thumbsPath)
        return nullptr;
    AutoFreeW fname(str::conv::FromAnsi(fingerPrint));

    return str::Format(L"%s\\%s.png", thumbsPath.Get(), fname.Get());
}

// removes thumbnails that don't belong to any frequently used item in file history
void CleanUpThumbnailCache(FileHistory& fileHistory)
{
    AutoFreeW thumbsPath(AppGenDataFilename(THUMBNAILS_DIR_NAME));
    if (!thumbsPath)
        return;
    AutoFreeW pattern(path::Join(thumbsPath, L"*.png"));

    WStrVec files;
    WIN32_FIND_DATA fdata;

    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return;
    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            files.Append(str::Dup(fdata.cFileName));
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    Vec<DisplayState *> list;
    fileHistory.GetFrequencyOrder(list);
    for (size_t i = 0; i < list.Count() && i < FILE_HISTORY_MAX_FREQUENT * 2; i++) {
        AutoFreeW bmpPath(GetThumbnailPath(list.At(i)->filePath));
        if (!bmpPath)
            continue;
        int idx = files.Find(path::GetBaseName(bmpPath));
        if (idx != -1) {
            CrashIf(idx < 0 || files.Count() <= (size_t)idx);
            free(files.PopAt(idx));
        }
    }

    for (size_t i = 0; i < files.Count(); i++) {
        AutoFreeW bmpPath(path::Join(thumbsPath, files.At(i)));
        file::Delete(bmpPath);
    }
}

static RenderedBitmap *LoadRenderedBitmap(const WCHAR *filePath)
{
    size_t len;
    ScopedMem<char> data(file::ReadAll(filePath, &len));
    if (!data)
        return nullptr;
    Bitmap *bmp = BitmapFromData(data, len);
    if (!bmp)
        return nullptr;

    HBITMAP hbmp;
    RenderedBitmap *rendered = nullptr;
    if (bmp->GetHBITMAP((ARGB)Color::White, &hbmp) == Ok)
        rendered = new RenderedBitmap(hbmp, SizeI(bmp->GetWidth(), bmp->GetHeight()));
    delete bmp;

    return rendered;
}

bool LoadThumbnail(DisplayState& ds)
{
    delete ds.thumbnail;
    ds.thumbnail = nullptr;

    AutoFreeW bmpPath(GetThumbnailPath(ds.filePath));
    if (!bmpPath)
        return false;

    RenderedBitmap *bmp = LoadRenderedBitmap(bmpPath);
    if (!bmp || bmp->Size().IsEmpty()) {
        delete bmp;
        return false;
    }

    ds.thumbnail = bmp;
    return true;
}

bool HasThumbnail(DisplayState& ds)
{
    if (!ds.thumbnail && !LoadThumbnail(ds))
        return false;

    AutoFreeW bmpPath(GetThumbnailPath(ds.filePath));
    if (!bmpPath)
        return true;
    FILETIME bmpTime = file::GetModificationTime(bmpPath);
    FILETIME fileTime = file::GetModificationTime(ds.filePath);
    // delete the thumbnail if the file is newer than the thumbnail
    if (FileTimeDiffInSecs(fileTime, bmpTime) > 0) {
        delete ds.thumbnail;
        ds.thumbnail = nullptr;
    }

    return ds.thumbnail != nullptr;
}

void SetThumbnail(DisplayState *ds, RenderedBitmap *bmp)
{
    CrashIf(bmp && bmp->Size().IsEmpty());
    if (!ds || !bmp || bmp->Size().IsEmpty()) {
        delete bmp;
        return;
    }
    delete ds->thumbnail;
    ds->thumbnail = bmp;
    SaveThumbnail(*ds);
}

void SaveThumbnail(DisplayState& ds)
{
    if (!ds.thumbnail)
        return;

    AutoFreeW bmpPath(GetThumbnailPath(ds.filePath));
    if (!bmpPath)
        return;
    AutoFreeW thumbsPath(path::GetDir(bmpPath));
    if (dir::Create(thumbsPath)) {
        CrashIf(!str::EndsWithI(bmpPath, L".png"));
        Bitmap bmp(ds.thumbnail->GetBitmap(), nullptr);
        CLSID tmpClsid = GetEncoderClsid(L"image/png");
        bmp.Save(bmpPath.Get(), &tmpClsid, nullptr);
    }
}

void RemoveThumbnail(DisplayState& ds)
{
    if (!HasThumbnail(ds))
        return;

    AutoFreeW bmpPath(GetThumbnailPath(ds.filePath));
    if (bmpPath)
        file::Delete(bmpPath);
    delete ds.thumbnail;
    ds.thumbnail = nullptr;
}

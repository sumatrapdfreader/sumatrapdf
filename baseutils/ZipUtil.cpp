/* Copyright 2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: Simplified BSD (see ./COPYING) */

#include "ZipUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

static void UnzipFileIfStartsWith(unzFile& uf,  FileToUnzip *files, const TCHAR *dir)
{
    char fileName[MAX_PATH];
    TCHAR *fileName2 = NULL;
    TCHAR *filePath = NULL;
    unz_file_info64 finfo;
    unsigned int readBytes;

    int err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
        return;

    int fileIdx = -1;
    for (int i=0; files[i].fileNamePrefix; i++) {
        if (Str::StartsWithI(fileName, files[i].fileNamePrefix)) {
            fileIdx = i;
            break;
        }
    }

    if (-1 == fileIdx)
        return;

    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return;

    unsigned len = (unsigned)finfo.uncompressed_size;
    ZPOS64_T len2 = len;
    if (len2 != finfo.uncompressed_size) // overflow check
        goto Exit;

    void *data = malloc(len);
    if (!data)
        goto Exit;

    readBytes = unzReadCurrentFile(uf, data, len);
    if (readBytes != len)
        goto Exit;

    if (files[fileIdx].unzippedName) {
        filePath = Path::Join(dir, files[fileIdx].unzippedName);
    } else {
        fileName2 = Str::Conv::FromAnsi(fileName); // Note: maybe FromUtf8?
        filePath = Path::Join(dir, fileName2);
    }
    if (!File::WriteAll(filePath, data, len))
        goto Exit;

    files[fileIdx].wasUnzipped = true;

Exit:
    unzCloseCurrentFile(uf); // ignoring error code
    free(fileName2);
    free(filePath);
    free(data);
}

static void MarkAllUnzipped(FileToUnzip *files)
{
    for (int i=0; files[i].fileNamePrefix; i++) {
        files[i].wasUnzipped = false;
    }
}

static bool WereAllUnzipped(FileToUnzip *files)
{
    for (int i=0; files[i].fileNamePrefix; i++) {
        if (!files[i].wasUnzipped) {
            return false;
        }
    }
    return true;
}

bool UnzipFilesStartingWith(const TCHAR *zipFile, FileToUnzip *files, const TCHAR *dir)
{
    MarkAllUnzipped(files);

    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    unzFile uf = unzOpen2_64(zipFile, &ffunc);
    if (!uf)
        return false;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK) {
        unzClose(uf);
        return false;
    }
    unzGoToFirstFile(uf);

    for (int n = 0; n < ginfo.number_entry; n++) {
        UnzipFileIfStartsWith(uf, files, dir);
        err = unzGoToNextFile(uf);
        if (err != UNZ_OK)
            break;
    }

    unzClose(uf);

    return WereAllUnzipped(files);
}


/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "RarUtil.h"

// undefine for using UnRAR instead (for now)
#define USE_LIBARCHIVE

#ifdef USE_LIBARCHIVE
extern "C" {
#define LIBARCHIVE_STATIC
#include "../../ext/libarchive/archive.h"
#include "../../ext/libarchive/archive_entry.h"
}
#else
#include "../../ext/unrar/dll.hpp"
#endif

RarFile::RarFile(const WCHAR *path) : path(str::Dup(path))
{
    ExtractFilenames();
}

size_t RarFile::GetFileCount() const
{
    return filenames.Count();
}

const WCHAR *RarFile::GetFileName(size_t fileindex)
{
    if (fileindex >= filenames.Count())
        return NULL;
    return filenames.At(fileindex);
}

size_t RarFile::GetFileIndex(const WCHAR *fileName)
{
    return filenames.FindI(fileName);
}

char *RarFile::GetFileDataByName(const WCHAR *fileName, size_t *len)
{
    return GetFileDataByIdx(GetFileIndex(fileName), len);
}

// TODO: implement the following without the need for reparsing
// most of the file for every call to RarFile::GetFileDataByIdx

#ifdef USE_LIBARCHIVE

void RarFile::ExtractFilenames()
{
    if (!path)
        return;

    struct archive *a = archive_read_new();
    if (!a)
        return;
    archive_read_support_format_rar(a);
    int r = archive_read_open_filename_w(a, path, 10240);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return;
    }

    for (;;) {
        struct archive_entry *entry;
        r = archive_read_next_header(a, &entry);
        if (ARCHIVE_EOF == r || r != ARCHIVE_OK)
            break;
        const WCHAR *name = archive_entry_pathname_w(entry);
        filenames.Append(str::Dup(name));
    }

    archive_read_close(a);
    archive_read_free(a);
}

char *RarFile::GetFileDataByIdx(size_t fileindex, size_t *len)
{
    if (!path || fileindex > filenames.Count())
        return NULL;

    struct archive *a = archive_read_new();
    if (!a)
        return NULL;
    archive_read_support_format_rar(a);
    int r = archive_read_open_filename_w(a, path, 10240);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return NULL;
    }

    for (size_t i = 0; i < fileindex; i++) {
        struct archive_entry *entry;
        archive_read_next_header(a, &entry);
    }

    str::Str<char> data;
    struct archive_entry *entry;
    r = archive_read_next_header(a, &entry);
    if (ARCHIVE_OK == r) {
        for (;;) {
            const void *buffer;
            size_t size;
            int64_t offset;
            int r = archive_read_data_block(a, &buffer, &size, &offset);
            if (ARCHIVE_EOF == r || r != ARCHIVE_OK)
                break;
            data.Append((char *)buffer, size);
        }
        if (ARCHIVE_EOF == r)
            r = ARCHIVE_OK;
        // zero-terminate for convenience
        data.Append("\0\0", 2);
    }

    archive_read_close(a);
    archive_read_free(a);

    if (r != ARCHIVE_OK)
        return NULL;
    if (len)
        *len = data.Size() - 2;
    return data.StealData();
}

#else

void RarFile::ExtractFilenames()
{
    if (!path)
        return;

    RAROpenArchiveDataEx  arcData = { 0 };
    arcData.ArcNameW = path;
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0)
        return;

    for (;;) {
        RARHeaderDataEx rarHeader;
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;
        filenames.Append(str::Dup(rarHeader.FileNameW));
        RARProcessFile(hArc, RAR_SKIP, NULL, NULL);
    }

    RARCloseArchive(hArc);
}

static int CALLBACK unrarCallback(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed)
{
    if (UCM_PROCESSDATA != msg || !userData)
        return -1;
    str::Str<char> *data = (str::Str<char> *)userData;
    data->Append((char *)rarBuffer, bytesProcessed);
    return 1;
}

char *RarFile::GetFileDataByIdx(size_t fileindex, size_t *len)
{
    if (!path || fileindex > filenames.Count())
        return NULL;

    RAROpenArchiveDataEx  arcData = { 0 };
    arcData.ArcNameW = path;
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0)
        return NULL;

    for (size_t i = 0; i < fileindex; i++) {
        RARHeaderDataEx rarHeader;
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;
        RARProcessFile(hArc, RAR_SKIP, NULL, NULL);
    }

    str::Str<char> data;
    RARHeaderDataEx rarHeader;
    int res = RARReadHeaderEx(hArc, &rarHeader);
    if (0 == res) {
        if (rarHeader.UnpSizeHigh != 0 || rarHeader.UnpSize == 0 || (int)rarHeader.UnpSize < 0) {
            res = 1;
        }
        else {
            RARSetCallback(hArc, unrarCallback, (LPARAM)&data);
            res = RARProcessFile(hArc, RAR_TEST, NULL, NULL);
            if (rarHeader.UnpSize != data.Size()) {
                res = 1;
            }
        }
        // zero-terminate for convenience
        data.Append("\0\0", 2);
    }

    RARCloseArchive(hArc);

    if (0 != res)
        return NULL;
    if (len)
        *len = data.Size() - 2;
    return data.StealData();
}

#endif

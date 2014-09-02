/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "7zUtil.h"

extern "C" {
#include <unarr.h>
#include <zlib.h>
}

// TODO: merge with ZipFile and RarFile?
// (unarr provides a unified interface)

_7zFile::_7zFile(const WCHAR *path) : ar(NULL)
{
    data = ar_open_file_w(path);
    if (data)
        ar = ar_open_7z_archive(data);
    ExtractFilenames();
}

_7zFile::_7zFile(IStream *stream) : ar(NULL)
{
    data = ar_open_istream(stream);
    if (data)
        ar = ar_open_7z_archive(data);
    ExtractFilenames();
}

_7zFile::~_7zFile()
{
    ar_close_archive(ar);
    ar_close(data);
}

void _7zFile::ExtractFilenames()
{
    if (!ar)
        return;
    while (ar_parse_entry(ar)) {
        const char *nameUtf8 = ar_entry_get_name(ar);
        filenames.Append(str::conv::FromUtf8(nameUtf8));
        filepos.Append(ar_entry_get_offset(ar));
    }
}

size_t _7zFile::GetFileIndex(const WCHAR *fileName)
{
    return filenames.FindI(fileName);
}

size_t _7zFile::GetFileCount() const
{
    CrashIf(filenames.Count() != filepos.Count());
    return filenames.Count();
}

const WCHAR *_7zFile::GetFileName(size_t fileindex)
{
    if (fileindex >= filenames.Count())
        return NULL;
    return filenames.At(fileindex);
}

char *_7zFile::GetFileDataByName(const WCHAR *fileName, size_t *len)
{
    return GetFileDataByIdx(GetFileIndex(fileName), len);
}

char *_7zFile::GetFileDataByIdx(size_t fileindex, size_t *len)
{
    if (!ar)
        return NULL;
    if (fileindex >= filenames.Count())
        return NULL;

    if (!ar_parse_entry_at(ar, filepos.At(fileindex)))
        return NULL;

    size_t size = ar_entry_get_size(ar);
    if (size > SIZE_MAX - 2)
        return NULL;
    char *data = (char *)malloc(size + 2);
    if (!data)
        return NULL;
    if (!ar_entry_uncompress(ar, data, size)) {
        free(data);
        return NULL;
    }
    // zero-terminate for convenience
    data[size] = data[size + 1] = '\0';

    if (len)
        *len = size;
    return data;
}

FILETIME _7zFile::GetFileTime(const WCHAR *fileName)
{
    return GetFileTime(GetFileIndex(fileName));
}

FILETIME _7zFile::GetFileTime(size_t fileindex)
{
    FILETIME ft = { (DWORD)-1, (DWORD)-1 };
    if (ar && fileindex < filepos.Count() && ar_parse_entry_at(ar, filepos.At(fileindex))) {
        time64_t filetime = ar_entry_get_filetime(ar);
        LocalFileTimeToFileTime((FILETIME *)&filetime, &ft);
    }
    return ft;
}

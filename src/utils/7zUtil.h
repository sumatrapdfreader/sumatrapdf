/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef _7zUtil_h
#define _7zUtil

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}

class _7zFile {
    WStrList filenames;
    Vec<int64_t> filepos;

    ar_stream *data;
    ar_archive *ar;

    void ExtractFilenames();

public:
    explicit _7zFile(const WCHAR *path);
    explicit _7zFile(IStream *stream);
    ~_7zFile();

    size_t GetFileCount() const;
    // the result is owned by _7zFile
    const WCHAR *GetFileName(size_t fileindex);
    // reverts GetFileName
    size_t GetFileIndex(const WCHAR *filename);

    // caller must free() the result
    char *GetFileDataByName(const WCHAR *filename, size_t *len=NULL);
    char *GetFileDataByIdx(size_t fileindex, size_t *len=NULL);

    FILETIME GetFileTime(const WCHAR *filename);
    FILETIME GetFileTime(size_t fileindex);
};

#endif

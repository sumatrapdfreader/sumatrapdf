/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef RarUtil_h
#define RarUtil_h

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}
class UnRarDll;

class RarFile {
    ScopedMem<WCHAR> path;
    WStrList filenames;

    ar_stream *data;
    ar_archive *ar;
    Vec<int64_t> filepos;
    UnRarDll *fallback;

    void ExtractFilenames();
    char *GetFileFromCallback(size_t fileindex, size_t *len=NULL);

public:
    explicit RarFile(const WCHAR *path);
    explicit RarFile(IStream *stream);
    ~RarFile();

    size_t GetFileCount() const;
    // the result is owned by RarFile
    const WCHAR *GetFileName(size_t fileindex);
    // reverts GetFileName
    size_t GetFileIndex(const WCHAR *filename);

    // caller must free() the result
    char *GetFileDataByName(const WCHAR *filename, size_t *len=NULL);
    char *GetFileDataByIdx(size_t fileindex, size_t *len=NULL);
};

#endif

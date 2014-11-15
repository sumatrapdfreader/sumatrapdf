/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}

class ArchFile {
protected:
    WStrList filenames;
    Vec<int64_t> filepos;

    ar_stream *data;
    ar_archive *ar;

    // call with fileindex = -1 for filename extraction using the fallback
    virtual char *GetFileFromFallback(size_t fileindex, size_t *len=NULL) { return NULL; }

public:
    ArchFile(ar_stream *data, ar_archive *(* openFormat)(ar_stream *));
    virtual ~ArchFile();

    size_t GetFileCount() const;
    // the result is owned by ArchFile
    const WCHAR *GetFileName(size_t fileindex);
    // reverts GetFileName
    size_t GetFileIndex(const WCHAR *filename);

    // caller must free() the result
    char *GetFileDataByName(const WCHAR *filename, size_t *len=NULL);
    char *GetFileDataByIdx(size_t fileindex, size_t *len=NULL);

    FILETIME GetFileTime(const WCHAR *filename);
    FILETIME GetFileTime(size_t fileindex);

    // caller must free() the result
    char *GetComment(size_t *len=NULL);
};

class ZipFile : public ArchFile {
public:
    explicit ZipFile(const WCHAR *path, bool deflatedOnly=false);
    explicit ZipFile(IStream *stream, bool deflatedOnly=false);
};

class _7zFile : public ArchFile {
public:
    explicit _7zFile(const WCHAR *path);
    explicit _7zFile(IStream *stream);
};

class TarFile : public ArchFile {
public:
    explicit TarFile(const WCHAR *path);
    explicit TarFile(IStream *stream);
};

class UnRarDll;

class RarFile : public ArchFile {
    ScopedMem<WCHAR> path;
    UnRarDll *fallback;

    void ExtractFilenamesWithFallback();
    virtual char *GetFileFromFallback(size_t fileindex, size_t *len=NULL);

public:
    explicit RarFile(const WCHAR *path);
    explicit RarFile(IStream *stream);
    virtual ~RarFile();
};

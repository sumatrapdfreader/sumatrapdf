/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}

// TODO: unused - remove?
class ZipFileAlloc {
    Allocator *allocator;
    WStrList filenames;
    Vec<int64_t> filepos;

    ar_stream *data;
    ar_archive *ar_;

    void ExtractFilenames();

public:
    explicit ZipFileAlloc(const WCHAR *path, bool deflatedOnly=false, Allocator *allocator=nullptr);
    explicit ZipFileAlloc(IStream *stream, bool deflatedOnly=false, Allocator *allocator=nullptr);
    ~ZipFileAlloc();

    size_t GetFileCount() const;
    // the result is owned by ZipFileAlloc
    const WCHAR *GetFileName(size_t fileindex);
    // reverts GetFileName
    size_t GetFileIndex(const WCHAR *filename);

    // caller must free() the result (or rather Allocator::Free it)
    char *GetFileDataByName(const WCHAR *filename, size_t *len=nullptr);
    char *GetFileDataByIdx(size_t fileindex, size_t *len=nullptr);

    FILETIME GetFileTime(const WCHAR *filename);
    FILETIME GetFileTime(size_t fileindex);

    // caller must free() the result (or rather Allocator::Free it)
    char *GetComment(size_t *len=nullptr);

    bool UnzipFile(const WCHAR *filename, const WCHAR *dir, const WCHAR *unzippedName=nullptr);
};

class ZipCreator {
    ISequentialStream *stream;
    str::Str<char> centraldir;
    size_t bytesWritten;
    size_t fileCount;

    bool WriteData(const void *data, size_t size);
    bool AddFileData(const char *nameUtf8, const void *data, size_t size, uint32_t dosdate=0);

public:
    ZipCreator(const WCHAR *zipFilePath);
    ZipCreator(ISequentialStream *stream);
    ~ZipCreator();

    bool AddFile(const WCHAR *filePath, const WCHAR *nameInZip=nullptr);
    bool AddFileFromDir(const WCHAR *filePath, const WCHAR *dir);
    bool AddDir(const WCHAR *dirPath, bool recursive=false);
    bool Finish();
};

IStream *OpenDirAsZipStream(const WCHAR *dirPath, bool recursive=false);

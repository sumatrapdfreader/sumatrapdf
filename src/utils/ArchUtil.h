/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}

struct ArchFileInfo {
    size_t fileId;
    std::string_view name;
    WCHAR* nameW;      // TODO: remove this one
    FILETIME fileTime; // TOOD: make portable
    size_t fileSizeUncompressed;

    // internal use
    int64_t filePos;
};

class ArchFile {
  protected:
    // used for allocating strings that are referenced by ArchFileInfo::name
    PoolAllocator allocator_;
    std::vector<ArchFileInfo> fileInfos_;

    WStrList fileNames_;

    ar_stream* data_ = nullptr;
    ar_archive* ar_ = nullptr;

    // call with fileindex = -1 for filename extraction using the fallback
    virtual char* GetFileFromFallback(size_t fileIndex, size_t* len = nullptr) {
        UNUSED(fileIndex);
        UNUSED(len);
        return nullptr;
    }

  public:
    ArchFile(ar_stream* data, ar_archive* (*openFormat)(ar_stream*));
    virtual ~ArchFile();

    // the result is owned by ArchFile
    std::vector<ArchFileInfo>* GetFileInfos();

    size_t GetFileCount() const;
    // the result is owned by ArchFile
    const WCHAR* GetFileName(size_t fileId);
    // reverts GetFileName
    size_t GetFileIndex(const WCHAR* filename);

    // caller must free() the result
    char* GetFileDataByName(const WCHAR* filename, size_t* len = nullptr);
    char* GetFileDataByIdx(size_t fileindex, size_t* len = nullptr);

    FILETIME GetFileTime(const WCHAR* filename);
    FILETIME GetFileTime(size_t fileindex);

    // caller must free() the result
    char* GetComment(size_t* len = nullptr);
};

ArchFile* CreateZipArchive(const WCHAR* path, bool deflatedOnly);
ArchFile* CreateZipArchive(IStream* stream, bool deflatedOnly);

ArchFile* Create7zArchive(const WCHAR* path);
ArchFile* Create7zArchive(IStream* stream);

ArchFile* CreateTarArchive(const WCHAR* path);
ArchFile* CreateTarArchive(IStream* stream);

class UnRarDll;

class RarFile : public ArchFile {
    AutoFreeW path;
    UnRarDll* fallback;

    void ExtractFilenamesWithFallback();
    virtual char* GetFileFromFallback(size_t fileindex, size_t* len = nullptr);

  public:
    explicit RarFile(const WCHAR* path);
    explicit RarFile(IStream* stream);
    virtual ~RarFile();
};

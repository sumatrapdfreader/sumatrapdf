/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}

struct ArchFileInfo {
    size_t fileId;
    std::string_view name;
    int64_t fileTime; // this is typedef'ed as time64_t in unrar.h
    size_t fileSizeUncompressed;

    // internal use
    int64_t filePos;

#if OS(WIN)
    FILETIME GetWinFileTime() const;
#endif
};

class ArchFile {
  protected:
    // used for allocating strings that are referenced by ArchFileInfo::name
    PoolAllocator allocator_;
    std::vector<ArchFileInfo*> fileInfos_;

    ar_stream* data_ = nullptr;
    ar_archive* ar_ = nullptr;

    // call with fileindex = -1 for filename extraction using the fallback
    virtual char* GetFileFromFallback(size_t fileId, size_t* len = nullptr) {
        UNUSED(fileId);
        UNUSED(len);
        return nullptr;
    }

  public:
    ArchFile(ar_stream* data, ar_archive* (*openFormat)(ar_stream*));
    virtual ~ArchFile();

    std::vector<ArchFileInfo*> const& GetFileInfos();

    size_t GetFileId(const char* fileName);

// caller must free() the result
#if OS(WIN)
    char* GetFileDataByName(const WCHAR* filename, size_t* len = nullptr);
#endif
    char* GetFileDataByName(const char* filename, size_t* len = nullptr);
    char* GetFileDataById(size_t fileId, size_t* len = nullptr);

    // caller must free() the result
    char* GetComment(size_t* len = nullptr);
};

ArchFile* OpenZipArchive(const char* path, bool deflatedOnly);
ArchFile* Open7zArchive(const char* path);
ArchFile* OpenTarArchive(const char* path);

// TODO: remove those
#if OS(WIN)
ArchFile* OpenZipArchive(const WCHAR* path, bool deflatedOnly);
ArchFile* Open7zArchive(const WCHAR* path);
ArchFile* OpenTarArchive(const WCHAR* path);
#endif

#if OS(WIN)
ArchFile* OpenZipArchive(IStream* stream, bool deflatedOnly);
ArchFile* Open7zArchive(IStream* stream);
ArchFile* OpenTarArchive(IStream* stream);
#endif

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

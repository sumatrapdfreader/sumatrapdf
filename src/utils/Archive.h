/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}

typedef ar_archive* (*archive_opener_t)(ar_stream*);

class Archive {
  public:
    enum class Format { Zip, Rar, SevenZip, Tar };
    struct FileInfo {
        size_t fileId;
        std::string_view name;
        int64_t fileTime; // this is typedef'ed as time64_t in unrar.h
        size_t fileSizeUncompressed;

        // internal use
        int64_t filePos;

#if OS_WIN
        FILETIME GetWinFileTime() const;
#endif
    };

    Archive(ar_stream* data, archive_opener_t opener, Format format);
    virtual ~Archive();

    Format format;

    std::vector<FileInfo*> const& GetFileInfos();

    size_t GetFileId(const char* fileName);

// caller must free() the result
#if OS_WIN
    char* GetFileDataByName(const WCHAR* filename, size_t* len = nullptr);
#endif
    char* GetFileDataByName(const char* filename, size_t* len = nullptr);
    char* GetFileDataById(size_t fileId, size_t* len = nullptr);

    // caller must free() the result
    char* GetComment(size_t* len = nullptr);

  protected:
    // used for allocating strings that are referenced by ArchFileInfo::name
    PoolAllocator allocator_;
    std::vector<FileInfo*> fileInfos_;

    ar_stream* data_ = nullptr;
    ar_archive* ar_ = nullptr;

    // call with fileindex = -1 for filename extraction using the fallback
    virtual char* GetFileFromFallback(size_t fileId, size_t* len = nullptr) {
        UNUSED(fileId);
        UNUSED(len);
        return nullptr;
    }
};

Archive* OpenZipArchive(const char* path, bool deflatedOnly);
Archive* Open7zArchive(const char* path);
Archive* OpenTarArchive(const char* path);

// TODO: remove those
#if OS_WIN
Archive* OpenZipArchive(const WCHAR* path, bool deflatedOnly);
Archive* Open7zArchive(const WCHAR* path);
Archive* OpenTarArchive(const WCHAR* path);
#endif

#if OS_WIN
Archive* OpenZipArchive(IStream* stream, bool deflatedOnly);
Archive* Open7zArchive(IStream* stream);
Archive* OpenTarArchive(IStream* stream);
#endif

class UnRarDll;

class RarFile : public Archive {
    AutoFreeW path;
    UnRarDll* fallback;

    void ExtractFilenamesWithFallback();
    virtual char* GetFileFromFallback(size_t fileindex, size_t* len = nullptr);

  public:
    explicit RarFile(const WCHAR* path);
    explicit RarFile(IStream* stream);
    virtual ~RarFile();
};

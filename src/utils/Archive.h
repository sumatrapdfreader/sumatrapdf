/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}

typedef ar_archive* (*archive_opener_t)(ar_stream*);

class MultiFormatArchive {
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

    MultiFormatArchive(archive_opener_t opener, Format format);
    ~MultiFormatArchive();

    Format format;

    bool Open(ar_stream* data, const char* archivePath);

    Vec<FileInfo*> const& GetFileInfos();

    size_t GetFileId(const char* fileName);

#if OS_WIN
    std::string_view GetFileDataByName(const WCHAR* filename);
#endif
    std::string_view GetFileDataByName(const char* filename);
    std::string_view GetFileDataById(size_t fileId);

    std::string_view GetComment();

  protected:
    // used for allocating strings that are referenced by ArchFileInfo::name
    PoolAllocator allocator_;
    Vec<FileInfo*> fileInfos_;

    archive_opener_t opener_ = nullptr;
    ar_stream* data_ = nullptr;
    ar_archive* ar_ = nullptr;

    // only set when we loaded file infos using unrar.dll fallback
    const char* rarFilePath_ = nullptr;

    bool OpenUnrarFallback(const char* rarPathUtf);
    std::string_view GetFileDataByIdUnarrDll(size_t fileId);
    bool LoadedUsingUnrarDll() const {
        return rarFilePath_ != nullptr;
    }
};

MultiFormatArchive* OpenZipArchive(const char* path, bool deflatedOnly);
MultiFormatArchive* Open7zArchive(const char* path);
MultiFormatArchive* OpenTarArchive(const char* path);

// TODO: remove those
#if OS_WIN
MultiFormatArchive* OpenZipArchive(const WCHAR* path, bool deflatedOnly);
MultiFormatArchive* Open7zArchive(const WCHAR* path);
MultiFormatArchive* OpenTarArchive(const WCHAR* path);
MultiFormatArchive* OpenRarArchive(const WCHAR* path);
#endif

#if OS_WIN
MultiFormatArchive* OpenZipArchive(IStream* stream, bool deflatedOnly);
MultiFormatArchive* Open7zArchive(IStream* stream);
MultiFormatArchive* OpenTarArchive(IStream* stream);
MultiFormatArchive* OpenRarArchive(IStream* stream);
#endif

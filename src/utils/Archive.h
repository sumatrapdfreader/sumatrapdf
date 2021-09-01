/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
        size_t fileId{0};
        std::string_view name{};
        i64 fileTime{0}; // this is typedef'ed as time64_t in unrar.h
        size_t fileSizeUncompressed{0};

        // internal use
        i64 filePos{0};
        char* data{nullptr};

        [[nodiscard]] FILETIME GetWinFileTime() const;
    };

    MultiFormatArchive(archive_opener_t opener, Format format);
    ~MultiFormatArchive();

    Format format;

    bool Open(ar_stream* data, const char* archivePath);

    Vec<FileInfo*> const& GetFileInfos();

    size_t GetFileId(const char* fileName);

    ByteSlice GetFileDataByName(const WCHAR* filename);
    ByteSlice GetFileDataByName(const char* filename);
    ByteSlice GetFileDataById(size_t fileId);

    std::string_view GetComment();

    // if true, will load and uncompress all files on open
    bool loadOnOpen{false};

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
    ByteSlice GetFileDataByIdUnarrDll(size_t fileId);
    [[nodiscard]] bool LoadedUsingUnrarDll() const {
        return rarFilePath_ != nullptr;
    }
};

MultiFormatArchive* OpenZipArchive(const char* path, bool deflatedOnly);
MultiFormatArchive* Open7zArchive(const char* path);
MultiFormatArchive* OpenTarArchive(const char* path);

// TODO: remove those
MultiFormatArchive* OpenZipArchive(const WCHAR* path, bool deflatedOnly);
MultiFormatArchive* Open7zArchive(const WCHAR* path);
MultiFormatArchive* OpenTarArchive(const WCHAR* path);
MultiFormatArchive* OpenRarArchive(const WCHAR* path);

MultiFormatArchive* OpenZipArchive(IStream* stream, bool deflatedOnly);
MultiFormatArchive* Open7zArchive(IStream* stream);
MultiFormatArchive* OpenTarArchive(IStream* stream);
MultiFormatArchive* OpenRarArchive(IStream* stream);

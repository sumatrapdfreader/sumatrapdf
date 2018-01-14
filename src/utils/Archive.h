/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// if ENABLE_UNRARDLL_FALLBACK is 1, SumatraPDF will look for unrar.dll/unrar64.dll
// in the same directory as SumatraPDF.exe whenever a RAR archive
// fails to open or extract and uses that as a fallback.
// If not set externally, we enable unarr fallback on Windows
#if !defined(ENABLE_UNRARDLL_FALLBACK)
#define ENABLE_UNRARDLL_FALLBACK OS_WIN
#endif

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

    Archive(archive_opener_t opener, Format format);
    ~Archive();

    Format format;

    bool Open(ar_stream* data, const char* archivePath);

    std::vector<FileInfo*> const& GetFileInfos();

    size_t GetFileId(const char* fileName);

// caller must free() the result
#if OS_WIN
    OwnedData GetFileDataByName(const WCHAR* filename);
#endif
    OwnedData GetFileDataByName(const char* filename);
    OwnedData GetFileDataById(size_t fileId);

    std::string_view GetComment();

  protected:
    // used for allocating strings that are referenced by ArchFileInfo::name
    PoolAllocator allocator_;
    std::vector<FileInfo*> fileInfos_;

    archive_opener_t opener_ = nullptr;
    ar_stream* data_ = nullptr;
    ar_archive* ar_ = nullptr;

#if ENABLE_UNRARDLL_FALLBACK
    // only set when we loaded file infos using unrar.dll fallback
    const char* rarFilePath_ = nullptr;

    bool OpenUnrarDllFallback(const char* rarPathUtf);
    OwnedData GetFileDataByIdUnarrDll(size_t fileId);
    bool LoadedUsingUnrarDll() const { return rarFilePath_ != nullptr; }
#endif
};

Archive* OpenZipArchive(const char* path, bool deflatedOnly);
Archive* Open7zArchive(const char* path);
Archive* OpenTarArchive(const char* path);

// TODO: remove those
#if OS_WIN
Archive* OpenZipArchive(const WCHAR* path, bool deflatedOnly);
Archive* Open7zArchive(const WCHAR* path);
Archive* OpenTarArchive(const WCHAR* path);
Archive* OpenRarArchive(const WCHAR* path);
#endif

#if OS_WIN
Archive* OpenZipArchive(IStream* stream, bool deflatedOnly);
Archive* Open7zArchive(IStream* stream);
Archive* OpenTarArchive(IStream* stream);
Archive* OpenRarArchive(IStream* stream);
#endif

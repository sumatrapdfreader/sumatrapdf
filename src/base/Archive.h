/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct archive;
struct archive_entry;

bool HasRarSignature(Str data);

struct Archive {
    enum class Format {
        Unknown,
        Zip,
        Rar,
        SevenZip,
        Tar
    };

    enum class RarHint {
        Detect,
        Rar,
        Other,
    };

    struct FileInfo {
        int fileId = 0;
        Str name;
        i64 fileTime = 0; // this is typedef'ed as time64_t in unrar.h
        int fileSizeUncompressed = 0;
        bool isDir = false;
        // Permanent extract failure (no path to reopen, corrupt entry).
        // Transient I/O (sleep, network drop) leaves this false so a later
        // GetFileDataById retries. `data` is nullptr when failed.
        bool failed = false;

        // internal use
        i64 filePos = 0;
        char* data = nullptr;
        // a thread is extracting it; others wait on Archive::extracted_
        bool extracting = false;
    };

    Archive();
    ~Archive();

    Format format = Format::Unknown;

    bool Open(Str path, bool eagerLoad, RarHint hint = RarHint::Detect);
    bool OpenFromData(Str data, bool eagerLoad = true);
    Str GetCompressedData() const { return archiveData_; }

    Vec<FileInfo*> const& GetFileInfos();

    int GetFileId(Str fileName);

    FileInfo* GetFileDataByName(Str filename);
    FileInfo* GetFileDataById(int fileId);
    Str GetFileDataPartById(int fileId, int sizeHint);
    // guards FileInfo::data / failed / extracting; extraction itself runs
    // unlocked so threads reading different entries don't wait for each other
    Mutex extractLock_;
    ConditionVariable extracted_;

    // password for encrypted archives (owned by this object)
    Str password;

    // set after Open() if the archive contains encrypted entries
    bool isEncrypted = false;

    // used for allocating strings that are referenced by ArchFileInfo::name
    Arena* a = nullptr;
    Vec<FileInfo*> fileInfos_;

    Str archivePath_;
    // compressed bytes when opened from memory; kept so we can re-open for lazy extract
    Str archiveData_;

    // only set when we loaded file infos using unrar.dll fallback
    Str rarFilePath_;

    bool OpenArchive(Str path, bool eagerLoad);
    bool ParseEntries(struct archive* a, bool eagerLoad);

    bool OpenUnrarFallback(Str rarPathUtf, bool eagerLoad);
    Str ReadUnrarEntry(FileInfo* fi, int toRead, bool* permanent);
    Str ReadLibarchiveEntry(FileInfo* fi, int toRead, bool* permanent);
    Str ReadEntry(FileInfo* fi, int toRead, bool* permanent);
    bool LoadedUsingUnrarDll() const { return (bool)rarFilePath_; }
};

Archive* OpenArchiveFromFile(Str path, bool eagerLoad);

Archive* OpenArchiveFromData(Str data);

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class FileType : u8;

struct archive;
struct archive_entry;

// forward-declared so ArchiveExtractProgress below can reference
// Archive::FileInfo, which is defined inside the class body.
struct Archive;

struct ArchiveExtractProgress;
using ArchiveExtractProgressCb = Func1<ArchiveExtractProgress*>;

// Thread-local progress callback honored by archive opens. Callers set
// it before triggering a load that may open archives (e.g. cbx / epub /
// fb2z); cleared afterwards. Archive openers pass it straight through to
// Archive::Open without further indirection.
extern thread_local ArchiveExtractProgressCb gArchiveProgressCb;

struct Archive {
    enum class Format {
        Unknown,
        Zip,
        Rar,
        SevenZip,
        Tar
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

        FILETIME GetWinFileTime() const;
    };

    Archive();
    ~Archive();

    Format format = Format::Unknown;

    bool Open(Str path, bool eagerLoad, FileType hintType, const ArchiveExtractProgressCb& cbProgress);
    bool OpenFromData(Str data);

    Vec<FileInfo*> const& GetFileInfos();

    int GetFileId(Str fileName);

    FileInfo* GetFileDataByName(Str filename);
    FileInfo* GetFileDataById(int fileId);
    Str GetFileDataPartById(int fileId, int sizeHint);

    // password for encrypted archives (owned by this object)
    Str password;

    // set after Open() if the archive contains encrypted entries
    bool isEncrypted = false;

    // used for allocating strings that are referenced by ArchFileInfo::name
    Arena* a = nullptr;
    Vec<FileInfo*> fileInfos_;

    Str archivePath_;

    // only set when we loaded file infos using unrar.dll fallback
    Str rarFilePath_;

    bool OpenArchive(Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);
    bool ParseEntries(struct archive* a, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);

    bool OpenUnrarFallback(Str rarPathUtf, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);
    void LoadFileDataByIdUnrarDll(int fileId);
    void LoadFileDataByIdLibarchive(int fileId);
    Str GetFileDataPartByIdUnrarDll(int fileId, int sizeHint);
    bool LoadedUsingUnrarDll() const { return (bool)rarFilePath_; }
};

// Progress callback payload. fileInfo points at the FileInfo record for
// the entry just processed (may have ->failed set). nDecoded is the
// running count (incremented whether decompression succeeded or failed);
// nTotal is the total count when known, -1 otherwise (libarchive only
// knows the total at the end, so most callbacks carry -1 and a final
// callback carries nDecoded == nTotal).
struct ArchiveExtractProgress {
    Archive::FileInfo* fileInfo;
    int nDecoded;
    int nTotal;
};

Archive* OpenArchiveFromFile(Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);

Archive* OpenArchiveFromData(Str data);

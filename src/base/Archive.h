/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct archive;
struct archive_entry;
struct IStream;

// forward-declared so ArchiveExtractProgress below can reference
// MultiFormatArchive::FileInfo, which is defined inside the class body.
struct MultiFormatArchive;

struct ArchiveExtractProgress;
using ArchiveExtractProgressCb = Func1<ArchiveExtractProgress*>;

// Thread-local progress callback honored by archive opens. Callers set
// it before triggering a load that may open archives (e.g. cbx / epub /
// fb2z); cleared afterwards. Archive openers pass it straight through to
// MultiFormatArchive::Open without further indirection.
extern thread_local ArchiveExtractProgressCb gArchiveProgressCb;

struct MultiFormatArchive {
    enum class Format {
        Unknown,
        Zip,
        Rar,
        SevenZip,
        Tar
    };

    struct FileInfo {
        int fileId = 0;
        Str name = {};
        i64 fileTime = 0; // this is typedef'ed as time64_t in unrar.h
        int fileSizeUncompressed = 0;
        bool isDir = false;
        // set when eagerLoad extraction failed for this entry (bad data,
        // OOM, etc.). `data` will be nullptr in that case.
        bool failed = false;

        // internal use
        i64 filePos = 0;
        char* data = nullptr;

        FILETIME GetWinFileTime() const;
    };

    MultiFormatArchive();
    ~MultiFormatArchive();

    Format format = Format::Unknown;

    bool Open(Str path, bool eagerLoad, Kind hintKind, const ArchiveExtractProgressCb& cbProgress);
    bool Open(IStream* stream);

    Vec<FileInfo*> const& GetFileInfos();

    int GetFileId(Str fileName);

    // Return the FileInfo record for a given entry, loading its data into
    // fileInfo->data on demand (on a miss, re-opens the archive unless
    // that was disabled by eager-load mode).
    //
    // Ownership: the returned FileInfo* is owned by this archive. By
    // default fileInfo->data is *not* transferred to the caller — a later
    // call for the same entry returns the same cached buffer, and the
    // archive destructor frees it. If the caller wants the buffer to
    // outlive the archive, they should set fileInfo->data = nullptr after
    // saving the pointer; they then become responsible for free()ing it.
    //
    // Returns nullptr for an unknown name / out-of-range fileId. For an
    // entry whose decompression failed check fileInfo->failed — data will
    // be nullptr in that case.
    FileInfo* GetFileDataByName(Str filename);
    FileInfo* GetFileDataById(int fileId);
    Str GetFileDataPartById(int fileId, int sizeHint);

    Str GetComment();

    // password for encrypted archives (owned by this object)
    Str password;

    // set after Open() if the archive contains encrypted entries
    bool isEncrypted = false;

    // used for allocating strings that are referenced by ArchFileInfo::name
    Arena* allocator_ = nullptr;
    Vec<FileInfo*> fileInfos_;

    Str archivePath_;

    // only set when we loaded file infos using unrar.dll fallback
    Str rarFilePath_;

    bool OpenArchive(Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);
    bool ParseEntries(struct archive* a, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);

    bool OpenUnrarFallback(Str rarPathUtf, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);
    // Populate fileInfos_[fileId]->data via the respective backend; set
    // ->failed when extraction didn't produce the expected bytes.
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
    MultiFormatArchive::FileInfo* fileInfo;
    int nDecoded;
    int nTotal;
};

MultiFormatArchive* OpenArchiveFromFile(Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);

MultiFormatArchive* OpenArchiveFromStream(IStream* stream);

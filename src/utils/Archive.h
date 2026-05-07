/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct archive;
struct archive_entry;

// forward-declared so ArchiveExtractProgress below can reference
// MultiFormatArchive::FileInfo, which is defined inside the class body.
class MultiFormatArchive;

struct ArchiveExtractProgress;
using ArchiveExtractProgressCb = Func1<ArchiveExtractProgress*>;

// Thread-local progress callback honored by archive opens. Callers set
// it before triggering a load that may open archives (e.g. cbx / epub /
// fb2z); cleared afterwards. Archive openers pass it straight through to
// MultiFormatArchive::Open without further indirection.
extern thread_local ArchiveExtractProgressCb gArchiveProgressCb;

class MultiFormatArchive {
  public:
    enum class Format {
        Unknown,
        Zip,
        Rar,
        SevenZip,
        Tar
    };

    struct FileInfo {
        size_t fileId = 0;
        const char* name = nullptr;
        i64 fileTime = 0; // this is typedef'ed as time64_t in unrar.h
        size_t fileSizeUncompressed = 0;
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

    // hintKind is the result of a prior GuessFileTypeFromContent() done
    // by the caller. When non-null we skip the internal 2 KiB sniff and
    // use it to drive rar-first vs. libarchive routing.
    // eagerLoad = true: decompress every entry at open time and close
    //   the archive so no re-open will ever happen.
    // cbProgress fires after each entry is processed (see
    // ArchiveExtractProgress). Pass a default-constructed Func1 to skip
    // notifications.
    bool Open(const char* path, bool eagerLoad, Kind hintKind, const ArchiveExtractProgressCb& cbProgress);
    bool Open(IStream* stream);

    Vec<FileInfo*> const& GetFileInfos();

    size_t GetFileId(const char* fileName);

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
    FileInfo* GetFileDataByName(const char* filename);
    FileInfo* GetFileDataById(size_t fileId);
    ByteSlice GetFileDataPartById(size_t fileId, size_t sizeHint);

    const char* GetComment();

    // password for encrypted archives (owned by this object)
    char* password = nullptr;

    // set after Open() if the archive contains encrypted entries
    bool isEncrypted = false;

  protected:
    // used for allocating strings that are referenced by ArchFileInfo::name
    Arena* allocator_ = nullptr;
    Vec<FileInfo*> fileInfos_;

    char* archivePath_ = nullptr;

    // only set when we loaded file infos using unrar.dll fallback
    const char* rarFilePath_ = nullptr;

    bool OpenArchive(const char* path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);
    bool ParseEntries(struct archive* a, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);

    bool OpenUnrarFallback(const char* rarPathUtf, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);
    // Populate fileInfos_[fileId]->data via the respective backend; set
    // ->failed when extraction didn't produce the expected bytes.
    void LoadFileDataByIdUnarrDll(size_t fileId);
    void LoadFileDataByIdLibarchive(size_t fileId);
    ByteSlice GetFileDataPartByIdUnarrDll(size_t fileId, size_t sizeHint);
    bool LoadedUsingUnrarDll() const { return rarFilePath_ != nullptr; }
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

// Open a file on disk. MultiFormatArchive::Open(path) detects RAR via a
// content sniff and routes it through unrar.dll; everything else goes
// through libarchive.
//
// eagerLoad: if true, every file is decompressed during Open() and the
// archive is then closed. GetFileDataById for a file that failed to
// decompress returns FileInfo with data=nullptr and never re-opens the
// file; use FileInfo::failed to tell "not yet loaded" from "failed".
// cbProgress fires once per entry (see ArchiveExtractProgress); pass a
// default-constructed Func1 to skip notifications.
MultiFormatArchive* OpenArchiveFromFile(const char* path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress);

// Open from an IStream. libarchive auto-detects the container (zip/rar/
// 7z/tar/etc.). Always eager-loads (can't re-open a stream); no progress
// reporting.
MultiFormatArchive* OpenArchiveFromStream(IStream* stream);

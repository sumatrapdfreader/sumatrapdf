/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "base/Archive.h"

#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"

#if OS_WIN
// TODO: set include path to ext/ dir
#include "../../ext/unrar/dll.hpp"
#endif

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
#define ZERO_PADDING_COUNT 3

thread_local ArchiveExtractProgressCb gArchiveProgressCb{};

#if OS_WIN
FILETIME Archive::FileInfo::GetWinFileTime() const {
    FILETIME ft = {(DWORD)-1, (DWORD)-1};
    LocalFileTimeToFileTime((FILETIME*)&fileTime, &ft);
    return ft;
}
#else
FILETIME Archive::FileInfo::GetWinFileTime() const {
    if (fileTime < 0) {
        return {(DWORD)-1, (DWORD)-1};
    }
    u64 ns = (u64)fileTime * 1000000000ULL;
    return {(DWORD)ns, (DWORD)(ns >> 32)};
}
#endif

Archive::Archive() {
    a = ArenaNew();
}

static Archive::Format FormatFromArchive(struct archive* a) {
    int fmt = archive_format(a);
    // archive_format returns a bitmask; the high bits identify the family
    if ((fmt & ARCHIVE_FORMAT_ZIP) == ARCHIVE_FORMAT_ZIP) {
        return Archive::Format::Zip;
    }
    if ((fmt & ARCHIVE_FORMAT_RAR) == ARCHIVE_FORMAT_RAR || (fmt & ARCHIVE_FORMAT_RAR_V5) == ARCHIVE_FORMAT_RAR_V5) {
        return Archive::Format::Rar;
    }
    if ((fmt & ARCHIVE_FORMAT_7ZIP) == ARCHIVE_FORMAT_7ZIP) {
        return Archive::Format::SevenZip;
    }
    if ((fmt & ARCHIVE_FORMAT_TAR) == ARCHIVE_FORMAT_TAR) {
        return Archive::Format::Tar;
    }
    return Archive::Format::Unknown;
}

Archive::~Archive() {
    for (auto& fi : fileInfos_) {
        free((void*)fi->data);
    }
    str::Free(archivePath_);
    str::Free(password);
    ArenaDelete(a);
}

bool Archive::ParseEntries(struct archive* a, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress) {
    struct archive_entry* entry;
    int fileId = 0;
    ArchiveExtractProgress prog{};
    prog.nTotal = -1; // libarchive streams; total is only known at end
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        Str entryName;
        const char* nameZ = archive_entry_pathname_utf8(entry);
        if (nameZ) {
            entryName = Str(nameZ);
        } else {
            nameZ = archive_entry_pathname(entry);
            entryName = nameZ ? Str(nameZ) : Str{};
        }
        FileInfo* i = AllocArray<FileInfo>(this->a);
        i->fileId = fileId;
        i->fileSizeUncompressed = (int)archive_entry_size(entry);
        i->filePos = (i64)fileId; // use fileId as position identifier
        i->fileTime = (i64)archive_entry_mtime(entry);
        i->name = str::Dup(this->a, entryName);
        i->isDir = (archive_entry_filetype(entry) == AE_IFDIR);
        i->data = nullptr;
        fileInfos_.Append(i);

        if (eagerLoad) {
            int size = i->fileSizeUncompressed;
            if (size > 0) {
                i->data = AllocArray<char>(size + ZERO_PADDING_COUNT);
                if (i->data) {
                    la_ssize_t n = archive_read_data(a, (void*)i->data, (size_t)size);
                    if (n < 0 || (int)n != size) {
                        free(i->data);
                        i->data = nullptr;
                        i->failed = true;
                    }
                } else {
                    i->failed = true; // OOM
                }
            }
        } else {
            archive_read_data_skip(a);
        }
        fileId++;
        prog.fileInfo = i;
        prog.nDecoded = fileId;
        cbProgress.Call(&prog);
    }
    if (fileId > 0) {
        // final callback with total known
        prog.fileInfo = fileInfos_[fileId - 1];
        prog.nDecoded = fileId;
        prog.nTotal = fileId;
        cbProgress.Call(&prog);
    }
    return fileId > 0;
}

// unfortunately libarchive's rar support is weak
static bool gUnrarFirst = true;

#if OS_WIN
static bool TryOpenUnrarFallback(Archive* archive, Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress,
                                 bool isRar) {
    if (!isRar) {
        return false;
    }
    bool ok = archive->OpenUnrarFallback(path, eagerLoad, cbProgress);
    if (ok) {
        archive->format = Archive::Format::Rar;
    }
    return ok;
}
#else
static bool TryOpenUnrarFallback(Archive*, Str, bool, const ArchiveExtractProgressCb&, bool) {
    return false;
}
#endif

// hintType is the result of a prior GuessFileTypeFromData() done
// by the caller. When not Unknown we skip the internal 2 KiB sniff and
// use it to drive rar-first vs. libarchive routing.
// eagerLoad = true: decompress every entry at open time and close
//   the archive so no re-open will ever happen.
// cbProgress fires after each entry is processed (see
// ArchiveExtractProgress). Pass a default-constructed Func1 to skip
// notifications.
bool Archive::Open(Str path, bool eagerLoad, FileType hintType, const ArchiveExtractProgressCb& cbProgress) {
    if (!path) {
        return false;
    }
    FileType ft = hintType;
    if (ft == FileType::Unknown) {
        // No pre-sniffed hint: peek at the first 2 KiB ourselves so we can
        // route RAR files through unrar.dll instead of libarchive.
        char buf[2048 + 1]{};
        int n = file::ReadN(path, (u8*)buf, dimof(buf) - 1);
        if (n > 0) {
            Str d = Str((char*)(buf), n);
            ft = GuessFileTypeFromData(d);
        }
    }

    // tar archives can't seek, so we have to eager-load even when the
    // caller didn't ask for it.
    if (ft == FileType::Tar) {
        eagerLoad = true;
    }

    bool isRar = ft == FileType::Rar;
    bool ok = false;
    if (gUnrarFirst && isRar) {
        ok = TryOpenUnrarFallback(this, path, eagerLoad, cbProgress, isRar);
    }
    if (!ok) {
        ok = OpenArchive(path, eagerLoad, cbProgress);
    }
    if (!ok && !gUnrarFirst && isRar) {
        // libarchive can open rar files but then fail to read them — fall
        // back to unrar.dll.
        ok = TryOpenUnrarFallback(this, path, eagerLoad, cbProgress, isRar);
    }
    if (!ok) {
        return false;
    }
    if (eagerLoad) {
        // Discard the paths so LoadFileDataByIdLibarchive / LoadFileDataByIdUnrarDll
        // can't re-open the archive to fetch a missing entry. Entries whose
        // decompression failed above have failed=true and data=nullptr, and
        // later GetFileDataById will see archivePath_==nullptr and mark
        // the entry as failed.
        str::Free(archivePath_);
        archivePath_ = {};
        rarFilePath_ = {}; // arena-allocated; don't free
    }
    return true;
}

static void SetArchivePassword(struct archive* a, Str password) {
    if (password) {
        archive_read_add_passphrase(a, CStrTemp(password));
    }
}

#if OS_WIN
static int ArchiveReadOpenFilename(struct archive* a, Str path) {
    WCHAR* pathW = CWStrTemp(path);
    return archive_read_open_filename_w(a, pathW, 10240);
}
#else
static int ArchiveReadOpenFilename(struct archive* a, Str path) {
    return archive_read_open_filename(a, CStrTemp(path), 10240);
}
#endif

bool Archive::OpenFromData(Str data) {
    if (len(data) == 0) {
        return false;
    }

    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    SetArchivePassword(a, password);
    int r = archive_read_open_memory(a, data.s, (size_t)data.len);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        str::Free(data);
        return false;
    }
    // no file path to re-open from, so load all file data now; no
    // progress reporting on this path.
    ArchiveExtractProgressCb emptyCb;
    format = FormatFromArchive(a);
    bool ok = ParseEntries(a, /*eagerLoad=*/true, emptyCb);
    if (archive_read_has_encrypted_entries(a) > 0) {
        isEncrypted = true;
    }
    archive_read_free(a);
    return ok;
}

bool Archive::OpenArchive(Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress) {
    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    SetArchivePassword(a, password);
    int r = ArchiveReadOpenFilename(a, path);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }
    archivePath_ = str::Dup(path);
    bool ok = ParseEntries(a, eagerLoad, cbProgress);
    if (ok) {
        format = FormatFromArchive(a);
    }
    if (archive_read_has_encrypted_entries(a) > 0) {
        isEncrypted = true;
    }
    archive_read_free(a);
    return ok;
}

Vec<Archive::FileInfo*> const& Archive::GetFileInfos() {
    return fileInfos_;
}

int getFileIdByName(Vec<Archive::FileInfo*>& fileInfos, Str name) {
    for (auto fileInfo : fileInfos) {
        if (str::EqI(fileInfo->name, name)) {
            return fileInfo->fileId;
        }
    }
    return -1;
}

int Archive::GetFileId(Str fileName) {
    return getFileIdByName(fileInfos_, fileName);
}

Archive::FileInfo* Archive::GetFileDataByName(Str fileName) {
    int fileId = getFileIdByName(fileInfos_, fileName);
    return GetFileDataById(fileId);
}

// Returns the FileInfo whose ->data field holds decompressed bytes (or
// nullptr / ->failed if extraction failed). The buffer stays owned by
// this archive; callers that want to keep the data past the archive's
// lifetime should set ->data = nullptr to transfer ownership.
Archive::FileInfo* Archive::GetFileDataById(int fileId) {
    if (fileId < 0) {
        return nullptr;
    }
    ReportIf(fileId >= len(fileInfos_));

    auto* fileInfo = fileInfos_[fileId];
    ReportIf(fileInfo->fileId != fileId);

    if (fileInfo->data != nullptr) {
        return fileInfo; // cached
    }
    if (fileInfo->failed) {
        return fileInfo; // already tried
    }

    if (LoadedUsingUnrarDll()) {
        LoadFileDataByIdUnrarDll(fileId);
    } else {
        LoadFileDataByIdLibarchive(fileId);
    }
    return fileInfo;
}

void Archive::LoadFileDataByIdLibarchive(int fileId) {
    auto* fileInfo = fileInfos_[fileId];
    if (!archivePath_) {
        fileInfo->failed = true;
        return;
    }

    // re-open the archive and skip to the right entry
    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    SetArchivePassword(a, password);
    int r = ArchiveReadOpenFilename(a, archivePath_);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        fileInfo->failed = true;
        return;
    }

    struct archive_entry* entry;
    int idx = 0;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (idx == fileId) {
            int size = fileInfo->fileSizeUncompressed;
            if (addOverflows<int>(size, ZERO_PADDING_COUNT)) {
                archive_read_free(a);
                fileInfo->failed = true;
                return;
            }
            u8* data = AllocArray<u8>(size + ZERO_PADDING_COUNT);
            if (!data) {
                archive_read_free(a);
                fileInfo->failed = true;
                return;
            }
            la_ssize_t n = archive_read_data(a, data, (size_t)size);
            archive_read_free(a);
            if (n < 0 || (int)n != size) {
                free(data);
                fileInfo->failed = true;
                return;
            }
            fileInfo->data = (char*)data;
            return;
        }
        archive_read_data_skip(a);
        idx++;
    }
    archive_read_free(a);
    fileInfo->failed = true;
}

Str Archive::GetFileDataPartById(int fileId, int sizeHint) {
    if (fileId < 0) {
        return {};
    }
    ReportIf(fileId >= len(fileInfos_));

    auto* fileInfo = fileInfos_[fileId];
    // if full data is cached, return a copy of the prefix
    if (fileInfo->data != nullptr) {
        int n = std::min(fileInfo->fileSizeUncompressed, sizeHint);
        u8* data = AllocArray<u8>(n + ZERO_PADDING_COUNT);
        if (!data) {
            return {};
        }
        memcpy(data, fileInfo->data, (size_t)n);
        return Str((char*)(data), n);
    }

    if (LoadedUsingUnrarDll()) {
        return GetFileDataPartByIdUnrarDll(fileId, sizeHint);
    }

    if (!archivePath_) {
        return {};
    }

    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    SetArchivePassword(a, password);
    int r = ArchiveReadOpenFilename(a, archivePath_);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return {};
    }

    struct archive_entry* entry;
    int idx = 0;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (idx == fileId) {
            int fullSize = fileInfo->fileSizeUncompressed;
            int toRead = std::min(fullSize, sizeHint);
            u8* data = AllocArray<u8>(toRead + ZERO_PADDING_COUNT);
            if (!data) {
                archive_read_free(a);
                return {};
            }
            la_ssize_t n = archive_read_data(a, data, (size_t)toRead);
            archive_read_free(a);
            if (n < 0) {
                free(data);
                return {};
            }
            return Str((char*)(data), (int)n);
        }
        archive_read_data_skip(a);
        idx++;
    }
    archive_read_free(a);
    return {};
}

Str Archive::GetComment() {
    // libarchive doesn't support zip global comments
    return {};
}

///// format specific handling /////

// Open a file on disk. Archive::Open(path) detects RAR via a
// content sniff and routes it through unrar.dll; everything else goes
// through libarchive.
//
// eagerLoad: if true, every file is decompressed during Open() and the
// archive is then closed. GetFileDataById for a file that failed to
// decompress returns FileInfo with data=nullptr and never re-opens the
// file; use FileInfo::failed to tell "not yet loaded" from "failed".
// cbProgress fires once per entry (see ArchiveExtractProgress); pass a
// default-constructed Func1 to skip notifications.
Archive* OpenArchiveFromFile(Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress) {
    auto* archive = new Archive();
    if (!archive->Open(path, eagerLoad, FileType::Unknown, cbProgress)) {
        delete archive;
        return nullptr;
    }
    return archive;
}

// Open from in-memory data. libarchive auto-detects the container (zip/rar/
// 7z/tar/etc.). Always eager-loads (can't re-open data); no progress reporting.
Archive* OpenArchiveFromData(Str data) {
    auto* archive = new Archive();
    if (!archive->OpenFromData(data)) {
        delete archive;
        return nullptr;
    }
    return archive;
}

#if OS_WIN
struct UnrarData {
    u8* d = nullptr;
    int sz = 0;
    u8* curr = nullptr;
    Str password;
};

static int DataLeft(const UnrarData& d) {
    int consumed = (int)(d.curr - d.d);
    ReportIf(consumed > d.sz);
    return d.sz - consumed;
}

// return 1 on success
static int CALLBACK unrarCallback(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed) {
    if (!userData) {
        return -1;
    }
    UnrarData* buf = (UnrarData*)userData;
    if (msg == UCM_PROCESSDATA) {
        int bytesGot = (int)bytesProcessed;
        if (bytesGot > DataLeft(*buf)) {
            return -1;
        }
        memcpy(buf->curr, (char*)rarBuffer, bytesGot);
        buf->curr += bytesGot;
        return 1;
    }
    if (msg == UCM_NEEDPASSWORDW) {
        if (!buf->password) {
            return -1;
        }
        WCHAR* pwdBuf = (WCHAR*)rarBuffer;
        int maxLen = (int)bytesProcessed;
        auto pwdW = ToWStrTemp(buf->password);
        int n = len(pwdW);
        if (n >= maxLen) {
            n = maxLen - 1;
        }
        memcpy(pwdBuf, pwdW.s, n * sizeof(WCHAR));
        pwdBuf[n] = 0;
        return 1;
    }
    return -1;
}

static bool FindFile(HANDLE hArc, RARHeaderDataEx* rarHeader, WStr fileName) {
    int res;
    for (;;) {
        res = RARReadHeaderEx(hArc, rarHeader);
        if (0 != res) {
            return false;
        }
        WStr nameW(rarHeader->FileNameW);
        wstr::TransCharsInPlace(nameW, WStrL(L"\\"), WStrL(L"/"));
        if (wstr::EqI(nameW, fileName)) {
            // don't support files whose uncompressed size is greater than 4GB
            return rarHeader->UnpSizeHigh == 0;
        }
        RARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
    }
}

void Archive::LoadFileDataByIdUnrarDll(int fileId) {
    auto* fileInfo = fileInfos_[fileId];
    ReportIf(fileInfo->fileId != fileId);
    if (fileInfo->data != nullptr) {
        return; // already loaded
    }
    if (!rarFilePath_) {
        fileInfo->failed = true;
        return;
    }

    WCHAR* rarPath = CWStrTemp(rarFilePath_);

    UnrarData uncompressedBuf;
    uncompressedBuf.password = password;

    RAROpenArchiveDataEx arcData = {nullptr};
    arcData.ArcNameW = rarPath;
    arcData.OpenMode = RAR_OM_EXTRACT;
    arcData.Callback = unrarCallback;
    arcData.UserData = (LPARAM)&uncompressedBuf;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        fileInfo->failed = true;
        return;
    }

    char* data = nullptr;
    int size = 0;
    auto fileName = ToWStrTemp(fileInfo->name);
    RARHeaderDataEx rarHeader{};
    int res;
    bool ok = FindFile(hArc, &rarHeader, fileName);
    if (!ok) {
        goto Exit;
    }
    size = fileInfo->fileSizeUncompressed;
    ReportIf(size != (int)rarHeader.UnpSize);
    if (addOverflows<int>(size, ZERO_PADDING_COUNT)) {
        ok = false;
        goto Exit;
    }

    data = AllocArray<char>(size + ZERO_PADDING_COUNT);
    if (!data) {
        ok = false;
        goto Exit;
    }

    uncompressedBuf.d = (u8*)data;
    uncompressedBuf.curr = (u8*)data;
    uncompressedBuf.sz = size;
    res = RARProcessFile(hArc, RAR_TEST, nullptr, nullptr);
    ok = (res == 0) && (DataLeft(uncompressedBuf) == 0);

Exit:
    RARCloseArchive(hArc);
    if (!ok) {
        free(data);
        fileInfo->failed = true;
        return;
    }
    fileInfo->data = data;
}

Str Archive::GetFileDataPartByIdUnrarDll(int fileId, int sizeHint) {
    ReportIf(!rarFilePath_);

    auto* fileInfo = fileInfos_[fileId];
    ReportIf(fileInfo->fileId != fileId);
    if (fileInfo->data != nullptr) {
        int n = std::min(fileInfo->fileSizeUncompressed, sizeHint);
        u8* data = AllocArray<u8>(n + ZERO_PADDING_COUNT);
        if (!data) {
            return {};
        }
        memcpy(data, fileInfo->data, (size_t)n);
        return Str((char*)(data), n);
    }

    WCHAR* rarPath = CWStrTemp(rarFilePath_);

    UnrarData uncompressedBuf;
    uncompressedBuf.password = password;

    RAROpenArchiveDataEx arcData = {nullptr};
    arcData.ArcNameW = rarPath;
    arcData.OpenMode = RAR_OM_EXTRACT;
    arcData.Callback = unrarCallback;
    arcData.UserData = (LPARAM)&uncompressedBuf;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        return {};
    }

    char* data = nullptr;
    int size = 0;
    auto fileName = ToWStrTemp(fileInfo->name);
    RARHeaderDataEx rarHeader{};
    bool ok = FindFile(hArc, &rarHeader, fileName);
    if (!ok) {
        goto Exit;
    }
    // allocate only sizeHint bytes; the callback will stop when the buffer is full
    size = std::min(fileInfo->fileSizeUncompressed, sizeHint);
    if (addOverflows<int>(size, ZERO_PADDING_COUNT)) {
        ok = false;
        goto Exit;
    }

    data = AllocArray<char>(size + ZERO_PADDING_COUNT);
    if (!data) {
        ok = false;
        goto Exit;
    }

    uncompressedBuf.d = (u8*)data;
    uncompressedBuf.curr = (u8*)data;
    uncompressedBuf.sz = size;
    RARProcessFile(hArc, RAR_TEST, nullptr, nullptr);
    // if we requested less than full size, the callback returns -1 when full,
    // causing RARProcessFile to return an error; that's expected
    ok = (uncompressedBuf.curr > uncompressedBuf.d);

Exit:
    RARCloseArchive(hArc);
    if (!ok) {
        free(data);
        return {};
    }
    int got = (int)(uncompressedBuf.curr - uncompressedBuf.d);
    return Str((char*)((u8*)data), got);
}

// asan build crashes in UnRAR code
// see https://codeeval.dev/gist/801ad556960e59be41690d0c2fa7cba0
bool Archive::OpenUnrarFallback(Str rarPath, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress) {
    if (!rarPath) {
        return false;
    }
    ReportIf(rarFilePath_);
    WCHAR* rarPathW = CWStrTemp(rarPath);

    UnrarData uncompressedBuf;
    uncompressedBuf.password = password;

    RAROpenArchiveDataEx arcData = {nullptr};
    arcData.ArcNameW = rarPathW;
    arcData.OpenMode = eagerLoad ? RAR_OM_EXTRACT : RAR_OM_LIST;
    arcData.Callback = unrarCallback;
    arcData.UserData = (LPARAM)&uncompressedBuf;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        return false;
    }

    ArchiveExtractProgress prog{};
    prog.nTotal = -1;
    int fileId = 0;
    while (true) {
        RARHeaderDataEx rarHeader{};
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res) {
            break;
        }

        if (rarHeader.Flags & RHDF_ENCRYPTED) {
            isEncrypted = true;
        }

        WStr nameW(rarHeader.FileNameW);
        wstr::TransCharsInPlace(nameW, WStrL(L"\\"), WStrL(L"/"));
        auto name = ToUtf8Temp(rarHeader.FileNameW);

        FileInfo* i = AllocArray<FileInfo>(a);
        i->fileId = fileId;
        i->fileSizeUncompressed = (int)rarHeader.UnpSize;
        i->filePos = 0;
        i->fileTime = (i64)rarHeader.FileTime;
        i->name = str::Dup(a, name);
        i->isDir = (rarHeader.Flags & RHDF_DIRECTORY) != 0;
        i->data = nullptr;
        if (eagerLoad) {
            // +2 so that it's zero-terminated even when interprted as WCHAR*
            i->data = AllocArray<char>(i->fileSizeUncompressed + 2);
            if (i->data) {
                uncompressedBuf.d = (u8*)i->data;
                uncompressedBuf.curr = (u8*)i->data;
                uncompressedBuf.sz = i->fileSizeUncompressed;
            } else {
                i->failed = true; // OOM
            }
        }
        fileInfos_.Append(i);

        fileId++;

        int op = RAR_SKIP;
        if (eagerLoad && !i->failed) {
            op = RAR_EXTRACT;
        }
        int rres = RARProcessFile(hArc, op, nullptr, nullptr);
        if (eagerLoad && !i->failed) {
            // Unrar treats extraction errors as non-zero return; also
            // require the buffer was fully filled (curr advanced by exactly
            // the declared uncompressed size).
            bool extracted = (rres == 0) && (int)(uncompressedBuf.curr - uncompressedBuf.d) == uncompressedBuf.sz;
            if (!extracted) {
                free(i->data);
                i->data = nullptr;
                i->failed = true;
            }
        }
        prog.fileInfo = i;
        prog.nDecoded = fileId;
        cbProgress.Call(&prog);
    }
    if (fileId > 0) {
        prog.fileInfo = fileInfos_[fileId - 1];
        prog.nDecoded = fileId;
        prog.nTotal = fileId;
        cbProgress.Call(&prog);
    }

    RARCloseArchive(hArc);

    rarFilePath_ = str::Dup(a, rarPath);
    return true;
}
#else
void Archive::LoadFileDataByIdUnrarDll(int fileId) {
    fileInfos_[fileId]->failed = true;
}

Str Archive::GetFileDataPartByIdUnrarDll(int, int) {
    return {};
}

bool Archive::OpenUnrarFallback(Str, bool, const ArchiveExtractProgressCb&) {
    return false;
}
#endif

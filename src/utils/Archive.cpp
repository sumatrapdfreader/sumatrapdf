/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Archive.h"

#include "StrSlice.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "CryptoUtil.h"

extern "C" {
#include <unarr.h>
}

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
#define ZERO_PADDING_COUNT 3

// for debugging of unrar.dll fallback, if set to true we'll try to
// open .rar files using unrar.dll (otherwise it only happens if unarr
// fails to open
static bool tryUnrarDllFirst = true;

#if OS_WIN
FILETIME Archive::FileInfo::GetWinFileTime() const {
    FILETIME ft = {(DWORD)-1, (DWORD)-1};
    LocalFileTimeToFileTime((FILETIME*)&fileTime, &ft);
    return ft;
}
#endif

Archive::Archive(archive_opener_t opener, Archive::Format format) : format(format), opener_(opener) {
    CrashIf(!opener);
}

bool Archive::Open(ar_stream* data, const char* archivePath) {
    data_ = data;
    if (!data) {
        return false;
    }
    if ((format == Format::Rar) && archivePath && tryUnrarDllFirst) {
        bool ok = OpenUnrarDllFallback(archivePath);
        if (ok) {
            return true;
        }
    }
    ar_ = opener_(data);
    if (!ar_ || ar_at_eof(ar_)) {
        if (format == Format::Rar && archivePath) {
            return OpenUnrarDllFallback(archivePath);
        }
        return false;
    }

    size_t fileId = 0;
    while (ar_parse_entry(ar_)) {
        const char* name = ar_entry_get_name(ar_);
        if (!name) {
            name = "";
        }

        FileInfo* i = allocator_.AllocStruct<FileInfo>();
        i->fileId = fileId;
        i->fileSizeUncompressed = ar_entry_get_size(ar_);
        i->filePos = ar_entry_get_offset(ar_);
        i->fileTime = ar_entry_get_filetime(ar_);
        i->name = Allocator::AllocString(&allocator_, name);
        fileInfos_.push_back(i);

        fileId++;
    }
    return true;
}

Archive::~Archive() {
    ar_close_archive(ar_);
    ar_close(data_);
}

size_t getFileIdByName(std::vector<Archive::FileInfo*>& fileInfos, const char* name) {
    for (auto fileInfo : fileInfos) {
        if (str::EqI(fileInfo->name.data(), name)) {
            return fileInfo->fileId;
        }
    }
    return (size_t)-1;
}

std::vector<Archive::FileInfo*> const& Archive::GetFileInfos() {
    return fileInfos_;
}

size_t Archive::GetFileId(const char* fileName) {
    return getFileIdByName(fileInfos_, fileName);
}

#if OS_WIN
OwnedData Archive::GetFileDataByName(const WCHAR* fileName) {
    auto fileNameUtf8 = str::conv::ToUtf8(fileName);
    return GetFileDataByName(fileNameUtf8.Get());
}
#endif

OwnedData Archive::GetFileDataByName(const char* fileName) {
    size_t fileId = getFileIdByName(fileInfos_, fileName);
    return GetFileDataById(fileId);
}

OwnedData Archive::GetFileDataById(size_t fileId) {
    if (fileId == (size_t)-1) {
        return {};
    }
    CrashIf(fileId >= fileInfos_.size());

    if (LoadedUsingUnrarDll()) {
        return GetFileDataByIdUnarrDll(fileId);
    }

    if (!ar_) {
        return {};
    }

    auto* fileInfo = fileInfos_[fileId];
    CrashIf(fileInfo->fileId != fileId);

    auto filePos = fileInfo->filePos;
    if (!ar_parse_entry_at(ar_, filePos)) {
        return {};
    }
    size_t size = fileInfo->fileSizeUncompressed;
    if (addOverflows<size_t>(size, ZERO_PADDING_COUNT)) {
        return {};
    }
    OwnedData data(AllocArray<char>(size + ZERO_PADDING_COUNT), size);
    if (!data.data) {
        return {};
    }
    if (!ar_entry_uncompress(ar_, data.data, size)) {
        return {};
    }

    return data;
}

std::string_view Archive::GetComment() {
    if (!ar_) {
        return {};
    }

    size_t n = ar_get_global_comment(ar_, nullptr, 0);
    if (0 == n || (size_t)-1 == n) {
        return {};
    }
    char* comment = Allocator::Alloc<char>(&allocator_, n + 1);
    if (!comment) {
        return {};
    }
    size_t nRead = ar_get_global_comment(ar_, comment, n);
    if (nRead != n) {
        return {};
    }
    return std::string_view(comment, n);
}

///// format specific handling /////

static ar_archive* ar_open_zip_archive_any(ar_stream* stream) {
    return ar_open_zip_archive(stream, false);
}
static ar_archive* ar_open_zip_archive_deflated(ar_stream* stream) {
    return ar_open_zip_archive(stream, true);
}

static Archive* open(Archive* archive, const char* path) {
    FILE* f = file::OpenFILE(path);
    archive->Open(ar_open(f), path);
    return archive;
}

#if OS_WIN
static Archive* open(Archive* archive, const WCHAR* path) {
    FILE* f = file::OpenFILE(path);
    auto pathUtf = str::conv::ToUtf8(path);
    archive->Open(ar_open(f), pathUtf.Get());
    return archive;
}

static Archive* open(Archive* archive, IStream* stream) {
    archive->Open(ar_open_istream(stream), nullptr);
    return archive;
}
#endif

Archive* OpenZipArchive(const char* path, bool deflatedOnly) {
    auto opener = ar_open_zip_archive_any;
    if (deflatedOnly) {
        opener = ar_open_zip_archive_deflated;
    }
    auto* archive = new Archive(opener, Archive::Format::Zip);
    return open(archive, path);
}

Archive* Open7zArchive(const char* path) {
    auto* archive = new Archive(ar_open_7z_archive, Archive::Format::SevenZip);
    return open(archive, path);
}

Archive* OpenTarArchive(const char* path) {
    auto* archive = new Archive(ar_open_tar_archive, Archive::Format::Tar);
    return open(archive, path);
}

Archive* OpenRarArchive(const char* path) {
    auto* archive = new Archive(ar_open_rar_archive, Archive::Format::Rar);
    return open(archive, path);
}

#if OS_WIN
Archive* OpenZipArchive(const WCHAR* path, bool deflatedOnly) {
    auto opener = ar_open_zip_archive_any;
    if (deflatedOnly) {
        opener = ar_open_zip_archive_deflated;
    }
    auto* archive = new Archive(opener, Archive::Format::Zip);
    return open(archive, path);
}

Archive* Open7zArchive(const WCHAR* path) {
    auto* archive = new Archive(ar_open_7z_archive, Archive::Format::SevenZip);
    return open(archive, path);
}

Archive* OpenTarArchive(const WCHAR* path) {
    auto* archive = new Archive(ar_open_tar_archive, Archive::Format::Tar);
    return open(archive, path);
}

Archive* OpenRarArchive(const WCHAR* path) {
    auto* archive = new Archive(ar_open_rar_archive, Archive::Format::Rar);
    return open(archive, path);
}
#endif

#if OS_WIN
Archive* OpenZipArchive(IStream* stream, bool deflatedOnly) {
    auto opener = ar_open_zip_archive_any;
    if (deflatedOnly) {
        opener = ar_open_zip_archive_deflated;
    }
    auto* archive = new Archive(opener, Archive::Format::Zip);
    return open(archive, stream);
}

Archive* Open7zArchive(IStream* stream) {
    auto* archive = new Archive(ar_open_7z_archive, Archive::Format::SevenZip);
    return open(archive, stream);
}

Archive* OpenTarArchive(IStream* stream) {
    auto* archive = new Archive(ar_open_tar_archive, Archive::Format::Tar);
    return open(archive, stream);
}

Archive* OpenRarArchive(IStream* stream) {
    auto* archive = new Archive(ar_open_rar_archive, Archive::Format::Rar);
    return open(archive, stream);
}
#endif

// TODO: set include path to ext/ dir
// TODO: delay link with UnRAR.lib to avoid dynamically loading it
#include "../../ext/UnrarDLL/unrar.h"

typedef int(PASCAL* RARGetDllVersionProc)();
typedef HANDLE(PASCAL* RAROpenArchiveExProc)(struct RAROpenArchiveDataEx* ArchiveData);
typedef int(PASCAL* RARReadHeaderExProc)(HANDLE hArcData, struct RARHeaderDataEx* HeaderData);
typedef int(PASCAL* RARProcessFileProc)(HANDLE hArcData, int Operation, char* DestPath, char* DestName);
typedef int(PASCAL* RARCloseArchiveProc)(HANDLE hArcData);

static RAROpenArchiveExProc fnRAROpenArchiveEx = nullptr;
static RARReadHeaderExProc fnRARReadHeaderEx = nullptr;
static RARProcessFileProc fnRARProcessFile = nullptr;
static RARCloseArchiveProc fnRARCloseArchive = nullptr;
static RARGetDllVersionProc fnRARGetDllVersion = nullptr;

static bool IsUnrarDllLoaded() {
    return fnRAROpenArchiveEx && fnRARReadHeaderEx && fnRARProcessFile && fnRARCloseArchive && fnRARGetDllVersion;
}

static bool IsValidUnrarDll() {
    int ver = fnRARGetDllVersion();
    return ver >= 6;
}

#ifdef _WIN64
static const WCHAR* unrarFileName = L"unrar64.dll";
#else
static const WCHAR* unrarFileName = L"unrar.dll";
#endif

static AutoFreeW unrarDllPath;

void SetUnrarDllPath(const WCHAR* path) {
    unrarDllPath.SetCopy(path);
}

static bool TryLoadUnrarDll() {
    if (IsUnrarDllLoaded()) {
        return IsValidUnrarDll();
    }

    HMODULE h = nullptr;
    if (unrarDllPath.Get() != nullptr) {
        h = LoadLibraryW(unrarDllPath.Get());
    }
    if (h == nullptr) {
        auto* dllPath = path::GetPathOfFileInAppDir(unrarFileName);
        h = LoadLibraryW(dllPath);
        free(dllPath);
    }
    if (h == nullptr) {
        return false;
    }
    fnRAROpenArchiveEx = (RAROpenArchiveExProc)GetProcAddress(h, "RAROpenArchiveEx");
    fnRARReadHeaderEx = (RARReadHeaderExProc)GetProcAddress(h, "RARReadHeaderEx");
    fnRARProcessFile = (RARProcessFileProc)GetProcAddress(h, "RARProcessFile");
    fnRARCloseArchive = (RARCloseArchiveProc)GetProcAddress(h, "RARCloseArchive");
    fnRARGetDllVersion = (RARGetDllVersionProc)GetProcAddress(h, "RARGetDllVersion");
    return IsUnrarDllLoaded() && IsValidUnrarDll();
}

// return 1 on success. Other values for msg that we don't handle: UCM_CHANGEVOLUME, UCM_NEEDPASSWORD
static int CALLBACK unrarCallback(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed) {
    if (UCM_PROCESSDATA != msg || !userData) {
        return -1;
    }
    str::Slice* buf = (str::Slice*)userData;
    size_t bytesGot = (size_t)bytesProcessed;
    if (bytesGot > buf->Left()) {
        return -1;
    }
    memcpy(buf->curr, (char*)rarBuffer, bytesGot);
    buf->curr += bytesGot;
    return 1;
}

static bool FindFile(HANDLE hArc, RARHeaderDataEx* rarHeader, const WCHAR* fileName) {
    int res;
    for (;;) {
        res = fnRARReadHeaderEx(hArc, rarHeader);
        if (0 != res) {
            return false;
        }
        str::TransChars(rarHeader->FileNameW, L"\\", L"/");
        if (str::EqI(rarHeader->FileNameW, fileName)) {
            // don't support files whose uncompressed size is greater than 4GB
            return rarHeader->UnpSizeHigh == 0;
        }
        fnRARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
    }
}

OwnedData Archive::GetFileDataByIdUnarrDll(size_t fileId) {
    CrashIf(!IsUnrarDllLoaded());
    CrashIf(!IsValidUnrarDll());
    CrashIf(!rarFilePath_);

    AutoFreeW rarPath(str::conv::FromUtf8(rarFilePath_));

    str::Slice uncompressedBuf;

    RAROpenArchiveDataEx arcData = {0};
    arcData.ArcNameW = rarPath.Get();
    arcData.OpenMode = RAR_OM_EXTRACT;
    arcData.Callback = unrarCallback;
    arcData.UserData = (LPARAM)&uncompressedBuf;

    HANDLE hArc = fnRAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        return {};
    }

    auto* fileInfo = fileInfos_[fileId];
    CrashIf(fileInfo->fileId != fileId);

    char* data = nullptr;
    size_t size = 0;
    AutoFreeW fileName(str::conv::FromUtf8(fileInfo->name.data()));
    RARHeaderDataEx rarHeader = {0};
    bool ok = FindFile(hArc, &rarHeader, fileName.Get());
    if (!ok) {
        goto Exit;
    }
    size = fileInfo->fileSizeUncompressed;
    CrashIf(size != rarHeader.UnpSize);
    if (addOverflows<size_t>(size, ZERO_PADDING_COUNT)) {
        ok = false;
        goto Exit;
    }

    data = AllocArray<char>(size + ZERO_PADDING_COUNT);
    if (!data) {
        ok = false;
        goto Exit;
    }
    uncompressedBuf.Set(data, size);
    int res = fnRARProcessFile(hArc, RAR_TEST, nullptr, nullptr);
    ok = (res == 0) && (uncompressedBuf.Left() == 0);

Exit:
    fnRARCloseArchive(hArc);
    if (!ok) {
        free(data);
        return {};
    }
    return {data, size};
}

bool Archive::OpenUnrarDllFallback(const char* rarPathUtf) {
    if (!rarPathUtf || !TryLoadUnrarDll()) {
        return false;
    }
    CrashIf(rarFilePath_);
    AutoFreeW rarPath(str::conv::FromUtf8(rarPathUtf));

    RAROpenArchiveDataEx arcData = {0};
    arcData.ArcNameW = (WCHAR*)rarPath;
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = fnRAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        return false;
    }

    size_t fileId = 0;
    while (true) {
        RARHeaderDataEx rarHeader = {0};
        int res = fnRARReadHeaderEx(hArc, &rarHeader);
        if (0 != res) {
            break;
        }

        str::TransChars(rarHeader.FileNameW, L"\\", L"/");
        OwnedData name(str::conv::ToUtf8(rarHeader.FileNameW));

        FileInfo* i = allocator_.AllocStruct<FileInfo>();
        i->fileId = fileId;
        i->fileSizeUncompressed = (size_t)rarHeader.UnpSize;
        i->filePos = 0;
        i->fileTime = (int64_t)rarHeader.FileTime;
        i->name = Allocator::AllocString(&allocator_, name.Get());
        fileInfos_.push_back(i);

        fileId++;

        fnRARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
    }

    fnRARCloseArchive(hArc);

    auto tmp = Allocator::AllocString(&allocator_, rarPathUtf);
    rarFilePath_ = tmp.data();
    return true;
}

/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/CryptoUtil.h"

#include "utils/Archive.h"

extern "C" {
#include <unarr.h>
}

// TODO: set include path to ext/ dir
#include "../../ext/unrar/dll.hpp"

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
#define ZERO_PADDING_COUNT 3

FILETIME MultiFormatArchive::FileInfo::GetWinFileTime() const {
    FILETIME ft = {(DWORD)-1, (DWORD)-1};
    LocalFileTimeToFileTime((FILETIME*)&fileTime, &ft);
    return ft;
}

MultiFormatArchive::MultiFormatArchive(archive_opener_t opener, MultiFormatArchive::Format format)
    : format(format), opener_(opener) {
    CrashIf(!opener);
    if(format == Format::Tar)
        loadOnOpen = true;
}

bool MultiFormatArchive::Open(ar_stream* data, const char* archivePath) {
    data_ = data;
    if (!data) {
        return false;
    }
    if ((format == Format::Rar) && archivePath) {
        bool ok = OpenUnrarFallback(archivePath);
        if (ok) {
            return true;
        }
    }
    ar_ = opener_(data);
    if (!ar_ || ar_at_eof(ar_)) {
        if (format == Format::Rar && archivePath) {
            return OpenUnrarFallback(archivePath);
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
        i->name = str::Dup(&allocator_, name);
        i->data = nullptr;
        fileInfos_.Append(i);
        // doesn't benchmark faster for .zip files but not much slower either
        // is probably faster for .tar.gz files
        if (loadOnOpen) {
            size_t size = i->fileSizeUncompressed;
            i->data = AllocArray<char>(size + ZERO_PADDING_COUNT);
            if (i->data) {
                bool ok = ar_entry_uncompress(ar_, (void*)i->data, size);
                if (!ok) {
                    free(i->data);
                    i->data = nullptr;
                }
            }
        }

        fileId++;
    }
    return true;
}

MultiFormatArchive::~MultiFormatArchive() {
    ar_close_archive(ar_);
    ar_close(data_);
    for (auto& fi : fileInfos_) {
        free((void*)fi->data);
    }
}

size_t getFileIdByName(Vec<MultiFormatArchive::FileInfo*>& fileInfos, const char* name) {
    for (auto fileInfo : fileInfos) {
        if (str::EqI(fileInfo->name.data(), name)) {
            return fileInfo->fileId;
        }
    }
    return (size_t)-1;
}

Vec<MultiFormatArchive::FileInfo*> const& MultiFormatArchive::GetFileInfos() {
    return fileInfos_;
}

size_t MultiFormatArchive::GetFileId(const char* fileName) {
    return getFileIdByName(fileInfos_, fileName);
}

ByteSlice MultiFormatArchive::GetFileDataByName(const WCHAR* fileName) {
    auto fileNameA = ToUtf8Temp(fileName);
    return GetFileDataByName(fileNameA);
}

ByteSlice MultiFormatArchive::GetFileDataByName(const char* fileName) {
    size_t fileId = getFileIdByName(fileInfos_, fileName);
    return GetFileDataById(fileId);
}

// the caller must free()
ByteSlice MultiFormatArchive::GetFileDataById(size_t fileId) {
    if (fileId == (size_t)-1) {
        return {};
    }
    CrashIf(fileId >= fileInfos_.size());

    auto* fileInfo = fileInfos_[fileId];
    CrashIf(fileInfo->fileId != fileId);

    if (fileInfo->data != nullptr) {
        // the caller takes ownership
        ByteSlice res{(u8*)fileInfo->data, fileInfo->fileSizeUncompressed};
        fileInfo->data = nullptr;
        return res;
    }

    if (LoadedUsingUnrarDll()) {
        return GetFileDataByIdUnarrDll(fileId);
    }

    if (!ar_) {
        return {};
    }

    auto filePos = fileInfo->filePos;
    if (!ar_parse_entry_at(ar_, filePos)) {
        return {};
    }
    size_t size = fileInfo->fileSizeUncompressed;
    if (addOverflows<size_t>(size, ZERO_PADDING_COUNT)) {
        return {};
    }
    u8* data = AllocArray<u8>(size + ZERO_PADDING_COUNT);
    if (!data) {
        return {};
    }
    if (!ar_entry_uncompress(ar_, data, size)) {
        return {};
    }

    return {data, size};
}

std::string_view MultiFormatArchive::GetComment() {
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

static MultiFormatArchive* open(MultiFormatArchive* archive, const char* path) {
    archive->Open(ar_open_file(path), path);
    return archive;
}

static MultiFormatArchive* open(MultiFormatArchive* archive, const WCHAR* path) {
    auto pathA = ToUtf8Temp(path);
    bool ok = archive->Open(ar_open_file_w(path), pathA.Get());
    if (!ok) {
        delete archive;
        return nullptr;
    }
    return archive;
}

static MultiFormatArchive* open(MultiFormatArchive* archive, IStream* stream) {
    bool ok = archive->Open(ar_open_istream(stream), nullptr);
    if (!ok) {
        delete archive;
        return nullptr;
    }
    return archive;
}

MultiFormatArchive* OpenZipArchive(const char* path, bool deflatedOnly) {
    auto opener = ar_open_zip_archive_any;
    if (deflatedOnly) {
        opener = ar_open_zip_archive_deflated;
    }
    auto* archive = new MultiFormatArchive(opener, MultiFormatArchive::Format::Zip);
    return open(archive, path);
}

MultiFormatArchive* Open7zArchive(const char* path) {
    auto* archive = new MultiFormatArchive(ar_open_7z_archive, MultiFormatArchive::Format::SevenZip);
    return open(archive, path);
}

MultiFormatArchive* OpenTarArchive(const char* path) {
    auto* archive = new MultiFormatArchive(ar_open_tar_archive, MultiFormatArchive::Format::Tar);
    return open(archive, path);
}

MultiFormatArchive* OpenRarArchive(const char* path) {
    auto* archive = new MultiFormatArchive(ar_open_rar_archive, MultiFormatArchive::Format::Rar);
    return open(archive, path);
}

MultiFormatArchive* OpenZipArchive(const WCHAR* path, bool deflatedOnly) {
    auto opener = ar_open_zip_archive_any;
    if (deflatedOnly) {
        opener = ar_open_zip_archive_deflated;
    }
    auto* archive = new MultiFormatArchive(opener, MultiFormatArchive::Format::Zip);
    return open(archive, path);
}

MultiFormatArchive* Open7zArchive(const WCHAR* path) {
    auto* archive = new MultiFormatArchive(ar_open_7z_archive, MultiFormatArchive::Format::SevenZip);
    return open(archive, path);
}

MultiFormatArchive* OpenTarArchive(const WCHAR* path) {
    auto* archive = new MultiFormatArchive(ar_open_tar_archive, MultiFormatArchive::Format::Tar);
    return open(archive, path);
}

MultiFormatArchive* OpenRarArchive(const WCHAR* path) {
    auto* archive = new MultiFormatArchive(ar_open_rar_archive, MultiFormatArchive::Format::Rar);
    return open(archive, path);
}

MultiFormatArchive* OpenZipArchive(IStream* stream, bool deflatedOnly) {
    auto opener = ar_open_zip_archive_any;
    if (deflatedOnly) {
        opener = ar_open_zip_archive_deflated;
    }
    auto* archive = new MultiFormatArchive(opener, MultiFormatArchive::Format::Zip);
    return open(archive, stream);
}

MultiFormatArchive* Open7zArchive(IStream* stream) {
    auto* archive = new MultiFormatArchive(ar_open_7z_archive, MultiFormatArchive::Format::SevenZip);
    return open(archive, stream);
}

MultiFormatArchive* OpenTarArchive(IStream* stream) {
    auto* archive = new MultiFormatArchive(ar_open_tar_archive, MultiFormatArchive::Format::Tar);
    return open(archive, stream);
}

MultiFormatArchive* OpenRarArchive(IStream* stream) {
    auto* archive = new MultiFormatArchive(ar_open_rar_archive, MultiFormatArchive::Format::Rar);
    return open(archive, stream);
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
        res = RARReadHeaderEx(hArc, rarHeader);
        if (0 != res) {
            return false;
        }
        str::TransCharsInPlace(rarHeader->FileNameW, L"\\", L"/");
        if (str::EqI(rarHeader->FileNameW, fileName)) {
            // don't support files whose uncompressed size is greater than 4GB
            return rarHeader->UnpSizeHigh == 0;
        }
        RARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
    }
}

ByteSlice MultiFormatArchive::GetFileDataByIdUnarrDll(size_t fileId) {
    CrashIf(!rarFilePath_);

    auto* fileInfo = fileInfos_[fileId];
    CrashIf(fileInfo->fileId != fileId);
    if (fileInfo->data != nullptr) {
        return {(u8*)fileInfo->data, fileInfo->fileSizeUncompressed};
    }

    auto rarPath = ToWstrTemp(rarFilePath_);

    str::Slice uncompressedBuf;

    RAROpenArchiveDataEx arcData = {nullptr};
    arcData.ArcNameW = rarPath.Get();
    arcData.OpenMode = RAR_OM_EXTRACT;
    arcData.Callback = unrarCallback;
    arcData.UserData = (LPARAM)&uncompressedBuf;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        return {};
    }

    char* data = nullptr;
    size_t size = 0;
    auto fileName = ToWstrTemp(fileInfo->name.data());
    RARHeaderDataEx rarHeader = {0};
    int res;
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
    res = RARProcessFile(hArc, RAR_TEST, nullptr, nullptr);
    ok = (res == 0) && (uncompressedBuf.Left() == 0);

Exit:
    RARCloseArchive(hArc);
    if (!ok) {
        free(data);
        return {};
    }
    return {(u8*)data, size};
}

// asan build crashes in UnRAR code
// see https://codeeval.dev/gist/801ad556960e59be41690d0c2fa7cba0
bool MultiFormatArchive::OpenUnrarFallback(const char* rarPath) {
    if (!rarPath) {
        return false;
    }
    CrashIf(rarFilePath_);
    auto rarPathW = ToWstrTemp(rarPath);

    str::Slice uncompressedBuf;

    RAROpenArchiveDataEx arcData = {nullptr};
    arcData.ArcNameW = (WCHAR*)rarPathW;
    arcData.OpenMode = RAR_OM_LIST;
    if (loadOnOpen) {
        arcData.OpenMode = RAR_OM_EXTRACT;
        arcData.Callback = unrarCallback;
        arcData.UserData = (LPARAM)&uncompressedBuf;
    }

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        return false;
    }

    size_t fileId = 0;
    while (true) {
        RARHeaderDataEx rarHeader = {0};
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res) {
            break;
        }

        str::TransCharsInPlace(rarHeader.FileNameW, L"\\", L"/");
        auto name = ToUtf8Temp(rarHeader.FileNameW);

        FileInfo* i = allocator_.AllocStruct<FileInfo>();
        i->fileId = fileId;
        i->fileSizeUncompressed = (size_t)rarHeader.UnpSize;
        i->filePos = 0;
        i->fileTime = (i64)rarHeader.FileTime;
        i->name = str::Dup(&allocator_, name.Get());
        i->data = nullptr;
        if (loadOnOpen) {
            // +2 so that it's zero-terminated even when interprted as WCHAR*
            i->data = AllocArray<char>(i->fileSizeUncompressed + 2);
            uncompressedBuf.Set(i->data, i->fileSizeUncompressed);
        }
        fileInfos_.Append(i);

        fileId++;

        int op = RAR_SKIP;
        if (loadOnOpen) {
            op = RAR_EXTRACT;
        }
        RARProcessFile(hArc, op, nullptr, nullptr);
    }

    RARCloseArchive(hArc);

    rarFilePath_ = str::Dup(&allocator_, rarPath);
    return true;
}

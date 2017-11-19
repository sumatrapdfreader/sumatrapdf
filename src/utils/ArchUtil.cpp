/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ArchUtil.h"

extern "C" {
#include <unarr.h>
}

// if this is defined, SumatraPDF will look for unrar.dll in
// the same directory as SumatraPDF.exe whenever a RAR archive
// fails to open or extract and uses that as a fallback
#define ENABLE_UNRARDLL_FALLBACK

#ifdef ENABLE_UNRARDLL_FALLBACK
#include "FileUtil.h"
#endif

#if OS(WIN)
FILETIME ArchFileInfo::GetWinFileTime() const {
    FILETIME ft = {(DWORD)-1, (DWORD)-1};
    LocalFileTimeToFileTime((FILETIME*)&fileTime, &ft);
    return ft;
}
#endif

// TODO: move the code into Open() function
ArchFile::ArchFile(ar_stream* data, ar_archive* (*openFormat)(ar_stream*)) : data_(data) {
    if (data_ && openFormat)
        ar_ = openFormat(data);
    if (!ar_)
        return;
    size_t fileId = 0;
    while (ar_parse_entry(ar_)) {
        const char* name = ar_entry_get_name(ar_);
        if (!name) {
            name = "";
        }

        auto* nameW = str::conv::FromUtf8(name);
        fileNames_.Append(nameW);

        ArchFileInfo* i = allocator_.AllocStruct<ArchFileInfo>();
        i->fileId = fileId;
        i->fileSizeUncompressed = ar_entry_get_size(ar_);
        i->filePos = ar_entry_get_offset(ar_);
        i->fileTime = ar_entry_get_filetime(ar_);
        i->name = allocator_.AllocString(name);
        i->nameW = nameW;
        fileInfos_.push_back(i);

        fileId++;
    }
    // extract (further) filenames with fallback in derived class constructor
    // once GetFileFromFallback has been correctly set in the vtable
}

ArchFile::~ArchFile() {
    ar_close_archive(ar_);
    ar_close(data_);
}

size_t getFileIdByName(std::vector<ArchFileInfo*>& fileInfos, const char* name) {
    for (auto fileInfo : fileInfos) {
        if (str::EqI(fileInfo->name.data(), name)) {
            return fileInfo->fileId;
        }
    }
    return (size_t)-1;
}

std::vector<ArchFileInfo*> const& ArchFile::GetFileInfos() {
    return fileInfos_;
}

size_t ArchFile::GetFileId(const WCHAR* fileName) {
    return fileNames_.FindI(fileName);
}

size_t ArchFile::GetFileId(const char* fileName) {
    return getFileIdByName(fileInfos_, fileName);
}

char* ArchFile::GetFileDataByName(const WCHAR* fileName, size_t* len) {
    return GetFileDataById(GetFileId(fileName), len);
}

char* ArchFile::GetFileDataByName(const char* fileName, size_t* len) {
    size_t fileId = getFileIdByName(fileInfos_, fileName);
    return GetFileDataById(fileId, len);
}

char* ArchFile::GetFileDataById(size_t fileId, size_t* len) {
    if (!ar_ || (fileId >= fileInfos_.size())) {
        return nullptr;
    }

    auto* fileInfo = fileInfos_[fileId];
    auto filePos = fileInfo->filePos;
    CrashIf(fileInfo->fileId != fileId);
    if (!ar_parse_entry_at(ar_, filePos))
        return GetFileFromFallback(fileId, len);

    size_t size = fileInfo->fileSizeUncompressed;
    if (size > SIZE_MAX - 3)
        return nullptr;
    AutoFree data((char*)malloc(size + 3));
    if (!data)
        return nullptr;
    if (!ar_entry_uncompress(ar_, data, size))
        return GetFileFromFallback(fileId, len);
    // zero-terminate for convenience
    data[size] = 0;
    data[size + 1] = 0;
    data[size + 2] = 0;

    if (len)
        *len = size;
    return data.StealData();
}

char* ArchFile::GetComment(size_t* len) {
    if (!ar_)
        return nullptr;
    size_t commentLen = ar_get_global_comment(ar_, nullptr, 0);
    if (0 == commentLen || (size_t)-1 == commentLen)
        return nullptr;
    AutoFree comment((char*)malloc(commentLen + 1));
    if (!comment)
        return nullptr;
    size_t read = ar_get_global_comment(ar_, comment, commentLen);
    if (read != commentLen)
        return nullptr;
    comment[commentLen] = '\0';
    if (len)
        *len = commentLen;
    return comment.StealData();
}

///// format specific handling /////

static ar_archive* ar_open_zip_archive_any(ar_stream* stream) {
    return ar_open_zip_archive(stream, false);
}
static ar_archive* ar_open_zip_archive_deflated(ar_stream* stream) {
    return ar_open_zip_archive(stream, true);
}

ArchFile* CreateZipArchive(const WCHAR* path, bool deflatedOnly) {
    auto opener = ar_open_zip_archive_any;
    if (deflatedOnly) {
        opener = ar_open_zip_archive_deflated;
    }
    return new ArchFile(ar_open_file_w(path), opener);
}

ArchFile* CreateZipArchive(IStream* stream, bool deflatedOnly) {
    auto opener = ar_open_zip_archive_any;
    if (deflatedOnly) {
        opener = ar_open_zip_archive_deflated;
    }
    return new ArchFile(ar_open_istream(stream), opener);
}

ArchFile* Create7zArchive(const WCHAR* path) {
    return new ArchFile(ar_open_file_w(path), ar_open_7z_archive);
}

ArchFile* Create7zArchive(IStream* stream) {
    return new ArchFile(ar_open_istream(stream), ar_open_7z_archive);
}

ArchFile* CreateTarArchive(const WCHAR* path) {
    return new ArchFile(ar_open_file_w(path), ar_open_tar_archive);
}

ArchFile* CreateTarArchive(IStream* stream) {
    return new ArchFile(ar_open_istream(stream), ar_open_tar_archive);
}

#ifdef ENABLE_UNRARDLL_FALLBACK
class UnRarDll {
  public:
    UnRarDll();

    bool ExtractFilenames(const WCHAR* rarPath, WStrList& filenames);
    char* GetFileByName(const WCHAR* rarPath, const WCHAR* filename, size_t* len = nullptr);
};
#else
class UnRarDll {};
#endif

RarFile::RarFile(const WCHAR* path)
    : ArchFile(ar_open_file_w(path), ar_open_rar_archive), path(str::Dup(path)), fallback(nullptr) {
    ExtractFilenamesWithFallback();
}
RarFile::RarFile(IStream* stream)
    : ArchFile(ar_open_istream(stream), ar_open_rar_archive), path(nullptr), fallback(nullptr) {
    ExtractFilenamesWithFallback();
}
RarFile::~RarFile() {
    delete fallback;
}

void RarFile::ExtractFilenamesWithFallback() {
    if (!ar_ || !ar_at_eof(ar_))
        (void)GetFileFromFallback((size_t)-1);
}

char* RarFile::GetFileFromFallback(size_t fileId, size_t* len) {
#ifdef ENABLE_UNRARDLL_FALLBACK
    // TODO: not tested yet
    CrashAlwaysIf(true);

    if (!path) {
        return nullptr;
    }

    if (!fallback)
        fallback = new UnRarDll();

    if (fileId != (size_t)-1) {
        // always use the fallback for this file from now on
        auto* e = fileInfos_.at(fileId);
        e->filePos = -1;
        return fallback->GetFileByName(path, fileNames_.at(fileId), len);
    }
    // if fileindex == -1, (re)load the entire archive listing using UnRAR
    fallback->ExtractFilenames(path, fileNames_);
    // always use the fallback for all additionally found files
#if 0
    while (filepos.size() < fileNames_.size()) {
        filepos.push_back(-1);
    }
#endif
#endif
    return nullptr;
}

#ifdef ENABLE_UNRARDLL_FALLBACK

    // the following has been extracted from UnRARDLL.exe -> unrar.h
    // publicly available from http://www.rarlab.com/rar_add.htm

#define RAR_DLL_VERSION 6
#define RAR_OM_EXTRACT 1
#define RAR_SKIP 0
#define RAR_TEST 1
#define UCM_PROCESSDATA 1

#pragma pack(1)

struct RARHeaderDataEx {
    char ArcName[1024];
    wchar_t ArcNameW[1024];
    char FileName[1024];
    wchar_t FileNameW[1024];
    unsigned int Flags;
    unsigned int PackSize;
    unsigned int PackSizeHigh;
    unsigned int UnpSize;
    unsigned int UnpSizeHigh;
    unsigned int HostOS;
    unsigned int FileCRC;
    unsigned int FileTime;
    unsigned int UnpVer;
    unsigned int Method;
    unsigned int FileAttr;
    char* CmtBuf;
    unsigned int CmtBufSize;
    unsigned int CmtSize;
    unsigned int CmtState;
    unsigned int DictSize;
    unsigned int HashType;
    char Hash[32];
    unsigned int Reserved[1014];
};

typedef int(CALLBACK* UNRARCALLBACK)(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2);

struct RAROpenArchiveDataEx {
    char* ArcName;
    wchar_t* ArcNameW;
    unsigned int OpenMode;
    unsigned int OpenResult;
    char* CmtBuf;
    unsigned int CmtBufSize;
    unsigned int CmtSize;
    unsigned int CmtState;
    unsigned int Flags;
    UNRARCALLBACK Callback;
    LPARAM UserData;
    unsigned int Reserved[28];
};

#pragma pack()

typedef int(PASCAL* RARGetDllVersionProc)();
typedef HANDLE(PASCAL* RAROpenArchiveExProc)(struct RAROpenArchiveDataEx* ArchiveData);
typedef int(PASCAL* RARReadHeaderExProc)(HANDLE hArcData, struct RARHeaderDataEx* HeaderData);
typedef int(PASCAL* RARProcessFileProc)(HANDLE hArcData, int Operation, char* DestPath, char* DestName);
typedef int(PASCAL* RARCloseArchiveProc)(HANDLE hArcData);

static RAROpenArchiveExProc RAROpenArchiveEx = nullptr;
static RARReadHeaderExProc RARReadHeaderEx = nullptr;
static RARProcessFileProc RARProcessFile = nullptr;
static RARCloseArchiveProc RARCloseArchive = nullptr;
static RARGetDllVersionProc RARGetDllVersion = nullptr;

UnRarDll::UnRarDll() {
    if (!RARGetDllVersion) {
        AutoFreeW dllPath(path::GetAppPath(L"unrar.dll"));
#ifdef _WIN64
        AutoFreeW dll64Path(path::GetAppPath(L"unrar64.dll"));
        if (file::Exists(dll64Path))
            dllPath.Set(dll64Path.StealData());
#endif
        if (!file::Exists(dllPath))
            return;
        HMODULE h = LoadLibrary(dllPath);
        if (!h)
            return;
#define LoadProcOrFail(name)                     \
    name = (name##Proc)GetProcAddress(h, #name); \
    if (!name)                                   \
    return
        LoadProcOrFail(RAROpenArchiveEx);
        LoadProcOrFail(RARReadHeaderEx);
        LoadProcOrFail(RARProcessFile);
        LoadProcOrFail(RARCloseArchive);
        // only load RARGetDllVersion if everything else has succeeded so far
        LoadProcOrFail(RARGetDllVersion);
#undef LoadProc
    }
}

bool UnRarDll::ExtractFilenames(const WCHAR* rarPath, WStrList& filenames) {
    // assume that unrar.dll is forward compatible (as indicated by its documentation)
    if (!RARGetDllVersion || RARGetDllVersion() < RAR_DLL_VERSION || !rarPath)
        return false;

    RAROpenArchiveDataEx arcData = {0};
    arcData.ArcNameW = (WCHAR*)rarPath;
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0)
        return false;

    for (size_t idx = 0;; idx++) {
        RARHeaderDataEx rarHeader;
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;
        str::TransChars(rarHeader.FileNameW, L"\\", L"/");
        if (filenames.size() == idx)
            filenames.Append(str::Dup(rarHeader.FileNameW));
        else
            CrashIf(!str::Eq(filenames.at(idx), rarHeader.FileNameW));
        RARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
    }

    RARCloseArchive(hArc);
    return true;
}

static int CALLBACK unrarCallback(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed) {
    if (UCM_PROCESSDATA != msg || !userData)
        return -1;
    str::Str<char>* data = (str::Str<char>*)userData;
    bool ok = data->AppendChecked((char*)rarBuffer, bytesProcessed);
    return ok ? 1 : -1;
}

char* UnRarDll::GetFileByName(const WCHAR* rarPath, const WCHAR* filename, size_t* len) {
    // assume that unrar.dll is forward compatible (as indicated by its documentation)
    if (!RARGetDllVersion || RARGetDllVersion() < RAR_DLL_VERSION || !rarPath)
        return false;

    str::Str<char> data;

    RAROpenArchiveDataEx arcData = {0};
    arcData.ArcNameW = (WCHAR*)rarPath;
    arcData.OpenMode = RAR_OM_EXTRACT;
    arcData.Callback = unrarCallback;
    arcData.UserData = (LPARAM)&data;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0)
        return nullptr;

    int res = 0;
    RARHeaderDataEx rarHeader;
    for (;;) {
        res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;
        str::TransChars(rarHeader.FileNameW, L"\\", L"/");
        if (str::EqI(rarHeader.FileNameW, filename))
            break;
        RARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
    }

    if (0 == res) {
        if (rarHeader.UnpSizeHigh != 0) {
            res = 1;
        } else {
            res = RARProcessFile(hArc, RAR_TEST, nullptr, nullptr);
            if (rarHeader.UnpSize != data.size()) {
                res = 1;
            }
        }
        // zero-terminate for convenience
        if (!data.AppendChecked("\0\0\0", 3))
            res = 1;
    }

    RARCloseArchive(hArc);

    if (0 != res)
        return nullptr;
    if (len)
        *len = data.size() - 2;
    return data.StealData();
}

#endif

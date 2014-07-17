/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "RarUtil.h"

// if this is defined, SumatraPDF will look for unrar.dll in
// the same directory as SumatraPDF.exe whenever a RAR archive
// fails to open or extract and uses that as a fallback
// #define ENABLE_UNRARDLL_FALLBACK

extern "C" {
#include <unarr.h>
}

#ifdef ENABLE_UNRARDLL_FALLBACK
#include "FileUtil.h"

class UnRarDll {
    ScopedMem<WCHAR> rarPath;

public:
    UnRarDll(const WCHAR *rarPath);
    ~UnRarDll() { };

    bool ExtractFilenames(WStrList& filenames);
    char *GetFileByName(const WCHAR *filename, size_t *len=NULL);
};
#else

class UnRarDll { };

#endif

RarFile::RarFile(const WCHAR *path) : path(str::Dup(path)), fallback(NULL)
{
    data = ar_open_file_w(path);
    ar = data ? ar_open_rar_archive(data) : NULL;
    ExtractFilenames();
}

RarFile::RarFile(IStream *stream) : path(NULL), fallback(NULL)
{
    data = ar_open_istream(stream);
    ar = data ? ar_open_rar_archive(data) : NULL;
    ExtractFilenames();
}

RarFile::~RarFile()
{
    ar_close_archive(ar);
    ar_close(data);
    delete fallback;
}

size_t RarFile::GetFileCount() const
{
    return filenames.Count();
}

const WCHAR *RarFile::GetFileName(size_t fileindex)
{
    if (fileindex >= filenames.Count())
        return NULL;
    return filenames.At(fileindex);
}

size_t RarFile::GetFileIndex(const WCHAR *fileName)
{
    return filenames.FindI(fileName);
}

char *RarFile::GetFileDataByName(const WCHAR *fileName, size_t *len)
{
    return GetFileDataByIdx(GetFileIndex(fileName), len);
}

void RarFile::ExtractFilenames()
{
    if (ar) {
        while (ar_parse_entry(ar)) {
            const char *name = ar_entry_get_name(ar);
            filenames.Append(str::conv::FromUtf8(name));
            filepos.Append(ar_entry_get_offset(ar));
        }
    }
    if (!ar || !ar_at_eof(ar)) {
#ifdef ENABLE_UNRARDLL_FALLBACK
        if (path) {
            fallback = new UnRarDll(path);
            fallback->ExtractFilenames(filenames);
            filepos.Reset();
            for (size_t i = 0; i < filenames.Count(); i++) {
                filepos.Append(-1);
            }
        }
#endif
    }
}

char *RarFile::GetFileDataByIdx(size_t fileindex, size_t *len)
{
    if (fileindex >= filepos.Count())
        return NULL;
#ifdef ENABLE_UNRARDLL_FALLBACK
    if (fallback && -1 == filepos.At(fileindex))
        return fallback->GetFileByName(filenames.At(fileindex), len);
#endif
    if (!ar)
        return NULL;

    if (!ar_parse_entry_at(ar, filepos.At(fileindex))) {
#ifdef ENABLE_UNRARDLL_FALLBACK
        if (path) {
            if (!fallback)
                fallback = new UnRarDll(path);
            filepos.At(fileindex) = -1;
            return fallback->GetFileByName(filenames.At(fileindex), len);
        }
#endif
        return NULL;
    }

    size_t size = ar_entry_get_size(ar);
    if (size > SIZE_MAX - 2)
        return NULL;
    ScopedMem<char> data((char *)malloc(size + 2));
    if (!data)
        return NULL;
    if (!ar_entry_uncompress(ar, data, size)) {
#ifdef ENABLE_UNRARDLL_FALLBACK
        if (path) {
            if (!fallback)
                fallback = new UnRarDll(path);
            filepos.At(fileindex) = -1;
            return fallback->GetFileByName(filenames.At(fileindex), len);
        }
#endif
        return NULL;
    }
    // zero-terminate for convenience
    data[size] = data[size + 1] = '\0';

    if (len)
        *len = size;
    return data.StealData();
}

#ifdef ENABLE_UNRARDLL_FALLBACK

// the following has been extracted from UnRARDLL.exe -> unrar.h
// publicly available from http://www.rarlab.com/rar_add.htm

#define RAR_DLL_VERSION 6
#define RAR_OM_EXTRACT  1
#define RAR_SKIP        0
#define RAR_TEST        1
#define UCM_PROCESSDATA 1

#pragma pack(1)

struct RARHeaderDataEx
{
  char         ArcName[1024];
  wchar_t      ArcNameW[1024];
  char         FileName[1024];
  wchar_t      FileNameW[1024];
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
  char         *CmtBuf;
  unsigned int CmtBufSize;
  unsigned int CmtSize;
  unsigned int CmtState;
  unsigned int DictSize;
  unsigned int HashType;
  char         Hash[32];
  unsigned int Reserved[1014];
};

typedef int (CALLBACK *UNRARCALLBACK)(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2);

struct RAROpenArchiveDataEx
{
  char         *ArcName;
  wchar_t      *ArcNameW;
  unsigned int  OpenMode;
  unsigned int  OpenResult;
  char         *CmtBuf;
  unsigned int  CmtBufSize;
  unsigned int  CmtSize;
  unsigned int  CmtState;
  unsigned int  Flags;
  UNRARCALLBACK Callback;
  LPARAM        UserData;
  unsigned int  Reserved[28];
};

#pragma pack()

typedef int     (PASCAL *RARGetDllVersionProc)();
typedef HANDLE  (PASCAL *RAROpenArchiveExProc)(struct RAROpenArchiveDataEx *ArchiveData);
typedef int     (PASCAL *RARReadHeaderExProc)(HANDLE hArcData, struct RARHeaderDataEx *HeaderData);
typedef int     (PASCAL *RARProcessFileProc)(HANDLE hArcData, int Operation, char *DestPath, char *DestName);
typedef int     (PASCAL *RARCloseArchiveProc)(HANDLE hArcData);

static RAROpenArchiveExProc RAROpenArchiveEx = NULL;
static RARReadHeaderExProc  RARReadHeaderEx = NULL;
static RARProcessFileProc   RARProcessFile = NULL;
static RARCloseArchiveProc  RARCloseArchive = NULL;
static RARGetDllVersionProc RARGetDllVersion = NULL;

UnRarDll::UnRarDll(const WCHAR *rarPath) : rarPath(str::Dup(rarPath))
{
    if (!RARGetDllVersion) {
        ScopedMem<WCHAR> dllPath(path::GetAppPath(L"unrar.dll"));
        if (!file::Exists(dllPath))
            return;
        HMODULE h = LoadLibrary(dllPath);
        if (!h)
            return;
#define LoadProcOrFail(name) name = (name ## Proc)GetProcAddress(h, #name); if (!name) return
        LoadProcOrFail(RAROpenArchiveEx);
        LoadProcOrFail(RARReadHeaderEx);
        LoadProcOrFail(RARProcessFile);
        LoadProcOrFail(RARCloseArchive);
        // only load RARGetDllVersion if everything else has succeeded so far
        LoadProcOrFail(RARGetDllVersion);
#undef LoadProc
    }
}

bool UnRarDll::ExtractFilenames(WStrList &filenames)
{
    if (!RARGetDllVersion || RARGetDllVersion() != RAR_DLL_VERSION)
        return false;

    RAROpenArchiveDataEx arcData = { 0 };
    arcData.ArcNameW = rarPath;
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0)
        return false;

    for (size_t idx = 0; ; idx++) {
        RARHeaderDataEx rarHeader;
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;
        str::TransChars(rarHeader.FileNameW, L"\\", L"/");
        if (filenames.Count() == idx)
            filenames.Append(str::Dup(rarHeader.FileNameW));
        else
            CrashIf(!str::Eq(filenames.At(idx), rarHeader.FileNameW));
        RARProcessFile(hArc, RAR_SKIP, NULL, NULL);
    }

    RARCloseArchive(hArc);
    return true;
}

static int CALLBACK unrarCallback(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed)
{
    if (UCM_PROCESSDATA != msg || !userData)
        return -1;
    str::Str<char> *data = (str::Str<char> *)userData;
    bool ok = data->AppendChecked((char *)rarBuffer, bytesProcessed);
    return ok ? 1 : -1;
}

char *UnRarDll::GetFileByName(const WCHAR *filename, size_t *len)
{
    if (!RARGetDllVersion || RARGetDllVersion() != RAR_DLL_VERSION)
        return false;

    str::Str<char> data;

    RAROpenArchiveDataEx arcData = { 0 };
    arcData.ArcNameW = rarPath;
    arcData.OpenMode = RAR_OM_EXTRACT;
    arcData.Callback = unrarCallback;
    arcData.UserData = (LPARAM)&data;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0)
        return NULL;

    int res = 0;
    RARHeaderDataEx rarHeader;
    for (;;) {
        res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;
        str::TransChars(rarHeader.FileNameW, L"\\", L"/");
        if (str::EqI(rarHeader.FileNameW, filename))
            break;
        RARProcessFile(hArc, RAR_SKIP, NULL, NULL);
    }

    if (0 == res) {
        if (rarHeader.UnpSizeHigh != 0) {
            res = 1;
        }
        else {
            res = RARProcessFile(hArc, RAR_TEST, NULL, NULL);
            if (rarHeader.UnpSize != data.Size()) {
                res = 1;
            }
        }
        // zero-terminate for convenience
        data.Append("\0\0", 2);
    }

    RARCloseArchive(hArc);

    if (0 != res)
        return NULL;
    if (len)
        *len = data.Size() - 2;
    return data.StealData();
}

#endif

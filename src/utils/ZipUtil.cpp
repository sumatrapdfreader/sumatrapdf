/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "ZipUtil.h"

#include "ByteWriter.h"
#include "DirIter.h"
#include "FileUtil.h"

#ifdef USE_UNARR_AS_UNZIP
extern "C" {
#include <unarr.h>
}
#else
#include <ioapi.h>
#include <iowin32.h>
#include <iowin32s.h>
#endif
#include <zlib.h>

ZipFile::ZipFile(const WCHAR *path, bool deflatedOnly, Allocator *allocator) :
    filenames(0, allocator), fileinfo(0, allocator), filepos(0, allocator),
    allocator(allocator), commentLen(0)
{
#ifdef USE_UNARR_AS_UNZIP
    data = ar_open_file_w(path);
    ar = data ? ar_open_zip_archive(data, deflatedOnly) : NULL;
    if (ar)
        ExtractFilenames(deflatedOnly);
#else
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    uf = unzOpen2_64(path, &ffunc);
    if (uf)
        ExtractFilenames(deflatedOnly);
#endif
}

ZipFile::ZipFile(IStream *stream, bool deflatedOnly, Allocator *allocator) :
    filenames(0, allocator), fileinfo(0, allocator), filepos(0, allocator),
    allocator(allocator), commentLen(0)
{
#ifdef USE_UNARR_AS_UNZIP
    data = ar_open_istream(stream);
    ar = data ? ar_open_zip_archive(data, deflatedOnly) : NULL;
    if (ar)
        ExtractFilenames(deflatedOnly);
#else
    zlib_filefunc64_def ffunc;
    fill_win32s_filefunc64(&ffunc);
    uf = unzOpen2_64(stream, &ffunc);
    if (uf)
        ExtractFilenames(deflatedOnly);
#endif
}

ZipFile::~ZipFile()
{
#ifdef USE_UNARR_AS_UNZIP
    ar_close_archive(ar);
    ar_close(data);
#else
    unzClose(uf);
#endif
}

// cf. http://www.pkware.com/documents/casestudies/APPNOTE.TXT Appendix D
#define CP_ZIP 437

#define INVALID_ZIP_FILE_POS ((ZPOS64_T)-1)

void ZipFile::ExtractFilenames(bool deflatedOnly)
{
#ifdef USE_UNARR_AS_UNZIP
    while (ar_parse_entry(ar)) {
        const WCHAR *name = ar_entry_get_name_w(ar);
        filenames.Append(Allocator::StrDup(allocator, name));
        filepos.Append(ar_entry_get_offset(ar));
    }
#else
    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK)
        return;
    unzGoToFirstFile(uf);

    for (int i = 0; i < ginfo.number_entry && UNZ_OK == err; i++) {
        unz_file_info64 finfo;
        char fileName[MAX_PATH];
        err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
        // some file format specifications only allow Deflate as compression method (e.g. XPS and EPUB)
        bool isDeflated = (0 == finfo.compression_method) || (Z_DEFLATED == finfo.compression_method);
        if (err == UNZ_OK && (isDeflated || !deflatedOnly)) {
            WCHAR fileNameW[MAX_PATH];
            UINT cp = (finfo.flag & (1 << 11)) ? CP_UTF8 : CP_ZIP;
            str::conv::FromCodePageBuf(fileNameW, dimof(fileNameW), fileName, cp);
            filenames.Append(Allocator::StrDup(allocator, fileNameW));
            fileinfo.Append(finfo);

            unz64_file_pos fpos;
            err = unzGetFilePos64(uf, &fpos);
            if (err != UNZ_OK)
                fpos.num_of_file = INVALID_ZIP_FILE_POS;
            filepos.Append(fpos);
        }
        err = unzGoToNextFile(uf);
    }
    commentLen = ginfo.size_comment;
#endif
}

size_t ZipFile::GetFileIndex(const WCHAR *fileName)
{
    return filenames.FindI(fileName);
}

size_t ZipFile::GetFileCount() const
{
#ifndef USE_UNARR_AS_UNZIP
    CrashIf(filenames.Count() != fileinfo.Count());
#endif
    CrashIf(filenames.Count() != filepos.Count());
    return filenames.Count();
}

const WCHAR *ZipFile::GetFileName(size_t fileindex)
{
    if (fileindex >= filenames.Count())
        return NULL;
    return filenames.At(fileindex);
}

char *ZipFile::GetFileDataByName(const WCHAR *fileName, size_t *len)
{
    return GetFileDataByIdx(GetFileIndex(fileName), len);
}

char *ZipFile::GetFileDataByIdx(size_t fileindex, size_t *len)
{
#ifdef USE_UNARR_AS_UNZIP
    if (!ar)
        return NULL;
    if (fileindex >= filenames.Count())
        return NULL;

    if (!ar_parse_entry_at(ar, filepos.At(fileindex)))
        return NULL;

    size_t size = ar_entry_get_size(ar);
    if (size > SIZE_MAX - 2)
        return NULL;
    char *data = (char *)Allocator::Alloc(allocator, size + 2);
    if (!data)
        return NULL;
    if (!ar_entry_uncompress(ar, data, size)) {
        Allocator::Free(allocator, data);
        return NULL;
    }
    // zero-terminate for convenience
    data[size] = data[size + 1] = '\0';

    if (len)
        *len = size;
    return data;
#else
    if (!uf)
        return NULL;
    if (fileindex >= filenames.Count())
        return NULL;

    int err = -1;
    if (filepos.At(fileindex).num_of_file != INVALID_ZIP_FILE_POS)
        err = unzGoToFilePos64(uf, &filepos.At(fileindex));
    if (err != UNZ_OK) {
        char fileNameA[MAX_PATH];
        UINT cp = (fileinfo.At(fileindex).flag & (1 << 11)) ? CP_UTF8 : CP_ZIP;
        str::conv::ToCodePageBuf(fileNameA, dimof(fileNameA), filenames.At(fileindex), cp);
        err = unzLocateFile(uf, fileNameA, 0);
    }
    if (err != UNZ_OK)
        return NULL;
    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return NULL;

    unsigned int len2 = (unsigned int)fileinfo.At(fileindex).uncompressed_size;
    // overflow check
    if (len2 != fileinfo.At(fileindex).uncompressed_size ||
        len2 > UINT_MAX - sizeof(WCHAR) ||
        len2 / 1024 > fileinfo.At(fileindex).compressed_size) {
        unzCloseCurrentFile(uf);
        return NULL;
    }

    char *result = (char *)Allocator::Alloc(allocator, len2 + sizeof(WCHAR));
    if (result) {
        unsigned int readBytes = unzReadCurrentFile(uf, result, len2);
        // zero-terminate for convenience
        result[len2] = result[len2 + 1] = '\0';
        if (readBytes != len2) {
            Allocator::Free(allocator, result);
            result = NULL;
        }
        else if (len) {
            *len = len2;
        }
    }

    err = unzCloseCurrentFile(uf);
    if (err != UNZ_OK) {
        // CRC mismatch, file content is likely damaged
        Allocator::Free(allocator, result);
        result = NULL;
    }

    return result;
#endif
}

FILETIME ZipFile::GetFileTime(const WCHAR *fileName)
{
    return GetFileTime(GetFileIndex(fileName));
}

FILETIME ZipFile::GetFileTime(size_t fileindex)
{
    FILETIME ft = { (DWORD)-1, (DWORD)-1 };
#ifdef USE_UNARR_AS_UNZIP
    if (ar && fileindex < filepos.Count() && ar_parse_entry_at(ar, filepos.At(fileindex))) {
        FILETIME ftLocal;
        time64_t filetime = ar_entry_get_filetime(ar);
        ftLocal.dwLowDateTime = (DWORD)filetime;
        ftLocal.dwHighDateTime = (DWORD)((filetime >> 32) & 0xFFFFFFFF);
        LocalFileTimeToFileTime(&ftLocal, &ft);
    }
#else
    if (uf && fileindex < fileinfo.Count()) {
        FILETIME ftLocal;
        DWORD dosDate = fileinfo.At(fileindex).dosDate;
        DosDateTimeToFileTime(HIWORD(dosDate), LOWORD(dosDate), &ftLocal);
        LocalFileTimeToFileTime(&ftLocal, &ft);
    }
#endif
    return ft;
}

char *ZipFile::GetComment(size_t *len)
{
#ifdef USE_UNARR_AS_UNZIP
    if (!ar)
        return NULL;
    commentLen = ar_get_global_comment(ar, NULL, 0);
    char *comment = (char *)Allocator::Alloc(allocator, commentLen + 1);
    if (!comment)
        return NULL;
    size_t read = ar_get_global_comment(ar, comment, commentLen);
#else
    if (!uf)
        return NULL;
    char *comment = (char *)Allocator::Alloc(allocator, commentLen + 1);
    if (!comment)
        return NULL;
    uLong read = (uLong)unzGetGlobalComment(uf, comment, commentLen);
#endif
    if (read != commentLen) {
        Allocator::Free(allocator, comment);
        return NULL;
    }
    comment[commentLen] = '\0';
    if (len)
        *len = commentLen;
    return comment;
}

bool ZipFile::UnzipFile(const WCHAR *fileName, const WCHAR *dir, const WCHAR *unzippedName)
{
    size_t len;
    char *data = GetFileDataByName(fileName, &len);
    if (!data)
        return false;

    str::Str<WCHAR> filePath(MAX_PATH * 2, allocator);
    filePath.Append(dir);
    if (!str::EndsWith(filePath.Get(), L"\\"))
        filePath.Append(L"\\");
    if (unzippedName) {
        filePath.Append(unzippedName);
    } else {
        filePath.Append(fileName);
        str::TransChars(filePath.Get(), L"/", L"\\");
    }

    bool ok = file::WriteAll(filePath.Get(), data, len);
    Allocator::Free(allocator, data);
    return ok;
}


static uint32_t zip_deflate(void *dst, uint32_t dstlen, const void *src, uint32_t srclen)
{
    z_stream stream = { 0 };
    stream.next_in = (Bytef *)src;
    stream.avail_in = srclen;
    stream.next_out = (Bytef *)dst;
    stream.avail_out = dstlen;

    uint32_t newdstlen = 0;
    int err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (err != Z_OK)
        return 0;
    err = deflate(&stream, Z_FINISH);
    if (Z_STREAM_END == err)
        newdstlen = stream.total_out;
    err = deflateEnd(&stream);
    if (err != Z_OK)
        return 0;
    return newdstlen;
}

bool ZipCreator::AddFileData(const char *nameUtf8, const void *data, size_t size, uint32_t dosdate)
{
    CrashIf(size >= UINT32_MAX);
    CrashIf(fileCount >= UINT16_MAX);
    CrashIf(str::Len(nameUtf8) >= UINT16_MAX);
    if (size >= UINT32_MAX)
        return false;
    if (fileCount >= UINT16_MAX - 1)
        return false;

    size_t fileOffset = filedata.Size();
    uint16_t flags = (1 << 11); // filename is UTF-8
    uInt crc = crc32(0, (const Bytef *)data, (uInt)size);
    size_t namelen = str::Len(nameUtf8);
    if (namelen >= UINT16_MAX)
        return false;

    uint16_t method = Z_DEFLATED;
    uLongf compressedSize = size;
    ScopedMem<char> compressed((char *)malloc(size));
    if (!compressed)
        return false;
    compressedSize = zip_deflate(compressed, size, data, size);
    if (!compressedSize) {
        method = 0; // Store
        memcpy(compressed.Get(), data, size);
        compressedSize = size;
    }

    ByteWriterLE local(filedata.AppendBlanks(30), 30);
    local.Write32(0x04034B50); // signature
    local.Write16(20); // version needed to extract
    local.Write16(flags);
    local.Write16(method);
    local.Write32(dosdate);
    local.Write32(crc);
    local.Write32(compressedSize);
    local.Write32((uint32_t)size);
    local.Write16((uint16_t)namelen);
    local.Write16(0); // extra field length
    filedata.Append(nameUtf8, namelen);

    bool ok = filedata.AppendChecked(compressed, compressedSize);
    if (!ok || filedata.Size() >= UINT32_MAX) {
        filedata.RemoveAt(fileOffset, filedata.Size() - fileOffset);
        return false;
    }

    ByteWriterLE central(centraldir.AppendBlanks(46), 46);
    central.Write32(0x02014B50); // signature
    central.Write16(20); // version made by
    central.Write16(20); // version needed to extract
    central.Write16(flags);
    central.Write16(method);
    central.Write32(dosdate);
    central.Write32(crc);
    central.Write32(compressedSize);
    central.Write32((uint32_t)size);
    central.Write16((uint16_t)namelen);
    central.Write16(0); // extra field length
    central.Write16(0); // file comment length
    central.Write16(0); // disk number
    central.Write16(0); // internal file attributes
    central.Write32(0); // external file attributes
    central.Write32(fileOffset);
    centraldir.Append(nameUtf8, namelen);

    fileCount++;

    return true;
}

void ZipCreator::CreateEOCD(char eocdData[22])
{
    ByteWriterLE eocd(eocdData, 22);
    eocd.Write32(0x06054B50); // signature
    eocd.Write16(0); // disk number
    eocd.Write16(0); // disk number of central directory
    eocd.Write16((uint16_t)fileCount);
    eocd.Write16((uint16_t)fileCount);
    eocd.Write32(centraldir.Size());
    eocd.Write32(filedata.Size());
    eocd.Write16(0); // comment len
}

// add a given file under (optional) nameInZip
bool ZipCreator::AddFile(const WCHAR *filePath, const WCHAR *nameInZip)
{
    size_t filelen;
    ScopedMem<char> filedata(file::ReadAll(filePath, &filelen));
    if (!filedata)
        return false;

    uint32_t dosdatetime = 0;
    FILETIME ft = file::GetModificationTime(filePath);
    if (ft.dwLowDateTime || ft.dwHighDateTime) {
        FILETIME ftLocal;
        WORD dosDate, dosTime;
        if (FileTimeToLocalFileTime(&ft, &ftLocal) &&
            FileTimeToDosDateTime(&ftLocal, &dosDate, &dosTime)) {
            dosdatetime = MAKELONG(dosTime, dosDate);
        }
    }

    if (!nameInZip)
        nameInZip = path::IsAbsolute(filePath) ? path::GetBaseName(filePath) : filePath;
    ScopedMem<char> nameUtf8(str::conv::ToUtf8(nameInZip));
    str::TransChars(nameUtf8, "\\", "/");

    return AddFileData(nameUtf8, filedata, filelen, dosdatetime);
}

// we use the filePath relative to dir as the zip name
bool ZipCreator::AddFileFromDir(const WCHAR *filePath, const WCHAR *dir)
{
    if (str::IsEmpty(dir) || !str::StartsWith(filePath, dir))
        return false;
    const WCHAR *nameInZip = filePath + str::Len(dir) + 1;
    if (!path::IsSep(nameInZip[-1]))
        return false;
    return AddFile(filePath, nameInZip);
}

bool ZipCreator::AddDir(const WCHAR *dirPath, bool recursive)
{
    DirIter di(dirPath, recursive);
    for (const WCHAR *filePath = di.First(); filePath; filePath = di.Next()) {
        if (!AddFileFromDir(filePath, dirPath))
            return false;
    }
    return true;
}

bool ZipCreator::SaveTo(const WCHAR *zipFilePath)
{
    char endOfCentralDir[22];
    CreateEOCD(endOfCentralDir);

    ScopedHandle h(CreateFile(zipFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
    if (INVALID_HANDLE_VALUE == h)
        return false;
    DWORD written;
    BOOL ok = WriteFile(h, filedata.Get(), (DWORD)filedata.Size(), &written, NULL);
    if (!ok || written != filedata.Size())
        return false;
    ok = WriteFile(h, centraldir.Get(), (DWORD)centraldir.Size(), &written, NULL);
    if (!ok || written != centraldir.Size())
        return false;
    ok = WriteFile(h, endOfCentralDir, sizeof(endOfCentralDir), &written, NULL);
    if (!ok || written != sizeof(endOfCentralDir))
        return false;

    return true;
}

bool ZipCreator::SaveTo(IStream *stream)
{
    char endOfCentralDir[22];
    CreateEOCD(endOfCentralDir);

    ULONG written;
    HRESULT res = stream->Write(filedata.Get(), (ULONG)filedata.Size(), &written);
    if (FAILED(res) || (size_t)written != filedata.Size())
        return false;
    res = stream->Write(centraldir.Get(), (ULONG)centraldir.Size(), &written);
    if (FAILED(res) || (size_t)written != centraldir.Size())
        return false;
    res = stream->Write(endOfCentralDir, sizeof(endOfCentralDir), &written);
    if (FAILED(res) || written != sizeof(endOfCentralDir))
        return false;

    return true;
}

IStream *OpenDirAsZipStream(const WCHAR *dirPath, bool recursive)
{
    if (!dir::Exists(dirPath))
        return NULL;

    ScopedComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream)))
        return NULL;

    ZipCreator zc;
    if (!zc.AddDir(dirPath, recursive))
        return NULL;
    if (!zc.SaveTo(stream))
        return NULL;

    stream->AddRef();
    return stream;
}

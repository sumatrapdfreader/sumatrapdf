/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "ZipUtil.h"

#include "ByteWriter.h"
#include "DirIter.h"
#include "FileUtil.h"

extern "C" {
#include <unarr.h>
#include <zlib.h>
}

ZipFileAlloc::ZipFileAlloc(const WCHAR *path, bool deflatedOnly, Allocator *allocator) :
    ar(NULL), filenames(0, allocator), filepos(0, allocator), allocator(allocator)
{
    data = ar_open_file_w(path);
    if (data)
        ar = ar_open_zip_archive(data, deflatedOnly);
    ExtractFilenames();
}

ZipFileAlloc::ZipFileAlloc(IStream *stream, bool deflatedOnly, Allocator *allocator) :
    ar(NULL), filenames(0, allocator), filepos(0, allocator), allocator(allocator)
{
    data = ar_open_istream(stream);
    if (data)
        ar = ar_open_zip_archive(data, deflatedOnly);
    ExtractFilenames();
}

ZipFileAlloc::~ZipFileAlloc()
{
    ar_close_archive(ar);
    ar_close(data);
}

void ZipFileAlloc::ExtractFilenames()
{
    if (!ar)
        return;
    while (ar_parse_entry(ar)) {
        const char *nameUtf8 = ar_entry_get_name(ar);
        if (nameUtf8) {
            int len = MultiByteToWideChar(CP_UTF8, 0, nameUtf8, -1, NULL, 0);
            WCHAR *name = Allocator::Alloc<WCHAR>(allocator, len);
            str::Utf8ToWcharBuf(nameUtf8, str::Len(nameUtf8), name, len);
            filenames.Append(name);
        }
        else
            filenames.Append(NULL);
        filepos.Append(ar_entry_get_offset(ar));
    }
}

size_t ZipFileAlloc::GetFileIndex(const WCHAR *fileName)
{
    return filenames.FindI(fileName);
}

size_t ZipFileAlloc::GetFileCount() const
{
    CrashIf(filenames.Count() != filepos.Count());
    return filenames.Count();
}

const WCHAR *ZipFileAlloc::GetFileName(size_t fileindex)
{
    if (fileindex >= filenames.Count())
        return NULL;
    return filenames.At(fileindex);
}

char *ZipFileAlloc::GetFileDataByName(const WCHAR *fileName, size_t *len)
{
    return GetFileDataByIdx(GetFileIndex(fileName), len);
}

char *ZipFileAlloc::GetFileDataByIdx(size_t fileindex, size_t *len)
{
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
}

FILETIME ZipFileAlloc::GetFileTime(const WCHAR *fileName)
{
    return GetFileTime(GetFileIndex(fileName));
}

FILETIME ZipFileAlloc::GetFileTime(size_t fileindex)
{
    FILETIME ft = { (DWORD)-1, (DWORD)-1 };
    if (ar && fileindex < filepos.Count() && ar_parse_entry_at(ar, filepos.At(fileindex))) {
        time64_t filetime = ar_entry_get_filetime(ar);
        LocalFileTimeToFileTime((FILETIME *)&filetime, &ft);
    }
    return ft;
}

char *ZipFileAlloc::GetComment(size_t *len)
{
    if (!ar)
        return NULL;
    size_t commentLen = ar_get_global_comment(ar, NULL, 0);
    char *comment = (char *)Allocator::Alloc(allocator, commentLen + 1);
    if (!comment)
        return NULL;
    size_t read = ar_get_global_comment(ar, comment, commentLen);
    if (read != commentLen) {
        Allocator::Free(allocator, comment);
        return NULL;
    }
    comment[commentLen] = '\0';
    if (len)
        *len = commentLen;
    return comment;
}

bool ZipFileAlloc::UnzipFile(const WCHAR *fileName, const WCHAR *dir, const WCHAR *unzippedName)
{
    size_t len;
    char *data = GetFileDataByName(fileName, &len);
    if (!data)
        return false;

    str::Str<WCHAR> filePath(MAX_PATH, allocator);
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

/***** ZipCreator *****/

class FileWriteStream : public ISequentialStream {
    HANDLE hFile;
    LONG refCount;
public:
    FileWriteStream(const WCHAR *filePath) : refCount(1) {
        hFile = CreateFile(filePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    virtual ~FileWriteStream() {
        CloseHandle(hFile);
    }
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        static const QITAB qit[] = { QITABENT(FileWriteStream, ISequentialStream), { 0 } };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&refCount);
    }
    IFACEMETHODIMP_(ULONG) Release() {
        LONG newCount = InterlockedDecrement(&refCount);
        if (newCount == 0)
            delete this;
        return newCount;
    }
    // ISequentialStream
    IFACEMETHODIMP Read(void *buffer, ULONG size, ULONG *read) {
        return E_NOTIMPL;
    }
    IFACEMETHODIMP Write(const void *data, ULONG size, ULONG *written) {
        bool ok = WriteFile(hFile, data, size, written, NULL);
        return ok && *written == size ? S_OK : E_FAIL;
    }
};

ZipCreator::ZipCreator(const WCHAR *zipFilePath) : bytesWritten(0), fileCount(0)
{
    stream = new FileWriteStream(zipFilePath);
}

ZipCreator::ZipCreator(ISequentialStream *stream) : bytesWritten(0), fileCount(0)
{
    stream->AddRef();
    this->stream = stream;
}

ZipCreator::~ZipCreator()
{
    stream->Release();
}

bool ZipCreator::WriteData(const void *data, size_t size)
{
    ULONG written = 0;
    HRESULT res = stream->Write(data, (ULONG)size, &written);
    bytesWritten += written;
    return SUCCEEDED(res) && written == size;
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
    CrashIf(str::Len(nameUtf8) >= UINT16_MAX);
    if (size >= UINT32_MAX)
        return false;

    size_t fileOffset = bytesWritten;
    uint16_t flags = (1 << 11); // filename is UTF-8
    uInt crc = crc32(0, (const Bytef *)data, (uInt)size);
    size_t namelen = str::Len(nameUtf8);
    if (namelen >= UINT16_MAX)
        return false;

    uint16_t method = Z_DEFLATED;
    uLongf compressedSize = (uint32_t)size;
    ScopedMem<char> compressed((char *)malloc(size));
    if (!compressed)
        return false;
    compressedSize = zip_deflate(compressed, (uint32_t)size, data, (uint32_t)size);
    if (!compressedSize) {
        method = 0; // Store
        memcpy(compressed.Get(), data, size);
        compressedSize = (uint32_t)size;
    }

    char localHeader[30];
    ByteWriterLE local(localHeader, sizeof(localHeader));
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

    bool ok = WriteData(localHeader, sizeof(localHeader)) &&
              WriteData(nameUtf8, namelen) &&
              WriteData(compressed, compressedSize);

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
    central.Write32((uint32_t)fileOffset);
    centraldir.Append(nameUtf8, namelen);

    fileCount++;
    return ok;
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

bool ZipCreator::Finish()
{
    CrashIf(bytesWritten >= UINT32_MAX);
    CrashIf(fileCount >= UINT16_MAX);
    if (bytesWritten >= UINT32_MAX || fileCount >= UINT16_MAX)
        return false;

    char endOfCentralDir[22];
    ByteWriterLE eocd(endOfCentralDir, sizeof(endOfCentralDir));
    eocd.Write32(0x06054B50); // signature
    eocd.Write16(0); // disk number
    eocd.Write16(0); // disk number of central directory
    eocd.Write16((uint16_t)fileCount);
    eocd.Write16((uint16_t)fileCount);
    eocd.Write32((uint32_t)centraldir.Size());
    eocd.Write32((uint32_t)bytesWritten);
    eocd.Write16(0); // comment len

    return WriteData(centraldir.Get(), centraldir.Size()) &&
           WriteData(endOfCentralDir, sizeof(endOfCentralDir));
}

IStream *OpenDirAsZipStream(const WCHAR *dirPath, bool recursive)
{
    if (!dir::Exists(dirPath))
        return NULL;

    ScopedComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream)))
        return NULL;

    ZipCreator zc(stream);
    if (!zc.AddDir(dirPath, recursive))
        return NULL;
    if (!zc.Finish())
        return NULL;

    stream->AddRef();
    return stream;
}

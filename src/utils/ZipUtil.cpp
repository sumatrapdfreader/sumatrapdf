/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define __STDC_LIMIT_MACROS
#include "utils/BaseUtil.h"
#include "utils/ZipUtil.h"

#include "utils/ByteWriter.h"
#include "utils/ScopedWin.h"
#include "utils/DirIter.h"
#include "utils/FileUtil.h"

extern "C" {
#include <unarr.h>
#include <zlib.h>
}

/***** ZipCreator *****/

class FileWriteStream : public ISequentialStream {
    HANDLE hFile;
    LONG refCount;

  public:
    FileWriteStream(const WCHAR* filePath) : refCount(1) {
        hFile = CreateFile(filePath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    }
    virtual ~FileWriteStream() {
        CloseHandle(hFile);
    }
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(FileWriteStream, ISequentialStream), {0}};
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
    IFACEMETHODIMP Read(void* buffer, ULONG size, ULONG* read) {
        UNUSED(buffer);
        UNUSED(size);
        UNUSED(read);
        return E_NOTIMPL;
    }
    IFACEMETHODIMP Write(const void* data, ULONG size, ULONG* written) {
        bool ok = WriteFile(hFile, data, size, written, nullptr);
        return ok && *written == size ? S_OK : E_FAIL;
    }
};

ZipCreator::ZipCreator(const WCHAR* zipFilePath) : bytesWritten(0), fileCount(0) {
    stream = new FileWriteStream(zipFilePath);
}

ZipCreator::ZipCreator(ISequentialStream* stream) : bytesWritten(0), fileCount(0) {
    stream->AddRef();
    this->stream = stream;
}

ZipCreator::~ZipCreator() {
    stream->Release();
}

bool ZipCreator::WriteData(const void* data, size_t size) {
    ULONG written = 0;
    HRESULT res = stream->Write(data, (ULONG)size, &written);
    if (FAILED(res) || written != size)
        return false;
    bytesWritten += written;
    return true;
}

static uint32_t zip_compress(void* dst, uint32_t dstlen, const void* src, uint32_t srclen) {
    z_stream stream = {0};
    stream.next_in = (Bytef*)src;
    stream.avail_in = srclen;
    stream.next_out = (Bytef*)dst;
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

bool ZipCreator::AddFileData(const char* nameUtf8, const void* data, size_t size, uint32_t dosdate) {
    CrashIf(size >= UINT32_MAX);
    CrashIf(str::Len(nameUtf8) >= UINT16_MAX);
    if (size >= UINT32_MAX)
        return false;

    size_t fileOffset = bytesWritten;
    uint16_t flags = (1 << 11); // filename is UTF-8
    uInt crc = crc32(0, (const Bytef*)data, (uInt)size);
    size_t namelen = str::Len(nameUtf8);
    if (namelen >= UINT16_MAX)
        return false;

    uint16_t method = Z_DEFLATED;
    uLongf compressedSize = (uint32_t)size;
    AutoFree compressed((char*)malloc(size));
    if (!compressed)
        return false;
    compressedSize = zip_compress(compressed, (uint32_t)size, data, (uint32_t)size);
    if (!compressedSize) {
        method = 0; // Store
        memcpy(compressed.Get(), data, size);
        compressedSize = (uint32_t)size;
    }

    char localHeader[30];
    ByteWriter local = MakeByteWriterLE(localHeader, sizeof(localHeader));
    local.Write32(0x04034B50); // signature
    local.Write16(20);         // version needed to extract
    local.Write16(flags);
    local.Write16(method);
    local.Write32(dosdate);
    local.Write32(crc);
    local.Write32(compressedSize);
    local.Write32((uint32_t)size);
    local.Write16((uint16_t)namelen);
    local.Write16(0); // extra field length

    bool ok = WriteData(localHeader, sizeof(localHeader)) && WriteData(nameUtf8, namelen) &&
              WriteData(compressed, compressedSize);

    ByteWriter central = MakeByteWriterLE(centraldir.AppendBlanks(46), 46);
    central.Write32(0x02014B50); // signature
    central.Write16(20);         // version made by
    central.Write16(20);         // version needed to extract
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
bool ZipCreator::AddFile(const WCHAR* filePath, const WCHAR* nameInZip) {
    AutoFree fileData = file::ReadFile(filePath);
    if (!fileData.data) {
        return false;
    }

    uint32_t dosdatetime = 0;
    FILETIME ft = file::GetModificationTime(filePath);
    if (ft.dwLowDateTime || ft.dwHighDateTime) {
        FILETIME ftLocal;
        WORD dosDate, dosTime;
        if (FileTimeToLocalFileTime(&ft, &ftLocal) && FileTimeToDosDateTime(&ftLocal, &dosDate, &dosTime)) {
            dosdatetime = MAKELONG(dosTime, dosDate);
        }
    }

    if (!nameInZip) {
        nameInZip = path::IsAbsolute(filePath) ? path::GetBaseNameNoFree(filePath) : filePath;
    }

    AutoFree nameUtf8 = strconv::WstrToUtf8(nameInZip);
    str::TransChars(nameUtf8.get(), "\\", "/");

    return AddFileData(nameUtf8.get(), fileData.get(), fileData.size(), dosdatetime);
}

// we use the filePath relative to dir as the zip name
bool ZipCreator::AddFileFromDir(const WCHAR* filePath, const WCHAR* dir) {
    if (str::IsEmpty(dir) || !str::StartsWith(filePath, dir))
        return false;
    const WCHAR* nameInZip = filePath + str::Len(dir) + 1;
    if (!path::IsSep(nameInZip[-1]))
        return false;
    return AddFile(filePath, nameInZip);
}

bool ZipCreator::AddDir(const WCHAR* dirPath, bool recursive) {
    DirIter di(dirPath, recursive);
    for (const WCHAR* filePath = di.First(); filePath; filePath = di.Next()) {
        if (!AddFileFromDir(filePath, dirPath))
            return false;
    }
    return true;
}

bool ZipCreator::Finish() {
    CrashIf(bytesWritten >= UINT32_MAX);
    CrashIf(fileCount >= UINT16_MAX);
    if (bytesWritten >= UINT32_MAX || fileCount >= UINT16_MAX)
        return false;

    char endOfCentralDir[22];
    ByteWriter eocd = MakeByteWriterLE(endOfCentralDir, sizeof(endOfCentralDir));
    eocd.Write32(0x06054B50); // signature
    eocd.Write16(0);          // disk number
    eocd.Write16(0);          // disk number of central directory
    eocd.Write16((uint16_t)fileCount);
    eocd.Write16((uint16_t)fileCount);
    eocd.Write32((uint32_t)centraldir.size());
    eocd.Write32((uint32_t)bytesWritten);
    eocd.Write16(0); // comment len

    return WriteData(centraldir.Get(), centraldir.size()) && WriteData(endOfCentralDir, sizeof(endOfCentralDir));
}

IStream* OpenDirAsZipStream(const WCHAR* dirPath, bool recursive) {
    if (!dir::Exists(dirPath))
        return nullptr;

    ScopedComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)))
        return nullptr;

    ZipCreator zc(stream);
    if (!zc.AddDir(dirPath, recursive))
        return nullptr;
    if (!zc.Finish())
        return nullptr;

    stream->AddRef();
    return stream;
}

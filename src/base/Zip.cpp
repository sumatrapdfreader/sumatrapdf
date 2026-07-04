/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define __STDC_LIMIT_MACROS
#include "base/Base.h"
#include "base/Zip.h"

#include "base/ByteWriter.h"
#include "base/ScopedWin.h"
#include "base/DirIter.h"
#include "base/File.h"

extern "C" {
#include <zlib.h>
}

/***** ZipCreator *****/

class FileWriteStream : public ISequentialStream {
    HANDLE hFile;
    LONG refCount;

  public:
    explicit FileWriteStream(Str filePath) : refCount(1) {
        WCHAR* path = CWStrTemp(filePath);
        hFile =
            CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    virtual ~FileWriteStream() { CloseHandle(hFile); }
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(FileWriteStream, ISequentialStream), {nullptr}};
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&refCount); }
    IFACEMETHODIMP_(ULONG) Release() {
        LONG newCount = InterlockedDecrement(&refCount);
        if (newCount == 0) {
            delete this;
        }
        return newCount;
    }
    // ISequentialStream
    IFACEMETHODIMP Read(void*, ULONG, ULONG*) { return E_NOTIMPL; }
    IFACEMETHODIMP Write(const void* data, ULONG size, ULONG* written) {
        bool ok = WriteFile(hFile, data, size, written, nullptr);
        return ok && *written == size ? S_OK : E_FAIL;
    }
};

ZipCreator::ZipCreator(Str zipFilePath) : bytesWritten(0), fileCount(0) {
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
    if (FAILED(res) || written != size) {
        return false;
    }
    bytesWritten += written;
    return true;
}

static u32 zip_compress(void* dst, u32 dstlen, const void* src, u32 srclen) {
    z_stream stream = {nullptr};
    stream.next_in = (Bytef*)src;
    stream.avail_in = srclen;
    stream.next_out = (Bytef*)dst;
    stream.avail_out = dstlen;

    u32 newdstlen = 0;
    int err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (err != Z_OK) {
        return 0;
    }
    err = deflate(&stream, Z_FINISH);
    if (Z_STREAM_END == err) {
        newdstlen = stream.total_out;
    }
    err = deflateEnd(&stream);
    if (err != Z_OK) {
        return 0;
    }
    return newdstlen;
}

bool ZipCreator::AddFileData(Str name, Str data, u32 dosdate) {
    size_t size = (size_t)data.len;
    ReportIf(size >= UINT32_MAX);
    ReportIf(len(name) >= UINT16_MAX);
    if (size >= UINT32_MAX) {
        return false;
    }

    size_t fileOffset = bytesWritten;
    u16 flags = (1 << 11); // filename is UTF-8
    uInt crc = crc32(0, (const Bytef*)data.s, (uInt)size);
    int namelen = len(name);
    if (namelen >= UINT16_MAX) {
        return false;
    }

    u16 method = Z_DEFLATED;
    uLongf compressedSize = (u32)size;
    char* compressed = AllocArrayTemp<char>(size);
    if (!compressed) {
        return false;
    }
    compressedSize = zip_compress(compressed, (u32)size, data.s, (u32)size);
    if (!compressedSize) {
        method = 0; // Store
        memcpy(compressed, data.s, size);
        compressedSize = (u32)size;
    }

    constexpr size_t kHdrSize = 30;
    ByteWriterLE local(kHdrSize);
    local.Write32(0x04034B50); // signature
    local.Write16(20);         // version needed to extract
    local.Write16(flags);
    local.Write16(method);
    local.Write32(dosdate);
    local.Write32(crc);
    local.Write32(compressedSize);
    local.Write32((u32)size);
    local.Write16((u16)namelen);
    local.Write16(0); // extra field length
    ReportIf(len(local.d) != kHdrSize);

    Str localHeader = ToStr(local.d);
    bool ok = WriteData(localHeader.s, kHdrSize);
    ok = ok && WriteData(name.s, namelen);
    ok = ok && WriteData(compressed, compressedSize);

    constexpr size_t kCentralSize = 46;
    ByteWriterLE central(kCentralSize);
    central.Write32(0x02014B50); // signature
    central.Write16(20);         // version made by
    central.Write16(20);         // version needed to extract
    central.Write16(flags);
    central.Write16(method);
    central.Write32(dosdate);
    central.Write32(crc);
    central.Write32(compressedSize);
    central.Write32((u32)size);
    central.Write16((u16)namelen);
    central.Write16(0); // extra field length
    central.Write16(0); // file comment length
    central.Write16(0); // disk number
    central.Write16(0); // internal file attributes
    central.Write32(0); // external file attributes
    central.Write32((u32)fileOffset);
    ReportIf(len(central.d) != kCentralSize);

    centraldir.Append(Str(ToStr(central.d).s, (int)kCentralSize));
    centraldir.Append(name);

    fileCount++;
    return ok;
}

// add a given file under (optional) nameInZip
bool ZipCreator::AddFile(Str path, Str nameInZip) {
    Str fileData = file::ReadFile(path);
    if (!fileData) {
        return false;
    }

    u32 dosdatetime = 0;
    FILETIME ft = file::GetModificationTime(path);
    if (ft.dwLowDateTime || ft.dwHighDateTime) {
        FILETIME ftLocal;
        WORD dosDate, dosTime;
        if (FileTimeToLocalFileTime(&ft, &ftLocal) && FileTimeToDosDateTime(&ftLocal, &dosDate, &dosTime)) {
            dosdatetime = MAKELONG(dosTime, dosDate);
        }
    }

    if (!nameInZip) {
        nameInZip = path::IsAbsolute(path) ? path::GetBaseNameTemp(path) : path;
    }

    Str name = str::Dup(nameInZip);
    str::TransCharsInPlace(name, StrL("\\"), StrL("/"));

    bool res = AddFileData(name, fileData, dosdatetime);
    str::Free(fileData);
    return res;
}

// we use the filePath relative to dir as the zip name
bool ZipCreator::AddFileFromDir(Str filePath, Str dir) {
    if (str::IsEmpty(dir) || !str::StartsWith(filePath, dir)) {
        return false;
    }
    if (!path::IsSep(filePath.s[dir.len])) {
        return false;
    }
    Str nameInZip = Str(filePath.s + dir.len + 1, filePath.len - dir.len - 1);
    return AddFile(filePath, nameInZip);
}

bool ZipCreator::AddDir(Str dir, bool recursive) {
    DirIter di{dir};
    di.recurse = recursive;
    bool ok;
    for (DirIterEntry* de : di) {
        ok = this->AddFileFromDir(de->filePath, dir);
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool ZipCreator::Finish() {
    ReportIf(bytesWritten >= UINT32_MAX);
    ReportIf(fileCount >= UINT16_MAX);
    if (bytesWritten >= UINT32_MAX || fileCount >= UINT16_MAX) {
        return false;
    }

    constexpr size_t kDirSize = 22;
    ByteWriterLE eocd(kDirSize);
    eocd.Write32(0x06054B50); // signature
    eocd.Write16(0);          // disk number
    eocd.Write16(0);          // disk number of central directory
    eocd.Write16((u16)fileCount);
    eocd.Write16((u16)fileCount);
    eocd.Write32((u32)len(centraldir));
    eocd.Write32((u32)bytesWritten);
    eocd.Write16(0); // comment len
    ReportIf(len(eocd.d) != kDirSize);

    bool ok = WriteData(ToStr(centraldir).s, len(centraldir));
    ok = ok && WriteData(ToStr(eocd.d).s, kDirSize);
    return ok;
}

IStream* OpenDirAsZipStream(Str dirPath, bool recursive) {
    if (!dir::Exists(dirPath)) {
        return nullptr;
    }

    ScopedComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) {
        return nullptr;
    }

    ZipCreator zc(stream);
    if (!zc.AddDir(dirPath, recursive)) {
        return nullptr;
    }
    if (!zc.Finish()) {
        return nullptr;
    }

    stream->AddRef();
    return stream;
}

// adapted from https://www.cocoanetics.com/2012/02/decompressing-files-into-memory/
// d is a content of gzip file
// returns uncpmpressed data
// the returned data will have 2 zero bytes at end to make sure it's also
// a 0-terminated char* or WCHA* string
// those 2 bytes are not reported as
Str Ungzip(const Str& d) {
    size_t n = (size_t)d.len;
    u8* dataCompr = (u8*)d.s;
    // aggressive growth for uncompressed buffer because I use this
    // for .syntex files and they compress really well
    size_t lenUncr = n * 2;

    bool done = false;
    int res;

    z_stream strm;
    strm.next_in = (Bytef*)dataCompr;
    strm.avail_in = (uInt)n;
    strm.total_out = 0;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;

    res = inflateInit2(&strm, (15 + 32));
    if (res != Z_OK) {
        return {};
    }

    // +2 for space for terminating char* or WCHAR*
    u8* dataUncr = AllocArray<u8>(lenUncr + 2);
    if (!dataUncr) {
        return {};
    }

    while (!done) {
        if (strm.total_out >= lenUncr) {
            size_t newLen = lenUncr * 2;
            u8* dataUncr2 = (u8*)realloc(dataUncr, newLen + 2);
            if (!dataUncr2) {
                free((void*)dataUncr);
                return {};
            }
            dataUncr = dataUncr2;
            lenUncr = newLen;
        }

        strm.next_out = dataUncr + strm.total_out;
        strm.avail_out = (uInt)lenUncr - (uInt)strm.total_out;

        // Inflate another chunk.
        res = inflate(&strm, Z_SYNC_FLUSH);

        if (res == Z_STREAM_END) {
            done = true;
        } else if (res != Z_OK) {
            break;
        }
    }
    res = inflateEnd(&strm);
    if (!done || res != Z_OK) {
        free((void*)dataUncr);
        return {};
    }

    lenUncr = strm.total_out;
    // also make it a valid 0-terminated char* or WCHAR* string
    dataUncr[lenUncr] = 0;
    dataUncr[lenUncr + 1] = 0;
    return Str((char*)dataUncr, (int)lenUncr);
}

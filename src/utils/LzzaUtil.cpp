/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "LzzaUtil.h"

#include "ByteReader.h"
#include "ByteWriter.h"
#include "FileUtil.h"

#include <LzmaDec.h>
#include <LzmaEnc.h>
#include <zlib.h>

namespace lzza {

// the signature must appear right before the first ZIP file entry
#define LZZA_SIGNATURE      0x415a7a4c
#define ZIP_LOCAL_SIGNATURE 0x04034b50
#define ZIP_CDIR_SIGNATURE  0x02014b50
#define ZIP_EOCD_SIGNATURE  0x06054b50

#define ZIP_LZMA_HEADER_LEN     (4 + LZMA_PROPS_SIZE)
// LZMA data has EOS marker; filename is UTF-8
#define ZIP_LZMA_ENTRY_FLAGS    ((1 << 1) | (1 << 11))

struct ISzAllocatorAlloc : ISzAlloc {
    Allocator *allocator;

    static void *_Alloc(void *p, size_t size) {
        ISzAllocatorAlloc *a = (ISzAllocatorAlloc *)p;
        return Allocator::Alloc(a->allocator, size);
    }
    static void _Free(void *p, void *address) {
        ISzAllocatorAlloc *a = (ISzAllocatorAlloc *)p;
        Allocator::Free(a->allocator, address);
    }

    ISzAllocatorAlloc(Allocator *allocator) {
        this->Alloc = _Alloc;
        this->Free = _Free;
        this->allocator = allocator;
    }
};

static uint32_t LzmaCompress(uint8_t *out, uint32_t outsize, const uint8_t *in, uint32_t insize, Allocator *allocator=NULL)
{
    if (outsize < ZIP_LZMA_HEADER_LEN)
        return 0;

    ISzAllocatorAlloc lzmaAlloc(allocator);
    SizeT destLen = outsize - ZIP_LZMA_HEADER_LEN;
    SizeT propsLen = LZMA_PROPS_SIZE;
    CLzmaEncProps props;

    LzmaEncProps_Init(&props);
    SRes res = LzmaEncode(out + ZIP_LZMA_HEADER_LEN, &destLen, in, insize, &props, out + 4, &propsLen, TRUE, NULL, &lzmaAlloc, &lzmaAlloc);
    if (res != SZ_OK || propsLen != LZMA_PROPS_SIZE)
        return 0;

    ByteWriterLE lzmaZipHeader(out, 4);
    lzmaZipHeader.Write16(0);
    lzmaZipHeader.Write16((uint16_t)propsLen);
    return (uint32_t)(destLen + ZIP_LZMA_HEADER_LEN);
}

static bool LzmaDecompress(uint8_t *out, uint32_t outsize, const uint8_t *in, uint32_t insize, Allocator *allocator=NULL)
{
    if (insize < ZIP_LZMA_HEADER_LEN)
        return false;

    ByteReader br(in, 4);
    if (br.WordLE(0) != 0 || br.WordLE(2) != LZMA_PROPS_SIZE)
        return false;

    ISzAllocatorAlloc lzmaAlloc(allocator);
    SizeT uncompressedSize = outsize;
    SizeT compressedSize = insize - ZIP_LZMA_HEADER_LEN;
    ELzmaStatus status;

    SRes res = LzmaDecode(out, &uncompressedSize, in + ZIP_LZMA_HEADER_LEN, &compressedSize, in + 4, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &lzmaAlloc);

    return res == SZ_OK && status == LZMA_STATUS_FINISHED_WITH_MARK && uncompressedSize == outsize;
}

struct ArchiveEntry {
    const char *name;
    const uint8_t *data;
    uint16_t method;
    uint32_t crc32;
    uint32_t dosdate;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
};

// TODO: optimize by verifying the entire archive and caching all entry data?
static bool FindArchiveEntry(Archive *lzza, ArchiveEntry *entry, const char *fileName, int idx)
{
    ByteReader br(lzza->data, lzza->dataLen);
    if (br.DWordLE(0) != LZZA_SIGNATURE)
        return false;

    size_t offset = 4;
    for (;;) {
        // sanity checks
        if (offset + 30 > lzza->dataLen)
            return false;
        if (br.DWordLE(offset + 0) != ZIP_LOCAL_SIGNATURE)
            return false;
        if (br.WordLE(offset + 28) != 4)
            return false;
        if (br.WordLE(offset + 6) != ZIP_LZMA_ENTRY_FLAGS)
            return false;
        // read entry values
        entry->method = br.WordLE(offset + 8);
        entry->dosdate = br.DWordLE(offset + 10);
        entry->crc32 = br.DWordLE(offset + 14);
        entry->compressedSize = br.DWordLE(offset + 18);
        entry->uncompressedSize = br.DWordLE(offset + 22);
        uint16_t nameLen = br.WordLE(offset + 26);
        // sanity checks
        if (entry->method != 0 && entry->method != 14)
            return false;
        if (0 == entry->method && entry->compressedSize != entry->uncompressedSize)
            return false;
        if (entry->compressedSize > lzza->dataLen || offset + 34 + nameLen + entry->compressedSize > lzza->dataLen)
            return false;
        if (br.Byte(offset + 30 + nameLen))
            return false;
        // name and data are now safe to use
        entry->name = &lzza->data[offset + 30];
        entry->data = (const uint8_t *)lzza->data + offset + 34 + nameLen;

        if (fileName && str::Eq(entry->name, fileName))
            return true;
        if (idx-- == 0)
            return true;
        offset += 34 + nameLen + entry->compressedSize;
    }
}

static FileData *GetFileData(Archive *lzza, const char *fileName, int idx, Allocator *allocator=NULL)
{
    ArchiveEntry entry;
    if (!FindArchiveEntry(lzza, &entry, fileName, idx))
        return NULL;

    FileData *fd = (FileData *)Allocator::Alloc(allocator, sizeof(FileData) + entry.uncompressedSize);
    if (!fd)
        return NULL;
    fd->dataSize = entry.uncompressedSize;
    fd->name = entry.name;

    FILETIME localFt;
    if (!DosDateTimeToFileTime(HIWORD(entry.dosdate), LOWORD(entry.dosdate), &localFt) ||
        !LocalFileTimeToFileTime(&localFt, &fd->ft)) {
        fd->ft.dwLowDateTime = fd->ft.dwHighDateTime = 0;
    }

    if (entry.method == 0) {
        memcpy(fd->data, entry.data, fd->dataSize);
    }
    else if (!LzmaDecompress((uint8_t *)fd->data, fd->dataSize, (const uint8_t *)entry.data, entry.compressedSize, allocator)) {
        Allocator::Free(allocator, fd);
        return NULL;
    }

    uint32_t realCrc = crc32(0, (const uint8_t *)fd->data, (uint32_t)fd->dataSize);
    if (realCrc != entry.crc32) {
        Allocator::Free(allocator, fd);
        return NULL;
    }

    return fd;
}

FileData *GetFileData(Archive *lzza, int idx, Allocator *allocator)
{
    return GetFileData(lzza, NULL, idx, allocator);
}

FileData *GetFileData(Archive *lzza, const char *fileName, Allocator *allocator)
{
    return GetFileData(lzza, fileName, -1, allocator);
}

bool ExtractFiles(const char *archivePath, const char *dstDir, const char **files, Allocator *allocator)
{
    Archive lzza;
    lzza.data = file::ReadAllUtf(archivePath, &lzza.dataLen, allocator);
    if (!lzza.data)
        return false;

    bool ok = true;
    for (; *files; files++) {
        FileData *fd = GetFileData(&lzza, *files, -1, allocator);
        if (!fd) {
            ok = false;
            continue;
        }
        char *filePath = path::JoinUtf(dstDir, fd->name, allocator);
        if (filePath) {
            str::TransChars(filePath, "/", "\\");
            ok &= file::WriteAllUtf(filePath, fd->data, fd->dataSize);
            Allocator::Free(allocator, filePath);
        }
        else {
            ok = false;
        }
        Allocator::Free(allocator, fd);
    }
    return ok;
}

static bool AppendEntry(str::Str<char>& data, str::Str<char>& centralDir, const char *nameUtf8, const char *filedata, size_t filelen, uint32_t dosdate)
{
    CrashIf(filelen >= UINT32_MAX);
    CrashIf(str::Len(nameUtf8) >= UINT16_MAX);

    size_t fileOffset = data.Size();
    uInt crc = crc32(0, (const Bytef *)filedata, (uInt)filelen);
    size_t namelen = str::Len(nameUtf8);

    uint16_t method = 14 /* LZMA */;
    uLongf compressedSize = (uint32_t)filelen;
    ScopedMem<uint8_t> compressed((uint8_t *)malloc(filelen));
    if (!compressed)
        return false;
    compressedSize = LzmaCompress(compressed, (uint32_t)filelen, (const uint8_t *)filedata, (uint32_t)filelen);
    if (!compressedSize) {
        method = 0 /* Stored */;
        memcpy(compressed.Get(), filedata, filelen);
        compressedSize = (uint32_t)filelen;
    }

    ByteWriterLE local(data.AppendBlanks(30), 30);
    local.Write32(ZIP_LOCAL_SIGNATURE);
    local.Write16(63); // version needed to extract
    local.Write16(ZIP_LZMA_ENTRY_FLAGS);
    local.Write16(method);
    local.Write32(dosdate);
    local.Write32(crc);
    local.Write32(compressedSize);
    local.Write32((uint32_t)filelen);
    local.Write16((uint16_t)namelen);
    local.Write16(4); // 0-terminator(s) in extra data field
    data.Append(nameUtf8, namelen);
    data.AppendBlanks(4);
    data.Append((const char *)compressed.Get(), compressedSize);

    ByteWriterLE central(centralDir.AppendBlanks(46), 46);
    central.Write32(ZIP_CDIR_SIGNATURE);
    central.Write32(63 | (63 << 16)); // versions
    central.Write16(ZIP_LZMA_ENTRY_FLAGS);
    central.Write16(method);
    central.Write32(dosdate);
    central.Write32(crc);
    central.Write32(compressedSize);
    central.Write32((uint32_t)filelen);
    central.Write16((uint16_t)namelen);
    central.Write32(0); // extra field and file comment lengths
    central.Write32(0); // disk number and internal attributes
    central.Write32(0); // external file attributes
    central.Write32((uint32_t)fileOffset);
    centralDir.Append(nameUtf8, namelen);

    return true;
}

bool CreateArchive(const WCHAR *archivePath, const WCHAR *srcDir, WStrVec& names)
{
    str::Str<char> data;
    str::Str<char> centralDir;

    ByteWriterLE lzzaHeader(data.AppendBlanks(4), 4);
    lzzaHeader.Write32(LZZA_SIGNATURE);

    for (size_t i = 0; i < names.Count(); i += 2) {
        ScopedMem<WCHAR> filepath(path::Join(srcDir, names.At(i)));
        size_t filelen;
        ScopedMem<char> filedata(file::ReadAll(filepath, &filelen));
        if (!filedata)
            return false;

        uint32_t dosdatetime = 0;
        FILETIME ft = file::GetModificationTime(filepath);
        if (ft.dwLowDateTime || ft.dwHighDateTime) {
            FILETIME ftLocal;
            WORD dosDate, dosTime;
            if (FileTimeToLocalFileTime(&ft, &ftLocal) &&
                FileTimeToDosDateTime(&ftLocal, &dosDate, &dosTime)) {
                dosdatetime = MAKELONG(dosTime, dosDate);
            }
        }

        ScopedMem<char> inArchiveName(str::conv::ToUtf8(names.At(names.At(i + 1) ? i + 1 : i)));
        str::TransChars(inArchiveName, "\\", "/");

        if (!AppendEntry(data, centralDir, inArchiveName, filedata, filelen, dosdatetime))
            return false;
    }

    CrashIf(data.Count() >= UINT32_MAX);
    CrashIf(names.Count() / 2 >= UINT16_MAX);

    // central directory is only needed so that LZZA archives are proper ZIP archives
    // (it isn't read or verified by the extraction code above)
    size_t centralDirOffset = data.Size();
    data.Append(centralDir.Get(), centralDir.Size());
    ByteWriterLE eocd(data.AppendBlanks(22), 22);
    eocd.Write32(ZIP_EOCD_SIGNATURE);
    eocd.Write32(0); // disk numbers
    eocd.Write16((uint16_t)(names.Count() / 2));
    eocd.Write16((uint16_t)(names.Count() / 2));
    eocd.Write32((uint32_t)centralDir.Size());
    eocd.Write32((uint32_t)centralDirOffset);
    eocd.Write16(0); // comment len

    return file::WriteAll(archivePath, data.Get(), data.Size());
}

}

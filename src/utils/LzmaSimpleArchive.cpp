/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "LzmaSimpleArchive.h"

#include "ByteOrderDecoder.h"
#include "ByteWriter.h"
#include "FileUtil.h"
#include <LzmaDec.h>
#include <LzmaEnc.h>
#include <Bra.h>
#include <zlib.h> // for crc32

/*
Implements extracting data from a simple archive format, made up by me.
For the description of the format, see comment below, above ParseSimpleArchive().
Archives are simple to create (in SumatraPDF, we used to use lzma.exe and a python script).
*/

// 'LzSA' for "Lzma Simple Archive"
#define LZMA_MAGIC_ID 0x41537a4c

namespace lzma {

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

// adapted from lzma/C/Lzma86Dec.c

#define LZMA86_HEADER_SIZE (1 + LZMA_PROPS_SIZE + sizeof(uint64_t))

bool Decompress(const char *compressed, size_t compressedSize, char *uncompressed, size_t uncompressedSize, Allocator *allocator)
{
    if (compressedSize < 1)
        return false;

    ByteOrderDecoder br(compressed, compressedSize, ByteOrderDecoder::LittleEndian);
    uint8_t usesX86Filter = br.UInt8();
    // handle stored data
    if (usesX86Filter == (uint8_t)-1) {
        if (uncompressedSize != compressedSize - 1)
            return false;
        memcpy(uncompressed, compressed + 1, compressedSize - 1);
        return true;
    }

    if (compressedSize < LZMA86_HEADER_SIZE || usesX86Filter > 1)
        return false;

    br.Skip(LZMA_PROPS_SIZE);
    SizeT uncompressedSizeCmp = (SizeT)br.UInt64();
    if (uncompressedSizeCmp != uncompressedSize)
        return false;
    CrashIf(br.Offset() != LZMA86_HEADER_SIZE);
    SizeT compressedSizeTmp = compressedSize - LZMA86_HEADER_SIZE;

    ISzAllocatorAlloc lzmaAlloc(allocator);
    ELzmaStatus status;
    int res = LzmaDecode((Byte *)uncompressed, &uncompressedSizeCmp,
        (Byte *)compressed + LZMA86_HEADER_SIZE, &compressedSizeTmp,
        (Byte *)compressed + 1, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status,
        &lzmaAlloc);

    if (SZ_OK != res || (status != LZMA_STATUS_FINISHED_WITH_MARK && status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK))
        return false;
    if (uncompressedSizeCmp != uncompressedSize)
        return false;

    if (usesX86Filter) {
        UInt32 x86State;
        x86_Convert_Init(x86State);
        x86_Convert((Byte *)uncompressed, uncompressedSize, 0, &x86State, 0);
    }

    return true;
}

bool Compress(const char *uncompressed, size_t uncompressedSize, char *compressed, size_t *compressedSize, Allocator *allocator)
{
    ISzAllocatorAlloc lzmaAlloc(allocator);

    if (*compressedSize < uncompressedSize + 1)
        return false;
    if (*compressedSize < LZMA86_HEADER_SIZE)
        goto Store;

    CLzmaEncProps props;
    LzmaEncProps_Init(&props);

    size_t bjc_size = (size_t)-1, lzma_size = (size_t)-1;

    SizeT outSize = *compressedSize - LZMA86_HEADER_SIZE;
    SizeT propsSize = LZMA_PROPS_SIZE;
    SRes res = LzmaEncode((Byte *)compressed + LZMA86_HEADER_SIZE, &outSize, (const Byte *)uncompressed, uncompressedSize, &props, (Byte *)compressed + 1, &propsSize, FALSE, NULL, &lzmaAlloc, &lzmaAlloc);
    if (SZ_OK == res && propsSize == LZMA_PROPS_SIZE)
        lzma_size = outSize + LZMA86_HEADER_SIZE;

    uint8_t *bjc_enc = (uint8_t *)Allocator::Alloc(allocator, uncompressedSize);
    if (bjc_enc) {
        memcpy(bjc_enc, uncompressed, uncompressedSize);
        UInt32 x86State;
        x86_Convert_Init(x86State);
        x86_Convert(bjc_enc, uncompressedSize, 0, &x86State, 1);

        outSize = *compressedSize - LZMA86_HEADER_SIZE;
        propsSize = LZMA_PROPS_SIZE;
        res = LzmaEncode((Byte *)compressed + LZMA86_HEADER_SIZE, &outSize, bjc_enc, uncompressedSize, &props, (Byte *)compressed + 1, &propsSize, FALSE, NULL, &lzmaAlloc, &lzmaAlloc);
        if (SZ_OK == res && propsSize == LZMA_PROPS_SIZE)
            bjc_size = outSize + LZMA86_HEADER_SIZE;

        Allocator::Free(allocator, bjc_enc);
    }

    if (bjc_size < lzma_size) {
        compressed[0] = 1;
        *compressedSize = bjc_size;
    }
    else if (lzma_size < uncompressedSize + 1) {
        compressed[0] = 0;
        *compressedSize = lzma_size;
        if (bjc_enc) {
            outSize = *compressedSize - LZMA86_HEADER_SIZE;
            propsSize = LZMA_PROPS_SIZE;
            LzmaEncode((Byte *)compressed + LZMA86_HEADER_SIZE, &outSize, (const Byte *)uncompressed, uncompressedSize, &props, (Byte *)compressed + 1, &propsSize, FALSE, NULL, &lzmaAlloc, &lzmaAlloc);
        }
    }
    else {
Store:
        compressed[0] = (uint8_t)-1;
        memcpy(compressed + 1, uncompressed, uncompressedSize);
        *compressedSize = uncompressedSize + 1;
        return true;
    }
    // TODO: don't save this redundant information
    ByteWriterLE(compressed + 1 + LZMA_PROPS_SIZE, LZMA86_HEADER_SIZE - 1 - LZMA_PROPS_SIZE).Write64(uncompressedSize);
    return true;
}

/* archiveHeader points to the beginning of archive, which has a following format:

u32   magic_id 0x41537a4c ("LzSA' for "Lzma Simple Archive")
u32   number of files
for each file:
  u32        length of file metadata (from this field to file name's 0-terminator)
  u32        file size compressed
  u32        file size uncompressed
  u32        crc32 checksum of uncompressed data
  FILETIME   last modification time in Windows's FILETIME format (8 bytes)
  char[...]  file name, 0-terminated
u32   crc32 checksum of the header (i.e. data so far)
for each file:
  compressed file data (for each file, the first byte indicates the compression method)

Integers are little-endian.
*/

// magic_id + number of files
#define HEADER_START_SIZE (4 + 4)

// 4 * u32 + FILETIME + name
#define FILE_ENTRY_MIN_SIZE (4 * 4 + 8 + 1)

bool ParseSimpleArchive(const char *archiveHeader, size_t dataLen, SimpleArchive* archiveOut)
{
    if (dataLen < HEADER_START_SIZE)
        return false;
    if (dataLen > (uint32_t)-1)
        return false;

    ByteOrderDecoder br(archiveHeader, dataLen, ByteOrderDecoder::LittleEndian);
    uint32_t magic_id = br.UInt32();
    if (magic_id != LZMA_MAGIC_ID)
        return false;

    uint32_t filesCount = br.UInt32();
    archiveOut->filesCount = filesCount;
    if (filesCount > dimof(archiveOut->files))
        return false;

    FileInfo *fi;
    for (uint32_t i = 0; i < filesCount; i++) {
        if (br.Offset() + FILE_ENTRY_MIN_SIZE > dataLen)
            return false;

        uint32_t fileHeaderSize = br.UInt32();
        if (fileHeaderSize < FILE_ENTRY_MIN_SIZE || fileHeaderSize > 1024)
            return false;
        if (br.Offset() + fileHeaderSize - 4 > dataLen)
            return false;

        fi = &archiveOut->files[i];
        fi->compressedSize = br.UInt32();
        fi->uncompressedSize = br.UInt32();
        fi->uncompressedCrc32 = br.UInt32();
        fi->ftModified.dwLowDateTime = br.UInt32();
        fi->ftModified.dwHighDateTime = br.UInt32();
        fi->name = archiveHeader + br.Offset();
        br.Skip(fileHeaderSize - FILE_ENTRY_MIN_SIZE);
        if (br.Char() != '\0')
            return false;
    }

    if (br.Offset() + 4 > dataLen)
        return false;

    uint32_t headerSize = br.Offset();
    uint32_t headerCrc32 = br.UInt32();
    uint32_t realCrc = crc32(0, (const uint8_t *)archiveHeader, headerSize);
    if (headerCrc32 != realCrc)
        return false;

    for (uint32_t i = 0; i < filesCount; i++) {
        fi = &archiveOut->files[i];
        // overflow check
        if (fi->compressedSize > dataLen || br.Offset() + fi->compressedSize > dataLen)
            return false;
        fi->compressedData = archiveHeader + br.Offset();
        br.Skip(fi->compressedSize);
    }

    return br.Offset() == dataLen;
}

int GetIdxFromName(SimpleArchive *archive, const char *fileName)
{
    for (int i = 0; i < archive->filesCount; i++) {
        const char *file = archive->files[i].name;
        if (str::Eq(file, fileName))
            return i;
    }
    return -1;
}

char *GetFileDataByIdx(SimpleArchive *archive, int idx, Allocator *allocator)
{
    if (idx >= archive->filesCount)
        return NULL;

    FileInfo *fi = &archive->files[idx];

    char *uncompressed = (char *)Allocator::Alloc(allocator, fi->uncompressedSize);
    if (!uncompressed)
        return NULL;

    bool ok = Decompress(fi->compressedData, fi->compressedSize, uncompressed, fi->uncompressedSize, allocator);
    if (!ok) {
        Allocator::Free(allocator, uncompressed);
        return NULL;
    }

    uint32_t realCrc = crc32(0, (const uint8_t *)uncompressed, fi->uncompressedSize);
    if (realCrc != fi->uncompressedCrc32) {
        Allocator::Free(allocator, uncompressed);
        return NULL;
    }

    return uncompressed;
}

char *GetFileDataByName(SimpleArchive *archive, const char *fileName, Allocator *allocator)
{
    int idx = GetIdxFromName(archive, fileName);
    if (-1 != idx)
        return GetFileDataByIdx(archive, idx, allocator);
    return NULL;
}

static bool ExtractFileByIdx(SimpleArchive *archive, int idx, const char *dstDir, Allocator *allocator)
{
    FileInfo *fi = &archive->files[idx];

    char *uncompressed = GetFileDataByIdx(archive, idx, allocator);
    if (!uncompressed)
        return false;

    bool ok = false;
    char *filePath = path::JoinUtf(dstDir, fi->name, allocator);
    if (filePath)
        ok = file::WriteAllUtf(filePath, uncompressed, fi->uncompressedSize);

    Allocator::Free(allocator, filePath);
    Allocator::Free(allocator, uncompressed);

    return ok;
}

bool ExtractFiles(const char *archivePath, const char *dstDir, const char **files, Allocator *allocator)
{
    size_t archiveDataSize;
    char *archiveData = file::ReadAllUtf(archivePath, &archiveDataSize, allocator);
    if (!archiveData)
        return false;

    SimpleArchive archive;
    bool ok = ParseSimpleArchive(archiveData, archiveDataSize, &archive);
    if (!ok) {
        Allocator::Free(allocator, archiveData);
        return false;
    }
    for (; *files; files++) {
        int idx = GetIdxFromName(&archive, *files);
        if (-1 != idx) {
            ok &= ExtractFileByIdx(&archive, idx, dstDir, allocator);
        }
    }
    Allocator::Free(allocator, archiveData);
    return ok;
}

static bool AppendEntry(str::Str<char>& data, str::Str<char>& content, const WCHAR *filePath, const char *inArchiveName, FileInfo *fi=NULL)
{
    size_t headerSize = 25 + str::Len(inArchiveName);
    FILETIME ft = file::GetModificationTime(filePath);
    if (fi && FileTimeEq(ft, fi->ftModified)) {
ReusePrevious:
        ByteWriterLE meta(data.AppendBlanks(24), 24);
        meta.Write32(headerSize);
        meta.Write32(fi->compressedSize);
        meta.Write32(fi->uncompressedSize);
        meta.Write32(fi->uncompressedCrc32);
        meta.Write32(ft.dwLowDateTime);
        meta.Write32(ft.dwHighDateTime);
        strcpy_s(data.AppendBlanks(headerSize - 24), headerSize - 24, inArchiveName);
        return content.AppendChecked(fi->compressedData, fi->compressedSize);
    }

    size_t fileDataLen;
    ScopedMem<char> fileData(file::ReadAll(filePath, &fileDataLen));
    if (!fileData || fileDataLen > UINT32_MAX)
        return false;
    uint32_t fileDataCrc = crc32(0, (const uint8_t *)fileData.Get(), (uint32_t)fileDataLen);
    if (fi && fi->uncompressedCrc32 == fileDataCrc && fi->uncompressedSize == fileDataLen)
        goto ReusePrevious;

    ScopedMem<char> compressed((char *)malloc(fileDataLen + 1));
    if (!compressed)
        return false;
    size_t compressedSize = fileDataLen + 1;
    if (!Compress(fileData, fileDataLen, compressed, &compressedSize))
        return false;

    ByteWriterLE meta(data.AppendBlanks(24), 24);
    meta.Write32(headerSize);
    meta.Write32(compressedSize);
    meta.Write32((uint32_t)fileDataLen);
    meta.Write32(fileDataCrc);
    meta.Write32(ft.dwLowDateTime);
    meta.Write32(ft.dwHighDateTime);
    strcpy_s(data.AppendBlanks(headerSize - 24), headerSize - 24, inArchiveName);
    return content.AppendChecked(compressed, compressedSize);
}

bool CreateArchive(const WCHAR *archivePath, const WCHAR *srcDir, WStrVec& names)
{
    size_t prevDataLen = 0;
    ScopedMem<char> prevData(file::ReadAll(archivePath, &prevDataLen));
    SimpleArchive prevArchive;
    if (!ParseSimpleArchive(prevData, prevDataLen, &prevArchive))
        prevArchive.filesCount = 0;

    str::Str<char> data;
    str::Str<char> content;

    ByteWriterLE lzsaHeader(data.AppendBlanks(8), 8);
    lzsaHeader.Write32(LZMA_MAGIC_ID);
    lzsaHeader.Write32(names.Count() / 2);

    for (size_t i = 0; i < names.Count(); i += 2) {
        ScopedMem<WCHAR> filePath(path::Join(srcDir, names.At(i)));
        ScopedMem<char> utf8Name(str::conv::ToUtf8(names.At(names.At(i + 1) ? i + 1 : i)));
        str::TransChars(utf8Name, "/", "\\");
        int idx = GetIdxFromName(&prevArchive, utf8Name);
        FileInfo *fi = NULL;
        if (idx != -1)
            fi = &prevArchive.files[idx];
        if (!AppendEntry(data, content, filePath, utf8Name, fi))
            return false;
    }

    uint32_t headerCrc32 = crc32(0, (const uint8_t *)data.Get(), (uint32_t)data.Size());
    ByteWriterLE(data.AppendBlanks(4), 4).Write32(headerCrc32);
    if (!data.AppendChecked(content.Get(), content.Size()))
        return false;

    return file::WriteAll(archivePath, data.Get(), data.Size());
}

}

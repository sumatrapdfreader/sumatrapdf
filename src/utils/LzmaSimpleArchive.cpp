/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "LzmaSimpleArchive.h"

#include "ByteOrderDecoder.h"
#include "FileUtil.h"
#include <LzmaDec.h>
#include <Bra.h>
#include <zlib.h> // for crc32

/*
Implements extracting data from a simple archive format, made up by me.
Raw data is compressed with lzma.exe from LZMA SDK.
For the description of the format, see comment below, above ParseSimpleArchive().
Archives are simple to create (in Sumatra, we use a python script).
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

bool Decompress(const char *compressed, size_t compressedSize, char *out, size_t *uncompressedSizeInOut, Allocator *allocator)
{
    if (compressedSize < LZMA86_HEADER_SIZE)
        return false;

    ByteOrderDecoder br(compressed, compressedSize, ByteOrderDecoder::LittleEndian);
    uint8_t usesX86Filter = br.UInt8();
    if (usesX86Filter > 1)
        return false;

    br.Skip(LZMA_PROPS_SIZE);
    SizeT uncompressedSize = (SizeT)br.UInt64();
    if (uncompressedSize > *uncompressedSizeInOut)
        return false;
    CrashIf(br.Offset() != LZMA86_HEADER_SIZE);
    SizeT compressedSizeTmp = compressedSize - LZMA86_HEADER_SIZE;

    ISzAllocatorAlloc lzmaAlloc(allocator);
    ELzmaStatus status;
    int res = LzmaDecode((Byte *)out, uncompressedSizeInOut,
        (Byte *)compressed + LZMA86_HEADER_SIZE, &compressedSizeTmp,
        (Byte *)compressed + 1, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status,
        &lzmaAlloc);

    if (SZ_OK != res || (status != LZMA_STATUS_FINISHED_WITH_MARK && status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK))
        return false;

    if (usesX86Filter) {
        UInt32 x86State;
        x86_Convert_Init(x86State);
        x86_Convert((Byte *)out, *uncompressedSizeInOut, 0, &x86State, 0);
    }

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
  compressed file data

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

    size_t uncompressedSize = fi->uncompressedSize;
    bool ok = Decompress(fi->compressedData, fi->compressedSize, uncompressed, &uncompressedSize, allocator);
    if (!ok || uncompressedSize != fi->uncompressedSize) {
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

// files is an array of char * entries, last element must be NULL
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

}

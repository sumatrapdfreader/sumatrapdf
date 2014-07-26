/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "LzmaSimpleArchive.h"

#include "FileUtil.h"
#include <LzmaDec.h>
#include <Lzma86.h>
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

// read u32, little-endian
static uint32_t Read4(const uint8_t* in)
{
    return (static_cast<uint32_t>(in[3]) << 24)
      |    (static_cast<uint32_t>(in[2]) << 16)
      |    (static_cast<uint32_t>(in[1]) <<  8)
      |    (static_cast<uint32_t>(in[0])      );
}

static uint32_t Read4Skip(const char **in)
{
    const uint8_t *tmp = (const uint8_t*)*in;
    uint32_t res = Read4(tmp);
    *in = *in + 4;
    return res;
}

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

bool Decompress(const char *compressed, size_t compressedSize, char *out, size_t *uncompressedSizeInOut, Allocator *allocator)
{
    ISzAllocatorAlloc lzmaAlloc(allocator);

    if (compressedSize < LZMA86_HEADER_SIZE)
        return false;

    uint8_t usesX86Filter = (uint8_t)compressed[0];
    if (usesX86Filter > 1)
        return false;

    SizeT uncompressedSize = Read4((Byte *)compressed + LZMA86_SIZE_OFFSET);
    SizeT compressedSizeTmp = compressedSize - LZMA86_HEADER_SIZE;
    SizeT uncompressedSizeTmp = *uncompressedSizeInOut;

    ELzmaStatus status;
    // Note: would be nice to understand why status returns LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK and
    // not LZMA_STATUS_FINISHED_WITH_MARK. It seems to work, though.
    int res = LzmaDecode((Byte *)out, &uncompressedSizeTmp,
        (Byte *)compressed + LZMA86_HEADER_SIZE, &compressedSizeTmp,
        (Byte *)compressed + 1, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status,
        &lzmaAlloc);

    if (SZ_OK != res)
        return false;

    if (usesX86Filter) {
        UInt32 x86State;
        x86_Convert_Init(x86State);
        x86_Convert((Byte *)out, uncompressedSizeTmp, 0, &x86State, 0);
    }
    *uncompressedSizeInOut = uncompressedSize;
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
    const char *data = archiveHeader;
    const char *dataEnd = data + dataLen;

    if (dataLen < HEADER_START_SIZE)
        return false;
    if (dataLen > (uint32_t)-1)
        return false;

    uint32_t magic_id = Read4Skip(&data);
    if (magic_id != LZMA_MAGIC_ID)
        return false;

    int filesCount = (int)Read4Skip(&data);
    archiveOut->filesCount = filesCount;
    if (filesCount > MAX_LZMA_ARCHIVE_FILES)
        return false;

    FileInfo *fi;
    for (int i = 0; i < filesCount; i++) {
        if (data + FILE_ENTRY_MIN_SIZE > dataEnd)
            return false;

        uint32_t fileHeaderSize = Read4Skip(&data);
        if (fileHeaderSize < FILE_ENTRY_MIN_SIZE || fileHeaderSize > 1024)
            return false;
        if (data + fileHeaderSize > dataEnd)
            return false;

        fi = &archiveOut->files[i];
        fi->compressedSize = Read4Skip(&data);
        fi->uncompressedSize = Read4Skip(&data);
        fi->uncompressedCrc32 = Read4Skip(&data);
        fi->ftModified.dwLowDateTime = Read4Skip(&data);
        fi->ftModified.dwHighDateTime = Read4Skip(&data);
        fi->name = data;
        data += fileHeaderSize - FILE_ENTRY_MIN_SIZE;
        if (*data++ != '\0')
            return false;
    }

    if (data + 4 > dataEnd)
        return false;

    uint32_t headerSize = (uint32_t)(data - archiveHeader);
    uint32_t headerCrc32 = Read4Skip(&data);
    uint32_t realCrc = crc32(0, (const uint8_t *)archiveHeader, headerSize);
    if (headerCrc32 != realCrc)
        return false;

    for (int i = 0; i < filesCount; i++) {
        fi = &archiveOut->files[i];
        fi->compressedData = data;
        data += fi->compressedSize;
        // overflow check
        if (data < fi->compressedData)
            return false;
    }

    return data == dataEnd;
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
            // TODO: check result?
            ExtractFileByIdx(&archive, idx, dstDir, allocator);
        }
    }
    Allocator::Free(allocator, archiveData);
    return true;
}

}

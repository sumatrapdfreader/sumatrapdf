/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "LzmaDecUtil.h"

#include "FileUtil.h"
#include <LzmaDec.h>
#include <Lzma86.h>
#include <Bra.h>
#include <zlib.h> // for crc32

// 'LzSA' for "Lzma Simple Archive"
#define LZMA_MAGIC_ID 0x4c7a5341

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
};

static void *LzmaAllocatorAlloc(void *p, size_t size)
{
    ISzAllocatorAlloc *a = (ISzAllocatorAlloc*)p;
    return Allocator::Alloc(a->allocator, size);
}

static void LzmaAllocatorFree(void *p, void *address)
{
    ISzAllocatorAlloc *a = (ISzAllocatorAlloc*)p;
    Allocator::Free(a->allocator, address);
}

char* Decompress(const char *compressed, size_t compressedSize, size_t* uncompressedSizeOut, Allocator *allocator)
{
    ISzAllocatorAlloc lzmaAlloc;
    lzmaAlloc.Alloc = LzmaAllocatorAlloc;
    lzmaAlloc.Free = LzmaAllocatorFree;
    lzmaAlloc.allocator = allocator;

    if (compressedSize < LZMA86_HEADER_SIZE)
        return NULL;

    uint8_t usesX86Filter = (uint8_t)compressed[0];
    if (usesX86Filter > 1)
        return NULL;

    const uint8_t* compressed2 = (const uint8_t*)compressed;
    SizeT uncompressedSize = Read4(compressed2 + LZMA86_SIZE_OFFSET);
    uint8_t* uncompressed = (uint8_t*)Allocator::Alloc(allocator, uncompressedSize);
    if (!uncompressed)
        return NULL;

    SizeT compressedSizeTmp = compressedSize - LZMA86_HEADER_SIZE;
    SizeT uncompressedSizeTmp = uncompressedSize;

    ELzmaStatus status;
    // Note: would be nice to understand why status returns LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK and
    // not LZMA_STATUS_FINISHED_WITH_MARK. It seems to work, though.
    int res = LzmaDecode(uncompressed, &uncompressedSizeTmp,
        compressed2 + LZMA86_HEADER_SIZE, &compressedSizeTmp,
        compressed2 + 1, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status,
        &lzmaAlloc);

    if (SZ_OK != res) {
        Allocator::Free(allocator, uncompressed);
        return NULL;
    }

    if (usesX86Filter) {
        UInt32 x86State;
        x86_Convert_Init(x86State);
        x86_Convert(uncompressed, uncompressedSizeTmp, 0, &x86State, 0);
    }
    *uncompressedSizeOut = uncompressedSize;
    return (char*)uncompressed;
}

/* archiveData points to the beginning of archive, which has a following format:

u32   magic_id 0x4c7a5341 ("LzSA' for "Lzma Simple Archive")
u32   number of files
for each file:
  u32        file size uncompressed
  u32        file size compressed
  u32        crc32 checksum of compressed data
  FILETIME   last modification time in Windows's FILETIME format
  char[...]  file name, 0-terminated
u32   crc32 checksum of the header (i.e. data so far)
for each file:
  compressed file data

Integers are little-endian.
*/
bool GetArchiveInfo(const char *archiveData, size_t dataLen, ArchiveInfo* archiveInfoOut)
{
    const char *data = archiveData;

    uint32_t magic_id = Read4Skip(&data);
    if (magic_id != LZMA_MAGIC_ID)
        return false;

    int filesCount = (int)Read4Skip(&data);
    archiveInfoOut->filesCount = filesCount;
    if (filesCount > MAX_LZMA_ARCHIVE_FILES)
        return false;

    size_t off = 0;
    FileInfo *fi;
    for (int i = 0; i < filesCount; i++) {
        fi = &archiveInfoOut->files[i];
        fi->off = off;
        fi->uncompressedSize = Read4Skip(&data);
        fi->compressedSize = Read4Skip(&data);

        fi->compressedCrc32 = Read4Skip(&data);
        fi->ftModified.dwLowDateTime = Read4Skip(&data);
        fi->ftModified.dwHighDateTime = Read4Skip(&data);
        fi->name = data;
        while (*data) {
            ++data;
        }
        ++data;

        off += fi->compressedSize;
        if (off > dataLen)
            return false;
    }

    // TODO: verify crc
    //uint32_t headerCrc32 = Read4Skip(&data);
    //uint32_t crcData = crc32(0, (const Bytef *)data, dataSize - 4);
    Read4Skip(&data);

    for (int i = 0; i < filesCount; i++) {
        fi = &archiveInfoOut->files[i];
        fi->compressedData = data + fi->off;
    }

    return true;
}

char *GetFileDataByIdx(ArchiveInfo *archive, int idx, Allocator *allocator)
{
    size_t uncompressedSize;

    FileInfo *fi = &archive->files[idx];
    char *uncompressed = Decompress(fi->compressedData, fi->compressedSize, &uncompressedSize, allocator);
    if (!uncompressed)
        return NULL;

    if (uncompressedSize != fi->uncompressedSize) {
        Allocator::Free(allocator, uncompressed);
        return NULL;
    }
    return uncompressed;
}

bool ExtractFileByIdx(ArchiveInfo *archive, int idx, const char *dstDir, Allocator *allocator)
{
    bool ok;
    const char *filePath = NULL;
    FileInfo *fi = &archive->files[idx];

    char *uncompressed = GetFileDataByIdx(archive, idx, allocator);
    if (!uncompressed)
        goto Error;

    const char *fileName = fi->name;
    filePath = path::JoinUtf(dstDir, fileName, allocator);

    ok = file::WriteAllUtf(filePath, uncompressed, fi->uncompressedSize);
Exit:
    Allocator::Free(allocator, (void*)filePath);
    Allocator::Free(allocator, uncompressed);
    return ok;
Error:
    ok = false;
    goto Exit;
}

bool ExtractFileByName(ArchiveInfo *archive, const char *fileName, const char *dstDir, Allocator *allocator)
{
    for (int i = 0; i < archive->filesCount; i++) {
        const char *file = archive->files[i].name;
        if (str::Eq(file, fileName)) {
            return ExtractFileByIdx(archive, i, dstDir, allocator);
        }
    }
    // didn't find a file with this name
    return false;
}

// files array must be NULL-terminated
bool ExtractFiles(const char *archivePath, const char *dstDir, const char **files, Allocator *allocator)
{
    size_t archiveDataSize;
    const char *archiveData = (const char*)file::ReadAllUtf(archivePath, &archiveDataSize);
    if (!archiveData)
        return false;

    ArchiveInfo archive;
    bool ok = GetArchiveInfo(archiveData, archiveDataSize, &archive);
    if (!ok)
        return false;
    int i = 0;
    for (;;) {
        const char *file = files[i++];
        if (!file)
            break;
        ExtractFileByName(&archive, file, dstDir, allocator);
    }
    free((void*)archiveData);
    return true;
}

}

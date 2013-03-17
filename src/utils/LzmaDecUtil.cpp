/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "LzmaDecUtil.h"

#include "FileUtil.h"
#include <LzmaDec.h>

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

#define PROP_HEADER_SiZE 5
#define HEADER_SIZE 13

char* Decompress(const char *compressed, size_t compressedSize, size_t* uncompressedSizeOut, Allocator *allocator)
{
    ISzAllocatorAlloc lzmaAlloc;
    lzmaAlloc.Alloc = LzmaAllocatorAlloc;
    lzmaAlloc.Free = LzmaAllocatorFree;
    lzmaAlloc.allocator = allocator;

    const uint8_t* compressed2 = (const uint8_t*)compressed;
    SizeT uncompressedSize = Read4(compressed2 + PROP_HEADER_SiZE);

    uint8_t* uncompressed = (uint8_t*)Allocator::Alloc(allocator, uncompressedSize);

    SizeT compressedSizeTmp = compressedSize;
    SizeT uncompressedSizeTmp = uncompressedSize;

    ELzmaStatus status;
    // Note: would be nice to understand why status returns LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK and
    // not LZMA_STATUS_FINISHED_WITH_MARK. It seems to work, though.
    int result = LzmaDecode(uncompressed, &uncompressedSizeTmp, compressed2 + HEADER_SIZE, &compressedSizeTmp,
        compressed2, PROP_HEADER_SiZE,
        LZMA_FINISH_END, &status, &lzmaAlloc);

    *uncompressedSizeOut = uncompressedSize;
    return (char*)uncompressed;
}

/* archiveData points to the beginning of archive, which has a following format:

  uint32 magic id 'LzSA' 0x4c7a5341
  uint32 number of files
    for each file:
      uint32 file size uncompressed
      uint32 file size compressed
      string file name, 0-terminated
  for each file:
    file data

   Numbers are little-endian.
*/
bool GetArchiveInfo(const char *archiveData, ArchiveInfo* archiveInfoOut)
{
    const char *data = archiveData;

    uint32_t magic_id = Read4Skip(&data);
    if (magic_id != LZMA_MAGIC_ID)
        return false;

    int filesCount = (int)Read4Skip(&data);
    archiveInfoOut->filesCount = filesCount;
    if (filesCount > MAX_LZMA_ARCHIVE_FILES)
        return false;

    int off = 0;
    for (int i = 0; i < filesCount; i++) {
        archiveInfoOut->files[i].off = off;
        archiveInfoOut->files[i].sizeUncompressed = Read4Skip(&data);
        archiveInfoOut->files[i].sizeCompressed = Read4Skip(&data);
        off += archiveInfoOut->files[i].sizeCompressed;
        archiveInfoOut->files[i].name = data;
        while (*data) {
            ++data;
        }
        ++data;
    }

    for (int i = 0; i < filesCount; i++) {
      archiveInfoOut->files[i].data = data + archiveInfoOut->files[i].off;
    }

    return true;
}

bool ExtractFileByIdx(ArchiveInfo *archive, int idx, const char *dstDir, Allocator *allocator)
{
    char *uncompressed = NULL;
    char *filePath = NULL;
    bool ok;

    const char *compressed = archive->files[idx].data;
    size_t sizeCompressed = archive->files[idx].sizeCompressed;
    size_t sizeUncompressedExpected = archive->files[idx].sizeUncompressed;
    size_t sizeUncompressed;

    uncompressed = Decompress(compressed, sizeCompressed, &sizeUncompressed, allocator);
    if (!uncompressed)
        goto Error;

    if (sizeUncompressed != sizeUncompressedExpected)
        goto Error;

    const char *fileName = archive->files[idx].name;
    filePath = path::JoinUtf(dstDir, fileName, allocator);

    ok = file::WriteAllUtf(filePath, uncompressed, sizeUncompressed);
Exit:
    Allocator::Free(allocator, filePath);
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
    bool ok = GetArchiveInfo(archiveData, &archive);
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

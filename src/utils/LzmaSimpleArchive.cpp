/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include <LzmaDec.h>
#include <Bra.h>
#include "ByteOrderDecoder.h"
#include "LzmaSimpleArchive.h"
#include "FileUtil.h"

/*
Implements extracting data from a simple archive format, made up by me.
For the description of the format, see comment below, above ParseSimpleArchive().
Archives are simple to create (in SumatraPDF, we used to use lzma.exe and a python script)
there's a tool for creating them in ../MakeLzSA.cpp
*/

// 'LzSA' for "Lzma Simple Archive"
#define LZMA_MAGIC_ID 0x41537a4c

namespace lzma {

struct ISzAllocatorAlloc : ISzAlloc {
    Allocator* allocator;

    static void* _Alloc(void* p, size_t size) {
        ISzAllocatorAlloc* a = (ISzAllocatorAlloc*)p;
        return Allocator::Alloc(a->allocator, size);
    }
    static void _Free(void* p, void* address) {
        ISzAllocatorAlloc* a = (ISzAllocatorAlloc*)p;
        Allocator::Free(a->allocator, address);
    }

    explicit ISzAllocatorAlloc(Allocator* allocator) {
        this->Alloc = _Alloc;
        this->Free = _Free;
        this->allocator = allocator;
    }
};

/* code adapted from https://gnunet.org/svn/gnunet/src/util/crypto_crc.c (public domain) */
static bool crc_table_ready = false;
static u32 crc_table[256];

u32 lzma_crc32(u32 crc32, const u8* data, size_t data_len) {
    if (!crc_table_ready) {
        u32 i, j;
        u32 h = 1;
        crc_table[0] = 0;
        for (i = 128; i; i >>= 1) {
            h = (h >> 1) ^ ((h & 1) ? 0xEDB88320 : 0);
            for (j = 0; j < 256; j += 2 * i) {
                crc_table[i + j] = crc_table[j] ^ h;
            }
        }
        crc_table_ready = true;
    }

    crc32 = crc32 ^ 0xFFFFFFFF;
    while (data_len-- > 0) {
        crc32 = (crc32 >> 8) ^ crc_table[(crc32 ^ *data++) & 0xFF];
    }
    return crc32 ^ 0xFFFFFFFF;
}

// adapted from lzma/C/Lzma86Dec.c
// (main difference: the uncompressed size isn't stored in bytes 6 to 13)

#define LZMA_HEADER_SIZE (1 + LZMA_PROPS_SIZE)

// the first compressed byte indicates whether compression is LZMA (0), LZMA+BJC (1) or none (-1)
static bool Decompress(const u8* compressed, size_t compressedSize, u8* uncompressed, size_t uncompressedSize,
                       Allocator* allocator) {
    if (compressedSize < 1) {
        return false;
    }

    u8 usesX86Filter = compressed[0];
    // handle stored data
    if (usesX86Filter == (u8)-1) {
        if (uncompressedSize != compressedSize - 1) {
            return false;
        }
        memcpy(uncompressed, compressed + 1, compressedSize - 1);
        return true;
    }

    if (compressedSize < LZMA_HEADER_SIZE || usesX86Filter > 1) {
        return false;
    }

    SizeT uncompressedSizeCmp = uncompressedSize;
    SizeT compressedSizeTmp = compressedSize - LZMA_HEADER_SIZE;

    ISzAllocatorAlloc lzmaAlloc(allocator);
    ELzmaStatus status;
    u8* dest = (u8*)uncompressed;
    u8* src = (u8*)(compressed + LZMA_HEADER_SIZE);
    u8* propData = (u8*)(compressed + 1);
    int res = LzmaDecode(dest, &uncompressedSizeCmp, src, &compressedSizeTmp, propData, LZMA_PROPS_SIZE,
                         LZMA_FINISH_END, &status, &lzmaAlloc);

    if (SZ_OK != res || status != LZMA_STATUS_FINISHED_WITH_MARK) {
        return false;
    }
    if (uncompressedSizeCmp != uncompressedSize) {
        return false;
    }

    if (usesX86Filter) {
        UInt32 x86State;
        x86_Convert_Init(x86State);
        x86_Convert((Byte*)uncompressed, uncompressedSize, 0, &x86State, 0);
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
  compressed file data (for each file, the first byte indicates the compression method)

Integers are little-endian.
*/

// magic_id + number of files
#define HEADER_START_SIZE (4 + 4)

// 4 * u32 + FILETIME + name
#define FILE_ENTRY_MIN_SIZE (4 * 4 + 8 + 1)

bool ParseSimpleArchive(const u8* archiveHeader, size_t dataLen, SimpleArchive* archiveOut) {
    if (dataLen < HEADER_START_SIZE) {
        return false;
    }
    if (dataLen > (u32)-1) {
        return false;
    }

    ByteOrderDecoder br(archiveHeader, dataLen, ByteOrderDecoder::LittleEndian);
    u32 magic_id = br.UInt32();
    if (magic_id != LZMA_MAGIC_ID) {
        return false;
    }

    u32 filesCount = br.UInt32();
    archiveOut->filesCount = filesCount;
    if (filesCount > dimof(archiveOut->files)) {
        return false;
    }

    FileInfo* fi;
    for (u32 i = 0; i < filesCount; i++) {
        if (br.Offset() + FILE_ENTRY_MIN_SIZE > dataLen) {
            return false;
        }

        u32 fileHeaderSize = br.UInt32();
        if (fileHeaderSize < FILE_ENTRY_MIN_SIZE || fileHeaderSize > 1024) {
            return false;
        }
        if (br.Offset() + fileHeaderSize - 4 > dataLen) {
            return false;
        }

        fi = &archiveOut->files[i];
        fi->compressedSize = br.UInt32();
        fi->uncompressedSize = br.UInt32();
        fi->uncompressedCrc32 = br.UInt32();
        fi->ftModified.dwLowDateTime = br.UInt32();
        fi->ftModified.dwHighDateTime = br.UInt32();
        fi->name = (char*)archiveHeader + br.Offset();
        br.Skip(fileHeaderSize - FILE_ENTRY_MIN_SIZE);
        if (br.Char() != '\0') {
            return false;
        }
    }

    if (br.Offset() + 4 > dataLen) {
        return false;
    }

    size_t headerSize = br.Offset();
    u32 headerCrc32 = br.UInt32();
    u32 realCrc = lzma_crc32(0, (const u8*)archiveHeader, (u32)headerSize);
    if (headerCrc32 != realCrc) {
        return false;
    }

    for (u32 i = 0; i < filesCount; i++) {
        fi = &archiveOut->files[i];
        // overflow check
        if (fi->compressedSize > dataLen || br.Offset() + fi->compressedSize > dataLen) {
            return false;
        }
        fi->compressedData = archiveHeader + br.Offset();
        br.Skip(fi->compressedSize);
    }

    return br.Offset() == dataLen;
}

int GetIdxFromName(SimpleArchive* archive, const char* fileName) {
    for (int i = 0; i < archive->filesCount; i++) {
        const char* file = archive->files[i].name;
        if (str::Eq(file, fileName)) {
            return i;
        }
    }
    return -1;
}

u8* GetFileDataByIdx(SimpleArchive* archive, int idx, Allocator* allocator) {
    if (idx >= archive->filesCount) {
        return nullptr;
    }

    FileInfo* fi = &archive->files[idx];

    u8* uncompressed = (u8*)Allocator::Alloc(allocator, fi->uncompressedSize);
    if (!uncompressed) {
        return nullptr;
    }

    bool ok = Decompress(fi->compressedData, fi->compressedSize, uncompressed, fi->uncompressedSize, allocator);
    if (!ok) {
        Allocator::Free(allocator, uncompressed);
        return nullptr;
    }

    u32 realCrc = lzma_crc32(0, (const u8*)uncompressed, fi->uncompressedSize);
    if (realCrc != fi->uncompressedCrc32) {
        Allocator::Free(allocator, uncompressed);
        return nullptr;
    }

    return uncompressed;
}

u8* GetFileDataByName(SimpleArchive* archive, const char* fileName, Allocator* allocator) {
    int idx = GetIdxFromName(archive, fileName);
    if (-1 != idx) {
        return GetFileDataByIdx(archive, idx, allocator);
    }
    return nullptr;
}

static bool ExtractFileByIdx(SimpleArchive* archive, int idx, const char* dstDir, Allocator* allocator) {
    FileInfo* fi = &archive->files[idx];

    u8* uncompressed = GetFileDataByIdx(archive, idx, allocator);
    if (!uncompressed) {
        return false;
    }

    bool ok = false;
    char* filePath = path::Join(allocator, dstDir, fi->name);
    if (filePath) {
        ByteSlice d = {(u8*)uncompressed, fi->uncompressedSize};
        ok = file::WriteFile(filePath, d);
    }

    Allocator::Free(allocator, filePath);
    Allocator::Free(allocator, uncompressed);

    return ok;
}

bool ExtractFiles(const char* archivePath, const char* dstDir, const char** files, Allocator* allocator) {
    auto d = file::ReadFileWithAllocator(archivePath, allocator);
    if (d.empty()) {
        return false;
    }

    defer {
        Allocator::Free(allocator, (void*)d.data());
    };

    SimpleArchive archive;
    bool ok = ParseSimpleArchive(d.data(), d.size(), &archive);
    if (!ok) {
        return false;
    }
    for (; *files; files++) {
        int idx = GetIdxFromName(&archive, *files);
        if (-1 != idx) {
            ok &= ExtractFileByIdx(&archive, idx, dstDir, allocator);
        }
    }
    return ok;
}

} // namespace lzma

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include <LzmaDec.h>
#include <Bra.h>
#include "ByteOrderDecoder.h"
#include "LzmaSimpleArchive.h"
#include "File.h"

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
    Arena* a;

    static void* _Alloc(void* p, size_t size) {
        ISzAllocatorAlloc* alloc = (ISzAllocatorAlloc*)p;
        return ::Alloc(alloc->a, size);
    }
    static void _Free(void* p, void* address) {
        ISzAllocatorAlloc* alloc = (ISzAllocatorAlloc*)p;
        ::Free(alloc->a, address);
    }

    explicit ISzAllocatorAlloc(Arena* a) {
        this->Alloc = _Alloc;
        this->Free = _Free;
        this->a = a;
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
                       Arena* a) {
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

    ISzAllocatorAlloc lzmaAlloc(a);
    ELzmaStatus status;
    u8* dest = uncompressed;
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

bool ParseSimpleArchive(const u8* archiveHeader, int dataLen, SimpleArchive* archiveOut) {
    if (dataLen < HEADER_START_SIZE) {
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
        int fileHdrSize = (int)fileHeaderSize;
        if (br.Offset() + fileHdrSize - 4 > dataLen) {
            return false;
        }

        fi = &archiveOut->files[i];
        fi->compressedSize = br.UInt32();
        fi->uncompressedSize = br.UInt32();
        fi->uncompressedCrc32 = br.UInt32();
        fi->ftModified.dwLowDateTime = br.UInt32();
        fi->ftModified.dwHighDateTime = br.UInt32();
        fi->name = Str((char*)archiveHeader + br.Offset());
        br.Skip(fileHdrSize - FILE_ENTRY_MIN_SIZE);
        if (br.Char() != '\0') {
            return false;
        }
    }

    if (br.Offset() + 4 > dataLen) {
        return false;
    }

    int headerSize = br.Offset();
    u32 headerCrc32 = br.UInt32();
    u32 realCrc = lzma_crc32(0, archiveHeader, (u32)headerSize);
    if (headerCrc32 != realCrc) {
        return false;
    }

    for (u32 i = 0; i < filesCount; i++) {
        fi = &archiveOut->files[i];
        // overflow check
        int compressedSize = (int)fi->compressedSize;
        if (compressedSize > dataLen || br.Offset() + compressedSize > dataLen) {
            return false;
        }
        fi->compressedData = archiveHeader + br.Offset();
        br.Skip(compressedSize);
    }

    return br.Offset() == dataLen;
}

int GetIdxFromName(SimpleArchive* archive, Str fileName) {
    for (int i = 0; i < archive->filesCount; i++) {
        if (str::Eq(archive->files[i].name, fileName)) {
            return i;
        }
    }
    return -1;
}

u8* GetFileDataByIdx(SimpleArchive* archive, int idx, Arena* a) {
    if (idx >= archive->filesCount) {
        return nullptr;
    }

    FileInfo* fi = &archive->files[idx];

    // over-allocate by 2 bytes and zero them so the result is always null-terminated
    u8* uncompressed = (u8*)Alloc(a, (size_t)(fi->uncompressedSize + 2));
    if (!uncompressed) {
        return nullptr;
    }
    uncompressed[fi->uncompressedSize] = 0;
    uncompressed[fi->uncompressedSize + 1] = 0;

    bool ok = Decompress(fi->compressedData, fi->compressedSize, uncompressed, fi->uncompressedSize, a);
    if (!ok) {
        Free(a, uncompressed);
        return nullptr;
    }

    u32 realCrc = lzma_crc32(0, (const u8*)uncompressed, fi->uncompressedSize);
    if (realCrc != fi->uncompressedCrc32) {
        Free(a, uncompressed);
        return nullptr;
    }

    return uncompressed;
}

u8* GetFileDataByName(SimpleArchive* archive, Str fileName, Arena* a) {
    int idx = GetIdxFromName(archive, fileName);
    if (-1 != idx) {
        return GetFileDataByIdx(archive, idx, a);
    }
    return nullptr;
}

static bool ExtractFileByIdx(SimpleArchive* archive, int idx, Str dstDir, Arena* a) {
    FileInfo* fi = &archive->files[idx];

    u8* uncompressed = GetFileDataByIdx(archive, idx, a);
    if (!uncompressed) {
        return false;
    }

    bool ok = false;
    Str filePath = path::Join(a, dstDir, fi->name);
    if (filePath) {
        Str d = Str((char*)uncompressed, (int)fi->uncompressedSize);
        ok = file::WriteFile(filePath, d);
    }

    Free(a, filePath.s);
    Free(a, uncompressed);

    return ok;
}

bool ExtractFiles(Str archivePath, Str dstDir, Str* files, Arena* a) {
    auto d = file::ReadFileWithArena(archivePath, a);
    if (len(d) == 0) {
        return false;
    }

    AutoCall freeData(Free, a, (void*)d.s);

    SimpleArchive archive;
    bool ok = ParseSimpleArchive((u8*)d.s, d.len, &archive);
    if (!ok) {
        return false;
    }
    for (; files->s; files++) {
        int idx = GetIdxFromName(&archive, *files);
        if (-1 != idx) {
            ok &= ExtractFileByIdx(&archive, idx, dstDir, a);
        }
    }
    return ok;
}

} // namespace lzma

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Archive.h"
#include "base/Zip.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

static const u8 kEncryptedZip[] = {
    0x50, 0x4b, 0x03, 0x04, 0x33, 0x00, 0x01, 0x00, 0x63, 0x00, 0x6c, 0x6d, 0x44, 0x5d, 0x00, 0x00, 0x00, 0x00,
    0x2a, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x09, 0x00, 0x0b, 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2e,
    0x74, 0x78, 0x74, 0x01, 0x99, 0x07, 0x00, 0x02, 0x00, 0x41, 0x45, 0x03, 0x00, 0x00, 0xa9, 0x0c, 0xde, 0xe8,
    0x9c, 0x3e, 0x3b, 0xb9, 0x0b, 0xb9, 0x42, 0x91, 0x8c, 0x73, 0x66, 0xe3, 0x5c, 0x19, 0xae, 0x6f, 0x3b, 0xe0,
    0xdf, 0x57, 0xe1, 0x6c, 0x99, 0x3c, 0x79, 0x48, 0xbe, 0x42, 0x96, 0xc9, 0x53, 0x4f, 0xf6, 0x32, 0x10, 0xc0,
    0xea, 0x2b, 0x50, 0x4b, 0x01, 0x02, 0x3f, 0x00, 0x33, 0x00, 0x01, 0x00, 0x63, 0x00, 0x6c, 0x6d, 0x44, 0x5d,
    0x00, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x09, 0x00, 0x2f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2e,
    0x74, 0x78, 0x74, 0x0a, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x1f, 0xd8, 0xbb,
    0x8f, 0xf5, 0x53, 0xdd, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x99, 0x07, 0x00, 0x02, 0x00, 0x41, 0x45, 0x03, 0x00, 0x00, 0x50, 0x4b, 0x05, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x66, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void ArchiveTest() {
#if !OS_WASM
    Archive archive;
    archive.password = str::Dup(StrL("secret"));
    Str data{(char*)kEncryptedZip, dimof(kEncryptedZip)};
    utassert(archive.OpenFromData(data));
    utassert(archive.isEncrypted);
    auto* fi = archive.GetFileDataByName(StrL("hello.txt"));
    utassert(fi);
    if (!fi) {
        return;
    }
    utassert(str::Eq(Str(fi->data, fi->fileSizeUncompressed), StrL("secret payload")));
#endif
}

// flags every zip header as UTF-8 and puts an invalid UTF-8 byte in name, so
// libarchive returns ARCHIVE_WARN for that entry's header
static void MakeZipNameInvalidUtf8(Str zip, Str name) {
    u8* d = (u8*)zip.s;
    int n = len(zip);
    for (int i = 0; i + 4 <= n; i++) {
        bool local = d[i] == 'P' && d[i + 1] == 'K' && d[i + 2] == 3 && d[i + 3] == 4;
        bool central = d[i] == 'P' && d[i + 1] == 'K' && d[i + 2] == 1 && d[i + 3] == 2;
        int flagsOff = local ? 7 : (central ? 9 : 0);
        if (flagsOff && i + flagsOff < n) {
            d[i + flagsOff] |= 0x08; // bit 11: name is UTF-8
        }
        if (i + len(name) <= n && memcmp(d + i, name.s, (size_t)len(name)) == 0) {
            d[i + 1] = 0xff;
        }
    }
}

// both entries, in whatever order the directory listing gave them
static bool HasBothEntries(Archive& archive) {
    if (len(archive.GetFileInfos()) != 2) {
        return false;
    }
    str::Builder all;
    for (int i = 0; i < 2; i++) {
        auto* fi = archive.GetFileDataById(i);
        if (!fi || !fi->data) {
            return false;
        }
        all.Append(Str(fi->data, fi->fileSizeUncompressed));
    }
    Str got = ToStr(all);
    return str::Eq(got, StrL("firstsecond")) || str::Eq(got, StrL("secondfirst"));
}

void ArchiveWarnTest() {
#if !OS_WASM
    TempStr dir = GetTempFilePathTemp(StrL("zipwarn"));
    file::Delete(dir);
    utassert(dir::Create(dir));
    utassert(file::WriteFile(path::JoinTemp(dir, StrL("aaaa.txt")), StrL("first")));
    utassert(file::WriteFile(path::JoinTemp(dir, StrL("bbbb.txt")), StrL("second")));
    Str zip = ZipDirToData(dir, false);
    dir::RemoveAll(dir);
    utassert(len(zip) > 0);
    MakeZipNameInvalidUtf8(zip, StrL("aaaa.txt"));
    MakeZipNameInvalidUtf8(zip, StrL("bbbb.txt"));

    // a warning on an entry's header must not end the listing
    Archive archive;
    utassert(archive.OpenFromData(zip));
    utassert(HasBothEntries(archive));

    // lazy loading re-reads the headers to reach an entry
    Archive lazy;
    utassert(lazy.OpenFromData(zip, false));
    utassert(HasBothEntries(lazy));
    str::Free(zip);
#endif
}

// a ustar archive with one entry
static Str MakeTar(Str name, Str content) {
    constexpr int kBlock = 512;
    int dataBlocks = (len(content) + kBlock - 1) / kBlock;
    int size = kBlock * (1 + dataBlocks + 2);
    char* d = AllocArray<char>(size);
    memcpy(d, name.s, (size_t)len(name));
    memcpy(d + 100, "0000644", 7);
    memcpy(d + 108, "0000000", 7);
    memcpy(d + 116, "0000000", 7);
    snprintf(d + 124, 12, "%011o", (unsigned)len(content));
    memcpy(d + 136, "00000000000", 11);
    d[156] = '0';
    memcpy(d + 257, "ustar", 6);
    memcpy(d + 263, "00", 2);
    memset(d + 148, ' ', 8);
    unsigned sum = 0;
    for (int i = 0; i < kBlock; i++) {
        sum += (u8)d[i];
    }
    snprintf(d + 148, 8, "%06o", sum);
    memcpy(d + kBlock, content.s, (size_t)len(content));
    return Str(d, size);
}

// a tar file opened lazily stays lazy: entries are read on demand
void ArchiveTarLazyTest() {
#if !OS_WASM
    Str tar = MakeTar(StrL("page.txt"), StrL("tar content"));
    TempStr path = GetTempFilePathTemp(StrL("lazytar"));
    utassert(file::WriteFile(path, tar));

    Archive archive;
    ArchiveExtractProgressCb emptyCb;
    utassert(archive.Open(path, false, FileType::Tar, emptyCb));
    auto& infos = archive.GetFileInfos();
    utassert(len(infos) == 1 && infos[0]->data == nullptr);
    auto* fi = archive.GetFileDataById(0);
    utassert(fi && fi->data && str::Eq(Str(fi->data, fi->fileSizeUncompressed), StrL("tar content")));

    Archive fromData;
    utassert(fromData.OpenFromData(tar, false));
    utassert(len(fromData.GetFileInfos()) == 1 && fromData.GetFileInfos()[0]->data == nullptr);

    file::Delete(path);
    str::Free(tar);
#endif
}

#if OS_WIN
static void PutU16(u8* d, u32 v) {
    d[0] = (u8)v;
    d[1] = (u8)(v >> 8);
}

static void PutU32(u8* d, u32 v) {
    PutU16(d, v);
    PutU16(d + 2, v >> 16);
}

static u32 Crc32(const u8* d, int n) {
    u32 crc = 0xffffffff;
    for (int i = 0; i < n; i++) {
        crc ^= d[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xedb88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

// RAR 4 block CRC: low 16 bits of crc32 over the block after the CRC field
static void SetRarBlockCrc(u8* block, int size) {
    u32 crc = Crc32(block + 2, size - 2);
    PutU16(block, crc & 0xffff);
}

// a RAR 4 archive with one stored entry whose header claims unpSize bytes
static Str MakeRarWithUnpSize(u32 unpSize) {
    static const u8 kMarker[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00};
    constexpr int kMainSize = 13;
    constexpr int kNameLen = 5;
    constexpr int kFileHdrSize = 32 + kNameLen;
    constexpr int kPackSize = 4;
    constexpr int kEndSize = 7;
    constexpr int kSize = (int)sizeof(kMarker) + kMainSize + kFileHdrSize + kPackSize + kEndSize;
    u8* d = AllocArray<u8>(kSize);
    u8* p = d;
    memcpy(p, kMarker, sizeof(kMarker));
    p += sizeof(kMarker);

    p[2] = 0x73;
    PutU16(p + 5, kMainSize);
    SetRarBlockCrc(p, kMainSize);
    p += kMainSize;

    p[2] = 0x74;
    PutU16(p + 3, 0x8000);
    PutU16(p + 5, kFileHdrSize);
    PutU32(p + 7, kPackSize);
    PutU32(p + 11, unpSize);
    p[15] = 2;    // Windows
    p[24] = 20;   // unpack version 2.0
    p[25] = 0x30; // stored
    PutU16(p + 26, kNameLen);
    PutU32(p + 28, 0x20); // archive attribute
    memcpy(p + 32, "a.txt", kNameLen);
    SetRarBlockCrc(p, kFileHdrSize);
    p += kFileHdrSize;
    memcpy(p, "data", kPackSize);
    p += kPackSize;

    p[2] = 0x7b;
    PutU16(p + 3, 0x4000);
    PutU16(p + 5, kEndSize);
    SetRarBlockCrc(p, kEndSize);
    return Str((char*)d, kSize);
}
#endif

// an entry size that doesn't fit an int must not open as a negative size
void ArchiveRarHugeEntryTest() {
#if OS_WIN
    Str rar = MakeRarWithUnpSize(0xfffffff0);
    TempStr path = GetTempFilePathTemp(StrL("hugerar"));
    utassert(file::WriteFile(path, rar));
    Archive archive;
    ArchiveExtractProgressCb emptyCb;
    bool ok = archive.Open(path, false, FileType::Unknown, emptyCb);
    for (auto* fi : archive.GetFileInfos()) {
        utassert(fi->fileSizeUncompressed >= 0);
    }
    utassert(!ok);

    // the same archive with a sane size lists through unrar
    str::Free(rar);
    rar = MakeRarWithUnpSize(4);
    utassert(file::WriteFile(path, rar));
    Archive sane;
    utassert(sane.Open(path, false, FileType::Unknown, emptyCb));
    utassert(sane.format == Archive::Format::Rar && len(sane.GetFileInfos()) == 1);
    file::Delete(path);
    str::Free(rar);
#endif
}

void ZipDirTest() {
#if !OS_WASM
    TempStr dir = GetTempFilePathTemp(StrL("zipdir"));
    file::Delete(dir);
    utassert(dir::Create(dir));
    utassert(file::WriteFile(path::JoinTemp(dir, StrL("empty.txt")), Str()));
    utassert(file::WriteFile(path::JoinTemp(dir, StrL("data.txt")), StrL("hello")));

    Str zip = ZipDirToData(dir, false);
    utassert(len(zip) > 0);
    Archive archive;
    utassert(archive.OpenFromData(zip));
    auto* fi = archive.GetFileDataByName(StrL("empty.txt"));
    utassert(fi && fi->fileSizeUncompressed == 0);
    fi = archive.GetFileDataByName(StrL("data.txt"));
    utassert(fi && str::Eq(Str(fi->data, fi->fileSizeUncompressed), StrL("hello")));
    str::Free(zip);

    // a trailing separator on the dir must not reject its files
    zip = ZipDirToData(str::JoinTemp(dir, StrL(kPathSep)), false);
    utassert(len(zip) > 0);
    Archive archive2;
    utassert(archive2.OpenFromData(zip));
    utassert(archive2.GetFileDataByName(StrL("data.txt")));
    str::Free(zip);

    dir::RemoveAll(dir);
#endif
}

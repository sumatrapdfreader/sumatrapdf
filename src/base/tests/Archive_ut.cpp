/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
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

/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/LzmaSimpleArchive.h"

#include "mupdf/noto_sumatra.h"
#include "EmbeddedResources.h"

void fz_set_builtin_font_loader(__unused fz_builtin_font_loader loader) {}

// this tool links the loader but not mupdf, where fonts_map.c defines it
extern "C" const char* sumatra_lookup_font_url(const char*) {
    return nullptr;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: test_embedded <archive> <staging dir>\n");
        return 1;
    }
    Str raw = GetEmbeddedLzsa();
    Str expectedArchive = file::ReadFile(Str(argv[1]));
    AutoCall freeArchive(free, (void*)expectedArchive.s);
    if (len(raw) != len(expectedArchive) || !MemEq(raw.s, expectedArchive.s, len(raw))) {
        fprintf(stderr, "embedded archive differs from packed archive\n");
        return 1;
    }
    lzma::SimpleArchive* archive = GetEmbeddedArchive();
    if (!archive) {
        return 1;
    }
    int size = -1;
    if (GetEmbeddedFileData(StrL("missing-file"), &size) || size != 0) {
        return 1;
    }
    for (int i = 0; i < archive->filesCount; i++) {
        TempStr name = str::DupTemp(archive->files[i].name);
        str::TransCharsInPlace(name, StrL("\\"), StrL("/"));
        Str expected = file::ReadFile(path::JoinTemp(Str(argv[2]), name));
        AutoCall freeExpected(free, (void*)expected.s);
        AutoFree<u8> data(GetEmbeddedFileData(name, &size));
        if (!data || size != len(expected) || !MemEq(data, expected.s, size) || data[size] || data[size + 1]) {
            fprintf(stderr, "incorrect extraction: %s\n", CStrTemp(name));
            return 1;
        }
    }
    printf("Verified %d embedded files\n", archive->filesCount);
    return 0;
}

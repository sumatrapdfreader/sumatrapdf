/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/LzmaSimpleArchive.h"

#include "resource.h"
#include "EmbeddedResources.h"

static LoadedDataResource gEmbeddedData{};
static lzma::SimpleArchive gEmbeddedArchive{};
static bool gEmbeddedTried = false;

bool EnsureEmbeddedArchiveLoaded() {
    if (gEmbeddedTried) {
        return gEmbeddedArchive.filesCount > 0;
    }
    gEmbeddedTried = true;
    if (!LockDataResource(IDR_EMBEDDED_PAK, &gEmbeddedData)) {
        logf("EnsureEmbeddedArchiveLoaded: LockDataResource(IDR_EMBEDDED_PAK) failed\n");
        return false;
    }
    if (!lzma::ParseSimpleArchive(gEmbeddedData.data, gEmbeddedData.dataSize, &gEmbeddedArchive)) {
        logf("EnsureEmbeddedArchiveLoaded: ParseSimpleArchive failed (size=%d)\n", gEmbeddedData.dataSize);
        gEmbeddedArchive.filesCount = 0;
        return false;
    }
    logf("EnsureEmbeddedArchiveLoaded: %d files in embedded.dat (%d bytes)\n", gEmbeddedArchive.filesCount,
         gEmbeddedData.dataSize);
    return gEmbeddedArchive.filesCount > 0;
}

lzma::SimpleArchive* GetEmbeddedArchive() {
    if (!EnsureEmbeddedArchiveLoaded()) {
        return nullptr;
    }
    return &gEmbeddedArchive;
}

u8* GetEmbeddedFileData(Str name, int* outSize) {
    if (outSize) {
        *outSize = 0;
    }
    if (len(name) == 0 || !EnsureEmbeddedArchiveLoaded()) {
        return nullptr;
    }
    int idx = lzma::GetIdxFromName(&gEmbeddedArchive, name);
    if (idx < 0) {
        return nullptr;
    }
    u8* data = lzma::GetFileDataByIdx(&gEmbeddedArchive, idx, nullptr);
    if (!data) {
        return nullptr;
    }
    if (outSize) {
        *outSize = (int)gEmbeddedArchive.files[idx].uncompressedSize;
    }
    return data;
}

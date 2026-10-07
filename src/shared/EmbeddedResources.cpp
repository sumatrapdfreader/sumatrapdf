/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#if OS_WIN
#include "base/Win.h"
#include "base/File.h"
#endif
#include "base/LzmaSimpleArchive.h"

#include "mupdf/noto_sumatra.h"

#if OS_WIN
#include "resource.h"
#endif
#include "EmbeddedResources.h"

#if OS_WIN
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

static LoadedDataResource gEmbeddedData{};
#else
extern "C" const u8 gEmbeddedLzsa[];
extern "C" const u8 gEmbeddedLzsaEnd[];
#endif

static lzma::SimpleArchive gEmbeddedArchive{};
static bool gEmbeddedTried = false;

#if OS_WIN
// PdfPreview.dll, PdfFilter.dll and sumatrapdf-tool.exe carry no archive of
// their own: they read the one in SumatraPDF.exe installed next to them.
static HMODULE GetArchiveModule() {
    HMODULE self = (HMODULE)&__ImageBase;
    if (FindResourceW(self, MAKEINTRESOURCEW(IDR_EMBEDDED_PAK), RT_RCDATA)) {
        return self;
    }
    TempStr path = GetPathInExeDirTemp(StrL("SumatraPDF.exe"));
    WStr wpath = ToWStrTemp(path);
    return LoadLibraryExW(wpath.s, nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
}
#endif

// Returns a read-only view valid for the lifetime of the executable.
Str GetEmbeddedLzsa() {
#if OS_WIN
    if (!gEmbeddedData.data && !LockDataResource(IDR_EMBEDDED_PAK, &gEmbeddedData, GetArchiveModule())) {
        return {};
    }
    return Str((char*)gEmbeddedData.data, gEmbeddedData.dataSize);
#else
    return Str((char*)gEmbeddedLzsa, (int)((uintptr_t)gEmbeddedLzsaEnd - (uintptr_t)gEmbeddedLzsa));
#endif
}

bool EnsureEmbeddedArchiveLoaded() {
    if (gEmbeddedTried) {
        return gEmbeddedArchive.filesCount > 0;
    }
    gEmbeddedTried = true;
    Str data = GetEmbeddedLzsa();
    if (!lzma::ParseSimpleArchive((const u8*)data.s, len(data), &gEmbeddedArchive)) {
        logf("EnsureEmbeddedArchiveLoaded: ParseSimpleArchive failed (size=%d)\n", len(data));
        gEmbeddedArchive.filesCount = 0;
        return false;
    }
    logf("EnsureEmbeddedArchiveLoaded: %d files in IDR_EMBEDDED_PAK (%d bytes)\n", gEmbeddedArchive.filesCount,
         len(data));
    return gEmbeddedArchive.filesCount > 0;
}

lzma::SimpleArchive* GetEmbeddedArchive() {
    if (!EnsureEmbeddedArchiveLoaded()) {
        return nullptr;
    }
    return &gEmbeddedArchive;
}

// Returns malloc'd, NUL-terminated data; the caller frees it.
u8* GetEmbeddedFileData(Str name, int* outSize) {
    if (outSize) {
        *outSize = 0;
    }
    if (len(name) == 0 || !EnsureEmbeddedArchiveLoaded()) {
        return nullptr;
    }
    if (str::Contains(name, StrL("/"))) {
        name = str::DupTemp(name);
        str::TransCharsInPlace(name, StrL("/"), StrL("\\"));
    }
    int idx = lzma::GetIdxFromName(&gEmbeddedArchive, name);
    u8* data = lzma::GetFileDataByIdx(&gEmbeddedArchive, idx, nullptr);
    if (data && outSize) {
        *outSize = (int)gEmbeddedArchive.files[idx].uncompressedSize;
    }
    return data;
}

struct EmbeddedFont {
    EmbeddedFont* next;
    Str name;
    u8* data;
    int size;
};

static Mutex gFontsMutex;
static EmbeddedFont* gFonts = nullptr;

// mupdf's built-in fonts, from fonts\<name> in the archive (src/mupdf/noto_sumatra.c).
// Each is unpacked once and kept for the life of the process: mupdf holds on to
// the pointer. Misses are remembered too, as the table names fonts we don't pack.
static const u8* LoadEmbeddedFont(const char* fileName, int* size) {
    Str name(fileName);
    AutoUnlockMutex lock(&gFontsMutex);
    for (EmbeddedFont* f = gFonts; f; f = f->next) {
        if (str::Eq(f->name, name)) {
            *size = f->size;
            return f->data;
        }
    }
    EmbeddedFont* f = AllocStruct<EmbeddedFont>();
    f->name = str::Dup(name);
    f->data = GetEmbeddedFileData(fmt("fonts\\%s", name), &f->size);
    f->next = gFonts;
    gFonts = f;
    *size = f->size;
    return f->data;
}

void InstallEmbeddedFontLoader() {
    fz_set_builtin_font_loader(LoadEmbeddedFont);
}

/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Zip.h"

#include "mupdf/noto_sumatra.h"

#include "EmbeddedResources.h"

#include "SumatraLog.h"

// ng: orig unpacks the LzSA resource IDR_EMBEDDED_PAK. Here the same files are
// gzipped byte arrays cmd/gen-embedded.ts generated, so the data is the same on
// every platform and needs no .rc.

struct EmbeddedTable {
    const EmbeddedBlob* blobs;
    const int* count;
};

static const EmbeddedTable gTables[] = {
    {gEmbeddedText, &gEmbeddedTextCount},           {gEmbeddedMermaid, &gEmbeddedMermaidCount},
    {gEmbeddedFontsUrw, &gEmbeddedFontsUrwCount},   {gEmbeddedFontsSil, &gEmbeddedFontsSilCount},
    {gEmbeddedFontsNoto, &gEmbeddedFontsNotoCount}, {gEmbeddedFontsDroid, &gEmbeddedFontsDroidCount},
    {gEmbeddedManual, &gEmbeddedManualCount},
};

static const EmbeddedBlob* FindBlob(Str name) {
    for (const EmbeddedTable& t : gTables) {
        for (int i = 0; i < *t.count; i++) {
            const EmbeddedBlob* b = &t.blobs[i];
            if (str::Eq(name, Str(b->name))) {
                return b;
            }
        }
    }
    return nullptr;
}

bool EnsureEmbeddedArchiveLoaded() {
    return true;
}

lzma::SimpleArchive* GetEmbeddedArchive() {
    return nullptr;
}

u8* GetEmbeddedFileData(Str name, int* outSize) {
    if (outSize) {
        *outSize = 0;
    }
    const EmbeddedBlob* b = FindBlob(name);
    if (!b) {
        logf("GetEmbeddedFileData: no embedded file '%s'\n", name);
        return nullptr;
    }
    Str gz((char*)b->gz, b->gzSize);
    Str d = Ungzip(gz, b->size + 1);
    if (len(d) != b->size) {
        logf("GetEmbeddedFileData: '%s' unzipped to %d, expected %d\n", name, len(d), b->size);
        free(d.s);
        return nullptr;
    }
    if (outSize) {
        *outSize = d.len;
    }
    return (u8*)d.s;
}

struct EmbeddedFont {
    EmbeddedFont* next;
    Str name;
    u8* data;
    int size;
};

static Mutex gFontsMutex;
static EmbeddedFont* gFonts = nullptr;

// mupdf's built-in fonts, from fonts/<name> (src/mupdf/noto_sumatra.c). Each is
// unpacked once and kept for the life of the process: mupdf holds on to the
// pointer. Misses are remembered too, as the table names fonts we don't pack.
static const u8* LoadEmbeddedFont(const char* fileName, int* size) {
    Str name(fileName);
    ScopedMutex lock(&gFontsMutex);
    for (EmbeddedFont* f = gFonts; f; f = f->next) {
        if (str::Eq(f->name, name)) {
            *size = f->size;
            return f->data;
        }
    }
    EmbeddedFont* f = AllocStruct<EmbeddedFont>();
    f->name = str::Dup(name);
    f->data = GetEmbeddedFileData(fmt("fonts/%s", name), &f->size);
    f->next = gFonts;
    gFonts = f;
    *size = f->size;
    return f->data;
}

void InstallEmbeddedFontLoader() {
    fz_set_builtin_font_loader(LoadEmbeddedFont);
}

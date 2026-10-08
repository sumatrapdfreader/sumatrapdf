/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "base/File.h"
#include "base/Http.h"
#include "base/LzmaSimpleArchive.h"

#include "Version.h"
#if OS_WASM
#include <emscripten/emscripten.h>

#include "gui/WasmBridge.h"
#endif
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

#if OS_WASM
// Sync so a font lookup, which mupdf makes on the calling thread, can wait.
// The response is buffered in JS and written whole, so a failed request leaves
// no half file. /fonts is an OPFS root (src/gui/WasmShell.js).
EM_JS(int, WasmDownloadFont, (const char* url, int urlLen, const char* path, int pathLen), {
    var u = UTF8ToString(url, urlLen);
    var p = UTF8ToString(path, pathLen);
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", u, false);
        xhr.overrideMimeType("text/plain; charset=x-user-defined");
        xhr.send(null);
        if (xhr.status != 200) {
            console.error("font download failed", xhr.status, u);
            return 0;
        }
        var text = xhr.responseText;
        var bytes = new Uint8Array(text.length);
        for (var i = 0; i < text.length; i++) {
            bytes[i] = text.charCodeAt(i) & 255;
        }
        FS.writeFile(p, bytes);
        return bytes.length;
    } catch (e) {
        console.error("font download failed", u, e);
        try {
            FS.unlink(p);
        } catch (e2) {
        }
        return 0;
    }
});
#endif

// %LOCALAPPDATA%\SumatraPDF\fonts, or the XDG / macOS equivalent. Not the
// settings directory: a portable exe and the preview dll share this cache.
// Wasm uses /fonts, which the page mirrors to OPFS.
static TempStr FontCacheDirTemp() {
#if OS_WASM
    return str::DupTemp(StrL("/fonts"));
#elif OS_WIN
    TempStr dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true);
    if (len(dir) == 0) {
        dir = GetTempDirTemp();
    }
    return path::JoinTemp(dir, StrL(kAppName), StrL("fonts"));
#else
    const char* home = getenv("HOME");
#if OS_DARWIN
    if (home && *home) {
        TempStr dir = path::JoinTemp(Str((char*)home), StrL("Library/Application Support"));
        dir = path::JoinTemp(dir, StrL(kAppName));
        return path::JoinTemp(dir, StrL("fonts"));
    }
#else
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        return path::JoinTemp(Str((char*)xdg), StrL(kAppName), StrL("fonts"));
    }
    if (home && *home) {
        TempStr dir = path::JoinTemp(Str((char*)home), StrL(".config"));
        dir = path::JoinTemp(dir, StrL(kAppName));
        return path::JoinTemp(dir, StrL("fonts"));
    }
#endif
    const char* tmp = getenv("TMPDIR");
    TempStr dir = path::JoinTemp(Str((char*)(tmp && *tmp ? tmp : "/tmp")), StrL(kAppName));
    return path::JoinTemp(dir, StrL("fonts"));
#endif
}

static Str ReadKeptFont(Str path) {
    Str data = file::ReadFile(path);
    if (len(data) <= 0) {
        str::Free(data);
        return {};
    }
    return data;
}

static bool DownloadFont(Str url, Str dest) {
    if (!dir::CreateForFile(dest)) {
        logf("font cache: cannot create '%s'\n", dest);
        return false;
    }
#if OS_WASM
    if (WasmDownloadFont(url.s, len(url), dest.s, len(dest)) <= 0) {
        return false;
    }
    WasmPersistSettings();
    return true;
#else
    TempStr part = str::JoinTemp(dest, StrL(".part"));
    Func1<HttpProgress*> progress;
    if (!HttpGetToFile(url, part, progress)) {
        file::Delete(part);
        return false;
    }
    if (!file::RenameReplace(dest, part)) {
        file::Delete(part);
        return false;
    }
    return true;
#endif
}

// Embedded archive first, then the cache file named by the URL, then a download
// into that cache. The bytes are kept for the process: mupdf holds the pointer.
// A miss is remembered too, so a font we cannot get is not retried.
static void LoadFontFromCacheOrNet(Str name, EmbeddedFont* f) {
    const char* url = sumatra_lookup_font_url(CStrTemp(name));
    if (!url || !url[0]) {
        return;
    }
    const char* slash = strrchr(url, '/');
    const char* file = slash ? slash + 1 : url;
    if (!file[0]) {
        return;
    }
    TempStr path = path::JoinTemp(FontCacheDirTemp(), Str(file));
    Str data = ReadKeptFont(path);
    if (len(data) == 0) {
        logf("font '%s': downloading\n", name);
        if (!DownloadFont(Str(url), path)) {
            logf("font '%s': download failed\n", name);
            return;
        }
        data = ReadKeptFont(path);
    }
    f->data = (u8*)data.s;
    f->size = len(data);
}

// mupdf's built-in fonts (src/mupdf/noto_sumatra.c). Each is loaded once.
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
    if (!f->data) {
        LoadFontFromCacheOrNet(name, f);
    }
    ListInsertFront(&gFonts, f);
    *size = f->size;
    return f->data;
}

void InstallEmbeddedFontLoader() {
    fz_set_builtin_font_loader(LoadEmbeddedFont);
}

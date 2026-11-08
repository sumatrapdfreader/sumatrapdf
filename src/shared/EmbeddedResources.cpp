/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "base/Crypto.h"
#include "base/File.h"
#include "base/Http.h"
#include "base/Timer.h"
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
// render threads load fonts from it while the ui thread reads other files
static Mutex gEmbeddedMutex;

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
    // without the lock a second thread saw gEmbeddedTried before the parse
    // finished and took the archive for empty
    AutoUnlockMutex lock(&gEmbeddedMutex);
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
    // Node (console targets, tests) has no XHR: SUMATRA_FONT_MIRROR is a directory
    // with the files under their URL names (bun cmd/upload-fonts.ts -mirror)
    if (typeof XMLHttpRequest == "undefined") {
        var mirror = typeof process != "undefined" && process.env ? process.env.SUMATRA_FONT_MIRROR : "";
        if (!mirror) return 0;
        try {
            var bytes = FS.readFile(mirror + "/" + u.split("/").pop());
            FS.writeFile(p, bytes);
            return bytes.length;
        } catch (e) {
            console.error("font not in SUMATRA_FONT_MIRROR", u, e.message || e);
            return 0;
        }
    }
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

// "url\tpath\n" lines. fetch() is async, so this doesn't block the tab the way
// WasmDownloadFont does; a font mupdf asks for before it arrives is still
// downloaded then.
EM_JS(void, WasmJsPrefetchFonts, (const char* list, int listLen), {
    var lines = UTF8ToString(list, listLen).split("\n").filter(function(l) { return l; });
    var have = function(p) {
        try {
            FS.stat(p);
            return true;
        } catch (e) {
            return false;
        }
    };
    var one = function(line) {
        var parts = line.split("\t");
        var url = parts[0];
        var path = parts[1];
        return fetch(url)
            .then(function(r) { return r.ok ? r.arrayBuffer() : null; })
            .then(function(b) {
                if (!b || have(path)) return false;
                FS.writeFile(path, new Uint8Array(b));
                return true;
            })
            .catch(function(e) {
                console.warn("prefetching font failed", url, e);
                return false;
            });
    };
    Promise.all(lines.map(one)).then(function(wrote) {
        if (wrote.indexOf(true) >= 0 && Module.sumatraStorageSave) Module.sumatraStorageSave();
    });
});

// a browser has no environment; Node (console targets) does
EM_JS(void, WasmJsGetEnv, (const char* name, int nameLen, char* out, int outCap), {
    var v = typeof process != "undefined" && process.env ? process.env[UTF8ToString(name, nameLen)] : "";
    stringToUTF8(v ? v : "", out, outCap);
});
#endif

// Test overrides, e.g. tests/font-download.ts:
//   SUMATRA_FONT_CACHE     the font cache directory
//   SUMATRA_FONT_URL_BASE  where fonts download from instead of R2 (desktop)
//   SUMATRA_FONT_MIRROR    a directory Node reads them from (wasm, no XHR)
static TempStr FontEnvTemp(Str name) {
#if OS_WASM
    char buf[1024];
    WasmJsGetEnv(name.s, len(name), buf, (int)sizeof(buf));
    return str::DupTemp(Str(buf));
#elif OS_WIN
    return GetEnvVariableTemp(name);
#else
    const char* v = getenv(CStrTemp(name));
    return str::DupTemp(Str((char*)(v ? v : "")));
#endif
}

// %LOCALAPPDATA%\SumatraPDF\fonts, or the XDG / macOS equivalent. Not the
// settings directory: a portable exe and the preview dll share this cache.
// Wasm uses /fonts, which the page mirrors to OPFS.
static TempStr FontCacheDirTemp() {
    TempStr testDir = FontEnvTemp(StrL("SUMATRA_FONT_CACHE"));
    if (len(testDir) > 0) {
        return testDir;
    }
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

static FontDownloadProgressFn gFontDownloadProgressFn = nullptr;

// the app shows download progress; the preview dll and the tool don't set it
void SetFontDownloadProgressFn(FontDownloadProgressFn fn) {
    gFontDownloadProgressFn = fn;
}

// a font's size from fonts_map.c, 0 if it isn't there
static i64 FontMapSize(Str name) {
    int n = sumatra_font_count();
    for (int i = 0; i < n; i++) {
        const char* url;
        int size;
        if (str::Eq(Str(sumatra_font_at(i, &url, &size)), name)) {
            return size;
        }
    }
    return 0;
}

static void ReportFontDownload(Str name, i64 nDownloaded, i64 total, FontDownloadEvent ev) {
    if (gFontDownloadProgressFn) {
        gFontDownloadProgressFn(name, nDownloaded, total, ev);
    }
}

#if !OS_WASM
constexpr int kFontProgressMs = 200;

struct FontDownloadProgress {
    Str name;
    i64 total = 0;
    TimeStamp lastReport{};
};

// HttpGetToFile calls this per chunk; the app hears about it at most every kFontProgressMs
static void OnFontDownloadChunk(FontDownloadProgress* p, HttpProgress* hp) {
    if (TimeSinceInMs(p->lastReport) < kFontProgressMs) {
        return;
    }
    p->lastReport = TimeGet();
    ReportFontDownload(p->name, hp->nDownloaded, p->total, FontDownloadEvent::Progress);
}
#endif

static bool DownloadFont(Str name, Str url, Str dest) {
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
    FontDownloadProgress p;
    p.name = name;
    p.total = FontMapSize(name);
    p.lastReport = TimeGet();
    ReportFontDownload(name, 0, p.total, FontDownloadEvent::Progress);
    auto progress = MkFunc1<FontDownloadProgress, HttpProgress*>(OnFontDownloadChunk, &p);
    bool ok = HttpGetToFile(url, part, progress) && file::RenameReplace(dest, part);
    if (!ok) {
        file::Delete(part);
    }
    ReportFontDownload(name, ok ? p.total : 0, p.total, ok ? FontDownloadEvent::Done : FontDownloadEvent::Failed);
    return ok;
#endif
}

#if OS_WASM
// The base 14 PDF fonts, which documents most often use without embedding them,
// and Charis SIL, the default EPUB font. Fetched in the background on startup so
// opening such a document doesn't wait on a synchronous download.
void PrefetchCommonFonts() {
    static const char* names[] = {
        "NimbusSans-Regular.cff",
        "NimbusSans-Bold.cff",
        "NimbusSans-Italic.cff",
        "NimbusSans-BoldItalic.cff",
        "NimbusRoman-Regular.cff",
        "NimbusRoman-Bold.cff",
        "NimbusRoman-Italic.cff",
        "NimbusRoman-BoldItalic.cff",
        "NimbusMonoPS-Regular.cff",
        "NimbusMonoPS-Bold.cff",
        "NimbusMonoPS-Italic.cff",
        "NimbusMonoPS-BoldItalic.cff",
        "StandardSymbolsPS.cff",
        "Dingbats.cff",
        "CharisSIL.cff",
        "CharisSIL-Bold.cff",
        "CharisSIL-Italic.cff",
        "CharisSIL-BoldItalic.cff",
    };
    TempStr dir = FontCacheDirTemp();
    dir::CreateAll(dir);
    str::Builder list;
    for (const char* name : names) {
        const char* url = sumatra_lookup_font_url(name);
        const char* slash = url ? strrchr(url, '/') : nullptr;
        if (!slash || !slash[1]) {
            continue;
        }
        TempStr path = path::JoinTemp(dir, Str(slash + 1));
        if (file::Exists(path)) {
            continue;
        }
        list.Append(Str((char*)url));
        list.AppendChar('\t');
        list.Append(path);
        list.AppendChar('\n');
    }
    Str s = ToStr(list);
    if (len(s) == 0) {
        return;
    }
    WasmJsPrefetchFonts(s.s, len(s));
}
#endif

// A font's URL file name ends in a prefix of its sha1 (DroidSansFallbackFull-f3cd.ttf,
// cmd/upload-fonts.ts). Data that doesn't match, e.g. a proxy's error page, is not
// a font. A name without such a suffix can't be checked and passes.
bool FontMatchesFileName(Str fileName, Str data) {
    // "<name>-<hex>.<ext>"
    int end = len(fileName);
    while (end > 0 && fileName.s[end - 1] != '.') {
        end--;
    }
    end--;
    int start = end;
    while (start > 0 && fileName.s[start - 1] != '-') {
        start--;
    }
    int hexLen = end - start;
    if (end <= 0 || start <= 0 || hexLen < 4 || hexLen > 8) {
        return true;
    }
    Str hex(fileName.s + start, hexLen);
    for (int i = 0; i < hexLen; i++) {
        if (!isxdigit((u8)hex.s[i])) {
            return true;
        }
    }
    u8 digest[20];
    CalcSHA1Digest(data, digest);
    TempStr sha1 = str::MemToHexTemp(Str((char*)digest, (int)sizeof(digest)));
    return str::StartsWithI(sha1, hex);
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
    if (len(data) > 0 && !FontMatchesFileName(Str(file), data)) {
        logf("font '%s': cached file is damaged, downloading again\n", name);
        str::Free(data);
        data = {};
        file::Delete(path);
    }
    if (len(data) == 0) {
        TempStr dlUrl = Str((char*)url);
        TempStr base = FontEnvTemp(StrL("SUMATRA_FONT_URL_BASE"));
        if (len(base) > 0) {
            dlUrl = str::JoinTemp(base, StrL("/"), Str(file));
        }
        logf("font '%s': downloading\n", name);
        if (!DownloadFont(name, dlUrl, path)) {
            logf("font '%s': download failed\n", name);
            return;
        }
        data = ReadKeptFont(path);
        if (len(data) > 0 && !FontMatchesFileName(Str(file), data)) {
            logf("font '%s': downloaded file doesn't match its sha1, not kept\n", name);
            ReportFontDownload(name, 0, 0, FontDownloadEvent::Failed);
            str::Free(data);
            file::Delete(path);
            return;
        }
    }
    f->data = (u8*)data.s;
    f->size = len(data);
}

// mupdf asks for these fallback fonts directly, without trying system fonts
// first as it does for scripts. The system usually has a font covering the
// same blocks (math, music, symbols, emoji), so it's used instead of
// downloading: on Windows a font every Windows 10+ has, elsewhere whatever
// system font has the sample character.
struct SystemFontSubstitute {
    const char* name;
    const char* winFile;
    u32 sample;
};

static const SystemFontSubstitute kSystemFontSubstitutes[] = {
    {"NotoSansMath-Regular.otf", "seguisym.ttf", 0x1d400},    // math bold A
    {"NotoMusic-Regular.otf", "seguisym.ttf", 0x1d11e},       // G clef
    {"NotoSansSymbols-Regular.otf", "seguisym.ttf", 0x2600},  // sun
    {"NotoSansSymbols2-Regular.otf", "seguisym.ttf", 0x2800}, // braille
    {"NotoEmoji-Regular.ttf", "seguiemj.ttf", 0x1f600},       // grinning face
};

static const SystemFontSubstitute* FindSystemSubstitute(Str name) {
    for (auto& sub : kSystemFontSubstitutes) {
        if (str::Eq(name, Str(sub.name))) {
            return &sub;
        }
    }
    return nullptr;
}

static void LoadSystemSubstitute(Str name, EmbeddedFont* f) {
    const SystemFontSubstitute* sub = FindSystemSubstitute(name);
    if (!sub) {
        return;
    }
#if OS_WIN
    WCHAR winDir[MAX_PATH];
    UINT n = GetWindowsDirectoryW(winDir, dimof(winDir));
    if (n == 0 || n >= dimof(winDir)) {
        return;
    }
    TempStr path = path::JoinTemp(ToUtf8Temp(WStr(winDir)), StrL("Fonts"), Str(sub->winFile));
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        return;
    }
    logf("font '%s': using Windows' %s\n", name, Str(sub->winFile));
    f->data = (u8*)data.s;
    f->size = len(data);
#elif OS_LINUX || OS_DARWIN
    int size = 0;
    u8* data = sumatra_system_font_data_for_rune(sub->sample, &size);
    if (!data) {
        return;
    }
    logf("font '%s': using a system font with U+%04X\n", name, (int)sub->sample);
    f->data = data;
    f->size = size;
#endif
}

// mupdf's built-in fonts (src/mupdf/noto_sumatra.c). Each is loaded once.
// From the embedded archive, else a system font covering the same glyphs,
// else the font cache or a download.
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
        LoadSystemSubstitute(name, f);
    }
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

// whether the loader tried this font in this session and got nothing. TryLock:
// the loader holds the lock while it downloads, the UI must not wait for that
static bool FontFailedThisSession(Str name) {
    if (!gFontsMutex.TryLock()) {
        return false;
    }
    bool failed = false;
    for (EmbeddedFont* f = gFonts; f; f = f->next) {
        if (str::Eq(f->name, name)) {
            failed = !f->data;
            break;
        }
    }
    gFontsMutex.Unlock();
    return failed;
}

static void AppendPadded(str::Builder& b, Str s, int width) {
    b.Append(s);
    for (int i = len(s); i < width; i++) {
        b.AppendChar(' ');
    }
}

// mupdf's built-in fonts as 3 columns: name, size, where it comes from.
// For CmdDebugShowDownloadedFonts.
TempStr DownloadedFontsReportTemp() {
    constexpr int kSizeWidth = 12;
    lzma::SimpleArchive* archive = GetEmbeddedArchive();
    TempStr dir = FontCacheDirTemp();
    int n = sumatra_font_count();

    int nameWidth = len(StrL("Font"));
    for (int i = 0; i < n; i++) {
        const char* url;
        int size;
        nameWidth = std::max(nameWidth, (int)strlen(sumatra_font_at(i, &url, &size)));
    }
    nameWidth += 2;

    str::Builder b;
    AppendPadded(b, StrL("Font"), nameWidth);
    AppendPadded(b, StrL("Size"), kSizeWidth);
    b.Append(StrL("Status\n"));

    int nEmbedded = 0;
    int nDownloaded = 0;
    i64 downloadedSize = 0;
    for (int i = 0; i < n; i++) {
        const char* url;
        int size;
        Str name(sumatra_font_at(i, &url, &size));
        const char* slash = strrchr(url, '/');
        TempStr path = path::JoinTemp(dir, Str(slash ? slash + 1 : url));

        Str status = StrL("not downloaded");
        if (archive && lzma::GetIdxFromName(archive, fmt("fonts\\%s", name)) >= 0) {
            status = StrL("embedded");
            nEmbedded++;
        } else if (file::GetSize(path) > 0) {
            status = StrL("downloaded");
            nDownloaded++;
            downloadedSize += size;
        } else if (OS_WIN && FindSystemSubstitute(name)) {
            status = fmt("Windows' %s", Str(FindSystemSubstitute(name)->winFile));
        } else if (FontFailedThisSession(name)) {
            status = StrL("download failed");
        }

        AppendPadded(b, name, nameWidth);
        AppendPadded(b, str::FormatSizeShortTemp(size), kSizeWidth);
        b.Append(status);
        b.AppendChar('\n');
    }

    b.Append(fmt("\n%d fonts: %d embedded, %d downloaded (%s), %d not downloaded\n", n, nEmbedded, nDownloaded,
                 str::FormatSizeShortTemp(downloadedSize), n - nEmbedded - nDownloaded));
    b.Append(fmt("Fonts directory: %s\n", dir));
    return str::DupTemp(ToStr(b));
}

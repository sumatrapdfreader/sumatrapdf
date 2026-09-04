/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "gui/Dpi.h"
#include "base/Timer.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "SumatraPDF.h"
#include "DocController.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "LitDoc.h"
#include "StressTesting.h"
#include "EngineAll.h"

static bool gEnableEpubWithPdfEngine = true;

// Deterministic cache path for a cbx file on a slow drive. Uses
// md5(path + size) so it stays stable across opens of the same file, and
// reuses the file's own extension (.cbr/.cbz/.cb7/.cbt) so tooling that
// inspects the temp copy still sniffs the right format.
static TempStr GetCbxCachePathTemp(Str path, i64 fileSize) {
    TempStr dataDir = GetSumatraDataDirTemp();
    if (len(dataDir) == 0) {
        return {};
    }
    if (path::IsOnNetworkDrive(dataDir)) {
        // local-appdata is also remote (unusual) -- caching wouldn't help
        return {};
    }
    TempStr cacheDir = path::JoinTemp(dataDir, StrL("cbx-cache"));

    u8 digest[16]{};
    TempStr keyStr = fmt("%s|%lld", path, (long long)fileSize);
    CalcMD5Digest(keyStr, digest);
    TempStr hex = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));

    TempStr ext = path::GetExtTemp(path);
    if (len(ext) == 0) {
        ext = StrL(".cbx");
    }
    TempStr name = str::JoinTemp(hex, ext);
    return path::JoinTemp(cacheDir, name);
}

struct CbxCopyProgressState {
    u64 lastUpdate = 0;
};

static void OnCbxCopyProgress(CbxCopyProgressState* s, file::CopyProgress* p) {
    // throttle to once every 100 ms; the "done" callback (bytesCopied ==
    // bytesTotal) always fires because CopyFileExW issues a final update.
    bool isFinal = (p->bytesTotal > 0 && p->bytesCopied == p->bytesTotal);
    u64 now = GetTickCount64();
    if (!isFinal && (now - s->lastUpdate) < 100) {
        return;
    }
    s->lastUpdate = now;
    if (file::gFileCopyProgressCb.IsValid()) {
        file::gFileCopyProgressCb.Call(p);
    }
}

constexpr i64 kCbxNetworkLoadInMemoryMax = 32LL * 1024 * 1024;

// Network-drive cbx under 32 MB: one sequential read into RAM, then extract
// pages from those bytes. Avoids a local cache copy and the per-page re-open
// over the wire. On failure the caller falls through to the cache-copy path.
static EngineBase* MaybeCreateCbxFromMemory(Str path) {
    if (!path::IsOnNetworkDrive(path) || IsStressTesting()) {
        return nullptr;
    }
    i64 fileSize = file::GetSize(path);
    if (fileSize <= 0 || fileSize > kCbxNetworkLoadInMemoryMax) {
        return nullptr;
    }
    auto timeStart = TimeGet();
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        logf("MaybeCreateCbxFromMemory: ReadFile('%s') failed\n", path);
        return nullptr;
    }
    EngineBase* engine = CreateEngineCbxFromData(data);
    str::Free(data);
    if (!engine) {
        logf("MaybeCreateCbxFromMemory: CreateEngineCbxFromData('%s') failed\n", path);
        return nullptr;
    }
    engine->SetFilePath(path);
    logf("MaybeCreateCbxFromMemory: loaded '%s' (%lld bytes) in %.2f ms\n", path, (long long)fileSize,
         TimeSinceInMs(timeStart));
    return engine;
}

// If `path` is on a network drive and the local cache dir is not, copy
// it into a deterministically named file under <dataDir>/cbx-cache and
// return the cache path. Files under 32 MB are loaded in memory instead
// (see MaybeCreateCbxFromMemory). On cache hit we bump the access time so
// the stale-files sweep in DeleteStaleFilesAsync() keeps the file warm.
// Any failure (copy error, cache dir unavailable, ...) returns nullptr and
// the caller falls back to opening the original file directly.
static TempStr MaybeCopyCbxToLocalCache(Str path) {
    if (!path::IsOnNetworkDrive(path)) {
        return {};
    }
    // stress testing opens thousands of files back-to-back; copying each
    // one into the local cache would just churn the disk.
    if (IsStressTesting()) {
        return {};
    }
    i64 fileSize = file::GetSize(path);
    if (fileSize <= 0) {
        return {};
    }
    TempStr cachePath = GetCbxCachePathTemp(path, fileSize);
    if (len(cachePath) == 0) {
        return {};
    }
    if (file::Exists(cachePath)) {
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        file::SetAccessTime(cachePath, now);
        logf("MaybeCopyCbxToLocalCache: cache hit '%s'\n", cachePath);
        return cachePath;
    }
    if (!dir::CreateForFile(cachePath)) {
        logf("MaybeCopyCbxToLocalCache: dir::CreateForFile('%s') failed\n", cachePath);
        return {};
    }

    auto timeStart = TimeGet();
    CbxCopyProgressState progState{0};
    auto cb = MkFunc1<CbxCopyProgressState, file::CopyProgress*>(OnCbxCopyProgress, &progState);
    bool ok = file::Copy(cachePath, path, false, cb);
    if (!ok) {
        logf("MaybeCopyCbxToLocalCache: file::Copy('%s' -> '%s') failed\n", path, cachePath);
        file::Delete(cachePath);
        return {};
    }
    logf("MaybeCopyCbxToLocalCache: copied '%s' -> '%s' in %.2f ms\n", path, cachePath, TimeSinceInMs(timeStart));
    return cachePath;
}

static AtomicInt gOpenCacheSeq;

static TempStr GetOpenCacheDirTemp() {
    TempStr dataDir = GetSumatraDataDirTemp();
    if (len(dataDir) == 0) {
        return {};
    }
    return path::JoinTemp(dataDir, StrL("open-cache"));
}

// Copies we made of OneNote / Outlook extracts live under <dataDir>/open-cache.
bool IsOpenCachePath(Str path) {
    TempStr dir = GetOpenCacheDirTemp();
    if (len(path) == 0 || len(dir) == 0) {
        return false;
    }
    if (!str::StartsWithI(path, dir)) {
        return false;
    }
    int n = len(dir);
    return len(path) > n && path::IsSep(path.s[n]);
}

// Read the source with FILE_SHARE_READ|WRITE|DELETE so the host can still
// exclusive-open or delete it the moment we close, then write our private copy.
static bool CopyUnlockingSource(Str dst, Str src) {
    Str data = file::ReadFile(src);
    if (len(data) == 0) {
        return false;
    }
    bool ok = file::WriteFile(dst, data);
    str::Free(data);
    if (!ok) {
        file::Delete(dst);
    }
    return ok;
}

// OneNote and Outlook extract the attachment to a cache file, launch us, then
// need exclusive access to that file (or its folder) to sync the section.
// Copy it into our open-cache and load the copy so we are not holding the
// original (issue #4705).
TempStr MaybeCopyEphemeralHostFile(Str path) {
    if (!path::IsEphemeralHostFile(path)) {
        return {};
    }
    if (IsOpenCachePath(path)) {
        return {};
    }
    i64 fileSize = file::GetSize(path);
    if (fileSize <= 0) {
        return {};
    }
    TempStr dir = GetOpenCacheDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    if (!dir::CreateAll(dir)) {
        logf("MaybeCopyEphemeralHostFile: dir::CreateAll('%s') failed\n", dir);
        return {};
    }
    TempStr ext = path::GetExtTemp(path);
    int seq = AtomicIntInc(&gOpenCacheSeq);
    TempStr name = fmt("%d%s", seq, ext);
    TempStr dst = path::JoinTemp(dir, name);
    if (!CopyUnlockingSource(dst, path)) {
        logf("MaybeCopyEphemeralHostFile: copy '%s' -> '%s' failed\n", path, dst);
        return {};
    }
    logf("MaybeCopyEphemeralHostFile: '%s' -> '%s'\n", path, dst);
    return dst;
}

/* EngineCreate.cpp */
bool IsSupportedFileType(FileType kind, bool enableEngineEbooks) {
    if (kind == FileType::Unknown) {
        return false;
    }
    if (IsEngineMupdfSupportedFileType(kind)) {
        return true;
    }
    if (IsEngineDjVuSupportedFileType(kind)) {
        return true;
    }
    if (IsEngineImageSupportedFileType(kind)) {
        return true;
    }
    if (kind == FileType::Directory) {
        // TODO: more complex
        return false;
    }
    if (IsEngineCbxSupportedFileType(kind)) {
        return true;
    }
    if (IsEnginePsSupportedFileType(kind)) {
        return true;
    }
    if (kind == FileType::Lit) {
        return true;
    }

    if (!enableEngineEbooks) {
        return false;
    }

    if (kind == FileType::Epub) {
        return true;
    }
    if (kind == FileType::Fb2) {
        return true;
    }
    if (kind == FileType::Fb2z) {
        return true;
    }
    if (kind == FileType::Mobi) {
        return true;
    }
    if (kind == FileType::PalmDoc) {
        return true;
    }
    if (kind == FileType::HTML) {
        return true;
    }
    if (kind == FileType::Txt) {
        return true;
    }
    return false;
}

static EngineBase* CreateEngineForKind(FileType kind, FileType contentHintKind, Str path, PasswordUI* pwdUI,
                                       bool enableChmEngine) {
    if (kind == FileType::Unknown) {
        return nullptr;
    }
    int dpi = DpiGet();
    EngineBase* engine = nullptr;
    // markdown has no native SumatraPDF engine; always use mupdf (cmark-gfm),
    // regardless of gEnableEpubWithPdfEngine.
    if (kind == FileType::PDF || kind == FileType::Xps || kind == FileType::Markdown) {
        engine = CreateEngineMupdfFromFile(path, kind, dpi, pwdUI);
        return engine;
    }
    if (IsEngineDjVuSupportedFileType(kind)) {
        engine = CreateEngineDjvuDecFromFile(path);
        return engine;
    }
    if (IsEngineImageSupportedFileType(kind)) {
        engine = CreateEngineImageFromFile(path);
        return engine;
    }
    if (kind == FileType::Directory) {
        // Image-dir engine only; a -folder-open-* flag could expose pdfs/other formats in toc.
        if (!engine) {
            engine = CreateEngineImageDirFromFile(path);
        }
        return engine;
    }

    if (IsEngineCbxSupportedFileType(kind)) {
        // reading a cbx straight off a network drive is painfully slow
        // (lazy-load re-opens the file for every page). Files under 32 MB
        // are read once into memory; larger ones are copied to a local
        // cache. FilePath() still reports the user's original path so file
        // history / bookmarks are unchanged.
        engine = MaybeCreateCbxFromMemory(path);
        if (engine) {
            return engine;
        }
        TempStr realPath = MaybeCopyCbxToLocalCache(path);
        engine = CreateEngineCbxFromFile(path, pwdUI, contentHintKind, realPath);
        return engine;
    }
    if (IsEnginePsSupportedFileType(kind)) {
        engine = CreateEnginePsFromFile(path);
        return engine;
    }
    if (kind == FileType::Lit) {
        return CreateEngineLitFromFile(path, pwdUI);
    }
    if (enableChmEngine && (kind == FileType::Chm)) {
        engine = CreateEngineChmFromFile(path);
        return engine;
    }
    if (gEnableEpubWithPdfEngine && IsEngineMupdfSupportedFileType(kind)) {
        engine = CreateEngineMupdfFromFile(path, kind, dpi, pwdUI);
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/2212
        // if failed to open with EngineMupdf, will also try to open
        // with my engine
        if (engine) {
            return engine;
        }
    }
#if 0
    if (kind == FileType::Txt) {
        engine = CreateEngineTxtFromFile(path);
        return engine;
    }
#endif

    if (kind == FileType::Epub) {
        engine = CreateEngineEpubFromFile(path);
        return engine;
    }
    if (kind == FileType::Fb2 || kind == FileType::Fb2z) {
        engine = CreateEngineFb2FromFile(path);
        return engine;
    }
    if (kind == FileType::Mobi) {
        // AZW4 / Kindle Print Replica is a PDF inside a MOBI wrapper.
        Str pdf = ExtractPdfFromPrintReplicaFile(path);
        if (len(pdf) > 0) {
            engine = CreateEngineMupdfFromData(pdf, StrL("file.pdf"), pwdUI);
            str::Free(pdf);
            if (engine) {
                engine->SetFilePath(path);
                return engine;
            }
        }
        engine = CreateEngineMobiFromFile(path);
        return engine;
    }
    if (kind == FileType::PalmDoc) {
        engine = CreateEnginePdbFromFile(path);
        return engine;
    }
    if (kind == FileType::HTML) {
        engine = CreateEngineHtmlFromFile(path);
        return engine;
    }
    return nullptr;
}

EngineBase* CreateEngineFromFile(Str path, PasswordUI* pwdUI, bool enableChmEngine) {
    ReportIf(len(path) == 0);

    if (str::EndsWithI(path, StrL(".p7m"))) {
        Str fileData = file::ReadFile(path);
        Str extracted = ExtractP7m(fileData);
        str::Free(fileData);
        if (len(extracted) > 0) {
            FileType kind = GuessFileTypeFromData(extracted);
            if (kind == FileType::PDF) {
                EngineBase* engine = CreateEngineMupdfFromData(extracted, StrL("file.pdf"), pwdUI);
                str::Free(extracted);
                if (engine) {
                    engine->SetFilePath(path);
                    engine->disableAntiAlias = gSettings->disableAntiAlias;
                    engine->disableAutoLinks = gSettings->disableAutoLinks;
                    return engine;
                }
            } else {
                str::Free(extracted);
            }
        }
    }

    // try to open with the engine guess from file name; if that fails,
    // guess the file type from content (one disk read inside
    // GuessFileTypeFromData) and retry.
    FileType kind = GuessFileTypeFromName(path);

    // For archive-backed engines (cbx), pre-sniff the content upfront so
    // Archive::Open can skip its own 2 KiB read. For all other
    // engines the hint is unused.
    FileType contentHint = FileType::Unknown;
    if (IsEngineCbxSupportedFileType(kind)) {
        contentHint = GuessFileTypeFromFile(path);
    }

    EngineBase* engine = CreateEngineForKind(kind, contentHint, path, pwdUI, enableChmEngine);
    if (engine) {
        // gSettings can be null in early/headless code paths (e.g. the
        // -extract-text test harness runs before LoadSettings)
        if (gSettings) {
            engine->disableAntiAlias = gSettings->disableAntiAlias;
            engine->disableAutoLinks = gSettings->disableAutoLinks;
        }
        return engine;
    }

    if (contentHint == FileType::Unknown) {
        contentHint = GuessFileTypeFromFile(path);
    }
    // avoid trying the same engine type twice (e.g. FileType::Cbz vs FileType::Zip
    // both use the cbx engine, causing duplicate password prompts)
    bool sameCbx = IsEngineCbxSupportedFileType(kind) && IsEngineCbxSupportedFileType(contentHint);
    if (kind != contentHint && !sameCbx) {
        engine = CreateEngineForKind(contentHint, contentHint, path, pwdUI, enableChmEngine);
    }
    if (engine) {
        engine->disableAntiAlias = gSettings->disableAntiAlias;
        engine->disableAutoLinks = gSettings->disableAutoLinks;
    }
    return engine;
}

static EngineBase* CreateEngineForKindFromData(FileType kind, Str data, Str nameHint, PasswordUI* pwdUI) {
    if (kind == FileType::Unknown || len(data) == 0) {
        return nullptr;
    }
    if (kind == FileType::PDF || kind == FileType::Xps || kind == FileType::Markdown) {
        return CreateEngineMupdfFromData(data, nameHint, pwdUI);
    }
    if (IsEngineDjVuSupportedFileType(kind)) {
        return CreateEngineDjvuDecFromData(data);
    }
    if (IsEngineImageSupportedFileType(kind)) {
        return CreateEngineImageFromData(data);
    }
    if (IsEngineCbxSupportedFileType(kind)) {
        return CreateEngineCbxFromData(data);
    }
    if (gEnableEpubWithPdfEngine && IsEngineMupdfSupportedFileType(kind)) {
        EngineBase* engine = CreateEngineMupdfFromData(data, nameHint, pwdUI);
        if (engine) {
            return engine;
        }
    }
    if (kind == FileType::Epub) {
        return CreateEngineEpubFromData(data);
    }
    if (kind == FileType::Fb2 || kind == FileType::Fb2z) {
        return CreateEngineFb2FromData(data);
    }
    if (kind == FileType::Mobi) {
        Str pdf = ExtractPdfFromPrintReplicaData(data);
        if (len(pdf) > 0) {
            EngineBase* engine = CreateEngineMupdfFromData(pdf, StrL("file.pdf"), pwdUI);
            str::Free(pdf);
            if (engine) {
                return engine;
            }
        }
        return CreateEngineMobiFromData(data);
    }
    return nullptr;
}

static void ApplyEngineSettings(EngineBase* engine) {
    if (!engine || !gSettings) {
        return;
    }
    engine->disableAntiAlias = gSettings->disableAntiAlias;
    engine->disableAutoLinks = gSettings->disableAutoLinks;
}

EngineBase* CreateEngineFromData(Str data, Str nameHint, PasswordUI* pwdUI) {
    if (len(data) == 0) {
        return nullptr;
    }
    FileType kind = GuessFileTypeFromName(nameHint, true);
    EngineBase* engine = CreateEngineForKindFromData(kind, data, nameHint, pwdUI);
    if (!engine) {
        FileType contentKind = GuessFileTypeFromData(data);
        if (contentKind != kind) {
            engine = CreateEngineForKindFromData(contentKind, data, nameHint, pwdUI);
        }
    }
    ApplyEngineSettings(engine);
    return engine;
}

static bool IsEngineMupdf(EngineBase* engine) {
    if (!engine) {
        return false;
    }
    return engine->kind == kindEngineMupdf;
}

bool EngineSupportsAnnotations(EngineBase* engine) {
    if (AnnotationsAreDisabled()) {
        return false;
    }
    if (!IsEngineMupdf(engine)) {
        return false;
    }
    return EngineMupdfSupportsAnnotations(engine);
}

bool EngineGetAnnotations(EngineBase* engine, Vec<Annotation*>& annotsOut) {
    if (!IsEngineMupdf(engine)) {
        return false;
    }
    EngineMupdfGetAnnotations(engine, annotsOut);
    return true;
}

bool EngineHasUnsavedAnnotations(EngineBase* engine) {
    if (!IsEngineMupdf(engine)) {
        return false;
    }
    return EngineMupdfHasUnsavedAnnotations(engine);
}

bool EngineHasRedactMarks(EngineBase* engine) {
    if (!IsEngineMupdf(engine)) {
        return false;
    }
    return EngineMupdfHasRedactMarks(engine);
}

Annotation* EngineGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, Annotation* annot) {
    if (!IsEngineMupdf(engine)) {
        return nullptr;
    }
    return EngineMupdfGetAnnotationAtPos(engine, pageNo, pos, annot);
}

Annotation* EngineGetWidgetAtPos(EngineBase* engine, int pageNo, PointF pos) {
    if (!IsEngineMupdf(engine)) {
        return nullptr;
    }
    return EngineMupdfGetWidgetAtPos(engine, pageNo, pos);
}

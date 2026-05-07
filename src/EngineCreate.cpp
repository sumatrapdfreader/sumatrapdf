/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CryptoUtil.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/GuessFileType.h"
#include "utils/Dpi.h"
#include "utils/Log.h"
#include "utils/Timer.h"

#include "wingui/UIModels.h"

#include "SumatraConfig.h"
#include "Annotation.h"
#include "Settings.h"
#include "SumatraPDF.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "StressTesting.h"

static bool gEnableEpubWithPdfEngine = true;

// Deterministic cache path for a cbx file on a slow drive. Uses
// md5(path + size) so it stays stable across opens of the same file, and
// reuses the file's own extension (.cbr/.cbz/.cb7/.cbt) so tooling that
// inspects the temp copy still sniffs the right format.
static TempStr GetCbxCachePathTemp(const char* path, i64 fileSize) {
    TempStr dataDir = GetNotImportantDataDirTemp();
    if (!dataDir) {
        return nullptr;
    }
    if (path::IsOnNetworkDrive(dataDir)) {
        // local-appdata is also remote (unusual) -- caching wouldn't help
        return nullptr;
    }
    TempStr cacheDir = path::JoinTemp(dataDir, "cbx-cache");

    u8 digest[16]{};
    TempStr keyStr = str::FormatTemp("%s|%lld", path, (long long)fileSize);
    CalcMD5Digest((const u8*)keyStr, str::Leni(keyStr), digest);
    AutoFreeStr hex = str::MemToHex(digest, dimof(digest));

    TempStr ext = path::GetExtTemp(path);
    if (str::IsEmpty(ext)) {
        ext = (TempStr) ".cbx";
    }
    TempStr name = str::JoinTemp(hex, ext);
    return path::JoinTemp(cacheDir, name);
}

struct CbxCopyProgressState {
    DWORD lastUpdate;
};

static void OnCbxCopyProgress(CbxCopyProgressState* s, file::CopyProgress* p) {
    // throttle to once every 100 ms; the "done" callback (bytesCopied ==
    // bytesTotal) always fires because CopyFileExW issues a final update.
    bool isFinal = (p->bytesTotal > 0 && p->bytesCopied == p->bytesTotal);
    DWORD now = GetTickCount();
    if (!isFinal && (now - s->lastUpdate) < 100) {
        return;
    }
    s->lastUpdate = now;
    if (file::gFileCopyProgressCb.IsValid()) {
        file::gFileCopyProgressCb.Call(p);
    }
}

// If `path` is on a network drive and the local cache dir is not, copy
// it into a deterministically named file under <dataDir>/cbx-cache and
// return the cache path. On cache hit we bump the access time so the
// stale-files sweep in DeleteStaleFilesAsync() keeps the file warm. Any
// failure (copy error, cache dir unavailable, ...) returns nullptr and
// the caller falls back to opening the original file directly.
static TempStr MaybeCopyCbxToLocalCache(const char* path) {
    if (!path::IsOnNetworkDrive(path)) {
        return nullptr;
    }
    // stress testing opens thousands of files back-to-back; copying each
    // one into the local cache would just churn the disk.
    if (IsStressTesting()) {
        return nullptr;
    }
    i64 fileSize = file::GetSize(path);
    if (fileSize <= 0) {
        return nullptr;
    }
    TempStr cachePath = GetCbxCachePathTemp(path, fileSize);
    if (!cachePath) {
        return nullptr;
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
        return nullptr;
    }

    auto timeStart = TimeGet();
    CbxCopyProgressState progState{0};
    auto cb = MkFunc1<CbxCopyProgressState, file::CopyProgress*>(OnCbxCopyProgress, &progState);
    bool ok = file::Copy(cachePath, path, false, cb);
    if (!ok) {
        logf("MaybeCopyCbxToLocalCache: file::Copy('%s' -> '%s') failed\n", path, cachePath);
        file::Delete(cachePath);
        return nullptr;
    }
    logf("MaybeCopyCbxToLocalCache: copied '%s' -> '%s' in %.2f ms\n", path, cachePath, TimeSinceInMs(timeStart));
    return cachePath;
}

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks) {
    if (!kind) return false;
    if (IsEngineMupdfSupportedFileType(kind)) {
        return true;
    } else if (IsEngineDjVuSupportedFileType(kind)) {
        return true;
    } else if (IsEngineImageSupportedFileType(kind)) {
        return true;
    } else if (kind == kindDirectory) {
        // TODO: more complex
        return false;
    } else if (IsEngineCbxSupportedFileType(kind)) {
        return true;
    } else if (IsEnginePsSupportedFileType(kind)) {
        return true;
    }

    if (!enableEngineEbooks) {
        return false;
    }

    if (kind == kindFileEpub) {
        return true;
    } else if (kind == kindFileFb2) {
        return true;
    } else if (kind == kindFileFb2z) {
        return true;
    } else if (kind == kindFileMobi) {
        return true;
    } else if (kind == kindFilePalmDoc) {
        return true;
    } else if (kind == kindFileHTML) {
        return true;
    } else if (kind == kindFileTxt) {
        return true;
    }
    return false;
}

static EngineBase* CreateEngineForKind(Kind kind, Kind contentHintKind, const char* path, PasswordUI* pwdUI,
                                       bool enableChmEngine) {
    if (!kind) {
        return nullptr;
    }
    int dpi = DpiGet(nullptr);
    EngineBase* engine = nullptr;
    if (kind == kindFilePDF || kind == kindFileXps) {
        engine = CreateEngineMupdfFromFile(path, kind, dpi, pwdUI);
        return engine;
    }
    if (IsEngineDjVuSupportedFileType(kind)) {
        engine = CreateEngineDjVuFromFile(path);
        return engine;
    }
    if (IsEngineImageSupportedFileType(kind)) {
        engine = CreateEngineImageFromFile(path);
        return engine;
    }
    if (kind == kindDirectory) {
        // TODO: in 3.1.2 we open folder of images (IsEngineImageDirSupportedFile())
        // To avoid changing behavior, we open pdfs only in ramicro build
        // this should be controlled via cmd-line flag e.g. -folder-open-pdf
        // Then we could have more options, like -folder-open-images (default)
        // -folder-open-all (show all files we support in toc)
        if (!engine) {
            engine = CreateEngineImageDirFromFile(path);
        }
        return engine;
    }

    if (IsEngineCbxSupportedFileType(kind)) {
        // reading a cbx straight off a network drive is painfully slow
        // (lazy-load re-opens the file for every page and even eager-load
        // reads the whole archive over the wire). Copy it to a local
        // cache once and load from there; FilePath() still reports the
        // user's original path so file history / bookmarks are unchanged.
        TempStr realPath = MaybeCopyCbxToLocalCache(path);
        engine = CreateEngineCbxFromFile(path, pwdUI, contentHintKind, realPath);
        return engine;
    }
    if (IsEnginePsSupportedFileType(kind)) {
        engine = CreateEnginePsFromFile(path);
        return engine;
    }
    if (enableChmEngine && (kind == kindFileChm)) {
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
    if (kind == kindFileTxt) {
        engine = CreateEngineTxtFromFile(path);
        return engine;
    }
#endif

    if (kind == kindFileEpub) {
        engine = CreateEngineEpubFromFile(path);
        return engine;
    }
    if (kind == kindFileFb2 || kind == kindFileFb2z) {
        engine = CreateEngineFb2FromFile(path);
        return engine;
    }
    if (kind == kindFileMobi) {
        engine = CreateEngineMobiFromFile(path);
        return engine;
    }
    if (kind == kindFilePalmDoc) {
        engine = CreateEnginePdbFromFile(path);
        return engine;
    }
    if (kind == kindFileHTML) {
        engine = CreateEngineHtmlFromFile(path);
        return engine;
    }
    return nullptr;
}

EngineBase* CreateEngineFromFile(const char* path, PasswordUI* pwdUI, bool enableChmEngine) {
    ReportIf(!path);

    // try to open with the engine guess from file name; if that fails,
    // guess the file type from content (one disk read inside
    // GuessFileTypeFromContent) and retry.
    Kind kind = GuessFileTypeFromName(path);

    // For archive-backed engines (cbx), pre-sniff the content upfront so
    // MultiFormatArchive::Open can skip its own 2 KiB read. For all other
    // engines the hint is unused.
    Kind contentHint = nullptr;
    if (IsEngineCbxSupportedFileType(kind)) {
        contentHint = GuessFileTypeFromContent(path);
    }

    EngineBase* engine = CreateEngineForKind(kind, contentHint, path, pwdUI, enableChmEngine);
    if (engine) {
        engine->disableAntiAlias = gGlobalPrefs->disableAntiAlias;
        return engine;
    }

    if (!contentHint) {
        contentHint = GuessFileTypeFromContent(path);
    }
    // avoid trying the same engine type twice (e.g. kindFileCbz vs kindFileZip
    // both use the cbx engine, causing duplicate password prompts)
    bool sameCbx = IsEngineCbxSupportedFileType(kind) && IsEngineCbxSupportedFileType(contentHint);
    if (kind != contentHint && !sameCbx) {
        engine = CreateEngineForKind(contentHint, contentHint, path, pwdUI, enableChmEngine);
    }
    if (engine) {
        engine->disableAntiAlias = gGlobalPrefs->disableAntiAlias;
    }
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

Annotation* EngineGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, Annotation* annot) {
    if (!IsEngineMupdf(engine)) {
        return nullptr;
    }
    return EngineMupdfGetAnnotationAtPos(engine, pageNo, pos, annot);
}

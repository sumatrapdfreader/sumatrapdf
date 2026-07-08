/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Timer.h"

#include "zopflipng/zopflipng_lib.h"

#include "PngOptimizer.h"

#include "base/Log.h"

// zopfli is slow (roughly a second or more per MB of PNG) so don't try to
// optimize huge files; typical screenshots are well under this
constexpr int kMaxPngSizeToOptimize = 16 * 1024 * 1024;

// Losslessly recompress the PNG file at path with zopflipng and replace it if
// the result is smaller. The new content is written to a temp file which is
// then atomically swapped in, so anyone reading the file concurrently (e.g.
// the document we just loaded from it) sees either the old or the new
// content, never a partial write.
static void OptimizePngFile(Str path) {
    auto timeStart = TimeGet();
    Str d = file::ReadFile(path);
    int nOrig = len(d);
    if (nOrig == 0 || nOrig > kMaxPngSizeToOptimize) {
        str::Free(d);
        return;
    }
    CZopfliPNGOptions opts;
    CZopfliPNGSetDefaults(&opts);
    unsigned char* out = nullptr;
    size_t outSize = 0;
    int err = CZopfliPNGOptimize((const unsigned char*)d.s, (size_t)nOrig, &opts, 0, &out, &outSize);
    str::Free(d);
    if (err != 0 || !out || outSize == 0) {
        free(out);
        logf("OptimizePngFile: failed to optimize '%s', error: %d\n", path, err);
        return;
    }
    if (outSize >= (size_t)nOrig) {
        free(out);
        logf("OptimizePngFile: '%s' is already optimal (%d bytes)\n", path, nOrig);
        return;
    }
    TempStr tmpPath = fmt("%s.zopfli-tmp", path);
    bool ok = file::WriteFile(tmpPath, Str((char*)out, (int)outSize));
    free(out);
    if (!ok) {
        logf("OptimizePngFile: failed to write '%s'\n", tmpPath);
        return;
    }
    if (!MoveFileExW(CWStrTemp(tmpPath), CWStrTemp(path), MOVEFILE_REPLACE_EXISTING)) {
        file::Delete(tmpPath);
        logf("OptimizePngFile: failed to replace '%s'\n", path);
        return;
    }
    int savedPercent = (int)(100 - ((i64)outSize * 100 / nOrig));
    double secs = TimeSinceInMs(timeStart) / 1000.0;
    logf("OptimizePngFile: '%s' %d => %d bytes (saved %d%%) in %.1f s\n", path, nOrig, (int)outSize, savedPercent,
         secs);
}

struct OptimizePngData {
    Str path;
};

static void OptimizePngThread(OptimizePngData* d) {
    OptimizePngFile(d->path);
    str::Free(d->path);
    delete d;
}

// Optimize the PNG file at path on a background thread. Does nothing if path
// is not a .png file, so it's safe to call unconditionally after saving an
// image in a user-selected format.
void OptimizePngFileAsync(Str path) {
    if (!str::EndsWithI(path, ".png")) {
        return;
    }
    auto d = new OptimizePngData();
    d->path = str::Dup(path);
    RunAsync(MkFunc0(OptimizePngThread, d), "OptimizePngThread");
}

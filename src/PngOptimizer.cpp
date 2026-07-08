/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Timer.h"

#include "zopflipng/zopflipng_lib.h"
#include "zopflipng/lodepng/lodepng.h"

#include "PngOptimizer.h"

#include "base/Log.h"

// zopfli is slow (roughly a second or more per MB of PNG) so don't try to
// optimize huge files; typical screenshots are well under this
constexpr int kMaxPngSizeToOptimize = 16 * 1024 * 1024;

// After optimizing we insert this tEXt chunk ("Software" keyword + text, the
// standard PNG way of naming the producing program) directly after IHDR, so
// that a later OptimizePngFileAsync() on the same file recognizes it as our
// own output and skips the expensive zopfli run. The chunk is at a fixed
// offset (IHDR is always first and fixed-size), so detection is a memcmp of
// the file's first kMarkerOffset + kMarkerChunkSize bytes.
static const char kMarkerPayload[] = "Software\0SumatraPDF zopfli";
constexpr int kMarkerPayloadLen = (int)sizeof(kMarkerPayload) - 1; // sans implicit terminating NUL
constexpr int kMarkerChunkSize = 4 + 4 + kMarkerPayloadLen + 4;    // length + type + payload + crc
// 8-byte PNG signature + IHDR chunk (4 length + 4 type + 13 data + 4 crc)
constexpr int kMarkerOffset = 8 + 25;

static void BuildMarkerChunk(u8* buf) {
    u32 n = (u32)kMarkerPayloadLen;
    buf[0] = (u8)(n >> 24);
    buf[1] = (u8)(n >> 16);
    buf[2] = (u8)(n >> 8);
    buf[3] = (u8)n;
    memcpy(buf + 4, "tEXt", 4);
    memcpy(buf + 8, kMarkerPayload, kMarkerPayloadLen);
    u32 crc = lodepng_crc32(buf + 4, 4 + kMarkerPayloadLen);
    u8* p = buf + 8 + kMarkerPayloadLen;
    p[0] = (u8)(crc >> 24);
    p[1] = (u8)(crc >> 16);
    p[2] = (u8)(crc >> 8);
    p[3] = (u8)crc;
}

// true if d starts with a PNG signature followed by an IHDR chunk
static bool IsPngWithIhdr(const u8* d, int n) {
    static const u8 hdr[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    return n >= kMarkerOffset && memcmp(d, hdr, sizeof(hdr)) == 0;
}

// true if the PNG data in d was produced by us (has our marker chunk after IHDR)
static bool HasOptimizedMarker(const u8* d, int n) {
    if (n < kMarkerOffset + kMarkerChunkSize || !IsPngWithIhdr(d, n)) {
        return false;
    }
    u8 chunk[kMarkerChunkSize];
    BuildMarkerChunk(chunk);
    return memcmp(d + kMarkerOffset, chunk, kMarkerChunkSize) == 0;
}

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
    if (HasOptimizedMarker((const u8*)d.s, nOrig)) {
        str::Free(d);
        logf("OptimizePngFile: '%s' was already optimized by us, skipping\n", path);
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
    // insert the "optimized by us" marker chunk after IHDR
    bool canMark = IsPngWithIhdr(out, (int)outSize);
    ReportIf(!canMark); // zopflipng output always starts with signature + IHDR
    size_t outSizeTotal = outSize + (canMark ? kMarkerChunkSize : 0);
    if (outSizeTotal >= (size_t)nOrig) {
        free(out);
        logf("OptimizePngFile: '%s' is already optimal (%d bytes)\n", path, nOrig);
        return;
    }
    u8* withMarker = (u8*)malloc(outSizeTotal);
    if (!withMarker) {
        free(out);
        return;
    }
    if (canMark) {
        memcpy(withMarker, out, kMarkerOffset);
        BuildMarkerChunk(withMarker + kMarkerOffset);
        memcpy(withMarker + kMarkerOffset + kMarkerChunkSize, out + kMarkerOffset, outSize - kMarkerOffset);
    } else {
        memcpy(withMarker, out, outSize);
    }
    free(out);
    TempStr tmpPath = fmt("%s.zopfli-tmp", path);
    bool ok = file::WriteFile(tmpPath, Str((char*)withMarker, (int)outSizeTotal));
    free(withMarker);
    if (!ok) {
        logf("OptimizePngFile: failed to write '%s'\n", tmpPath);
        return;
    }
    if (!MoveFileExW(CWStrTemp(tmpPath), CWStrTemp(path), MOVEFILE_REPLACE_EXISTING)) {
        file::Delete(tmpPath);
        logf("OptimizePngFile: failed to replace '%s'\n", path);
        return;
    }
    i64 nOpt = (i64)outSizeTotal;
    int savedPercent = (int)(100 - (nOpt * 100 / nOrig));
    double secs = TimeSinceInMs(timeStart) / 1000.0;
    TempStr humanOrig = str::FormatSizeShortTemp(nOrig);
    TempStr humanOpt = str::FormatSizeShortTemp(nOpt);
    TempStr sepOrig = str::FormatNumWithThousandSepTemp(nOrig);
    TempStr sepOpt = str::FormatNumWithThousandSepTemp(nOpt);
    TempStr sepSaved = str::FormatNumWithThousandSepTemp(nOrig - nOpt);
    logf("optimized %s %s => %s, %s => %s, saved %s %d%% in %.1f s\n", path, humanOrig, humanOpt, sepOrig, sepOpt,
         sepSaved, savedPercent, secs);
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

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/Timer.h"

#include "zopflipng/zopflipng_lib.h"
#include "zopflipng/lodepng/lodepng.h"

#include "PngOptimizer.h"

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
constexpr int kMarkerPayloadLen = sizeofi(kMarkerPayload) - 1;  // sans implicit terminating NUL
constexpr int kMarkerChunkSize = 4 + 4 + kMarkerPayloadLen + 4; // length + type + payload + crc
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
    if (!str::EndsWithI(path, StrL(".png"))) {
        return;
    }
    auto* d = new OptimizePngData();
    d->path = str::Dup(path);
    RunAsync(MkFunc0(OptimizePngThread, d), "OptimizePngThread");
}

// Pack pixmap pixels as tightly packed RGBA8 for lodepng_encode32.
static u8* PixmapToRgbaContiguous(const Pixmap* px) {
    if (!px || !px->data || px->width <= 0 || px->height <= 0) {
        return nullptr;
    }
    int w = px->width;
    int h = px->height;
    int n = w * h * 4;
    u8* rgba = (u8*)malloc((size_t)n);
    if (!rgba) {
        return nullptr;
    }
    for (int y = 0; y < h; y++) {
        const u8* src = px->data + ((ptrdiff_t)y * px->stride);
        u8* dst = rgba + ((ptrdiff_t)y * w * 4);
        if (px->format == PixmapFormat::RGBA8) {
            memcpy(dst, src, (size_t)w * 4);
        } else if (px->format == PixmapFormat::BGRA8) {
            for (int x = 0; x < w; x++) {
                dst[0] = src[2]; // R
                dst[1] = src[1]; // G
                dst[2] = src[0]; // B
                dst[3] = src[3]; // A
                src += 4;
                dst += 4;
            }
        } else if (px->format == PixmapFormat::BGR8) {
            for (int x = 0; x < w; x++) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = 255;
                src += 3;
                dst += 4;
            }
        } else {
            free(rgba);
            return nullptr;
        }
    }
    return rgba;
}

// Losslessly recompress PNG bytes with zopfli. Returns owned Str (may be the
// original duplicated if optimize fails or does not shrink). Caller frees.
static Str OptimizePngBytesOwned(Str png) {
    int nOrig = len(png);
    if (nOrig == 0) {
        return {};
    }
    if (nOrig > kMaxPngSizeToOptimize) {
        return str::Dup(png);
    }
    CZopfliPNGOptions opts;
    CZopfliPNGSetDefaults(&opts);
    unsigned char* out = nullptr;
    size_t outSize = 0;
    int err = CZopfliPNGOptimize((const unsigned char*)png.s, (size_t)nOrig, &opts, 0, &out, &outSize);
    if (err != 0 || !out || outSize == 0 || outSize >= (size_t)nOrig) {
        free(out);
        return str::Dup(png);
    }
    Str res = str::Dup(Str((char*)out, (int)outSize));
    free(out);
    return res;
}

Str EncodeAndOptimizePngFromPixmap(const Pixmap* px) {
    if (!px) {
        return {};
    }
    u8* rgba = PixmapToRgbaContiguous(px);
    if (!rgba) {
        return {};
    }
    unsigned char* pngOut = nullptr;
    size_t pngSize = 0;
    unsigned err = lodepng_encode32(&pngOut, &pngSize, rgba, (unsigned)px->width, (unsigned)px->height);
    free(rgba);
    if (err != 0 || !pngOut || pngSize == 0) {
        free(pngOut);
        logf("EncodeAndOptimizePngFromPixmap: lodepng_encode32 failed, err=%u\n", err);
        return {};
    }
    Str rawPng((char*)pngOut, (int)pngSize);
    // lodepng allocates with malloc; transfer ownership into Optimize via Dup then free
    Str owned = str::Dup(rawPng);
    free(pngOut);
    Str optimized = OptimizePngBytesOwned(owned);
    str::Free(owned);
    if (len(optimized) > 0) {
        logf("EncodeAndOptimizePngFromPixmap: %dx%d png %d -> %d bytes\n", px->width, px->height, (int)pngSize,
             len(optimized));
    }
    return optimized;
}

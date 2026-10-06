/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ByteReaderWriter.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/Timer.h"

#include "zopflipng/zopflipng_lib.h"
#include "zopflipng/lodepng/lodepng.h"

#include "PngOptimizer.h"

// Skip huge PNGs because zopfli takes roughly a second per MB.
constexpr int kMaxPngSizeToOptimize = 16 * 1024 * 1024;

// Large pages use one filter and iteration to avoid slow default searches.
constexpr i64 kLargePngPixels = 2 * 1000 * 1000;
constexpr int kLargePngIterations = 1;
static ZopfliPNGFilterStrategy gLargePngFilter = kStrategyMinSum;

// A Software tEXt chunk immediately after IHDR marks our output so later
// optimization skips it. IHDR's fixed size makes the marker offset constant.
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
    lodepng_chunk_generate_crc(buf);
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

// IHDR width * height, 0 if not a PNG
static i64 PngPixelCount(const u8* d, int n) {
    if (!IsPngWithIhdr(d, n)) {
        return 0;
    }
    const u8* p = d + 16; // signature + IHDR length + type
    return (i64)UInt32BE(p) * UInt32BE(p + sizeof(u32));
}

static void SetZopfliOpts(CZopfliPNGOptions* opts, const u8* png, int n) {
    CZopfliPNGSetDefaults(opts);
    if (PngPixelCount(png, n) <= kLargePngPixels) {
        return;
    }
    opts->auto_filter_strategy = 0;
    opts->filter_strategies = &gLargePngFilter;
    opts->num_filter_strategies = 1;
    opts->num_iterations = kLargePngIterations;
    opts->num_iterations_large = kLargePngIterations;
}

// Replace with a smaller, losslessly compressed PNG. An atomic rename keeps
// concurrent readers from seeing a partial write.
static void OptimizePngFile(Str path) {
    auto timeStart = TimeGet();
    Str d = file::ReadFile(path);
    AutoFree dataOwner(d.s);
    int nOrig = len(d);
    if (nOrig == 0 || nOrig > kMaxPngSizeToOptimize) {
        return;
    }
    if (HasOptimizedMarker((const u8*)d.s, nOrig)) {
        logf("OptimizePngFile: '%s' was already optimized by us, skipping\n", path);
        return;
    }
    CZopfliPNGOptions opts;
    SetZopfliOpts(&opts, (const u8*)d.s, nOrig);
    unsigned char* out = nullptr;
    size_t outSize = 0;
    int err = CZopfliPNGOptimize((const unsigned char*)d.s, (size_t)nOrig, &opts, 0, &out, &outSize);
    AutoFree outOwner(out);
    if (err != 0 || !out || outSize == 0) {
        logf("OptimizePngFile: failed to optimize '%s', error: %d\n", path, err);
        return;
    }
    // insert the "optimized by us" marker chunk after IHDR
    bool canMark = IsPngWithIhdr(out, (int)outSize);
    ReportIf(!canMark); // zopflipng output always starts with signature + IHDR
    size_t outSizeTotal = outSize + (canMark ? kMarkerChunkSize : 0);
    if (outSizeTotal >= (size_t)nOrig) {
        logf("OptimizePngFile: '%s' is already optimal (%d bytes)\n", path, nOrig);
        return;
    }
    u8* withMarker = (u8*)malloc(outSizeTotal);
    AutoFree markerOwner(withMarker);
    if (!withMarker) {
        return;
    }
    if (canMark) {
        memcpy(withMarker, out, kMarkerOffset);
        BuildMarkerChunk(withMarker + kMarkerOffset);
        memcpy(withMarker + kMarkerOffset + kMarkerChunkSize, out + kMarkerOffset, outSize - kMarkerOffset);
    } else {
        memcpy(withMarker, out, outSize);
    }
    TempStr tmpPath = fmt("%s.zopfli-tmp", path);
    bool ok = file::WriteFile(tmpPath, Str((char*)withMarker, (int)outSizeTotal));
    if (!ok) {
        logf("OptimizePngFile: failed to write '%s'\n", tmpPath);
        return;
    }
    if (!file::RenameReplace(path, tmpPath)) {
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

// Optimize a saved PNG in the background; other file extensions are skipped.
void OptimizePngFileAsync(Str path) {
    StrVec paths;
    paths.Append(path);
    OptimizePngFilesAsync(paths);
}

static void OptimizePngFilesThread(StrVec* paths) {
    for (Str path : *paths) {
        OptimizePngFile(path);
    }
    delete paths;
}

// Use one background thread so exporting many pages does not spawn one per PNG.
void OptimizePngFilesAsync(const StrVec& paths) {
    auto* toOptimize = new StrVec();
    for (Str path : paths) {
        if (str::EndsWithI(path, StrL(".png"))) {
            toOptimize->Append(path);
        }
    }
    if (len(*toOptimize) == 0) {
        delete toOptimize;
        return;
    }
    RunAsync(MkFunc0(OptimizePngFilesThread, toOptimize), StrL("OptimizePngFilesThread"));
}

// Pack pixmap pixels as tightly packed RGBA8 for lodepng_encode32.
static u8* PixmapToRgbaContiguous(const Pixmap* px) {
    if (!px || !px->data || px->width <= 0 || px->height <= 0) {
        return nullptr;
    }
    if (px->format != PixmapFormat::RGBA8 && px->format != PixmapFormat::BGRA8 && px->format != PixmapFormat::BGR8) {
        return nullptr;
    }
    int w = px->width;
    int h = px->height;
    int bpp = PixmapBytesPerPixel(px->format);
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
            continue;
        }
        for (int x = 0; x < w; x++, src += bpp, dst += 4) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = bpp == 4 ? src[3] : 255;
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
    SetZopfliOpts(&opts, (const u8*)png.s, nOrig);
    unsigned char* out = nullptr;
    size_t outSize = 0;
    int err = CZopfliPNGOptimize((const unsigned char*)png.s, (size_t)nOrig, &opts, 0, &out, &outSize);
    if (err != 0 || !out || outSize == 0 || outSize >= (size_t)nOrig) {
        free(out);
        return str::Dup(png);
    }
    return Str((char*)out, (int)outSize);
}

// plain lodepng encode, no zopfli. Caller frees
Str EncodePngFromPixmap(const Pixmap* px) {
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
        logf("EncodePngFromPixmap: lodepng_encode32 failed, err=%u\n", err);
        return {};
    }
    return Str((char*)pngOut, (int)pngSize);
}

// encode and recompress with zopfli, for embedding in a PDF. Caller frees
Str EncodeAndOptimizePngFromPixmap(const Pixmap* px) {
    Str rawPng = EncodePngFromPixmap(px);
    if (len(rawPng) == 0) {
        return {};
    }
    Str optimized = OptimizePngBytesOwned(rawPng);
    if (len(optimized) > 0) {
        logf("EncodeAndOptimizePngFromPixmap: %dx%d png %d -> %d bytes\n", px->width, px->height, len(rawPng),
             len(optimized));
    }
    str::Free(rawPng);
    return optimized;
}

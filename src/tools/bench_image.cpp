/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Benchmark image decode: native library vs WIC vs GDI+.
// -jpeg: libjpeg-turbo vs WIC vs GDI+ on .jpg/.jpeg
// -webp: libwebp vs WIC vs GDI+ on .webp
// Loads each file into memory once, times full decode (to pixels), 3 runs,
// keeps the best time.

#include "base/Base.h"
#include "base/DirIter.h"
#include "base/File.h"
#include "base/ScopedWin.h"
#include "base/Timer.h"
#include "base/Win.h"

#include <setjmp.h>
#include <wincodec.h>

#include <webp/decode.h>

extern "C" {
#include "jpeglib.h"
}

void log(Str s) {
    if (!s) {
        return;
    }
    fwrite(s.s, 1, (size_t)s.len, stderr);
}

void loga(Str s) {
    log(s);
}

void _uploadDebugReport(Str, Str, bool, bool) {}

enum class BenchFormat {
    Jpeg,
    Webp,
};

static bool IsJpegPath(Str path) {
    TempStr ext = path::GetExtTemp(path);
    return str::EqI(ext, StrL(".jpg")) || str::EqI(ext, StrL(".jpeg"));
}

static bool IsWebpPath(Str path) {
    TempStr ext = path::GetExtTemp(path);
    return str::EqI(ext, StrL(".webp"));
}

static bool MatchesFormat(Str path, BenchFormat fmt) {
    return fmt == BenchFormat::Jpeg ? IsJpegPath(path) : IsWebpPath(path);
}

static void CollectFiles(Str path, BenchFormat fmt, StrVec& out) {
    path::Type t = path::GetType(path);
    if (t == path::Type::File) {
        if (MatchesFormat(path, fmt)) {
            out.Append(path);
        }
        return;
    }
    if (t != path::Type::Dir) {
        return;
    }
    DirIter di{};
    di.dir = path;
    di.includeFiles = true;
    di.includeDirs = false;
    di.recurse = true;
    for (DirIterEntry* de : di) {
        if (de && de->isFile && MatchesFormat(de->filePath, fmt)) {
            out.Append(de->filePath);
        }
    }
}

// --- libjpeg-turbo ---------------------------------------------------------

struct JpegErrorMgr {
    jpeg_error_mgr pub;
    jmp_buf setjmpBuf;
};

static void JpegErrorExit(j_common_ptr cinfo) {
    auto* err = (JpegErrorMgr*)cinfo->err;
    longjmp(err->setjmpBuf, 1);
}

static bool DecodeLibjpegTurbo(Str data, int* outW, int* outH) {
    if (!data || data.len < 2) {
        return false;
    }
    jpeg_decompress_struct cinfo{};
    JpegErrorMgr jerr{};
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = JpegErrorExit;

    if (setjmp(jerr.setjmpBuf)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (const unsigned char*)data.s, (unsigned long)data.len);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    cinfo.out_color_space = JCS_RGB;
    if (!jpeg_start_decompress(&cinfo)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    int w = (int)cinfo.output_width;
    int h = (int)cinfo.output_height;
    int rowBytes = w * (int)cinfo.output_components;
    auto* row = (JSAMPLE*)malloc((size_t)rowBytes);
    if (!row) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    JSAMPROW rowPtr[1] = {row};
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, rowPtr, 1);
    }
    free(row);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    if (outW) {
        *outW = w;
    }
    if (outH) {
        *outH = h;
    }
    return true;
}

// --- libwebp ----------------------------------------------------------------

static bool DecodeLibwebp(Str data, int* outW, int* outH) {
    if (!data) {
        return false;
    }
    int w = 0, h = 0;
    if (!WebPGetInfo((const u8*)data.s, (size_t)data.len, &w, &h) || w <= 0 || h <= 0) {
        return false;
    }
    // Full BGRA decode into a temporary buffer (discarded after timing).
    size_t nbytes = (size_t)w * (size_t)h * 4;
    auto* buf = (u8*)malloc(nbytes);
    if (!buf) {
        return false;
    }
    int stride = w * 4;
    u8* out = WebPDecodeBGRAInto((const u8*)data.s, (size_t)data.len, buf, nbytes, stride);
    free(buf);
    if (!out) {
        return false;
    }
    if (outW) {
        *outW = w;
    }
    if (outH) {
        *outH = h;
    }
    return true;
}

// --- WIC -------------------------------------------------------------------

static bool DecodeWic(Str data, int* outW, int* outH) {
    IStream* stream = CreateStreamFromData(data);
    if (!stream) {
        return false;
    }

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    BYTE* buf = nullptr;
    bool ok = false;
    UINT w = 0, h = 0;
    UINT stride = 0, bufSize = 0;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        goto done;
    }
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        goto done;
    }
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        goto done;
    }
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        goto done;
    }
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.f,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        goto done;
    }

    hr = converter->GetSize(&w, &h);
    if (FAILED(hr) || w == 0 || h == 0) {
        goto done;
    }

    stride = w * 4;
    bufSize = stride * h;
    buf = (BYTE*)malloc(bufSize);
    if (!buf) {
        goto done;
    }
    hr = converter->CopyPixels(nullptr, stride, bufSize, buf);
    if (FAILED(hr)) {
        goto done;
    }

    if (outW) {
        *outW = (int)w;
    }
    if (outH) {
        *outH = (int)h;
    }
    ok = true;

done:
    free(buf);
    if (converter) {
        converter->Release();
    }
    if (frame) {
        frame->Release();
    }
    if (decoder) {
        decoder->Release();
    }
    if (factory) {
        factory->Release();
    }
    stream->Release();
    return ok;
}

// --- GDI+ ------------------------------------------------------------------

static bool DecodeGdiplus(Str data, int* outW, int* outH) {
    IStream* stream = CreateStreamFromData(data);
    if (!stream) {
        return false;
    }
    Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromStream(stream);
    stream->Release();
    if (!bmp) {
        return false;
    }
    if (bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp;
        return false;
    }

    int w = (int)bmp->GetWidth();
    int h = (int)bmp->GetHeight();
    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData bd{};
    // PixelFormat32bppARGB is a GDI+ macro; do not qualify with Gdiplus::.
    Gdiplus::Status st = bmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd);
    if (st != Gdiplus::Ok) {
        delete bmp;
        return false;
    }
    volatile BYTE sink = 0;
    if (bd.Scan0 && h > 0) {
        sink = *((BYTE*)bd.Scan0);
        sink ^= *((BYTE*)bd.Scan0 + (size_t)(h - 1) * (size_t)bd.Stride);
        (void)sink;
    }
    bmp->UnlockBits(&bd);
    delete bmp;

    if (outW) {
        *outW = w;
    }
    if (outH) {
        *outH = h;
    }
    return true;
}

// --- timing ----------------------------------------------------------------

constexpr int kRuns = 3;

using DecodeFn = bool (*)(Str data, int* outW, int* outH);

static double BestMs(DecodeFn fn, Str data, int* outW, int* outH, bool* okOut) {
    double best = 1e300;
    bool anyOk = false;
    int w = 0, h = 0;
    for (int i = 0; i < kRuns; i++) {
        auto t0 = TimeGet();
        bool ok = fn(data, &w, &h);
        double ms = TimeSinceInMs(t0);
        if (ok) {
            anyOk = true;
            if (ms < best) {
                best = ms;
            }
        }
    }
    if (okOut) {
        *okOut = anyOk;
    }
    if (anyOk) {
        if (outW) {
            *outW = w;
        }
        if (outH) {
            *outH = h;
        }
        return best;
    }
    return -1;
}

struct Totals {
    double nativeMs = 0;
    double wicMs = 0;
    double gdiMs = 0;
    int nativeOk = 0;
    int wicOk = 0;
    int gdiOk = 0;
    int nativeWins = 0;
    int wicWins = 0;
    int gdiWins = 0;
    int ties = 0;
    int files = 0;
};

static const char* NativeShortName(BenchFormat fmt) {
    return fmt == BenchFormat::Jpeg ? "turbo" : "webp";
}

static const char* NativeLongName(BenchFormat fmt) {
    return fmt == BenchFormat::Jpeg ? "libjpeg" : "libwebp";
}

static const char* WinnerName(double native, bool nOk, double wic, bool wOk, double gdi, bool gOk,
                              const char* nativeName) {
    struct Cand {
        double ms;
        bool ok;
        const char* name;
    };
    Cand c[3] = {{native, nOk, nativeName}, {wic, wOk, "wic"}, {gdi, gOk, "gdi+"}};
    double best = 1e300;
    int nBest = 0;
    const char* name = "none";
    for (int i = 0; i < 3; i++) {
        if (!c[i].ok) {
            continue;
        }
        if (c[i].ms < best - 1e-9) {
            best = c[i].ms;
            name = c[i].name;
            nBest = 1;
        } else if (c[i].ms <= best + 1e-9) {
            nBest++;
        }
    }
    return nBest > 1 ? "tie" : name;
}

static void BenchFile(Str path, BenchFormat fmt, Totals& tot) {
    Str data = file::ReadFile(path);
    if (!data) {
        printf("READ FAIL  %.*s\n", path.len, path.s);
        return;
    }

    DecodeFn nativeFn = fmt == BenchFormat::Jpeg ? DecodeLibjpegTurbo : DecodeLibwebp;
    const char* nativeShort = NativeShortName(fmt);

    int w = 0, h = 0;
    bool nOk = false, wOk = false, gOk = false;
    double nMs = BestMs(nativeFn, data, &w, &h, &nOk);
    double wMs = BestMs(DecodeWic, data, nullptr, nullptr, &wOk);
    double gMs = BestMs(DecodeGdiplus, data, nullptr, nullptr, &gOk);
    str::Free(data);

    tot.files++;
    if (nOk) {
        tot.nativeMs += nMs;
        tot.nativeOk++;
    }
    if (wOk) {
        tot.wicMs += wMs;
        tot.wicOk++;
    }
    if (gOk) {
        tot.gdiMs += gMs;
        tot.gdiOk++;
    }

    const char* win = WinnerName(nMs, nOk, wMs, wOk, gMs, gOk, nativeShort);
    if (strcmp(win, nativeShort) == 0) {
        tot.nativeWins++;
    } else if (strcmp(win, "wic") == 0) {
        tot.wicWins++;
    } else if (strcmp(win, "gdi+") == 0) {
        tot.gdiWins++;
    } else if (strcmp(win, "tie") == 0) {
        tot.ties++;
    }

    printf("%7.2f  %7.2f  %7.2f  %5s  %4dx%-4d  %.*s\n", nOk ? nMs : -1.0, wOk ? wMs : -1.0, gOk ? gMs : -1.0, win, w,
           h, path.len, path.s);
}

static void Usage() {
    printf("usage: bench_image -jpeg|-webp <file-or-dir>\n");
    printf("  -jpeg  bench .jpg/.jpeg with libjpeg-turbo vs WIC vs GDI+\n");
    printf("  -webp  bench .webp with libwebp vs WIC vs GDI+\n");
    printf("  Recursively finds matching files under a directory.\n");
    printf("  Loads each file into memory, decodes 3x per backend, reports best ms.\n");
}

int main(int argc, char** argv) {
    BenchFormat fmt = BenchFormat::Jpeg;
    bool haveFmt = false;
    Str root{};

    for (int i = 1; i < argc; i++) {
        Str arg = argv[i];
        if (str::EqI(arg, StrL("-jpeg")) || str::EqI(arg, StrL("--jpeg"))) {
            fmt = BenchFormat::Jpeg;
            haveFmt = true;
        } else if (str::EqI(arg, StrL("-webp")) || str::EqI(arg, StrL("--webp"))) {
            fmt = BenchFormat::Webp;
            haveFmt = true;
        } else if (str::EqI(arg, StrL("-h")) || str::EqI(arg, StrL("--help")) || str::EqI(arg, StrL("/?"))) {
            Usage();
            return 0;
        } else if (arg.s && arg.s[0] == '-') {
            printf("unknown option: %s\n", arg.s);
            Usage();
            return 1;
        } else if (!root) {
            root = arg;
        } else {
            printf("extra argument: %s\n", arg.s);
            Usage();
            return 1;
        }
    }

    if (!haveFmt || !root) {
        Usage();
        return 1;
    }

    ScopedCom com;
    ScopedGdiPlus gdiplus;

    StrVec files;
    CollectFiles(root, fmt, files);
    if (len(files) == 0) {
        const char* exts = fmt == BenchFormat::Jpeg ? ".jpg/.jpeg" : ".webp";
        printf("no %s files under '%.*s'\n", exts, root.len, root.s);
        return 1;
    }

    const char* nativeShort = NativeShortName(fmt);
    const char* nativeLong = NativeLongName(fmt);
    printf("format: %s  files: %d  runs/decoder: %d (best time)\n", nativeLong, len(files), kRuns);
    printf("%7s  %7s  %7s  %5s  %9s  path\n", nativeShort, "wic", "gdi+", "win", "size");
    printf("-------  -------  -------  -----  ---------  ----\n");

    Totals tot{};
    for (Str path : files) {
        BenchFile(path, fmt, tot);
    }

    printf("\n=== totals (sum of best ms over files that succeeded) ===\n");
    printf("files:       %d\n", tot.files);
    printf("%s:%*s%.2f ms  ok=%d/%d  wins=%d\n", nativeLong, (int)(11 - strlen(nativeLong)), "", tot.nativeMs,
           tot.nativeOk, tot.files, tot.nativeWins);
    printf("wic:         %.2f ms  ok=%d/%d  wins=%d\n", tot.wicMs, tot.wicOk, tot.files, tot.wicWins);
    printf("gdi+:        %.2f ms  ok=%d/%d  wins=%d\n", tot.gdiMs, tot.gdiOk, tot.files, tot.gdiWins);
    printf("ties:        %d\n", tot.ties);

    double vals[3] = {tot.nativeMs, tot.wicMs, tot.gdiMs};
    int oks[3] = {tot.nativeOk, tot.wicOk, tot.gdiOk};
    const char* names[3] = {nativeLong, "wic", "gdi+"};
    double slowest = 0;
    for (int i = 0; i < 3; i++) {
        if (oks[i] == tot.files && vals[i] > slowest) {
            slowest = vals[i];
        }
    }
    if (slowest > 0) {
        printf("\nrelative (lower is faster; only full-ok decoders):\n");
        for (int i = 0; i < 3; i++) {
            if (oks[i] == tot.files) {
                printf("  %s: %.2fx\n", names[i], vals[i] / slowest);
            }
        }
    }
    return 0;
}

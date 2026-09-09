/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"

#ifdef _MSC_VER
#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
#endif

extern "C" {
#include "mupdf/fitz.h"
#include "../ext/mupdf/source/fitz/color-imp.h"
}

#if OS_WIN
#include "base/File.h"
#include "base/ScopedWin.h"
#include "base/TgaReader.h"
#include "base/Win.h"
#include "base/GdiPlusUtil.h"
#include "AvifReader.h"
#include "JxlReader.h"
#include "WebpReader.h"

#if COMPILER_MSVC
#pragma warning(disable : 4668)
#endif
#include <wincodec.h>
#endif

#include "ImageReader.h"

struct MupdfContext {
    fz_locks_context fz_locks_ctx{};
    Mutex mutexes[FZ_LOCK_MAX];
    fz_context* ctx = nullptr;
};

static void fz_lock_context_cs(void* user, int lock) {
    MupdfContext* ctx = (MupdfContext*)user;
    ctx->mutexes[lock].Lock();
}

static void fz_unlock_context_cs(void* user, int lock) {
    MupdfContext* ctx = (MupdfContext*)user;
    ctx->mutexes[lock].Unlock();
}

// route mupdf's warnings/errors through our log() instead of the default
// callback, which does fputs() to stderr; that first fputs makes the CRT
// allocate a stdio buffer it never frees, which shows up as a leak.
// mupdf hands us the message without the newline its default callback prints
static void fz_log_cb(void* /*user*/, const char* msg) {
    Str msgStr = Str(msg);
    if (!str::EndsWith(msgStr, StrL("\n"))) {
        msgStr = str::JoinTemp(msgStr, StrL("\n"));
    }
    log(msgStr);
}

fz_context* fz_new_context_windows(size_t maxStore) {
    auto* c = new MupdfContext();
    c->fz_locks_ctx.user = c;
    c->fz_locks_ctx.lock = fz_lock_context_cs;
    c->fz_locks_ctx.unlock = fz_unlock_context_cs;
    c->ctx = fz_new_context(nullptr, &c->fz_locks_ctx, maxStore);
    if (c->ctx) {
        fz_set_warning_callback(c->ctx, fz_log_cb, nullptr);
        fz_set_error_callback(c->ctx, fz_log_cb, nullptr);
    }
    return c->ctx;
}

void fz_drop_context_windows(fz_context* ctx) {
    auto* c = (MupdfContext*)ctx->locks.user;
    ReportIf(ctx != c->ctx);
    fz_drop_context(ctx);
    delete c;
}

static Pixmap* PixmapFromFzPixmap(fz_context* ctx, fz_pixmap* pix);

static Pixmap* PixmapFromImageData(fz_context* ctx, const u8* data, size_t n) {
    fz_buffer* buf = nullptr;
    fz_image* img = nullptr;
    fz_pixmap* pix = nullptr;

    fz_var(buf);
    fz_var(img);
    fz_var(pix);

    fz_try(ctx) {
        buf = fz_new_buffer_from_shared_data(ctx, data, n);
        img = fz_new_image_from_buffer(ctx, buf);
        pix = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, nullptr, nullptr);
    }
    fz_always(ctx) {
        fz_drop_image(ctx, img);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        fz_drop_pixmap(ctx, pix);
        return nullptr;
    }

    return pix ? PixmapFromFzPixmap(ctx, pix) : nullptr;
}

// Standalone JPEG → packed C,M,Y,K (stride = w*4). 0 = no ink (PDF polarity).
// False if the JPEG is not CMYK/YCCK.
bool DecodeJpegToCmyk(Str jpeg, int& w, int& h, int& stride, Vec<u8>& samples) {
    w = 0;
    h = 0;
    stride = 0;
    VecReset(samples);
    if (len(jpeg) < 4) {
        return false;
    }

    fz_context* ctx = fz_new_context_windows();
    if (!ctx) {
        return false;
    }

    fz_buffer* buf = nullptr;
    fz_image* img = nullptr;
    fz_pixmap* pix = nullptr;
    fz_pixmap* deviceCmyk = nullptr;
    bool ok = false;
    fz_var(buf);
    fz_var(img);
    fz_var(pix);
    fz_var(deviceCmyk);

    fz_try(ctx) {
        buf = fz_new_buffer_from_shared_data(ctx, (const u8*)jpeg.s, (size_t)len(jpeg));
        img = fz_new_image_from_buffer(ctx, buf);
        pix = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, nullptr, nullptr);
        fz_pixmap* src = pix;
        if (src && src->colorspace && fz_colorspace_is_cmyk(ctx, src->colorspace) && src->n >= 4) {
            if (src->colorspace != fz_device_cmyk(ctx) || src->alpha || src->s > 0) {
                deviceCmyk =
                    fz_convert_pixmap(ctx, src, fz_device_cmyk(ctx), nullptr, nullptr, fz_default_color_params, 1);
                src = deviceCmyk;
            }
        } else {
            src = nullptr;
        }
        if (src && src->n == 4 && src->w > 0 && src->h > 0) {
            int rowBytes = src->w * 4;
            i64 nBytes = (i64)rowBytes * src->h;
            if (nBytes > 0 && nBytes <= INT_MAX && VecReserve(samples, (int)nBytes)) {
                samples.len = (int)nBytes;
                for (int y = 0; y < src->h; y++) {
                    memcpy(samples.els + ((size_t)y * (size_t)rowBytes),
                           src->samples + ((size_t)y * (size_t)src->stride), (size_t)rowBytes);
                }
                w = src->w;
                h = src->h;
                stride = rowBytes;
                ok = true;
            }
        }
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, deviceCmyk);
        fz_drop_pixmap(ctx, pix);
        fz_drop_image(ctx, img);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        ok = false;
        VecReset(samples);
        w = 0;
        h = 0;
        stride = 0;
    }

    fz_drop_context_windows(ctx);
    return ok;
}

static Pixmap* PixmapFromFzPixmap(fz_context* ctx, fz_pixmap* pix) {
    int w = pix->w;
    int h = pix->h;
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8);
    if (!px) {
        fz_drop_pixmap(ctx, pix);
        return nullptr;
    }
    px->xres = (float)pix->xres;
    px->yres = (float)pix->yres;

    // Zero-copy: borrow the Pixmap's buffer in an fz_pixmap and convert the decoded image
    // straight into it. fz_device_bgr lays the samples out as B,G,R,A, matching BGRA8.
    fz_pixmap* dest = nullptr;
    fz_pixmap* bgr = nullptr;
    fz_var(px);
    fz_var(dest);
    fz_var(bgr);

    fz_try(ctx) {
        fz_colorspace* csdest = fz_device_bgr(ctx);
        if (pix->alpha) {
            dest = fz_new_pixmap_with_data(ctx, csdest, w, h, nullptr, 1, px->stride, px->data);
            fz_convert_pixmap_samples(ctx, pix, dest, nullptr, nullptr, fz_default_color_params, 0);
        } else {
            // mupdf's ICC pixmap transform needs the alpha channels to match; converting a
            // decoded JPEG (no alpha) straight into BGRA made lcms reject the transform
            // ("Mismatched alpha channels") and every image with an embedded ICC profile
            // fell back to the non-color-managed fast conversion, with a warning per page.
            // Convert to BGR first, then expand to BGRA with an opaque alpha
            bgr = fz_new_pixmap(ctx, csdest, w, h, nullptr, 0);
            fz_convert_pixmap_samples(ctx, pix, bgr, nullptr, nullptr, fz_default_color_params, 0);
            for (int y = 0; y < h; y++) {
                const u8* s = bgr->samples + ((size_t)y * (size_t)bgr->stride);
                u8* d = px->data + ((size_t)y * (size_t)px->stride);
                for (int x = 0; x < w; x++) {
                    d[0] = s[0];
                    d[1] = s[1];
                    d[2] = s[2];
                    d[3] = 0xff;
                    s += 3;
                    d += 4;
                }
            }
        }
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, dest);
        fz_drop_pixmap(ctx, bgr);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        FreePixmap(px);
        return nullptr;
    }

    return px;
}

bool ImageDecodedPixmapWouldBeHuge(Str d) {
    FileTypeInfo fti = GuessFileInfoFromData(d);
    i64 w = fti.imageDx;
    i64 h = fti.imageDy;
    FreeFileTypeInfo(&fti);
    if (w <= 0 || h <= 0) {
        return false;
    }
    return w * h * 4 > kMaxDecodedPixmapBytes;
}

// Decode via MuPDF (JPEG / JPEG2000 currently).
Pixmap* PixmapFromDataFz(Str d) {
    const u8* data = (const u8*)d.s;
    size_t n = (size_t)d.len;
    if (n > INT_MAX || n < 12) {
        return nullptr;
    }

    fz_context* ctx = fz_new_context_windows();
    if (!ctx) {
        return nullptr;
    }

    Pixmap* result = nullptr;
    Str icc;
    bool jpegOrJp2 = str::StartsWith(d, StrL("\xFF\xD8")) || MemEq(data, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", 12);
    // WebP with an ICCP chunk: mupdf applies the profile. Plain WebP stays on
    // the faster libwebp path in ImageReader_win / webp::PixmapFromData.
    bool webpIcc = FindWebpChunk(d, "ICCP", icc);
    if (jpegOrJp2 || webpIcc) {
        result = PixmapFromImageData(ctx, data, n);
    }

    fz_drop_context_windows(ctx);

    return result;
}

// adapted from http://cpansearch.perl.org/src/RJRAY/Image-Size-3.230/lib/Image/Size.pm
// Cheap size probe (header parse, else full decode). Returns empty Size on failure.
Size ImageSizeFromData(Str d) {
    Size result;
    FileTypeInfo fti = GuessFileInfoFromData(d);
    if (fti.hasImageSize) {
        result = Size(fti.imageDx, fti.imageDy);
    } else if (fti.imageSizes) {
        // multi-image file (animated GIF, multi-page TIFF, ...): the first image
        result = fti.imageSizes[0];
    }
    FreeFileTypeInfo(&fti);
    if (!result.IsEmpty()) {
        return result;
    }
    // try expensive way of getting the info by decoding the image
    Pixmap* px = PixmapFromData(d);
    if (px) {
        result = Size(px->width, px->height);
        FreePixmap(px);
    }
    return result;
}

#if OS_WIN

using Gdiplus::Bitmap;
using Gdiplus::BitmapData;
using Gdiplus::Ok;
using Gdiplus::Status;

// WebP / JXL / HEIC/AVIF via our dedicated decoders (not GDI+/WIC).
static Pixmap* PixmapFromExtFormatsData(Str bmpData, FileType kind) {
    if (FileType::Webp == kind) {
        Pixmap* px = webp::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
    }
    if (FileType::Jxl == kind) {
        Pixmap* px = jxl::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
    }
    if (FileType::Heic == kind || FileType::Avif == kind) {
        return PixmapFromAvifData(bmpData);
    }
    return nullptr;
}

static Bitmap* WICFrameToBitmap(IWICImagingFactory* pFactory, IWICBitmapFrameDecode* srcFrame) {
    if (!pFactory || !srcFrame) {
        return nullptr;
    }
    HRESULT hr;

#define HR(hr) \
    if (FAILED(hr)) return nullptr;
    ScopedComPtr<IWICFormatConverter> pConverter;

    int orientation = 0;
    ScopedComPtr<IWICMetadataQueryReader> pMetadataReader;
    hr = srcFrame->GetMetadataQueryReader(&pMetadataReader);
    if (SUCCEEDED(hr)) {
        PROPVARIANT variant;
        PropVariantInit(&variant);
        hr = pMetadataReader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &variant);
        if (SUCCEEDED(hr)) {
            orientation = (int)variant.uintVal;
        }
    }

    HR(pFactory->CreateFormatConverter(&pConverter));
    HR(pConverter->Initialize(srcFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.f,
                              WICBitmapPaletteTypeCustom));

    uint w, h;
    HR(pConverter->GetSize(&w, &h));
    if (w == 0 || h == 0 || w > (uint)INT_MAX || h > (uint)INT_MAX) {
        return nullptr;
    }
    if ((i64)w * (i64)h * 4 > kMaxDecodedPixmapBytes) {
        return nullptr;
    }
    double xres, yres;
    HR(pConverter->GetResolution(&xres, &yres));
    Bitmap bmp((INT)w, (INT)h, PixelFormat32bppARGB);
    Gdiplus::Rect bmpRect(0, 0, (INT)w, (INT)h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Ok) {
        return nullptr;
    }
    size_t bufBytes = (size_t)bmpData.Stride * (size_t)h;
    if (bufBytes > UINT_MAX) {
        bmp.UnlockBits(&bmpData);
        return nullptr;
    }
    HR(pConverter->CopyPixels(nullptr, bmpData.Stride, (UINT)bufBytes, (BYTE*)bmpData.Scan0));
    bmp.UnlockBits(&bmpData);
    bmp.SetResolution((float)xres, (float)yres);
#undef HR
    ApplyExifOrientation(&bmp, orientation);
    return bmp.Clone(0, 0, (INT)bmp.GetWidth(), (INT)bmp.GetHeight(), PixelFormat32bppARGB);
}

static Bitmap* WICDecodeImageFromStream(IStream* stream) {
    ScopedCom com;

#define HR(hr) \
    if (FAILED(hr)) return nullptr;
    ScopedComPtr<IWICImagingFactory> pFactory;
    if (!pFactory.Create(CLSID_WICImagingFactory)) {
        return nullptr;
    }
    ScopedComPtr<IWICBitmapDecoder> pDecoder;
    HR(pFactory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder));
    ScopedComPtr<IWICBitmapFrameDecode> srcFrame;
    HR(pDecoder->GetFrame(0, &srcFrame));
#undef HR
    return WICFrameToBitmap(pFactory, srcFrame);
}

static void MaybeFlipBitmap(Bitmap* bmp) {
    u8 buf[64] = {}; // empirically is 26

    // propSize is derived from the image's EXIF Orientation field, so a crafted
    // TIFF can make it exceed buf (e.g. many Orientation values). GetPropertyItem
    // would then write propSize bytes past the fixed stack buffer - a stack
    // overflow. The ReportIf here used to be diagnostic-only and did not stop it,
    // so reject an oversized property before calling GDI+.
    UINT propSize = bmp->GetPropertyItemSize(PropertyTagOrientation);
    if (propSize == 0 || propSize > dimof(buf)) {
        bmp->GetLastStatus(); // clear last status
        return;
    }

    auto status = bmp->GetPropertyItem(PropertyTagOrientation, propSize, (Gdiplus::PropertyItem*)buf);
    if (status != Status::Ok) {
        bmp->GetLastStatus(); // clear last status
        return;
    }
    auto* propItem = (Gdiplus::PropertyItem*)buf;
    // guard against a malformed/short property before reading the first value
    if (!propItem->value || propItem->length < sizeof(u16)) {
        return;
    }
    u16* propValPtr = (u16*)propItem->value;
    ApplyExifOrientation(bmp, propValPtr[0]);
}

static Bitmap* DecodeWithWIC(Str bmpData) {
    auto* strm = CreateStreamFromData(bmpData);
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return nullptr;
    }
    return WICDecodeImageFromStream(stream);
}

static Bitmap* DecodeWithGdiplus(Str bmpData) {
    auto* strm = CreateStreamFromData(bmpData);
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return nullptr;
    }
    Bitmap* bmp = Gdiplus::Bitmap::FromStream(stream);
    if (!bmp) {
        return nullptr;
    }
    if (bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp;
        return nullptr;
    }
    MaybeFlipBitmap(bmp);
    return bmp;
}

static Pixmap* PixmapFromWic(Str bmpData) {
    Gdiplus::Bitmap* bmp = DecodeWithWIC(bmpData);
    if (!bmp) {
        return nullptr;
    }
    Pixmap* px = PixmapFromGdiplus(bmp);
    delete bmp;
    return px;
}

// Decode an image to a single (first-frame) Pixmap via Windows paths (TGA, ext formats, GDI+/WIC).
static Pixmap* PixmapFromDataWin(Str bmpData) {
    FileType kind = GuessFileTypeFromData(bmpData);
    if (FileType::Tga == kind) {
        Pixmap* px = tga::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
    }

    // HEIC/AVIF: in Debug, prefer heicdec so we exercise our decoder; fall back
    // to WIC. In Release, try WIC first — src/tools/bench_image (Release x64) found
    // the OS HEIF codec via WIC faster than heicdec (~1.2x AVIF / ~2x HEIC when
    // the Windows codec is installed), then fall back to heicdec.
    if (FileType::Heic == kind || FileType::Avif == kind) {
#if IS_DEBUG
        Pixmap* px = PixmapFromAvifData(bmpData);
        if (px) {
            return px;
        }
        return PixmapFromWic(bmpData);
#else
        Pixmap* px = PixmapFromWic(bmpData);
        if (px) {
            return px;
        }
        return PixmapFromAvifData(bmpData);
#endif
    }

    Pixmap* px = PixmapFromExtFormatsData(bmpData, kind);
    if (px) {
        return px;
    }

    // remaining formats (png, bmp, jxr, tiff, gif, ...) decode via GDI+/WIC. tryGdiplusFirst
    // for potentially multi-image formats (WICDecodeImageFromStream is single-frame). The
    // (first) frame is copied out into a uniform Pixmap.
    bool tryGdiplusFirst = (FileType::Tiff == kind) || (FileType::Gif == kind);
    Gdiplus::Bitmap* bmp = nullptr;
    if (tryGdiplusFirst) {
        bmp = DecodeWithGdiplus(bmpData);
    }
    if (!bmp) {
        bmp = DecodeWithWIC(bmpData);
    }
    if (!bmp && !tryGdiplusFirst) {
        bmp = DecodeWithGdiplus(bmpData);
    }
    if (!bmp) {
        return nullptr;
    }
    px = PixmapFromGdiplus(bmp);
    delete bmp;
    return px;
}

constexpr UINT kMaxImageFrames = 1000;
constexpr i64 kMaxDecodedFrameBytes = 512LL * 1024 * 1024;

// All frames from a WIC decoder (ICO sizes, and a fallback if GDI+ multi-frame fails).
static Vec<Pixmap*> PixmapsFromWicFrames(Str bmpData) {
    Vec<Pixmap*> res;
    auto* strm = CreateStreamFromData(bmpData);
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return res;
    }
    ScopedCom com;
    ScopedComPtr<IWICImagingFactory> pFactory;
    if (!pFactory.Create(CLSID_WICImagingFactory)) {
        return res;
    }
    ScopedComPtr<IWICBitmapDecoder> pDecoder;
    HRESULT hr = pFactory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) {
        return res;
    }
    UINT nFrames = 0;
    hr = pDecoder->GetFrameCount(&nFrames);
    if (FAILED(hr) || nFrames == 0) {
        return res;
    }
    nFrames = std::min(nFrames, kMaxImageFrames);
    i64 decodedBytes = 0;
    for (UINT i = 0; i < nFrames; i++) {
        ScopedComPtr<IWICBitmapFrameDecode> srcFrame;
        if (FAILED(pDecoder->GetFrame(i, &srcFrame))) {
            break;
        }
        Bitmap* bmp = WICFrameToBitmap(pFactory, srcFrame);
        if (!bmp) {
            continue;
        }
        Pixmap* px = PixmapFromGdiplus(bmp);
        delete bmp;
        if (!px) {
            continue;
        }
        decodedBytes += PixmapByteSize(px);
        if (decodedBytes > kMaxDecodedFrameBytes) {
            FreePixmap(px);
            break;
        }
        VecAppend(res, px);
    }
    return res;
}

static Vec<Pixmap*> PixmapsFromMultiFrameData(Str bmpData, FileType kind) {
    Vec<Pixmap*> res;
    Gdiplus::Bitmap* bmp = DecodeWithGdiplus(bmpData);
    if (!bmp) {
        bmp = DecodeWithWIC(bmpData);
    }
    if (!bmp) {
        return res;
    }
    const GUID* dim = (FileType::Tiff == kind) ? &Gdiplus::FrameDimensionPage : &Gdiplus::FrameDimensionTime;
    UINT nFrames = std::min(bmp->GetFrameCount(dim), kMaxImageFrames);
    i64 decodedBytes = 0;
    for (UINT i = 0; i < nFrames; i++) {
        if (bmp->SelectActiveFrame(dim, i) != Gdiplus::Ok) {
            break;
        }
        Pixmap* px = PixmapFromGdiplus(bmp);
        if (px) {
            decodedBytes += PixmapByteSize(px);
            if (decodedBytes > kMaxDecodedFrameBytes) {
                FreePixmap(px);
                break;
            }
            VecAppend(res, px);
        }
    }
    delete bmp;
    return res;
}

// Prefer the fastest decoder per format (see src/tools/bench_image, Release x64):
//   JPEG/JP2 → MuPDF/libjpeg-turbo (beats WIC/GDI+)
//   WebP     → libwebp (beats WIC; GDI+ often missing)
//   HEIC/AVIF→ Debug: heicdec then WIC; Release: WIC then heicdec
// Other formats: TGA / JXL / GDI+/WIC via PixmapFromDataWin.
// Decode image bytes to a single (first-frame) Pixmap. Caller owns it (FreePixmap).
// Windows: JPEG→turbo, WebP→libwebp, JXL→jxldec; HEIC/AVIF→heicdec then WIC in
// Debug, WIC then heicdec in Release; else TGA/GDI+/WIC. POSIX: MuPDF for now.
Pixmap* PixmapFromData(Str bmpData) {
    if (ImageDecodedPixmapWouldBeHuge(bmpData)) {
        return nullptr;
    }
    Pixmap* px = PixmapFromDataFz(bmpData);
    if (px) {
        // ICC WebP comes from mupdf, which does not apply EXIF orientation
        if (GuessFileTypeFromData(bmpData) == FileType::Webp) {
            px = PixmapApplyExifOrientation(px, WebpExifOrientation(bmpData));
        }
        return px;
    }
    FileType kind = GuessFileTypeFromData(bmpData);
    if (FileType::Webp == kind) {
        px = webp::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
    }
    return PixmapFromDataWin(bmpData);
}

// Multi-page TIFF / animated GIF / ICO: Windows multi-frame path first. Everything
// else is a single Pixmap via PixmapFromData (native codec then Win).
// One Pixmap per frame (multi-page TIFF / animated GIF / ICO yield >1); caller owns each.
Vec<Pixmap*> PixmapsFromData(Str bmpData) {
    FileType kind = GuessFileTypeFromData(bmpData);
    if (FileType::Tiff == kind || FileType::Gif == kind) {
        Vec<Pixmap*> res = PixmapsFromMultiFrameData(bmpData, kind);
        if (len(res) > 0) {
            return res;
        }
    }
    if (FileType::Ico == kind) {
        Vec<Pixmap*> res = PixmapsFromWicFrames(bmpData);
        if (len(res) > 0) {
            return res;
        }
    }

    Vec<Pixmap*> res;
    Pixmap* px = PixmapFromData(bmpData);
    if (px) {
        VecAppend(res, px);
    }
    return res;
}

// Load path into a RenderedBitmap (Windows); nullptr on POSIX for now.
RenderedBitmap* LoadRenderedBitmap(Str path) {
    if (len(path) == 0) {
        return nullptr;
    }
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        return nullptr;
    }

    Gdiplus::Bitmap* bmp = NewGdiplusBitmapFromPixmap(PixmapFromData(data));
    str::Free(data);
    if (!bmp) {
        return nullptr;
    }

    HBITMAP hbmp = nullptr;
    RenderedBitmap* rendered = nullptr;
    if (bmp->GetHBITMAP((Gdiplus::ARGB)Gdiplus::Color::White, &hbmp) == Gdiplus::Ok) {
        rendered = new RenderedBitmap(hbmp, Size((int)bmp->GetWidth(), (int)bmp->GetHeight()));
    }
    delete bmp;

    return rendered;
}

#endif

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ByteReaderWriter.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"

#ifdef _MSC_VER
#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
#endif

extern "C" {
#include "mupdf/fitz.h"
#include "../../ext/mupdf/source/fitz/color-imp.h"
}

#include "base/File.h"
#include "base/TgaReader.h"
#include "AvifReader.h"
#include "JxlReader.h"
#include "WebpReader.h"

#if OS_LINUX
#include <gdk-pixbuf/gdk-pixbuf.h>
#elif OS_DARWIN
#include "base/MacTypesHide.h"
#include <ImageIO/ImageIO.h>
#include "base/MacTypesShow.h"
#endif

#if OS_WIN
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/GdiPlusUtil.h"

#if COMPILER_MSVC
#pragma warning(disable : 4668)
#endif
#include <wincodec.h>
#endif

#include "ImageReader.h"

static int GifColorIndex(u8 r, u8 g, u8 b) {
    return ((int)r & 0xe0) | (((int)g >> 3) & 0x1c) | ((int)b >> 6);
}

static void GifPaletteColor(int idx, int& r, int& g, int& b) {
    r = ((idx >> 5) & 7) * 255 / 7;
    g = ((idx >> 2) & 7) * 255 / 7;
    b = (idx & 3) * 255 / 3;
}

static int GifPixelIndex(const Pixmap* px, const u8* src, bool& transparent) {
    int bpp = PixmapBytesPerPixel(px->format);
    u8 a = px->hasAlpha && bpp == 4 ? src[3] : 255;
    transparent = a < 128;
    u8 r = px->format == PixmapFormat::RGBA8 ? src[0] : src[2];
    u8 g = src[1];
    u8 b = px->format == PixmapFormat::RGBA8 ? src[2] : src[0];
    if (px->premultiplied && a > 0) {
        r = (u8)std::min(255, ((int)r * 255 + a / 2) / a);
        g = (u8)std::min(255, ((int)g * 255 + a / 2) / a);
        b = (u8)std::min(255, ((int)b * 255 + a / 2) / a);
    }
    return GifColorIndex(r, g, b);
}

struct GifBits {
    str::Builder data;
    u32 bits = 0;
    int count = 0;

    void Write(int code, int codeSize) {
        bits |= (u32)code << count;
        count += codeSize;
        while (count >= 8) {
            data.AppendChar((char)(bits & 0xff));
            bits >>= 8;
            count -= 8;
        }
    }

    void Flush() {
        if (count > 0) {
            data.AppendChar((char)(bits & 0xff));
            bits = 0;
            count = 0;
        }
    }
};

Str EncodeGifFromPixmap(const Pixmap* px) {
    if (!px || !px->data || px->width <= 0 || px->height <= 0 || px->width > UINT16_MAX || px->height > UINT16_MAX ||
        px->format == PixmapFormat::Native) {
        return {};
    }
    i64 pixelCount = (i64)px->width * px->height;
    if (pixelCount > (INT_MAX - 1024) / 3) {
        return {};
    }

    u32 usage[256]{};
    bool hasTransparency = false;
    int bpp = PixmapBytesPerPixel(px->format);
    for (int y = 0; y < px->height; y++) {
        const u8* src = px->data + (size_t)y * px->stride;
        for (int x = 0; x < px->width; x++, src += bpp) {
            bool transparent = false;
            int idx = GifPixelIndex(px, src, transparent);
            if (transparent) {
                hasTransparency = true;
            } else {
                usage[idx]++;
            }
        }
    }

    int transparentIdx = 0;
    int replaceIdx = 1;
    if (hasTransparency) {
        for (int i = 1; i < 256; i++) {
            if (usage[i] < usage[transparentIdx]) {
                transparentIdx = i;
            }
        }
        int tr = 0, tg = 0, tb = 0;
        GifPaletteColor(transparentIdx, tr, tg, tb);
        int bestDistance = INT_MAX;
        for (int i = 0; i < 256; i++) {
            if (i == transparentIdx) {
                continue;
            }
            int r = 0, g = 0, b = 0;
            GifPaletteColor(i, r, g, b);
            int distance = (r - tr) * (r - tr) + (g - tg) * (g - tg) + (b - tb) * (b - tb);
            if (distance < bestDistance) {
                bestDistance = distance;
                replaceIdx = i;
            }
        }
    }

    constexpr int kClearCode = 256;
    constexpr int kEndCode = 257;
    GifBits compressed;
    str::BuilderReserve(compressed.data, (int)pixelCount);
    int keys[8192];
    int values[8192];
    memset(keys, 0xff, sizeof(keys));
    int codeSize = 9;
    int nextCode = 258;
    int prefix = -1;
    compressed.Write(kClearCode, codeSize);
    for (int y = 0; y < px->height; y++) {
        const u8* src = px->data + (size_t)y * px->stride;
        for (int x = 0; x < px->width; x++, src += bpp) {
            bool transparent = false;
            int idx = GifPixelIndex(px, src, transparent);
            if (transparent) {
                idx = transparentIdx;
            } else if (hasTransparency && idx == transparentIdx) {
                idx = replaceIdx;
            }
            if (prefix < 0) {
                prefix = idx;
                continue;
            }

            int key = (prefix << 8) | idx;
            int slot = (int)(((u32)key * 2654435761u) & 8191);
            while (keys[slot] >= 0 && keys[slot] != key) {
                slot = (slot + 1) & 8191;
            }
            if (keys[slot] == key) {
                prefix = values[slot];
                continue;
            }

            compressed.Write(prefix, codeSize);
            if (nextCode < 4096) {
                keys[slot] = key;
                values[slot] = nextCode++;
                if (nextCode == (1 << codeSize) && codeSize < 12) {
                    codeSize++;
                }
            } else {
                compressed.Write(kClearCode, codeSize);
                memset(keys, 0xff, sizeof(keys));
                codeSize = 9;
                nextCode = 258;
            }
            prefix = idx;
        }
    }
    compressed.Write(prefix, codeSize);
    compressed.Write(kEndCode, codeSize);
    compressed.Flush();

    ByteWriterLE w(1024 + len(compressed.data));
    w.d.Append(StrL("GIF89a"));
    w.Write16((u16)px->width);
    w.Write16((u16)px->height);
    w.Write8(0xf7); // global 256-color table, eight-bit color resolution
    w.Write8(0);
    w.Write8(0);
    for (int i = 0; i < 256; i++) {
        int r = 0, g = 0, b = 0;
        GifPaletteColor(i, r, g, b);
        w.Write8((u8)r);
        w.Write8((u8)g);
        w.Write8((u8)b);
    }
    if (hasTransparency) {
        w.Write8x2(0x21, 0xf9);
        w.Write8(4);
        w.Write8(1);
        w.Write16(0);
        w.Write8((u8)transparentIdx);
        w.Write8(0);
    }
    w.Write8(0x2c);
    w.Write16(0);
    w.Write16(0);
    w.Write16((u16)px->width);
    w.Write16((u16)px->height);
    w.Write8(0);
    w.Write8(8); // LZW minimum code size
    Str codes = ToStr(compressed.data);
    for (int off = 0; off < len(codes);) {
        int count = std::min(255, len(codes) - off);
        w.Write8((u8)count);
        w.d.Append(Str(codes.s + off, count));
        off += count;
    }
    w.Write8(0);
    w.Write8(0x3b);
    return str::Dup(w.AsByteSlice());
}

Str EncodeTiffFromPixmap(const Pixmap* px) {
    constexpr u32 kIfdOffset = 8;
    constexpr u16 kEntryCount = 14;
    constexpr u32 kIfdSize = 2 + kEntryCount * 12 + 4;
    constexpr u32 kBitsOffset = kIfdOffset + kIfdSize;
    constexpr u32 kXResOffset = kBitsOffset + 8;
    constexpr u32 kYResOffset = kXResOffset + 8;
    constexpr u32 kPixelOffset = kYResOffset + 8;
    constexpr u16 kShort = 3;
    constexpr u16 kLong = 4;
    constexpr u16 kRational = 5;
    if (!px || !px->data || px->width <= 0 || px->height <= 0 || px->format == PixmapFormat::Native) {
        return {};
    }
    i64 pixelBytes = (i64)px->width * px->height * 4;
    if (pixelBytes > INT_MAX - kPixelOffset) {
        return {};
    }

    ByteWriterLE w(kPixelOffset + (int)pixelBytes);
    w.Write8x2('I', 'I');
    w.Write16(42);
    w.Write32(kIfdOffset);
    w.Write16(kEntryCount);
    auto entry = [&w](u16 tag, u16 type, u32 count, u32 value) {
        w.Write16(tag);
        w.Write16(type);
        w.Write32(count);
        w.Write32(value);
    };
    entry(256, kLong, 1, (u32)px->width);  // ImageWidth
    entry(257, kLong, 1, (u32)px->height); // ImageLength
    entry(258, kShort, 4, kBitsOffset);    // BitsPerSample
    entry(259, kShort, 1, 1);              // Compression = none
    entry(262, kShort, 1, 2);              // PhotometricInterpretation = RGB
    entry(273, kLong, 1, kPixelOffset);    // StripOffsets
    entry(277, kShort, 1, 4);              // SamplesPerPixel
    entry(278, kLong, 1, (u32)px->height); // RowsPerStrip
    entry(279, kLong, 1, (u32)pixelBytes); // StripByteCounts
    entry(282, kRational, 1, kXResOffset); // XResolution
    entry(283, kRational, 1, kYResOffset); // YResolution
    entry(284, kShort, 1, 1);              // PlanarConfiguration = chunky
    entry(296, kShort, 1, 2);              // ResolutionUnit = inch
    entry(338, kShort, 1, 2);              // ExtraSamples = unassociated alpha
    w.Write32(0);
    for (int i = 0; i < 4; i++) {
        w.Write16(8);
    }
    w.Write32((u32)std::max(1, (int)(px->xres + 0.5f)));
    w.Write32(1);
    w.Write32((u32)std::max(1, (int)(px->yres + 0.5f)));
    w.Write32(1);

    int bpp = PixmapBytesPerPixel(px->format);
    bool isRgba = px->format == PixmapFormat::RGBA8;
    for (int y = 0; y < px->height; y++) {
        const u8* src = px->data + (size_t)y * px->stride;
        for (int x = 0; x < px->width; x++, src += bpp) {
            u8 a = px->hasAlpha && bpp == 4 ? src[3] : 255;
            u8 r = isRgba ? src[0] : src[2];
            u8 g = src[1];
            u8 b = isRgba ? src[2] : src[0];
            if (px->premultiplied && a > 0) {
                r = (u8)std::min(255, ((int)r * 255 + a / 2) / a);
                g = (u8)std::min(255, ((int)g * 255 + a / 2) / a);
                b = (u8)std::min(255, ((int)b * 255 + a / 2) / a);
            }
            w.Write8(r);
            w.Write8(g);
            w.Write8(b);
            w.Write8(a);
        }
    }
    return str::Dup(w.AsByteSlice());
}

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
    px->hasAlpha = pix->alpha != 0;
    px->premultiplied = pix->alpha != 0;

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

// Decode formats where MuPDF is preferred to the platform codec.
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
    FileType kind = GuessFileTypeFromData(d);
    bool useFz = kind == FileType::Tiff || kind == FileType::Gif;
    if (jpegOrJp2 || webpIcc || useFz) {
        result = PixmapFromImageData(ctx, data, n);
    }

    fz_drop_context_windows(ctx);

    return result;
}

#if !OS_WIN
static Pixmap* PixmapFromDataFzAny(Str d) {
    if (len(d) <= 0 || len(d) > INT_MAX) {
        return nullptr;
    }
    fz_context* ctx = fz_new_context_windows();
    if (!ctx) {
        return nullptr;
    }
    Pixmap* result = PixmapFromImageData(ctx, (const u8*)d.s, (size_t)len(d));
    fz_drop_context_windows(ctx);
    return result;
}
#endif

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

#else

// ng: the POSIX decoders. mupdf covers png / jpeg / gif / bmp / tiff / pnm;
// the formats it does not know go to the dedicated decoders, in the order
// the Windows path uses them.
Pixmap* PixmapFromData(Str bmpData) {
    if (ImageDecodedPixmapWouldBeHuge(bmpData)) {
        return nullptr;
    }
    FileType kind = GuessFileTypeFromData(bmpData);
    Str icc;
    bool useFz = kind != FileType::Webp || FindWebpChunk(bmpData, "ICCP", icc);
    Pixmap* px = useFz ? PixmapFromDataFzAny(bmpData) : nullptr;
    if (px) {
        // mupdf does not apply EXIF orientation to a WebP
        if (kind == FileType::Webp) {
            px = PixmapApplyExifOrientation(px, WebpExifOrientation(bmpData));
        }
        return px;
    }
    if (FileType::Webp == kind) {
        return webp::PixmapFromData(bmpData);
    }
    if (FileType::Jxl == kind) {
        return jxl::PixmapFromData(bmpData);
    }
    if (FileType::Heic == kind || FileType::Avif == kind) {
        return PixmapFromAvifData(bmpData);
    }
    if (tga::HasSignature(bmpData)) {
        return tga::PixmapFromData(bmpData);
    }
    return nullptr;
}

static Vec<Pixmap*> PixmapsFromTiffData(Str data) {
    Vec<Pixmap*> res;
    if (len(data) <= 0 || len(data) > INT_MAX) {
        return res;
    }
    fz_context* ctx = fz_new_context_windows();
    if (!ctx) {
        return res;
    }

    fz_pixmap* pix = nullptr;
    fz_var(pix);
    fz_try(ctx) {
        int nFrames = std::min(fz_load_tiff_subimage_count(ctx, (const u8*)data.s, (size_t)len(data)), 1000);
        i64 decodedBytes = 0;
        for (int i = 0; i < nFrames; i++) {
            pix = fz_load_tiff_subimage(ctx, (const u8*)data.s, (size_t)len(data), i);
            i64 frameBytes = (i64)pix->w * pix->h * 4;
            if (frameBytes <= 0 || decodedBytes + frameBytes > kMaxDecodedPixmapBytes) {
                break;
            }
            decodedBytes += frameBytes;
            fz_pixmap* owned = pix;
            pix = nullptr;
            Pixmap* frame = PixmapFromFzPixmap(ctx, owned);
            if (frame) {
                VecAppend(res, frame);
            }
        }
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    fz_drop_context_windows(ctx);
    return res;
}

#if OS_WASM
static int GifSubBlocksEnd(const ByteReader& r, int offset) {
    while (r.CanRead(offset, 1)) {
        int size = r.UInt8(offset++);
        if (size == 0) {
            return offset;
        }
        if (!r.CanRead(offset, size)) {
            return -1;
        }
        offset += size;
    }
    return -1;
}

static void ClearGifRect(Pixmap* canvas, Rect rect) {
    rect = rect.Intersect(Rect(0, 0, canvas->width, canvas->height));
    for (int y = rect.y; y < rect.y + rect.dy; y++) {
        memset(canvas->data + y * canvas->stride + rect.x * 4, 0, (size_t)rect.dx * 4);
    }
}

static void CompositeGifFrame(Pixmap* canvas, const Pixmap* frame, int left, int top) {
    for (int y = 0; y < frame->height; y++) {
        int dstY = top + y;
        if (dstY < 0 || dstY >= canvas->height) {
            continue;
        }
        const u8* src = frame->data + y * frame->stride;
        for (int x = 0; x < frame->width; x++, src += 4) {
            int dstX = left + x;
            if (dstX < 0 || dstX >= canvas->width) {
                continue;
            }
            u8 alpha = frame->hasAlpha ? src[3] : 255;
            if (alpha == 0) {
                continue;
            }
            u8* dst = canvas->data + dstY * canvas->stride + dstX * 4;
            if (alpha == 255) {
                memcpy(dst, src, 4);
                dst[3] = 255;
                continue;
            }
            u32 inverse = 255 - alpha;
            dst[0] = (u8)(src[0] + ((u32)dst[0] * inverse + 127) / 255);
            dst[1] = (u8)(src[1] + ((u32)dst[1] * inverse + 127) / 255);
            dst[2] = (u8)(src[2] + ((u32)dst[2] * inverse + 127) / 255);
            dst[3] = (u8)(alpha + ((u32)dst[3] * inverse + 127) / 255);
        }
    }
}

static Vec<Pixmap*> PixmapsFromGifData(Str data) {
    Vec<Pixmap*> res;
    ByteReader r(data);
    if (len(data) < 13 || (!str::StartsWith(data, StrL("GIF87a")) && !str::StartsWith(data, StrL("GIF89a")))) {
        return res;
    }
    int canvasWidth = r.UInt16LE(6);
    int canvasHeight = r.UInt16LE(8);
    int packed = r.UInt8(10);
    int globalTableBytes = (packed & 0x80) ? 3 << ((packed & 7) + 1) : 0;
    int prefixEnd = 13 + globalTableBytes;
    if (canvasWidth <= 0 || canvasHeight <= 0 || !r.CanRead(0, prefixEnd) ||
        (i64)canvasWidth * canvasHeight * 4 > kMaxDecodedPixmapBytes) {
        return res;
    }

    Pixmap* canvas = AllocPixmap(canvasWidth, canvasHeight, PixmapFormat::BGRA8, true);
    if (!canvas) {
        return res;
    }
    memset(canvas->data, 0, (size_t)canvas->stride * canvas->height);
    canvas->hasAlpha = true;
    Pixmap* restore = nullptr;
    Rect previousRect;
    int previousDisposal = 0;
    int gceStart = -1;
    int gceEnd = -1;
    int disposal = 0;
    int offset = prefixEnd;
    i64 decodedBytes = 0;
    while (r.CanRead(offset, 1) && len(res) < 1000) {
        int marker = r.UInt8(offset);
        if (marker == 0x3b) {
            break;
        }
        if (marker == 0x21) {
            if (!r.CanRead(offset, 2)) {
                break;
            }
            int end = GifSubBlocksEnd(r, offset + 2);
            if (end < 0) {
                break;
            }
            if (r.UInt8(offset + 1) == 0xf9 && r.CanRead(offset, 8) && r.UInt8(offset + 2) == 4) {
                gceStart = offset;
                gceEnd = end;
                disposal = (r.UInt8(offset + 3) >> 2) & 7;
            } else if (r.UInt8(offset + 1) == 0x01) {
                gceStart = gceEnd = -1;
                disposal = 0;
            }
            offset = end;
            continue;
        }
        if (marker != 0x2c || !r.CanRead(offset, 10)) {
            break;
        }
        int left = r.UInt16LE(offset + 1);
        int top = r.UInt16LE(offset + 3);
        int width = r.UInt16LE(offset + 5);
        int height = r.UInt16LE(offset + 7);
        int imagePacked = r.UInt8(offset + 9);
        int localTableBytes = (imagePacked & 0x80) ? 3 << ((imagePacked & 7) + 1) : 0;
        int imageData = offset + 10 + localTableBytes;
        if (width <= 0 || height <= 0 || !r.CanRead(imageData, 1)) {
            break;
        }
        int imageEnd = GifSubBlocksEnd(r, imageData + 1);
        if (imageEnd < 0) {
            break;
        }

        if (previousDisposal == 2) {
            ClearGifRect(canvas, previousRect);
        } else if (previousDisposal == 3 && restore) {
            memcpy(canvas->data, restore->data, (size_t)canvas->stride * canvas->height);
        }
        FreePixmap(restore);
        restore = disposal == 3 ? ClonePixmap(canvas) : nullptr;

        ByteWriterLE one(prefixEnd + (gceEnd - gceStart) + (imageEnd - offset) + 1);
        one.d.Append(Str(data.s, 6));
        one.Write16((u16)width);
        one.Write16((u16)height);
        one.d.Append(Str(data.s + 10, prefixEnd - 10));
        if (gceStart >= 0) {
            one.d.Append(Str(data.s + gceStart, gceEnd - gceStart));
        }
        one.Write8(0x2c);
        one.Write16(0);
        one.Write16(0);
        one.Write16((u16)width);
        one.Write16((u16)height);
        one.Write8((u8)imagePacked);
        one.d.Append(Str(data.s + offset + 10, imageEnd - (offset + 10)));
        one.Write8(0x3b);

        Pixmap* frame = PixmapFromData(one.AsByteSlice());
        if (!frame) {
            break;
        }
        CompositeGifFrame(canvas, frame, left, top);
        FreePixmap(frame);
        Pixmap* snapshot = ClonePixmap(canvas);
        decodedBytes += PixmapByteSize(snapshot);
        if (!snapshot || decodedBytes > kMaxDecodedPixmapBytes) {
            FreePixmap(snapshot);
            break;
        }
        VecAppend(res, snapshot);
        previousRect = Rect(left, top, width, height);
        previousDisposal = disposal;
        gceStart = gceEnd = -1;
        disposal = 0;
        offset = imageEnd;
    }
    FreePixmap(restore);
    FreePixmap(canvas);
    return res;
}
#endif

static Vec<Pixmap*> PixmapsFromIcoData(Str data) {
    Vec<Pixmap*> res;
    ByteReader r(data);
    int nImages = std::min((int)r.UInt16LE(4), 256);
    i64 decodedBytes = 0;
    for (int i = 0; i < nImages; i++) {
        int entryOffset = 6 + (i * 16);
        if (!r.CanRead(entryOffset, 16)) {
            break;
        }
        int imageBytes = (int)r.UInt32LE(entryOffset + 8);
        int imageOffset = (int)r.UInt32LE(entryOffset + 12);
        if (imageBytes <= 0 || !r.CanRead(imageOffset, imageBytes)) {
            continue;
        }

        Str imageData((char*)r.d + imageOffset, imageBytes);
        Pixmap* frame = nullptr;
        if (imageBytes >= 8 && MemEq(imageData.s, "\x89PNG\r\n\x1a\n", 8)) {
            frame = PixmapFromData(imageData);
        } else if (imageBytes >= 40) {
            ByteReader image(imageData);
            int headerSize = (int)image.UInt32LE(0);
            int width = (int)image.UInt32LE(4);
            int height = (int)image.UInt32LE(8);
            int bpp = image.UInt16LE(14);
            int compression = (int)image.UInt32LE(16);
            int paletteEntries = (int)image.UInt32LE(32);
            if (paletteEntries == 0 && bpp > 0 && bpp <= 8) {
                paletteEntries = 1 << bpp;
            }
            int bitfieldBytes = headerSize == 40 && compression == 3 ? 12 : 0;
            int dibPixelOffset = headerSize + bitfieldBytes + paletteEntries * 4;
            int pixelOffset = 14 + dibPixelOffset;
            if (headerSize >= 40 && headerSize <= imageBytes && width > 0 && height > 1 && bpp > 0 &&
                pixelOffset <= 14 + imageBytes) {
                ByteWriterLE bmp(14 + imageBytes);
                bmp.Write8x2('B', 'M');
                bmp.Write32(14 + imageBytes);
                bmp.Write32(0);
                bmp.Write32(pixelOffset);
                bmp.d.Append(Str(imageData.s, 8));
                bmp.Write32((u32)(height / 2));
                bmp.d.Append(Str(imageData.s + 12, imageBytes - 12));
                frame = PixmapFromData(bmp.AsByteSlice());
                int iconHeight = height / 2;
                i64 xorStride = ((i64)width * bpp + 31) / 32 * 4;
                i64 maskStride = ((i64)width + 31) / 32 * 4;
                i64 maskOffset = dibPixelOffset + xorStride * iconHeight;
                bool hasMask = maskOffset >= 0 && maskStride > 0 && maskOffset + maskStride * iconHeight <= imageBytes;
                bool usePixelAlpha = false;
                if (frame && !frame->hasAlpha && bpp == 32 && compression == 0 && dibPixelOffset >= 0 &&
                    dibPixelOffset + xorStride * iconHeight <= imageBytes) {
                    for (int y = 0; y < iconHeight && !usePixelAlpha; y++) {
                        const u8* src = (const u8*)imageData.s + dibPixelOffset + y * xorStride;
                        for (int x = 0; x < width; x++) {
                            if (src[x * 4 + 3] != 0) {
                                usePixelAlpha = true;
                                break;
                            }
                        }
                    }
                }
                bool hasTransparency = false;
                if (frame && frame->format == PixmapFormat::BGRA8 && frame->width == width &&
                    frame->height == iconHeight && (hasMask || usePixelAlpha)) {
                    for (int y = 0; y < iconHeight; y++) {
                        int srcY = iconHeight - y - 1;
                        const u8* alphaRow = (const u8*)imageData.s + dibPixelOffset + srcY * xorStride;
                        const u8* maskRow = hasMask ? (const u8*)imageData.s + maskOffset + srcY * maskStride : nullptr;
                        u8* dst = frame->data + y * frame->stride;
                        for (int x = 0; x < width; x++, dst += 4) {
                            bool masked = hasMask && (maskRow[x / 8] & (0x80 >> (x % 8)));
                            u8 alpha = usePixelAlpha ? alphaRow[x * 4 + 3] : 255;
                            if (masked) {
                                alpha = 0;
                            }
                            if (alpha == 255) {
                                continue;
                            }
                            dst[0] = (u8)(((u32)dst[0] * alpha + 127) / 255);
                            dst[1] = (u8)(((u32)dst[1] * alpha + 127) / 255);
                            dst[2] = (u8)(((u32)dst[2] * alpha + 127) / 255);
                            dst[3] = alpha;
                            hasTransparency = true;
                        }
                    }
                }
                if (hasTransparency) {
                    frame->hasAlpha = true;
                    frame->premultiplied = true;
                }
            }
        }
        if (!frame) {
            continue;
        }
        decodedBytes += PixmapByteSize(frame);
        if (decodedBytes > kMaxDecodedPixmapBytes) {
            FreePixmap(frame);
            break;
        }
        VecAppend(res, frame);
    }
    return res;
}

#if OS_LINUX
static Pixmap* PixmapFromGdkPixbuf(GdkPixbuf* src) {
    if (!src) {
        return nullptr;
    }
    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);
    int channels = gdk_pixbuf_get_n_channels(src);
    bool hasAlpha = gdk_pixbuf_get_has_alpha(src);
    Pixmap* dst = AllocPixmap(w, h, PixmapFormat::BGRA8, true);
    if (!dst) {
        return nullptr;
    }
    dst->hasAlpha = hasAlpha;
    const u8* srcData = gdk_pixbuf_get_pixels(src);
    int srcStride = gdk_pixbuf_get_rowstride(src);
    for (int y = 0; y < h; y++) {
        const u8* s = srcData + ((size_t)y * srcStride);
        u8* d = dst->data + ((size_t)y * dst->stride);
        for (int x = 0; x < w; x++) {
            u32 a = hasAlpha && channels > 3 ? s[3] : 255;
            d[0] = (u8)(((u32)s[2] * a + 127) / 255);
            d[1] = (u8)(((u32)s[1] * a + 127) / 255);
            d[2] = (u8)(((u32)s[0] * a + 127) / 255);
            d[3] = (u8)a;
            s += channels;
            d += 4;
        }
    }
    return dst;
}

static Vec<Pixmap*> PixmapsFromGifData(Str data) {
    Vec<Pixmap*> res;
    GError* error = nullptr;
    GdkPixbufLoader* loader = gdk_pixbuf_loader_new_with_type("gif", &error);
    if (!loader || !gdk_pixbuf_loader_write(loader, (const guchar*)data.s, (gsize)len(data), &error) ||
        !gdk_pixbuf_loader_close(loader, &error)) {
        if (error) {
            g_error_free(error);
        }
        if (loader) {
            g_object_unref(loader);
        }
        return res;
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GdkPixbufAnimation* animation = gdk_pixbuf_loader_get_animation(loader);
    GTimeVal time{};
    GdkPixbufAnimationIter* iter = animation ? gdk_pixbuf_animation_get_iter(animation, &time) : nullptr;
    FileTypeInfo info = GuessFileInfoFromData(data);
    int nFrames = std::min(std::max(info.nImages, 1), 1000);
    FreeFileTypeInfo(&info);
    i64 decodedBytes = 0;
    for (int i = 0; iter && i < nFrames; i++) {
        GdkPixbuf* src = gdk_pixbuf_animation_iter_get_pixbuf(iter);
        Pixmap* frame = PixmapFromGdkPixbuf(src);
        if (!frame) {
            break;
        }
        decodedBytes += PixmapByteSize(frame);
        if (decodedBytes > kMaxDecodedPixmapBytes) {
            FreePixmap(frame);
            break;
        }
        VecAppend(res, frame);
        if (i + 1 == nFrames) {
            break;
        }

        int delay = gdk_pixbuf_animation_iter_get_delay_time(iter);
        if (delay < 0) {
            break;
        }
        g_time_val_add(&time, (std::max(delay, 1) + 1) * 1000);
        if (!gdk_pixbuf_animation_iter_advance(iter, &time)) {
            break;
        }
    }
    if (iter) {
        g_object_unref(iter);
    }
    G_GNUC_END_IGNORE_DEPRECATIONS
    g_object_unref(loader);
    return res;
}

#endif

#if OS_DARWIN
static Pixmap* PixmapFromCgImage(CGImageRef image) {
    if (!image) {
        return nullptr;
    }
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    if (width == 0 || height == 0 || width > INT_MAX || height > INT_MAX ||
        (i64)width * (i64)height * 4 > kMaxDecodedPixmapBytes) {
        return nullptr;
    }
    Pixmap* pixmap = AllocPixmap((int)width, (int)height, PixmapFormat::BGRA8, true);
    if (!pixmap) {
        return nullptr;
    }
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo info = (CGBitmapInfo)((u32)kCGBitmapByteOrder32Little | (u32)kCGImageAlphaPremultipliedFirst);
    CGContextRef context = CGBitmapContextCreate(pixmap->data, width, height, 8, pixmap->stride, colorSpace, info);
    CGColorSpaceRelease(colorSpace);
    if (!context) {
        FreePixmap(pixmap);
        return nullptr;
    }
    CGContextTranslateCTM(context, 0, height);
    CGContextScaleCTM(context, 1, -1);
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    CGContextFlush(context);
    CGContextRelease(context);
    CGImageAlphaInfo alpha = CGImageGetAlphaInfo(image);
    pixmap->hasAlpha =
        alpha != kCGImageAlphaNone && alpha != kCGImageAlphaNoneSkipFirst && alpha != kCGImageAlphaNoneSkipLast;
    return pixmap;
}

static Vec<Pixmap*> PixmapsFromImageIo(Str data) {
    Vec<Pixmap*> result;
    CFDataRef bytes = CFDataCreate(kCFAllocatorDefault, (const UInt8*)data.s, len(data));
    CGImageSourceRef source = bytes ? CGImageSourceCreateWithData(bytes, nullptr) : nullptr;
    if (bytes) {
        CFRelease(bytes);
    }
    if (!source) {
        return result;
    }
    size_t count = std::min(CGImageSourceGetCount(source), (size_t)1000);
    i64 decodedBytes = 0;
    for (size_t i = 0; i < count; i++) {
        CGImageRef image = CGImageSourceCreateImageAtIndex(source, i, nullptr);
        Pixmap* frame = PixmapFromCgImage(image);
        if (image) {
            CGImageRelease(image);
        }
        if (!frame) {
            continue;
        }
        decodedBytes += PixmapByteSize(frame);
        if (decodedBytes > kMaxDecodedPixmapBytes) {
            FreePixmap(frame);
            break;
        }
        VecAppend(result, frame);
    }
    CFRelease(source);
    return result;
}
#endif

Vec<Pixmap*> PixmapsFromData(Str bmpData) {
    FileType kind = GuessFileTypeFromData(bmpData);
    if (kind == FileType::Tiff) {
        Vec<Pixmap*> frames = PixmapsFromTiffData(bmpData);
        if (len(frames) > 0) {
            return frames;
        }
    }
    if (kind == FileType::Ico) {
        Vec<Pixmap*> frames = PixmapsFromIcoData(bmpData);
        if (len(frames) > 0) {
            return frames;
        }
    }
#if OS_LINUX
    if (kind == FileType::Gif) {
        Vec<Pixmap*> frames = PixmapsFromGifData(bmpData);
        if (len(frames) > 0) {
            return frames;
        }
    }
#elif OS_DARWIN
    if (kind == FileType::Gif) {
        Vec<Pixmap*> frames = PixmapsFromImageIo(bmpData);
        if (len(frames) > 0) {
            return frames;
        }
    }
#elif OS_WASM
    if (kind == FileType::Gif) {
        Vec<Pixmap*> frames = PixmapsFromGifData(bmpData);
        if (len(frames) > 0) {
            return frames;
        }
    }
#endif
    Vec<Pixmap*> res;
    Pixmap* px = PixmapFromData(bmpData);
    if (px) {
        VecAppend(res, px);
    }
    return res;
}

RenderedBitmap* LoadRenderedBitmap(Str) {
    return nullptr;
}

#endif

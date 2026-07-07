/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/ByteReader.h"
#include "base/TgaReader.h"
#include "base/Win.h"
#include "GdiPlusExtFormats.h"
#include "base/GdiPlus.h"

#if COMPILER_MSVC
#pragma warning(disable : 4668)
#endif
#include <wincodec.h>

using Gdiplus::Bitmap;
using Gdiplus::BitmapData;
using Gdiplus::CharacterRange;
using Gdiplus::Font;
using Gdiplus::Graphics;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::Region;
using Gdiplus::Status;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsMeasureTrailingSpaces;

Gdiplus::RectF RectToRectF(const Gdiplus::Rect r) {
    return Gdiplus::RectF((float)r.X, (float)r.Y, (float)r.Width, (float)r.Height);
}

// note: gdi+ seems to under-report the width, the longer the text, the
// bigger the difference. I'm trying to correct for that with those magic values
#define PER_CHAR_DX_ADJUST .2f
#define PER_STR_DX_ADJUST 1.f

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
RectF MeasureTextAccurate(Graphics* g, Font* f, WStr s) {
    int n = s.len;
    if (0 == n) {
        return RectF(0, 0, 0, 0); // TODO: should set height to font's height
    }
    // note: frankly, I don't see a difference between those StringFormat variations
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    // StringFormat sf(StringFormat::GenericDefault());
    // StringFormat sf;
    Gdiplus::RectF layoutRect;
    CharacterRange cr(0, n);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region r;
    Status status = g->MeasureCharacterRanges(s.s, n, f, layoutRect, &sf, 1, &r);
    if (status != Ok) {
        // TODO: remove whem we figure out why we crash
        WStr logW = s ? s : WStr(L"<null>");
        TempStr s2 = ToUtf8Temp(logW);
        Str logStr = s2.len > 256 ? Str(s2.s, 256) : s2;
        logf("MeasureTextAccurate: status: %d, font: %p, len: %d, s: '%s'\n", (int)status, f, n, logStr);
        // ReportIf(status != Ok);
    }
    Gdiplus::RectF bbox;
    r.GetBounds(&bbox, g);
    if (bbox.Width != 0) {
        bbox.Width += PER_STR_DX_ADJUST + (PER_CHAR_DX_ADJUST * (float)n);
    }
    return RectF{bbox};
}

// this usually reports size that is too large
RectF MeasureTextStandard(Graphics* g, Font* f, WStr s) {
    Gdiplus::RectF bbox;
    Gdiplus::PointF pz(0, 0);
    g->MeasureString(s.s, s.len, f, pz, &bbox);
    return RectF{bbox};
}

RectF MeasureTextQuick(Graphics* g, Font* f, WStr s) {
    int n = s.len;
    ReportIf(0 >= n);

    static Vec<Font*> fontCache;
    static Vec<bool> fixCache;

    Gdiplus::RectF bbox;
    g->MeasureString(s.s, n, f, Gdiplus::PointF(0, 0), &bbox);
    int idx = fontCache.Find(f);
    if (-1 == idx) {
        LOGFONTW lfw;
        Status ok = f->GetLogFontW(g, &lfw);
        bool isItalicOrMonospace = Ok != ok || lfw.lfItalic || wstr::Eq(lfw.lfFaceName, L"Courier New") ||
                                   wstr::FindFrom(lfw.lfFaceName, L"Consol") ||
                                   wstr::EndsWith(lfw.lfFaceName, L"Mono") ||
                                   wstr::EndsWith(lfw.lfFaceName, L"Typewriter");
        fontCache.Append(f);
        fixCache.Append(isItalicOrMonospace);
        idx = fontCache.len - 1;
    }
    // most documents look good enough with these adjustments
    if (!fixCache[idx]) {
        float correct = 0;
        for (int i = 0; i < n; i++) {
            switch (s.s[i]) {
                case 'i':
                case 'l':
                    correct += 0.2f;
                    break;
                case 't':
                case 'f':
                case 'I':
                    correct += 0.1f;
                    break;
                case '.':
                case ',':
                case '!':
                    correct += 0.1f;
                    break;
            }
        }
        bbox.Width *= (1.0f - correct / n) * 0.99f;
    }
    bbox.Height *= 0.95f;
    return RectF{bbox};
}

RectF MeasureText(Graphics* g, Font* f, WStr s, TextMeasureAlgorithm algo) {
    // TODO: ideally we should not be here with len == 0. This
    // might indicate a problem with fromatter code. See internals-en.epub
    // for a repro
    ReportIf((s.len == 0) || (s.len > INT_MAX));
    if (algo) {
        return algo(g, f, s);
    }
    return MeasureTextAccurate(g, f, s);
}

// returns number of characters of string s that fits in a given width dx
// note: could be speed up a bit because in our use case we already know
// the width of the whole string so we could supply it to the function, but
// this shouldn't happen often, so that's fine. It's also possible that
// a smarter approach is possible, but this usually only does 3 MeasureText
// calls, so it's not that bad
int StringLenForWidth(Graphics* g, Font* f, WStr s, float dx, TextMeasureAlgorithm algo) {
    int sLen = s.len;
    auto r = MeasureText(g, f, s, algo);
    if (r.dx <= dx) {
        return sLen;
    }
    // make the best guess of the length that fits
    int n = (int)((dx / r.dx) * (float)sLen);
    ReportIf(n > sLen);
    if (n == 0) {
        // nothing fits in the remaining space; caller flushes the line and
        // re-lays the run at full width. Don't Measure an empty string.
        return 0;
    }
    r = MeasureText(g, f, WStr(s.s, n), algo);
    // find the length len of s that fits within dx iff width of len+1 exceeds dx
    int dir = 1; // increasing length
    if (r.dx > dx) {
        dir = -1; // decreasing length
    }
    for (;;) {
        n += dir;
        r = MeasureText(g, f, WStr(s.s, n), algo);
        if (1 == dir) {
            // if advancing length, we know that previous string did fit, so if
            // the new one doesn't fit, the previous length was the right one
            if (r.dx > dx) {
                return n - 1;
            }
        } else {
            // if decreasing length, we know that previous string didn't fit, so if
            // the one one fits, it's of the correct length
            if (r.dx < dx) {
                return n;
            }
        }
    }
}

// TODO: not quite sure why spaceDx1 != spaceDx2, using spaceDx2 because
// is smaller and looks as better spacing to me
float GetSpaceDx(Graphics* g, Font* f, TextMeasureAlgorithm algo) {
    RectF bbox;
#if 0
    bbox = MeasureText(g, f, L" ", 1, algo);
    float spaceDx1 = bbox.dx;
    return spaceDx1;
#else
    // this method seems to return (much) smaller size that measuring
    // the space itself
    bbox = MeasureText(g, f, WStr(L"wa", 2), algo);
    float l1 = bbox.dx;
    bbox = MeasureText(g, f, WStr(L"w a", 3), algo);
    float l2 = bbox.dx;
    float spaceDx2 = l2 - l1;
    return spaceDx2;
#endif
}

void GetBaseTransform(Matrix& m, Gdiplus::RectF pageRect, float zoom, int rotation) {
    rotation = rotation % 360;
    if (rotation < 0) {
        rotation = rotation + 360;
    }
    if (90 == rotation) {
        m.Translate(0, -pageRect.Height, MatrixOrderAppend);
    } else if (180 == rotation) {
        m.Translate(-pageRect.Width, -pageRect.Height, MatrixOrderAppend);
    } else if (270 == rotation) {
        m.Translate(-pageRect.Width, 0, MatrixOrderAppend);
    } else if (0 == rotation) {
        m.Translate(0, 0, MatrixOrderAppend);
    } else {
        ReportIf(true);
    }

    m.Scale(zoom, zoom, MatrixOrderAppend);
    m.Rotate((float)rotation, MatrixOrderAppend);
}

static Gdiplus::RotateFlipType rfts[] = {
    Gdiplus::RotateNoneFlipX,  Gdiplus::Rotate180FlipNone, Gdiplus::Rotate180FlipX,    Gdiplus::Rotate90FlipX,
    Gdiplus::Rotate90FlipNone, Gdiplus::Rotate270FlipX,    Gdiplus::Rotate270FlipNone,
};

static Bitmap* WICDecodeImageFromStream(IStream* stream) {
    ScopedCom com;
    HRESULT hr;
    int iRot = -1;

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
    ScopedComPtr<IWICFormatConverter> pConverter;

    ScopedComPtr<IWICMetadataQueryReader> pMetadataReader;

    hr = srcFrame->GetMetadataQueryReader(&pMetadataReader);
    if (SUCCEEDED(hr)) {
        PROPVARIANT variant;
        PropVariantInit(&variant);
        // hr = pMetadataReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=274}", &variant);
        hr = pMetadataReader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &variant);
        if (SUCCEEDED(hr)) {
            iRot = (int)variant.uintVal - 2;
        }
    }

    HR(pFactory->CreateFormatConverter(&pConverter));
    HR(pConverter->Initialize(srcFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.f,
                              WICBitmapPaletteTypeCustom));

    uint w, h;
    HR(pConverter->GetSize(&w, &h));
    double xres, yres;
    HR(pConverter->GetResolution(&xres, &yres));
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    Gdiplus::Rect bmpRect(0, 0, w, h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Ok) {
        return nullptr;
    }
    HR(pConverter->CopyPixels(nullptr, bmpData.Stride, bmpData.Stride * h, (BYTE*)bmpData.Scan0));
    bmp.UnlockBits(&bmpData);
    bmp.SetResolution((float)xres, (float)yres);
#undef HR
    // TODO: maybe use IWICBitmapFlipRotator
    if (iRot >= 0 && iRot < dimof(rfts)) {
        bmp.RotateFlip(rfts[iRot]);
    }
    return bmp.Clone(0, 0, bmp.GetWidth(), bmp.GetHeight(), PixelFormat32bppARGB);
}

void ApplyExifOrientation(Bitmap* bmp, int exifOrientation) {
    if (!bmp || exifOrientation < 2 || exifOrientation > 8) {
        return;
    }
    int iRot = exifOrientation - 2;
    if (iRot < (int)dimof(rfts)) {
        bmp->RotateFlip(rfts[iRot]);
    }
}

static void MaybeFlipBitmap(Bitmap* bmp) {
    u8 buf[64] = {}; // empirically is 26

    UINT propSize = bmp->GetPropertyItemSize(PropertyTagOrientation);
    if (propSize == 0) {
        bmp->GetLastStatus(); // clear last status
        return;
    }
    ReportIf(propSize > dimof(buf));

    auto status = bmp->GetPropertyItem(PropertyTagOrientation, propSize, (Gdiplus::PropertyItem*)buf);
    if (status != Status::Ok) {
        bmp->GetLastStatus(); // clear last status
        return;
    }
    auto propItem = (Gdiplus::PropertyItem*)buf;
    u16* propValPtr = (u16*)propItem->value;
    ApplyExifOrientation(bmp, propValPtr[0]);
}

static Bitmap* DecodeWithWIC(Str bmpData) {
    auto strm = CreateStreamFromData(bmpData);
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return nullptr;
    }
    auto bmp = WICDecodeImageFromStream(stream);
    return bmp;
}

static Bitmap* DecodeWithGdiplus(Str bmpData) {
    auto strm = CreateStreamFromData(bmpData);
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

static Gdiplus::PixelFormat PixmapToGdiplusPixelFormat(const Pixmap* px) {
    if (px->format == PixmapFormat::BGR8) {
        return PixelFormat24bppRGB;
    }
    // BGRA8 (RGBA8 isn't produced by our decoders and has no zero-copy GDI+ format)
    return px->premultiplied ? PixelFormat32bppPARGB : PixelFormat32bppARGB;
}

// a Gdiplus::Bitmap that borrows a Pixmap's pixels and frees the Pixmap when destroyed.
// Gdiplus::Image has a virtual destructor, so deleting via Gdiplus::Bitmap* (as all
// callers do) runs this destructor and releases the borrowed buffer.
namespace {
struct PixmapBackedBitmap : Gdiplus::Bitmap {
    Pixmap* px;
    PixmapBackedBitmap(Pixmap* p, Gdiplus::PixelFormat fmt)
        : Gdiplus::Bitmap(p->width, p->height, p->stride, fmt, p->data), px(p) {}
    ~PixmapBackedBitmap() override { FreePixmap(px); }
};
} // namespace

// Zero-copy: wrap a Pixmap's pixels in a Gdiplus::Bitmap that borrows the buffer and
// takes ownership of the Pixmap (frees it when the returned bitmap is deleted). The
// Pixmap must outlive the bitmap, which the returned object guarantees. Returns nullptr
// (and frees px) on failure. Only BGRA8/BGR8 Pixmaps are supported.
Gdiplus::Bitmap* NewGdiplusBitmapFromPixmap(Pixmap* px) {
    if (!px) {
        return nullptr;
    }
    Gdiplus::PixelFormat fmt = PixmapToGdiplusPixelFormat(px);
    auto* bmp = new PixmapBackedBitmap(px, fmt);
    if (bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp; // also frees px
        return nullptr;
    }
    bmp->SetResolution(px->xres, px->yres);
    return bmp;
}

// Copy a Gdiplus::Bitmap's pixels out into a freshly allocated BGRA8 Pixmap (used to
// turn an awkwardly-formatted GDI+ decode - 16bpp TGA, CMYK JPEG - into a uniform
// Pixmap). Does not take ownership of bmp. Returns nullptr on failure.
Pixmap* PixmapFromGdiplus(Gdiplus::Bitmap* bmp) {
    if (!bmp) {
        return nullptr;
    }
    int w = (int)bmp->GetWidth();
    int h = (int)bmp->GetHeight();
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8);
    if (!px) {
        return nullptr;
    }
    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData bd;
    if (bmp->LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        FreePixmap(px);
        return nullptr;
    }
    for (int y = 0; y < h; y++) {
        memcpy(px->data + (size_t)y * px->stride, (u8*)bd.Scan0 + (size_t)y * bd.Stride, (size_t)w * 4);
    }
    bmp->UnlockBits(&bd);
    px->xres = bmp->GetHorizontalResolution();
    px->yres = bmp->GetVerticalResolution();
    return px;
}

// Apply an EXIF orientation (2..8) to a Pixmap, returning a possibly-rotated Pixmap and
// freeing the input. orientation 0/1 (or out of range) returns px unchanged. Rotation
// is done via GDI+, so this lives here rather than in the portable Pixmap.h.
Pixmap* PixmapApplyExifOrientation(Pixmap* px, int orientation) {
    if (!px || orientation < 2 || orientation > 8) {
        return px;
    }
    Gdiplus::PixelFormat fmt = PixmapToGdiplusPixelFormat(px);
    // borrow px's pixels (no copy), clone to an owning bitmap we can rotate in place
    Gdiplus::Bitmap borrow(px->width, px->height, px->stride, fmt, px->data);
    Gdiplus::Bitmap* rot = borrow.Clone(0, 0, px->width, px->height, fmt);
    if (!rot) {
        return px;
    }
    ApplyExifOrientation(rot, orientation);
    Pixmap* out = PixmapFromGdiplus(rot);
    delete rot;
    if (!out) {
        return px; // rotation failed; keep the unrotated pixels
    }
    out->xres = px->xres;
    out->yres = px->yres;
    FreePixmap(px);
    return out;
}

// Zero-copy borrow: wrap a Pixmap's pixels in a Gdiplus::Bitmap that does NOT own the
// Pixmap. The Pixmap must outlive the returned bitmap. Use when the Pixmap is owned
// elsewhere (e.g. EngineImage's frame list). Returns nullptr on failure.
Gdiplus::Bitmap* WrapPixmapGdiplus(const Pixmap* px) {
    if (!px) {
        return nullptr;
    }
    Gdiplus::PixelFormat fmt = PixmapToGdiplusPixelFormat(px);
    auto* bmp = new Gdiplus::Bitmap(px->width, px->height, px->stride, fmt, px->data);
    if (bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp;
        return nullptr;
    }
    bmp->SetResolution(px->xres, px->yres);
    return bmp;
}

// Decode an image to a single (first-frame) Pixmap. Caller owns it (FreePixmap).
Pixmap* PixmapFromDataWin(Str bmpData) {
    FileType kind = GuessFileTypeFromContent(bmpData);
    if (FileType::Tga == kind) {
        Pixmap* px = tga::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
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

// Decode an image to one Pixmap per frame: multi-page TIFF and animated GIF yield more
// than one, everything else exactly one. Empty on failure. Caller owns each Pixmap.
Vec<Pixmap*> PixmapsFromDataWin(Str bmpData) {
    Vec<Pixmap*> res;
    FileType kind = GuessFileTypeFromContent(bmpData);
    if (FileType::Tiff == kind || FileType::Gif == kind) {
        // decode every frame of a multi-page TIFF / animated GIF via GDI+
        Gdiplus::Bitmap* bmp = DecodeWithGdiplus(bmpData);
        if (!bmp) {
            bmp = DecodeWithWIC(bmpData);
        }
        if (bmp) {
            const GUID* dim = (FileType::Tiff == kind) ? &Gdiplus::FrameDimensionPage : &Gdiplus::FrameDimensionTime;
            UINT nFrames = bmp->GetFrameCount(dim);
            for (UINT i = 0; i < nFrames; i++) {
                if (bmp->SelectActiveFrame(dim, i) != Gdiplus::Ok) {
                    break;
                }
                Pixmap* px = PixmapFromGdiplus(bmp);
                if (px) {
                    res.Append(px);
                }
            }
            delete bmp;
        }
        if (len(res) > 0) {
            return res;
        }
    }
    // single-frame (or multi-frame decode failed): exactly one Pixmap
    Pixmap* px = PixmapFromDataWin(bmpData);
    if (px) {
        res.Append(px);
    }
    return res;
}

#define JP2_JP2H 0x6a703268 /**< JP2 header box (super-box) */
#define JP2_IHDR 0x69686472 /**< Image header box */

static bool BmpSizeFromData(ByteReader r, Size& result) {
    if (r.len < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
        return false;
    }
    BITMAPINFOHEADER bmi;
    bool ok = r.UnpackLE(&bmi, sizeof(bmi), "3d2w6d", sizeof(BITMAPFILEHEADER));
    ReportIf(!ok);
    result.dx = bmi.biWidth;
    result.dy = bmi.biHeight;
    return true;
}

static bool GifSizeFromData(ByteReader r, Size& result) {
    const u8* data = r.d;
    int n = r.len;
    if (n < 13) {
        return false;
    }
    // find the first image's actual size instead of using the
    // "logical screen" size which is sometimes too large
    int idx = 13;
    // skip the global color table
    if ((r.Byte(10) & 0x80)) {
        idx += 3 * (1 << ((r.Byte(10) & 0x07) + 1));
    }
    while (idx + 8 < r.len) {
        if (r.Byte(idx) == 0x2C) {
            result.dx = r.WordLE(idx + 5);
            result.dy = r.WordLE(idx + 7);
            return true;
        } else if (r.Byte(idx) == 0x21 && r.Byte(idx + 1) == 0xF9) {
            idx += 8;
        } else if (r.Byte(idx) == 0x21 && r.Byte(idx + 1) == 0xFE) {
            const u8* commentEnd = r.Find(idx + 2, 0x00);
            idx = commentEnd ? (int)(commentEnd - data) + 1 : n;
        } else if (r.Byte(idx) == 0x21 && r.Byte(idx + 1) == 0x01 && idx + 15 < n) {
            const u8* textDataEnd = r.Find(idx + 15, 0x00);
            idx = textDataEnd ? (int)(textDataEnd - data) + 1 : n;
        } else if (r.Byte(idx) == 0x21 && r.Byte(idx + 1) == 0xFF && idx + 14 < n) {
            const u8* applicationDataEnd = r.Find(idx + 14, 0x00);
            idx = applicationDataEnd ? (int)(applicationDataEnd - data) + 1 : n;
        } else {
            return false;
        }
    }
    return false;
}

// try to get image dimensions from EXIF sub-IFD (tags 0xA002/0xA003)
// tiffBase is the offset into r where the TIFF header starts
static bool JpegSizeFromExif(ByteReader r, int tiffBase, Size& result) {
    int n = r.len;
    if (tiffBase + 8 > n) {
        return false;
    }
    bool isBE = r.Byte(tiffBase) == 'M';
    // read IFD0 offset
    int ifdOff = (int)r.DWord(tiffBase + 4, isBE);
    int ifdAbs = tiffBase + ifdOff;
    if (ifdAbs + 2 > n) {
        return false;
    }
    WORD count = r.Word(ifdAbs, isBE);
    int exifIfdOff = 0;
    // scan IFD0 for ExifIFD pointer (tag 0x8769)
    for (WORD i = 0; i < count; i++) {
        int entryOff = ifdAbs + 2 + i * 12;
        if (entryOff + 12 > n) {
            break;
        }
        WORD tag = r.Word(entryOff, isBE);
        if (tag == 0x8769) {
            exifIfdOff = (int)r.DWord(entryOff + 8, isBE);
            break;
        }
    }
    if (exifIfdOff == 0) {
        return false;
    }
    // read EXIF sub-IFD
    int exifAbs = tiffBase + exifIfdOff;
    if (exifAbs + 2 > n) {
        return false;
    }
    count = r.Word(exifAbs, isBE);
    for (WORD i = 0; i < count; i++) {
        int entryOff = exifAbs + 2 + i * 12;
        if (entryOff + 12 > n) {
            break;
        }
        WORD tag = r.Word(entryOff, isBE);
        WORD type = r.Word(entryOff + 2, isBE);
        if (tag == 0xA002) {
            // PixelXDimension
            if (type == 4) {
                result.dx = (int)r.DWord(entryOff + 8, isBE);
            } else if (type == 3) {
                result.dx = r.Word(entryOff + 8, isBE);
            }
        } else if (tag == 0xA003) {
            // PixelYDimension
            if (type == 4) {
                result.dy = (int)r.DWord(entryOff + 8, isBE);
            } else if (type == 3) {
                result.dy = r.Word(entryOff + 8, isBE);
            }
        }
    }
    return !result.IsEmpty();
}

// Read EXIF orientation from JPEG data. Returns 1-8 or 0 if not found.
static int JpegExifOrientation(ByteReader r) {
    int n = r.len;
    int idx = 2;
    for (;;) {
        if (idx + 4 > n) {
            return 0;
        }
        if (r.Byte(idx) != 0xff) {
            return 0;
        }
        u8 marker = r.Byte(idx + 1);
        if (marker == 0xDA) { // start of scan, stop
            return 0;
        }
        int segLen = r.WordBE(idx + 2);
        if (marker == 0xE1 && idx + 10 <= n) {
            // APP1 - check for EXIF
            if (r.Byte(idx + 4) == 'E' && r.Byte(idx + 5) == 'x' && r.Byte(idx + 6) == 'i' && r.Byte(idx + 7) == 'f' &&
                r.Byte(idx + 8) == 0 && r.Byte(idx + 9) == 0) {
                return JpegExifOrientationFromTiff(r, idx + 10);
            }
        }
        int nextIdx = idx + segLen + 2;
        if (nextIdx <= idx) {
            return 0; // overflow protection
        }
        idx = nextIdx;
    }
}

// EXIF orientations 5-8 swap width and height
bool ExifOrientationSwapsDimensions(int orientation) {
    return orientation >= 5 && orientation <= 8;
}

static bool JpegSizeFromData(ByteReader r, Size& result) {
    // find the last start of frame marker for non-differential Huffman/arithmetic coding
    int n = r.len;
    int idx = 2;
    for (;;) {
        if (idx + 9 >= n) {
            return false;
        }
        u8 b = r.Byte(idx);
        if (b != 0xff) {
            return false;
        }
        b = r.Byte(idx + 1);
        if (0xC0 <= b && b <= 0xC3 || 0xC9 <= b && b <= 0xCB) {
            result.dx = r.WordBE(idx + 7);
            result.dy = r.WordBE(idx + 5);
            return true;
        }
        int segLen = r.WordBE(idx + 2);
        int nextIdx = idx + segLen + 2;
        if (nextIdx + 9 >= n) {
            // can't read past this segment; if it's APP1/EXIF, try parsing dimensions from EXIF
            if (b == 0xE1 && idx + 10 < n) {
                // check for "Exif\0\0" signature
                if (r.Byte(idx + 4) == 'E' && r.Byte(idx + 5) == 'x' && r.Byte(idx + 6) == 'i' &&
                    r.Byte(idx + 7) == 'f' && r.Byte(idx + 8) == 0 && r.Byte(idx + 9) == 0) {
                    int tiffBase = idx + 10; // TIFF header starts after "Exif\0\0"
                    if (JpegSizeFromExif(r, tiffBase, result)) {
                        return true;
                    }
                }
            }
            return false;
        }
        idx = nextIdx;
    }
    return false;
}

static bool TiffSizeFromData(ByteReader r, Size& result) {
    if (r.len < 10) {
        return false;
    }
    bool isBE = r.Byte(0) == 'M', isJXR = r.Byte(2) == 0xBC;
    ReportIf(!isBE && r.Byte(0) != 'I' || isJXR && isBE);
    const WORD WIDTH = isJXR ? 0xBC80 : 0x0100, HEIGHT = isJXR ? 0xBC81 : 0x0101;
    int idx = (int)r.DWord(4, isBE);
    WORD count = idx <= r.len - 2 ? r.Word(idx, isBE) : 0;
    for (idx += 2; count > 0 && idx <= r.len - 12; count--, idx += 12) {
        WORD tag = r.Word(idx, isBE), type = r.Word(idx + 2, isBE);
        if (r.DWord(idx + 4, isBE) != 1) {
            continue;
        } else if (WIDTH == tag && 4 == type) {
            result.dx = r.DWord(idx + 8, isBE);
        } else if (WIDTH == tag && 3 == type) {
            result.dx = r.Word(idx + 8, isBE);
        } else if (WIDTH == tag && 1 == type) {
            result.dx = r.Byte(idx + 8);
        } else if (HEIGHT == tag && 4 == type) {
            result.dy = r.DWord(idx + 8, isBE);
        } else if (HEIGHT == tag && 3 == type) {
            result.dy = r.Word(idx + 8, isBE);
        } else if (HEIGHT == tag && 1 == type) {
            result.dy = r.Byte(idx + 8);
        }
    }
    return true;
}

static bool PngSizeFromData(ByteReader r, Size& result) {
    if (r.len >= 24 && str::StartsWith(Str((char*)(r.d + 12), r.len - 12), "IHDR")) {
        result.dx = r.DWordBE(16);
        result.dy = r.DWordBE(20);
        return true;
    }
    return false;
}

static bool TgaSizeFromData(ByteReader r, Size& result) {
    if (r.len >= 16) {
        result.dx = r.WordLE(12);
        result.dy = r.WordLE(14);
        return true;
    }
    return false;
}

static bool Jp2SizeFromData(ByteReader r, Size& result) {
    int n = r.len;
    if (n < 32) {
        return false;
    }
    int idx = 0;
    while (idx < n - 32) {
        u32 boxLen = r.DWordBE(idx);
        u32 boxType = r.DWordBE(idx + 4);
        if (JP2_JP2H == boxType) {
            idx += 8;
            u32 boxLen2 = r.DWordBE(idx);
            u32 boxType2 = r.DWordBE(idx + 4);
            bool isIhdr = boxType2 == JP2_IHDR;
            idx += 8;
            if (isIhdr && boxLen2 <= (boxLen - 8)) {
                result.dy = r.DWordBE(idx);
                result.dx = r.DWordBE(idx + 4);
                if (result.dx > 64 * 1024 || result.dy > 64 * 1024) {
                    // sanity check, assuming that images that big can't
                    // possibly be valid
                    return false;
                }
                return true;
            }
            break;
        } else if (boxLen != 0 && (u32)idx < UINT32_MAX - boxLen) {
            idx += boxLen;
        } else {
            break;
        }
    }
    return false;
}

// adapted from http://cpansearch.perl.org/src/RJRAY/Image-Size-3.230/lib/Image/Size.pm
Size ImageSizeFromData(Str d) {
    Size result;
    bool ok = false;
    FileType kind = GuessFileTypeFromContent(d);

    ByteReader r(d);
    if (kind == FileType::Bmp) {
        ok = BmpSizeFromData(r, result);
    } else if (kind == FileType::Gif) {
        ok = GifSizeFromData(r, result);
    } else if (kind == FileType::Jpeg) {
        ok = JpegSizeFromData(r, result);
        if (ok && ExifOrientationSwapsDimensions(JpegExifOrientation(r))) {
            std::swap(result.dx, result.dy);
        }
    } else if (kind == FileType::Jxr || kind == FileType::Tiff) {
        ok = TiffSizeFromData(r, result);
    } else if (kind == FileType::Png) {
        ok = PngSizeFromData(r, result);
    } else if (kind == FileType::Tga) {
        ok = TgaSizeFromData(r, result);
    } else if (kind == FileType::Webp) {
        ok = WebpImageSizeFromData(r, result);
        if (ok && ExifOrientationSwapsDimensions(WebpExifOrientation(d))) {
            std::swap(result.dx, result.dy);
        }
    } else if (kind == FileType::Jp2) {
        ok = Jp2SizeFromData(r, result);
    } else if (kind == FileType::Avif || kind == FileType::Heic) {
        ok = AvifImageSizeFromData(r, result);
    }
    if (ok && !result.IsEmpty()) {
        return result;
    }

    // try expensive way of getting the info by decoding the image
    // (currently happens for animated GIF)
    Pixmap* px = PixmapFromDataWin(d);
    if (px) {
        result = Size(px->width, px->height);
        FreePixmap(px);
    }
    return result;
}

// like ImageSizeFromData but only parses headers, no full decode fallback
Size ImageSizeFromHeader(Str d) {
    Size result;
    bool ok = false;
    FileType kind = GuessFileTypeFromContent(d);

    ByteReader r(d);
    if (kind == FileType::Bmp) {
        ok = BmpSizeFromData(r, result);
    } else if (kind == FileType::Gif) {
        ok = GifSizeFromData(r, result);
    } else if (kind == FileType::Jpeg) {
        ok = JpegSizeFromData(r, result);
        if (ok && ExifOrientationSwapsDimensions(JpegExifOrientation(r))) {
            std::swap(result.dx, result.dy);
        }
    } else if (kind == FileType::Jxr || kind == FileType::Tiff) {
        ok = TiffSizeFromData(r, result);
    } else if (kind == FileType::Png) {
        ok = PngSizeFromData(r, result);
    } else if (kind == FileType::Tga) {
        ok = TgaSizeFromData(r, result);
    } else if (kind == FileType::Webp) {
        ok = WebpImageSizeFromData(r, result);
        if (ok && ExifOrientationSwapsDimensions(WebpExifOrientation(d))) {
            std::swap(result.dx, result.dy);
        }
    } else if (kind == FileType::Jp2) {
        ok = Jp2SizeFromData(r, result);
    } else if (kind == FileType::Avif || kind == FileType::Heic) {
        ok = AvifImageSizeFromData(r, result);
    }
    if (ok && !result.IsEmpty()) {
        return result;
    }
    return {};
}

CLSID GetGdiPlusEncoderClsid(WStr format) {
    CLSID null{};
    uint numEncoders, size;
    Status ok = Gdiplus::GetImageEncodersSize(&numEncoders, &size);
    if (ok != Ok || 0 == size) {
        return null;
    }
    ScopedMem<Gdiplus::ImageCodecInfo> codecInfo((Gdiplus::ImageCodecInfo*)malloc(size));
    if (!codecInfo) {
        return null;
    }
    GetImageEncoders(numEncoders, size, codecInfo);
    for (uint j = 0; j < numEncoders; j++) {
        if (wstr::Eq(WStr(codecInfo[j].MimeType), format)) {
            return codecInfo[j].Clsid;
        }
    }
    return null;
}

RenderedBitmap* LoadRenderedBitmapWin(Str path) {
    if (!path) {
        return nullptr;
    }
    Str data = file::ReadFile(path);
    if (!data) {
        return nullptr;
    }
    Gdiplus::Bitmap* bmp = NewGdiplusBitmapFromPixmap(PixmapFromDataWin(data));
    str::Free(data);

    if (!bmp) {
        return nullptr;
    }

    HBITMAP hbmp;
    RenderedBitmap* rendered = nullptr;
    if (bmp->GetHBITMAP((Gdiplus::ARGB)Gdiplus::Color::White, &hbmp) == Gdiplus::Ok) {
        rendered = new RenderedBitmap(hbmp, Size(bmp->GetWidth(), bmp->GetHeight()));
    }
    delete bmp;

    return rendered;
}

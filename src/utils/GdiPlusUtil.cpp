/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#if COMPILER_MSVC
#pragma warning(disable : 4668)
#endif
#include <wincodec.h>
#include "utils/ScopedWin.h"
#include "utils/GdiPlusUtil.h"
#include "utils/ByteReader.h"
#include "utils/FzImgReader.h"
#include "utils/TgaReader.h"
#include "utils/WebpReader.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

using namespace Gdiplus;

// Get width of each character and add them up.
// Doesn't seem to be any different than MeasureTextAccurate() i.e. it still
// underreports the width
RectF MeasureTextAccurate2(Graphics* g, Font* f, const WCHAR* s, int len) {
    CrashIf(0 >= len);
    FixedArray<Region, 1024> regionBuf(len);
    Region* r = regionBuf.Get();
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    RectF layoutRect;
    FixedArray<CharacterRange, 1024> charRangesBuf(len);
    CharacterRange* charRanges = charRangesBuf.Get();
    for (int i = 0; i < len; i++) {
        charRanges[i].First = i;
        charRanges[i].Length = 1;
    }
    sf.SetMeasurableCharacterRanges(len, charRanges);
    Status status = g->MeasureCharacterRanges(s, len, f, layoutRect, &sf, len, r);
    CrashIf(status != Ok);
    RectF bbox;
    REAL maxDy = 0;
    REAL totalDx = 0;
    for (int i = 0; i < len; i++) {
        r[i].GetBounds(&bbox, g);
        if (bbox.Height > maxDy)
            maxDy = bbox.Height;
        totalDx += bbox.Width;
    }
    bbox.Width = totalDx;
    bbox.Height = maxDy;
    return bbox;
}

// note: gdi+ seems to under-report the width, the longer the text, the
// bigger the difference. I'm trying to correct for that with those magic values
#define PER_CHAR_DX_ADJUST .2f
#define PER_STR_DX_ADJUST 1.f

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
RectF MeasureTextAccurate(Graphics* g, Font* f, const WCHAR* s, int len) {
    if (0 == len) {
        return RectF(0, 0, 0, 0); // TODO: should set height to font's height
    }
    // note: frankly, I don't see a difference between those StringFormat variations
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    // StringFormat sf(StringFormat::GenericDefault());
    // StringFormat sf;
    RectF layoutRect;
    CharacterRange cr(0, len);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region r;
    Status status = g->MeasureCharacterRanges(s, len, f, layoutRect, &sf, 1, &r);
    if (status != Ok) {
        // TODO: remove whem we figure out why we crash
        if (!s) {
            s = L"<null>";
        }
        AutoFree s2 = strconv::WstrToUtf8(s, (size_t)len);
        logf("MeasureTextAccurate: status: %d, font: %p, len: %d, s: '%s'\n", (int)status, f, len, s2.Get());
        CrashIf(status != Ok);
    }
    RectF bbox;
    r.GetBounds(&bbox, g);
    if (bbox.Width != 0)
        bbox.Width += PER_STR_DX_ADJUST + (PER_CHAR_DX_ADJUST * (float)len);
    return bbox;
}

// this usually reports size that is too large
RectF MeasureTextStandard(Graphics* g, Font* f, const WCHAR* s, int len) {
    RectF bbox;
    PointF pz(0, 0);
    g->MeasureString(s, len, f, pz, &bbox);
    return bbox;
}

RectF MeasureTextQuick(Graphics* g, Font* f, const WCHAR* s, int len) {
    CrashIf(0 >= len);

    static Vec<Font*> fontCache;
    static Vec<bool> fixCache;

    RectF bbox;
    g->MeasureString(s, len, f, PointF(0, 0), &bbox);
    int idx = fontCache.Find(f);
    if (-1 == idx) {
        LOGFONTW lfw;
        Status ok = f->GetLogFontW(g, &lfw);
        bool isItalicOrMonospace = Ok != ok || lfw.lfItalic || str::Eq(lfw.lfFaceName, L"Courier New") ||
                                   str::Find(lfw.lfFaceName, L"Consol") || str::EndsWith(lfw.lfFaceName, L"Mono") ||
                                   str::EndsWith(lfw.lfFaceName, L"Typewriter");
        fontCache.Append(f);
        fixCache.Append(isItalicOrMonospace);
        idx = (int)fontCache.size() - 1;
    }
    // most documents look good enough with these adjustments
    if (!fixCache.at(idx)) {
        REAL correct = 0;
        for (int i = 0; i < len; i++) {
            switch (s[i]) {
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
        bbox.Width *= (1.0f - correct / len) * 0.99f;
    }
    bbox.Height *= 0.95f;
    return bbox;
}

RectF MeasureText(Graphics* g, Font* f, const WCHAR* s, size_t len, TextMeasureAlgorithm algo) {
    // TODO: ideally we should not be here with len == 0. This
    // might indicate a problem with fromatter code. See internals-en.epub
    // for a repro
    if (-1 == len || 0 == len) {
        len = str::Len(s);
    }
    CrashIf((len == 0) || (len > INT_MAX));
    if (algo) {
        return algo(g, f, s, (int)len);
    }
    RectF bbox = MeasureTextAccurate(g, f, s, static_cast<int>(len));
    return bbox;
}

// returns number of characters of string s that fits in a given width dx
// note: could be speed up a bit because in our use case we already know
// the width of the whole string so we could supply it to the function, but
// this shouldn't happen often, so that's fine. It's also possible that
// a smarter approach is possible, but this usually only does 3 MeasureText
// calls, so it's not that bad
size_t StringLenForWidth(Graphics* g, Font* f, const WCHAR* s, size_t len, float dx, TextMeasureAlgorithm algo) {
    RectF r = MeasureText(g, f, s, len, algo);
    if (r.Width <= dx) {
        return len;
    }
    // make the best guess of the length that fits
    size_t n = (size_t)((dx / r.Width) * (float)len);
    CrashIf((0 == n) || (n > len));
    r = MeasureText(g, f, s, n, algo);
    // find the length len of s that fits within dx iff width of len+1 exceeds dx
    int dir = 1; // increasing length
    if (r.Width > dx) {
        dir = -1; // decreasing length
    }
    for (;;) {
        n += dir;
        r = MeasureText(g, f, s, n, algo);
        if (1 == dir) {
            // if advancing length, we know that previous string did fit, so if
            // the new one doesn't fit, the previous length was the right one
            if (r.Width > dx)
                return n - 1;
        } else {
            // if decreasing length, we know that previous string didn't fit, so if
            // the one one fits, it's of the correct length
            if (r.Width < dx)
                return n;
        }
    }
}

// TODO: not quite sure why spaceDx1 != spaceDx2, using spaceDx2 because
// is smaller and looks as better spacing to me
REAL GetSpaceDx(Graphics* g, Font* f, TextMeasureAlgorithm algo) {
    RectF bbox;
#if 0
    bbox = MeasureText(g, f, L" ", 1, algo);
    REAL spaceDx1 = bbox.Width;
    return spaceDx1;
#else
    // this method seems to return (much) smaller size that measuring
    // the space itself
    bbox = MeasureText(g, f, L"wa", 2, algo);
    REAL l1 = bbox.Width;
    bbox = MeasureText(g, f, L"w a", 3, algo);
    REAL l2 = bbox.Width;
    REAL spaceDx2 = l2 - l1;
    return spaceDx2;
#endif
}

void GetBaseTransform(Matrix& m, RectF pageRect, float zoom, int rotation) {
    rotation = rotation % 360;
    if (rotation < 0)
        rotation = rotation + 360;
    if (90 == rotation)
        m.Translate(0, -pageRect.Height, MatrixOrderAppend);
    else if (180 == rotation)
        m.Translate(-pageRect.Width, -pageRect.Height, MatrixOrderAppend);
    else if (270 == rotation)
        m.Translate(-pageRect.Width, 0, MatrixOrderAppend);
    else if (0 == rotation)
        m.Translate(0, 0, MatrixOrderAppend);
    else
        CrashIf(true);

    m.Scale(zoom, zoom, MatrixOrderAppend);
    m.Rotate((REAL)rotation, MatrixOrderAppend);
}

static Bitmap* WICDecodeImageFromStream(IStream* stream) {
    ScopedCom com;

#define HR(hr)      \
    if (FAILED(hr)) \
        return nullptr;
    ScopedComPtr<IWICImagingFactory> pFactory;
    if (!pFactory.Create(CLSID_WICImagingFactory))
        return nullptr;
    ScopedComPtr<IWICBitmapDecoder> pDecoder;
    HR(pFactory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder));
    ScopedComPtr<IWICBitmapFrameDecode> srcFrame;
    HR(pDecoder->GetFrame(0, &srcFrame));
    ScopedComPtr<IWICFormatConverter> pConverter;
    HR(pFactory->CreateFormatConverter(&pConverter));
    HR(pConverter->Initialize(srcFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.f,
                              WICBitmapPaletteTypeCustom));

    UINT w, h;
    HR(pConverter->GetSize(&w, &h));
    double xres, yres;
    HR(pConverter->GetResolution(&xres, &yres));
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    Rect bmpRect(0, 0, w, h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Ok)
        return nullptr;
    HR(pConverter->CopyPixels(nullptr, bmpData.Stride, bmpData.Stride * h, (BYTE*)bmpData.Scan0));
    bmp.UnlockBits(&bmpData);
    bmp.SetResolution((REAL)xres, (REAL)yres);
#undef HR

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
}

ImgFormat GfxFormatFromData(const char* data, size_t len) {
    if (!data || len < 12) {
        return ImgFormat::Unknown;
    }
    // check the most common formats first
    if (str::StartsWith(data, "\x89PNG\x0D\x0A\x1A\x0A")) {
        return ImgFormat::PNG;
    }
    if (str::StartsWith(data, "\xFF\xD8")) {
        return ImgFormat::JPEG;
    }
    if (str::StartsWith(data, "GIF87a") || str::StartsWith(data, "GIF89a")) {
        return ImgFormat::GIF;
    }
    if (str::StartsWith(data, "BM")) {
        return ImgFormat::BMP;
    }
    if (memeq(data, "MM\x00\x2A", 4) || memeq(data, "II\x2A\x00", 4)) {
        return ImgFormat::TIFF;
    }
    if (tga::HasSignature(data, len)) {
        return ImgFormat::TGA;
    }
    if (memeq(data, "II\xBC\x01", 4) || memeq(data, "II\xBC\x00", 4)) {
        return ImgFormat::JXR;
    }
    if (webp::HasSignature(data, len)) {
        return ImgFormat::WebP;
    }
    if (memeq(data, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", 12)) {
        return ImgFormat::JP2;
    }
    return ImgFormat::Unknown;
}

const WCHAR* GfxFileExtFromData(const char* data, size_t len) {
    switch (GfxFormatFromData(data, len)) {
        case ImgFormat::BMP:
            return L".bmp";
        case ImgFormat::GIF:
            return L".gif";
        case ImgFormat::JPEG:
            return L".jpg";
        case ImgFormat::JXR:
            return L".jxr";
        case ImgFormat::PNG:
            return L".png";
        case ImgFormat::TGA:
            return L".tga";
        case ImgFormat::TIFF:
            return L".tif";
        case ImgFormat::WebP:
            return L".webp";
        case ImgFormat::JP2:
            return L".jp2";
        default:
            return nullptr;
    }
}

// Windows' JPEG codec doesn't support arithmetic coding
static bool JpegUsesArithmeticCoding(const char* data, size_t len) {
    CrashIf(GfxFormatFromData(data, len) != ImgFormat::JPEG);

    ByteReader r(data, len);
    for (size_t ix = 2; ix + 9 < len && r.Byte(ix) == 0xFF; ix += r.WordBE(ix + 2) + 2) {
        if (0xC9 <= r.Byte(ix + 1) && r.Byte(ix + 1) <= 0xCB) {
            // found the start of a frame using arithmetic coding
            return true;
        }
    }
    return false;
}

// Windows' PNG codec fails to handle an edge case, resulting in
// an infinite loop (cf. http://cxsecurity.com/issue/WLB-2014080021 )
static bool PngRequiresPresetDict(const char* data, size_t len) {
    CrashIf(GfxFormatFromData(data, len) != ImgFormat::PNG);

    ByteReader r(data, len);
    for (size_t ix = 8; ix + 12 < len && r.DWordBE(ix) < len - ix - 12; ix += r.DWordBE(ix) + 12) {
        if (r.DWordBE(ix + 4) == 0x49444154 /* IDAT */) {
            // check the zlib header's FDICT flag
            // (even if this image claims not to be zlib compressed!)
            return (r.Byte(ix + 9) & (1 << 5)) != 0;
        }
    }

    return false;
}

bool IsGdiPlusNativeFormat(const char* data, size_t len) {
    ImgFormat fmt = GfxFormatFromData(data, len);
    return ImgFormat::BMP == fmt || ImgFormat::GIF == fmt || ImgFormat::TIFF == fmt ||
           (ImgFormat::JPEG == fmt && !JpegUsesArithmeticCoding(data, len)) ||
           (ImgFormat::PNG == fmt && !PngRequiresPresetDict(data, len));
}

// cf. http://stackoverflow.com/questions/4598872/creating-hbitmap-from-memory-buffer/4616394#4616394
Bitmap* BitmapFromData(const char* data, size_t len) {
    ImgFormat format = GfxFormatFromData(data, len);
    if (ImgFormat::TGA == format) {
        return tga::ImageFromData(data, len);
    }
    if (ImgFormat::WebP == format) {
        return webp::ImageFromData(data, len);
    }
    if (ImgFormat::JP2 == format) {
        return fitz::ImageFromData(data, len);
    }
    if (ImgFormat::JPEG == format && JpegUsesArithmeticCoding(data, len)) {
        return fitz::ImageFromData(data, len);
    }
    if (ImgFormat::PNG == format && PngRequiresPresetDict(data, len)) {
        return nullptr;
    }

    auto strm = CreateStreamFromData({data, len});
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return nullptr;
    }
    if (ImgFormat::JXR == format) {
        return WICDecodeImageFromStream(stream);
    }

    Bitmap* bmp = Bitmap::FromStream(stream);
    if (bmp && bmp->GetLastStatus() != Ok) {
        delete bmp;
        bmp = nullptr;
    }
    // GDI+ under Windows XP sometimes fails to extract JPEG image dimensions
    if (bmp && ImgFormat::JPEG == format && (0 == bmp->GetWidth() || 0 == bmp->GetHeight())) {
        delete bmp;
        bmp = fitz::ImageFromData(data, len);
    }
    return bmp;
}

// adapted from http://cpansearch.perl.org/src/RJRAY/Image-Size-3.230/lib/Image/Size.pm
Size BitmapSizeFromData(const char* data, size_t len) {
    Size result;
    ByteReader r(data, len);
    switch (GfxFormatFromData(data, len)) {
        case ImgFormat::BMP:
            if (len >= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
                BITMAPINFOHEADER bmi;
                bool ok = r.UnpackLE(&bmi, sizeof(bmi), "3d2w6d", sizeof(BITMAPFILEHEADER));
                CrashIf(!ok);
                result.Width = bmi.biWidth;
                result.Height = bmi.biHeight;
            }
            break;
        case ImgFormat::GIF:
            if (len >= 13) {
                // find the first image's actual size instead of using the
                // "logical screen" size which is sometimes too large
                size_t ix = 13;
                // skip the global color table
                if ((r.Byte(10) & 0x80))
                    ix += 3 * (1 << ((r.Byte(10) & 0x07) + 1));
                while (ix + 8 < len) {
                    if (r.Byte(ix) == 0x2C) {
                        result.Width = r.WordLE(ix + 5);
                        result.Height = r.WordLE(ix + 7);
                        break;
                    } else if (r.Byte(ix) == 0x21 && r.Byte(ix + 1) == 0xF9)
                        ix += 8;
                    else if (r.Byte(ix) == 0x21 && r.Byte(ix + 1) == 0xFE) {
                        const char* commentEnd = r.Find(ix + 2, 0x00);
                        ix = commentEnd ? commentEnd - data + 1 : len;
                    } else if (r.Byte(ix) == 0x21 && r.Byte(ix + 1) == 0x01 && ix + 15 < len) {
                        const char* textDataEnd = r.Find(ix + 15, 0x00);
                        ix = textDataEnd ? textDataEnd - data + 1 : len;
                    } else if (r.Byte(ix) == 0x21 && r.Byte(ix + 1) == 0xFF && ix + 14 < len) {
                        const char* applicationDataEnd = r.Find(ix + 14, 0x00);
                        ix = applicationDataEnd ? applicationDataEnd - data + 1 : len;
                    } else
                        break;
                }
            }
            break;
        case ImgFormat::JPEG:
            // find the last start of frame marker for non-differential Huffman/arithmetic coding
            for (size_t ix = 2; ix + 9 < len && r.Byte(ix) == 0xFF;) {
                if (0xC0 <= r.Byte(ix + 1) && r.Byte(ix + 1) <= 0xC3 ||
                    0xC9 <= r.Byte(ix + 1) && r.Byte(ix + 1) <= 0xCB) {
                    result.Width = r.WordBE(ix + 7);
                    result.Height = r.WordBE(ix + 5);
                }
                ix += r.WordBE(ix + 2) + 2;
            }
            break;
        case ImgFormat::JXR:
        case ImgFormat::TIFF:
            if (len >= 10) {
                bool isBE = r.Byte(0) == 'M', isJXR = r.Byte(2) == 0xBC;
                CrashIf(!isBE && r.Byte(0) != 'I' || isJXR && isBE);
                const WORD WIDTH = isJXR ? 0xBC80 : 0x0100, HEIGHT = isJXR ? 0xBC81 : 0x0101;
                size_t idx = r.DWord(4, isBE);
                WORD count = idx <= len - 2 ? r.Word(idx, isBE) : 0;
                for (idx += 2; count > 0 && idx <= len - 12; count--, idx += 12) {
                    WORD tag = r.Word(idx, isBE), type = r.Word(idx + 2, isBE);
                    if (r.DWord(idx + 4, isBE) != 1)
                        continue;
                    else if (WIDTH == tag && 4 == type)
                        result.Width = r.DWord(idx + 8, isBE);
                    else if (WIDTH == tag && 3 == type)
                        result.Width = r.Word(idx + 8, isBE);
                    else if (WIDTH == tag && 1 == type)
                        result.Width = r.Byte(idx + 8);
                    else if (HEIGHT == tag && 4 == type)
                        result.Height = r.DWord(idx + 8, isBE);
                    else if (HEIGHT == tag && 3 == type)
                        result.Height = r.Word(idx + 8, isBE);
                    else if (HEIGHT == tag && 1 == type)
                        result.Height = r.Byte(idx + 8);
                }
            }
            break;
        case ImgFormat::PNG:
            if (len >= 24 && str::StartsWith(data + 12, "IHDR")) {
                result.Width = r.DWordBE(16);
                result.Height = r.DWordBE(20);
            }
            break;
        case ImgFormat::TGA:
            if (len >= 16) {
                result.Width = r.WordLE(12);
                result.Height = r.WordLE(14);
            }
            break;
        case ImgFormat::WebP:
            if (len >= 30 && str::StartsWith(data + 12, "VP8 ")) {
                result.Width = r.WordLE(26) & 0x3fff;
                result.Height = r.WordLE(28) & 0x3fff;
            } else {
                result = webp::SizeFromData(data, len);
            }
            break;
        case ImgFormat::JP2:
            if (len >= 32) {
                size_t ix = 0;
                while (ix < len - 32) {
                    uint32_t lbox = r.DWordBE(ix);
                    uint32_t tbox = r.DWordBE(ix + 4);
                    if (0x6A703268 /* jp2h */ == tbox) {
                        ix += 8;
                        if (r.DWordBE(ix) == 24 && r.DWordBE(ix + 4) == 0x69686472 /* ihdr */) {
                            result.Width = r.DWordBE(ix + 16);
                            result.Height = r.DWordBE(ix + 12);
                        }
                        break;
                    } else if (lbox != 0 && ix < UINT32_MAX - lbox) {
                        ix += lbox;
                    } else {
                        break;
                    }
                }
            }
            break;
    }

    if (result.Empty()) {
        // let GDI+ extract the image size if we've failed
        // (currently happens for animated GIF)
        Bitmap* bmp = BitmapFromData(data, len);
        if (bmp)
            result = Size(bmp->GetWidth(), bmp->GetHeight());
        delete bmp;
    }

    return result;
}

CLSID GetEncoderClsid(const WCHAR* format) {
    CLSID null = {0};
    UINT numEncoders, size;
    Status ok = GetImageEncodersSize(&numEncoders, &size);
    if (ok != Ok || 0 == size)
        return null;
    ScopedMem<ImageCodecInfo> codecInfo((ImageCodecInfo*)malloc(size));
    if (!codecInfo)
        return null;
    GetImageEncoders(numEncoders, size, codecInfo);
    for (UINT j = 0; j < numEncoders; j++) {
        if (str::Eq(codecInfo[j].MimeType, format)) {
            return codecInfo[j].Clsid;
        }
    }
    return null;
}

size_t ImageData::size() const {
    return len;
}

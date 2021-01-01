/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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

// using namespace Gdiplus;

using Gdiplus::ARGB;
using Gdiplus::Bitmap;
using Gdiplus::BitmapData;
using Gdiplus::Brush;
using Gdiplus::CharacterRange;
using Gdiplus::Color;
using Gdiplus::CombineModeReplace;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::FontStyle;
using Gdiplus::FontStyleBold;
using Gdiplus::FontStyleItalic;
using Gdiplus::FontStyleRegular;
using Gdiplus::FontStyleStrikeout;
using Gdiplus::FontStyleUnderline;
using Gdiplus::FrameDimensionPage;
using Gdiplus::FrameDimensionTime;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Image;
using Gdiplus::ImageAttributes;
using Gdiplus::InterpolationModeHighQualityBicubic;
using Gdiplus::LinearGradientBrush;
using Gdiplus::LinearGradientMode;
using Gdiplus::LinearGradientModeVertical;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::OutOfMemory;
using Gdiplus::Pen;
using Gdiplus::PenAlignmentInset;
using Gdiplus::PropertyItem;
using Gdiplus::Region;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsDirectionRightToLeft;
using Gdiplus::StringFormatFlagsMeasureTrailingSpaces;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;
using Gdiplus::Win32Error;

Gdiplus::RectF RectToRectF(const Gdiplus::Rect r) {
    return Gdiplus::RectF((float)r.X, (float)r.Y, (float)r.Width, (float)r.Height);
}

// Get width of each character and add them up.
// Doesn't seem to be any different than MeasureTextAccurate() i.e. it still
// underreports the width
RectF MeasureTextAccurate2(Graphics* g, Font* f, const WCHAR* s, int len) {
    CrashIf(0 >= len);
    FixedArray<Region, 1024> regionBuf(len);
    Region* r = regionBuf.Get();
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    Gdiplus::RectF layoutRect;
    FixedArray<CharacterRange, 1024> charRangesBuf(len);
    CharacterRange* charRanges = charRangesBuf.Get();
    for (int i = 0; i < len; i++) {
        charRanges[i].First = i;
        charRanges[i].Length = 1;
    }
    sf.SetMeasurableCharacterRanges(len, charRanges);
    Status status = g->MeasureCharacterRanges(s, len, f, layoutRect, &sf, len, r);
    CrashIf(status != Ok);
    Gdiplus::RectF bbox;
    float maxDy = 0;
    float totalDx = 0;
    for (int i = 0; i < len; i++) {
        r[i].GetBounds(&bbox, g);
        if (bbox.Height > maxDy) {
            maxDy = bbox.Height;
        }
        totalDx += bbox.Width;
    }
    bbox.Width = totalDx;
    bbox.Height = maxDy;
    return RectF{bbox};
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
    Gdiplus::RectF layoutRect;
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
    Gdiplus::RectF bbox;
    r.GetBounds(&bbox, g);
    if (bbox.Width != 0) {
        bbox.Width += PER_STR_DX_ADJUST + (PER_CHAR_DX_ADJUST * (float)len);
    }
    return RectF{bbox};
}

// this usually reports size that is too large
RectF MeasureTextStandard(Graphics* g, Font* f, const WCHAR* s, int len) {
    Gdiplus::RectF bbox;
    Gdiplus::PointF pz(0, 0);
    g->MeasureString(s, len, f, pz, &bbox);
    return RectF{bbox};
}

RectF MeasureTextQuick(Graphics* g, Font* f, const WCHAR* s, int len) {
    CrashIf(0 >= len);

    static Vec<Font*> fontCache;
    static Vec<bool> fixCache;

    Gdiplus::RectF bbox;
    g->MeasureString(s, len, f, Gdiplus::PointF(0, 0), &bbox);
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
        float correct = 0;
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
    return RectF{bbox};
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
    auto bbox = MeasureTextAccurate(g, f, s, static_cast<int>(len));
    return bbox;
}

// returns number of characters of string s that fits in a given width dx
// note: could be speed up a bit because in our use case we already know
// the width of the whole string so we could supply it to the function, but
// this shouldn't happen often, so that's fine. It's also possible that
// a smarter approach is possible, but this usually only does 3 MeasureText
// calls, so it's not that bad
size_t StringLenForWidth(Graphics* g, Font* f, const WCHAR* s, size_t len, float dx, TextMeasureAlgorithm algo) {
    auto r = MeasureText(g, f, s, len, algo);
    if (r.dx <= dx) {
        return len;
    }
    // make the best guess of the length that fits
    size_t n = (size_t)((dx / r.dx) * (float)len);
    CrashIf((0 == n) || (n > len));
    r = MeasureText(g, f, s, n, algo);
    // find the length len of s that fits within dx iff width of len+1 exceeds dx
    int dir = 1; // increasing length
    if (r.dx > dx) {
        dir = -1; // decreasing length
    }
    for (;;) {
        n += dir;
        r = MeasureText(g, f, s, n, algo);
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
    bbox = MeasureText(g, f, L"wa", 2, algo);
    float l1 = bbox.dx;
    bbox = MeasureText(g, f, L"w a", 3, algo);
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
        CrashIf(true);
    }

    m.Scale(zoom, zoom, MatrixOrderAppend);
    m.Rotate((float)rotation, MatrixOrderAppend);
}

static Bitmap* WICDecodeImageFromStream(IStream* stream) {
    ScopedCom com;

#define HR(hr)      \
    if (FAILED(hr)) \
        return nullptr;
    ScopedComPtr<IWICImagingFactory> pFactory;
    if (!pFactory.Create(CLSID_WICImagingFactory)) {
        return nullptr;
    }
    ScopedComPtr<IWICBitmapDecoder> pDecoder;
    HR(pFactory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder));
    ScopedComPtr<IWICBitmapFrameDecode> srcFrame;
    HR(pDecoder->GetFrame(0, &srcFrame));
    ScopedComPtr<IWICFormatConverter> pConverter;
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

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
}

#include "utils/GuessFileType.h"

static const Kind gImageKinds[] = {
    kindFilePng, kindFileJpeg, kindFileGif,  kindFileBmp, kindFileTiff,
    kindFileTga, kindFileJxr,  kindFileWebp, kindFileJp2,
};

static const ImgFormat gImageFormats[] = {
    ImgFormat::PNG, ImgFormat::JPEG, ImgFormat::GIF,  ImgFormat::BMP, ImgFormat::TIFF,
    ImgFormat::TGA, ImgFormat::JXR,  ImgFormat::WebP, ImgFormat::JP2,
};

static const WCHAR* gImageFormatExts =
    L".png\0"
    L".jpg\0"
    L".gif\0"
    L".bmp\0"
    L".tif\0"
    L".tga\0"
    L".jxr\0"
    L".webp\0"
    L".jp2\0"
    L"\0";

static_assert(dimof(gImageKinds) == dimof(gImageFormats));

static int FindImageKindIdx(Kind kind) {
    int n = (int)dimof(gImageKinds);
    for (int i = 0; i < n; i++) {
        if (kind == gImageKinds[i]) {
            return i;
        }
    }
    return -1;
}

ImgFormat GfxFormatFromData(std::span<u8> d) {
    Kind kind = GuessFileTypeFromContent(d);
    int idx = FindImageKindIdx(kind);
    if (idx >= 0) {
        return gImageFormats[idx];
    }
    return ImgFormat::Unknown;
}

const WCHAR* GfxFileExtFromData(std::span<u8> d) {
    Kind kind = GuessFileTypeFromContent(d);
    int idx = FindImageKindIdx(kind);
    if (idx >= 0) {
        return seqstrings::IdxToStr(gImageFormatExts, idx);
    }
    return nullptr;
}

// Windows' JPEG codec doesn't support arithmetic coding
static bool JpegUsesArithmeticCoding(std::span<u8> d) {
    CrashIf(GfxFormatFromData(d) != ImgFormat::JPEG);

    ByteReader r(d);
    size_t len = d.size();
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
static bool PngRequiresPresetDict(std::span<u8> d) {
    CrashIf(GfxFormatFromData(d) != ImgFormat::PNG);

    ByteReader r(d);
    size_t len = d.size();
    for (size_t i = 8; i + 12 < len && r.DWordBE(i) < len - i - 12; i += r.DWordBE(i) + 12) {
        if (r.DWordBE(i + 4) == 0x49444154 /* IDAT */) {
            // check the zlib header's FDICT flag
            // (even if this image claims not to be zlib compressed!)
            return (r.Byte(i + 9) & (1 << 5)) != 0;
        }
    }

    return false;
}

bool IsGdiPlusNativeFormat(std::span<u8> d) {
    ImgFormat fmt = GfxFormatFromData(d);
    return ImgFormat::BMP == fmt || ImgFormat::GIF == fmt || ImgFormat::TIFF == fmt ||
           (ImgFormat::JPEG == fmt && !JpegUsesArithmeticCoding(d)) ||
           (ImgFormat::PNG == fmt && !PngRequiresPresetDict(d));
}

// see http://stackoverflow.com/questions/4598872/creating-hbitmap-from-memory-buffer/4616394#4616394
Bitmap* BitmapFromData(std::span<u8> bmpData) {
    ImgFormat format = GfxFormatFromData(bmpData);
    if (ImgFormat::TGA == format) {
        return tga::ImageFromData(bmpData);
    }
    if (ImgFormat::WebP == format) {
        return webp::ImageFromData(bmpData);
    }
    if (ImgFormat::JP2 == format) {
        return fitz::ImageFromData(bmpData);
    }
    if (ImgFormat::JPEG == format && JpegUsesArithmeticCoding(bmpData)) {
        return fitz::ImageFromData(bmpData);
    }
    if (ImgFormat::PNG == format && PngRequiresPresetDict(bmpData)) {
        return nullptr;
    }

    auto strm = CreateStreamFromData(bmpData);
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
        bmp = fitz::ImageFromData(bmpData);
    }
    return bmp;
}

// adapted from http://cpansearch.perl.org/src/RJRAY/Image-Size-3.230/lib/Image/Size.pm
Size BitmapSizeFromData(std::span<u8> d) {
    Size result;
    ByteReader r(d);
    size_t len = d.size();
    u8* data = d.data();
    switch (GfxFormatFromData(d)) {
        case ImgFormat::BMP:
            if (len >= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
                BITMAPINFOHEADER bmi;
                bool ok = r.UnpackLE(&bmi, sizeof(bmi), "3d2w6d", sizeof(BITMAPFILEHEADER));
                CrashIf(!ok);
                result.dx = bmi.biWidth;
                result.dy = bmi.biHeight;
            }
            break;
        case ImgFormat::GIF:
            if (len >= 13) {
                // find the first image's actual size instead of using the
                // "logical screen" size which is sometimes too large
                size_t ix = 13;
                // skip the global color table
                if ((r.Byte(10) & 0x80)) {
                    ix += 3 * (1 << ((r.Byte(10) & 0x07) + 1));
                }
                while (ix + 8 < len) {
                    if (r.Byte(ix) == 0x2C) {
                        result.dx = r.WordLE(ix + 5);
                        result.dy = r.WordLE(ix + 7);
                        break;
                    } else if (r.Byte(ix) == 0x21 && r.Byte(ix + 1) == 0xF9) {
                        ix += 8;
                    } else if (r.Byte(ix) == 0x21 && r.Byte(ix + 1) == 0xFE) {
                        const u8* commentEnd = r.Find(ix + 2, 0x00);
                        ix = commentEnd ? commentEnd - data + 1 : len;
                    } else if (r.Byte(ix) == 0x21 && r.Byte(ix + 1) == 0x01 && ix + 15 < len) {
                        const u8* textDataEnd = r.Find(ix + 15, 0x00);
                        ix = textDataEnd ? textDataEnd - data + 1 : len;
                    } else if (r.Byte(ix) == 0x21 && r.Byte(ix + 1) == 0xFF && ix + 14 < len) {
                        const u8* applicationDataEnd = r.Find(ix + 14, 0x00);
                        ix = applicationDataEnd ? applicationDataEnd - data + 1 : len;
                    } else {
                        break;
                    }
                }
            }
            break;
        case ImgFormat::JPEG:
            // find the last start of frame marker for non-differential Huffman/arithmetic coding
            for (size_t ix = 2; ix + 9 < len && r.Byte(ix) == 0xFF;) {
                if (0xC0 <= r.Byte(ix + 1) && r.Byte(ix + 1) <= 0xC3 ||
                    0xC9 <= r.Byte(ix + 1) && r.Byte(ix + 1) <= 0xCB) {
                    result.dx = r.WordBE(ix + 7);
                    result.dy = r.WordBE(ix + 5);
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
            }
            break;
        case ImgFormat::PNG:
            if (len >= 24 && str::StartsWith(data + 12, "IHDR")) {
                result.dx = r.DWordBE(16);
                result.dy = r.DWordBE(20);
            }
            break;
        case ImgFormat::TGA:
            if (len >= 16) {
                result.dx = r.WordLE(12);
                result.dy = r.WordLE(14);
            }
            break;
        case ImgFormat::WebP:
            if (len >= 30 && str::StartsWith(data + 12, "VP8 ")) {
                result.dx = r.WordLE(26) & 0x3fff;
                result.dy = r.WordLE(28) & 0x3fff;
            } else {
                result = webp::SizeFromData(d);
            }
            break;
        case ImgFormat::JP2:
            if (len >= 32) {
                size_t ix = 0;
                while (ix < len - 32) {
                    u32 lbox = r.DWordBE(ix);
                    u32 tbox = r.DWordBE(ix + 4);
                    if (0x6A703268 /* jp2h */ == tbox) {
                        ix += 8;
                        if (r.DWordBE(ix) == 24 && r.DWordBE(ix + 4) == 0x69686472 /* ihdr */) {
                            result.dx = r.DWordBE(ix + 16);
                            result.dy = r.DWordBE(ix + 12);
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

    if (result.IsEmpty()) {
        // let GDI+ extract the image size if we've failed
        // (currently happens for animated GIF)
        Bitmap* bmp = BitmapFromData(d);
        if (bmp) {
            result = Size(bmp->GetWidth(), bmp->GetHeight());
        }
        delete bmp;
    }

    return result;
}

CLSID GetEncoderClsid(const WCHAR* format) {
    CLSID null = {0};
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
        if (str::Eq(codecInfo[j].MimeType, format)) {
            return codecInfo[j].Clsid;
        }
    }
    return null;
}

size_t ImageData::size() const {
    return len;
}

std::span<u8> ImageData::AsSpan() const {
    return {(u8*)data, len};
}

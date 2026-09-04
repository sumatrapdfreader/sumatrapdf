/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/File.h"
#include "base/GuessFileType.h"
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

#include "ImageReader.h"

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

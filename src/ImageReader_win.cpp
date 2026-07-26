/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/ScopedWin.h"
#include "base/TgaReader.h"
#include "base/Win.h"
#include "base/GdiPlus.h"
#include "AvifReader.h"
#include "JxlReader.h"
#include "WebpReader.h"
#include "ImageReader.h"

#if COMPILER_MSVC
#pragma warning(disable : 4668)
#endif
#include <wincodec.h>

using Gdiplus::Bitmap;
using Gdiplus::BitmapData;
using Gdiplus::Ok;
using Gdiplus::Status;

// WebP / JXL / AVIF / HEIC via dedicated decoders (not GDI+/WIC).
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
        Pixmap* px = PixmapFromAvifData(bmpData);
        if (px) {
            return px;
        }
    }
    return nullptr;
}

static Bitmap* WICDecodeImageFromStream(IStream* stream) {
    ScopedCom com;
    HRESULT hr;

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
    ApplyExifOrientation(&bmp, orientation);
    return bmp.Clone(0, 0, bmp.GetWidth(), bmp.GetHeight(), PixelFormat32bppARGB);
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
    return WICDecodeImageFromStream(stream);
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

// Decode an image to a single (first-frame) Pixmap via Windows paths (TGA, ext formats, GDI+/WIC).
static Pixmap* PixmapFromDataWin(Str bmpData) {
    FileType kind = GuessFileTypeFromData(bmpData);
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
    return res;
}

// Decode an image to one Pixmap per frame: multi-page TIFF and animated GIF yield more
// than one, everything else exactly one. Empty on failure. Caller owns each Pixmap.
static Vec<Pixmap*> PixmapsFromDataWin(Str bmpData) {
    Vec<Pixmap*> res;
    FileType kind = GuessFileTypeFromData(bmpData);
    if (FileType::Tiff == kind || FileType::Gif == kind) {
        res = PixmapsFromMultiFrameData(bmpData, kind);
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

Pixmap* PixmapFromData(Str bmpData) {
    Pixmap* px = PixmapFromDataWin(bmpData);
    if (px) {
        return px;
    }
    return PixmapFromDataFz(bmpData);
}

Vec<Pixmap*> PixmapsFromData(Str bmpData) {
    Vec<Pixmap*> res = PixmapsFromDataWin(bmpData);
    if (len(res) > 0) {
        return res;
    }

    Pixmap* px = PixmapFromDataFz(bmpData);
    if (px) {
        res.Append(px);
    }
    return res;
}

RenderedBitmap* LoadRenderedBitmap(Str path) {
    if (!path) {
        return nullptr;
    }
    Str data = file::ReadFile(path);
    if (!data) {
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
        rendered = new RenderedBitmap(hbmp, Size(bmp->GetWidth(), bmp->GetHeight()));
    }
    delete bmp;

    return rendered;
}

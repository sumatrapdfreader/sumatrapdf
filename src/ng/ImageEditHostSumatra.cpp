/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// What the shared image editor (ImageSaveCropResize.cpp) needs from SumatraPDF:
// decoding an image file, writing a PDF or an encoded image, opening what was
// saved and translations. Another app fills the same hooks in with its own.
// ng: orig works on Gdiplus::Bitmap and encodes with WIC, which is Windows
// only. Here the currency is Pixmap and PNG / JPEG come out of mupdf, so the
// editor works on every platform; WebP still needs Windows.

#include "gui/GpuiBridge.h"
#include "base/File.h"
#include "base/Pixmap.h"
#if OS_WIN
#include "base/GdiPlusUtil.h"
#endif

#include "gui/UIModels.h"

#include "ImageReader.h"
#include "SumatraConfig.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "PdfCreator.h"
#include "PngOptimizer.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Translations.h"
#include "ImageSaveCropResize.h"

extern "C" {
#include "mupdf/fitz.h"
}

#include "SumatraLog.h"

ImageEditHost gImageEditHost;

static Pixmap* LoadImageFile(Str path) {
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        return nullptr;
    }
    Pixmap* px = PixmapFromData(data);
    str::Free(data);
    return px;
}

// PDF date format is "D:YYYYMMDDHHmmSSOHH'mm'" where O is the relationship of
// local time to UTC (+, - or Z). Used to stamp CreationDate/ModDate (issue #949).
static TempStr FormatPdfDateTemp() {
    i64 t = time(nullptr);
    struct tm lt{};
#if OS_WIN
    localtime_s(&lt, (const time_t*)&t);
#else
    time_t tt = (time_t)t;
    localtime_r(&tt, &lt);
#endif
    // ng: orig reads the bias out of TIME_ZONE_INFORMATION; mktime of the
    // broken-down local time against timegm-style arithmetic is portable
    struct tm gt{};
#if OS_WIN
    gmtime_s(&gt, (const time_t*)&t);
#else
    gmtime_r(&tt, &gt);
#endif
    int off = (lt.tm_hour - gt.tm_hour) * 60 + (lt.tm_min - gt.tm_min);
    int dayDiff = lt.tm_yday - gt.tm_yday;
    if (dayDiff == 1 || dayDiff < -1) {
        off += 24 * 60;
    } else if (dayDiff == -1 || dayDiff > 1) {
        off -= 24 * 60;
    }
    char sign = '+';
    if (off < 0) {
        sign = '-';
        off = -off;
    }
    int offH = off / 60;
    int offM = off % 60;
    return fmt("D:%04d%02d%02d%02d%02d%02d%c%02d'%02d'", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
               lt.tm_min, lt.tm_sec, sign, offH, offM);
}

// Create a single-page PDF from a bitmap using PdfCreator. The image goes in
// as PNG (a format PDF can wrap as-is) and is stamped with the current time as
// CreationDate/ModDate (issue #949).
static bool SavePixmapAsPdf(Pixmap* px, Str destPath) {
    if (!px || len(destPath) == 0) {
        return false;
    }
    Str png = EncodeAndOptimizePngFromPixmap(px);
    if (len(png) == 0) {
        return false;
    }
    auto* c = new PdfCreator();
    bool ok = c->AddPageFromImageData(png, 0);
    str::Free(png);
    if (ok) {
        TempStr now = FormatPdfDateTemp();
        c->SetProperty(DocProp::CreationDate, now);
        c->SetProperty(DocProp::ModificationDate, now);
        c->SetProperty(DocProp::CreatorApp, StrL("SumatraPDF"));
        ok = c->SaveToFile(destPath);
    }
    delete c;
    return ok;
}

// A 24-bit RGB fz_pixmap over our pixels, so mupdf's encoders can read them.
static fz_pixmap* NewFzPixmapFromPixmap(fz_context* ctx, Pixmap* px) {
    int w = px->width;
    int h = px->height;
    int bpp = PixmapBytesPerPixel(px->format);
    bool isRgb = px->format == PixmapFormat::RGBA8;
    fz_pixmap* fzPix = fz_new_pixmap(ctx, fz_device_rgb(ctx), w, h, nullptr, 0);
    for (int y = 0; y < h; y++) {
        const u8* src = px->data + ((size_t)y * px->stride);
        u8* dst = fzPix->samples + ((size_t)y * fzPix->stride);
        for (int x = 0; x < w; x++) {
            dst[0] = isRgb ? src[0] : src[2];
            dst[1] = src[1];
            dst[2] = isRgb ? src[2] : src[0];
            src += bpp;
            dst += 3;
        }
    }
    return fzPix;
}

static bool SavePixmapAsJpeg(Pixmap* px, Str destPath) {
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        return false;
    }
    bool ok = false;
    fz_pixmap* fzPix = nullptr;
    fz_try(ctx) {
        fzPix = NewFzPixmapFromPixmap(ctx, px);
        fz_save_pixmap_as_jpeg(ctx, fzPix, CStrTemp(destPath), 90);
        ok = true;
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, fzPix);
    }
    fz_catch(ctx) {
        logf("SavePixmapAsJpeg: %s\n", Str(fz_caught_message(ctx)));
    }
    fz_drop_context(ctx);
    return ok;
}

#if OS_WIN
// WebP has no portable encoder here; Windows 10+ adds one, which is what orig
// probes WIC for.
static WStr GdiPlusMimeForExt(Str ext) {
    if (str::EqI(ext, StrL(".webp"))) {
        return WStrL(L"image/webp");
    }
    return {};
}

static bool SavePixmapWithGdiPlus(Pixmap* px, Str destPath, Str ext) {
    WStr mime = GdiPlusMimeForExt(ext);
    if (len(mime) == 0) {
        return false;
    }
    CLSID clsid = GetGdiPlusEncoderClsid(mime);
    static const CLSID kNoEncoder{};
    if (memcmp(&clsid, &kNoEncoder, sizeof(CLSID)) == 0) {
        return false;
    }
    Gdiplus::Bitmap* bmp = NewGdiplusBitmapFromPixmap(px);
    if (!bmp) {
        return false;
    }
    Gdiplus::Status st = bmp->Save(CWStrTemp(destPath), &clsid, nullptr);
    delete bmp;
    return st == Gdiplus::Ok;
}
#endif

static bool SavePixmapAsImage(Pixmap* px, Str destPath, Str ext) {
    if (!px || len(destPath) == 0) {
        return false;
    }
    if (str::EqI(ext, StrL(".png"))) {
        Str png = EncodeAndOptimizePngFromPixmap(px);
        if (len(png) == 0) {
            return false;
        }
        bool ok = file::WriteFile(destPath, png);
        str::Free(png);
        return ok;
    }
    if (str::EqI(ext, StrL(".jpg")) || str::EqI(ext, StrL(".jpeg"))) {
        return SavePixmapAsJpeg(px, destPath);
    }
    if (str::EqI(ext, StrL(".bmp"))) {
        Str bmp = PixmapToBmpFormat(px);
        if (len(bmp) == 0) {
            return false;
        }
        bool ok = file::WriteFile(destPath, bmp);
        str::Free(bmp);
        return ok;
    }
    if (str::EqI(ext, StrL(".tif")) || str::EqI(ext, StrL(".tiff"))) {
        Str tiff = EncodeTiffFromPixmap(px);
        if (len(tiff) == 0) {
            return false;
        }
        bool ok = file::WriteFile(destPath, tiff);
        str::Free(tiff);
        return ok;
    }
    if (str::EqI(ext, StrL(".gif"))) {
        Str gif = EncodeGifFromPixmap(px);
        if (len(gif) == 0) {
            return false;
        }
        bool ok = file::WriteFile(destPath, gif);
        str::Free(gif);
        return ok;
    }
#if OS_WIN
    return SavePixmapWithGdiPlus(px, destPath, ext);
#else
    return false;
#endif
}

static bool ImageFormatAvailable(Str ext) {
    if (str::EqI(ext, StrL(".png")) || str::EqI(ext, StrL(".jpg")) || str::EqI(ext, StrL(".bmp")) ||
        str::EqI(ext, StrL(".gif")) || str::EqI(ext, StrL(".tif")) || str::EqI(ext, StrL(".tiff")) ||
        str::EqI(ext, StrL(".pdf"))) {
        return true;
    }
#if OS_WIN
    WStr mime = GdiPlusMimeForExt(ext);
    if (len(mime) == 0) {
        return false;
    }
    CLSID clsid = GetGdiPlusEncoderClsid(mime);
    static const CLSID kNoEncoder{};
    return memcmp(&clsid, &kNoEncoder, sizeof(CLSID)) != 0;
#else
    return false;
#endif
}

// ng: gpui cannot write images; Windows keeps orig's PNG + CF_DIBV5 pair.
bool ImageEditCopyToClipboard(Pixmap* px) {
    if (!px) {
        return false;
    }
#if OS_WIN
    return CopyPixmapToClipboard(px, false);
#else
    return false;
#endif
}

Pixmap* ImageEditGetClipboard(MainWindow* win) {
#if !OS_WIN
    if (!win || !win->gpuiWin) {
        return nullptr;
    }
    gp::ClipboardItem item = gp::ClipboardGetItem(gp::GetTempArena(), win->gpuiWin);
    if (!item.HasImage()) {
        return nullptr;
    }
    return PixmapFromData(Str((char*)item.imageBytes, item.imageBytesLen));
#else
    (void)win;
    return GetClipboardImageAsPixmap();
#endif
}

bool ImageEditHasClipboard(MainWindow* win) {
#if OS_WIN
    (void)win;
    return IsClipboardFormatAvailable(CF_BITMAP) != 0;
#else
    if (!win || !win->gpuiWin) {
        return false;
    }
    return gp::ClipboardGetItem(gp::GetTempArena(), win->gpuiWin).HasImage();
#endif
}

static void OpenSavedFile(MainWindow* parent, Str path) {
    MainWindow* win = IsMainWindowValidAndNotClosing(parent) ? parent : nullptr;
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    if (!win) {
        return;
    }
    LoadDocument(win, path);
}

static Str TranslateStr(Str s) {
    return Tr(s);
}

// fills the hooks above in with SumatraPDF's implementations
void InitImageEditHost() {
    gImageEditHost.LoadImageFile = LoadImageFile;
    gImageEditHost.SavePixmapAsPdf = SavePixmapAsPdf;
    gImageEditHost.SavePixmapAsImage = SavePixmapAsImage;
    gImageEditHost.ImageFormatAvailable = ImageFormatAvailable;
    gImageEditHost.OpenSavedFile = OpenSavedFile;
    gImageEditHost.Translate = TranslateStr;
    gImageEditHost.escToExit = gSettings && gSettings->escToExit;
}

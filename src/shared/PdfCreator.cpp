/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Win.h"
#include "base/GdiPlusUtil.h"

extern "C" {
#include <mupdf/pdf.h>
}

#include "gui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "Annotation.h"
#include "ImageReader.h"
#include "PdfCreator.h"

// EngineImages.cpp — avoid including EngineAll.h (needs full FileType for defaults)
Str EngineImagesGetImageData(EngineBase*, int pageNo);
extern const pdf_write_options gPdfDefaultWriteOptions;

static Str gPdfProducer;

// this name is included in all saved PDF files
void PdfCreator::SetProducerName(Str name) {
    if (!str::Eq(gPdfProducer, name)) {
        gPdfProducer = str::Dup(GetPermArena(), name);
    }
}

// Takes ownership of data through the temporary pixmap.
static fz_image* FzImageFromRgbData(fz_context* ctx, u8* data, int width, int height, int stride) {
    fz_pixmap* pixmap = nullptr;
    fz_image* image = nullptr;
    fz_var(pixmap);
    fz_var(image);
    fz_try(ctx) {
        pixmap = fz_new_pixmap_with_data(ctx, fz_device_rgb(ctx), width, height, nullptr, 0, stride, data);
        pixmap->flags |= FZ_PIXMAP_FLAG_FREE_SAMPLES;
        image = fz_new_image_from_pixmap(ctx, pixmap, nullptr);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        fz_rethrow(ctx);
    }
    return image;
}

// TODO: the resulting pdf is big, even though we tell it to compress images
// maybe encode bitmaps to *.png or .jp2 and use AddPageFromImageData
#if OS_WIN
static fz_image* render_to_pixmap(fz_context* ctx, HBITMAP hbmp, Size size) {
    int w = size.dx;
    int h = size.dy;
    int stride = (((w * 3) + 3) / 4) * 4;

    size_t totalSize = (size_t)stride * (size_t)h;
    u8* data = (u8*)fz_malloc(ctx, totalSize);
    if (!data) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "render_to_pixmap: failed to allocate %d bytes", stride * h);
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hDC = GetDC(nullptr);
    int res = GetDIBits(hDC, hbmp, 0, h, data, &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hDC);
    if (res == 0) {
        fz_free(ctx, data);
        fz_throw(ctx, FZ_ERROR_GENERIC, "GetDIBits failed");
    }

    // convert BGR to RGB without padding (fz_new_pixmap_with_data handles stride)
    u8 r, b;
    for (int y = 0; y < h; y++) {
        u8* d = data + ((size_t)y * stride);
        for (int x = 0; x < w; x++) {
            b = d[0];
            // gree in the middle, stays in place
            r = d[2];
            d[0] = r;
            // gree in the middle, stays in place
            d[2] = b;
            d += 3;
        }
    }

    return FzImageFromRgbData(ctx, data, w, h, stride);
}

#endif

// ng: the portable path into mupdf: read the Pixmap's own pixels instead of
// asking GDI for the bits of a DIB. A Native pixmap has no readable layout,
// so only the Windows path above can describe one.
static fz_image* fz_image_from_pixmap(fz_context* ctx, const Pixmap* px) {
    int w = px->width;
    int h = px->height;
    int bpp = PixmapBytesPerPixel(px->format);
    bool isRgb = px->format == PixmapFormat::RGBA8;
    int stride = w * 3;

    u8* data = (u8*)fz_malloc(ctx, (size_t)stride * (size_t)h);
    for (int y = 0; y < h; y++) {
        const u8* src = px->data + ((size_t)y * (size_t)px->stride);
        u8* dst = data + ((size_t)y * (size_t)stride);
        for (int x = 0; x < w; x++) {
            dst[0] = isRgb ? src[0] : src[2];
            dst[1] = src[1];
            dst[2] = isRgb ? src[2] : src[0];
            src += bpp;
            dst += 3;
        }
    }

    return FzImageFromRgbData(ctx, data, w, h, stride);
}

PdfCreator::PdfCreator() {
    // fz_new_context_windows() routes mupdf warnings / errors to log()
    ctx = fz_new_context_windows(kFzStoreUnlimited);
    if (!ctx) {
        return;
    }

    fz_try(ctx) {
        doc = pdf_create_document(ctx);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        doc = nullptr;
    }
}

PdfCreator::~PdfCreator() {
    pdf_drop_document(ctx, doc);
    fz_flush_warnings(ctx);
    fz_drop_context_windows(ctx);
}

// based on create_page in pdfcreate.c
bool PdfCreator::AddPageFromFzImage(fz_image* image, float imgDpi) const {
    ReportIf(!ctx || !doc);
    if (!ctx || !doc) {
        return false;
    }

    pdf_obj* resources = nullptr;
    fz_buffer* contents = nullptr;
    fz_device* dev = nullptr;

    fz_var(contents);
    fz_var(resources);
    fz_var(dev);

    bool ok = true;
    fz_var(ok);
    fz_try(ctx) {
        float zoom = 1.0f;
        if (imgDpi > 0) {
            zoom = 72.0f / imgDpi;
        }
        fz_matrix ctm = {(float)image->w * zoom, 0, 0, (float)image->h * zoom, 0, 0};
        fz_rect bounds = fz_unit_rect;
        bounds = fz_transform_rect(bounds, ctm);

        dev = pdf_page_write(ctx, doc, bounds, &resources, &contents);
        fz_fill_image(ctx, dev, image, ctm, 1.0f, fz_default_color_params);
        fz_drop_device(ctx, dev);
        dev = nullptr;

        pdf_obj* page = pdf_add_page(ctx, doc, bounds, 0, resources, contents);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
    }
    fz_always(ctx) {
        pdf_drop_obj(ctx, resources);
        fz_drop_buffer(ctx, contents);
        fz_drop_device(ctx, dev);
    }
    fz_catch(ctx) {
        ok = false;
        fz_report_error(ctx);
    }
    return ok;
}

#if OS_WIN
static bool AddPageFromHBITMAP(PdfCreator* c, HBITMAP hbmp, Size size, float imgDpi) {
    if (!c->ctx || !c->doc) {
        return false;
    }

    bool ok = false;
    fz_var(ok);
    fz_try(c->ctx) {
        fz_image* image = render_to_pixmap(c->ctx, hbmp, size);
        ok = c->AddPageFromFzImage(image, imgDpi);
        fz_drop_image(c->ctx, image);
    }
    fz_catch(c->ctx) {
        fz_report_error(c->ctx);
        return false;
    }
    return ok;
}

bool PdfCreator::AddPageFromGdiplusBitmap(Gdiplus::Bitmap* bmp, float imgDpi) {
    HBITMAP hbmp;
    if (bmp->GetHBITMAP((Gdiplus::ARGB)Gdiplus::Color::White, &hbmp) != Gdiplus::Ok) {
        return false;
    }
    if (!(bool)imgDpi) {
        imgDpi = bmp->GetHorizontalResolution();
    }
    bool ok = AddPageFromHBITMAP(this, hbmp, Size((int)bmp->GetWidth(), (int)bmp->GetHeight()), imgDpi);
    DeleteObject(hbmp);
    return ok;
}
#endif

// One page from a rendered Pixmap: the DIB it is backed by on Windows (which
// is the only way to read a Native one), its pixels everywhere else.
static bool AddPageFromPixmap(PdfCreator* c, Pixmap* px, float imgDpi) {
    if (!c->ctx || !c->doc || !px) {
        return false;
    }
#if OS_WIN
    if (px->hbmp) {
        return AddPageFromHBITMAP(c, px->hbmp, Size(px->width, px->height), imgDpi);
    }
#endif
    if (px->format == PixmapFormat::Native || !px->data) {
        return false;
    }
    bool ok = false;
    fz_var(ok);
    fz_try(c->ctx) {
        fz_image* image = fz_image_from_pixmap(c->ctx, px);
        ok = c->AddPageFromFzImage(image, imgDpi);
        fz_drop_image(c->ctx, image);
    }
    fz_catch(c->ctx) {
        fz_report_error(c->ctx);
        return false;
    }
    return ok;
}

bool PdfCreator::AddPageFromImageData(Str data, float imgDpi) const {
    ReportIf(!ctx || !doc);
    if (!ctx || !doc || len(data) == 0) {
        return false;
    }

    fz_image* img = nullptr;
    fz_var(img);

    fz_try(ctx) {
        fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (u8*)data.s, (size_t)data.len);
        img = fz_new_image_from_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        img = nullptr;
    }
    if (!img) {
        return false;
    }
    bool ok = AddPageFromFzImage(img, imgDpi);
    fz_drop_image(ctx, img);
    return ok;
}

bool PdfCreator::SetProperty(DocProp prop, Str value) const {
    if (!ctx || !doc) {
        return false;
    }

    Str name = PdfInfoKeyFromProp(prop);
    if (len(name) == 0) {
        return false;
    }

    fz_try(ctx) {
        pdf_obj* info = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info));
        if (!info) {
            info = pdf_new_dict(ctx, doc, 8);
            pdf_dict_put(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info), info);
            pdf_drop_obj(ctx, info);
        }

        // TODO: not sure if pdf_new_text_string() handles utf8
        pdf_obj* valobj = pdf_new_text_string(ctx, CStrTemp(value));
        pdf_dict_puts_drop(ctx, info, name.s, valobj);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return false;
    }
    return true;
}

// clang-format off
static const DocProp propsToCopy[] = {
    DocProp::Title,
    DocProp::Author,
    DocProp::Subject,
    DocProp::Copyright,
    DocProp::ModificationDate,
    DocProp::CreatorApp,
};
// clang-format on

bool PdfCreator::CopyProperties(EngineBase* engine) const {
    bool ok;
    for (DocProp prop : propsToCopy) {
        TempStr value = engine->GetPropertyTemp(prop);
        if (value) {
            ok = SetProperty(prop, value);
            if (!ok) {
                return false;
            }
        }
    }
    return true;
}

bool PdfCreator::SaveToFile(Str filePath) const {
    if (!ctx || !doc) {
        return false;
    }

    if (gPdfProducer) {
        SetProperty(DocProp::PdfProducer, gPdfProducer);
    }

    fz_try(ctx) {
        pdf_write_options opts = gPdfDefaultWriteOptions;
        opts.do_compress = 1;
        opts.do_compress_images = 1;
        pdf_save_document(ctx, doc, CStrTemp(filePath), &opts);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return false;
    }
    return true;
}

// creates a simple PDF with all pages rendered as a single image
bool PdfCreator::RenderToFile(Str pdfFileName, EngineBase* engine, int dpi) {
    EnsureFullLayout(engine);
    PdfCreator* c = new PdfCreator();
    bool ok = true;
    // render all pages to images
    float zoom = (float)dpi / engine->fileDPI;
    for (int i = 1; ok && i <= engine->PageCount(); i++) {
        RenderPageArgs args(i, zoom, 0, nullptr, RenderTarget::Export);
        Pixmap* bmp = engine->RenderPage(args);
        ok = AddPageFromPixmap(c, bmp, (float)dpi);
        FreePixmap(bmp);
    }
    if (!ok) {
        delete c;
        return false;
    }
    c->CopyProperties(engine);
    ok = c->SaveToFile(pdfFileName);
    delete c;
    return ok;
}

// Comic book / image folder / multi-page image → multi-page PDF (issue #4118).
// 1) Embed original bytes when MuPDF can re-wrap them (JPEG, PNG, …).
// 2) Else optional fallbackToEmbeddable (e.g. decode → optimized PNG).
// 3) Else render the page at native resolution.
// Pages that fail every path are skipped.
bool PdfCreator::SaveImageCollectionAsPdf(Str pdfFileName, EngineBase* engine,
                                          ImageDataFallbackFn fallbackToEmbeddable) {
    if (!engine || !engine->isImageCollection || engine->PageCount() <= 0) {
        return false;
    }
    EnsureFullLayout(engine);

    PdfCreator* c = new PdfCreator();
    if (!c->ctx || !c->doc) {
        delete c;
        return false;
    }

    float dpi = engine->fileDPI;
    if (dpi <= 0) {
        dpi = 96.0f;
    }

    int pagesAdded = 0;
    int nPages = engine->PageCount();
    for (int i = 1; i <= nPages; i++) {
        bool pageOk = false;

        Str data = EngineImagesGetImageData(engine, i);
        // One file holds every frame (GIF/TIFF/ICO). Embedding that blob on
        // each PDF page makes MuPDF show the last frame (issue #1930).
        if (engine->kind == kindEngineImage && nPages > 1) {
            data = {};
        }
        if (len(data) > 0) {
            pageOk = c->AddPageFromImageData(data, dpi);
            if (!pageOk && fallbackToEmbeddable) {
                // WebP, JXL, HEIC, AVIF, TGA, … — convert to something PDF can store.
                Str converted = fallbackToEmbeddable(data);
                if (len(converted) > 0) {
                    pageOk = c->AddPageFromImageData(converted, dpi);
                }
                str::Free(converted);
            }
        }

        if (!pageOk) {
            // Last resort: render at native resolution (zoom 1.0 relative to file DPI).
            RenderPageArgs args(i, 1.0f, 0, nullptr, RenderTarget::Export);
            Pixmap* bmp = engine->RenderPage(args);
            pageOk = AddPageFromPixmap(c, bmp, dpi);
#if OS_WIN
            if (!pageOk && bmp) {
                Gdiplus::Bitmap* gp = WrapPixmapGdiplus(bmp);
                if (gp) {
                    pageOk = c->AddPageFromGdiplusBitmap(gp, dpi);
                    delete gp;
                }
            }
#endif
            FreePixmap(bmp);
        }

        if (pageOk) {
            pagesAdded++;
        } else {
            logf("PdfCreator::SaveImageCollectionAsPdf: skipped page %d (embed+convert+render failed)\n", i);
        }
    }

    if (pagesAdded == 0) {
        delete c;
        return false;
    }

    c->CopyProperties(engine);
    bool ok = c->SaveToFile(pdfFileName);
    delete c;
    return ok;
}

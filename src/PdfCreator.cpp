/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/GdiplusUtil.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"
#include "PdfCreator.h"

using namespace Gdiplus;

static AutoFreeWstr gPdfProducer;

void PdfCreator::SetProducerName(const WCHAR* name) {
    if (!str::Eq(gPdfProducer, name)) {
        gPdfProducer.SetCopy(name);
    }
}

// TODO: in 3.1.2 we had grayscale optimization, not sure if worth it
// TODO: the resulting pdf is big, even though we tell it to compress images
// mabye encode bitmaps to *.png or .jp2 and use AddPageFromImageData
static fz_image* render_to_pixmap(fz_context* ctx, HBITMAP hbmp, SizeI size) {
    int w = size.dx;
    int h = size.dy;
    int stride = ((w * 3 + 3) / 4) * 4;

    unsigned char* data = (unsigned char*)fz_malloc(ctx, stride * h);
    if (!data) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "render_to_pixmap: failed to allocate %d bytes", (int)stride * h);
    }

    BITMAPINFO bmi = {0};
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

    // convert BGR to RGB without padding
    // Note: in 3.1.2 it was also moving data but maybe not necessary because
    // fz_new_pixmap_with_data() understands stride?
    u8 r, b;
    for (int y = 0; y < h; y++) {
        u8* d = data + y * stride;
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

    fz_color_params cp = fz_default_color_params;
    fz_colorspace* cs = fz_device_rgb(ctx);
    fz_image* img = nullptr;
    fz_var(img);

    fz_try(ctx) {
        fz_pixmap* pix = fz_new_pixmap_with_data(ctx, cs, w, h, nullptr, 0, stride, data);
        pix->flags |= FZ_PIXMAP_FLAG_FREE_SAMPLES;
        img = fz_new_image_from_pixmap(ctx, pix, nullptr);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return img;
}

static void fz_print_cb(void* user, const char* msg) {
    log(msg);
}

static void installFitzErrorCallbacks(fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, nullptr);
    fz_set_error_callback(ctx, fz_print_cb, nullptr);
}

PdfCreator::PdfCreator() {
    ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!ctx) {
        return;
    }

    installFitzErrorCallbacks(ctx);
    fz_try(ctx) {
        doc = pdf_create_document(ctx);
    }
    fz_catch(ctx) {
        doc = nullptr;
    }
}

PdfCreator::~PdfCreator() {
    pdf_drop_document(ctx, doc);
    fz_flush_warnings(ctx);
    fz_drop_context(ctx);
}

pdf_obj* add_image_res(fz_context* ctx, pdf_document* doc, pdf_obj* resources, char* name, fz_image* image) {
    pdf_obj *subres, *ref;

    subres = pdf_dict_get(ctx, resources, PDF_NAME(XObject));
    if (!subres) {
        subres = pdf_new_dict(ctx, doc, 10);
        pdf_dict_put_drop(ctx, resources, PDF_NAME(XObject), subres);
    }

    ref = pdf_add_image(ctx, doc, image);
    pdf_dict_puts(ctx, subres, name, ref);
    pdf_drop_obj(ctx, ref);
    return ref;
}

// based on create_page in pdfcreate.c
bool PdfCreator::AddPageFromFzImage(fz_image* image, float imgDpi) {
    CrashIf(!ctx || !doc);
    if (!ctx || !doc) {
        return false;
    }

    pdf_obj* resources = nullptr;
    fz_buffer* contents = nullptr;
    fz_device* dev = nullptr;

    fz_var(contents);
    fz_var(resources);
    fz_var(dev);

    fz_try(ctx) {
        float zoom = 1.0f;
        if (imgDpi > 0) {
            zoom = 72.0f / imgDpi;
        }
        fz_matrix ctm = {image->w * zoom, 0, 0, image->h * zoom, 0, 0};
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
        return false;
    }
    return true;
}

static bool AddPageFromHBITMAP(PdfCreator* c, HBITMAP hbmp, SizeI size, float imgDpi) {
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
        return false;
    }
    return ok;
}

bool PdfCreator::AddPageFromGdiplusBitmap(Gdiplus::Bitmap* bmp, float imgDpi) {
    HBITMAP hbmp;
    if (bmp->GetHBITMAP((Gdiplus::ARGB)Gdiplus::Color::White, &hbmp) != Gdiplus::Ok) {
        return false;
    }
    if (!imgDpi) {
        imgDpi = bmp->GetHorizontalResolution();
    }
    bool ok = AddPageFromHBITMAP(this, hbmp, SizeI(bmp->GetWidth(), bmp->GetHeight()), imgDpi);
    DeleteObject(hbmp);
    return ok;
}

bool PdfCreator::AddPageFromImageData(const char* data, size_t len, float imgDpi) {
    CrashIf(!ctx || !doc);
    if (!ctx || !doc || !data || len == 0) {
        return false;
    }

    fz_image* img = nullptr;
    fz_var(img);

    fz_try(ctx) {
        fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (u8*)data, len);
        img = fz_new_image_from_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        return false;
    }
    if (!img) {
        return false;
    }
    bool ok = AddPageFromFzImage(img, imgDpi);
    fz_drop_image(ctx, img);
    return ok;
}

bool PdfCreator::SetProperty(DocumentProperty prop, const WCHAR* value) {
    if (!ctx || !doc) {
        return false;
    }

    // adapted from EnginePdf::GetProperty
    static struct {
        DocumentProperty prop;
        char* name;
    } pdfPropNames[] = {
        {DocumentProperty::Title, "Title"},
        {DocumentProperty::Author, "Author"},
        {DocumentProperty::Subject, "Subject"},
        {DocumentProperty::Copyright, "Copyright"},
        {DocumentProperty::ModificationDate, "ModDate"},
        {DocumentProperty::CreatorApp, "Creator"},
        {DocumentProperty::PdfProducer, "Producer"},
    };
    const char* name = nullptr;
    for (int i = 0; i < dimof(pdfPropNames) && !name; i++) {
        if (pdfPropNames[i].prop == prop) {
            name = pdfPropNames[i].name;
        }
    }
    if (!name) {
        return false;
    }

    AutoFree val = strconv::WstrToUtf8(value);

    pdf_obj* obj = nullptr;
    fz_var(obj);
    fz_try(ctx) {
        pdf_obj* info = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info));
        if (!info) {
            info = pdf_new_dict(ctx, doc, 8);
            pdf_dict_put(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info), info);
            pdf_drop_obj(ctx, info);
        }

        // TODO: not sure if pdf_new_text_string() handles utf8
        pdf_obj* valobj = pdf_new_text_string(ctx, val.get());
        pdf_dict_puts_drop(ctx, info, name, valobj);
    }
    fz_catch(ctx) {
        pdf_drop_obj(ctx, obj);
        return false;
    }
    return true;
}

// clang-format off
static DocumentProperty propsToCopy[] = {
    DocumentProperty::Title,
    DocumentProperty::Author,
    DocumentProperty::Subject,
    DocumentProperty::Copyright,
    DocumentProperty::ModificationDate,
    DocumentProperty::CreatorApp
};
// clang-format on

bool PdfCreator::CopyProperties(EngineBase* engine) {
    bool ok = true;
    for (int i = 0; i < dimof(propsToCopy); i++) {
        AutoFreeWstr value = engine->GetProperty(propsToCopy[i]);
        if (value) {
            ok = SetProperty(propsToCopy[i], value);
            if (!ok) {
                return false;
            }
        }
    }
    return true;
}

const pdf_write_options pdf_default_write_options2 = {
    0,  /* do_incremental */
    0,  /* do_pretty */
    0,  /* do_ascii */
    1,  /* do_compress */
    1,  /* do_compress_images */
    0,  /* do_compress_fonts */
    0,  /* do_decompress */
    0,  /* do_garbage */
    0,  /* do_linear */
    0,  /* do_clean */
    0,  /* do_sanitize */
    0,  /* do_appearance */
    0,  /* do_encrypt */
    ~0, /* permissions */
    "", /* opwd_utf8[128] */
    "", /* upwd_utf8[128] */
};

bool PdfCreator::SaveToFile(const char* filePath) {
    if (!ctx || !doc)
        return false;

    if (gPdfProducer) {
        SetProperty(DocumentProperty::PdfProducer, gPdfProducer);
    }

    fz_try(ctx) {
        pdf_write_options opts = pdf_default_write_options2;
        opts.do_compress = 1;
        opts.do_compress_images = 1;
        pdf_save_document(ctx, doc, (const char*)filePath, &opts);
    }
    fz_catch(ctx) {
        return false;
    }
    return true;
}

bool PdfCreator::RenderToFile(const char* pdfFileName, EngineBase* engine, int dpi) {
    PdfCreator* c = new PdfCreator();
    bool ok = true;
    // render all pages to images
    float zoom = dpi / engine->GetFileDPI();
    for (int i = 1; ok && i <= engine->PageCount(); i++) {
        RenderPageArgs args(i, zoom, 0, nullptr, RenderTarget::Export);
        RenderedBitmap* bmp = engine->RenderPage(args);
        ok = false;
        if (bmp) {
            ok = AddPageFromHBITMAP(c, bmp->GetBitmap(), bmp->Size(), dpi);
        }
        delete bmp;
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

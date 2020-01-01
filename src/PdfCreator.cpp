/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/GdiplusUtil.h"

#include "TreeModel.h"
#include "EngineBase.h"
#include "PdfCreator.h"

using namespace Gdiplus;

static AutoFreeWstr gPdfProducer;

void PdfCreator::SetProducerName(const WCHAR* name) {
    if (!str::Eq(gPdfProducer, name))
        gPdfProducer.SetCopy(name);
}

static fz_image* render_to_pixmap(fz_context* ctx, HBITMAP hbmp, SizeI size) {
#if 1
    CrashMePort();
    // TODO(port): fix me
    return nullptr;
#else
    int w = size.dx, h = size.dy;
    int stride = ((w * 3 + 3) / 4) * 4;

    unsigned char* data = (unsigned char*)fz_malloc(ctx, stride * h);

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
    if (!res) {
        fz_free(ctx, data);
        fz_throw(ctx, FZ_ERROR_GENERIC, "GetDIBits failed");
    }

    // convert BGR with padding to RGB without padding
    unsigned char* out = data;
    bool is_grayscale = true;
    for (int y = 0; y < h; y++) {
        const unsigned char* in = data + y * stride;
        unsigned char green, blue;
        for (int x = 0; x < w; x++) {
            is_grayscale = is_grayscale && in[0] == in[1] && in[0] == in[2];
            blue = *in++;
            green = *in++;
            *out++ = *in++;
            *out++ = green;
            *out++ = blue;
        }
    }
    // convert grayscale RGB to proper grayscale
    if (is_grayscale) {
        const unsigned char* in = out = data;
        for (int i = 0; i < w * h; i++) {
            *out++ = *in++;
            in += 2;
        }
    }

    fz_compressed_buffer* buf = nullptr;
    fz_var(buf);

    fz_try(ctx) {
        buf = fz_malloc_struct(ctx, fz_compressed_buffer);
        buf->buffer = fz_new_buffer(ctx, w * h * 4 + 10);
        buf->params.type = FZ_IMAGE_FLATE;
        buf->params.u.flate.predictor = 1;

        z_stream zstm = {0};
        zstm.next_in = data;
        zstm.avail_in = out - data;
        zstm.next_out = buf->buffer->data;
        zstm.avail_out = buf->buffer->cap;

        res = deflateInit(&zstm, 9);
        if (res != Z_OK)
            fz_throw(ctx, FZ_ERROR_GENERIC, "deflate failure %d", res);
        res = deflate(&zstm, Z_FINISH);
        if (res != Z_STREAM_END)
            fz_throw(ctx, FZ_ERROR_GENERIC, "deflate failure %d", res);
        buf->buffer->len = zstm.total_out;
        res = deflateEnd(&zstm);
        if (res != Z_OK)
            fz_throw(ctx, FZ_ERROR_GENERIC, "deflate failure %d", res);
    }
    fz_always(ctx) {
        fz_free(ctx, data);
    }
    fz_catch(ctx) {
        fz_drop_compressed_buffer(ctx, buf);
        fz_rethrow(ctx);
    }

    fz_colorspace* cs = is_grayscale ? fz_device_gray(ctx) : fz_device_rgb(ctx);
    return fz_new_image(ctx, w, h, 8, cs, 96, 96, 0, 0, nullptr, nullptr, buf, nullptr);
#endif
}

PdfCreator::PdfCreator() {
    ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!ctx)
        return;
    fz_try(ctx) {
        doc = pdf_create_document(ctx);
    }
    fz_catch(ctx) {
        doc = nullptr;
    }
}

PdfCreator::~PdfCreator() {
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
}

static void add_image_res(fz_context* ctx, pdf_document* doc, pdf_obj* resources, char* name, fz_image* image) {
    pdf_obj *subres, *ref;

    subres = pdf_dict_get(ctx, resources, PDF_NAME(XObject));
    if (!subres) {
        subres = pdf_new_dict(ctx, doc, 10);
        pdf_dict_put_drop(ctx, resources, PDF_NAME(XObject), subres);
    }

    ref = pdf_add_image(ctx, doc, image);
    pdf_dict_puts(ctx, subres, name, ref);
    pdf_drop_obj(ctx, ref);

    fz_drop_image(ctx, image);
}

bool PdfCreator::AddPageFromFzImage(fz_image* image, float imgDpi) {
    CrashIf(!ctx || !doc);
    if (!ctx || !doc) {
        return false;
    }

    pdf_obj* page = nullptr;
    pdf_obj* resources = nullptr;

    fz_var(page);

    fz_rect mediabox = {0, 0, 595, 842};

    fz_try(ctx) {
        resources = pdf_new_dict(ctx, doc, 2);
        add_image_res(ctx, doc, resources, "image", image);
        page = pdf_add_page(ctx, doc, mediabox, 0, resources, nullptr);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
        pdf_drop_obj(ctx, resources);
    }
    fz_always(ctx) {
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

    fz_image* image = nullptr;
    fz_try(c->ctx) {
        image = render_to_pixmap(c->ctx, hbmp, size);
    }
    fz_catch(c->ctx) {
        return false;
    }
    bool ok = c->AddPageFromFzImage(image, imgDpi);
    fz_drop_image(c->ctx, image);
    return ok;
}

bool PdfCreator::AddImagePage(Gdiplus::Bitmap* bmp, float imgDpi) {
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
        fz_buffer* buf = fz_new_buffer_from_data(ctx, (u8*)data, len);
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

    // adapted from PdfEngineImpl::GetProperty
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

bool PdfCreator::SaveToFile(const char* filePath) {
    if (!ctx || !doc)
        return false;

    if (gPdfProducer) {
        SetProperty(DocumentProperty::PdfProducer, gPdfProducer);
    }

    fz_try(ctx) {
        pdf_save_document(ctx, doc, const_cast<char*>(filePath), nullptr);
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
        RenderedBitmap* bmp = engine->RenderBitmap(i, zoom, 0, nullptr, RenderTarget::Export);
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

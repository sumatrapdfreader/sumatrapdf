/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// interaction between '_setjmp' and C++ object destruction is non-portable
#pragma warning(disable: 4611)

extern "C" {
#include <mupdf/pdf.h>
}

#include "BaseUtil.h"
#include "PdfCreator.h"

#include "BaseEngine.h"
using namespace Gdiplus;
#include "GdiplusUtil.h"

#include <zlib.h>

static fz_image *render_to_pixmap(fz_context *ctx, HBITMAP hbmp, SizeI size)
{
    int w = size.dx, h = size.dy;
    int stride = ((w * 3 + 3) / 4) * 4;

    unsigned char *data = (unsigned char *)fz_malloc(ctx, stride * h);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hDC = GetDC(NULL);
    int res = GetDIBits(hDC, hbmp, 0, h, data, &bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hDC);
    if (!res) {
        fz_free(ctx, data);
        fz_throw(ctx, FZ_ERROR_GENERIC, "GetDIBits failed");
    }

    // convert BGR with padding to RGB without padding
    unsigned char *out = data;
    for (int y = 0; y < h; y++) {
        unsigned char *in = data + y * stride;
        unsigned char rgb[3];
        for (int x = 0; x < w; x++) {
            rgb[2] = *in++;
            rgb[1] = *in++;
            *out++ = *in++;
            *out++ = rgb[1];
            *out++ = rgb[2];
        }
    }

    fz_compressed_buffer *buf = NULL;
    fz_var(buf);

    fz_try(ctx) {
        buf = fz_malloc_struct(ctx, fz_compressed_buffer);
        buf->buffer = fz_new_buffer(ctx, w * h * 4 + 10);
        buf->params.type = FZ_IMAGE_FLATE;
        buf->params.u.flate.predictor = 1;

        z_stream zstm = { 0 };
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
        fz_free_compressed_buffer(ctx, buf);
        fz_rethrow(ctx);
    }

    return fz_new_image(ctx, w, h, 8, fz_device_rgb(ctx), 96, 96, 0, 0, NULL, NULL, buf, NULL);
}

static fz_image *pack_jpeg(fz_context *ctx, const char *data, size_t len, SizeI size)
{
    fz_compressed_buffer *buf = NULL;
    fz_var(buf);

    fz_try(ctx) {
        buf = fz_malloc_struct(ctx, fz_compressed_buffer);
        buf->buffer = fz_new_buffer(ctx, (int)len);
        memcpy(buf->buffer->data, data, (buf->buffer->len = (int)len));
        buf->params.type = FZ_IMAGE_JPEG;
        buf->params.u.jpeg.color_transform = -1;
    }
    fz_catch(ctx) {
        fz_free_compressed_buffer(ctx, buf);
        fz_rethrow(ctx);
    }

    return fz_new_image(ctx, size.dx, size.dy, 8, fz_device_rgb(ctx), 96, 96, 0, 0, NULL, NULL, buf, NULL);
}

static fz_image *pack_jp2(fz_context *ctx, const char *data, size_t len, SizeI size)
{
    fz_compressed_buffer *buf = NULL;
    fz_var(buf);

    fz_try(ctx) {
        buf = fz_malloc_struct(ctx, fz_compressed_buffer);
        buf->buffer = fz_new_buffer(ctx, (int)len);
        memcpy(buf->buffer->data, data, (buf->buffer->len = (int)len));
        buf->params.type = FZ_IMAGE_JPX;
    }
    fz_catch(ctx) {
        fz_free_compressed_buffer(ctx, buf);
        fz_rethrow(ctx);
    }

    return fz_new_image(ctx, size.dx, size.dy, 8, fz_device_rgb(ctx), 96, 96, 0, 0, NULL, NULL, buf, NULL);
}

PdfCreator::PdfCreator(int dpi) : dpi(dpi)
{
    ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    if (!ctx)
        return;
    fz_try(ctx) {
        doc = pdf_create_document(ctx);
    }
    fz_catch(ctx) {
        doc = NULL;
    }
}

PdfCreator::~PdfCreator()
{
    pdf_close_document(doc);
    fz_free_context(ctx);
}

bool PdfCreator::AddImagePage(fz_image *image, float imgDpi)
{
    pdf_page *page = NULL;
    fz_device *dev = NULL;
    fz_var(page);
    fz_var(dev);

    fz_try(ctx) {
        float zoom = imgDpi ? dpi / imgDpi : 1.0f;
        fz_matrix ctm = { image->w / zoom, 0, 0, image->h / zoom, 0, 0 };
        fz_rect bounds = fz_unit_rect;
        fz_transform_rect(&bounds, &ctm);
        page = pdf_create_page(doc, bounds, dpi, 0);
        dev = pdf_page_write(doc, page);
        fz_fill_image(dev, image, &ctm, 1.0);
        fz_free_device(dev);
        dev = NULL;
        pdf_insert_page(doc, page, INT_MAX);
    }
    fz_always(ctx) {
        fz_free_device(dev);
        pdf_free_page(doc, page);
    }
    fz_catch(ctx) {
        return false;
    }
    return true;
}

bool PdfCreator::AddImagePage(HBITMAP hbmp, SizeI size, float imgDpi)
{
    fz_image *image = NULL;
    fz_try(ctx) {
        image = render_to_pixmap(ctx, hbmp, size);
    }
    fz_catch(ctx) {
        return false;
    }
    bool ok = AddImagePage(image, imgDpi);
    fz_drop_image(ctx, image);
    return ok;
}

bool PdfCreator::AddImagePage(Bitmap *bmp, float imgDpi)
{
    HBITMAP hbmp;
    if (bmp->GetHBITMAP((ARGB)Color::White, &hbmp) != Ok)
        return false;
    if (!imgDpi)
        imgDpi = bmp->GetHorizontalResolution();
    bool ok = AddImagePage(hbmp, SizeI(bmp->GetWidth(), bmp->GetHeight()), imgDpi);
    DeleteObject(hbmp);
    return ok;
}

bool PdfCreator::AddImagePage(const char *data, size_t len, float imgDpi)
{
    const WCHAR *ext = GfxFileExtFromData(data, len);
    if (str::Eq(ext, L".jpg") || str::Eq(ext, L".jp2")) {
        Size size = BitmapSizeFromData(data, len);
        fz_image *image = NULL;
        fz_try(ctx) {
            image = (str::Eq(ext, L".jpg") ? pack_jpeg : pack_jp2)(ctx, data, len, SizeI(size.Width, size.Height));
        }
        fz_catch(ctx) {
            return false;
        }
        bool ok = AddImagePage(image, imgDpi);
        fz_drop_image(ctx, image);
        return ok;
    }
    Bitmap *bmp = BitmapFromData(data, len);
    if (!bmp)
        return false;
    bool ok = AddImagePage(bmp, imgDpi);
    delete bmp;
    return ok;
}

bool PdfCreator::AddRenderedPage(BaseEngine *engine, int pageNo)
{
    float zoom = dpi / engine->GetFileDPI();
    RenderedBitmap *bmp = engine->RenderBitmap(pageNo, zoom, 0, NULL, Target_Export);
    if (!bmp)
        return false;
    bool ok = AddImagePage(bmp->GetBitmap(), bmp->Size(), engine->GetFileDPI());
    delete bmp;
    return ok;
}

bool PdfCreator::SetProperty(DocumentProperty prop, const WCHAR *value)
{
    // adapted from PdfEngineImpl::GetProperty
    static struct {
        DocumentProperty prop;
        char *name;
    } pdfPropNames[] = {
        { Prop_Title, "Title" }, { Prop_Author, "Author" },
        { Prop_Subject, "Subject" }, { Prop_Copyright, "Copyright" },
        { Prop_ModificationDate, "ModDate" },
        { Prop_CreatorApp, "Creator" }, { Prop_PdfProducer, "Producer" },
    };
    const char *name = NULL;
    for (int i = 0; i < dimof(pdfPropNames) && !name; i++) {
        if (pdfPropNames[i].prop == prop)
            name = pdfPropNames[i].name;
    }
    if (!name)
        return false;

    // TODO: proper encoding?
    ScopedMem<char> valueUtf8(str::conv::ToUtf8(value));
    pdf_obj *obj = NULL;
    fz_var(obj);
    fz_try(ctx) {
        pdf_obj *info = pdf_dict_getp(pdf_trailer(doc), "Info");
        if (!pdf_is_indirect(info) || !pdf_is_dict(info)) {
            info = obj = pdf_new_dict(doc, 4);
            pdf_dict_puts_drop(pdf_trailer(doc), "Info", pdf_new_ref(doc, obj));
            pdf_drop_obj(obj);
        }
        pdf_dict_puts_drop(info, name, pdf_new_string(doc, valueUtf8, (int)str::Len(valueUtf8)));
    }
    fz_catch(ctx) {
        pdf_drop_obj(obj);
        return false;
    }
    return true;
}

bool PdfCreator::SaveToFile(const WCHAR *filePath)
{
    ScopedMem<char> pathUtf8(str::conv::ToUtf8(filePath));
    fz_try(ctx) {
        pdf_write_document(doc, pathUtf8, NULL);
    }
    fz_catch(ctx) {
        return false;
    }
    return true;
}

PdfCreator *PdfCreator::Create(int dpi)
{
    PdfCreator *c = new PdfCreator(dpi);
    if (!c->ctx || !c->doc) {
        delete c;
        return NULL;
    }
    return c;
}

bool RenderToPDF(const WCHAR *pdfFileName, BaseEngine *engine, int dpi)
{
    PdfCreator *c = PdfCreator::Create(dpi);
    if (!c)
        return false;
    bool ok = true;
    // render all pages to images
    for (int i = 1; ok && i <= engine->PageCount(); i++) {
        ok = c->AddRenderedPage(engine, i);
    }
    if (ok) {
        // copy document properties
        static DocumentProperty props[] = { Prop_Title, Prop_Author, Prop_Subject, Prop_Copyright, Prop_ModificationDate, Prop_CreatorApp };
        for (int i = 0; i < dimof(props); i++) {
            ScopedMem<WCHAR> value(engine->GetProperty(props[i]));
            if (value)
                c->SetProperty(props[i], value);
        }
        c->SetProperty(Prop_PdfProducer, L"SumatraPDF's RenderToPDF");
        ok = c->SaveToFile(pdfFileName);
    }
    delete c;
    return ok;
}

/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// interaction between '_setjmp' and C++ object destruction is non-portable
#pragma warning(disable: 4611)

// utils
#include "BaseUtil.h"
// must be included after "BaseUtil.h" to avoid redefining
// _HAS_EXCEPTIONS in VS 2015 
extern "C" {
#include <mupdf/pdf.h>
#include <zlib.h>
}
#include "GdiplusUtil.h"
// rendering engines
#include "BaseEngine.h"
#include "PdfCreator.h"

static ScopedMem<WCHAR> gPdfProducer;

void PdfCreator::SetProducerName(const WCHAR *name)
{
    if (!str::Eq(gPdfProducer, name))
        gPdfProducer.Set(str::Dup(name));
}

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

    HDC hDC = GetDC(nullptr);
    int res = GetDIBits(hDC, hbmp, 0, h, data, &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hDC);
    if (!res) {
        fz_free(ctx, data);
        fz_throw(ctx, FZ_ERROR_GENERIC, "GetDIBits failed");
    }

    // convert BGR with padding to RGB without padding
    unsigned char *out = data;
    bool is_grayscale = true;
    for (int y = 0; y < h; y++) {
        const unsigned char *in = data + y * stride;
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
        const unsigned char *in = out = data;
        for (int i = 0; i < w * h; i++) {
            *out++ = *in++;
            in += 2;
        }
    }

    fz_compressed_buffer *buf = nullptr;
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

    fz_colorspace *cs = is_grayscale ? fz_device_gray(ctx) : fz_device_rgb(ctx);
    return fz_new_image(ctx, w, h, 8, cs, 96, 96, 0, 0, nullptr, nullptr, buf, nullptr);
}

static fz_image *pack_jpeg(fz_context *ctx, const char *data, size_t len, SizeI size)
{
    fz_compressed_buffer *buf = nullptr;
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

    return fz_new_image(ctx, size.dx, size.dy, 8, fz_device_rgb(ctx), 96, 96, 0, 0, nullptr, nullptr, buf, nullptr);
}

static fz_image *pack_jp2(fz_context *ctx, const char *data, size_t len, SizeI size)
{
    fz_compressed_buffer *buf = nullptr;
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

    return fz_new_image(ctx, size.dx, size.dy, 8, fz_device_rgb(ctx), 96, 96, 0, 0, nullptr, nullptr, buf, nullptr);
}

PdfCreator::PdfCreator()
{
    ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx)
        return;
    fz_try(ctx) {
        doc = pdf_create_document(ctx);
    }
    fz_catch(ctx) {
        doc = nullptr;
    }
}

PdfCreator::~PdfCreator()
{
    pdf_close_document(doc);
    fz_free_context(ctx);
}

bool PdfCreator::AddImagePage(fz_image *image, float imgDpi)
{
    CrashIf(!ctx || !doc);
    if (!ctx || !doc) return false;

    pdf_page *page = nullptr;
    fz_device *dev = nullptr;
    fz_var(page);
    fz_var(dev);

    fz_try(ctx) {
        float zoom = imgDpi ? 72 / imgDpi : 1.0f;
        fz_matrix ctm = { image->w * zoom, 0, 0, image->h * zoom, 0, 0 };
        fz_rect bounds = fz_unit_rect;
        fz_transform_rect(&bounds, &ctm);
        page = pdf_create_page(doc, bounds, 72, 0);
        dev = pdf_page_write(doc, page);
        fz_fill_image(dev, image, &ctm, 1.0);
        fz_free_device(dev);
        dev = nullptr;
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
    if (!ctx || !doc) return false;

    fz_image *image = nullptr;
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
    CrashIf(!ctx || !doc);
    if (!ctx || !doc) return false;

    const WCHAR *ext = GfxFileExtFromData(data, len);
    if (str::Eq(ext, L".jpg") || str::Eq(ext, L".jp2")) {
        Size size = BitmapSizeFromData(data, len);
        fz_image *image = nullptr;
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

static bool Is7BitAscii(const WCHAR *str)
{
    for (const WCHAR *c = str; *c; c++) {
        if (*c < 32 || *c > 127)
            return false;
    }
    return true;
}

bool PdfCreator::SetProperty(DocumentProperty prop, const WCHAR *value)
{
    if (!ctx || !doc) return false;

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
    const char *name = nullptr;
    for (int i = 0; i < dimof(pdfPropNames) && !name; i++) {
        if (pdfPropNames[i].prop == prop)
            name = pdfPropNames[i].name;
    }
    if (!name)
        return false;

    ScopedMem<char> encValue;
    int encValueLen;
    if (Is7BitAscii(value)) {
        encValue.Set(str::conv::ToUtf8(value));
        encValueLen = (int)str::Len(encValue);
    }
    else {
        encValue.Set((char *)str::Join(L"\uFEFF", value));
        encValueLen = (int)((str::Len(value) + 1) * sizeof(WCHAR));
    }
    pdf_obj *obj = nullptr;
    fz_var(obj);
    fz_try(ctx) {
        pdf_obj *info = pdf_dict_getp(pdf_trailer(doc), "Info");
        if (!pdf_is_indirect(info) || !pdf_is_dict(info)) {
            info = obj = pdf_new_dict(doc, 4);
            pdf_dict_puts_drop(pdf_trailer(doc), "Info", pdf_new_ref(doc, obj));
            pdf_drop_obj(obj);
        }
        pdf_dict_puts_drop(info, name, pdf_new_string(doc, encValue, encValueLen));
    }
    fz_catch(ctx) {
        pdf_drop_obj(obj);
        return false;
    }
    return true;
}

bool PdfCreator::CopyProperties(BaseEngine *engine)
{
    static DocumentProperty props[] = {
        Prop_Title, Prop_Author, Prop_Subject, Prop_Copyright,
        Prop_ModificationDate, Prop_CreatorApp
    };
    bool ok = true;
    for (int i = 0; i < dimof(props); i++) {
        ScopedMem<WCHAR> value(engine->GetProperty(props[i]));
        if (value) {
            ok = ok && SetProperty(props[i], value);
        }
    }
    return ok;
}

bool PdfCreator::SaveToFile(const WCHAR *filePath)
{
    if (!ctx || !doc) return false;

    if (gPdfProducer)
        SetProperty(Prop_PdfProducer, gPdfProducer);

    ScopedMem<char> pathUtf8(str::conv::ToUtf8(filePath));
    fz_try(ctx) {
        pdf_write_document(doc, pathUtf8, nullptr);
    }
    fz_catch(ctx) {
        return false;
    }
    return true;
}

bool PdfCreator::RenderToFile(const WCHAR *pdfFileName, BaseEngine *engine, int dpi)
{
    PdfCreator *c = new PdfCreator();
    bool ok = true;
    // render all pages to images
    float zoom = dpi / engine->GetFileDPI();
    for (int i = 1; ok && i <= engine->PageCount(); i++) {
        RenderedBitmap *bmp = engine->RenderBitmap(i, zoom, 0, nullptr, Target_Export);
        if (bmp)
            ok = c->AddImagePage(bmp->GetBitmap(), bmp->Size(), dpi);
        else
            ok = false;
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

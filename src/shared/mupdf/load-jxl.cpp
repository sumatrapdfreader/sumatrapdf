#include "base/Base.h"
#include "mupdf/fitz.h"
#include "jxl.h"
extern "C" {
#include "pixmap-imp.h"
}
#include "load-jxl.h"

enum class JxlRead {
    Info,
    Pixels
};

static constexpr int kJxlDpi = 72;

static fz_pixmap* ReadJxl(fz_context* ctx, const unsigned char* data, size_t size, fz_colorspace* defcs, JxlRead mode,
                          int* w, int* h, fz_colorspace** cs) {
    jxl_ctx* decoder = jxl_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!decoder) {
        fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot create JPEG XL decoder");
    }
    // PDF image matrices supply orientation; decode the stored samples.
    jxl_ctx_set_keep_orientation(decoder, 1);
    jxl_doc* doc = nullptr;
    fz_pixmap* pix = nullptr;
    fz_var(doc);
    fz_var(pix);

    fz_try(ctx) {
        doc = jxl_doc_open(decoder, data, size);
        jxl_image_info info{};
        if (!doc || jxl_doc_info(doc, &info) != 0) {
            fz_throw(ctx, FZ_ERROR_FORMAT, "invalid JPEG XL image");
        }
        bool gray = info.num_color_channels == 1;
        bool alpha = info.alpha_bits != 0;
        jxl_format format = gray ? (alpha ? JXLDEC_FORMAT_GRAYA8 : JXLDEC_FORMAT_GRAY8)
                                 : (alpha ? JXLDEC_FORMAT_RGBA32 : JXLDEC_FORMAT_RGB24);
        jxl_render_info render{};
        if (jxl_frame_render_info(doc, 0, format, &render) != 0 || render.width <= 0 || render.height <= 0) {
            fz_throw(ctx, FZ_ERROR_FORMAT, "invalid JPEG XL dimensions");
        }
        *w = render.width;
        *h = render.height;
        if (defcs && fz_colorspace_n(ctx, defcs) == info.num_color_channels) {
            *cs = fz_keep_colorspace(ctx, defcs);
            if (defcs == fz_device_rgb(ctx) || defcs == fz_device_gray(ctx)) {
                jxl_ctx_set_srgb_output(decoder, 1);
            }
        } else {
            size_t profileSize = 0;
            const uint8_t* profile = jxl_doc_icc_profile(doc, &profileSize);
            if (profile && profileSize) {
                fz_buffer* icc = fz_new_buffer_from_copied_data(ctx, profile, profileSize);
                fz_try(ctx) {
                    *cs = fz_new_icc_colorspace(ctx, gray ? FZ_COLORSPACE_GRAY : FZ_COLORSPACE_RGB, 0, "JPEG XL", icc);
                }
                fz_always(ctx) {
                    fz_drop_buffer(ctx, icc);
                }
                fz_catch(ctx) {
                    fz_rethrow(ctx);
                }
            } else {
                *cs = fz_keep_colorspace(ctx, gray ? fz_device_gray(ctx) : fz_device_rgb(ctx));
                jxl_ctx_set_srgb_output(decoder, 1);
            }
        }
        if (mode == JxlRead::Pixels) {
            pix = fz_new_pixmap(ctx, *cs, *w, *h, nullptr, alpha);
            fz_set_pixmap_resolution(ctx, pix, kJxlDpi, kJxlDpi);
            if (jxl_frame_render_into(doc, 0, format, pix->samples, pix->stride) != 0) {
                fz_throw(ctx, FZ_ERROR_FORMAT, "cannot decode JPEG XL image");
            }
            if (alpha && !info.alpha_premultiplied) {
                fz_premultiply_pixmap(ctx, pix);
            }
        }
    }
    fz_always(ctx) {
        jxl_doc_close(doc);
        jxl_ctx_free(decoder);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, pix);
        fz_rethrow(ctx);
    }
    return pix;
}

fz_pixmap* fz_load_jxl(fz_context* ctx, const unsigned char* data, size_t size, fz_colorspace* defcs) {
    fz_colorspace* cs = nullptr;
    fz_pixmap* pix = nullptr;
    int w, h;
    fz_var(cs);
    fz_var(pix);
    fz_try(ctx) {
        pix = ReadJxl(ctx, data, size, defcs, JxlRead::Pixels, &w, &h, &cs);
    }
    fz_always(ctx) {
        fz_drop_colorspace(ctx, cs);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return pix;
}

void fz_load_jxl_info(fz_context* ctx, const unsigned char* data, size_t size, int* w, int* h, int* xres, int* yres,
                      fz_colorspace** cs) {
    *cs = nullptr;
    fz_try(ctx) {
        ReadJxl(ctx, data, size, nullptr, JxlRead::Info, w, h, cs);
    }
    fz_catch(ctx) {
        fz_drop_colorspace(ctx, *cs);
        *cs = nullptr;
        fz_rethrow(ctx);
    }
    *xres = *yres = kJxlDpi;
}

fz_image* fz_new_jxl_image(fz_context* ctx, fz_buffer* buffer, fz_colorspace* defcs) {
    fz_colorspace* cs = nullptr;
    fz_compressed_buffer* compressed = nullptr;
    fz_image* image = nullptr;
    int w, h;
    fz_var(cs);
    fz_var(compressed);
    fz_var(image);
    fz_try(ctx) {
        ReadJxl(ctx, buffer->data, buffer->len, defcs, JxlRead::Info, &w, &h, &cs);
        compressed = fz_new_compressed_buffer(ctx);
        compressed->buffer = fz_keep_buffer(ctx, buffer);
        compressed->params.type = FZ_IMAGE_JXL;
        fz_compressed_buffer* owned = compressed;
        compressed = nullptr;
        image = fz_new_image_from_compressed_buffer(ctx, w, h, 8, cs, kJxlDpi, kJxlDpi, 0, 0, nullptr, nullptr, owned,
                                                    nullptr);
    }
    fz_always(ctx) {
        fz_drop_colorspace(ctx, cs);
        fz_drop_compressed_buffer(ctx, compressed);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return image;
}

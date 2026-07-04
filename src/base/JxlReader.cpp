/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/GdiPlus.h"
#include "base/JxlReader.h"

#ifndef NO_LIBJXL

#include <jxl/decode.h>

namespace jxl {

// libjxl detects both the raw JPEG XL codestream and the ISOBMFF container form
bool HasSignature(Str d) {
    JxlSignature sig = JxlSignatureCheck((u8*)d.s, (size_t)d.len);
    return sig == JXL_SIG_CODESTREAM || sig == JXL_SIG_CONTAINER;
}

// RGBA bytes -> a freshly allocated BGRA8 Pixmap (swizzle, no extra copy)
static Pixmap* RgbaToPixmap(const u8* rgba, int w, int h) {
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8);
    if (!px) {
        return nullptr;
    }
    const u8* src = rgba;
    for (int y = 0; y < h; y++) {
        u8* dst = px->data + (size_t)y * px->stride;
        for (int x = 0; x < w; x++) {
            dst[0] = src[2]; // B
            dst[1] = src[1]; // G
            dst[2] = src[0]; // R
            dst[3] = src[3]; // A
            src += 4;
            dst += 4;
        }
    }
    return px;
}

Pixmap* PixmapFromData(Str d) {
    // JXL container format starts with a 0 byte
    if (len(d) == 0) {
        return nullptr;
    }
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    if (!dec) {
        return nullptr;
    }
    Pixmap* result = nullptr;
    u8* pixels = nullptr; // RGBA, 8 bits per channel
    uint32_t w = 0, h = 0;

    bool ok = JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) == JXL_DEC_SUCCESS &&
              JxlDecoderSetInput(dec, (u8*)d.s, (size_t)d.len) == JXL_DEC_SUCCESS;
    if (ok) {
        JxlDecoderCloseInput(dec);
        bool done = false;
        while (!done) {
            switch (JxlDecoderProcessInput(dec)) {
                case JXL_DEC_BASIC_INFO: {
                    JxlBasicInfo info;
                    if (JxlDecoderGetBasicInfo(dec, &info) != JXL_DEC_SUCCESS) {
                        done = true;
                        break;
                    }
                    w = info.xsize;
                    h = info.ysize;
                    if (w == 0 || h == 0) {
                        done = true;
                    }
                    break;
                }
                case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
                    JxlPixelFormat fmt = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
                    size_t need = 0;
                    if (JxlDecoderImageOutBufferSize(dec, &fmt, &need) != JXL_DEC_SUCCESS ||
                        need != (size_t)w * h * 4) {
                        done = true;
                        break;
                    }
                    pixels = AllocArray<u8>((int)need);
                    if (!pixels || JxlDecoderSetImageOutBuffer(dec, &fmt, pixels, need) != JXL_DEC_SUCCESS) {
                        done = true;
                    }
                    break;
                }
                case JXL_DEC_FULL_IMAGE:
                    // first frame is decoded into `pixels`; that's all we need
                    if (pixels) {
                        result = RgbaToPixmap(pixels, (int)w, (int)h);
                    }
                    done = true;
                    break;
                default:
                    // JXL_DEC_SUCCESS, JXL_DEC_ERROR, JXL_DEC_NEED_MORE_INPUT, ...
                    done = true;
                    break;
            }
        }
    }
    free(pixels);
    JxlDecoderDestroy(dec);
    return result;
}

Size SizeFromData(Str d) {
    Size size;
    // JXL container format starts with a 0 byte
    if (len(d) == 0) {
        return size;
    }
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    if (!dec) {
        return size;
    }
    if (JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO) == JXL_DEC_SUCCESS &&
        JxlDecoderSetInput(dec, (u8*)d.s, (size_t)d.len) == JXL_DEC_SUCCESS) {
        JxlDecoderCloseInput(dec);
        if (JxlDecoderProcessInput(dec) == JXL_DEC_BASIC_INFO) {
            JxlBasicInfo info;
            if (JxlDecoderGetBasicInfo(dec, &info) == JXL_DEC_SUCCESS) {
                size = Size((int)info.xsize, (int)info.ysize);
            }
        }
    }
    JxlDecoderDestroy(dec);
    return size;
}

} // namespace jxl

#else

namespace jxl {
bool HasSignature(Str) {
    return false;
}
Size SizeFromData(Str) {
    return Size();
}
Pixmap* PixmapFromData(Str) {
    return nullptr;
}
} // namespace jxl

#endif

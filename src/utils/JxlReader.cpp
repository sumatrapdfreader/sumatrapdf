/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/JxlReader.h"

#ifndef NO_LIBJXL

#include <jxl/decode.h>

namespace jxl {

// libjxl detects both the raw JPEG XL codestream and the ISOBMFF container form
bool HasSignature(const ByteSlice& d) {
    JxlSignature sig = JxlSignatureCheck(d.data(), d.size());
    return sig == JXL_SIG_CODESTREAM || sig == JXL_SIG_CONTAINER;
}

// RGBA bytes -> a freshly allocated 32bpp ARGB GDI+ bitmap (which is BGRA in memory)
static Gdiplus::Bitmap* RgbaToBitmap(const u8* rgba, int w, int h) {
    Gdiplus::Bitmap bmp(w, h, PixelFormat32bppARGB);
    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData bd;
    if (bmp.LockBits(&rc, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        return nullptr;
    }
    const u8* src = rgba;
    for (int y = 0; y < h; y++) {
        u8* dst = (u8*)bd.Scan0 + (size_t)y * bd.Stride;
        for (int x = 0; x < w; x++) {
            dst[0] = src[2]; // B
            dst[1] = src[1]; // G
            dst[2] = src[0]; // R
            dst[3] = src[3]; // A
            src += 4;
            dst += 4;
        }
    }
    bmp.UnlockBits(&bd);
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
}

Gdiplus::Bitmap* ImageFromData(const ByteSlice& d) {
    if (d.empty()) {
        return nullptr;
    }
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    if (!dec) {
        return nullptr;
    }
    Gdiplus::Bitmap* result = nullptr;
    u8* pixels = nullptr; // RGBA, 8 bits per channel
    uint32_t w = 0, h = 0;

    bool ok = JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) == JXL_DEC_SUCCESS &&
              JxlDecoderSetInput(dec, d.data(), d.size()) == JXL_DEC_SUCCESS;
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
                    pixels = AllocArray<u8>(need);
                    if (!pixels || JxlDecoderSetImageOutBuffer(dec, &fmt, pixels, need) != JXL_DEC_SUCCESS) {
                        done = true;
                    }
                    break;
                }
                case JXL_DEC_FULL_IMAGE:
                    // first frame is decoded into `pixels`; that's all we need
                    if (pixels) {
                        result = RgbaToBitmap(pixels, (int)w, (int)h);
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

Size SizeFromData(const ByteSlice& d) {
    Size size;
    if (d.empty()) {
        return size;
    }
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    if (!dec) {
        return size;
    }
    if (JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO) == JXL_DEC_SUCCESS &&
        JxlDecoderSetInput(dec, d.data(), d.size()) == JXL_DEC_SUCCESS) {
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
bool HasSignature(const ByteSlice&) {
    return false;
}
Size SizeFromData(const ByteSlice&) {
    return Size();
}
Gdiplus::Bitmap* ImageFromData(const ByteSlice&) {
    return nullptr;
}
} // namespace jxl

#endif

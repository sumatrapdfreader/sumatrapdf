/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/AvifReader.h"

#ifndef NO_AVIF

#include <libheif/heif.h>

// - image/heic           HEIF file using h265 compression
// - image/heif           HEIF file using any other compression
// - image/heic-sequence  HEIF image sequence using h265 compression
// - image/heif-sequence  HEIF image sequence using any other compression
// - image/avif
// - image/avif-sequence

static const char* gSupportedTypes =
    "image/heic\0"
    "image/heif\0"
    "image/avif\0\0";

bool HasAvifSignature(const ByteSlice& d) {
    const char* mimeType = heif_get_file_mime_type(d.Get(), d.Size());
    if (!mimeType) {
        return false;
    }
    int idx = seqstrings::StrToIdxIS(gSupportedTypes, mimeType);
    return idx >= 0;
}

Size AvifSizeFromData(const ByteSlice& d) {
    Size res;
    struct heif_image_handle* hdl = nullptr;
    struct heif_image* img = nullptr;

    heif_context* ctx = heif_context_alloc();

    // TODO: can I provide a subset of the image?
    auto err = heif_context_read_from_memory_without_copy(ctx, (const void*)d.Get(), d.size(), nullptr);
    if (err.code != heif_error_Ok) {
        goto Exit;
    }
    err = heif_context_get_primary_image_handle(ctx, &hdl);
    if (err.code != heif_error_Ok) {
        goto Exit;
    }
    res.dx = heif_image_handle_get_width(hdl);
    res.dy = heif_image_handle_get_width(hdl);

Exit:
    if (hdl) {
        heif_image_handle_release(hdl);
    }
    heif_context_free(ctx);
    return res;
}

Gdiplus::Bitmap* AvifImageFromData(const ByteSlice& d) {
    Gdiplus::Bitmap* bmp = nullptr;
    struct heif_image_handle* hdl = nullptr;
    struct heif_image* img = nullptr;
    int dx, dy, stride;
    int has_alpha;
    const u8* data = nullptr;

    heif_context* ctx = heif_context_alloc();
    auto err = heif_context_read_from_memory_without_copy(ctx, (const void*)d.Get(), d.size(), nullptr);

    if (err.code != heif_error_Ok) {
        goto Exit;
    }

    err = heif_context_get_primary_image_handle(ctx, &hdl);
    if (err.code != heif_error_Ok) {
        goto Exit;
    }

    dx = heif_image_handle_get_width(hdl);
    dy = heif_image_handle_get_width(hdl);

    has_alpha = heif_image_handle_has_alpha_channel(hdl);

    err = heif_decode_image(hdl, &img, heif_colorspace_RGB,
                            has_alpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB, nullptr);

    if (err.code != heif_error_Ok) {
        goto Exit;
    }

    data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

Exit:
    if (hdl) {
        heif_image_handle_release(hdl);
    }
    if (ctx) {
        heif_context_free(ctx);
    }
    return bmp;
}
#else
bool HasAvifSignature(const ByteSlice&) {
    return false;
}
Size AvifSizeFromData(const ByteSlice&) {
    return {};
}
Gdiplus::Bitmap* AvifImageFromData(const ByteSlice&) {
    return nullptr;
}
#endif

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/AvifReader.h"

#include <dav1d/dav1d.h>

bool HasAvifSignature(ByteSlice) {
    return false;
}
Size AvifSizeFromData(ByteSlice) {
    return {};
}

#if 0
avifCodec* avifCodecCreateDav1d(void) {
    avifCodec* codec = (avifCodec*)avifAlloc(sizeof(avifCodec));
    memset(codec, 0, sizeof(struct avifCodec));
    codec->getNextImage = dav1dCodecGetNextImage;
    codec->destroyInternal = dav1dCodecDestroyInternal;

    codec->internal = (struct avifCodecInternal*)avifAlloc(sizeof(struct avifCodecInternal));
    memset(codec->internal, 0, sizeof(struct avifCodecInternal));
    dav1d_default_settings(&codec->internal->dav1dSettings);

    // Ensure that we only get the "highest spatial layer" as a single frame
    // for each input sample, instead of getting each spatial layer as its own
    // frame one at a time ("all layers").
    codec->internal->dav1dSettings.all_layers = 0;
    return codec;
}
#endif

void log_callback(void*, const char*, va_list) {
}

Gdiplus::Bitmap* AvifImageFromData(ByteSlice) {
    // Dav1dPicture* p;
    Dav1dContext* c = NULL;

    Dav1dSettings settings = {};
    dav1d_default_settings(&settings);
    // NOTE(ledyba-z):
    // If > 1, dav1d tends to buffer frames(?). See libavif
    settings.max_frame_delay = 1;
    settings.logger.cookie = 0;
    settings.logger.callback = log_callback;
    settings.n_threads = 1;
    int err = dav1d_open(&c, &settings);
    if (err != 0) {
        return nullptr;
    }
    // Dav1dPicture primaryImg = {};
    // Dav1dPicture alphaImg = {};

    return nullptr;
}

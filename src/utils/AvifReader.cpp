/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/AvifReader.h"

bool HasAvifSignature(ByteSlice) {
    return false;
}
Size AvifSizeFromData(ByteSlice) {
    return {};
}

Gdiplus::Bitmap* AvifImageFromData(ByteSlice) {
    return nullptr;
}

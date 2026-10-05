/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: gpui leaves base::log(Str) to the app. This forwards it to our log().

#include "gui/GpuiBridge.h"

#include "SumatraLog.h"

void base::log(base::Str s) {
    ::log(FromGpui(s));
}

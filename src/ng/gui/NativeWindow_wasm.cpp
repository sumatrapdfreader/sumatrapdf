/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include <emscripten/emscripten.h>

#include "gui/AppShell.h"

// NOLINTNEXTLINE
EM_JS(double, WasmDevicePixelRatio, (),
      { return typeof devicePixelRatio == "number" && devicePixelRatio > 0 ? devicePixelRatio : 1; });

// Device pixels per CSS pixel. The canvas context is scaled by this, so a
// tile rendered at GetZoomReal is upscaled the same way.
float AppShellRenderScale(gpui::Window*) {
    double scale = WasmDevicePixelRatio();
    if (!(scale > 0)) {
        return 1;
    }
    return (float)scale;
}

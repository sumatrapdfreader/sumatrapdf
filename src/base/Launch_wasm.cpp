/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: the wasm half of Launch_posix.cpp. A browser tab cannot start a
// program, so the only thing left of this API is opening a URL in a new tab.

#include "base/Base.h"

#include <emscripten/emscripten.h>

#include "base/Launch.h"

// NOLINTNEXTLINE
EM_JS(void, WasmOpenUrl, (const char* s, int n), {
    var url = UTF8ToString(s, n);
    window.open(url, "_blank", "noopener");
});

static bool IsWebUrl(Str s) {
    return str::StartsWithI(s, StrL("http://")) || str::StartsWithI(s, StrL("https://")) ||
           str::StartsWithI(s, StrL("mailto:"));
}

bool LaunchFileShell(Str path, Str, Str, bool) {
    if (!IsWebUrl(path)) {
        logf("LaunchFileShell: '%s' cannot be launched in a browser\n", path);
        return false;
    }
    WasmOpenUrl(path.s, path.len);
    return true;
}

bool LaunchBrowser(Str url) {
    if (len(url) == 0) {
        return false;
    }
    WasmOpenUrl(url.s, url.len);
    return true;
}

void OpenPathInDefaultFileManager(Str) {
    // there is no file manager to show a path in
}

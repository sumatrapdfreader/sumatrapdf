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
    if (Module.sumatraHostCall && Module.sumatraHostCall("openUrl", {url : url})) {
        return;
    }
    window.open(url, "_blank", "noopener");
});

static bool IsWebUrl(Str s) {
    return str::StartsWithI(s, StrL("http://")) || str::StartsWithI(s, StrL("https://")) ||
           str::StartsWithI(s, StrL("mailto:"));
}

bool gBlockExternalLaunch = false;
int gExternalLaunchesBlocked = 0;
static Str gExternalLaunchAllowDir;

void SetExternalLaunchAllowDir(Str dir) {
    str::ReplaceWithCopy(&gExternalLaunchAllowDir, dir);
    int n = len(gExternalLaunchAllowDir);
    if (n > 1) {
        char c = gExternalLaunchAllowDir.s[n - 1];
        if (c == '\\' || c == '/') {
            gExternalLaunchAllowDir.len--;
            gExternalLaunchAllowDir.s[n - 1] = 0;
        }
    }
}

static bool ExternalLaunchAllowed(Str what) {
    int n = len(gExternalLaunchAllowDir);
    if (n == 0 || len(what) <= n || !str::StartsWithI(what, gExternalLaunchAllowDir)) {
        return false;
    }
    char sep = what.s[n];
    return sep == '\\' || sep == '/';
}

bool ExternalLaunchBlocked(Str what) {
    if (!gBlockExternalLaunch || ExternalLaunchAllowed(what)) {
        return false;
    }
    logf("ExternalLaunchBlocked: not opening '%s' in test mode\n", what);
    gExternalLaunchesBlocked++;
    return true;
}

bool LaunchFileShell(Str path, Str, Str, bool) {
    if (len(path) == 0 || ExternalLaunchBlocked(path)) {
        return false;
    }
    if (!IsWebUrl(path)) {
        logf("LaunchFileShell: '%s' cannot be launched in a browser\n", path);
        return false;
    }
    WasmOpenUrl(path.s, path.len);
    return true;
}

bool LaunchProgram(Str exe, Str) {
    logf("LaunchProgram: '%s' cannot run in a browser\n", exe);
    return false;
}

bool LaunchBrowser(Str url) {
    if (len(url) == 0 || ExternalLaunchBlocked(url)) {
        return false;
    }
    WasmOpenUrl(url.s, url.len);
    return true;
}

void OpenPathInDefaultFileManager(Str) {
    // there is no file manager to show a path in
}

void OpenFilesInDefaultManager(Str, const StrVec&) {}

/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include <emscripten/emscripten.h>

#include "gui/WasmBridge.h"

// the pending pick: the browser answers on a later turn of its event loop, so
// the callback outlives WasmPickFile(), and the path comes back in a buffer
// the JS half fills instead of through a malloc'd string
static Func1<Str> gOnPicked;
static char gPickedPath[1024];

// NOLINTNEXTLINE
EM_JS(void, WasmJsPickFile, (const char* dir, int dirLen, char* out, int outCap), {
    var dirPath = UTF8ToString(dir, dirLen);
    var input = document.createElement("input");
    input.type = "file";
    input.style.display = "none";
    document.body.appendChild(input);
    var done = function(path) {
        input.remove();
        stringToUTF8(path ? path : "", out, outCap);
        _sumatra_wasm_file_picked();
    };
    input.addEventListener(
        "change", function() {
            var file = input.files && input.files[0];
            if (!file) {
                done(null);
                return;
            }
            file.arrayBuffer().then(
                function(bytes) {
                    var path = dirPath + "/" + file.name;
                    try {
                        FS.writeFile(path, new Uint8Array(bytes));
                    } catch (e) {
                        console.error("writing " + path + " failed", e);
                        done(null);
                        return;
                    }
                    FS.syncfs(
                        false, function(err) {
                            if (err) {
                                console.error("saving " + path + " to IndexedDB failed", err);
                            }
                            done(path);
                        });
                },
                function(e) {
                    console.error("reading the picked file failed", e);
                    done(null);
                });
        });
    // a file input only opens from a user gesture; this call is one
    input.click();
});

// NOLINTNEXTLINE
EM_JS(void, WasmJsDownload, (const char* path, int pathLen, const char* name, int nameLen), {
    var filePath = UTF8ToString(path, pathLen);
    var fileName = UTF8ToString(name, nameLen);
    var bytes;
    try {
        bytes = FS.readFile(filePath);
    } catch (e) {
        console.error("reading " + filePath + " failed", e);
        return;
    }
    var url = URL.createObjectURL(new Blob([bytes], {
        type:
            "application/octet-stream"
    }));
    var a = document.createElement("a");
    a.href = url;
    a.download = fileName;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(function() { URL.revokeObjectURL(url); }, 10000);
});

// NOLINTNEXTLINE
EM_JS(void, WasmJsPersist, (const char* dir, int dirLen), {
    var dirPath = UTF8ToString(dir, dirLen);
    if (Module.__persistTimer) {
        return;
    }
    Module.__persistTimer = setTimeout(
        function() {
            Module.__persistTimer = 0;
            FS.syncfs(
                false, function(err) {
                    if (err) {
                        console.error("saving " + dirPath + " to IndexedDB failed", err);
                    }
                });
        },
        500);
});

extern "C" EMSCRIPTEN_KEEPALIVE void sumatra_wasm_file_picked() {
    Func1<Str> cb = gOnPicked;
    gOnPicked = {};
    if (gPickedPath[0] == 0 || !cb.IsValid()) {
        return;
    }
    TempStr path = str::DupTemp(Str(gPickedPath));
    gPickedPath[0] = 0;
    logf("sumatra_wasm_file_picked: '%s'\n", path);
    cb.Call(path);
}

void WasmPickFile(const Func1<Str>& onPicked) {
    dir::CreateAll(Str((char*)kWasmUploadDir));
    gOnPicked = onPicked;
    gPickedPath[0] = 0;
    Str dir((char*)kWasmUploadDir);
    WasmJsPickFile(dir.s, dir.len, gPickedPath, (int)dimof(gPickedPath));
}

bool WasmDownloadFile(Str path) {
    if (!file::Exists(path)) {
        logf("WasmDownloadFile: '%s' does not exist\n", path);
        return false;
    }
    TempStr name = path::GetBaseNameTemp(path);
    WasmJsDownload(path.s, path.len, name.s, name.len);
    return true;
}

// NOLINTNEXTLINE
EM_JS(int, WasmJsCopyImage, (const char* path, int pathLen), {
    if (!navigator.clipboard || !navigator.clipboard.write || typeof ClipboardItem == "undefined") {
        return 0;
    }
    var filePath = UTF8ToString(path, pathLen);
    var bytes;
    try {
        bytes = FS.readFile(filePath);
    } catch (e) {
        console.error("reading " + filePath + " failed", e);
        return 0;
    }
    try {
        var blob = new Blob([bytes], {
            type:
                "image/png"
        });
        var item = new ClipboardItem({"image/png" : Promise.resolve(blob)});
        navigator.clipboard.write([item]).catch(function(e) {
            if (e && e.name != "NotAllowedError") {
                console.error("copying " + filePath + " failed", e);
            }
        });
    } catch (e) {
        console.error("copying " + filePath + " failed", e);
        return 0;
    }
    return 1;
});

// image/png on the browser clipboard. The write is async; false means the
// browser has no such API, or the file could not be read.
bool WasmCopyImageFile(Str path) {
    if (!file::Exists(path)) {
        return false;
    }
    return WasmJsCopyImage(path.s, path.len) != 0;
}

// NOLINTNEXTLINE
EM_JS(int, WasmJsShareFile, (const char* path, int pathLen, const char* name, int nameLen), {
    if (!navigator.share || !navigator.canShare || typeof File == "undefined") return 0;
    var filePath = UTF8ToString(path, pathLen);
    var fileName = UTF8ToString(name, nameLen);
    var bytes;
    try {
        bytes = FS.readFile(filePath);
    } catch (e) {
        console.error("reading " + filePath + " failed", e);
        return 0;
    }
    var lower = fileName.toLowerCase();
    var mime = lower.endsWith(".pdf")                              ? "application/pdf"
               : lower.endsWith(".epub")                           ? "application/epub+zip"
               : lower.endsWith(".png")                            ? "image/png"
               : lower.endsWith(".jpg") || lower.endsWith(".jpeg") ? "image/jpeg"
                                                                   : "application/octet-stream";
    var file = new File([bytes], fileName, {
        type:
            mime
    });
    var data = {title : fileName, files : [file]};
    if (!navigator.canShare(data)) return 0;
    navigator.share(data).catch(function(e) {
        if (e && e.name != "AbortError") console.error("sharing " + filePath + " failed", e);
    });
    return 1;
});

bool WasmShareFile(Str path) {
    if (!file::Exists(path)) {
        return false;
    }
    TempStr name = path::GetBaseNameTemp(path);
    return WasmJsShareFile(path.s, path.len, name.s, name.len) != 0;
}

// NOLINTNEXTLINE
EM_JS(int, WasmJsPrintPdf, (const char* path, int pathLen), {
    var filePath = UTF8ToString(path, pathLen);
    var bytes;
    try {
        bytes = FS.readFile(filePath);
    } catch (e) {
        console.error("reading " + filePath + " failed", e);
        return 0;
    }
    var url = URL.createObjectURL(new Blob([bytes], {
        type:
            "application/pdf"
    }));
    var frame = document.createElement("iframe");
    frame.style.position = "fixed";
    frame.style.width = "1px";
    frame.style.height = "1px";
    frame.style.right = "0";
    frame.style.bottom = "0";
    frame.style.border = "0";
    frame.addEventListener(
        "load", function() {
            setTimeout(
                function() {
                    try {
                        frame.contentWindow.focus();
                        frame.contentWindow.print();
                    } catch (e) {
                        console.error("printing " + filePath + " failed", e);
                    }
                },
                0);
        });
    frame.src = url;
    document.body.appendChild(frame);
    setTimeout(
        function() {
            frame.remove();
            URL.revokeObjectURL(url);
        },
        60000);
    return 1;
});

bool WasmPrintPdf(Str path) {
    if (!file::Exists(path)) {
        return false;
    }
    return WasmJsPrintPdf(path.s, path.len) != 0;
}

void WasmPersistSettings() {
    Str dir((char*)kWasmSettingsDir);
    WasmJsPersist(dir.s, dir.len);
}

// NOLINTNEXTLINE
EM_JS(void, WasmJsQueryFile, (char* out, int outCap), {
    var file = new URLSearchParams(location.search).get("file");
    stringToUTF8(file ? file : "", out, outCap);
});

TempStr WasmQueryFileTemp() {
    char buf[1024];
    WasmJsQueryFile(buf, (int)dimof(buf));
    if (buf[0] == 0) {
        return {};
    }
    return str::DupTemp(Str(buf));
}

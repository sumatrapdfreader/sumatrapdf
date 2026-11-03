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
    var input = null;
    var settled = false;
    var done = function(path) {
        if (settled) return;
        settled = true;
        if (input) input.remove();
        stringToUTF8(path ? path : "", out, outCap);
        _sumatra_wasm_file_picked();
        if (Module.sumatraFocusCanvas) Module.sumatraFocusCanvas();
    };
    // WasmShell.js picks the name (no silent overwrite) and saves to OPFS
    var store = function(name, bytes) {
        Module.sumatraStoreUpload(dirPath, name, bytes)
            .then(
                done, function(e) {
                    console.error("storing " + name + " failed", e);
                    done(null);
                });
    };
    var fromHost = function(file) {
        if (!file || !file.name || !file.bytes) {
            done(null);
            return;
        }
        store(String(file.name), file.bytes);
    };
    if (Module.sumatraHostCall && Module.sumatraHostCall("pickFile", fromHost)) {
        return;
    }
    input = document.createElement("input");
    input.type = "file";
    input.style.display = "none";
    document.body.appendChild(input);
    input.addEventListener("cancel", function() { done(null); });
    input.addEventListener(
        "change", function() {
            var file = input.files && input.files[0];
            if (!file) {
                done(null);
                return;
            }
            file.arrayBuffer().then(
                function(bytes) { store(file.name, bytes); },
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
    // bytes() copies out of MEMFS, so a host that declines doesn't pay for it
    var hostArg = {name : fileName, path : filePath, bytes : function(){return FS.readFile(filePath); }
}
;
if (Module.sumatraHostCall && Module.sumatraHostCall("download", hostArg)) {
    return;
}
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
            var save = Module.sumatraStorageSave;
            if (!save) {
                console.error("saving " + dirPath + " to OPFS failed", "storage is not initialized");
                return;
            }
            save(function(err) {
                if (err) {
                    console.error("saving " + dirPath + " to OPFS failed", err);
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

// Open from URL: the fetches in flight, each with the buffer its stored path
// comes back in ("" when it failed)
struct WasmUrlFetch {
    Func1<Str> onStored;
    char path[1024];
};
static WasmUrlFetch gUrlFetches[8];

// NOLINTNEXTLINE
EM_JS(void, WasmJsFetchUrl,
      (const char* url, int urlLen, const char* name, int nameLen, int slot, char* out, int outCap), {
          var u = UTF8ToString(url, urlLen);
          var n = UTF8ToString(name, nameLen);
          var settled = false;
          var done = function(path) {
              if (settled) return;
              settled = true;
              stringToUTF8(path ? path : "", out, outCap);
              _sumatra_wasm_url_fetched(slot);
          };
          // stored like a dropped file: a taken name gets a numbered one
          var store = function(file) {
              if (!file || !file.bytes) {
                  done(null);
                  return;
              }
              Module.sumatraStoreUpload("/uploads", String(file.name || n), file.bytes)
                  .then(
                      done, function(e) {
                          console.error("storing " + n + " failed", e);
                          done(null);
                      });
          };
          // a host can fetch it another way, e.g. through its server for a site
          // that doesn't allow cross-origin requests (docs/wasm-host-api.md)
          if (Module.sumatraHostCall && Module.sumatraHostCall("fetchUrl", {url : u, name : n, done : store})) {
              return;
          }
          fetch(u)
              .then(function(r) {
                  if (!r.ok) throw new Error("HTTP " + r.status);
                  return r.arrayBuffer();
              })
              .then(
                  function(bytes) { store({name : n, bytes : bytes}); },
                  function(e) {
                      console.error("fetching " + u + " failed", e);
                      done(null);
                  });
      });

extern "C" EMSCRIPTEN_KEEPALIVE void sumatra_wasm_url_fetched(int slot) {
    if (slot < 0 || slot >= (int)dimof(gUrlFetches)) {
        return;
    }
    WasmUrlFetch& f = gUrlFetches[slot];
    Func1<Str> cb = f.onStored;
    f.onStored = {};
    TempStr path = str::DupTemp(Str(f.path));
    f.path[0] = 0;
    logf("sumatra_wasm_url_fetched: '%s'\n", path);
    cb.Call(path);
}

// fetches url (or has the host fetch it) and stores it in kWasmUploadDir under
// name; onStored gets its path, "" when it failed
void WasmFetchUrl(Str url, Str name, const Func1<Str>& onStored) {
    dir::CreateAll(Str((char*)kWasmUploadDir));
    for (int i = 0; i < (int)dimof(gUrlFetches); i++) {
        WasmUrlFetch& f = gUrlFetches[i];
        if (f.onStored.IsValid()) {
            continue;
        }
        f.onStored = onStored;
        f.path[0] = 0;
        WasmJsFetchUrl(url.s, url.len, name.s, name.len, i, f.path, (int)dimof(f.path));
        return;
    }
    logf("WasmFetchUrl: too many downloads at once\n");
    onStored.Call(Str());
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

// NOLINTNEXTLINE
EM_JS(int, WasmJsOpenHtml, (const char* path, int pathLen), {
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
            "text/html"
    }));
    // no "noopener": with it window.open answers null, so a blocked tab
    // couldn't be told apart
    var w = window.open(url, "_blank");
    setTimeout(function() { URL.revokeObjectURL(url); }, 60000);
    if (!w) return 0;
    w.opener = null;
    return 1;
});

bool WasmOpenHtmlFile(Str path) {
    if (!file::Exists(path)) {
        return false;
    }
    return WasmJsOpenHtml(path.s, path.len) != 0;
}

void WasmPersistSettings() {
    Str dir((char*)kWasmSettingsDir);
    WasmJsPersist(dir.s, dir.len);
}

// NOLINTNEXTLINE
EM_JS(void, WasmJsExit, (), {
    // the app is gone: no more flushes, and no prompt when the host navigates
    Module.__exited = true;
    // without a host nothing navigates away, and the canvas would stay up,
    // frozen on the last frame
    var showClosed = function() {
        var canvas = document.getElementById("gpui-canvas");
        if (canvas) canvas.style.display = "none";
        var div = document.createElement("div");
        div.style.cssText = "font: 16px system-ui, sans-serif; padding: 48px; text-align: center";
        div.textContent = "SumatraPDF was closed. ";
        var a = document.createElement("a");
        a.href = location.href;
        a.textContent = "Open it again";
        div.appendChild(a);
        document.body.appendChild(div);
    };
    var notify = function(err) {
        if (err) console.error("saving to OPFS before exit failed", err);
        var handled = Module.sumatraHostCall && Module.sumatraHostCall("exit", {saved : !err});
        if (!handled) showClosed();
    };
    if (Module.sumatraStorageSave) {
        Module.sumatraStorageSave(notify);
        return;
    }
    notify("storage is not initialized");
});

// The app is quitting and its settings are in MEMFS: finish the write to OPFS,
// then tell the host, which may navigate away.
void WasmNotifyExit() {
    WasmJsExit();
}

// NOLINTNEXTLINE
EM_JS(void, WasmJsQueryFile, (char* out, int outCap), {
    var file = new URLSearchParams(location.search).get("file");
    stringToUTF8(file ? file : "", out, outCap);
});

// NOLINTNEXTLINE
EM_JS(void, WasmJsFocusCanvas, (), {
    if (Module.sumatraFocusCanvas) {
        Module.sumatraFocusCanvas();
        return;
    }
    var canvas = document.getElementById("gpui-canvas");
    if (canvas) canvas.focus();
});

// #gpui-canvas is the only element whose keys gpui receives.
void WasmFocusCanvas() {
    WasmJsFocusCanvas();
}

// NOLINTNEXTLINE
EM_JS(int, WasmJsTakeDropped, (char* out, int outCap), {
    var paths = Module.__droppedPaths;
    if (!paths || paths.length == 0) return 0;
    stringToUTF8(paths.shift(), out, outCap);
    return 1;
});

void WasmTakeDroppedPathsTemp(Vec<Str>& out) {
    char buf[1024];
    while (WasmJsTakeDropped(buf, (int)dimof(buf))) {
        VecAppend(out, (Str)str::DupTemp(Str(buf)));
    }
}

// NOLINTNEXTLINE
EM_JS(void, WasmJsStorageError, (char* out, int outCap), { stringToUTF8(Module.__storageError || "", out, outCap); });

TempStr WasmStorageErrorTemp() {
    char buf[512];
    WasmJsStorageError(buf, (int)dimof(buf));
    return str::DupTemp(Str(buf));
}

// NOLINTNEXTLINE
EM_JS(void, WasmJsStorageLoadError, (char* out, int outCap), {
    var s = window.__sumatraStorage || "";
    stringToUTF8(s == "ready" ? "" : s, out, outCap);
});

TempStr WasmStorageLoadErrorTemp() {
    char buf[512];
    WasmJsStorageLoadError(buf, (int)dimof(buf));
    if (buf[0] == 0) {
        return {};
    }
    return str::DupTemp(Str(buf));
}

TempStr WasmQueryFileTemp() {
    char buf[1024];
    WasmJsQueryFile(buf, (int)dimof(buf));
    if (buf[0] == 0) {
        return {};
    }
    return str::DupTemp(Str(buf));
}

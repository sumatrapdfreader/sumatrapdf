/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: what a browser tab offers in place of the desktop's file services.
// gpui's wasm backend has no PromptForPathTemp and no downloads, so these are
// ours (src/gui/WasmBridge_wasm.cpp): a <input type=file> that lands the bytes
// in IDBFS, blob download / sharing, and IndexedDB write-back.

#if OS_WASM

// where WasmPickFile() persists what the user chose and where the preloaded
// sample documents live
constexpr const char* kWasmUploadDir = "/uploads";
constexpr const char* kWasmDocsDir = "/docs";
// mounted on IndexedDB by src/gui/WasmShell.js; GetAppDataDirTemp() answers it
constexpr const char* kWasmSettingsDir = "/settings";

// Opens the browser's file picker. The file is written to kWasmUploadDir and
// the callback runs on the main thread with its path; it is not called if the
// user cancels. Only one pick can be in flight.
void WasmPickFile(const Func1<Str>& onPicked);

// Hands the file's bytes to the browser as a download named after it.
bool WasmDownloadFile(Str path);

// Opens the system share sheet with the file attached. Returns false when the
// browser cannot share files, so callers can fall back to a URL.
bool WasmShareFile(Str path);

// Opens the browser print dialog for a PDF in the virtual file system.
bool WasmPrintPdf(Str path);

// Schedules a write-back of kWasmSettingsDir to IndexedDB. Coalesces: many
// calls in a row cost one sync.
void WasmPersistSettings();

// The page's `?file=` query parameter, a path in MEMFS. It is the browser's
// version of a file name on the command line.
TempStr WasmQueryFileTemp();

#endif

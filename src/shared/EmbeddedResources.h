/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The LzSA archive holds translations, scripts, fonts, manual files and installer payloads.

namespace lzma {
struct SimpleArchive;
}

Str GetEmbeddedLzsa();
bool EnsureEmbeddedArchiveLoaded();
lzma::SimpleArchive* GetEmbeddedArchive();
u8* GetEmbeddedFileData(Str name, int* outSize = nullptr);
void InstallEmbeddedFontLoader();
bool FontMatchesFileName(Str fileName, Str data);

enum class FontDownloadEvent {
    Progress,
    Done,
    Failed,
};
// called on the downloading thread; total is the font's size, 0 if unknown
using FontDownloadProgressFn = void (*)(Str fontName, i64 nDownloaded, i64 total, FontDownloadEvent ev);
void SetFontDownloadProgressFn(FontDownloadProgressFn fn);
TempStr DownloadedFontsReportTemp();
#if OS_WASM
void PrefetchCommonFonts();
#endif

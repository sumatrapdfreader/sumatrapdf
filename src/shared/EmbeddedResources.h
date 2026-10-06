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

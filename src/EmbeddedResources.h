/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Single LzSA resource (IDR_EMBEDDED_PAK / .work/embedded.dat) holds:
//   translations.txt, marked.min.js, mermaid.min.js, and in-app manual files.

namespace lzma {
struct SimpleArchive;
}

bool EnsureEmbeddedArchiveLoaded();
lzma::SimpleArchive* GetEmbeddedArchive();
// malloc'd, free with free(); null-terminated after size bytes. outSize optional.
u8* GetEmbeddedFileData(Str name, int* outSize = nullptr);

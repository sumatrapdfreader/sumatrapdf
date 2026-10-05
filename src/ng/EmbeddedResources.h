/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig keeps translations.txt, marked.min.js, mermaid.min.js, fonts\* (mupdf's
// built-in fonts), the in-app manual and the installer payload in one LzSA
// resource (IDR_EMBEDDED_PAK, see resource.h).
// ng: a .rc resource is Windows-only, so each embedded file is a gzipped C++
// byte array in a generated src/EmbeddedData*.cpp (cmd/gen-embedded.ts) and
// every platform links the same data. Font names are "fonts/<file>", orig's
// "fonts\<file>" with our path separator. The in-app manual is
// "manual/<file>" (cmd/gen-docs.ts stages it, as orig's does).

namespace lzma {
struct SimpleArchive;
}

struct EmbeddedBlob {
    const char* name;
    const u8* gz;
    int gzSize;
    int size;
};

bool EnsureEmbeddedArchiveLoaded();
lzma::SimpleArchive* GetEmbeddedArchive();
// malloc'd, free with free(); null-terminated after size bytes. outSize optional.
u8* GetEmbeddedFileData(Str name, int* outSize = nullptr);
void InstallEmbeddedFontLoader();

// the generated tables (cmd/gen-embedded.ts)
extern const EmbeddedBlob gEmbeddedText[];
extern const int gEmbeddedTextCount;
extern const EmbeddedBlob gEmbeddedMermaid[];
extern const int gEmbeddedMermaidCount;
extern const EmbeddedBlob gEmbeddedFontsUrw[];
extern const int gEmbeddedFontsUrwCount;
extern const EmbeddedBlob gEmbeddedFontsSil[];
extern const int gEmbeddedFontsSilCount;
extern const EmbeddedBlob gEmbeddedFontsNoto[];
extern const int gEmbeddedFontsNotoCount;
extern const EmbeddedBlob gEmbeddedFontsDroid[];
extern const int gEmbeddedFontsDroidCount;
extern const EmbeddedBlob gEmbeddedManual[];
extern const int gEmbeddedManualCount;

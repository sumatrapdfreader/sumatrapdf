/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Pixmap;
struct StrVec;

void OptimizePngFileAsync(Str path);
void OptimizePngFilesAsync(const StrVec& paths);

// Encode pixmap as PNG. Returns owned Str (caller str::Free); empty on fail.
Str EncodePngFromPixmap(const Pixmap* px);

// Encode pixmap as PNG and losslessly recompress with zopfli (same compressor
// as OptimizePngFileAsync). Returns owned Str (caller str::Free); empty on fail.
// Used when embedding formats PDF cannot re-wrap (e.g. JXL → PNG for Convert to PDF).
// zopfli takes seconds on a page-sized image, so a caller that only needs the
// bytes (Google Lens) uses EncodePngFromPixmap instead.
Str EncodeAndOptimizePngFromPixmap(const Pixmap* px);

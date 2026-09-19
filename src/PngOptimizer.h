/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Pixmap;
struct StrVec;

void OptimizePngFileAsync(Str path);
void OptimizePngFilesAsync(const StrVec& paths);

Str EncodePngFromPixmap(const Pixmap* px);
Str EncodeAndOptimizePngFromPixmap(const Pixmap* px);

#ifndef MUPDF_FITZ_OUTPUT_TGA_H
#define MUPDF_FITZ_OUTPUT_TGA_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/pixmap.h"

void fz_write_tga(fz_context *ctx, fz_pixmap *pixmap, const char *filename, int savealpha);

#endif

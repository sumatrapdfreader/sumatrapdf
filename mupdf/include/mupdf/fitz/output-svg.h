#ifndef MUPDF_FITZ_OUTPUT_SVG_H
#define MUPDF_FITZ_OUTPUT_SVG_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/device.h"
#include "mupdf/fitz/output.h"

fz_device *fz_new_svg_device(fz_context *ctx, fz_output *out, float page_width, float page_height);

#endif

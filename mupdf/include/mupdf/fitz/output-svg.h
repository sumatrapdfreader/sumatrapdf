#ifndef MUPDF_FITZ_OUTPUT_SVG_H
#define MUPDF_FITZ_OUTPUT_SVG_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/device.h"
#include "mupdf/fitz/output.h"

enum {
	FZ_SVG_TEXT_AS_PATH = 0,
	FZ_SVG_TEXT_AS_TEXT = 1,
};

fz_device *fz_new_svg_device(fz_context *ctx, fz_output *out, float page_width, float page_height, int text_format, int reuse_images);

fz_device *fz_new_svg_device_with_id(fz_context *ctx, fz_output *out, float page_width, float page_height, int text_format, int reuse_images, int *id);

#endif

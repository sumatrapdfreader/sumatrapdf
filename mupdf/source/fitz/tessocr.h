#ifndef TESSOCR_H
#define TESSOCR_H

#include "mupdf/fitz.h"

void *ocr_init(fz_context *ctx, const char *lang);

void ocr_fin(fz_context *ctx, void *api);

void ocr_recognise(fz_context *ctx,
		void *api,
		fz_pixmap *pix,
		void (*callback)(fz_context *ctx,
				void *arg,
				int unicode,
				const char *font_name,
				const int *line_bbox,
				const int *word_bbox,
				const int *char_bbox,
				int pointsize),
		int (*progress)(fz_context *ctx,
				void *arg,
				int progress),
		void *arg);

#endif

#ifndef MUPDF_FITZ_OUTPUT_PNM_H
#define MUPDF_FITZ_OUTPUT_PNM_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/bitmap.h"

/*
	fz_write_pnm: Save a pixmap as a pnm

	filename: The filename to save as (including extension).
*/
void fz_write_pnm(fz_context *ctx, fz_pixmap *pixmap, char *filename);

void fz_output_pnm_header(fz_output *out, int w, int h, int n);
void fz_output_pnm_band(fz_output *out, int w, int h, int n, int band, int bandheight, unsigned char *p);

/*
	fz_write_pam: Save a pixmap as a pam

	filename: The filename to save as (including extension).
*/
void fz_write_pam(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha);

void fz_output_pam_header(fz_output *out, int w, int h, int n, int savealpha);
void fz_output_pam_band(fz_output *out, int w, int h, int n, int band, int bandheight, unsigned char *sp, int savealpha);

/*
	fz_write_pbm: Save a bitmap as a pbm

	filename: The filename to save as (including extension).
*/
void fz_write_pbm(fz_context *ctx, fz_bitmap *bitmap, char *filename);

#endif

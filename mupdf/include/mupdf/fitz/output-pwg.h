#ifndef MUPDF_FITZ_OUTPUT_PWG_H
#define MUPDF_FITZ_OUTPUT_PWG_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/bitmap.h"

typedef struct fz_pwg_options_s fz_pwg_options;

struct fz_pwg_options_s
{
	/* These are not interpreted as CStrings by the writing code, but
	 * are rather copied directly out. */
	char media_class[64];
	char media_color[64];
	char media_type[64];
	char output_type[64];

	unsigned int advance_distance;
	int advance_media;
	int collate;
	int cut_media;
	int duplex;
	int insert_sheet;
	int jog;
	int leading_edge;
	int manual_feed;
	unsigned int media_position;
	unsigned int media_weight;
	int mirror_print;
	int negative_print;
	unsigned int num_copies;
	int orientation;
	int output_face_up;
	unsigned int PageSize[2];
	int separations;
	int tray_switch;
	int tumble;

	int media_type_num;
	int compression;
	unsigned int row_count;
	unsigned int row_feed;
	unsigned int row_step;

	/* These are not interpreted as CStrings by the writing code, but
	 * are rather copied directly out. */
	char rendering_intent[64];
	char page_size_name[64];
};

/*
	fz_write_pwg: Save a pixmap as a pwg

	filename: The filename to save as (including extension).

	append: If non-zero, then append a new page to existing file.

	pwg: NULL, or a pointer to an options structure (initialised to zero
	before being filled in, for future expansion).
*/
void fz_write_pwg(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pwg_options *pwg);

/*
	fz_write_pwg_bitmap: Save a bitmap as a pwg

	filename: The filename to save as (including extension).

	append: If non-zero, then append a new page to existing file.

	pwg: NULL, or a pointer to an options structure (initialised to zero
	before being filled in, for future expansion).
*/
void fz_write_pwg_bitmap(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pwg_options *pwg);

/*
	Output a pixmap to an output stream as a pwg raster.
*/
void fz_output_pwg(fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg);

/*
	Output the file header to a pwg stream, ready for pages to follow it.
*/
void fz_output_pwg_file_header(fz_output *out);

/*
	Output a page to a pwg stream to follow a header, or other pages.
*/
void fz_output_pwg_page(fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg);

/*
	Output a bitmap page to a pwg stream to follow a header, or other pages.
*/
void fz_output_pwg_bitmap_page(fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg);

#endif

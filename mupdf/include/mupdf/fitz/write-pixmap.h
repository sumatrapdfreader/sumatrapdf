// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_FITZ_WRITE_PIXMAP_H
#define MUPDF_FITZ_WRITE_PIXMAP_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/band-writer.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/bitmap.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/writer.h"

/**
	PCL output
*/
typedef struct
{
	/* Features of a particular printer */
	int features;
	const char *odd_page_init;
	const char *even_page_init;

	/* Options for this job */
	int tumble;
	int duplex_set;
	int duplex;
	int paper_size;
	int manual_feed_set;
	int manual_feed;
	int media_position_set;
	int media_position;
	int orientation;

	/* Updated as we move through the job */
	int page_count;
} fz_pcl_options;

/**
	Initialize PCL option struct for a given preset.

	Currently defined presets include:

		generic	Generic PCL printer
		ljet4	HP DeskJet
		dj500	HP DeskJet 500
		fs600	Kyocera FS-600
		lj	HP LaserJet, HP LaserJet Plus
		lj2	HP LaserJet IIp, HP LaserJet IId
		lj3	HP LaserJet III
		lj3d	HP LaserJet IIId
		lj4	HP LaserJet 4
		lj4pl	HP LaserJet 4 PL
		lj4d	HP LaserJet 4d
		lp2563b	HP 2563B line printer
		oce9050	Oce 9050 Line printer
*/
void fz_pcl_preset(fz_context *ctx, fz_pcl_options *opts, const char *preset);

/**
	Parse PCL options.

	Currently defined options and values are as follows:

		preset=X	Either "generic" or one of the presets as for fz_pcl_preset.
		spacing=0	No vertical spacing capability
		spacing=1	PCL 3 spacing (<ESC>*p+<n>Y)
		spacing=2	PCL 4 spacing (<ESC>*b<n>Y)
		spacing=3	PCL 5 spacing (<ESC>*b<n>Y and clear seed row)
		mode2		Disable/Enable mode 2 graphics compression
		mode3		Disable/Enable mode 3 graphics compression
		eog_reset	End of graphics (<ESC>*rB) resets all parameters
		has_duplex	Duplex supported (<ESC>&l<duplex>S)
		has_papersize	Papersize setting supported (<ESC>&l<sizecode>A)
		has_copies	Number of copies supported (<ESC>&l<copies>X)
		is_ljet4pjl	Disable/Enable HP 4PJL model-specific output
		is_oce9050	Disable/Enable Oce 9050 model-specific output
*/
fz_pcl_options *fz_parse_pcl_options(fz_context *ctx, fz_pcl_options *opts, const char *args);

/**
	Create a new band writer, outputting monochrome pcl.
*/
fz_band_writer *fz_new_mono_pcl_band_writer(fz_context *ctx, fz_output *out, const fz_pcl_options *options);

/**
	Write a bitmap as mono PCL.
*/
void fz_write_bitmap_as_pcl(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pcl_options *pcl);

/**
	Save a bitmap as mono PCL.
*/
void fz_save_bitmap_as_pcl(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pcl_options *pcl);

/**
	Create a new band writer, outputting color pcl.
*/
fz_band_writer *fz_new_color_pcl_band_writer(fz_context *ctx, fz_output *out, const fz_pcl_options *options);

/**
	Write an (RGB) pixmap as color PCL.
*/
void fz_write_pixmap_as_pcl(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pcl_options *pcl);

/**
	Save an (RGB) pixmap as color PCL.
*/
void fz_save_pixmap_as_pcl(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pcl_options *pcl);

/**
	PCLm output
*/
typedef struct
{
	int compress;
	int strip_height;

	/* Updated as we move through the job */
	int page_count;
} fz_pclm_options;

/**
	Parse PCLm options.

	Currently defined options and values are as follows:

		compression=none: No compression
		compression=flate: Flate compression
		strip-height=n: Strip height (default 16)
*/
fz_pclm_options *fz_parse_pclm_options(fz_context *ctx, fz_pclm_options *opts, const char *args);

/**
	Create a new band writer, outputting pclm
*/
fz_band_writer *fz_new_pclm_band_writer(fz_context *ctx, fz_output *out, const fz_pclm_options *options);

/**
	Write a (Greyscale or RGB) pixmap as pclm.
*/
void fz_write_pixmap_as_pclm(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pclm_options *options);

/**
	Save a (Greyscale or RGB) pixmap as pclm.
*/
void fz_save_pixmap_as_pclm(fz_context *ctx, fz_pixmap *pixmap, const char *filename, int append, const fz_pclm_options *options);

/**
	PDFOCR output
*/
typedef struct
{
	int compress;
	int strip_height;
	char language[256];
	char datadir[1024];

	int skew_correct; /* 0 = no skew correction. 1 = automatic. 2 = use specified angle. */
	float skew_angle; /* Only used if skew == 2 */
	int skew_border; /* 0 = increase size so no content is lost. 1 = maintain size. 2 = decrease size so no new pixels are visible. */

	/* Updated as we move through the job */
	int page_count;

	char *options;
} fz_pdfocr_options;

/**
	Parse PDFOCR options.

	Currently defined options and values are as follows:

		compression=none: No compression
		compression=flate: Flate compression
		strip-height=n: Strip height (default 16)
		ocr-language=<lang>: OCR Language (default eng)
		ocr-datadir=<datadir>: OCR data path (default rely on TESSDATA_PREFIX)
*/
fz_pdfocr_options *fz_parse_pdfocr_options(fz_context *ctx, fz_pdfocr_options *opts, const char *args);

/**
	Create a new band writer, outputting pdfocr.

	Ownership of output stays with the caller, the band writer
	borrows the reference. The caller must keep the output around
	for the duration of the band writer, and then close/drop as
	appropriate.
*/
fz_band_writer *fz_new_pdfocr_band_writer(fz_context *ctx, fz_output *out, const fz_pdfocr_options *options);

/**
	Set the progress callback for a pdfocr bandwriter.
*/
void fz_pdfocr_band_writer_set_progress(fz_context *ctx, fz_band_writer *writer, fz_pdfocr_progress_fn *progress_fn, void *progress_arg);

/**
	Write a (Greyscale or RGB) pixmap as pdfocr.
*/
void fz_write_pixmap_as_pdfocr(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pdfocr_options *options);

/**
	Save a (Greyscale or RGB) pixmap as pdfocr.
*/
void fz_save_pixmap_as_pdfocr(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pdfocr_options *options);

/**
	Save a (Greyscale or RGB) pixmap as a png.
*/
void fz_save_pixmap_as_png(fz_context *ctx, fz_pixmap *pixmap, const char *filename);

/**
	Write a pixmap as a JPEG.
*/
void fz_write_pixmap_as_jpeg(fz_context *ctx, fz_output *out, fz_pixmap *pix, int quality, int invert_cmyk);

/**
	Save a pixmap as a JPEG.
*/
void fz_save_pixmap_as_jpeg(fz_context *ctx, fz_pixmap *pixmap, const char *filename, int quality);

/**
	Write a (Greyscale or RGB) pixmap as a png.
*/
void fz_write_pixmap_as_png(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);

/**
	Pixmap data as JP2K with no subsampling.

	quality = 100 = lossless
	otherwise for a factor of x compression use 100-x. (so 80 is 1:20 compression)
*/
void fz_write_pixmap_as_jpx(fz_context *ctx, fz_output *out, fz_pixmap *pix, int quality);

/**
	Save pixmap data as JP2K with no subsampling.

	quality = 100 = lossless
	otherwise for a factor of x compression use 100-x. (so 80 is 1:20 compression)
*/
void fz_save_pixmap_as_jpx(fz_context *ctx, fz_pixmap *pixmap, const char *filename, int q);

/**
	Create a new png band writer (greyscale or RGB, with or without
	alpha).
*/
fz_band_writer *fz_new_png_band_writer(fz_context *ctx, fz_output *out);

/**
	Re-encode a given image as a PNG into a buffer.

	Ownership of the buffer is returned.
*/
fz_buffer *fz_new_buffer_from_image_as_png(fz_context *ctx, fz_image *image, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_image_as_pbm(fz_context *ctx, fz_image *image, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_image_as_pkm(fz_context *ctx, fz_image *image, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_image_as_pnm(fz_context *ctx, fz_image *image, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_image_as_pam(fz_context *ctx, fz_image *image, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_image_as_psd(fz_context *ctx, fz_image *image, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_image_as_jpeg(fz_context *ctx, fz_image *image, fz_color_params color_params, int quality, int invert_cmyk);
fz_buffer *fz_new_buffer_from_image_as_jpx(fz_context *ctx, fz_image *image, fz_color_params color_params, int quality);

/**
	Re-encode a given pixmap as a PNG into a buffer.

	Ownership of the buffer is returned.
*/
fz_buffer *fz_new_buffer_from_pixmap_as_png(fz_context *ctx, fz_pixmap *pixmap, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_pixmap_as_pbm(fz_context *ctx, fz_pixmap *pixmap, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_pixmap_as_pkm(fz_context *ctx, fz_pixmap *pixmap, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_pixmap_as_pnm(fz_context *ctx, fz_pixmap *pixmap, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_pixmap_as_pam(fz_context *ctx, fz_pixmap *pixmap, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_pixmap_as_psd(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_pixmap_as_jpeg(fz_context *ctx, fz_pixmap *pixmap, fz_color_params color_params, int quality, int invert_cmyk);
fz_buffer *fz_new_buffer_from_pixmap_as_jpx(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params, int quality);

/**
	Save a pixmap as a pnm (greyscale or rgb, no alpha).
*/
void fz_save_pixmap_as_pnm(fz_context *ctx, fz_pixmap *pixmap, const char *filename);

/**
	Write a pixmap as a pnm (greyscale or rgb, no alpha).
*/
void fz_write_pixmap_as_pnm(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);

/**
	Create a band writer targeting pnm (greyscale or rgb, no
	alpha).
*/
fz_band_writer *fz_new_pnm_band_writer(fz_context *ctx, fz_output *out);

/**
	Save a pixmap as a pnm (greyscale, rgb or cmyk, with or without
	alpha).
*/
void fz_save_pixmap_as_pam(fz_context *ctx, fz_pixmap *pixmap, const char *filename);

/**
	Write a pixmap as a pnm (greyscale, rgb or cmyk, with or without
	alpha).
*/
void fz_write_pixmap_as_pam(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);

/**
	Create a band writer targeting pnm (greyscale, rgb or cmyk,
	with or without alpha).
*/
fz_band_writer *fz_new_pam_band_writer(fz_context *ctx, fz_output *out);

/**
	Save a bitmap as a pbm.
*/
void fz_save_bitmap_as_pbm(fz_context *ctx, fz_bitmap *bitmap, const char *filename);

/**
	Write a bitmap as a pbm.
*/
void fz_write_bitmap_as_pbm(fz_context *ctx, fz_output *out, fz_bitmap *bitmap);

/**
	Create a new band writer, targeting pbm.
*/
fz_band_writer *fz_new_pbm_band_writer(fz_context *ctx, fz_output *out);

/**
	Save a pixmap as a pbm. (Performing halftoning).
*/
void fz_save_pixmap_as_pbm(fz_context *ctx, fz_pixmap *pixmap, const char *filename);
void fz_write_pixmap_as_pkm(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);

/**
	Save a CMYK bitmap as a pkm.
*/
void fz_save_bitmap_as_pkm(fz_context *ctx, fz_bitmap *bitmap, const char *filename);
void fz_write_pixmap_as_pbm(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);

/**
	Write a CMYK bitmap as a pkm.
*/
void fz_write_bitmap_as_pkm(fz_context *ctx, fz_output *out, fz_bitmap *bitmap);

/**
	Create a new pkm band writer for CMYK pixmaps.
*/
fz_band_writer *fz_new_pkm_band_writer(fz_context *ctx, fz_output *out);

/**
	Save a CMYK pixmap as a pkm. (Performing halftoning).
*/
void fz_save_pixmap_as_pkm(fz_context *ctx, fz_pixmap *pixmap, const char *filename);

/**
	Write a (gray, rgb, or cmyk, no alpha) pixmap out as postscript.
*/
void fz_write_pixmap_as_ps(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap);

/**
	Save a (gray, rgb, or cmyk, no alpha) pixmap out as postscript.
*/
void fz_save_pixmap_as_ps(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append);

/**
	Create a postscript band writer for gray, rgb, or cmyk, no
	alpha.
*/
fz_band_writer *fz_new_ps_band_writer(fz_context *ctx, fz_output *out);

/**
	Write the file level header for ps band writer output.
*/
void fz_write_ps_file_header(fz_context *ctx, fz_output *out);

/**
	Write the file level trailer for ps band writer output.
*/
void fz_write_ps_file_trailer(fz_context *ctx, fz_output *out, int pages);

/**
	Save a pixmap as a PSD file.
*/
void fz_save_pixmap_as_psd(fz_context *ctx, fz_pixmap *pixmap, const char *filename);

/**
	Write a pixmap as a PSD file.
*/
void fz_write_pixmap_as_psd(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap);

/**
	Open a PSD band writer.
*/
fz_band_writer *fz_new_psd_band_writer(fz_context *ctx, fz_output *out);

typedef struct
{
	/* These are not interpreted as CStrings by the writing code,
	 * but are rather copied directly out. */
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
} fz_pwg_options;

/**
	Save a pixmap as a PWG.
*/
void fz_save_pixmap_as_pwg(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pwg_options *pwg);

/**
	Save a bitmap as a PWG.
*/
void fz_save_bitmap_as_pwg(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pwg_options *pwg);

/**
	Write a pixmap as a PWG.
*/
void fz_write_pixmap_as_pwg(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg);

/**
	Write a bitmap as a PWG.
*/
void fz_write_bitmap_as_pwg(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg);

/**
	Write a pixmap as a PWG page.

	Caller should provide a file header by calling
	fz_write_pwg_file_header, but can then write several pages to
	the same file.
*/
void fz_write_pixmap_as_pwg_page(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg);

/**
	Write a bitmap as a PWG page.

	Caller should provide a file header by calling
	fz_write_pwg_file_header, but can then write several pages to
	the same file.
*/
void fz_write_bitmap_as_pwg_page(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg);

/**
	Create a new monochrome pwg band writer.
*/
fz_band_writer *fz_new_mono_pwg_band_writer(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg);

/**
	Create a new color pwg band writer.
*/
fz_band_writer *fz_new_pwg_band_writer(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg);

/**
	Output the file header to a pwg stream, ready for pages to follow it.
*/
void fz_write_pwg_file_header(fz_context *ctx, fz_output *out); /* for use by mudraw.c */

#endif

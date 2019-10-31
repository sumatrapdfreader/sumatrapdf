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

/*
	PCL output
*/
typedef struct fz_pcl_options_s fz_pcl_options;

struct fz_pcl_options_s
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
};

void fz_pcl_preset(fz_context *ctx, fz_pcl_options *opts, const char *preset);

fz_pcl_options *fz_parse_pcl_options(fz_context *ctx, fz_pcl_options *opts, const char *args);

fz_band_writer *fz_new_mono_pcl_band_writer(fz_context *ctx, fz_output *out, const fz_pcl_options *options);
void fz_write_bitmap_as_pcl(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pcl_options *pcl);
void fz_save_bitmap_as_pcl(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pcl_options *pcl);

fz_band_writer *fz_new_color_pcl_band_writer(fz_context *ctx, fz_output *out, const fz_pcl_options *options);
void fz_write_pixmap_as_pcl(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pcl_options *pcl);
void fz_save_pixmap_as_pcl(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pcl_options *pcl);

/*
	PCLm output
*/
typedef struct fz_pclm_options_s fz_pclm_options;

struct fz_pclm_options_s
{
	int compress;
	int strip_height;

	/* Updated as we move through the job */
	int page_count;
};

fz_pclm_options *fz_parse_pclm_options(fz_context *ctx, fz_pclm_options *opts, const char *args);

fz_band_writer *fz_new_pclm_band_writer(fz_context *ctx, fz_output *out, const fz_pclm_options *options);
void fz_write_pixmap_as_pclm(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pclm_options *options);
void fz_save_pixmap_as_pclm(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pclm_options *options);

void fz_save_pixmap_as_png(fz_context *ctx, fz_pixmap *pixmap, const char *filename);
void fz_write_pixmap_as_png(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap);

fz_band_writer *fz_new_png_band_writer(fz_context *ctx, fz_output *out);

fz_buffer *fz_new_buffer_from_image_as_png(fz_context *ctx, fz_image *image, fz_color_params color_params);
fz_buffer *fz_new_buffer_from_pixmap_as_png(fz_context *ctx, fz_pixmap *pixmap, fz_color_params color_params);

void fz_save_pixmap_as_pnm(fz_context *ctx, fz_pixmap *pixmap, const char *filename);
void fz_write_pixmap_as_pnm(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);
fz_band_writer *fz_new_pnm_band_writer(fz_context *ctx, fz_output *out);

void fz_save_pixmap_as_pam(fz_context *ctx, fz_pixmap *pixmap, const char *filename);
void fz_write_pixmap_as_pam(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);
fz_band_writer *fz_new_pam_band_writer(fz_context *ctx, fz_output *out);

void fz_save_bitmap_as_pbm(fz_context *ctx, fz_bitmap *bitmap, const char *filename);
void fz_write_bitmap_as_pbm(fz_context *ctx, fz_output *out, fz_bitmap *bitmap);
fz_band_writer *fz_new_pbm_band_writer(fz_context *ctx, fz_output *out);
void fz_save_pixmap_as_pbm(fz_context *ctx, fz_pixmap *pixmap, const char *filename);

void fz_save_bitmap_as_pkm(fz_context *ctx, fz_bitmap *bitmap, const char *filename);
void fz_write_bitmap_as_pkm(fz_context *ctx, fz_output *out, fz_bitmap *bitmap);
fz_band_writer *fz_new_pkm_band_writer(fz_context *ctx, fz_output *out);
void fz_save_pixmap_as_pkm(fz_context *ctx, fz_pixmap *pixmap, const char *filename);

void fz_write_pixmap_as_ps(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap);
void fz_save_pixmap_as_ps(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append);

fz_band_writer *fz_new_ps_band_writer(fz_context *ctx, fz_output *out);
void fz_write_ps_file_header(fz_context *ctx, fz_output *out);
void fz_write_ps_file_trailer(fz_context *ctx, fz_output *out, int pages);

void fz_save_pixmap_as_psd(fz_context *ctx, fz_pixmap *pixmap, const char *filename);
void fz_write_pixmap_as_psd(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap);
fz_band_writer *fz_new_psd_band_writer(fz_context *ctx, fz_output *out);

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

void fz_save_pixmap_as_pwg(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pwg_options *pwg);
void fz_save_bitmap_as_pwg(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pwg_options *pwg);
void fz_write_pixmap_as_pwg(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg);
void fz_write_bitmap_as_pwg(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg);
void fz_write_pixmap_as_pwg_page(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg);
void fz_write_bitmap_as_pwg_page(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg);

fz_band_writer *fz_new_mono_pwg_band_writer(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg);
fz_band_writer *fz_new_pwg_band_writer(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg);

void fz_write_pwg_file_header(fz_context *ctx, fz_output *out); /* for use by mudraw.c */

#endif

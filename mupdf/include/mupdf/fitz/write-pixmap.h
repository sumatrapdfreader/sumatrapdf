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

/*
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

/*
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

fz_band_writer *fz_new_mono_pcl_band_writer(fz_context *ctx, fz_output *out, const fz_pcl_options *options);
void fz_write_bitmap_as_pcl(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pcl_options *pcl);
void fz_save_bitmap_as_pcl(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pcl_options *pcl);

fz_band_writer *fz_new_color_pcl_band_writer(fz_context *ctx, fz_output *out, const fz_pcl_options *options);
void fz_write_pixmap_as_pcl(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pcl_options *pcl);
void fz_save_pixmap_as_pcl(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pcl_options *pcl);

/*
	PCLm output
*/
typedef struct
{
	int compress;
	int strip_height;

	/* Updated as we move through the job */
	int page_count;
} fz_pclm_options;

/*
	Parse PCLm options.

	Currently defined options and values are as follows:

		compression=none: No compression
		compression=flate: Flate compression
		strip-height=n: Strip height (default 16)
*/
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

void fz_save_pixmap_as_pwg(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pwg_options *pwg);
void fz_save_bitmap_as_pwg(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pwg_options *pwg);
void fz_write_pixmap_as_pwg(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg);
void fz_write_bitmap_as_pwg(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg);
void fz_write_pixmap_as_pwg_page(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg);
void fz_write_bitmap_as_pwg_page(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg);

fz_band_writer *fz_new_mono_pwg_band_writer(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg);
fz_band_writer *fz_new_pwg_band_writer(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg);

/*
	Output the file header to a pwg stream, ready for pages to follow it.
*/
void fz_write_pwg_file_header(fz_context *ctx, fz_output *out); /* for use by mudraw.c */

#endif

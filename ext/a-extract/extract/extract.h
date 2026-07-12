#ifndef ARITFEX_EXTRACT_H
#define ARITFEX_EXTRACT_H

#include "extract/alloc.h"

#include <float.h>

typedef struct extract_t extract_t;

typedef struct extract_buffer_t extract_buffer_t;

typedef enum
{
	extract_format_ODT,
	extract_format_DOCX,
	extract_format_HTML,
	extract_format_TEXT,
	extract_format_JSON
} extract_format_t;

int extract_begin(extract_alloc_t *alloc, extract_format_t format, extract_t **pextract);

void extract_set_space_guess(extract_t *extract, double space_guess);

int extract_page_begin(extract_t *extract, double minx, double miny, double maxx, double maxy);

int extract_span_begin(extract_t  *extract,
		       const char *font_name,
		       int	   font_bold,
		       int	   font_italic,
		       int	   wmode,
		       double	   ctm_a,
		       double	   ctm_b,
		       double	   ctm_c,
		       double	   ctm_d,
		       double	   bbox_x0,
		       double	   bbox_y0,
		       double	   bbox_x1,
		       double	   bbox_y1);

int extract_add_char(extract_t *extract,
		     double	x,
		     double	y,
		     unsigned	ucs,
		     double	adv,
		     double	minx,
		     double	miny,
		     double	maxx,
		     double	maxy);

int extract_span_end(extract_t *extract);

typedef void (extract_image_data_free)(void *handle, void *image_data);

int extract_add_image(
		extract_t               *extract,
		const char              *type,
		double                   x,
		double                   y,
		double                   w,
		double                   h,
		void                    *data,
		size_t                   data_size,
		extract_image_data_free *data_free,
		void                    *data_free_handle);

int extract_add_path4(
		extract_t *extract,
		double     ctm_a,
		double     ctm_b,
		double     ctm_c,
		double     ctm_d,
		double     ctm_e,
		double     ctm_f,
		double     x0,
		double     y0,
		double     x1,
		double     y1,
		double     x2,
		double     y2,
		double     x3,
		double     y3,
		double     color);

int extract_add_line(
		extract_t *extract,
		double     ctm_a,
		double     ctm_b,
		double     ctm_c,
		double     ctm_d,
		double     ctm_e,
		double     ctm_f,
		double     width,
		double     x0,
		double     y0,
		double     x1,
		double     y1,
		double     color);

int extract_fill_begin(
		extract_t *extract,
		double     ctm_a,
		double     ctm_b,
		double     ctm_c,
		double     ctm_d,
		double     ctm_e,
		double     ctm_f,
		double     color);

int extract_stroke_begin(
		extract_t *extract,
		double     ctm_a,
		double     ctm_b,
		double     ctm_c,
		double     ctm_d,
		double     ctm_e,
		double     ctm_f,
		double     line_width,
		double     color);

int extract_moveto(extract_t *extract, double x, double y);

int extract_lineto(extract_t *extract, double x, double y);

int extract_closepath(extract_t *extract);

int extract_fill_end(extract_t *extract);

int extract_stroke_end(extract_t *extract);

int extract_page_end(extract_t *extract);

typedef enum
{
	extract_struct_INVALID = -1,
	extract_struct_UNDEFINED,

	extract_struct_DOCUMENT,
	extract_struct_PART,
	extract_struct_ART,
	extract_struct_SECT,
	extract_struct_DIV,
	extract_struct_BLOCKQUOTE,
	extract_struct_CAPTION,
	extract_struct_TOC,
	extract_struct_TOCI,
	extract_struct_INDEX,
	extract_struct_NONSTRUCT,
	extract_struct_PRIVATE,

	extract_struct_DOCUMENTFRAGMENT,

	extract_struct_ASIDE,

	extract_struct_TITLE,
	extract_struct_FENOTE,

	extract_struct_SUB,

	extract_struct_P,
	extract_struct_H,
	extract_struct_H1,
	extract_struct_H2,
	extract_struct_H3,
	extract_struct_H4,
	extract_struct_H5,
	extract_struct_H6,

	extract_struct_LIST,
	extract_struct_LISTITEM,
	extract_struct_LABEL,
	extract_struct_LISTBODY,

	extract_struct_TABLE,
	extract_struct_TR,
	extract_struct_TH,
	extract_struct_TD,
	extract_struct_THEAD,
	extract_struct_TBODY,
	extract_struct_TFOOT,

	extract_struct_SPAN,
	extract_struct_QUOTE,
	extract_struct_NOTE,
	extract_struct_REFERENCE,
	extract_struct_BIBENTRY,
	extract_struct_CODE,
	extract_struct_LINK,
	extract_struct_ANNOT,

	extract_struct_EM,
	extract_struct_STRONG,

	extract_struct_RUBY,
	extract_struct_RB,
	extract_struct_RT,
	extract_struct_RP,

	extract_struct_WARICHU,
	extract_struct_WT,
	extract_struct_WP,

	extract_struct_FIGURE,
	extract_struct_FORMULA,
	extract_struct_FORM,

	extract_struct_ARTIFACT
} extract_struct_t;

int extract_begin_struct(extract_t *extract, extract_struct_t type, int uid, int score);

int extract_end_struct(extract_t *extract);

const char *extract_struct_string(extract_struct_t type);

int extract_process(
		extract_t *extract,
		int        spacing,
		int        rotation,
		int        images);

int extract_write(extract_t *extract, extract_buffer_t *buffer);

int extract_write_content(extract_t *extract, extract_buffer_t *buffer);

int extract_write_template(
		extract_t  *extract,
		const char *path_template,
		const char *path_out,
		int         preserve_dir);

void extract_end(extract_t **pextract);

int extract_set_layout_analysis(extract_t *extract, int enable);

int extract_tables_csv_format(extract_t *extract, const char *path_format);

int extract_read_intermediate(
		extract_t        *extract,
		extract_buffer_t *buffer);

void extract_internal_end(void);

void extract_exp_min(extract_t *extract, size_t size);

void extract_analyse(extract_t *extract);

#endif

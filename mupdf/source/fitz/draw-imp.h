#ifndef MUPDF_DRAW_IMP_H
#define MUPDF_DRAW_IMP_H

/*
 * Scan converter
 */

typedef struct fz_gel_s fz_gel;

fz_gel *fz_new_gel(fz_context *ctx);
void fz_insert_gel(fz_gel *gel, float x0, float y0, float x1, float y1);
void fz_reset_gel(fz_gel *gel, const fz_irect *clip);
void fz_sort_gel(fz_gel *gel);
fz_irect *fz_bound_gel(const fz_gel *gel, fz_irect *bbox);
void fz_free_gel(fz_gel *gel);
int fz_is_rect_gel(fz_gel *gel);

void fz_scan_convert(fz_gel *gel, int eofill, const fz_irect *clip, fz_pixmap *pix, unsigned char *colorbv);

void fz_flatten_fill_path(fz_gel *gel, fz_path *path, const fz_matrix *ctm, float flatness);
void fz_flatten_stroke_path(fz_gel *gel, fz_path *path, const fz_stroke_state *stroke, const fz_matrix *ctm, float flatness, float linewidth);
void fz_flatten_dash_path(fz_gel *gel, fz_path *path, const fz_stroke_state *stroke, const fz_matrix *ctm, float flatness, float linewidth);

fz_irect *fz_bound_path_accurate(fz_context *ctx, fz_irect *bbox, const fz_irect *scissor, fz_path *path, const fz_stroke_state *stroke, const fz_matrix *ctm, float flatness, float linewidth);

/*
 * Plotting functions.
 */

void fz_paint_solid_alpha(unsigned char * restrict dp, int w, int alpha);
void fz_paint_solid_color(unsigned char * restrict dp, int n, int w, unsigned char *color);

void fz_paint_span(unsigned char * restrict dp, unsigned char * restrict sp, int n, int w, int alpha);
void fz_paint_span_with_color(unsigned char * restrict dp, unsigned char * restrict mp, int n, int w, unsigned char *color);

void fz_paint_image(fz_pixmap *dst, const fz_irect *scissor, fz_pixmap *shape, fz_pixmap *img, const fz_matrix *ctm, int alpha);
void fz_paint_image_with_color(fz_pixmap *dst, const fz_irect *scissor, fz_pixmap *shape, fz_pixmap *img, const fz_matrix *ctm, unsigned char *colorbv);

void fz_paint_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha);
void fz_paint_pixmap_with_mask(fz_pixmap *dst, fz_pixmap *src, fz_pixmap *msk);
void fz_paint_pixmap_with_bbox(fz_pixmap *dst, fz_pixmap *src, int alpha, fz_irect bbox);

void fz_blend_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha, int blendmode, int isolated, fz_pixmap *shape);
void fz_blend_pixel(unsigned char dp[3], unsigned char bp[3], unsigned char sp[3], int blendmode);

#endif

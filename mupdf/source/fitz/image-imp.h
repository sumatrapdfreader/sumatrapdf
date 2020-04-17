#ifndef FITZ_IMAGE_IMP_H
#define FITZ_IMAGE_IMP_H

fz_pixmap *fz_load_jpeg(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_png(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_tiff(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_jxr(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_gif(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_bmp(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_pnm(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_jbig2(fz_context *ctx, const unsigned char *data, size_t size);

void fz_load_jpeg_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_jpx_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_png_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_tiff_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_jxr_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_gif_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_bmp_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_pnm_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_jbig2_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);

#endif

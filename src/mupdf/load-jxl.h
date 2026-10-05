#ifdef __cplusplus
extern "C" {
#endif

fz_pixmap* fz_load_jxl(fz_context*, const unsigned char*, size_t, fz_colorspace*);
void fz_load_jxl_info(fz_context*, const unsigned char*, size_t, int*, int*, int*, int*, fz_colorspace**);
fz_image* fz_new_jxl_image(fz_context*, fz_buffer*, fz_colorspace*);

#ifdef __cplusplus
}
#endif

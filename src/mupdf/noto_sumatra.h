/* SumatraPDF: mupdf's built-in fonts come from this loader, see noto_sumatra.c */

#ifdef __cplusplus
extern "C" {
#endif

/* returns the font file's bytes (kept alive for the life of the process) or NULL */
typedef const unsigned char* (*fz_builtin_font_loader)(const char* file_name, int* size);

void fz_set_builtin_font_loader(fz_builtin_font_loader loader);

#ifdef __cplusplus
}
#endif

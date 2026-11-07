/* SumatraPDF: mupdf's built-in fonts come from this loader, see noto_sumatra.c */

#ifdef __cplusplus
extern "C" {
#endif

/* returns the font file's bytes (kept alive for the life of the process) or NULL */
typedef const unsigned char* (*fz_builtin_font_loader)(const char* file_name, int* size);

void fz_set_builtin_font_loader(fz_builtin_font_loader loader);

const char* sumatra_lookup_font_url(const char* name);
int sumatra_font_count(void);
const char* sumatra_font_at(int i, const char** url, int* size);

#if defined(__linux__) || defined(__APPLE__)
/* a system font with the character, malloc'ed, or NULL (mupdf_load_system_font*.c) */
unsigned char* sumatra_system_font_data_for_rune(unsigned int rune, int* size);
#endif

#ifdef __cplusplus
}
#endif

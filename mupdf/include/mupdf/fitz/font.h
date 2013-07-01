#ifndef MUPDF_FITZ_FONT_H
#define MUPDF_FITZ_FONT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/buffer.h"

/*
	An abstract font handle. Currently there are no public API functions
	for handling these.
*/
typedef struct fz_font_s fz_font;

/*
 * Fonts come in two variants:
 *	Regular fonts are handled by FreeType.
 *	Type 3 fonts have callbacks to the interpreter.
 */

char *ft_error_string(int err);

/* forward declaration for circular dependency */
struct fz_device_s;
struct fz_display_list_s;

struct fz_font_s
{
	int refs;
	char name[32];

	void *ft_face; /* has an FT_Face if used */
	int ft_substitute; /* ... substitute metrics */
	int ft_bold; /* ... synthesize bold */
	int ft_italic; /* ... synthesize italic */
	int ft_hint; /* ... force hinting for DynaLab fonts */

	/* origin of font data */
	char *ft_file;
	unsigned char *ft_data;
	int ft_size;

	fz_matrix t3matrix;
	void *t3resources;
	fz_buffer **t3procs; /* has 256 entries if used */
	struct fz_display_list_s **t3lists; /* has 256 entries if used */
	float *t3widths; /* has 256 entries if used */
	char *t3flags; /* has 256 entries if used */
	void *t3doc; /* a pdf_document for the callback */
	void (*t3run)(void *doc, void *resources, fz_buffer *contents, struct fz_device_s *dev, const fz_matrix *ctm, void *gstate, int nestedDepth);
	void (*t3freeres)(void *doc, void *resources);

	fz_rect bbox;	/* font bbox is used only for t3 fonts */

	/* per glyph bounding box cache */
	int use_glyph_bbox;
	int bbox_count;
	fz_rect *bbox_table;

	/* substitute metrics */
	int width_count;
	int *width_table; /* in 1000 units */
};

void fz_new_font_context(fz_context *ctx);
fz_font_context *fz_keep_font_context(fz_context *ctx);
void fz_drop_font_context(fz_context *ctx);

fz_font *fz_new_type3_font(fz_context *ctx, char *name, const fz_matrix *matrix);

fz_font *fz_new_font_from_memory(fz_context *ctx, char *name, unsigned char *data, int len, int index, int use_glyph_bbox);
fz_font *fz_new_font_from_file(fz_context *ctx, char *name, char *path, int index, int use_glyph_bbox);

fz_font *fz_keep_font(fz_context *ctx, fz_font *font);
void fz_drop_font(fz_context *ctx, fz_font *font);

void fz_set_font_bbox(fz_context *ctx, fz_font *font, float xmin, float ymin, float xmax, float ymax);
fz_rect *fz_bound_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *trm, fz_rect *r);
int fz_glyph_cacheable(fz_context *ctx, fz_font *font, int gid);

void fz_decouple_type3_font(fz_context *ctx, fz_font *font, void *t3doc);

#ifndef NDEBUG
void fz_print_font(fz_context *ctx, FILE *out, fz_font *font);
#endif

#endif

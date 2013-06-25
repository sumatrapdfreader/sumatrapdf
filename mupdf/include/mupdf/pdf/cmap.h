#ifndef MUPDF_PDF_CMAP_H
#define MUPDF_PDF_CMAP_H

/*
 * CMap
 */

typedef struct pdf_cmap_s pdf_cmap;
typedef struct pdf_range_s pdf_range;

enum { PDF_CMAP_SINGLE, PDF_CMAP_RANGE, PDF_CMAP_TABLE, PDF_CMAP_MULTI };

struct pdf_range_s
{
	unsigned short low;
	/* Next, we pack 2 fields into the same unsigned short. Top 14 bits
	 * are the extent, bottom 2 bits are flags: single, range, table,
	 * multi */
	unsigned short extent_flags;
	unsigned short offset;	/* range-delta or table-index */
};

struct pdf_cmap_s
{
	fz_storable storable;
	char cmap_name[32];

	char usecmap_name[32];
	pdf_cmap *usecmap;

	int wmode;

	int codespace_len;
	struct
	{
		unsigned short n;
		unsigned short low;
		unsigned short high;
	} codespace[40];

	int rlen, rcap;
	pdf_range *ranges;

	int tlen, tcap;
	unsigned short *table;
};

pdf_cmap *pdf_new_cmap(fz_context *ctx);
pdf_cmap *pdf_keep_cmap(fz_context *ctx, pdf_cmap *cmap);
void pdf_drop_cmap(fz_context *ctx, pdf_cmap *cmap);
void pdf_free_cmap_imp(fz_context *ctx, fz_storable *cmap);
unsigned int pdf_cmap_size(fz_context *ctx, pdf_cmap *cmap);

int pdf_cmap_wmode(fz_context *ctx, pdf_cmap *cmap);
void pdf_set_cmap_wmode(fz_context *ctx, pdf_cmap *cmap, int wmode);
void pdf_set_usecmap(fz_context *ctx, pdf_cmap *cmap, pdf_cmap *usecmap);

void pdf_add_codespace(fz_context *ctx, pdf_cmap *cmap, int low, int high, int n);
void pdf_map_range_to_table(fz_context *ctx, pdf_cmap *cmap, int low, int *map, int len);
void pdf_map_range_to_range(fz_context *ctx, pdf_cmap *cmap, int srclo, int srchi, int dstlo);
void pdf_map_one_to_many(fz_context *ctx, pdf_cmap *cmap, int one, int *many, int len);
void pdf_sort_cmap(fz_context *ctx, pdf_cmap *cmap);

int pdf_lookup_cmap(pdf_cmap *cmap, int cpt);
int pdf_lookup_cmap_full(pdf_cmap *cmap, int cpt, int *out);
int pdf_decode_cmap(pdf_cmap *cmap, unsigned char *s, int *cpt);

pdf_cmap *pdf_new_identity_cmap(fz_context *ctx, int wmode, int bytes);
pdf_cmap *pdf_load_cmap(fz_context *ctx, fz_stream *file);
pdf_cmap *pdf_load_system_cmap(fz_context *ctx, char *name);
pdf_cmap *pdf_load_builtin_cmap(fz_context *ctx, char *name);
pdf_cmap *pdf_load_embedded_cmap(pdf_document *doc, pdf_obj *ref);

#ifndef NDEBUG
void pdf_print_cmap(fz_context *ctx, pdf_cmap *cmap);
#endif

#endif

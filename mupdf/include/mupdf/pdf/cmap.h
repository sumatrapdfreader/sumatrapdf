#ifndef MUPDF_PDF_CMAP_H
#define MUPDF_PDF_CMAP_H

#define PDF_MRANGE_CAP 8

typedef struct
{
	unsigned short low, high, out;
} pdf_range;

typedef struct
{
	unsigned int low, high, out;
} pdf_xrange;

typedef struct
{
	unsigned int low, out;
} pdf_mrange;

typedef struct cmap_splay cmap_splay;

typedef struct pdf_cmap
{
	fz_storable storable;
	char cmap_name[32];

	char usecmap_name[32];
	struct pdf_cmap *usecmap;

	int wmode;

	int codespace_len;
	struct
	{
		int n;
		unsigned int low;
		unsigned int high;
	} codespace[40];

	int rlen, rcap;
	pdf_range *ranges;

	int xlen, xcap;
	pdf_xrange *xranges;

	int mlen, mcap;
	pdf_mrange *mranges;

	int dlen, dcap;
	int *dict;

	int tlen, tcap, ttop;
	cmap_splay *tree;
} pdf_cmap;

pdf_cmap *pdf_new_cmap(fz_context *ctx);
pdf_cmap *pdf_keep_cmap(fz_context *ctx, pdf_cmap *cmap);
void pdf_drop_cmap(fz_context *ctx, pdf_cmap *cmap);
void pdf_drop_cmap_imp(fz_context *ctx, fz_storable *cmap);
size_t pdf_cmap_size(fz_context *ctx, pdf_cmap *cmap);

int pdf_cmap_wmode(fz_context *ctx, pdf_cmap *cmap);
void pdf_set_cmap_wmode(fz_context *ctx, pdf_cmap *cmap, int wmode);
void pdf_set_usecmap(fz_context *ctx, pdf_cmap *cmap, pdf_cmap *usecmap);

/*
	Add a codespacerange section.
	These ranges are used by pdf_decode_cmap to decode
	multi-byte encoded strings.
*/
void pdf_add_codespace(fz_context *ctx, pdf_cmap *cmap, unsigned int low, unsigned int high, size_t n);

/*
	Add a range of contiguous one-to-one mappings (ie 1..5 maps to 21..25)
*/
void pdf_map_range_to_range(fz_context *ctx, pdf_cmap *cmap, unsigned int srclo, unsigned int srchi, int dstlo);

/*
	Add a single one-to-many mapping.
*/
void pdf_map_one_to_many(fz_context *ctx, pdf_cmap *cmap, unsigned int one, int *many, size_t len);
void pdf_sort_cmap(fz_context *ctx, pdf_cmap *cmap);

/*
	Lookup the mapping of a codepoint.
*/
int pdf_lookup_cmap(pdf_cmap *cmap, unsigned int cpt);
int pdf_lookup_cmap_full(pdf_cmap *cmap, unsigned int cpt, int *out);

/*
	Use the codespace ranges to extract a codepoint from a
	multi-byte encoded string.
*/
int pdf_decode_cmap(pdf_cmap *cmap, unsigned char *s, unsigned char *e, unsigned int *cpt);

/*
	Create an Identity-* CMap (for both 1 and 2-byte encodings)
*/
pdf_cmap *pdf_new_identity_cmap(fz_context *ctx, int wmode, int bytes);
pdf_cmap *pdf_load_cmap(fz_context *ctx, fz_stream *file);

/*
	Load predefined CMap from system.
*/
pdf_cmap *pdf_load_system_cmap(fz_context *ctx, const char *name);

/*
	Load built-in CMap resource.
*/
pdf_cmap *pdf_load_builtin_cmap(fz_context *ctx, const char *name);

/*
	Load CMap stream in PDF file
*/
pdf_cmap *pdf_load_embedded_cmap(fz_context *ctx, pdf_document *doc, pdf_obj *ref);

#endif

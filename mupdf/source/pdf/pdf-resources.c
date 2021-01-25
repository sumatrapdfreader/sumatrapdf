#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

static void pdf_drop_obj_as_void(fz_context *ctx, void *obj)
{
	pdf_drop_obj(ctx, obj);
}

/* We do need to come up with an effective way to see what is already in the
 * file to avoid adding to what is already there. This is avoided for pdfwrite
 * as we check as we add each font. For adding text to an existing file though
 * it may be more problematic. */

pdf_obj *
pdf_find_font_resource(fz_context *ctx, pdf_document *doc, int type, int encoding, fz_font *item, pdf_font_resource_key *key)
{
	pdf_obj *res;

	if (!doc->resources.fonts)
		doc->resources.fonts = fz_new_hash_table(ctx, 4096, sizeof(*key), -1, pdf_drop_obj_as_void);

	memset(key, 0, sizeof(*key));
	fz_font_digest(ctx, item, key->digest);

	key->type = type;
	key->encoding = encoding;
	key->local_xref = doc->local_xref;

	res = fz_hash_find(ctx, doc->resources.fonts, (void *)key);
	if (res)
		pdf_keep_obj(ctx, res);
	return res;
}

pdf_obj *
pdf_insert_font_resource(fz_context *ctx, pdf_document *doc, pdf_font_resource_key *key, pdf_obj *obj)
{
	pdf_obj *res = fz_hash_insert(ctx, doc->resources.fonts, (void *)key, obj);
	if (res)
		fz_warn(ctx, "warning: font resource already present");
	else
		res = pdf_keep_obj(ctx, obj);
	return pdf_keep_obj(ctx, res);
}

void
pdf_drop_resource_tables(fz_context *ctx, pdf_document *doc)
{
	if (doc)
	{
		fz_drop_hash_table(ctx, doc->resources.fonts);
	}
}

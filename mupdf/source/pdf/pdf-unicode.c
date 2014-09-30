#include "mupdf/pdf.h"

/* Load or synthesize ToUnicode map for fonts */

static void
pdf_remap_cmap_range(fz_context *ctx, pdf_cmap *ucs_from_gid,
	unsigned int cpt, unsigned int gid, unsigned int n, pdf_cmap *ucs_from_cpt)
{
	unsigned int k;
	int ucsbuf[8];
	int ucslen;

	for (k = 0; k <= n; ++k)
	{
		ucslen = pdf_lookup_cmap_full(ucs_from_cpt, cpt + k, ucsbuf);
		if (ucslen == 1)
			pdf_map_range_to_range(ctx, ucs_from_gid, gid + k, gid + k, ucsbuf[0]);
		else if (ucslen > 1)
			pdf_map_one_to_many(ctx, ucs_from_gid, gid + k, ucsbuf, ucslen);
	}
}

static pdf_cmap *
pdf_remap_cmap(fz_context *ctx, pdf_cmap *gid_from_cpt, pdf_cmap *ucs_from_cpt)
{
	pdf_cmap *ucs_from_gid;
	unsigned int a, b, x;
	int i;

	ucs_from_gid = pdf_new_cmap(ctx);

	if (gid_from_cpt->usecmap)
		ucs_from_gid->usecmap = pdf_remap_cmap(ctx, gid_from_cpt->usecmap, ucs_from_cpt);

	for (i = 0; i < gid_from_cpt->rlen; ++i)
	{
		a = gid_from_cpt->ranges[i].low;
		b = gid_from_cpt->ranges[i].high;
		x = gid_from_cpt->ranges[i].out;
		pdf_remap_cmap_range(ctx, ucs_from_gid, a, x, b - a, ucs_from_cpt);
	}

	for (i = 0; i < gid_from_cpt->xlen; ++i)
	{
		a = gid_from_cpt->xranges[i].low;
		b = gid_from_cpt->xranges[i].high;
		x = gid_from_cpt->xranges[i].out;
		pdf_remap_cmap_range(ctx, ucs_from_gid, a, x, b - a, ucs_from_cpt);
	}

	/* Font encoding CMaps don't have one-to-many mappings, so we can ignore the mranges. */

	pdf_sort_cmap(ctx, ucs_from_gid);

	return ucs_from_gid;
}

void
pdf_load_to_unicode(pdf_document *doc, pdf_font_desc *font,
	char **strings, char *collection, pdf_obj *cmapstm)
{
	fz_context *ctx = doc->ctx;
	unsigned int cpt;

	if (pdf_is_stream(doc, pdf_to_num(cmapstm), pdf_to_gen(cmapstm)))
	{
		pdf_cmap *ucs_from_cpt = pdf_load_embedded_cmap(doc, cmapstm);
		font->to_unicode = pdf_remap_cmap(ctx, font->encoding, ucs_from_cpt);
		pdf_drop_cmap(ctx, ucs_from_cpt);
		font->size += pdf_cmap_size(ctx, font->to_unicode);
	}

	else if (collection)
	{
		if (!strcmp(collection, "Adobe-CNS1"))
			font->to_unicode = pdf_load_system_cmap(ctx, "Adobe-CNS1-UCS2");
		else if (!strcmp(collection, "Adobe-GB1"))
			font->to_unicode = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");
		else if (!strcmp(collection, "Adobe-Japan1"))
			font->to_unicode = pdf_load_system_cmap(ctx, "Adobe-Japan1-UCS2");
		else if (!strcmp(collection, "Adobe-Korea1"))
			font->to_unicode = pdf_load_system_cmap(ctx, "Adobe-Korea1-UCS2");
		/* SumatraPDF: load an identity cmap (until a ToUnicode is synthesized below) */
		else if (!strcmp(collection, "Adobe-Identity") && !(font->flags & PDF_FD_SYMBOLIC))
			font->to_unicode = pdf_new_identity_cmap(ctx, font->wmode, 2);

		return;
	}

	if (strings)
	{
		/* TODO one-to-many mappings */

		font->cid_to_ucs_len = 256;
		font->cid_to_ucs = fz_malloc_array(ctx, 256, sizeof *font->cid_to_ucs);
		font->size += 256 * sizeof *font->cid_to_ucs;

		for (cpt = 0; cpt < 256; cpt++)
		{
			if (strings[cpt])
				font->cid_to_ucs[cpt] = pdf_lookup_agl(strings[cpt]);
			else
				font->cid_to_ucs[cpt] = '?';
		}
	}

	if (!font->to_unicode && !font->cid_to_ucs)
	{
		/* TODO: synthesize a ToUnicode if it's a freetype font with
		 * cmap and/or post tables or if it has glyph names. */
	}
}

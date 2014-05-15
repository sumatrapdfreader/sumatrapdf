#include "mupdf/pdf.h"

/* Load or synthesize ToUnicode map for fonts */

void
pdf_load_to_unicode(pdf_document *doc, pdf_font_desc *font,
	char **strings, char *collection, pdf_obj *cmapstm)
{
	unsigned int cpt;
	int gid;
	int ucsbuf[8];
	int ucslen;
	int i;
	fz_context *ctx = doc->ctx;

	if (pdf_is_stream(doc, pdf_to_num(cmapstm), pdf_to_gen(cmapstm)))
	{
		pdf_cmap *gid_from_cpt = font->encoding;
		pdf_cmap *ucs_from_cpt = pdf_load_embedded_cmap(doc, cmapstm);

		font->to_unicode = pdf_new_cmap(ctx);

		for (i = 0; i < gid_from_cpt->codespace_len; ++i)
		{
			for (cpt = gid_from_cpt->codespace[i].low; cpt <= gid_from_cpt->codespace[i].high; ++cpt)
			{
				gid = pdf_lookup_cmap(gid_from_cpt, cpt);
				if (gid >= 0)
				{
					ucslen = pdf_lookup_cmap_full(ucs_from_cpt, cpt, ucsbuf);
					if (ucslen == 1)
						pdf_map_range_to_range(ctx, font->to_unicode, gid, gid, ucsbuf[0]);
					if (ucslen > 1)
						pdf_map_one_to_many(ctx, font->to_unicode, gid, ucsbuf, ucslen);
				}
			}
		}

		pdf_sort_cmap(ctx, font->to_unicode);

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

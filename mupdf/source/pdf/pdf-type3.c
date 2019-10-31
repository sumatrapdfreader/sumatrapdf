#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

static void
pdf_run_glyph_func(fz_context *ctx, void *doc, void *rdb, fz_buffer *contents, fz_device *dev, fz_matrix ctm, void *gstate, fz_default_colorspaces *default_cs)
{
	pdf_run_glyph(ctx, doc, (pdf_obj *)rdb, contents, dev, ctm, gstate, default_cs);
}

static void
pdf_t3_free_resources(fz_context *ctx, void *doc, void *rdb_)
{
	pdf_obj *rdb = (pdf_obj *)rdb_;
	pdf_drop_obj(ctx, rdb);
}

pdf_font_desc *
pdf_load_type3_font(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *dict)
{
	char buf[256];
	const char *estrings[256];
	pdf_font_desc *fontdesc = NULL;
	pdf_obj *encoding;
	pdf_obj *widths;
	pdf_obj *charprocs;
	pdf_obj *obj;
	int first, last;
	int i, k, n;
	fz_rect bbox;
	fz_matrix matrix;
	fz_font *font = NULL;

	fz_var(fontdesc);

	/* Make a new type3 font entry in the document */
	if (doc->num_type3_fonts == doc->max_type3_fonts)
	{
		int new_max = doc->max_type3_fonts * 2;

		if (new_max == 0)
			new_max = 4;
		doc->type3_fonts = fz_realloc_array(ctx, doc->type3_fonts, new_max, fz_font*);
		doc->max_type3_fonts = new_max;
	}

	fz_try(ctx)
	{
		obj = pdf_dict_get(ctx, dict, PDF_NAME(Name));
		if (pdf_is_name(ctx, obj))
			fz_strlcpy(buf, pdf_to_name(ctx, obj), sizeof buf);
		else
			fz_strlcpy(buf, "Unnamed-T3", sizeof buf);

		fontdesc = pdf_new_font_desc(ctx);

		matrix = pdf_dict_get_matrix(ctx, dict, PDF_NAME(FontMatrix));
		bbox = pdf_dict_get_rect(ctx, dict, PDF_NAME(FontBBox));
		bbox = fz_transform_rect(bbox, matrix);

		font = fz_new_type3_font(ctx, buf, matrix);
		fontdesc->font = font;
		fontdesc->size += sizeof(fz_font) + 256 * (sizeof(fz_buffer*) + sizeof(float));

		fz_set_font_bbox(ctx, font, bbox.x0, bbox.y0, bbox.x1, bbox.y1);

		/* Encoding */

		for (i = 0; i < 256; i++)
			estrings[i] = NULL;

		encoding = pdf_dict_get(ctx, dict, PDF_NAME(Encoding));
		if (!encoding)
		{
			fz_throw(ctx, FZ_ERROR_SYNTAX, "Type3 font missing Encoding");
		}

		if (pdf_is_name(ctx, encoding))
			pdf_load_encoding(estrings, pdf_to_name(ctx, encoding));

		if (pdf_is_dict(ctx, encoding))
		{
			pdf_obj *base, *diff, *item;

			base = pdf_dict_get(ctx, encoding, PDF_NAME(BaseEncoding));
			if (pdf_is_name(ctx, base))
				pdf_load_encoding(estrings, pdf_to_name(ctx, base));

			diff = pdf_dict_get(ctx, encoding, PDF_NAME(Differences));
			if (pdf_is_array(ctx, diff))
			{
				n = pdf_array_len(ctx, diff);
				k = 0;
				for (i = 0; i < n; i++)
				{
					item = pdf_array_get(ctx, diff, i);
					if (pdf_is_int(ctx, item))
						k = pdf_to_int(ctx, item);
					if (pdf_is_name(ctx, item) && k >= 0 && k < (int)nelem(estrings))
						estrings[k++] = pdf_to_name(ctx, item);
				}
			}
		}

		fontdesc->encoding = pdf_new_identity_cmap(ctx, 0, 1);
		fontdesc->size += pdf_cmap_size(ctx, fontdesc->encoding);

		pdf_load_to_unicode(ctx, doc, fontdesc, estrings, NULL, pdf_dict_get(ctx, dict, PDF_NAME(ToUnicode)));

		/* Use the glyph index as ASCII when we can't figure out a proper encoding */
		if (fontdesc->cid_to_ucs_len == 256)
		{
			for (i = 32; i < 127; ++i)
				if (fontdesc->cid_to_ucs[i] == FZ_REPLACEMENT_CHARACTER)
					fontdesc->cid_to_ucs[i] = i;
		}

		/* Widths */

		pdf_set_default_hmtx(ctx, fontdesc, 0);

		first = pdf_dict_get_int(ctx, dict, PDF_NAME(FirstChar));
		last = pdf_dict_get_int(ctx, dict, PDF_NAME(LastChar));

		if (first < 0 || last > 255 || first > last)
			first = last = 0;

		widths = pdf_dict_get(ctx, dict, PDF_NAME(Widths));
		if (!widths)
		{
			fz_throw(ctx, FZ_ERROR_SYNTAX, "Type3 font missing Widths");
		}

		for (i = first; i <= last; i++)
		{
			float w = pdf_array_get_real(ctx, widths, i - first);
			w = font->t3matrix.a * w * 1000;
			font->t3widths[i] = w * 0.001f;
			pdf_add_hmtx(ctx, fontdesc, i, i, w);
		}

		pdf_end_hmtx(ctx, fontdesc);

		/* Resources -- inherit page resources if the font doesn't have its own */

		font->t3freeres = pdf_t3_free_resources;
		font->t3resources = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
		if (!font->t3resources)
			font->t3resources = rdb;
		if (font->t3resources)
			pdf_keep_obj(ctx, font->t3resources);
		if (!font->t3resources)
			fz_warn(ctx, "no resource dictionary for type 3 font!");

		font->t3doc = doc;
		font->t3run = pdf_run_glyph_func;

		/* CharProcs */

		charprocs = pdf_dict_get(ctx, dict, PDF_NAME(CharProcs));
		if (!charprocs)
		{
			fz_throw(ctx, FZ_ERROR_SYNTAX, "Type3 font missing CharProcs");
		}

		for (i = 0; i < 256; i++)
		{
			if (estrings[i])
			{
				obj = pdf_dict_gets(ctx, charprocs, estrings[i]);
				if (pdf_is_stream(ctx, obj))
				{
					font->t3procs[i] = pdf_load_stream(ctx, obj);
					fz_trim_buffer(ctx, font->t3procs[i]);
					fontdesc->size += fz_buffer_storage(ctx, font->t3procs[i], NULL);
					fontdesc->size += 0; // TODO: display list size calculation
				}
			}
		}
	}
	fz_catch(ctx)
	{
		pdf_drop_font(ctx, fontdesc);
		fz_rethrow(ctx);
	}

	doc->type3_fonts[doc->num_type3_fonts++] = fz_keep_font(ctx, font);

	return fontdesc;
}

void pdf_load_type3_glyphs(fz_context *ctx, pdf_document *doc, pdf_font_desc *fontdesc)
{
	int i;

	fz_try(ctx)
	{
		for (i = 0; i < 256; i++)
		{
			if (fontdesc->font->t3procs[i])
			{
				fz_prepare_t3_glyph(ctx, fontdesc->font, i);
				fontdesc->size += 0; // TODO: display list size calculation
			}
		}
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "Type3 glyph load failed: %s", fz_caught_message(ctx));
	}
}

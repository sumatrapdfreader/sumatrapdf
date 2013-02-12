#include "fitz-internal.h"
#include "mupdf-internal.h"

static void
pdf_run_glyph_func(void *doc, void *rdb, fz_buffer *contents, fz_device *dev, const fz_matrix *ctm, void *gstate, int nested_depth)
{
	pdf_run_glyph(doc, (pdf_obj *)rdb, contents, dev, ctm, gstate, nested_depth);
}

static void
pdf_t3_free_resources(void *doc, void *rdb_)
{
	pdf_obj *rdb = (pdf_obj *)rdb_;
	pdf_drop_obj(rdb);
}


pdf_font_desc *
pdf_load_type3_font(pdf_document *xref, pdf_obj *rdb, pdf_obj *dict)
{
	char buf[256];
	char *estrings[256];
	pdf_font_desc *fontdesc = NULL;
	pdf_obj *encoding;
	pdf_obj *widths;
	pdf_obj *charprocs;
	pdf_obj *obj;
	int first, last;
	int i, k, n;
	fz_rect bbox;
	fz_matrix matrix;
	fz_context *ctx = xref->ctx;

	fz_var(fontdesc);

	fz_try(ctx)
	{
		obj = pdf_dict_gets(dict, "Name");
		if (pdf_is_name(obj))
			fz_strlcpy(buf, pdf_to_name(obj), sizeof buf);
		else
			sprintf(buf, "Unnamed-T3");

		fontdesc = pdf_new_font_desc(ctx);

		obj = pdf_dict_gets(dict, "FontMatrix");
		pdf_to_matrix(ctx, obj, &matrix);

		obj = pdf_dict_gets(dict, "FontBBox");
		fz_transform_rect(pdf_to_rect(ctx, obj, &bbox), &matrix);

		fontdesc->font = fz_new_type3_font(ctx, buf, &matrix);
		fontdesc->size += sizeof(fz_font) + 256 * (sizeof(fz_buffer*) + sizeof(float));

		fz_set_font_bbox(ctx, fontdesc->font, bbox.x0, bbox.y0, bbox.x1, bbox.y1);

		/* SumatraPDF: expose Type3 FontDescriptor flags */
		fontdesc->flags = pdf_to_int(pdf_dict_gets(pdf_dict_gets(dict, "FontDescriptor"), "Flags"));

		/* Encoding */

		for (i = 0; i < 256; i++)
			estrings[i] = NULL;

		encoding = pdf_dict_gets(dict, "Encoding");
		if (!encoding)
		{
			fz_throw(ctx, "syntaxerror: Type3 font missing Encoding");
		}

		if (pdf_is_name(encoding))
			pdf_load_encoding(estrings, pdf_to_name(encoding));

		if (pdf_is_dict(encoding))
		{
			pdf_obj *base, *diff, *item;

			base = pdf_dict_gets(encoding, "BaseEncoding");
			if (pdf_is_name(base))
				pdf_load_encoding(estrings, pdf_to_name(base));

			diff = pdf_dict_gets(encoding, "Differences");
			if (pdf_is_array(diff))
			{
				n = pdf_array_len(diff);
				k = 0;
				for (i = 0; i < n; i++)
				{
					item = pdf_array_get(diff, i);
					if (pdf_is_int(item))
						k = pdf_to_int(item);
					if (pdf_is_name(item) && k >= 0 && k < nelem(estrings))
						estrings[k++] = pdf_to_name(item);
				}
			}
		}

		fontdesc->encoding = pdf_new_identity_cmap(ctx, 0, 1);
		fontdesc->size += pdf_cmap_size(ctx, fontdesc->encoding);

		pdf_load_to_unicode(xref, fontdesc, estrings, NULL, pdf_dict_gets(dict, "ToUnicode"));

		/* SumatraPDF: trying to match Adobe Reader's behavior */
		if (!(fontdesc->flags & PDF_FD_SYMBOLIC) && fontdesc->cid_to_ucs_len >= 128)
			for (i = 32; i < 128; i++)
				if (fontdesc->cid_to_ucs[i] == '?' || fontdesc->cid_to_ucs[i] == '\0')
					fontdesc->cid_to_ucs[i] = i;

		/* Widths */

		pdf_set_default_hmtx(ctx, fontdesc, 0);

		first = pdf_to_int(pdf_dict_gets(dict, "FirstChar"));
		last = pdf_to_int(pdf_dict_gets(dict, "LastChar"));

		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1966 */
		if (first >= 256 && last - first < 256)
		{
			fz_warn(ctx, "ignoring out-of-bound values for FirstChar/LastChar: %d/%d", first, last);
			last -= first;
			first = 0;
		}

		if (first < 0 || last > 255 || first > last)
			first = last = 0;

		widths = pdf_dict_gets(dict, "Widths");
		if (!widths)
		{
			fz_throw(ctx, "syntaxerror: Type3 font missing Widths");
		}

		for (i = first; i <= last; i++)
		{
			float w = pdf_to_real(pdf_array_get(widths, i - first));
			w = fontdesc->font->t3matrix.a * w * 1000;
			fontdesc->font->t3widths[i] = w * 0.001f;
			pdf_add_hmtx(ctx, fontdesc, i, i, w);
		}

		pdf_end_hmtx(ctx, fontdesc);

		/* Resources -- inherit page resources if the font doesn't have its own */

		fontdesc->font->t3freeres = pdf_t3_free_resources;
		fontdesc->font->t3resources = pdf_dict_gets(dict, "Resources");
		if (!fontdesc->font->t3resources)
			fontdesc->font->t3resources = rdb;
		if (fontdesc->font->t3resources)
			pdf_keep_obj(fontdesc->font->t3resources);
		if (!fontdesc->font->t3resources)
			fz_warn(ctx, "no resource dictionary for type 3 font!");

		fontdesc->font->t3doc = xref;
		fontdesc->font->t3run = pdf_run_glyph_func;

		/* CharProcs */

		charprocs = pdf_dict_gets(dict, "CharProcs");
		if (!charprocs)
		{
			fz_throw(ctx, "syntaxerror: Type3 font missing CharProcs");
		}

		for (i = 0; i < 256; i++)
		{
			if (estrings[i])
			{
				/* SumatraPDF: don't reject fonts with few broken glyphs */
				fz_try(ctx)
				{

				obj = pdf_dict_gets(charprocs, estrings[i]);
				if (pdf_is_stream(xref, pdf_to_num(obj), pdf_to_gen(obj)))
				{
					fontdesc->font->t3procs[i] = pdf_load_stream(xref, pdf_to_num(obj), pdf_to_gen(obj));
					fontdesc->size += fontdesc->font->t3procs[i]->cap;
					fontdesc->size += 0; // TODO: display list size calculation
				}

				}
				fz_catch(ctx)
				{
					fz_warn(ctx, "failed to get data for type 3 glyph '%s'", estrings[i]);
				}
			}
		}
	}
	fz_catch(ctx)
	{
		if (fontdesc)
			pdf_drop_font(ctx, fontdesc);
		fz_throw(ctx, "cannot load type3 font (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict));
	}
	return fontdesc;
}

void pdf_load_type3_glyphs(pdf_document *xref, pdf_font_desc *fontdesc, int nested_depth)
{
	int i;
	fz_context *ctx = xref->ctx;

	fz_try(ctx)
	{
		for (i = 0; i < 256; i++)
		{
			if (fontdesc->font->t3procs[i])
			{
				fz_prepare_t3_glyph(ctx, fontdesc->font, i, nested_depth);
				fontdesc->size += 0; // TODO: display list size calculation
			}
		}
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "Type3 glyph load failed: %s", fz_caught(ctx));
	}
}

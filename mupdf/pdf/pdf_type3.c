#include "fitz.h"
#include "mupdf.h"

static fz_error
pdf_run_glyph_func(void *xref, fz_obj *rdb, fz_buffer *contents, fz_device *dev, fz_matrix ctm)
{
	return pdf_run_glyph(xref, rdb, contents, dev, ctm);
}

fz_error
pdf_load_type3_font(pdf_font_desc **fontdescp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict)
{
	fz_error error;
	char buf[256];
	char *estrings[256];
	pdf_font_desc *fontdesc;
	fz_obj *encoding;
	fz_obj *widths;
	fz_obj *charprocs;
	fz_obj *obj;
	int first, last;
	int i, k, n;
	fz_rect bbox;
	fz_matrix matrix;

	obj = fz_dict_gets(dict, "Name");
	if (fz_is_name(obj))
		fz_strlcpy(buf, fz_to_name(obj), sizeof buf);
	else
		sprintf(buf, "Unnamed-T3");

	fontdesc = pdf_new_font_desc();

	obj = fz_dict_gets(dict, "FontMatrix");
	matrix = pdf_to_matrix(obj);

	obj = fz_dict_gets(dict, "FontBBox");
	bbox = pdf_to_rect(obj);

	fontdesc->font = fz_new_type3_font(buf, matrix);

	fz_set_font_bbox(fontdesc->font, bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	/* Encoding */

	for (i = 0; i < 256; i++)
		estrings[i] = NULL;

	encoding = fz_dict_gets(dict, "Encoding");
	if (!encoding)
	{
		error = fz_throw("syntaxerror: Type3 font missing Encoding");
		goto cleanup;
	}

	if (fz_is_name(encoding))
		pdf_load_encoding(estrings, fz_to_name(encoding));

	if (fz_is_dict(encoding))
	{
		fz_obj *base, *diff, *item;

		base = fz_dict_gets(encoding, "BaseEncoding");
		if (fz_is_name(base))
			pdf_load_encoding(estrings, fz_to_name(base));

		diff = fz_dict_gets(encoding, "Differences");
		if (fz_is_array(diff))
		{
			n = fz_array_len(diff);
			k = 0;
			for (i = 0; i < n; i++)
			{
				item = fz_array_get(diff, i);
				if (fz_is_int(item))
					k = fz_to_int(item);
				if (fz_is_name(item))
					estrings[k++] = fz_to_name(item);
				if (k < 0) k = 0;
				if (k > 255) k = 255;
			}
		}
	}

	fontdesc->encoding = pdf_new_identity_cmap(0, 1);

	error = pdf_load_to_unicode(fontdesc, xref, estrings, NULL, fz_dict_gets(dict, "ToUnicode"));
	if (error)
		goto cleanup;

	/* Widths */

	pdf_set_default_hmtx(fontdesc, 0);

	first = fz_to_int(fz_dict_gets(dict, "FirstChar"));
	last = fz_to_int(fz_dict_gets(dict, "LastChar"));

	widths = fz_dict_gets(dict, "Widths");
	if (!widths)
	{
		error = fz_throw("syntaxerror: Type3 font missing Widths");
		goto cleanup;
	}

	for (i = first; i <= last; i++)
	{
		float w = fz_to_real(fz_array_get(widths, i - first));
		w = fontdesc->font->t3matrix.a * w * 1000;
		fontdesc->font->t3widths[i] = w * 0.001f;
		pdf_add_hmtx(fontdesc, i, i, w);
	}

	pdf_end_hmtx(fontdesc);

	/* Resources -- inherit page resources if the font doesn't have its own */

	fontdesc->font->t3resources = fz_dict_gets(dict, "Resources");
	if (!fontdesc->font->t3resources)
		fontdesc->font->t3resources = rdb;
	if (fontdesc->font->t3resources)
		fz_keep_obj(fontdesc->font->t3resources);
	if (!fontdesc->font->t3resources)
		fz_warn("no resource dictionary for type 3 font!");

	fontdesc->font->t3xref = xref;
	fontdesc->font->t3run = pdf_run_glyph_func;

	/* CharProcs */

	charprocs = fz_dict_gets(dict, "CharProcs");
	if (!charprocs)
	{
		error = fz_throw("syntaxerror: Type3 font missing CharProcs");
		goto cleanup;
	}

	for (i = 0; i < 256; i++)
	{
		if (estrings[i])
		{
			obj = fz_dict_gets(charprocs, estrings[i]);
			if (pdf_is_stream(xref, fz_to_num(obj), fz_to_gen(obj)))
			{
				error = pdf_load_stream(&fontdesc->font->t3procs[i], xref, fz_to_num(obj), fz_to_gen(obj));
				if (error)
					goto cleanup;
			}
		}
	}

	*fontdescp = fontdesc;
	return fz_okay;

cleanup:
	fz_drop_font(fontdesc->font);
	fz_free(fontdesc);
	return fz_rethrow(error, "cannot load type3 font (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
}

#include "fitz.h"
#include "mupdf.h"

fz_error
pdf_loadtype3font(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict)
{
	fz_error error;
	char buf[256];
	char *estrings[256];
	pdf_fontdesc *fontdesc;
	fz_obj *encoding;
	fz_obj *widths;
	fz_obj *charprocs;
	fz_obj *obj;
	int first, last;
	int i, k, n;
	fz_rect bbox;
	fz_matrix matrix;

	obj = fz_dictgets(dict, "Name");
	if (fz_isname(obj))
		fz_strlcpy(buf, fz_toname(obj), sizeof buf);
	else
		sprintf(buf, "Unnamed-T3");

	fontdesc = pdf_newfontdesc();

	pdf_logfont("load type3 font (%d %d R) ptr=%p {\n", fz_tonum(dict), fz_togen(dict), fontdesc);
	pdf_logfont("name %s\n", buf);

	obj = fz_dictgets(dict, "FontMatrix");
	matrix = pdf_tomatrix(obj);

	pdf_logfont("matrix [%g %g %g %g %g %g]\n",
		matrix.a, matrix.b,
		matrix.c, matrix.d,
		matrix.e, matrix.f);

	obj = fz_dictgets(dict, "FontBBox");
	bbox = pdf_torect(obj);

	pdf_logfont("bbox [%g %g %g %g]\n",
		bbox.x0, bbox.y0,
		bbox.x1, bbox.y1);

	fontdesc->font = fz_newtype3font(buf, matrix);

	fz_setfontbbox(fontdesc->font, bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	/* Encoding */

	for (i = 0; i < 256; i++)
		estrings[i] = nil;

	encoding = fz_dictgets(dict, "Encoding");
	if (!encoding)
	{
		error = fz_throw("syntaxerror: Type3 font missing Encoding");
		goto cleanup;
	}

	if (fz_isname(encoding))
		pdf_loadencoding(estrings, fz_toname(encoding));

	if (fz_isdict(encoding))
	{
		fz_obj *base, *diff, *item;

		base = fz_dictgets(encoding, "BaseEncoding");
		if (fz_isname(base))
			pdf_loadencoding(estrings, fz_toname(base));

		diff = fz_dictgets(encoding, "Differences");
		if (fz_isarray(diff))
		{
			n = fz_arraylen(diff);
			k = 0;
			for (i = 0; i < n; i++)
			{
				item = fz_arrayget(diff, i);
				if (fz_isint(item))
					k = fz_toint(item);
				if (fz_isname(item))
					estrings[k++] = fz_toname(item);
				if (k < 0) k = 0;
				if (k > 255) k = 255;
			}
		}
	}

	fontdesc->encoding = pdf_newidentitycmap(0, 1);

	error = pdf_loadtounicode(fontdesc, xref, estrings, nil, fz_dictgets(dict, "ToUnicode"));
	if (error)
		goto cleanup;

	/* Widths */

	pdf_setdefaulthmtx(fontdesc, 0);

	first = fz_toint(fz_dictgets(dict, "FirstChar"));
	last = fz_toint(fz_dictgets(dict, "LastChar"));

	widths = fz_dictgets(dict, "Widths");
	if (!widths)
	{
		error = fz_throw("syntaxerror: Type3 font missing Widths");
		goto cleanup;
	}

	for (i = first; i <= last; i++)
	{
		float w = fz_toreal(fz_arrayget(widths, i - first));
		w = fontdesc->font->t3matrix.a * w * 1000;
		fontdesc->font->t3widths[i] = w * 0.001f;
		pdf_addhmtx(fontdesc, i, i, w);
	}

	pdf_endhmtx(fontdesc);

	/* Resources -- inherit page resources if the font doesn't have its own */

	fontdesc->font->t3resources = fz_dictgets(dict, "Resources");
	if (!fontdesc->font->t3resources)
		fontdesc->font->t3resources = rdb;
	if (fontdesc->font->t3resources)
		fz_keepobj(fontdesc->font->t3resources);
	if (!fontdesc->font->t3resources)
		fz_warn("no resource dictionary for type 3 font!");

	fontdesc->font->t3xref = xref;
	fontdesc->font->t3run = pdf_runglyph;

	/* CharProcs */

	charprocs = fz_dictgets(dict, "CharProcs");
	if (!charprocs)
	{
		error = fz_throw("syntaxerror: Type3 font missing CharProcs");
		goto cleanup;
	}

	for (i = 0; i < 256; i++)
	{
		if (estrings[i])
		{
			obj = fz_dictgets(charprocs, estrings[i]);
			if (pdf_isstream(xref, fz_tonum(obj), fz_togen(obj)))
			{
				error = pdf_loadstream(&fontdesc->font->t3procs[i], xref, fz_tonum(obj), fz_togen(obj));
				if (error)
					goto cleanup;
			}
		}
	}

	pdf_logfont("}\n");

	*fontdescp = fontdesc;
	return fz_okay;

cleanup:
	fz_dropfont(fontdesc->font);
	fz_free(fontdesc);
	return fz_rethrow(error, "cannot load type3 font (%d %d R)", fz_tonum(dict), fz_togen(dict));
}

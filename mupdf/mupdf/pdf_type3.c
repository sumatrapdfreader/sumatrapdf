#include "fitz.h"
#include "mupdf.h"

static fz_error
loadcharproc(fz_tree **treep, pdf_xref *xref, fz_obj *rdb, fz_obj *stmref)
{
	fz_error error;
	pdf_csi *csi;
	fz_stream *stm;

	error = pdf_newcsi(&csi, 1);
	if (error)
		return fz_rethrow(error, "cannot create interpreter");

	error = pdf_openstream(&stm, xref, fz_tonum(stmref), fz_togen(stmref));
	if (error)
	{
		pdf_dropcsi(csi);
		return fz_rethrow(error, "cannot open glyph content stream");
	}

	error = pdf_runcsi(csi, xref, rdb, stm);
	if (error)
	{
		fz_dropstream(stm);
		pdf_dropcsi(csi);
		return fz_rethrow(error, "cannot interpret glyph content stream");
	}

	*treep = csi->tree;
	csi->tree = nil;

	fz_dropstream(stm);
	pdf_dropcsi(csi);
	return fz_okay;
}

fz_error
pdf_loadtype3font(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *dict, fz_obj *ref)
{
	fz_error error;
	char buf[256];
	char *estrings[256];
	pdf_fontdesc *fontdesc;
	fz_obj *encoding;
	fz_obj *widths;
	fz_obj *resources;
	fz_obj *charprocs;
	fz_obj *obj;
	int first, last;
	int i, k, n;
	fz_rect bbox;
	fz_matrix matrix;

	obj = fz_dictgets(dict, "Name");
	if (obj)
		strlcpy(buf, fz_toname(obj), sizeof buf);
	else
		sprintf(buf, "Unnamed-T3");

	fontdesc = pdf_newfontdesc();
	if (!fontdesc)
		return fz_rethrow(-1, "out of memory: font struct");

	pdf_logfont("load type3 font (%d %d R) ptr=%p {\n", fz_tonum(ref), fz_togen(ref), fontdesc);
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

	bbox = fz_transformaabb(matrix, bbox);
	bbox.x0 = fz_floor(bbox.x0 * 1000);
	bbox.y0 = fz_floor(bbox.y0 * 1000);
	bbox.x1 = fz_ceil(bbox.x1 * 1000);
	bbox.y1 = fz_ceil(bbox.y1 * 1000);

	error = fz_newtype3font(&fontdesc->font, buf, matrix);
	if (error)
	{
	    pdf_dropfont(fontdesc);
	    return fz_rethrow(error, "could not create type3 font");
	}

	fz_setfontbbox(fontdesc->font, bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	/*
	 * Encoding
	 */

	for (i = 0; i < 256; i++)
		estrings[i] = nil;

	encoding = fz_dictgets(dict, "Encoding");
	if (!encoding) {
		error = fz_throw("syntaxerror: Type3 font missing Encoding");
		goto cleanup;
	}

	error = pdf_resolve(&encoding, xref);
	if (error)
		goto cleanup;

	if (fz_isname(obj))
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

	fz_dropobj(encoding);

	error = pdf_newidentitycmap(&fontdesc->encoding, 0, 1);
	if (error)
		goto cleanup;

	error = pdf_loadtounicode(fontdesc, xref,
			estrings, nil, fz_dictgets(dict, "ToUnicode"));
	if (error)
		goto cleanup;

	/*
	 * Widths
	 */

	pdf_setdefaulthmtx(fontdesc, 0);

	first = fz_toint(fz_dictgets(dict, "FirstChar"));
	last = fz_toint(fz_dictgets(dict, "LastChar"));

	widths = fz_dictgets(dict, "Widths");
	if (!widths) {
		error = fz_throw("syntaxerror: Type3 font missing Widths");
		goto cleanup;
	}

	error = pdf_resolve(&widths, xref);
	if (error)
		goto cleanup;

	for (i = first; i <= last; i++)
	{
		float w = fz_toreal(fz_arrayget(widths, i - first));
		w = fontdesc->font->t3matrix.a * w * 1000.0;
		error = pdf_addhmtx(fontdesc, i, i, w);
		if (error) {
			fz_dropobj(widths);
			goto cleanup;
		}
	}

	fz_dropobj(widths);

	error = pdf_endhmtx(fontdesc);
	if (error)
		goto cleanup;

	/*
	 * Resources
	 */

	resources = nil;

	obj = fz_dictgets(dict, "Resources");
	if (obj)
	{
		error = pdf_resolve(&obj, xref);
		if (error)
			goto cleanup;

		error = pdf_loadresources(&resources, xref, obj);

		fz_dropobj(obj);

		if (error)
			goto cleanup;
	}
	else
		fz_warn("no resource dictionary for type 3 font!");

	/*
	 * CharProcs
	 */

	charprocs = fz_dictgets(dict, "CharProcs");
	if (!charprocs)
	{
		error = fz_throw("syntaxerror: Type3 font missing CharProcs");
		goto cleanup2;
	}

	error = pdf_resolve(&charprocs, xref);
	if (error)
		goto cleanup2;

	for (i = 0; i < 256; i++)
	{
		if (estrings[i])
		{
			obj = fz_dictgets(charprocs, estrings[i]);
			if (obj)
			{
				pdf_logfont("load charproc %s {\n", estrings[i]);
				error = loadcharproc(&fontdesc->font->t3procs[i], xref, resources, obj);
				if (error)
					goto cleanup2;

				error = fz_optimizetree(fontdesc->font->t3procs[i]);
				if (error)
					goto cleanup2;

				pdf_logfont("}\n");
			}
		}
	}

	fz_dropobj(charprocs);
	if (resources)
		fz_dropobj(resources);

	pdf_logfont("}\n");

	*fontdescp = fontdesc;
	return fz_okay;

cleanup2:
	if (resources)
		fz_dropobj(resources);
cleanup:
	fz_dropfont(fontdesc->font);
	fz_free(fontdesc);
	return fz_rethrow(error, "cannot load type3 font");
}


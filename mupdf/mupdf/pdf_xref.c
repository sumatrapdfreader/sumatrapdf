#include "fitz.h"
#include "mupdf.h"

void
pdf_closexref(pdf_xref *xref)
{
	pdf_logxref("closexref %p\n", xref);

	/* don't touch the pdf_store module ... we don't want that dependency here */
	if (xref->store)
		fz_warn("someone forgot to empty the store before freeing xref!");

	if (xref->table)
	{
		pdf_flushxref(xref, 1);
		fz_free(xref->table);
	}

	if (xref->file)
		fz_dropstream(xref->file);
	if (xref->trailer)
		fz_dropobj(xref->trailer);
	if (xref->crypt)
		pdf_freecrypt(xref->crypt);

	fz_free(xref);
}

void
pdf_flushxref(pdf_xref *xref, int force)
{
	int i;

	pdf_logxref("flushxref %p (%d)\n", xref, force);

	for (i = 0; i < xref->len; i++)
	{
		if (force)
		{
			if (xref->table[i].obj)
			{
				fz_dropobj(xref->table[i].obj);
				xref->table[i].obj = nil;
			}
		}
		else
		{
			if (xref->table[i].obj && xref->table[i].obj->refs == 1)
			{
				fz_dropobj(xref->table[i].obj);
				xref->table[i].obj = nil;
			}
		}
	}
}

void
pdf_debugxref(pdf_xref *xref)
{
	int i;
	printf("xref\n0 %d\n", xref->len);
	for (i = 0; i < xref->len; i++)
	{
		printf("%010d %05d %c (ref=%d, ofs=%d)\n",
			xref->table[i].ofs,
			xref->table[i].gen,
			xref->table[i].type,
			xref->table[i].obj ? xref->table[i].obj->refs : 0,
			xref->table[i].stmofs);
	}
}

/*
 * compressed object streams
 */

static fz_error
pdf_loadobjstm(pdf_xref *xref, int oid, int gen, char *buf, int cap)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *objstm;
	int *oidbuf;
	int *ofsbuf;

	fz_obj *obj;
	int first;
	int count;
	int i, n;
	pdf_token_e tok;

	pdf_logxref("loadobjstm (%d %d R)\n", oid, gen);

	error = pdf_loadobject(&objstm, xref, oid, gen);
	if (error)
		return fz_rethrow(error, "cannot load object stream object");

	count = fz_toint(fz_dictgets(objstm, "N"));
	first = fz_toint(fz_dictgets(objstm, "First"));

	pdf_logxref("  count %d\n", count);

	oidbuf = fz_malloc(count * sizeof(int));
	ofsbuf = fz_malloc(count * sizeof(int));

	error = pdf_openstream(&stm, xref, oid, gen);
	if (error)
	{
		error = fz_rethrow(error, "cannot open object stream");
		goto cleanupbuf;
	}

	for (i = 0; i < count; i++)
	{
		error = pdf_lex(&tok, stm, buf, cap, &n);
		if (error || tok != PDF_TINT)
		{
			error = fz_rethrow(error, "corrupt object stream");
			goto cleanupstm;
		}
		oidbuf[i] = atoi(buf);

		error = pdf_lex(&tok, stm, buf, cap, &n);
		if (error || tok != PDF_TINT)
		{
			error = fz_rethrow(error, "corrupt object stream");
			goto cleanupstm;
		}
		ofsbuf[i] = atoi(buf);
	}

	error = fz_seek(stm, first, 0);
	if (error)
	{
		error = fz_rethrow(error, "cannot seek in object stream");
		goto cleanupstm;
	}

	for (i = 0; i < count; i++)
	{
		/* FIXME: seek to first + ofsbuf[i] */

		error = pdf_parsestmobj(&obj, xref, stm, buf, cap);
		if (error)
		{
			error = fz_rethrow(error, "cannot parse object %d in stream", i);
			goto cleanupstm;
		}

		if (oidbuf[i] < 1 || oidbuf[i] >= xref->len)
		{
			fz_dropobj(obj);
			error = fz_throw("object id (%d 0 R) out of range (0..%d)", oidbuf[i], xref->len - 1);
			goto cleanupstm;
		}

		if (xref->table[oidbuf[i]].obj)
			fz_dropobj(xref->table[oidbuf[i]].obj);
		xref->table[oidbuf[i]].obj = obj;
	}

	fz_dropstream(stm);
	fz_free(ofsbuf);
	fz_free(oidbuf);
	fz_dropobj(objstm);
	return fz_okay;

cleanupstm:
	fz_dropstream(stm);
cleanupbuf:
	fz_free(ofsbuf);
	fz_free(oidbuf);
	fz_dropobj(objstm);
	return error; /* already rethrown */
}

/*
 * object loading
 */

fz_error
pdf_cacheobject(pdf_xref *xref, int oid, int gen)
{
	fz_error error;
	pdf_xrefentry *x;
	int roid, rgen;

	if (oid < 0 || oid >= xref->len)
		return fz_throw("object out of range (%d %d R); xref size %d", oid, gen, xref->len);

	x = &xref->table[oid];

	if (x->obj)
		return fz_okay;

	if (x->type == 'f' || x->type == 'd')
	{
		x->obj = fz_newnull();
		return fz_okay;
	}

	if (x->type == 'n')
	{
		error = fz_seek(xref->file, x->ofs, 0);
		if (error)
			return fz_rethrow(error, "cannot seek to object (%d %d R) offset %d", oid, gen, x->ofs);

		error = pdf_parseindobj(&x->obj, xref, xref->file, xref->scratch, sizeof xref->scratch,
			&roid, &rgen, &x->stmofs);
		if (error)
			return fz_rethrow(error, "cannot parse object (%d %d R)", oid, gen);

		if (roid != oid)
			return fz_throw("found object (%d %d R) instead of (%d %d R)", roid, rgen, oid, gen);

		if (xref->crypt)
			pdf_cryptobj(xref->crypt, x->obj, oid, gen);
	}

	else if (x->type == 'o')
	{
		if (!x->obj)
		{
			error = pdf_loadobjstm(xref, x->ofs, 0, xref->scratch, sizeof xref->scratch);
			if (error)
				return fz_rethrow(error, "cannot load object stream containing object (%d %d R)", oid, gen);
		}
	}

	return fz_okay;
}

fz_error
pdf_loadobject(fz_obj **objp, pdf_xref *xref, int oid, int gen)
{
	fz_error error;

	error = pdf_cacheobject(xref, oid, gen);
	if (error)
		return fz_rethrow(error, "cannot load object (%d %d R) into cache", oid, gen);

	if (xref->table[oid].obj)
		*objp = fz_keepobj(xref->table[oid].obj);
	else
	{
		fz_warn("cannot load missing object (%d %d R), assuming null object", oid, gen);
		xref->table[oid].obj = fz_newnull();
	}

	return fz_okay;
}


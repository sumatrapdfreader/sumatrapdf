#include "fitz.h"
#include "mupdf.h"

void
pdf_closexref(pdf_xref *xref)
{
	int i;

	pdf_logxref("closexref %p\n", xref);

	/* don't touch the pdf_store module ... we don't want that dependency here */
	if (xref->store)
		fz_warn("someone forgot to empty the store before freeing xref!");

	if (xref->table)
	{
		pdf_flushxref(xref, 1);
		fz_free(xref->table);
	}

	if (xref->pageobjs)
	{
		for (i = 0; i < xref->pagelen; i++)
			fz_dropobj(xref->pageobjs[i]);
		fz_free(xref->pageobjs);
	}
	if (xref->pagerefs)
	{
		for (i = 0; i < xref->pagelen; i++)
			fz_dropobj(xref->pagerefs[i]);
		fz_free(xref->pagerefs);
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
		printf("%05d: %010d %05d %c (refs=%d, stmofs=%d)\n", i,
			xref->table[i].ofs,
			xref->table[i].gen,
			xref->table[i].type ? xref->table[i].type : '-',
			xref->table[i].obj ? xref->table[i].obj->refs : 0,
			xref->table[i].stmofs);
	}
}

/*
 * compressed object streams
 */

static fz_error
pdf_loadobjstm(pdf_xref *xref, int num, int gen, char *buf, int cap)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *objstm;
	int *numbuf;
	int *ofsbuf;

	fz_obj *obj;
	int first;
	int count;
	int i, n;
	pdf_token_e tok;

	pdf_logxref("loadobjstm (%d %d R)\n", num, gen);

	error = pdf_loadobject(&objstm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load object stream object (%d %d R)", num, gen);

	count = fz_toint(fz_dictgets(objstm, "N"));
	first = fz_toint(fz_dictgets(objstm, "First"));

	pdf_logxref("\tcount %d\n", count);

	numbuf = fz_malloc(count * sizeof(int));
	ofsbuf = fz_malloc(count * sizeof(int));

	error = pdf_openstream(&stm, xref, num, gen);
	if (error)
	{
		error = fz_rethrow(error, "cannot open object stream (%d %d R)", num, gen);
		goto cleanupbuf;
	}

	for (i = 0; i < count; i++)
	{
		error = pdf_lex(&tok, stm, buf, cap, &n);
		if (error || tok != PDF_TINT)
		{
			error = fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
			goto cleanupstm;
		}
		numbuf[i] = atoi(buf);

		error = pdf_lex(&tok, stm, buf, cap, &n);
		if (error || tok != PDF_TINT)
		{
			error = fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
			goto cleanupstm;
		}
		ofsbuf[i] = atoi(buf);
	}

	error = fz_seek(stm, first, 0);
	if (error)
	{
		error = fz_rethrow(error, "cannot seek in object stream (%d %d R)", num, gen);
		goto cleanupstm;
	}

	for (i = 0; i < count; i++)
	{
		error = fz_seek(stm, first + ofsbuf[i], 0);
		if (error)
		{
			error = fz_rethrow(error, "cannot seek in object stream (%d %d R)", num, gen);
			goto cleanupstm;
		}

		error = pdf_parsestmobj(&obj, xref, stm, buf, cap);
		if (error)
		{
			error = fz_rethrow(error, "cannot parse object %d in stream (%d %d R)", i, num, gen);
			goto cleanupstm;
		}

		if (numbuf[i] < 1 || numbuf[i] >= xref->len)
		{
			fz_dropobj(obj);
			error = fz_throw("object id (%d 0 R) out of range (0..%d)", numbuf[i], xref->len - 1);
			goto cleanupstm;
		}

		if (xref->table[numbuf[i]].type == 'o' && xref->table[numbuf[i]].ofs == num)
		{
			if (xref->table[numbuf[i]].obj)
				fz_dropobj(xref->table[numbuf[i]].obj);
			xref->table[numbuf[i]].obj = obj;
		}
		else
		{
			fz_dropobj(obj);
		}
	}

	fz_dropstream(stm);
	fz_free(ofsbuf);
	fz_free(numbuf);
	fz_dropobj(objstm);
	return fz_okay;

cleanupstm:
	fz_dropstream(stm);
cleanupbuf:
	fz_free(ofsbuf);
	fz_free(numbuf);
	fz_dropobj(objstm);
	return error; /* already rethrown */
}

/*
 * object loading
 */

fz_error
pdf_cacheobject(pdf_xref *xref, int num, int gen)
{
	fz_error error;
	pdf_xrefentry *x;
	int rnum, rgen;

	if (num < 0 || num >= xref->len)
		return fz_throw("object out of range (%d %d R); xref size %d", num, gen, xref->len);

	x = &xref->table[num];

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
			return fz_rethrow(error, "cannot seek to object (%d %d R) offset %d", num, gen, x->ofs);

		error = pdf_parseindobj(&x->obj, xref, xref->file, xref->scratch, sizeof xref->scratch,
			&rnum, &rgen, &x->stmofs);
		if (error)
			return fz_rethrow(error, "cannot parse object (%d %d R)", num, gen);

		if (rnum != num)
			return fz_throw("found object (%d %d R) instead of (%d %d R)", rnum, rgen, num, gen);

		if (xref->crypt)
			pdf_cryptobj(xref->crypt, x->obj, num, gen);
	}

	else if (x->type == 'o')
	{
		if (!x->obj)
		{
			error = pdf_loadobjstm(xref, x->ofs, 0, xref->scratch, sizeof xref->scratch);
			if (error)
				return fz_rethrow(error, "cannot load object stream containing object (%d %d R)", num, gen);
		}
	}

	return fz_okay;
}

fz_error
pdf_loadobject(fz_obj **objp, pdf_xref *xref, int num, int gen)
{
	fz_error error;

	error = pdf_cacheobject(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load object (%d %d R) into cache", num, gen);

	if (xref->table[num].obj)
		*objp = fz_keepobj(xref->table[num].obj);
	else
	{
		fz_warn("cannot load missing object (%d %d R), assuming null object", num, gen);
		xref->table[num].obj = fz_newnull();
		*objp = fz_keepobj(xref->table[num].obj);
	}

	return fz_okay;
}


#include "fitz.h"
#include "mupdf.h"

/*
 * create xref structure.
 * needs to be initialized by initxref, openxref or repairxref.
 */

pdf_xref *
pdf_newxref(void)
{
	pdf_xref *xref;

	xref = fz_malloc(sizeof(pdf_xref));

	memset(xref, 0, sizeof(pdf_xref));

	pdf_logxref("newxref %p\n", xref);

	xref->file = nil;
	xref->version = 13;
	xref->startxref = 0;
	xref->crypt = nil;

	xref->trailer = nil;
	xref->root = nil;
	xref->info = nil;
	xref->store = nil;

	xref->cap = 0;
	xref->len = 0;
	xref->table = nil;

	xref->store = nil;	/* you need to create this if you want to render */

	return xref;
}

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
	if (xref->root)
		fz_dropobj(xref->root);
	if (xref->info)
		fz_dropobj(xref->info);
	if (xref->crypt)
		pdf_freecrypt(xref->crypt);

	fz_free(xref);
}

fz_error
pdf_initxref(pdf_xref *xref)
{
	xref->table = fz_malloc(sizeof(pdf_xrefentry) * 128);
	xref->cap = 128;
	xref->len = 1;

	xref->crypt = nil;

	xref->table[0].type = 'f';
	xref->table[0].ofs = 0;
	xref->table[0].gen = 65535;
	xref->table[0].stmofs = 0;
	xref->table[0].obj = nil;

	return fz_okay;
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

fz_error
pdf_decryptxref(pdf_xref *xref)
{
	fz_error error;
	fz_obj *encrypt;
	fz_obj *id;

	encrypt = fz_dictgets(xref->trailer, "Encrypt");
	id = fz_dictgets(xref->trailer, "ID");

	if (encrypt)
	{
		if (fz_isnull(encrypt))
			return fz_okay;

		error = pdf_newcrypt(&xref->crypt, encrypt, id);
		if (error)
			return fz_rethrow(error, "cannot create decrypter");
	}

	return fz_okay;
}

/*
 * object loading
 */

fz_error
pdf_cacheobject(pdf_xref *xref, int oid, int gen)
{
	char buf[65536];	/* yeowch! */

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

		error = pdf_parseindobj(&x->obj, xref, xref->file, buf, sizeof buf, &roid, &rgen, &x->stmofs);
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
			error = pdf_loadobjstm(xref, x->ofs, 0, buf, sizeof buf);
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


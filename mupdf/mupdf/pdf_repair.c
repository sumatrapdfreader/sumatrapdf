#include "fitz.h"
#include "mupdf.h"

/*
 * open pdf and scan objects to reconstruct xref table
 */

struct entry
{
	int oid;
	int gen;
	int ofs;
	int stmofs;
	int stmlen;
};

static fz_error
fz_repairobj(fz_stream *file, char *buf, int cap,
	int *stmofsp, int *stmlenp, int *isroot, int *isinfo)
{
	fz_error error;
	pdf_token_e tok;
	int stmlen;
	int len;
	int n;

	*stmofsp = 0;
	*stmlenp = -1;
	*isroot = 0;
	*isinfo = 0;

	stmlen = 0;

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (tok == PDF_TODICT)
	{
		fz_obj *dict, *obj;

		/* Send nil xref so we don't try to resolve references */
		error = pdf_parsedict(&dict, nil, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse object");

		obj = fz_dictgets(dict, "Type");
		if (fz_isname(obj) && !strcmp(fz_toname(obj), "Catalog"))
			*isroot = 1;

		if (fz_dictgets(dict, "Producer"))
			if (fz_dictgets(dict, "Creator"))
				if (fz_dictgets(dict, "Title"))
					*isinfo = 1;

		obj = fz_dictgets(dict, "Length");
		if (fz_isint(obj))
			stmlen = fz_toint(obj);

		obj = fz_dictgets(dict, "Filter");
		if (fz_isname(obj) && !strcmp(fz_toname(obj), "Standard"))
		{
			fz_dropobj(dict);
			return fz_throw("cannot repair encrypted files");
		}

		fz_dropobj(dict);
	}

	while ( tok != PDF_TSTREAM &&
		tok != PDF_TENDOBJ &&
		tok != PDF_TERROR &&
		tok != PDF_TEOF )
	{
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
			return fz_rethrow(error, "cannot scan for endobj or stream token");
	}

	if (tok == PDF_TSTREAM)
	{
		int c = fz_readbyte(file);
		if (c == '\r') {
			c = fz_peekbyte(file);
			if (c == '\n')
				fz_readbyte(file);
		}

		error = fz_readerror(file);
		if (error)
			return fz_rethrow(error, "cannot read from file");

		*stmofsp = fz_tell(file);
		if (*stmofsp < 0)
			return fz_throw("cannot seek in file");

		if (stmlen > 0)
		{
			error = fz_seek(file, *stmofsp + stmlen, 0);
			if (error)
				return fz_rethrow(error, "cannot seek in file");
			error = pdf_lex(&tok, file, buf, cap, &len);
			if (error)
				return fz_rethrow(error, "cannot scan for endstream token");
			if (tok == PDF_TENDSTREAM)
				goto atobjend;
			error = fz_seek(file, *stmofsp, 0);
			if (error)
				return fz_rethrow(error, "cannot seek in file");
		}

		error = fz_read(&n, file, (unsigned char *) buf, 9);
		if (error)
			return fz_rethrow(error, "cannot read from file");

		while (memcmp(buf, "endstream", 9) != 0)
		{
			c = fz_readbyte(file);
			if (c == EOF)
				break;
			memmove(buf, buf + 1, 8);
			buf[8] = c;
		}

		error = fz_readerror(file);
		if (error)
			return fz_rethrow(error, "cannot read from file");

		*stmlenp = fz_tell(file) - *stmofsp - 9;

atobjend:
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
			return fz_rethrow(error, "cannot scan for endobj token");
		if (tok == PDF_TENDOBJ)
			;
	}

	return fz_okay;
}

fz_error
pdf_repairxref(pdf_xref *xref, char *filename)
{
	fz_error error;
	fz_stream *file;

	struct entry *list = nil;
	int listlen;
	int listcap;
	int maxoid = 0;

	char buf[65536];

	int oid = 0;
	int gen = 0;
	int tmpofs, oidofs = 0, genofs = 0;
	int isroot, rootoid = 0, rootgen = 0;
	int isinfo, infooid = 0, infogen = 0;
	int stmlen, stmofs = 0;
	pdf_token_e tok;
	int len;
	int next;
	int i;

	error = fz_openrfile(&file, filename);
	if (error)
		return fz_rethrow(error, "cannot open file '%s'", filename);

	pdf_logxref("repairxref '%s' %p\n", filename, xref);

	xref->file = file;

	/* TODO: extract version */

	listlen = 0;
	listcap = 1024;
	list = fz_malloc(listcap * sizeof(struct entry));

	while (1)
	{
		tmpofs = fz_tell(file);
		if (tmpofs < 0)
		{
			error = fz_throw("cannot tell in file");
			goto cleanup;
		}

		error = pdf_lex(&tok, file, buf, sizeof buf, &len);
		if (error)
		{
			error = fz_rethrow(error, "cannot scan for objects");
			goto cleanup;
		}

		if (tok == PDF_TINT)
		{
			oidofs = genofs;
			oid = gen;
			genofs = tmpofs;
			gen = atoi(buf);
		}

		if (tok == PDF_TOBJ)
		{
			error = fz_repairobj(file, buf, sizeof buf, &stmofs, &stmlen, &isroot, &isinfo);
			if (error)
			{
				error = fz_rethrow(error, "cannot parse object (%d %d R)", oid, gen);
				goto cleanup;
			}

			if (isroot) {
				pdf_logxref("found catalog: (%d %d R)\n", oid, gen);
				rootoid = oid;
				rootgen = gen;
			}

			if (isinfo) {
				pdf_logxref("found info: (%d %d R)\n", oid, gen);
				infooid = oid;
				infogen = gen;
			}

			if (listlen + 1 == listcap)
			{
				listcap = (listcap * 3) / 2;
				list = fz_realloc(list, listcap * sizeof(struct entry));
			}

			pdf_logxref("found object: (%d %d R)\n", oid, gen);

			list[listlen].oid = oid;
			list[listlen].gen = gen;
			list[listlen].ofs = oidofs;
			list[listlen].stmofs = stmofs;
			list[listlen].stmlen = stmlen;
			listlen ++;

			if (oid > maxoid)
				maxoid = oid;
		}

		if (tok == PDF_TERROR)
			fz_readbyte(file);

		if (tok == PDF_TEOF)
			break;
	}

	if (rootoid == 0)
	{
		error = fz_throw("cannot find catalog object");
		goto cleanup;
	}

	error = fz_packobj(&xref->trailer, xref,
		"<< /Size %i /Root %r >>",
		maxoid + 1, rootoid, rootgen);
	if (error)
	{
		error = fz_rethrow(error, "cannot create new trailer");
		goto cleanup;
	}

	xref->len = maxoid + 1;
	xref->cap = xref->len;
	xref->table = fz_malloc(xref->cap * sizeof(pdf_xrefentry));

	xref->table[0].type = 'f';
	xref->table[0].ofs = 0;
	xref->table[0].gen = 65535;
	xref->table[0].stmofs = 0;
	xref->table[0].obj = nil;

	for (i = 1; i < xref->len; i++)
	{
		xref->table[i].type = 'f';
		xref->table[i].ofs = 0;
		xref->table[i].gen = 0;
		xref->table[i].stmofs = 0;
		xref->table[i].obj = nil;
	}

	for (i = 0; i < listlen; i++)
	{
		xref->table[list[i].oid].type = 'n';
		xref->table[list[i].oid].ofs = list[i].ofs;
		xref->table[list[i].oid].gen = list[i].gen;

		xref->table[list[i].oid].stmofs = list[i].stmofs;

		/* corrected stream length */
		if (list[i].stmlen >= 0)
		{
			fz_obj *dict, *length;

			pdf_logxref("correct stream length %d %d = %d\n",
				list[i].oid, list[i].gen, list[i].stmlen);

			error = pdf_loadobject(&dict, xref, list[i].oid, list[i].gen);
			if (error)
			{
				error = fz_rethrow(error, "cannot load stream object");
				goto cleanup;
			}

			length = fz_newint(list[i].stmlen);
			fz_dictputs(dict, "Length", length);
			fz_dropobj(length);

			fz_dropobj(dict);
		}
	}

	next = 0;
	for (i = xref->len - 1; i >= 0; i--)
	{
		if (xref->table[i].type == 'f')
		{
			xref->table[i].ofs = next;
			if (xref->table[i].gen < 65535)
				xref->table[i].gen ++;
			next = i;
		}
	}

	fz_free(list);
	return fz_okay;

cleanup:
	fz_dropstream(file);
	xref->file = nil; /* don't keep the stale pointer */
	fz_free(list);
	return error; /* already rethrown */
}


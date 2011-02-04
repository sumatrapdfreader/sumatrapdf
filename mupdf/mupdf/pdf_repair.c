#include "fitz.h"
#include "mupdf.h"

/* Scan file for objects and reconstruct xref table */

struct entry
{
	int num;
	int gen;
	int ofs;
	int stmofs;
	int stmlen;
};

static fz_error
fz_repairobj(fz_stream *file, char *buf, int cap, int *stmofsp, int *stmlenp, fz_obj **encrypt, fz_obj **id)
{
	fz_error error;
	int tok;
	int stmlen;
	int len;
	int n;

	*stmofsp = 0;
	*stmlenp = -1;

	stmlen = 0;

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse object");
	if (tok == PDF_TODICT)
	{
		fz_obj *dict, *obj;

		/* Send nil xref so we don't try to resolve references */
		error = pdf_parsedict(&dict, nil, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse object");

		obj = fz_dictgets(dict, "Type");
		if (fz_isname(obj) && !strcmp(fz_toname(obj), "XRef"))
		{
			obj = fz_dictgets(dict, "Encrypt");
			if (obj)
			{
				if (*encrypt)
					fz_dropobj(*encrypt);
				*encrypt = fz_keepobj(obj);
			}

			obj = fz_dictgets(dict, "ID");
			if (obj)
			{
				if (*id)
					fz_dropobj(*id);
				*id = fz_keepobj(obj);
			}
		}

		obj = fz_dictgets(dict, "Length");
		if (fz_isint(obj))
			stmlen = fz_toint(obj);

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

		*stmofsp = fz_tell(file);
		if (*stmofsp < 0)
			return fz_throw("cannot seek in file");

		if (stmlen > 0)
		{
			fz_seek(file, *stmofsp + stmlen, 0);
			error = pdf_lex(&tok, file, buf, cap, &len);
			if (error)
				fz_catch(error, "cannot find endstream token, falling back to scanning");
			if (tok == PDF_TENDSTREAM)
				goto atobjend;
			fz_seek(file, *stmofsp, 0);
		}

		n = fz_read(file, (unsigned char *) buf, 9);
		if (n < 0)
			return fz_rethrow(n, "cannot read from file");

		while (memcmp(buf, "endstream", 9) != 0)
		{
			c = fz_readbyte(file);
			if (c == EOF)
				break;
			memmove(buf, buf + 1, 8);
			buf[8] = c;
		}

		*stmlenp = fz_tell(file) - *stmofsp - 9;

atobjend:
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
			return fz_rethrow(error, "cannot scan for endobj token");
		if (tok != PDF_TENDOBJ)
			fz_warn("object missing 'endobj' token");
	}

	return fz_okay;
}

static fz_error
pdf_repairobjstm(pdf_xref *xref, int num, int gen)
{
	fz_error error;
	fz_obj *obj;
	fz_stream *stm;
	int tok;
	int i, n, count;
	char buf[256];

	error = pdf_loadobject(&obj, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load object stream object (%d %d R)", num, gen);

	count = fz_toint(fz_dictgets(obj, "N"));

	fz_dropobj(obj);

	error = pdf_openstream(&stm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot open object stream object (%d %d R)", num, gen);

	for (i = 0; i < count; i++)
	{
		error = pdf_lex(&tok, stm, buf, sizeof buf, &n);
		if (error || tok != PDF_TINT)
		{
			fz_close(stm);
			return fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
		}

		n = atoi(buf);
		if (n >= xref->len)
			pdf_resizexref(xref, n + 1);

		xref->table[n].ofs = num;
		xref->table[n].gen = i;
		xref->table[n].stmofs = 0;
		xref->table[n].obj = nil;
		xref->table[n].type = 'o';

		error = pdf_lex(&tok, stm, buf, sizeof buf, &n);
		if (error || tok != PDF_TINT)
		{
			fz_close(stm);
			return fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
		}
	}

	fz_close(stm);
	return fz_okay;
}

fz_error
pdf_repairxref(pdf_xref *xref, char *buf, int bufsize)
{
	fz_error error;
	fz_obj *dict, *obj;
	fz_obj *length;

	fz_obj *encrypt = nil;
	fz_obj *id = nil;
	fz_obj *root = nil;
	fz_obj *info = nil;

	struct entry *list = nil;
	int listlen;
	int listcap;
	int maxnum = 0;

	int num = 0;
	int gen = 0;
	int tmpofs, numofs = 0, genofs = 0;
	int stmlen, stmofs = 0;
	int tok;
	int next;
	int i, n;

	pdf_logxref("repairxref %p\n", xref);

	fz_seek(xref->file, 0, 0);

	listlen = 0;
	listcap = 1024;
	list = fz_calloc(listcap, sizeof(struct entry));

	/* look for '%PDF' version marker within first kilobyte of file */
	n = fz_read(xref->file, (unsigned char *)buf, MAX(bufsize, 1024));
	if (n < 0)
	{
		error = fz_rethrow(n, "cannot read from file");
		goto cleanup;
	}

	fz_seek(xref->file, 0, 0);
	for (i = 0; i < n - 4; i++)
	{
		if (memcmp(buf + i, "%PDF", 4) == 0)
		{
			fz_seek(xref->file, i, 0);
			break;
		}
	}

	while (1)
	{
		tmpofs = fz_tell(xref->file);
		if (tmpofs < 0)
		{
			error = fz_throw("cannot tell in file");
			goto cleanup;
		}

		error = pdf_lex(&tok, xref->file, buf, bufsize, &n);
		if (error)
		{
			fz_catch(error, "ignoring the rest of the file");
			break;
		}

		if (tok == PDF_TINT)
		{
			numofs = genofs;
			num = gen;
			genofs = tmpofs;
			gen = atoi(buf);
		}

		if (tok == PDF_TOBJ)
		{
			error = fz_repairobj(xref->file, buf, bufsize, &stmofs, &stmlen, &encrypt, &id);
			if (error)
			{
				error = fz_rethrow(error, "cannot parse object (%d %d R)", num, gen);
				goto cleanup;
			}

			pdf_logxref("found object: (%d %d R)\n", num, gen);

			if (listlen + 1 == listcap)
			{
				listcap = (listcap * 3) / 2;
				list = fz_realloc(list, listcap, sizeof(struct entry));
			}

			list[listlen].num = num;
			list[listlen].gen = gen;
			list[listlen].ofs = numofs;
			list[listlen].stmofs = stmofs;
			list[listlen].stmlen = stmlen;
			listlen ++;

			if (num > maxnum)
				maxnum = num;
		}

		/* trailer dictionary */
		if (tok == PDF_TODICT)
		{
			error = pdf_parsedict(&dict, xref, xref->file, buf, bufsize);
			if (error)
			{
				error = fz_rethrow(error, "cannot parse object");
				goto cleanup;
			}

			obj = fz_dictgets(dict, "Encrypt");
			if (obj)
			{
				if (encrypt)
					fz_dropobj(encrypt);
				encrypt = fz_keepobj(obj);
			}

			obj = fz_dictgets(dict, "ID");
			if (obj)
			{
				if (id)
					fz_dropobj(id);
				id = fz_keepobj(obj);
			}

			obj = fz_dictgets(dict, "Root");
			if (obj)
			{
				if (root)
					fz_dropobj(root);
				root = fz_keepobj(obj);
			}

			obj = fz_dictgets(dict, "Info");
			if (obj)
			{
				if (info)
					fz_dropobj(info);
				info = fz_keepobj(obj);
			}

			fz_dropobj(dict);
		}

		if (tok == PDF_TERROR)
			fz_readbyte(xref->file);

		if (tok == PDF_TEOF)
			break;
	}

	/* make xref reasonable */

	pdf_resizexref(xref, maxnum + 1);

	for (i = 0; i < listlen; i++)
	{
		xref->table[list[i].num].type = 'n';
		xref->table[list[i].num].ofs = list[i].ofs;
		xref->table[list[i].num].gen = list[i].gen;

		xref->table[list[i].num].stmofs = list[i].stmofs;

		/* corrected stream length */
		if (list[i].stmlen >= 0)
		{
			pdf_logxref("correct stream length %d %d = %d\n",
				list[i].num, list[i].gen, list[i].stmlen);

			error = pdf_loadobject(&dict, xref, list[i].num, list[i].gen);
			if (error)
			{
				error = fz_rethrow(error, "cannot load stream object (%d %d R)", list[i].num, list[i].gen);
				goto cleanup;
			}

			length = fz_newint(list[i].stmlen);
			fz_dictputs(dict, "Length", length);
			fz_dropobj(length);

			fz_dropobj(dict);
		}

	}

	xref->table[0].type = 'f';
	xref->table[0].ofs = 0;
	xref->table[0].gen = 65535;
	xref->table[0].stmofs = 0;
	xref->table[0].obj = nil;

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

	/* create a repaired trailer, Root will be added later */

	xref->trailer = fz_newdict(5);

	obj = fz_newint(maxnum + 1);
	fz_dictputs(xref->trailer, "Size", obj);
	fz_dropobj(obj);

	if (root)
	{
		fz_dictputs(xref->trailer, "Root", root);
		fz_dropobj(root);
	}
	if (info)
	{
		fz_dictputs(xref->trailer, "Info", info);
		fz_dropobj(info);
	}

	if (encrypt)
	{
		if (fz_isindirect(encrypt))
		{
			/* create new reference with non-nil xref pointer */
			obj = fz_newindirect(fz_tonum(encrypt), fz_togen(encrypt), xref);
			fz_dropobj(encrypt);
			encrypt = obj;
		}
		fz_dictputs(xref->trailer, "Encrypt", encrypt);
		fz_dropobj(encrypt);
	}

	if (id)
	{
		if (fz_isindirect(id))
		{
			/* create new reference with non-nil xref pointer */
			obj = fz_newindirect(fz_tonum(id), fz_togen(id), xref);
			fz_dropobj(id);
			id = obj;
		}
		fz_dictputs(xref->trailer, "ID", id);
		fz_dropobj(id);
	}

	fz_free(list);
	return fz_okay;

cleanup:
	if (encrypt) fz_dropobj(encrypt);
	if (id) fz_dropobj(id);
	if (root) fz_dropobj(root);
	if (info) fz_dropobj(info);
	fz_free(list);
	return error; /* already rethrown */
}

fz_error
pdf_repairobjstms(pdf_xref *xref)
{
	fz_obj *dict;
	int i;

	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].stmofs)
		{
			pdf_loadobject(&dict, xref, i, 0);
			if (!strcmp(fz_toname(fz_dictgets(dict, "Type")), "ObjStm"))
				pdf_repairobjstm(xref, i, 0);
			fz_dropobj(dict);
		}
	}

	return fz_okay;
}

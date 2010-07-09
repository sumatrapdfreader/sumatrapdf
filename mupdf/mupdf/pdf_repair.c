#include "fitz.h"
#include "mupdf.h"

/*
 * open pdf and scan objects to reconstruct xref table
 */

struct entry
{
	int num;
	int gen;
	int ofs;
	int stmofs;
	int stmlen;
};

static fz_error
fz_repairobj(fz_stream *file, char *buf, int cap, int *stmofsp, int *stmlenp, int *isroot)
{
	fz_error error;
	pdf_token_e tok;
	int stmlen;
	int len;
	int n;

	*stmofsp = 0;
	*stmlenp = -1;
	*isroot = 0;

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
				fz_catch(error, "cannot find endstream token, falling back to scanning");
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
		if (tok != PDF_TENDOBJ)
			fz_warn("object missing 'endobj' token");
	}

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

	struct entry *list = nil;
	int listlen;
	int listcap;
	int maxnum = 0;

	int num = 0;
	int gen = 0;
	int tmpofs, numofs = 0, genofs = 0;
	int isroot, rootnum = 0, rootgen = 0;
	int stmlen, stmofs = 0;
	pdf_token_e tok;
	int len;
	int next;
	int i;

	pdf_logxref("repairxref %p\n", xref);

	error = fz_seek(xref->file, 0, 0);
	if (error)
		return fz_rethrow(error, "cannot seek to beginning of file");

	listlen = 0;
	listcap = 1024;
	list = fz_malloc(listcap * sizeof(struct entry));

	while (1)
	{
		tmpofs = fz_tell(xref->file);
		if (tmpofs < 0)
		{
			error = fz_throw("cannot tell in file");
			goto cleanup;
		}

		error = pdf_lex(&tok, xref->file, buf, bufsize, &len);
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
			error = fz_repairobj(xref->file, buf, bufsize, &stmofs, &stmlen, &isroot);
			if (error)
			{
				error = fz_rethrow(error, "cannot parse object (%d %d R)", num, gen);
				goto cleanup;
			}

			if (isroot) {
				pdf_logxref("found catalog: (%d %d R)\n", num, gen);
				rootnum = num;
				rootgen = gen;
			}

			if (listlen + 1 == listcap)
			{
				listcap = (listcap * 3) / 2;
				list = fz_realloc(list, listcap * sizeof(struct entry));
			}

			pdf_logxref("found object: (%d %d R)\n", num, gen);

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
				return fz_rethrow(error, "cannot parse object");

			obj = fz_dictgets(dict, "Encrypt");
			if (obj)
				encrypt = fz_keepobj(obj);

			obj = fz_dictgets(dict, "ID");
			if (obj)
				id = fz_keepobj(obj);

			fz_dropobj(dict);
		}

		if (tok == PDF_TERROR)
			fz_readbyte(xref->file);

		if (tok == PDF_TEOF)
			break;
	}

	if (rootnum == 0)
	{
		error = fz_throw("cannot find catalog object");
		goto cleanup;
	}

	xref->trailer = fz_newdict(4);

	obj = fz_newint(maxnum + 1);
	fz_dictputs(xref->trailer, "Size", obj);
	fz_dropobj(obj);

	obj = fz_newindirect(rootnum, rootgen, xref);
	fz_dictputs(xref->trailer, "Root", obj);
	fz_dropobj(obj);

	if (encrypt)
	{
		fz_dictputs(xref->trailer, "Encrypt", encrypt);
		fz_dropobj(encrypt);
	}

	if (id)
	{
		fz_dictputs(xref->trailer, "ID", id);
		fz_dropobj(id);
	}

	xref->len = maxnum + 1;
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
	fz_free(list);
	return error; /* already rethrown */
}

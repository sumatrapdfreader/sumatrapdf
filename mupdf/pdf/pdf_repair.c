#include "fitz.h"
#include "mupdf.h"

/* Scan file for objects and reconstruct xref table */

struct entry
{
	int num;
	int gen;
	int ofs;
	int stm_ofs;
	int stm_len;
};

static fz_error
pdf_repair_obj(fz_stream *file, char *buf, int cap, int *stmofsp, int *stmlenp, fz_obj **encrypt, fz_obj **id)
{
	fz_error error;
	int tok;
	int stm_len;
	int len;
	int n;

	*stmofsp = 0;
	*stmlenp = -1;

	stm_len = 0;

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse object");
	if (tok == PDF_TOK_OPEN_DICT)
	{
		fz_obj *dict, *obj;

		/* Send NULL xref so we don't try to resolve references */
		error = pdf_parse_dict(&dict, NULL, file, buf, cap);
		/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=692506 */
		if (error && !file->eof)
		{
			fz_catch(error, "cannot parse object, proceeding anyway");
			dict = fz_new_dict(2);
		}
		else
		if (error)
			return fz_rethrow(error, "cannot parse object");

		obj = fz_dict_gets(dict, "Type");
		if (fz_is_name(obj) && !strcmp(fz_to_name(obj), "XRef"))
		{
			obj = fz_dict_gets(dict, "Encrypt");
			if (obj)
			{
				if (*encrypt)
					fz_drop_obj(*encrypt);
				*encrypt = fz_keep_obj(obj);
			}

			obj = fz_dict_gets(dict, "ID");
			if (obj)
			{
				if (*id)
					fz_drop_obj(*id);
				*id = fz_keep_obj(obj);
			}
		}

		obj = fz_dict_gets(dict, "Length");
		if (fz_is_int(obj))
			stm_len = fz_to_int(obj);

		fz_drop_obj(dict);
	}

	/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=692424
	while ( tok != PDF_TOK_STREAM &&
		tok != PDF_TOK_ENDOBJ &&
		tok != PDF_TOK_ERROR &&
		tok != PDF_TOK_EOF )
	{
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
			return fz_rethrow(error, "cannot scan for endobj or stream token");
	}
	*/
	len = 0;
	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot scan for endobj or stream token");
	if (tok != PDF_TOK_STREAM && tok != PDF_TOK_ENDOBJ && tok != PDF_TOK_ERROR && tok != PDF_TOK_EOF)
	{
		while (len-- > 0)
			fz_unread_byte(file);
	}
	else
	if (tok == PDF_TOK_STREAM)
	{
		int c = fz_read_byte(file);
		if (c == '\r') {
			c = fz_peek_byte(file);
			if (c == '\n')
				fz_read_byte(file);
		}

		*stmofsp = fz_tell(file);
		if (*stmofsp < 0)
			return fz_throw("cannot seek in file");

		if (stm_len > 0)
		{
			fz_seek(file, *stmofsp + stm_len, 0);
			error = pdf_lex(&tok, file, buf, cap, &len);
			if (error)
				fz_catch(error, "cannot find endstream token, falling back to scanning");
			if (tok == PDF_TOK_ENDSTREAM)
				goto atobjend;
			fz_seek(file, *stmofsp, 0);
		}

		n = fz_read(file, (unsigned char *) buf, 9);
		if (n < 0)
			return fz_rethrow(n, "cannot read from file");

		while (memcmp(buf, "endstream", 9) != 0)
		{
			c = fz_read_byte(file);
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
		if (tok != PDF_TOK_ENDOBJ)
			fz_warn("object missing 'endobj' token");
	}

	return fz_okay;
}

static fz_error
pdf_repair_obj_stm(pdf_xref *xref, int num, int gen)
{
	fz_error error;
	fz_obj *obj;
	fz_stream *stm;
	int tok;
	int i, n, count;
	char buf[256];

	error = pdf_load_object(&obj, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load object stream object (%d %d R)", num, gen);

	count = fz_to_int(fz_dict_gets(obj, "N"));

	fz_drop_obj(obj);

	error = pdf_open_stream(&stm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot open object stream object (%d %d R)", num, gen);

	for (i = 0; i < count; i++)
	{
		error = pdf_lex(&tok, stm, buf, sizeof buf, &n);
		if (error || tok != PDF_TOK_INT)
		{
			fz_close(stm);
			return fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
		}

		n = atoi(buf);
		if (n >= xref->len)
			pdf_resize_xref(xref, n + 1);

		xref->table[n].ofs = num;
		xref->table[n].gen = i;
		xref->table[n].stm_ofs = 0;
		/* SumatraPDF: fix memory leak */
		if (xref->table[n].obj)
			fz_drop_obj(xref->table[n].obj);
		xref->table[n].obj = NULL;
		xref->table[n].type = 'o';

		error = pdf_lex(&tok, stm, buf, sizeof buf, &n);
		if (error || tok != PDF_TOK_INT)
		{
			fz_close(stm);
			return fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
		}
	}

	fz_close(stm);
	return fz_okay;
}

fz_error
pdf_repair_xref(pdf_xref *xref, char *buf, int bufsize)
{
	fz_error error;
	fz_obj *dict, *obj;
	fz_obj *length;

	fz_obj *encrypt = NULL;
	fz_obj *id = NULL;
	fz_obj *root = NULL;
	fz_obj *info = NULL;

	struct entry *list = NULL;
	int listlen;
	int listcap;
	int maxnum = 0;

	int num = 0;
	int gen = 0;
	int tmpofs, numofs = 0, genofs = 0;
	int stm_len, stm_ofs = 0;
	int tok;
	int next;
	int i, n, c;

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
			fz_seek(xref->file, i + 8, 0); /* skip "%PDF-X.Y" */
			break;
		}
	}

	/* skip comment line after version marker since some generators
	 * forget to terminate the comment with a newline */
	c = fz_read_byte(xref->file);
	while (c >= 0 && (c == ' ' || c == '%'))
		c = fz_read_byte(xref->file);
	fz_unread_byte(xref->file);

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

		if (tok == PDF_TOK_INT)
		{
			numofs = genofs;
			num = gen;
			genofs = tmpofs;
			gen = atoi(buf);
		}

		else if (tok == PDF_TOK_OBJ)
		{
			error = pdf_repair_obj(xref->file, buf, bufsize, &stm_ofs, &stm_len, &encrypt, &id);
			if (error)
			{
				error = fz_rethrow(error, "cannot parse object (%d %d R)", num, gen);
				/* SumatraPDF: if we've seen a root, try to do with what we've got */
				if (root)
				{
					fz_catch(error, "ignoring the rest of the file");
					break;
				}
				goto cleanup;
			}

			if (listlen + 1 == listcap)
			{
				listcap = (listcap * 3) / 2;
				list = fz_realloc(list, listcap, sizeof(struct entry));
			}

			list[listlen].num = num;
			list[listlen].gen = gen;
			list[listlen].ofs = numofs;
			list[listlen].stm_ofs = stm_ofs;
			list[listlen].stm_len = stm_len;
			listlen ++;

			if (num > maxnum)
				maxnum = num;
		}

		/* trailer dictionary */
		else if (tok == PDF_TOK_OPEN_DICT)
		{
			error = pdf_parse_dict(&dict, xref, xref->file, buf, bufsize);
			if (error)
			{
				error = fz_rethrow(error, "cannot parse object");
				/* SumatraPDF: if we've seen a root, try to do with what we've got */
				if (root)
				{
					fz_catch(error, "ignoring the rest of the file");
					break;
				}
				goto cleanup;
			}

			obj = fz_dict_gets(dict, "Encrypt");
			if (obj)
			{
				if (encrypt)
					fz_drop_obj(encrypt);
				encrypt = fz_keep_obj(obj);
			}

			obj = fz_dict_gets(dict, "ID");
			if (obj)
			{
				if (id)
					fz_drop_obj(id);
				id = fz_keep_obj(obj);
			}

			obj = fz_dict_gets(dict, "Root");
			if (obj)
			{
				if (root)
					fz_drop_obj(root);
				root = fz_keep_obj(obj);
			}

			obj = fz_dict_gets(dict, "Info");
			if (obj)
			{
				if (info)
					fz_drop_obj(info);
				info = fz_keep_obj(obj);
			}

			fz_drop_obj(dict);
		}

		else if (tok == PDF_TOK_ERROR)
			fz_read_byte(xref->file);

		else if (tok == PDF_TOK_EOF)
			break;
	}

	/* make xref reasonable */

	pdf_resize_xref(xref, maxnum + 1);

	for (i = 0; i < listlen; i++)
	{
		xref->table[list[i].num].type = 'n';
		xref->table[list[i].num].ofs = list[i].ofs;
		xref->table[list[i].num].gen = list[i].gen;

		xref->table[list[i].num].stm_ofs = list[i].stm_ofs;

		/* corrected stream length */
		if (list[i].stm_len >= 0)
		{
			error = pdf_load_object(&dict, xref, list[i].num, list[i].gen);
			if (error)
			{
				error = fz_rethrow(error, "cannot load stream object (%d %d R)", list[i].num, list[i].gen);
				goto cleanup;
			}

			length = fz_new_int(list[i].stm_len);
			fz_dict_puts(dict, "Length", length);
			fz_drop_obj(length);

			fz_drop_obj(dict);
		}

	}

	xref->table[0].type = 'f';
	xref->table[0].ofs = 0;
	xref->table[0].gen = 65535;
	xref->table[0].stm_ofs = 0;
	xref->table[0].obj = NULL;

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

	xref->trailer = fz_new_dict(5);

	obj = fz_new_int(maxnum + 1);
	fz_dict_puts(xref->trailer, "Size", obj);
	fz_drop_obj(obj);

	if (root)
	{
		fz_dict_puts(xref->trailer, "Root", root);
		fz_drop_obj(root);
	}
	if (info)
	{
		fz_dict_puts(xref->trailer, "Info", info);
		fz_drop_obj(info);
	}

	if (encrypt)
	{
		if (fz_is_indirect(encrypt))
		{
			/* create new reference with non-NULL xref pointer */
			obj = fz_new_indirect(fz_to_num(encrypt), fz_to_gen(encrypt), xref);
			fz_drop_obj(encrypt);
			encrypt = obj;
		}
		fz_dict_puts(xref->trailer, "Encrypt", encrypt);
		fz_drop_obj(encrypt);
	}

	if (id)
	{
		if (fz_is_indirect(id))
		{
			/* create new reference with non-NULL xref pointer */
			obj = fz_new_indirect(fz_to_num(id), fz_to_gen(id), xref);
			fz_drop_obj(id);
			id = obj;
		}
		fz_dict_puts(xref->trailer, "ID", id);
		fz_drop_obj(id);
	}

	fz_free(list);
	return fz_okay;

cleanup:
	if (encrypt) fz_drop_obj(encrypt);
	if (id) fz_drop_obj(id);
	if (root) fz_drop_obj(root);
	if (info) fz_drop_obj(info);
	fz_free(list);
	return error; /* already rethrown */
}

fz_error
pdf_repair_obj_stms(pdf_xref *xref)
{
	fz_obj *dict;
	int i;

	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].stm_ofs)
		{
			pdf_load_object(&dict, xref, i, 0);
			if (!strcmp(fz_to_name(fz_dict_gets(dict, "Type")), "ObjStm"))
				pdf_repair_obj_stm(xref, i, 0);
			fz_drop_obj(dict);
		}
	}

	/* SumatraPDF: ensure that streamed objects reside insided a known non-streamed object */
	for (i = 0; i < xref->len; i++)
		if (xref->table[i].type == 'o' && xref->table[xref->table[i].ofs].type != 'n')
			return fz_throw("invalid reference to non-object-stream: %d (%d 0 R)", xref->table[i].ofs, i);

	return fz_okay;
}

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

static void
pdf_repair_obj(fz_stream *file, char *buf, int cap, int *stmofsp, int *stmlenp, fz_obj **encrypt, fz_obj **id)
{
	int tok;
	int stm_len;
	int len;
	int n;
	fz_context *ctx = file->ctx;

	*stmofsp = 0;
	*stmlenp = -1;

	stm_len = 0;

	tok = pdf_lex(file, buf, cap, &len);
	/* RJW: "cannot parse object" */
	if (tok == PDF_TOK_OPEN_DICT)
	{
		fz_obj *dict, *obj;

		/* Send NULL xref so we don't try to resolve references */
		fz_try(ctx)
		{
			dict = pdf_parse_dict(NULL, file, buf, cap);
		}
		fz_catch(ctx)
		{
			/* Don't let a broken object at EOF overwrite a good one */
			if (file->eof)
				fz_throw(ctx, "broken object at EOF ignored");
			/* Silently swallow the error */
			dict = fz_new_dict(ctx, 2);
		}

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
		if (!fz_is_indirect(obj) && fz_is_int(obj))
			stm_len = fz_to_int(obj);

		fz_drop_obj(dict);
	}

	while ( tok != PDF_TOK_STREAM &&
		tok != PDF_TOK_ENDOBJ &&
		tok != PDF_TOK_ERROR &&
		tok != PDF_TOK_EOF &&
		tok != PDF_TOK_INT )
	{
		tok = pdf_lex(file, buf, cap, &len);
		/* RJW: "cannot scan for endobj or stream token" */
	}

	if (tok == PDF_TOK_INT)
	{
		while (len-- > 0)
			fz_unread_byte(file);
	}
	else if (tok == PDF_TOK_STREAM)
	{
		int c = fz_read_byte(file);
		if (c == '\r') {
			c = fz_peek_byte(file);
			if (c == '\n')
				fz_read_byte(file);
		}

		*stmofsp = fz_tell(file);
		if (*stmofsp < 0)
			fz_throw(ctx, "cannot seek in file");

		if (stm_len > 0)
		{
			fz_seek(file, *stmofsp + stm_len, 0);
			fz_try(ctx)
			{
				tok = pdf_lex(file, buf, cap, &len);
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "cannot find endstream token, falling back to scanning");
			}
			if (tok == PDF_TOK_ENDSTREAM)
				goto atobjend;
			fz_seek(file, *stmofsp, 0);
		}

		n = fz_read(file, (unsigned char *) buf, 9);
		if (n < 0)
			fz_throw(ctx, "cannot read from file");

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
		tok = pdf_lex(file, buf, cap, &len);
		/* RJW: "cannot scan for endobj token" */
		if (tok != PDF_TOK_ENDOBJ)
			fz_warn(ctx, "object missing 'endobj' token");
	}
}

static void
pdf_repair_obj_stm(pdf_document *xref, int num, int gen)
{
	fz_obj *obj;
	fz_stream *stm = NULL;
	int tok;
	int i, n, count;
	char buf[256];
	fz_context *ctx = xref->ctx;

	fz_var(stm);

	fz_try(ctx)
	{
		obj = pdf_load_object(xref, num, gen);

		count = fz_to_int(fz_dict_gets(obj, "N"));

		fz_drop_obj(obj);

		stm = pdf_open_stream(xref, num, gen);

		for (i = 0; i < count; i++)
		{
			tok = pdf_lex(stm, buf, sizeof buf, &n);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, "corrupt object stream (%d %d R)", num, gen);

			n = atoi(buf);
			if (n >= xref->len)
				pdf_resize_xref(xref, n + 1);

			xref->table[n].ofs = num;
			xref->table[n].gen = i;
			xref->table[n].stm_ofs = 0;
			fz_drop_obj(xref->table[n].obj);
			xref->table[n].obj = NULL;
			xref->table[n].type = 'o';

			tok = pdf_lex(stm, buf, sizeof buf, &n);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, "corrupt object stream (%d %d R)", num, gen);
		}
	}
	fz_always(ctx)
	{
		fz_close(stm);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot load object stream object (%d %d R)", num, gen);
	}
}

void
pdf_repair_xref(pdf_document *xref, char *buf, int bufsize)
{
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
	fz_context *ctx = xref->ctx;

	fz_var(encrypt);
	fz_var(id);
	fz_var(root);
	fz_var(info);
	fz_var(list);

	fz_seek(xref->file, 0, 0);

	fz_try(ctx)
	{
		listlen = 0;
		listcap = 1024;
		list = fz_malloc_array(ctx, listcap, sizeof(struct entry));

		/* look for '%PDF' version marker within first kilobyte of file */
		n = fz_read(xref->file, (unsigned char *)buf, MIN(bufsize, 1024));
		if (n < 0)
			fz_throw(ctx, "cannot read from file");

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
				fz_throw(ctx, "cannot tell in file");

			fz_try(ctx)
			{
				tok = pdf_lex(xref->file, buf, bufsize, &n);
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "ignoring the rest of the file");
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
				fz_try(ctx)
				{
					pdf_repair_obj(xref->file, buf, bufsize, &stm_ofs, &stm_len, &encrypt, &id);
				}
				fz_catch(ctx)
				{
					/* If we haven't seen a root yet, there is nothing
					 * we can do, but give up. Otherwise, we'll make
					 * do. */
					if (!root)
						fz_rethrow(ctx);
					fz_warn(ctx, "cannot parse object (%d %d R) - ignoring rest of file", num, gen);
					break;
				}

				if (listlen + 1 == listcap)
				{
					listcap = (listcap * 3) / 2;
					list = fz_resize_array(ctx, list, listcap, sizeof(struct entry));
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
				fz_try(ctx)
				{
					dict = pdf_parse_dict(xref, xref->file, buf, bufsize);
				}
				fz_catch(ctx)
				{
					/* If we haven't seen a root yet, there is nothing
					 * we can do, but give up. Otherwise, we'll make
					 * do. */
					if (!root)
						fz_rethrow(ctx);
					fz_warn(ctx, "cannot parse trailer dictionary - ignoring rest of file");
					break;
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
				fz_unlock(ctx, FZ_LOCK_FILE);
				dict = pdf_load_object(xref, list[i].num, list[i].gen);
				fz_lock(ctx, FZ_LOCK_FILE);
				/* RJW: "cannot load stream object (%d %d R)", list[i].num, list[i].gen */

				length = fz_new_int(ctx, list[i].stm_len);
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

		xref->trailer = fz_new_dict(ctx, 5);

		obj = fz_new_int(ctx, maxnum + 1);
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
				obj = fz_new_indirect(ctx, fz_to_num(encrypt), fz_to_gen(encrypt), xref);
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
				obj = fz_new_indirect(ctx, fz_to_num(id), fz_to_gen(id), xref);
				fz_drop_obj(id);
				id = obj;
			}
			fz_dict_puts(xref->trailer, "ID", id);
			fz_drop_obj(id);
		}

		fz_free(ctx, list);
	}
	fz_catch(ctx)
	{
		if (encrypt) fz_drop_obj(encrypt);
		if (id) fz_drop_obj(id);
		if (root) fz_drop_obj(root);
		if (info) fz_drop_obj(info);
		fz_free(ctx, list);
		fz_rethrow(ctx);
	}
}

void
pdf_repair_obj_stms(pdf_document *xref)
{
	fz_obj *dict;
	int i;

	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].stm_ofs)
		{
			dict = pdf_load_object(xref, i, 0);
			if (!strcmp(fz_to_name(fz_dict_gets(dict, "Type")), "ObjStm"))
				pdf_repair_obj_stm(xref, i, 0);
			fz_drop_obj(dict);
		}
	}

	/* Ensure that streamed objects reside inside a known non-streamed object */
	for (i = 0; i < xref->len; i++)
		if (xref->table[i].type == 'o' && xref->table[xref->table[i].ofs].type != 'n')
			fz_throw(xref->ctx, "invalid reference to non-object-stream: %d (%d 0 R)", xref->table[i].ofs, i);
}

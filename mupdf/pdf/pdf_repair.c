#include "fitz-internal.h"
#include "mupdf-internal.h"

/* Scan file for objects and reconstruct xref table */

/* Define in PDF 1.7 to be 8388607, but mupdf is more lenient. */
#define MAX_OBJECT_NUMBER (10 << 20)

struct entry
{
	int num;
	int gen;
	int ofs;
	int stm_ofs;
	int stm_len;
};

static void
pdf_repair_obj(fz_stream *file, pdf_lexbuf *buf, int *stmofsp, int *stmlenp, pdf_obj **encrypt, pdf_obj **id)
{
	int tok;
	int stm_len;
	int n;
	fz_context *ctx = file->ctx;

	*stmofsp = 0;
	*stmlenp = -1;

	stm_len = 0;

	tok = pdf_lex(file, buf);

	if (tok == PDF_TOK_OPEN_DICT)
	{
		pdf_obj *dict, *obj;

		/* Send NULL xref so we don't try to resolve references */
		fz_try(ctx)
		{
			dict = pdf_parse_dict(NULL, file, buf);
		}
		fz_catch(ctx)
		{
			/* Don't let a broken object at EOF overwrite a good one */
			if (file->eof)
				fz_throw(ctx, "broken object at EOF ignored");
			/* Silently swallow the error */
			dict = pdf_new_dict(ctx, 2);
		}

		obj = pdf_dict_gets(dict, "Type");
		if (pdf_is_name(obj) && !strcmp(pdf_to_name(obj), "XRef"))
		{
			obj = pdf_dict_gets(dict, "Encrypt");
			if (obj)
			{
				pdf_drop_obj(*encrypt);
				*encrypt = pdf_keep_obj(obj);
			}

			obj = pdf_dict_gets(dict, "ID");
			if (obj)
			{
				pdf_drop_obj(*id);
				*id = pdf_keep_obj(obj);
			}
		}

		obj = pdf_dict_gets(dict, "Length");
		if (!pdf_is_indirect(obj) && pdf_is_int(obj))
			stm_len = pdf_to_int(obj);

		pdf_drop_obj(dict);
	}

	while ( tok != PDF_TOK_STREAM &&
		tok != PDF_TOK_ENDOBJ &&
		tok != PDF_TOK_ERROR &&
		tok != PDF_TOK_EOF &&
		tok != PDF_TOK_INT )
	{
		tok = pdf_lex(file, buf);
	}

	if (tok == PDF_TOK_INT)
	{
		while (buf->len-- > 0)
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
				tok = pdf_lex(file, buf);
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "cannot find endstream token, falling back to scanning");
			}
			if (tok == PDF_TOK_ENDSTREAM)
				goto atobjend;
			fz_seek(file, *stmofsp, 0);
		}

		n = fz_read(file, (unsigned char *) buf->scratch, 9);
		if (n < 0)
			fz_throw(ctx, "cannot read from file");

		while (memcmp(buf->scratch, "endstream", 9) != 0)
		{
			c = fz_read_byte(file);
			if (c == EOF)
				break;
			memmove(&buf->scratch[0], &buf->scratch[1], 8);
			buf->scratch[8] = c;
		}

		*stmlenp = fz_tell(file) - *stmofsp - 9;

atobjend:
		tok = pdf_lex(file, buf);
		if (tok != PDF_TOK_ENDOBJ)
			fz_warn(ctx, "object missing 'endobj' token");
	}
}

static void
pdf_repair_obj_stm(pdf_document *xref, int num, int gen)
{
	pdf_obj *obj;
	fz_stream *stm = NULL;
	int tok;
	int i, n, count;
	fz_context *ctx = xref->ctx;
	pdf_lexbuf buf;

	fz_var(stm);

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);

	fz_try(ctx)
	{
		obj = pdf_load_object(xref, num, gen);

		count = pdf_to_int(pdf_dict_gets(obj, "N"));

		pdf_drop_obj(obj);

		stm = pdf_open_stream(xref, num, gen);

		for (i = 0; i < count; i++)
		{
			tok = pdf_lex(stm, &buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, "corrupt object stream (%d %d R)", num, gen);

			n = buf.i;
			if (n < 0)
			{
				fz_warn(ctx, "ignoring object with invalid object number (%d %d R)", n, i);
				continue;
			}
			else if (n > MAX_OBJECT_NUMBER)
			{
				fz_warn(ctx, "ignoring object with invalid object number (%d %d R)", n, i);
				continue;
			}
			if (n >= xref->len)
				pdf_resize_xref(xref, n + 1);

			xref->table[n].ofs = num;
			xref->table[n].gen = i;
			xref->table[n].stm_ofs = 0;
			pdf_drop_obj(xref->table[n].obj);
			xref->table[n].obj = NULL;
			xref->table[n].type = 'o';

			tok = pdf_lex(stm, &buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, "corrupt object stream (%d %d R)", num, gen);
		}
	}
	fz_always(ctx)
	{
		fz_close(stm);
		pdf_lexbuf_fin(&buf);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot load object stream object (%d %d R)", num, gen);
	}
}

/* Entered with file locked, remains locked throughout. */
void
pdf_repair_xref(pdf_document *xref, pdf_lexbuf *buf)
{
	pdf_obj *dict, *obj;
	pdf_obj *length;

	pdf_obj *encrypt = NULL;
	pdf_obj *id = NULL;
	pdf_obj *root = NULL;
	pdf_obj *info = NULL;

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

	xref->dirty = 1;

	fz_seek(xref->file, 0, 0);

	fz_try(ctx)
	{
		listlen = 0;
		listcap = 1024;
		list = fz_malloc_array(ctx, listcap, sizeof(struct entry));

		/* look for '%PDF' version marker within first kilobyte of file */
		n = fz_read(xref->file, (unsigned char *)buf->scratch, fz_mini(buf->size, 1024));
		if (n < 0)
			fz_throw(ctx, "cannot read from file");

		fz_seek(xref->file, 0, 0);
		for (i = 0; i < n - 4; i++)
		{
			if (memcmp(&buf->scratch[i], "%PDF", 4) == 0)
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
				tok = pdf_lex(xref->file, buf);
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
				gen = buf->i;
			}

			else if (tok == PDF_TOK_OBJ)
			{
				fz_try(ctx)
				{
					pdf_repair_obj(xref->file, buf, &stm_ofs, &stm_len, &encrypt, &id);
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

				/* SumatraPDF: fix memory leak */
				if (num <= 0)
				{
					fz_warn(ctx, "ignoring object with invalid object number (%d %d R)", num, gen);
					continue;
				}
				else if (num > MAX_OBJECT_NUMBER)
				{
					fz_warn(ctx, "ignoring object with invalid object number (%d %d R)", num, gen);
					continue;
				}

				gen = fz_clampi(gen, 0, 65535);

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
					dict = pdf_parse_dict(xref, xref->file, buf);
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

				obj = pdf_dict_gets(dict, "Encrypt");
				if (obj)
				{
					pdf_drop_obj(encrypt);
					encrypt = pdf_keep_obj(obj);
				}

				obj = pdf_dict_gets(dict, "ID");
				if (obj)
				{
					pdf_drop_obj(id);
					id = pdf_keep_obj(obj);
				}

				obj = pdf_dict_gets(dict, "Root");
				if (obj)
				{
					pdf_drop_obj(root);
					root = pdf_keep_obj(obj);
				}

				obj = pdf_dict_gets(dict, "Info");
				if (obj)
				{
					pdf_drop_obj(info);
					info = pdf_keep_obj(obj);
				}

				pdf_drop_obj(dict);
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

			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1841 */
			if (xref->table[list[i].num].obj)
			{
				pdf_drop_obj(xref->table[list[i].num].obj);
				xref->table[list[i].num].obj = NULL;
			}

			/* correct stream length for unencrypted documents */
			if (!encrypt && list[i].stm_len >= 0)
			{
				dict = pdf_load_object(xref, list[i].num, list[i].gen);

				length = pdf_new_int(ctx, list[i].stm_len);
				pdf_dict_puts(dict, "Length", length);
				pdf_drop_obj(length);

				pdf_drop_obj(dict);
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

		xref->trailer = pdf_new_dict(ctx, 5);

		obj = pdf_new_int(ctx, maxnum + 1);
		pdf_dict_puts(xref->trailer, "Size", obj);
		pdf_drop_obj(obj);

		if (root)
		{
			pdf_dict_puts(xref->trailer, "Root", root);
			pdf_drop_obj(root);
			root = NULL;
		}
		if (info)
		{
			pdf_dict_puts(xref->trailer, "Info", info);
			pdf_drop_obj(info);
			info = NULL;
		}

		if (encrypt)
		{
			if (pdf_is_indirect(encrypt))
			{
				/* create new reference with non-NULL xref pointer */
				obj = pdf_new_indirect(ctx, pdf_to_num(encrypt), pdf_to_gen(encrypt), xref);
				pdf_drop_obj(encrypt);
				encrypt = obj;
			}
			pdf_dict_puts(xref->trailer, "Encrypt", encrypt);
			pdf_drop_obj(encrypt);
			encrypt = NULL;
		}

		if (id)
		{
			if (pdf_is_indirect(id))
			{
				/* create new reference with non-NULL xref pointer */
				obj = pdf_new_indirect(ctx, pdf_to_num(id), pdf_to_gen(id), xref);
				pdf_drop_obj(id);
				id = obj;
			}
			pdf_dict_puts(xref->trailer, "ID", id);
			pdf_drop_obj(id);
			id = NULL;
		}

		fz_free(ctx, list);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(encrypt);
		pdf_drop_obj(id);
		pdf_drop_obj(root);
		pdf_drop_obj(info);
		fz_free(ctx, list);
		fz_rethrow(ctx);
	}
}

void
pdf_repair_obj_stms(pdf_document *xref)
{
	fz_context *ctx = xref->ctx;
	pdf_obj *dict;
	int i;

	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].stm_ofs)
		{
			dict = pdf_load_object(xref, i, 0);
			fz_try(ctx)
			{
				if (!strcmp(pdf_to_name(pdf_dict_gets(dict, "Type")), "ObjStm"))
					pdf_repair_obj_stm(xref, i, 0);
			}
			fz_always(ctx)
			{
				pdf_drop_obj(dict);
			}
			fz_catch(ctx)
			{
				fz_rethrow(ctx);
			}
		}
	}

	/* Ensure that streamed objects reside inside a known non-streamed object */
	for (i = 0; i < xref->len; i++)
		if (xref->table[i].type == 'o' && xref->table[xref->table[i].ofs].type != 'n')
			fz_throw(xref->ctx, "invalid reference to non-object-stream: %d (%d 0 R)", xref->table[i].ofs, i);
}

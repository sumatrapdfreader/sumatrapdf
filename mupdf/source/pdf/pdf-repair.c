#include "mupdf/pdf.h"

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

int
pdf_repair_obj(pdf_document *doc, pdf_lexbuf *buf, int *stmofsp, int *stmlenp, pdf_obj **encrypt, pdf_obj **id, pdf_obj **page, int *tmpofs)
{
	pdf_token tok;
	int stm_len;
	int n;
	fz_stream *file = doc->file;
	fz_context *ctx = file->ctx;

	*stmofsp = 0;
	if (stmlenp)
		*stmlenp = -1;

	stm_len = 0;

	/* On entry to this function, we know that we've just seen
	 * '<int> <int> obj'. We expect the next thing we see to be a
	 * pdf object. Regardless of the type of thing we meet next
	 * we only need to fully parse it if it is a dictionary. */
	tok = pdf_lex(file, buf);

	if (tok == PDF_TOK_OPEN_DICT)
	{
		pdf_obj *dict, *obj;

		/* Send NULL xref so we don't try to resolve references */
		fz_try(ctx)
		{
			dict = pdf_parse_dict(doc, file, buf);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			/* Don't let a broken object at EOF overwrite a good one */
			if (file->eof)
				fz_rethrow_message(ctx, "broken object at EOF ignored");
			/* Silently swallow the error */
			dict = pdf_new_dict(doc, 2);
		}

		if (encrypt && id)
		{
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
		}

		obj = pdf_dict_gets(dict, "Length");
		if (!pdf_is_indirect(obj) && pdf_is_int(obj))
			stm_len = pdf_to_int(obj);

		if (doc->file_reading_linearly && page)
		{
			obj = pdf_dict_gets(dict, "Type");
			if (!strcmp(pdf_to_name(obj), "Page"))
			{
				pdf_drop_obj(*page);
				*page = pdf_keep_obj(dict);
			}
		}

		pdf_drop_obj(dict);
	}

	while ( tok != PDF_TOK_STREAM &&
		tok != PDF_TOK_ENDOBJ &&
		tok != PDF_TOK_ERROR &&
		tok != PDF_TOK_EOF &&
		tok != PDF_TOK_INT )
	{
		*tmpofs = fz_tell(file);
		if (*tmpofs < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot tell in file");
		tok = pdf_lex(file, buf);
	}

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
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot seek in file");

		if (stm_len > 0)
		{
			fz_seek(file, *stmofsp + stm_len, 0);
			fz_try(ctx)
			{
				tok = pdf_lex(file, buf);
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_warn(ctx, "cannot find endstream token, falling back to scanning");
			}
			if (tok == PDF_TOK_ENDSTREAM)
				goto atobjend;
			fz_seek(file, *stmofsp, 0);
		}

		n = fz_read(file, (unsigned char *) buf->scratch, 9);
		if (n < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read from file");

		while (memcmp(buf->scratch, "endstream", 9) != 0)
		{
			c = fz_read_byte(file);
			if (c == EOF)
				break;
			memmove(&buf->scratch[0], &buf->scratch[1], 8);
			buf->scratch[8] = c;
		}

		if (stmlenp)
			*stmlenp = fz_tell(file) - *stmofsp - 9;

atobjend:
		*tmpofs = fz_tell(file);
		if (*tmpofs < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot tell in file");
		tok = pdf_lex(file, buf);
		if (tok != PDF_TOK_ENDOBJ)
			fz_warn(ctx, "object missing 'endobj' token");
		else
		{
			/* Read another token as we always return the next one */
			*tmpofs = fz_tell(file);
			if (*tmpofs < 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot tell in file");
			tok = pdf_lex(file, buf);
		}
	}
	return tok;
}

static void
pdf_repair_obj_stm(pdf_document *doc, int num, int gen)
{
	pdf_obj *obj;
	fz_stream *stm = NULL;
	pdf_token tok;
	int i, n, count;
	fz_context *ctx = doc->ctx;
	pdf_lexbuf buf;

	fz_var(stm);

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);

	fz_try(ctx)
	{
		obj = pdf_load_object(doc, num, gen);

		count = pdf_to_int(pdf_dict_gets(obj, "N"));

		pdf_drop_obj(obj);

		stm = pdf_open_stream(doc, num, gen);

		for (i = 0; i < count; i++)
		{
			pdf_xref_entry *entry;

			tok = pdf_lex(stm, &buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, FZ_ERROR_GENERIC, "corrupt object stream (%d %d R)", num, gen);

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

			entry = pdf_get_populating_xref_entry(doc, n);
			entry->ofs = num;
			entry->gen = i;
			entry->stm_ofs = 0;
			pdf_drop_obj(entry->obj);
			entry->obj = NULL;
			entry->type = 'o';

			tok = pdf_lex(stm, &buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, FZ_ERROR_GENERIC, "corrupt object stream (%d %d R)", num, gen);
		}
	}
	fz_always(ctx)
	{
		fz_close(stm);
		pdf_lexbuf_fin(&buf);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot load object stream object (%d %d R)", num, gen);
	}
}

/* Entered with file locked, remains locked throughout. */
void
pdf_repair_xref(pdf_document *doc, pdf_lexbuf *buf)
{
	pdf_obj *dict, *obj = NULL;
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
	int stm_len, stm_ofs;
	pdf_token tok;
	int next;
	int i, n, c;
	fz_context *ctx = doc->ctx;

	fz_var(encrypt);
	fz_var(id);
	fz_var(root);
	fz_var(info);
	fz_var(list);
	fz_var(obj);

	doc->dirty = 1;
	/* Can't support incremental update after repair */
	doc->freeze_updates = 1;

	fz_seek(doc->file, 0, 0);

	fz_try(ctx)
	{
		pdf_xref_entry *entry;
		listlen = 0;
		listcap = 1024;
		list = fz_malloc_array(ctx, listcap, sizeof(struct entry));

		/* look for '%PDF' version marker within first kilobyte of file */
		n = fz_read(doc->file, (unsigned char *)buf->scratch, fz_mini(buf->size, 1024));
		if (n < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read from file");

		fz_seek(doc->file, 0, 0);
		for (i = 0; i < n - 4; i++)
		{
			if (memcmp(&buf->scratch[i], "%PDF", 4) == 0)
			{
				fz_seek(doc->file, i + 8, 0); /* skip "%PDF-X.Y" */
				break;
			}
		}

		/* skip comment line after version marker since some generators
		 * forget to terminate the comment with a newline */
		c = fz_read_byte(doc->file);
		while (c >= 0 && (c == ' ' || c == '%'))
			c = fz_read_byte(doc->file);
		fz_unread_byte(doc->file);

		while (1)
		{
			tmpofs = fz_tell(doc->file);
			if (tmpofs < 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot tell in file");

			fz_try(ctx)
			{
				tok = pdf_lex(doc->file, buf);
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_warn(ctx, "ignoring the rest of the file");
				break;
			}

			/* If we have the next token already, then we'll jump
			 * back here, rather than going through the top of
			 * the loop. */
		have_next_token:

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
					stm_len = 0;
					stm_ofs = 0;
					tok = pdf_repair_obj(doc, buf, &stm_ofs, &stm_len, &encrypt, &id, NULL, &tmpofs);
				}
				fz_catch(ctx)
				{
					fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
					/* If we haven't seen a root yet, there is nothing
					 * we can do, but give up. Otherwise, we'll make
					 * do. */
					if (!root)
						fz_rethrow(ctx);
					fz_warn(ctx, "cannot parse object (%d %d R) - ignoring rest of file", num, gen);
					break;
				}

				if (num <= 0 || num > MAX_OBJECT_NUMBER)
				{
					fz_warn(ctx, "ignoring object with invalid object number (%d %d R)", num, gen);
					goto have_next_token;
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

				goto have_next_token;
			}

			/* trailer dictionary */
			else if (tok == PDF_TOK_OPEN_DICT)
			{
				fz_try(ctx)
				{
					dict = pdf_parse_dict(doc, doc->file, buf);
				}
				fz_catch(ctx)
				{
					fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
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
				obj = NULL;
			}

			else if (tok == PDF_TOK_ERROR)
				fz_read_byte(doc->file);

			else if (tok == PDF_TOK_EOF)
				break;
		}

		/* make xref reasonable */

		/*
			Dummy access to entry to assure sufficient space in the xref table
			and avoid repeated reallocs in the loop
		*/
		(void)pdf_get_populating_xref_entry(doc, maxnum);

		for (i = 0; i < listlen; i++)
		{
			entry = pdf_get_populating_xref_entry(doc, list[i].num);
			entry->type = 'n';
			entry->ofs = list[i].ofs;
			entry->gen = list[i].gen;

			entry->stm_ofs = list[i].stm_ofs;

			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1841 */
			if (entry->obj)
			{
				pdf_drop_obj(entry->obj);
				entry->obj = NULL;
			}

			/* correct stream length for unencrypted documents */
			if (!encrypt && list[i].stm_len >= 0)
			{
				dict = pdf_load_object(doc, list[i].num, list[i].gen);

				length = pdf_new_int(doc, list[i].stm_len);
				pdf_dict_puts(dict, "Length", length);
				pdf_drop_obj(length);

				pdf_drop_obj(dict);
			}
		}

		entry = pdf_get_populating_xref_entry(doc, 0);
		entry->type = 'f';
		entry->ofs = 0;
		entry->gen = 65535;
		entry->stm_ofs = 0;
		entry->obj = NULL;

		next = 0;
		for (i = pdf_xref_len(doc) - 1; i >= 0; i--)
		{
			entry = pdf_get_populating_xref_entry(doc, i);
			if (entry->type == 'f')
			{
				entry->ofs = next;
				if (entry->gen < 65535)
					entry->gen ++;
				next = i;
			}
		}

		/* create a repaired trailer, Root will be added later */

		obj = pdf_new_dict(doc, 5);
		/* During repair there is only a single xref section */
		pdf_set_populating_xref_trailer(doc, obj);
		pdf_drop_obj(obj);
		obj = NULL;

		obj = pdf_new_int(doc, maxnum + 1);
		pdf_dict_puts(pdf_trailer(doc), "Size", obj);
		pdf_drop_obj(obj);
		obj = NULL;

		if (root)
		{
			pdf_dict_puts(pdf_trailer(doc), "Root", root);
			pdf_drop_obj(root);
			root = NULL;
		}
		if (info)
		{
			pdf_dict_puts(pdf_trailer(doc), "Info", info);
			pdf_drop_obj(info);
			info = NULL;
		}

		if (encrypt)
		{
			if (pdf_is_indirect(encrypt))
			{
				/* create new reference with non-NULL xref pointer */
				obj = pdf_new_indirect(doc, pdf_to_num(encrypt), pdf_to_gen(encrypt));
				pdf_drop_obj(encrypt);
				encrypt = obj;
				obj = NULL;
			}
			pdf_dict_puts(pdf_trailer(doc), "Encrypt", encrypt);
			pdf_drop_obj(encrypt);
			encrypt = NULL;
		}

		if (id)
		{
			if (pdf_is_indirect(id))
			{
				/* create new reference with non-NULL xref pointer */
				obj = pdf_new_indirect(doc, pdf_to_num(id), pdf_to_gen(id));
				pdf_drop_obj(id);
				id = obj;
				obj = NULL;
			}
			pdf_dict_puts(pdf_trailer(doc), "ID", id);
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
		pdf_drop_obj(obj);
		pdf_drop_obj(info);
		fz_free(ctx, list);
		fz_rethrow(ctx);
	}
}

void
pdf_repair_obj_stms(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *dict;
	int i;
	int xref_len = pdf_xref_len(doc);

	for (i = 0; i < xref_len; i++)
	{
		pdf_xref_entry *entry = pdf_get_populating_xref_entry(doc, i);

		if (entry->stm_ofs)
		{
			dict = pdf_load_object(doc, i, 0);
			fz_try(ctx)
			{
				if (!strcmp(pdf_to_name(pdf_dict_gets(dict, "Type")), "ObjStm"))
					pdf_repair_obj_stm(doc, i, 0);
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
	for (i = 0; i < xref_len; i++)
	{
		pdf_xref_entry *entry = pdf_get_populating_xref_entry(doc, i);

		if (entry->type == 'o' && pdf_get_populating_xref_entry(doc, entry->ofs)->type != 'n')
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "invalid reference to non-object-stream: %d (%d 0 R)", entry->ofs, i);
	}
}

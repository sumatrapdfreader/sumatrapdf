#include "fitz-internal.h"
#include "mupdf-internal.h"

static inline int iswhite(int ch)
{
	return
		ch == '\000' || ch == '\011' || ch == '\012' ||
		ch == '\014' || ch == '\015' || ch == '\040';
}

/*
 * magic version tag and startxref
 */

static void
pdf_load_version(pdf_document *xref)
{
	char buf[20];

	fz_seek(xref->file, 0, 0);
	fz_read_line(xref->file, buf, sizeof buf);
	if (memcmp(buf, "%PDF-", 5) != 0)
		fz_throw(xref->ctx, "cannot recognize version marker");

	/* SumatraPDF: use fz_atof once the major or minor PDF version reaches 10 */
	xref->version = atoi(buf + 5) * 10 + atoi(buf + 7);
}

static void
pdf_read_start_xref(pdf_document *xref)
{
	unsigned char buf[1024];
	int t, n;
	int i;

	fz_seek(xref->file, 0, 2);

	xref->file_size = fz_tell(xref->file);

	t = fz_maxi(0, xref->file_size - (int)sizeof buf);
	fz_seek(xref->file, t, 0);

	n = fz_read(xref->file, buf, sizeof buf);
	if (n < 0)
		fz_throw(xref->ctx, "cannot read from file");

	for (i = n - 9; i >= 0; i--)
	{
		if (memcmp(buf + i, "startxref", 9) == 0)
		{
			i += 9;
			while (iswhite(buf[i]) && i < n)
				i ++;
			xref->startxref = atoi((char*)(buf + i));

			return;
		}
	}

	fz_throw(xref->ctx, "cannot find startxref");
}

/*
 * trailer dictionary
 */

static void
pdf_read_old_trailer(pdf_document *xref, pdf_lexbuf *buf)
{
	int len;
	char *s;
	int t;
	pdf_token tok;
	int c;

	fz_read_line(xref->file, buf->scratch, buf->size);
	if (strncmp(buf->scratch, "xref", 4) != 0)
		fz_throw(xref->ctx, "cannot find xref marker");

	while (1)
	{
		c = fz_peek_byte(xref->file);
		if (!(c >= '0' && c <= '9'))
			break;

		fz_read_line(xref->file, buf->scratch, buf->size);
		s = buf->scratch;
		fz_strsep(&s, " "); /* ignore ofs */
		if (!s)
			fz_throw(xref->ctx, "invalid range marker in xref");
		len = fz_atoi(fz_strsep(&s, " "));

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
			fz_seek(xref->file, -(2 + (int)strlen(s)), 1);

		t = fz_tell(xref->file);
		if (t < 0)
			fz_throw(xref->ctx, "cannot tell in file");

		fz_seek(xref->file, t + 20 * len, 0);
	}

	fz_try(xref->ctx)
	{
		tok = pdf_lex(xref->file, buf);
		if (tok != PDF_TOK_TRAILER)
			fz_throw(xref->ctx, "expected trailer marker");

		tok = pdf_lex(xref->file, buf);
		if (tok != PDF_TOK_OPEN_DICT)
			fz_throw(xref->ctx, "expected trailer dictionary");

		xref->trailer = pdf_parse_dict(xref, xref->file, buf);
	}
	fz_catch(xref->ctx)
	{
		fz_throw(xref->ctx, "cannot parse trailer");
	}
}

static void
pdf_read_new_trailer(pdf_document *xref, pdf_lexbuf *buf)
{
	fz_try(xref->ctx)
	{
		int num, gen, stm_ofs, ofs;
		ofs = fz_tell(xref->file);
		xref->trailer = pdf_parse_ind_obj(xref, xref->file, buf, &num, &gen, &stm_ofs);
		if (num > xref->len)
			pdf_resize_xref(xref, num+1);
		xref->table[num].ofs = ofs;
		xref->table[num].gen = gen;
		xref->table[num].stm_ofs = stm_ofs;
		pdf_drop_obj(xref->table[num].obj);
		xref->table[num].obj = pdf_keep_obj(xref->trailer);
		xref->table[num].type = 'n';
	}
	fz_catch(xref->ctx)
	{
		fz_throw(xref->ctx, "cannot parse trailer (compressed)");
	}
}

static void
pdf_read_trailer(pdf_document *xref, pdf_lexbuf *buf)
{
	int c;

	fz_seek(xref->file, xref->startxref, 0);

	while (iswhite(fz_peek_byte(xref->file)))
		fz_read_byte(xref->file);

	fz_try(xref->ctx)
	{
		c = fz_peek_byte(xref->file);
		if (c == 'x')
			pdf_read_old_trailer(xref, buf);
		else if (c >= '0' && c <= '9')
			pdf_read_new_trailer(xref, buf);
		else
			fz_throw(xref->ctx, "cannot recognize xref format: '%c'", c);
	}
	fz_catch(xref->ctx)
	{
		fz_throw(xref->ctx, "cannot read trailer");
	}
}

/*
 * xref tables
 */

void
pdf_resize_xref(pdf_document *xref, int newlen)
{
	int i;

	xref->table = fz_resize_array(xref->ctx, xref->table, newlen, sizeof(pdf_xref_entry));
	for (i = xref->len; i < newlen; i++)
	{
		xref->table[i].type = 0;
		xref->table[i].ofs = 0;
		xref->table[i].gen = 0;
		xref->table[i].stm_ofs = 0;
		xref->table[i].stm_buf = NULL;
		xref->table[i].obj = NULL;
	}
	xref->len = newlen;
}

pdf_obj *
pdf_new_ref(pdf_document *xref, pdf_obj *obj)
{
	int num = pdf_create_object(xref);
	pdf_update_object(xref, num, obj);
	return pdf_new_indirect(xref->ctx, num, 0, xref);
}

static pdf_obj *
pdf_read_old_xref(pdf_document *xref, pdf_lexbuf *buf)
{
	int ofs, len;
	char *s;
	int n;
	pdf_token tok;
	int i;
	int c;
	pdf_obj *trailer;

	fz_read_line(xref->file, buf->scratch, buf->size);
	if (strncmp(buf->scratch, "xref", 4) != 0)
		fz_throw(xref->ctx, "cannot find xref marker");

	while (1)
	{
		c = fz_peek_byte(xref->file);
		if (!(c >= '0' && c <= '9'))
			break;

		fz_read_line(xref->file, buf->scratch, buf->size);
		s = buf->scratch;
		ofs = fz_atoi(fz_strsep(&s, " "));
		len = fz_atoi(fz_strsep(&s, " "));

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
		{
			fz_warn(xref->ctx, "broken xref section. proceeding anyway.");
			fz_seek(xref->file, -(2 + (int)strlen(s)), 1);
		}

		/* broken pdfs where size in trailer undershoots entries in xref sections */
		if (ofs + len > xref->len)
		{
			fz_warn(xref->ctx, "broken xref section, proceeding anyway.");
			pdf_resize_xref(xref, ofs + len);
		}

		for (i = ofs; i < ofs + len; i++)
		{
			n = fz_read(xref->file, (unsigned char *) buf->scratch, 20);
			if (n < 0)
				fz_throw(xref->ctx, "cannot read xref table");
			if (!xref->table[i].type)
			{
				s = buf->scratch;

				/* broken pdfs where line start with white space */
				while (*s != '\0' && iswhite(*s))
					s++;

				xref->table[i].ofs = atoi(s);
				xref->table[i].gen = atoi(s + 11);
				xref->table[i].type = s[17];
				if (s[17] != 'f' && s[17] != 'n' && s[17] != 'o')
					fz_throw(xref->ctx, "unexpected xref type: %#x (%d %d R)", s[17], i, xref->table[i].gen);
			}
		}
	}

	fz_try(xref->ctx)
	{
		tok = pdf_lex(xref->file, buf);
		if (tok != PDF_TOK_TRAILER)
			fz_throw(xref->ctx, "expected trailer marker");

		tok = pdf_lex(xref->file, buf);
		if (tok != PDF_TOK_OPEN_DICT)
			fz_throw(xref->ctx, "expected trailer dictionary");

		trailer = pdf_parse_dict(xref, xref->file, buf);
	}
	fz_catch(xref->ctx)
	{
		fz_throw(xref->ctx, "cannot parse trailer");
	}
	return trailer;
}

static void
pdf_read_new_xref_section(pdf_document *xref, fz_stream *stm, int i0, int i1, int w0, int w1, int w2)
{
	int i, n;

	if (i0 < 0 || i1 < 0)
		fz_throw(xref->ctx, "negative xref stream entry index");
	if (i0 + i1 > xref->len)
		fz_throw(xref->ctx, "xref stream has too many entries");

	for (i = i0; i < i0 + i1; i++)
	{
		int a = 0;
		int b = 0;
		int c = 0;

		if (fz_is_eof(stm))
			fz_throw(xref->ctx, "truncated xref stream");

		for (n = 0; n < w0; n++)
			a = (a << 8) + fz_read_byte(stm);
		for (n = 0; n < w1; n++)
			b = (b << 8) + fz_read_byte(stm);
		for (n = 0; n < w2; n++)
			c = (c << 8) + fz_read_byte(stm);

		if (!xref->table[i].type)
		{
			int t = w0 ? a : 1;
			xref->table[i].type = t == 0 ? 'f' : t == 1 ? 'n' : t == 2 ? 'o' : 0;
			xref->table[i].ofs = w1 ? b : 0;
			xref->table[i].gen = w2 ? c : 0;
		}
	}
}

/* Entered with file locked, remains locked throughout. */
static pdf_obj *
pdf_read_new_xref(pdf_document *xref, pdf_lexbuf *buf)
{
	fz_stream *stm = NULL;
	pdf_obj *trailer = NULL;
	pdf_obj *index = NULL;
	pdf_obj *obj = NULL;
	int num, gen, stm_ofs;
	int size, w0, w1, w2;
	int t;
	fz_context *ctx = xref->ctx;

	fz_var(trailer);
	fz_var(stm);

	fz_try(ctx)
	{
		int ofs = fz_tell(xref->file);
		trailer = pdf_parse_ind_obj(xref, xref->file, buf, &num, &gen, &stm_ofs);
		if (num > xref->len)
			pdf_resize_xref(xref, num+1);
		xref->table[num].ofs = ofs;
		xref->table[num].gen = gen;
		xref->table[num].stm_ofs = stm_ofs;
		pdf_drop_obj(xref->table[num].obj);
		xref->table[num].obj = trailer;
		xref->table[num].type = 'n';
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(trailer);
		fz_throw(ctx, "cannot parse compressed xref stream object");
	}

	fz_try(ctx)
	{
		obj = pdf_dict_gets(trailer, "Size");
		if (!obj)
			fz_throw(ctx, "xref stream missing Size entry (%d %d R)", num, gen);

		size = pdf_to_int(obj);
		if (size > xref->len)
			pdf_resize_xref(xref, size);

		/* SumatraPDF: xref stream objects don't need an xref entry themselves */
		if (num < 0 || num >= xref->len)
			fz_warn(ctx, "object id (%d %d R) out of range (0..%d)", num, gen, xref->len - 1);

		obj = pdf_dict_gets(trailer, "W");
		if (!obj)
			fz_throw(ctx, "xref stream missing W entry (%d %d R)", num, gen);
		w0 = pdf_to_int(pdf_array_get(obj, 0));
		w1 = pdf_to_int(pdf_array_get(obj, 1));
		w2 = pdf_to_int(pdf_array_get(obj, 2));

		if (w0 < 0)
			fz_warn(ctx, "xref stream objects have corrupt type");
		if (w1 < 0)
			fz_warn(ctx, "xref stream objects have corrupt offset");
		if (w2 < 0)
			fz_warn(ctx, "xref stream objects have corrupt generation");

		w0 = w0 < 0 ? 0 : w0;
		w1 = w1 < 0 ? 0 : w1;
		w2 = w2 < 0 ? 0 : w2;

		index = pdf_dict_gets(trailer, "Index");

		stm = pdf_open_stream_with_offset(xref, num, gen, trailer, stm_ofs);

		if (!index)
		{
			pdf_read_new_xref_section(xref, stm, 0, size, w0, w1, w2);
		}
		else
		{
			int n = pdf_array_len(index);
			for (t = 0; t < n; t += 2)
			{
				int i0 = pdf_to_int(pdf_array_get(index, t + 0));
				int i1 = pdf_to_int(pdf_array_get(index, t + 1));
				pdf_read_new_xref_section(xref, stm, i0, i1, w0, w1, w2);
			}
		}
	}
	fz_always(ctx)
	{
		fz_close(stm);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return trailer;
}

/* File is locked on entry, and exit (but may be dropped in the middle) */
static pdf_obj *
pdf_read_xref(pdf_document *xref, int ofs, pdf_lexbuf *buf)
{
	int c;
	fz_context *ctx = xref->ctx;
	pdf_obj *trailer;

	fz_seek(xref->file, ofs, 0);

	while (iswhite(fz_peek_byte(xref->file)))
		fz_read_byte(xref->file);

	fz_try(ctx)
	{
		c = fz_peek_byte(xref->file);
		if (c == 'x')
			trailer = pdf_read_old_xref(xref, buf);
		else if (c >= '0' && c <= '9')
			trailer = pdf_read_new_xref(xref, buf);
		else
			fz_throw(ctx, "cannot recognize xref format");
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot read xref (ofs=%d)", ofs);
	}
	return trailer;
}

typedef struct ofs_list_s ofs_list;

struct ofs_list_s
{
	int max;
	int len;
	int *list;
};

static void
do_read_xref_sections(pdf_document *xref, int ofs, pdf_lexbuf *buf, ofs_list *offsets)
{
	pdf_obj *trailer = NULL;
	fz_context *ctx = xref->ctx;
	int xrefstmofs = 0;
	int prevofs = 0;

	fz_var(trailer);
	fz_var(xrefstmofs);
	fz_var(prevofs);

	fz_try(ctx)
	{
		do
		{
			int i;
			/* Avoid potential infinite recursion */
			for (i = 0; i < offsets->len; i ++)
			{
				if (offsets->list[i] == ofs)
					break;
			}
			if (i < offsets->len)
			{
				fz_warn(ctx, "ignoring xref recursion with offset %d", ofs);
				break;
			}
			if (offsets->len == offsets->max)
			{
				offsets->list = fz_resize_array(ctx, offsets->list, offsets->max*2, sizeof(int));
				offsets->max *= 2;
			}
			offsets->list[offsets->len++] = ofs;

			trailer = pdf_read_xref(xref, ofs, buf);

			/* FIXME: do we overwrite free entries properly? */
			xrefstmofs = pdf_to_int(pdf_dict_gets(trailer, "XRefStm"));
			prevofs = pdf_to_int(pdf_dict_gets(trailer, "Prev"));

			if (xrefstmofs < 0)
				fz_throw(ctx, "negative xref stream offset");
			if (prevofs < 0)
				fz_throw(ctx, "negative xref stream offset for previous xref stream");

			/* We only recurse if we have both xrefstm and prev.
			 * Hopefully this happens infrequently. */
			if (xrefstmofs && prevofs)
				do_read_xref_sections(xref, xrefstmofs, buf, offsets);
			if (prevofs)
				ofs = prevofs;
			else if (xrefstmofs)
				ofs = xrefstmofs;
			pdf_drop_obj(trailer);
			trailer = NULL;
		}
		while (prevofs || xrefstmofs);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(trailer);
		fz_throw(ctx, "cannot read xref at offset %d", ofs);
	}
}

static void
pdf_read_xref_sections(pdf_document *xref, int ofs, pdf_lexbuf *buf)
{
	fz_context *ctx = xref->ctx;
	ofs_list list;

	list.len = 0;
	list.max = 10;
	list.list = fz_malloc_array(ctx, 10, sizeof(int));
	fz_try(ctx)
	{
		do_read_xref_sections(xref, ofs, buf, &list);
	}
	fz_always(ctx)
	{
		fz_free(ctx, list.list);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/*
 * load xref tables from pdf
 *
 * File locked on entry, throughout and on exit.
 */

static void
pdf_load_xref(pdf_document *xref, pdf_lexbuf *buf)
{
	int size;
	int i;
	fz_context *ctx = xref->ctx;

	pdf_load_version(xref);

	pdf_read_start_xref(xref);

	pdf_read_trailer(xref, buf);

	size = pdf_to_int(pdf_dict_gets(xref->trailer, "Size"));
	if (!size)
		fz_throw(ctx, "trailer missing Size entry");

	if (size > xref->len)
		pdf_resize_xref(xref, size);

	pdf_read_xref_sections(xref, xref->startxref, buf);

	/* broken pdfs where first object is not free */
	if (xref->table[0].type != 'f')
		fz_throw(ctx, "first object in xref is not free");

	/* broken pdfs where object offsets are out of range */
	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].type == 'n')
		{
			/* Special case code: "0000000000 * n" means free,
			 * according to some producers (inc Quartz) */
			if (xref->table[i].ofs == 0)
				xref->table[i].type = 'f';
			else if (xref->table[i].ofs <= 0 || xref->table[i].ofs >= xref->file_size)
				fz_throw(ctx, "object offset out of range: %d (%d 0 R)", xref->table[i].ofs, i);
		}
		if (xref->table[i].type == 'o')
			if (xref->table[i].ofs <= 0 || xref->table[i].ofs >= xref->len || xref->table[xref->table[i].ofs].type != 'n')
				fz_throw(ctx, "invalid reference to an objstm that does not exist: %d (%d 0 R)", xref->table[i].ofs, i);
	}
}

void
pdf_ocg_set_config(pdf_document *xref, int config)
{
	int i, j, len, len2;
	pdf_ocg_descriptor *desc = xref->ocg;
	pdf_obj *obj, *cobj;
	char *name;

	obj = pdf_dict_gets(pdf_dict_gets(xref->trailer, "Root"), "OCProperties");
	if (!obj)
	{
		if (config == 0)
			return;
		else
			fz_throw(xref->ctx, "Unknown OCG config (None known!)");
	}
	if (config == 0)
	{
		cobj = pdf_dict_gets(obj, "D");
		if (!cobj)
			fz_throw(xref->ctx, "No default OCG config");
	}
	else
	{
		cobj = pdf_array_get(pdf_dict_gets(obj, "Configs"), config);
		if (!cobj)
			fz_throw(xref->ctx, "Illegal OCG config");
	}

	pdf_drop_obj(desc->intent);
	desc->intent = pdf_dict_gets(cobj, "Intent");
	if (desc->intent)
		pdf_keep_obj(desc->intent);

	len = desc->len;
	name = pdf_to_name(pdf_dict_gets(cobj, "BaseState"));
	if (strcmp(name, "Unchanged") == 0)
	{
		/* Do nothing */
	}
	else if (strcmp(name, "OFF") == 0)
	{
		for (i = 0; i < len; i++)
		{
			desc->ocgs[i].state = 0;
		}
	}
	else /* Default to ON */
	{
		for (i = 0; i < len; i++)
		{
			desc->ocgs[i].state = 1;
		}
	}

	obj = pdf_dict_gets(cobj, "ON");
	len2 = pdf_array_len(obj);
	for (i = 0; i < len2; i++)
	{
		pdf_obj *o = pdf_array_get(obj, i);
		int n = pdf_to_num(o);
		int g = pdf_to_gen(o);
		for (j=0; j < len; j++)
		{
			if (desc->ocgs[j].num == n && desc->ocgs[j].gen == g)
			{
				desc->ocgs[j].state = 1;
				break;
			}
		}
	}

	obj = pdf_dict_gets(cobj, "OFF");
	len2 = pdf_array_len(obj);
	for (i = 0; i < len2; i++)
	{
		pdf_obj *o = pdf_array_get(obj, i);
		int n = pdf_to_num(o);
		int g = pdf_to_gen(o);
		for (j=0; j < len; j++)
		{
			if (desc->ocgs[j].num == n && desc->ocgs[j].gen == g)
			{
				desc->ocgs[j].state = 0;
				break;
			}
		}
	}

	/* FIXME: Should make 'num configs' available in the descriptor. */
	/* FIXME: Should copy out 'Intent' here into the descriptor, and remove
	 * csi->intent in favour of that. */
	/* FIXME: Should copy 'AS' into the descriptor, and visibility
	 * decisions should respect it. */
	/* FIXME: Make 'Order' available via the descriptor (when we have an
	 * app that needs it) */
	/* FIXME: Make 'ListMode' available via the descriptor (when we have
	 * an app that needs it) */
	/* FIXME: Make 'RBGroups' available via the descriptor (when we have
	 * an app that needs it) */
	/* FIXME: Make 'Locked' available via the descriptor (when we have
	 * an app that needs it) */
}

static void
pdf_read_ocg(pdf_document *xref)
{
	pdf_obj *obj, *ocg;
	int len, i;
	pdf_ocg_descriptor *desc;
	fz_context *ctx = xref->ctx;

	fz_var(desc);

	obj = pdf_dict_gets(pdf_dict_gets(xref->trailer, "Root"), "OCProperties");
	if (!obj)
		return;
	ocg = pdf_dict_gets(obj, "OCGs");
	if (!ocg || !pdf_is_array(ocg))
		/* Not ever supposed to happen, but live with it. */
		return;
	len = pdf_array_len(ocg);
	fz_try(ctx)
	{
		desc = fz_calloc(ctx, 1, sizeof(*desc));
		desc->len = len;
		desc->ocgs = fz_calloc(ctx, len, sizeof(*desc->ocgs));
		desc->intent = NULL;
		for (i=0; i < len; i++)
		{
			pdf_obj *o = pdf_array_get(ocg, i);
			desc->ocgs[i].num = pdf_to_num(o);
			desc->ocgs[i].gen = pdf_to_gen(o);
			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=2011 */
			desc->ocgs[i].state = 1;
		}
		xref->ocg = desc;
	}
	fz_catch(ctx)
	{
		if (desc)
			fz_free(ctx, desc->ocgs);
		fz_free(ctx, desc);
		fz_rethrow(ctx);
	}

	pdf_ocg_set_config(xref, 0);
}

static void
pdf_free_ocg(fz_context *ctx, pdf_ocg_descriptor *desc)
{
	if (!desc)
		return;

	pdf_drop_obj(desc->intent);
	fz_free(ctx, desc->ocgs);
	fz_free(ctx, desc);
}

/*
 * Initialize and load xref tables.
 * If password is not null, try to decrypt.
 */

static void
pdf_init_document(pdf_document *xref)
{
	fz_context *ctx = xref->ctx;
	pdf_obj *encrypt, *id;
	pdf_obj *dict = NULL;
	pdf_obj *obj;
	pdf_obj *nobj = NULL;
	int i, repaired = 0;

	fz_var(dict);
	fz_var(nobj);

	fz_try(ctx)
	{
		pdf_load_xref(xref, &xref->lexbuf.base);
	}
	fz_catch(ctx)
	{
		if (xref->table)
		{
			fz_free(xref->ctx, xref->table);
			xref->table = NULL;
			xref->len = 0;
		}
		if (xref->trailer)
		{
			pdf_drop_obj(xref->trailer);
			xref->trailer = NULL;
		}
		fz_warn(xref->ctx, "trying to repair broken xref");
		repaired = 1;
	}

	fz_try(ctx)
	{
		int hasroot, hasinfo;

		if (repaired)
			pdf_repair_xref(xref, &xref->lexbuf.base);

		encrypt = pdf_dict_gets(xref->trailer, "Encrypt");
		id = pdf_dict_gets(xref->trailer, "ID");
		if (pdf_is_dict(encrypt))
			xref->crypt = pdf_new_crypt(ctx, encrypt, id);

		/* Allow lazy clients to read encrypted files with a blank password */
		pdf_authenticate_password(xref, "");

		if (repaired)
		{
			pdf_repair_obj_stms(xref);

			hasroot = (pdf_dict_gets(xref->trailer, "Root") != NULL);
			hasinfo = (pdf_dict_gets(xref->trailer, "Info") != NULL);

			for (i = 1; i < xref->len; i++)
			{
				if (xref->table[i].type == 0 || xref->table[i].type == 'f')
					continue;

				fz_try(ctx)
				{
					dict = pdf_load_object(xref, i, 0);
				}
				fz_catch(ctx)
				{
					fz_warn(ctx, "ignoring broken object (%d 0 R)", i);
					continue;
				}

				if (!hasroot)
				{
					obj = pdf_dict_gets(dict, "Type");
					if (pdf_is_name(obj) && !strcmp(pdf_to_name(obj), "Catalog"))
					{
						nobj = pdf_new_indirect(ctx, i, 0, xref);
						pdf_dict_puts(xref->trailer, "Root", nobj);
						pdf_drop_obj(nobj);
						nobj = NULL;
					}
				}

				if (!hasinfo)
				{
					if (pdf_dict_gets(dict, "Creator") || pdf_dict_gets(dict, "Producer"))
					{
						nobj = pdf_new_indirect(ctx, i, 0, xref);
						pdf_dict_puts(xref->trailer, "Info", nobj);
						pdf_drop_obj(nobj);
						nobj = NULL;
					}
				}

				pdf_drop_obj(dict);
				dict = NULL;
			}
		}
		xref->js = pdf_new_js(xref);
		pdf_js_load_document_level(xref->js);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(dict);
		pdf_drop_obj(nobj);
		pdf_close_document(xref);
		fz_throw(ctx, "cannot open document");
	}

	fz_try(ctx)
	{
		pdf_read_ocg(xref);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "Ignoring Broken Optional Content");
	}

	/* SumatraPDF: update xref->version with /Version */
	obj = pdf_dict_getp(xref->trailer, "Root/Version");
	if (pdf_is_name(obj))
	{
		int version = (int)(fz_atof(pdf_to_name(obj)) * 10 + 0.1);
		if (version > xref->version)
			xref->version = version;
	}
}

void
pdf_close_document(pdf_document *xref)
{
	int i;
	fz_context *ctx;

	if (!xref)
		return;
	ctx = xref->ctx;

	pdf_drop_js(xref->js);

	if (xref->table)
	{
		for (i = 0; i < xref->len; i++)
		{
			if (xref->table[i].obj)
			{
				pdf_drop_obj(xref->table[i].obj);
				xref->table[i].obj = NULL;
				fz_drop_buffer(ctx, xref->table[i].stm_buf);
			}
		}
		fz_free(xref->ctx, xref->table);
	}

	if (xref->page_objs)
	{
		for (i = 0; i < xref->page_len; i++)
			pdf_drop_obj(xref->page_objs[i]);
		fz_free(ctx, xref->page_objs);
	}

	if (xref->page_refs)
	{
		for (i = 0; i < xref->page_len; i++)
			pdf_drop_obj(xref->page_refs[i]);
		fz_free(ctx, xref->page_refs);
	}

	if (xref->focus_obj)
		pdf_drop_obj(xref->focus_obj);
	if (xref->file)
		fz_close(xref->file);
	pdf_drop_obj(xref->trailer);
	if (xref->crypt)
		pdf_free_crypt(ctx, xref->crypt);

	pdf_free_ocg(ctx, xref->ocg);

	fz_empty_store(ctx);

	pdf_lexbuf_fin(&xref->lexbuf.base);

	fz_free(ctx, xref);
}

void
pdf_print_xref(pdf_document *xref)
{
	int i;
	printf("xref\n0 %d\n", xref->len);
	for (i = 0; i < xref->len; i++)
	{
		printf("%05d: %010d %05d %c (stm_ofs=%d; stm_buf=%p)\n", i,
			xref->table[i].ofs,
			xref->table[i].gen,
			xref->table[i].type ? xref->table[i].type : '-',
			xref->table[i].stm_ofs,
			xref->table[i].stm_buf);
	}
}

/*
 * compressed object streams
 */

static void
pdf_load_obj_stm(pdf_document *xref, int num, int gen, pdf_lexbuf *buf)
{
	fz_stream *stm = NULL;
	pdf_obj *objstm = NULL;
	int *numbuf = NULL;
	int *ofsbuf = NULL;

	pdf_obj *obj;
	int first;
	int count;
	int i;
	pdf_token tok;
	fz_context *ctx = xref->ctx;

	fz_var(numbuf);
	fz_var(ofsbuf);
	fz_var(objstm);
	fz_var(stm);

	fz_try(ctx)
	{
		objstm = pdf_load_object(xref, num, gen);

		count = pdf_to_int(pdf_dict_gets(objstm, "N"));
		first = pdf_to_int(pdf_dict_gets(objstm, "First"));

		if (count < 0)
			fz_throw(ctx, "negative number of objects in object stream");
		if (first < 0)
			fz_throw(ctx, "first object in object stream resides outside stream");

		numbuf = fz_calloc(ctx, count, sizeof(int));
		ofsbuf = fz_calloc(ctx, count, sizeof(int));

		stm = pdf_open_stream(xref, num, gen);
		for (i = 0; i < count; i++)
		{
			tok = pdf_lex(stm, buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, "corrupt object stream (%d %d R)", num, gen);
			numbuf[i] = buf->i;

			tok = pdf_lex(stm, buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, "corrupt object stream (%d %d R)", num, gen);
			ofsbuf[i] = buf->i;
		}

		fz_seek(stm, first, 0);

		for (i = 0; i < count; i++)
		{
			fz_seek(stm, first + ofsbuf[i], 0);

			obj = pdf_parse_stm_obj(xref, stm, buf);

			if (numbuf[i] < 1 || numbuf[i] >= xref->len)
			{
				pdf_drop_obj(obj);
				fz_throw(ctx, "object id (%d 0 R) out of range (0..%d)", numbuf[i], xref->len - 1);
			}

			if (xref->table[numbuf[i]].type == 'o' && xref->table[numbuf[i]].ofs == num)
			{
				/* If we already have an entry for this object,
				 * we'd like to drop it and use the new one -
				 * but this means that anyone currently holding
				 * a pointer to the old one will be left with a
				 * stale pointer. Instead, we drop the new one
				 * and trust that the old one is correct. */
				if (xref->table[numbuf[i]].obj) {
					if (pdf_objcmp(xref->table[numbuf[i]].obj, obj))
						fz_warn(ctx, "Encountered new definition for object %d - keeping the original one", numbuf[i]);
					pdf_drop_obj(obj);
				} else
					xref->table[numbuf[i]].obj = obj;
			}
			else
			{
				pdf_drop_obj(obj);
			}
		}
	}
	fz_always(ctx)
	{
		fz_close(stm);
		fz_free(xref->ctx, ofsbuf);
		fz_free(xref->ctx, numbuf);
		pdf_drop_obj(objstm);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot open object stream (%d %d R)", num, gen);
	}
}

/*
 * object loading
 */

void
pdf_cache_object(pdf_document *xref, int num, int gen)
{
	pdf_xref_entry *x;
	int rnum, rgen;
	fz_context *ctx = xref->ctx;

	if (num < 0 || num >= xref->len)
		fz_throw(ctx, "object out of range (%d %d R); xref size %d", num, gen, xref->len);

	x = &xref->table[num];

	if (x->obj)
		return;

	if (x->type == 'f')
	{
		x->obj = pdf_new_null(ctx);
		return;
	}
	else if (x->type == 'n')
	{
		fz_seek(xref->file, x->ofs, 0);

		fz_try(ctx)
		{
			x->obj = pdf_parse_ind_obj(xref, xref->file, &xref->lexbuf.base,
					&rnum, &rgen, &x->stm_ofs);
		}
		fz_catch(ctx)
		{
			fz_throw(ctx, "cannot parse object (%d %d R)", num, gen);
		}

		if (rnum != num)
		{
			pdf_drop_obj(x->obj);
			x->obj = NULL;
			fz_throw(ctx, "found object (%d %d R) instead of (%d %d R)", rnum, rgen, num, gen);
		}

		if (xref->crypt)
			pdf_crypt_obj(ctx, xref->crypt, x->obj, num, gen);
	}
	else if (x->type == 'o')
	{
		if (!x->obj)
		{
			fz_try(ctx)
			{
				pdf_load_obj_stm(xref, x->ofs, 0, &xref->lexbuf.base);
			}
			fz_catch(ctx)
			{
				fz_throw(ctx, "cannot load object stream containing object (%d %d R)", num, gen);
			}
			if (!x->obj)
				fz_throw(ctx, "object (%d %d R) was not found in its object stream", num, gen);
		}
	}
	else
	{
		fz_throw(ctx, "cannot find object in xref (%d %d R)", num, gen);
	}
}

pdf_obj *
pdf_load_object(pdf_document *xref, int num, int gen)
{
	fz_context *ctx = xref->ctx;

	fz_try(ctx)
	{
		pdf_cache_object(xref, num, gen);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot load object (%d %d R) into cache", num, gen);
	}

	assert(xref->table[num].obj);

	return pdf_keep_obj(xref->table[num].obj);
}

pdf_obj *
pdf_resolve_indirect(pdf_obj *ref)
{
	int sanity = 10;
	int num;
	int gen;
	fz_context *ctx = NULL; /* Avoid warning for stupid compilers */
	pdf_document *xref;

	while (pdf_is_indirect(ref))
	{
		if (--sanity == 0)
		{
			fz_warn(ctx, "Too many indirections (possible indirection cycle involving %d %d R)", num, gen);
			return NULL;
		}
		xref = pdf_get_indirect_document(ref);
		if (!xref)
			return NULL;
		ctx = xref->ctx;
		num = pdf_to_num(ref);
		gen = pdf_to_gen(ref);
		fz_try(ctx)
		{
			pdf_cache_object(xref, num, gen);
		}
		fz_catch(ctx)
		{
			fz_warn(ctx, "cannot load object (%d %d R) into cache", num, gen);
			return NULL;
		}
		if (!xref->table[num].obj)
			return NULL;
		ref = xref->table[num].obj;
	}

	return ref;
}

int
pdf_count_objects(pdf_document *doc)
{
	return doc->len;
}

int
pdf_create_object(pdf_document *xref)
{
	/* TODO: reuse free object slots by properly linking free object chains in the ofs field */
	int num = xref->len;
	pdf_resize_xref(xref, num + 1);
	xref->table[num].type = 'f';
	xref->table[num].ofs = -1;
	xref->table[num].gen = 0;
	xref->table[num].stm_ofs = 0;
	xref->table[num].stm_buf = NULL;
	xref->table[num].obj = NULL;
	return num;
}

void
pdf_delete_object(pdf_document *xref, int num)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= xref->len)
	{
		fz_warn(xref->ctx, "object out of range (%d 0 R); xref size %d", num, xref->len);
		return;
	}

	x = &xref->table[num];

	fz_drop_buffer(xref->ctx, x->stm_buf);
	pdf_drop_obj(x->obj);

	x->type = 'f';
	x->ofs = 0;
	x->gen = 0;
	x->stm_ofs = 0;
	x->stm_buf = NULL;
	x->obj = NULL;
}

void
pdf_update_object(pdf_document *xref, int num, pdf_obj *newobj)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= xref->len)
	{
		fz_warn(xref->ctx, "object out of range (%d 0 R); xref size %d", num, xref->len);
		return;
	}

	x = &xref->table[num];

	pdf_drop_obj(x->obj);

	x->type = 'n';
	x->ofs = 0;
	x->obj = pdf_keep_obj(newobj);
}

void
pdf_update_stream(pdf_document *xref, int num, fz_buffer *newbuf)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= xref->len)
	{
		fz_warn(xref->ctx, "object out of range (%d 0 R); xref size %d", num, xref->len);
		return;
	}

	x = &xref->table[num];

	fz_drop_buffer(xref->ctx, x->stm_buf);
	x->stm_buf = fz_keep_buffer(xref->ctx, newbuf);
}

int
pdf_meta(pdf_document *doc, int key, void *ptr, int size)
{
	switch(key)
	{
	/*
		ptr: Pointer to block (uninitialised on entry)
		size: Size of block (at least 64 bytes)
		Returns: Document format as a brief text string.
	*/
	case FZ_META_FORMAT_INFO:
		sprintf((char *)ptr, "PDF %d.%d", doc->version/10, doc->version % 10);
		return FZ_META_OK;
	case FZ_META_CRYPT_INFO:
		if (doc->crypt)
			sprintf((char *)ptr, "Standard V%d R%d %d-bit %s",
				pdf_crypt_version(doc),
				pdf_crypt_revision(doc),
				pdf_crypt_length(doc),
				pdf_crypt_method(doc));
		else
			sprintf((char *)ptr, "None");
		return FZ_META_OK;
	case FZ_META_HAS_PERMISSION:
	{
		int i;
		switch (size)
		{
		case FZ_PERMISSION_PRINT:
			i = PDF_PERM_PRINT;
			break;
		case FZ_PERMISSION_CHANGE:
			i = PDF_PERM_CHANGE;
			break;
		case FZ_PERMISSION_COPY:
			i = PDF_PERM_COPY;
			break;
		case FZ_PERMISSION_NOTES:
			i = PDF_PERM_NOTES;
			break;
		default:
			return 0;
		}
		return pdf_has_permission(doc, i);
	}
	case FZ_META_INFO:
	{
		pdf_obj *info = pdf_dict_gets(doc->trailer, "Info");
		if (!info)
		{
			if (ptr)
				*(char *)ptr = 0;
			return 0;
		}
		info = pdf_dict_gets(info, *(char **)ptr);
		if (!info)
		{
			if (ptr)
				*(char *)ptr = 0;
			return 0;
		}
		if (info && ptr && size)
		{
			char *utf8 = pdf_to_utf8(doc, info);
			fz_strlcpy(ptr, utf8, size);
			fz_free(doc->ctx, utf8);
		}
		return 1;
	}
	default:
		return FZ_META_UNKNOWN_KEY;
	}
}

fz_transition *
pdf_page_presentation(pdf_document *doc, pdf_page *page, float *duration)
{
	*duration = page->duration;
	if (!page->transition_present)
		return NULL;
	return &page->transition;
}

static fz_interactive *
pdf_interact(pdf_document *doc)
{
	return (fz_interactive *)doc;
}

/*
	Initializers for the fz_document interface.

	The functions are split across two files to allow calls to a
	version of the constructor that does not link in the interpreter.
	The interpreter references the built-in font and cmap resources
	which are quite big. Not linking those into the mubusy binary
	saves roughly 6MB of space.
*/

static pdf_document *
pdf_new_document(fz_context *ctx, fz_stream *file)
{
	pdf_document *doc = fz_malloc_struct(ctx, pdf_document);

	doc->super.close = (void*)pdf_close_document;
	doc->super.needs_password = (void*)pdf_needs_password;
	doc->super.authenticate_password = (void*)pdf_authenticate_password;
	doc->super.load_outline = (void*)pdf_load_outline;
	doc->super.count_pages = (void*)pdf_count_pages;
	doc->super.load_page = (void*)pdf_load_page;
	doc->super.load_links = (void*)pdf_load_links;
	doc->super.bound_page = (void*)pdf_bound_page;
	doc->super.first_annot = (void*)pdf_first_annot;
	doc->super.next_annot = (void*)pdf_next_annot;
	doc->super.bound_annot = (void*)pdf_bound_annot;
	doc->super.run_page_contents = NULL; /* see pdf_xref_aux.c */
	doc->super.run_annot = NULL; /* see pdf_xref_aux.c */
	doc->super.free_page = (void*)pdf_free_page;
	doc->super.meta = (void*)pdf_meta;
	doc->super.page_presentation = (void*)pdf_page_presentation;
	doc->super.interact = (void*)pdf_interact;
	doc->super.write = (void*)pdf_write_document;

	pdf_lexbuf_init(ctx, &doc->lexbuf.base, PDF_LEXBUF_LARGE);
	doc->file = fz_keep_stream(file);
	doc->ctx = ctx;

	return doc;
}

pdf_document *
pdf_open_document_no_run_with_stream(fz_context *ctx, fz_stream *file)
{
	pdf_document *doc = pdf_new_document(ctx, file);
	pdf_init_document(doc);
	return doc;
}

pdf_document *
pdf_open_document_no_run(fz_context *ctx, const char *filename)
{
	fz_stream *file = NULL;
	pdf_document *doc;

	fz_var(file);

	fz_try(ctx)
	{
		file = fz_open_file(ctx, filename);
		doc = pdf_new_document(ctx, file);
		pdf_init_document(doc);
	}
	fz_always(ctx)
	{
		fz_close(file);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot load document '%s'", filename);
	}
	return doc;
}

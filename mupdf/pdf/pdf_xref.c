#include "fitz.h"
#include "mupdf.h"

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
pdf_load_version(pdf_xref *xref)
{
	char buf[20];

	fz_seek(xref->file, 0, 0);
	fz_read_line(xref->file, buf, sizeof buf);
	if (memcmp(buf, "%PDF-", 5) != 0)
		fz_throw(xref->ctx, "cannot recognize version marker");

	xref->version = fz_atof(buf + 5) * 10 + 0.1; /* SumatraPDF: don't accidentally round down */
}

static void
pdf_read_start_xref(pdf_xref *xref)
{
	unsigned char buf[1024];
	int t, n;
	int i;

	fz_seek(xref->file, 0, 2);

	xref->file_size = fz_tell(xref->file);

	t = MAX(0, xref->file_size - (int)sizeof buf);
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
pdf_read_old_trailer(pdf_xref *xref, char *buf, int cap)
{
	int len;
	char *s;
	int n;
	int t;
	int tok;
	int c;

	fz_read_line(xref->file, buf, cap);
	if (strncmp(buf, "xref", 4) != 0)
		fz_throw(xref->ctx, "cannot find xref marker");

	while (1)
	{
		c = fz_peek_byte(xref->file);
		if (!(c >= '0' && c <= '9'))
			break;

		fz_read_line(xref->file, buf, cap);
		s = buf;
		fz_strsep(&s, " "); /* ignore ofs */
		if (!s)
			fz_throw(xref->ctx, "invalid range marker in xref");
		len = atoi(fz_strsep(&s, " "));

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
		tok = pdf_lex(xref->file, buf, cap, &n);
		if (tok != PDF_TOK_TRAILER)
			fz_throw(xref->ctx, "expected trailer marker");

		tok = pdf_lex(xref->file, buf, cap, &n);
		if (tok != PDF_TOK_OPEN_DICT)
			fz_throw(xref->ctx, "expected trailer dictionary");

		xref->trailer = pdf_parse_dict(xref, xref->file, buf, cap);
	}
	fz_catch(xref->ctx)
	{
		fz_throw(xref->ctx, "cannot parse trailer");
	}
}

static void
pdf_read_new_trailer(pdf_xref *xref, char *buf, int cap)
{
	fz_try(xref->ctx)
	{
		xref->trailer = pdf_parse_ind_obj(xref, xref->file, buf, cap, NULL, NULL, NULL);
	}
	fz_catch(xref->ctx)
	{
		fz_throw(xref->ctx, "cannot parse trailer (compressed)");
	}
}

static void
pdf_read_trailer(pdf_xref *xref, char *buf, int cap)
{
	int c;

	fz_seek(xref->file, xref->startxref, 0);

	while (iswhite(fz_peek_byte(xref->file)))
		fz_read_byte(xref->file);

	fz_try(xref->ctx)
	{
		c = fz_peek_byte(xref->file);
		if (c == 'x')
			pdf_read_old_trailer(xref, buf, cap);
		else if (c >= '0' && c <= '9')
			pdf_read_new_trailer(xref, buf, cap);
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
pdf_resize_xref(pdf_xref *xref, int newlen)
{
	int i;

	xref->table = fz_resize_array(xref->ctx, xref->table, newlen, sizeof(pdf_xref_entry));
	for (i = xref->len; i < newlen; i++)
	{
		xref->table[i].type = 0;
		xref->table[i].ofs = 0;
		xref->table[i].gen = 0;
		xref->table[i].stm_ofs = 0;
		xref->table[i].obj = NULL;
	}
	xref->len = newlen;
}

static fz_obj *
pdf_read_old_xref(pdf_xref *xref, char *buf, int cap)
{
	int ofs, len;
	char *s;
	int n;
	int tok;
	int i;
	int c;
	fz_obj *trailer;

	fz_read_line(xref->file, buf, cap);
	if (strncmp(buf, "xref", 4) != 0)
		fz_throw(xref->ctx, "cannot find xref marker");

	while (1)
	{
		c = fz_peek_byte(xref->file);
		if (!(c >= '0' && c <= '9'))
			break;

		fz_read_line(xref->file, buf, cap);
		s = buf;
		ofs = atoi(fz_strsep(&s, " "));
		len = atoi(fz_strsep(&s, " "));

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
			n = fz_read(xref->file, (unsigned char *) buf, 20);
			if (n < 0)
				fz_throw(xref->ctx, "cannot read xref table");
			if (!xref->table[i].type)
			{
				s = buf;

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
		tok = pdf_lex(xref->file, buf, cap, &n);
		if (tok != PDF_TOK_TRAILER)
			fz_throw(xref->ctx, "expected trailer marker");

		tok = pdf_lex(xref->file, buf, cap, &n);
		if (tok != PDF_TOK_OPEN_DICT)
			fz_throw(xref->ctx, "expected trailer dictionary");

		trailer = pdf_parse_dict(xref, xref->file, buf, cap);
	}
	fz_catch(xref->ctx)
	{
		fz_throw(xref->ctx, "cannot parse trailer");
	}
	return trailer;
}

static void
pdf_read_new_xref_section(pdf_xref *xref, fz_stream *stm, int i0, int i1, int w0, int w1, int w2)
{
	int i, n;

	if (i0 < 0 || i0 + i1 > xref->len)
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

static fz_obj *
pdf_read_new_xref(pdf_xref *xref, char *buf, int cap)
{
	fz_stream *stm = NULL;
	fz_obj *trailer = NULL;
	fz_obj *index = NULL;
	fz_obj *obj = NULL;
	int num, gen, stm_ofs;
	int size, w0, w1, w2;
	int t;
	fz_context *ctx = xref->ctx;

	fz_var(trailer);
	fz_var(stm);

	fz_try(ctx)
	{
		trailer = pdf_parse_ind_obj(xref, xref->file, buf, cap, &num, &gen, &stm_ofs);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot parse compressed xref stream object");
	}

	fz_try(ctx)
	{
		obj = fz_dict_gets(trailer, "Size");
		if (!obj)
			fz_throw(ctx, "xref stream missing Size entry (%d %d R)", num, gen);

		size = fz_to_int(obj);
		if (size > xref->len)
			pdf_resize_xref(xref, size);

		if (num < 0 || num >= xref->len)
			fz_throw(ctx, "object id (%d %d R) out of range (0..%d)", num, gen, xref->len - 1);

		obj = fz_dict_gets(trailer, "W");
		if (!obj)
			fz_throw(ctx, "xref stream missing W entry (%d %d R)", num, gen);
		w0 = fz_to_int(fz_array_get(obj, 0));
		w1 = fz_to_int(fz_array_get(obj, 1));
		w2 = fz_to_int(fz_array_get(obj, 2));

		index = fz_dict_gets(trailer, "Index");

		stm = pdf_open_stream_at(xref, num, gen, trailer, stm_ofs);
		/* RJW: Ensure pdf_open_stream does fz_throw(ctx, "cannot open compressed xref stream (%d %d R)", num, gen); */

		if (!index)
		{
			pdf_read_new_xref_section(xref, stm, 0, size, w0, w1, w2);
			/* RJW: Ensure above does fz_throw(ctx, "cannot read xref stream (%d %d R)", num, gen); */
		}
		else
		{
			int n = fz_array_len(index);
			for (t = 0; t < n; t += 2)
			{
				int i0 = fz_to_int(fz_array_get(index, t + 0));
				int i1 = fz_to_int(fz_array_get(index, t + 1));
				pdf_read_new_xref_section(xref, stm, i0, i1, w0, w1, w2);
			}
		}
	}
	fz_catch(ctx)
	{
		fz_close(stm);
		fz_drop_obj(trailer);
		fz_drop_obj(index);
		fz_drop_obj(obj);
		fz_rethrow(ctx);
	}

	fz_close(stm);

	return trailer;
}

static fz_obj *
pdf_read_xref(pdf_xref *xref, int ofs, char *buf, int cap)
{
	int c;
	fz_context *ctx = xref->ctx;
	fz_obj *trailer;

	fz_seek(xref->file, ofs, 0);

	while (iswhite(fz_peek_byte(xref->file)))
		fz_read_byte(xref->file);

	fz_try(ctx)
	{
		c = fz_peek_byte(xref->file);
		if (c == 'x')
			trailer = pdf_read_old_xref(xref, buf, cap);
		else if (c >= '0' && c <= '9')
			trailer = pdf_read_new_xref(xref, buf, cap);
		else
			fz_throw(ctx, "cannot recognize xref format");
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot read xref (ofs=%d)", ofs);
	}
	return trailer;
}

static void
pdf_read_xref_sections(pdf_xref *xref, int ofs, char *buf, int cap)
{
	fz_obj *trailer = NULL;
	fz_obj *xrefstm = NULL;
	fz_obj *prev = NULL;
	fz_context *ctx = xref->ctx;

	fz_try(ctx)
	{
		trailer = pdf_read_xref(xref, ofs, buf, cap);

		/* FIXME: do we overwrite free entries properly? */
		xrefstm = fz_dict_gets(trailer, "XRefStm");
		if (xrefstm)
			pdf_read_xref_sections(xref, fz_to_int(xrefstm), buf, cap);

		prev = fz_dict_gets(trailer, "Prev");
		if (prev)
			pdf_read_xref_sections(xref, fz_to_int(prev), buf, cap);
	}
	fz_catch(ctx)
	{
		fz_drop_obj(trailer);
		fz_throw(ctx, "cannot read xref at offset %d", ofs);
	}

	fz_drop_obj(trailer);
}

/*
 * load xref tables from pdf
 */

static void
pdf_load_xref(pdf_xref *xref, char *buf, int bufsize)
{
	fz_obj *size;
	int i;
	fz_context *ctx = xref->ctx;

	pdf_load_version(xref);

	pdf_read_start_xref(xref);

	pdf_read_trailer(xref, buf, bufsize);

	size = fz_dict_gets(xref->trailer, "Size");
	if (!size)
		fz_throw(ctx, "trailer missing Size entry");

	pdf_resize_xref(xref, fz_to_int(size));

	pdf_read_xref_sections(xref, xref->startxref, buf, bufsize);

	/* broken pdfs where first object is not free */
	if (xref->table[0].type != 'f')
		fz_throw(ctx, "first object in xref is not free");

	/* broken pdfs where object offsets are out of range */
	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].type == 'n')
			if (xref->table[i].ofs <= 0 || xref->table[i].ofs >= xref->file_size)
				fz_throw(ctx, "object offset out of range: %d (%d 0 R)", xref->table[i].ofs, i);
		if (xref->table[i].type == 'o')
			if (xref->table[i].ofs <= 0 || xref->table[i].ofs >= xref->len || xref->table[xref->table[i].ofs].type != 'n')
				fz_throw(ctx, "invalid reference to an objstm that does not exist: %d (%d 0 R)", xref->table[i].ofs, i);
	}
}

void
pdf_ocg_set_config(pdf_xref *xref, int config)
{
	int i, j, len, len2;
	pdf_ocg_descriptor *desc = xref->ocg;
	fz_obj *obj, *cobj;
	char *name;

	obj = fz_dict_gets(fz_dict_gets(xref->trailer, "Root"), "OCProperties");
	if (!obj)
	{
		if (config == 0)
			return;
		else
			fz_throw(xref->ctx, "Unknown OCG config (None known!)");
	}
	if (config == 0)
	{
		cobj = fz_dict_gets(obj, "D");
		if (!cobj)
			fz_throw(xref->ctx, "No default OCG config");
	}
	else
	{
		cobj = fz_array_get(fz_dict_gets(obj, "Configs"), config);
		if (!cobj)
			fz_throw(xref->ctx, "Illegal OCG config");
	}

	if (desc->intent)
		fz_drop_obj(desc->intent);
	desc->intent = fz_dict_gets(cobj, "Intent");
	if (desc->intent)
		fz_keep_obj(desc->intent);

	len = desc->len;
	name = fz_to_name(fz_dict_gets(cobj, "BaseState"));
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

	obj = fz_dict_gets(cobj, "ON");
	len2 = fz_array_len(obj);
	for (i = 0; i < len2; i++)
	{
		fz_obj *o = fz_array_get(obj, i);
		int n = fz_to_num(o);
		int g = fz_to_gen(o);
		for (j=0; j < len; j++)
		{
			if (desc->ocgs[j].num == n && desc->ocgs[j].gen == g)
			{
				desc->ocgs[j].state = 1;
				break;
			}
		}
	}

	obj = fz_dict_gets(cobj, "OFF");
	len2 = fz_array_len(obj);
	for (i = 0; i < len2; i++)
	{
		fz_obj *o = fz_array_get(obj, i);
		int n = fz_to_num(o);
		int g = fz_to_gen(o);
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
pdf_read_ocg(pdf_xref *xref)
{
	fz_obj *obj, *ocg;
	int len, i;
	pdf_ocg_descriptor *desc;
	fz_context *ctx = xref->ctx;

	fz_var(desc);

	obj = fz_dict_gets(fz_dict_gets(xref->trailer, "Root"), "OCProperties");
	if (!obj)
		return;
	ocg = fz_dict_gets(obj, "OCGs");
	if (!ocg || !fz_is_array(ocg))
		/* Not ever supposed to happen, but live with it. */
		return;
	len = fz_array_len(ocg);
	fz_try(ctx)
	{
		desc = fz_calloc(ctx, 1, sizeof(*desc));
		desc->len = len;
		desc->ocgs = fz_calloc(ctx, len, sizeof(*desc->ocgs));
		desc->intent = NULL;
		for (i=0; i < len; i++)
		{
			fz_obj *o = fz_array_get(ocg, i);
			desc->ocgs[i].num = fz_to_num(o);
			desc->ocgs[i].gen = fz_to_gen(o);
			desc->ocgs[i].state = 0;
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

	if (desc->intent)
		fz_drop_obj(desc->intent);
	fz_free(ctx, desc->ocgs);
	fz_free(ctx, desc);
}

/*
 * Initialize and load xref tables.
 * If password is not null, try to decrypt.
 */

pdf_xref *
pdf_open_xref_with_stream(fz_stream *file, char *password)
{
	pdf_xref *xref;
	fz_obj *encrypt, *id;
	fz_obj *dict = NULL;
	fz_obj *obj;
	fz_obj *nobj = NULL;
	int i, repaired = 0;
	fz_context *ctx = file->ctx;

	fz_var(dict);
	fz_var(nobj);

	/* install pdf specific callback */
	fz_resolve_indirect = pdf_resolve_indirect;

	xref = fz_calloc(ctx, 1, sizeof(pdf_xref));
	xref->file = fz_keep_stream(file);
	xref->ctx = ctx;

	fz_try(ctx)
	{
		pdf_load_xref(xref, xref->scratch, sizeof xref->scratch);
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
			fz_drop_obj(xref->trailer);
			xref->trailer = NULL;
		}
		fz_warn(xref->ctx, "trying to repair broken xref");
		repaired = 1;
	}

	fz_try(ctx)
	{
		int hasroot, hasinfo;

		if (repaired)
			pdf_repair_xref(xref, xref->scratch, sizeof xref->scratch);

		encrypt = fz_dict_gets(xref->trailer, "Encrypt");
		id = fz_dict_gets(xref->trailer, "ID");
		if (fz_is_dict(encrypt))
			xref->crypt = pdf_new_crypt(ctx, encrypt, id);

		if (pdf_needs_password(xref))
		{
			/* Only care if we have a password */
			if (password)
			{
				int okay = pdf_authenticate_password(xref, password);
				if (!okay)
				{
					fz_throw(ctx, "invalid password");
				}
			}
		}

		if (repaired)
		{
			pdf_repair_obj_stms(xref);

			hasroot = (fz_dict_gets(xref->trailer, "Root") != NULL);
			hasinfo = (fz_dict_gets(xref->trailer, "Info") != NULL);

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
					obj = fz_dict_gets(dict, "Type");
					if (fz_is_name(obj) && !strcmp(fz_to_name(obj), "Catalog"))
					{
						nobj = fz_new_indirect(ctx, i, 0, xref);
						fz_dict_puts(xref->trailer, "Root", nobj);
						fz_drop_obj(nobj);
						nobj = NULL;
					}
				}

				if (!hasinfo)
				{
					if (fz_dict_gets(dict, "Creator") || fz_dict_gets(dict, "Producer"))
					{
						nobj = fz_new_indirect(ctx, i, 0, xref);
						fz_dict_puts(xref->trailer, "Info", nobj);
						fz_drop_obj(nobj);
						nobj = NULL;
					}
				}

				fz_drop_obj(dict);
				dict = NULL;
			}
		}
	}
	fz_catch(ctx)
	{
		fz_drop_obj(dict);
		fz_drop_obj(nobj);
		pdf_free_xref(xref);
		fz_throw(ctx, "cannot open document");
	}

	fz_try(ctx)
	{
		pdf_read_ocg(xref);
	}
	fz_catch(ctx)
	{
		/* SumatraPDF: this is far from critical */
		fz_warn(ctx, "Ignoring broken OCG configuration");
	}

	return xref;
}

void
pdf_free_xref(pdf_xref *xref)
{
	int i;
	fz_context *ctx;

	if (!xref)
		return;
	ctx = xref->ctx;

	if (xref->table)
	{
		for (i = 0; i < xref->len; i++)
		{
			if (xref->table[i].obj)
			{
				fz_drop_obj(xref->table[i].obj);
				xref->table[i].obj = NULL;
			}
		}
		fz_free(xref->ctx, xref->table);
	}

	if (xref->page_objs)
	{
		for (i = 0; i < xref->page_len; i++)
			fz_drop_obj(xref->page_objs[i]);
		fz_free(ctx, xref->page_objs);
	}

	if (xref->page_refs)
	{
		for (i = 0; i < xref->page_len; i++)
			fz_drop_obj(xref->page_refs[i]);
		fz_free(ctx, xref->page_refs);
	}

	if (xref->file)
		fz_close(xref->file);
	if (xref->trailer)
		fz_drop_obj(xref->trailer);
	if (xref->crypt)
		pdf_free_crypt(ctx, xref->crypt);

	pdf_free_ocg(ctx, xref->ocg);

	fz_free(ctx, xref);
}

void
pdf_debug_xref(pdf_xref *xref)
{
	int i;
	printf("xref\n0 %d\n", xref->len);
	for (i = 0; i < xref->len; i++)
	{
		printf("%05d: %010d %05d %c (stm_ofs=%d)\n", i,
			xref->table[i].ofs,
			xref->table[i].gen,
			xref->table[i].type ? xref->table[i].type : '-',
			xref->table[i].stm_ofs);
	}
}

/*
 * compressed object streams
 */

static void
pdf_load_obj_stm(pdf_xref *xref, int num, int gen, char *buf, int cap)
{
	fz_stream *stm = NULL;
	fz_obj *objstm = NULL;
	int *numbuf = NULL;
	int *ofsbuf = NULL;

	fz_obj *obj;
	int first;
	int count;
	int i, n;
	int tok;
	fz_context *ctx = xref->ctx;

	fz_var(numbuf);
	fz_var(ofsbuf);
	fz_var(objstm);
	fz_var(stm);

	fz_try(ctx)
	{
		objstm = pdf_load_object(xref, num, gen);

		count = fz_to_int(fz_dict_gets(objstm, "N"));
		first = fz_to_int(fz_dict_gets(objstm, "First"));

		numbuf = fz_calloc(ctx, count, sizeof(int));
		ofsbuf = fz_calloc(ctx, count, sizeof(int));

		stm = pdf_open_stream(xref, num, gen);
		for (i = 0; i < count; i++)
		{
			tok = pdf_lex(stm, buf, cap, &n);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, "corrupt object stream (%d %d R)", num, gen);
			numbuf[i] = atoi(buf);

			tok = pdf_lex(stm, buf, cap, &n);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, "corrupt object stream (%d %d R)", num, gen);
			ofsbuf[i] = atoi(buf);
		}

		fz_seek(stm, first, 0);

		for (i = 0; i < count; i++)
		{
			fz_seek(stm, first + ofsbuf[i], 0);

			obj = pdf_parse_stm_obj(xref, stm, buf, cap);
			/* RJW: Ensure above does fz_throw(ctx, "cannot parse object %d in stream (%d %d R)", i, num, gen); */

			if (numbuf[i] < 1 || numbuf[i] >= xref->len)
			{
				fz_drop_obj(obj);
				fz_throw(ctx, "object id (%d 0 R) out of range (0..%d)", numbuf[i], xref->len - 1);
			}

			if (xref->table[numbuf[i]].type == 'o' && xref->table[numbuf[i]].ofs == num)
			{
				if (xref->table[numbuf[i]].obj)
					fz_drop_obj(xref->table[numbuf[i]].obj);
				xref->table[numbuf[i]].obj = obj;
			}
			else
			{
				fz_drop_obj(obj);
			}
		}
	}
	fz_catch(ctx)
	{
		fz_close(stm);
		fz_free(xref->ctx, ofsbuf);
		fz_free(xref->ctx, numbuf);
		fz_drop_obj(objstm);
		fz_throw(ctx, "cannot open object stream (%d %d R)", num, gen);
	}
	fz_close(stm);
	fz_free(xref->ctx, ofsbuf);
	fz_free(xref->ctx, numbuf);
	fz_drop_obj(objstm);
}

/*
 * object loading
 */

void
pdf_cache_object(pdf_xref *xref, int num, int gen)
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
		x->obj = fz_new_null(ctx);
		return;
	}
	else if (x->type == 'n')
	{
		fz_seek(xref->file, x->ofs, 0);

		fz_try(ctx)
		{
			x->obj = pdf_parse_ind_obj(xref, xref->file, xref->scratch, sizeof xref->scratch,
					&rnum, &rgen, &x->stm_ofs);
		}
		fz_catch(ctx)
		{
			fz_throw(ctx, "cannot parse object (%d %d R)", num, gen);
		}

		if (rnum != num)
		{
			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1728 */
			fz_drop_obj(x->obj);
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
				pdf_load_obj_stm(xref, x->ofs, 0, xref->scratch, sizeof xref->scratch);
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
		fz_throw(ctx, "assert: corrupt xref struct");
	}
}

fz_obj *
pdf_load_object(pdf_xref *xref, int num, int gen)
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

	return fz_keep_obj(xref->table[num].obj);
}

fz_obj *
pdf_resolve_indirect(fz_obj *ref)
{
	if (fz_is_indirect(ref))
	{
		pdf_xref *xref = fz_get_indirect_xref(ref);
		if (xref)
		{
			int num = fz_to_num(ref);
			int gen = fz_to_gen(ref);
			fz_context *ctx = xref->ctx;
			fz_try(ctx)
			{
				pdf_cache_object(xref, num, gen);
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "cannot load object (%d %d R) into cache", num, gen);
				return ref;
			}
			/* SumatraPDF: base_object.c can't handle multiple indirections */
			if (fz_is_indirect(xref->table[num].obj))
			{
				fz_warn(ctx, "ignoring unexpected double-indirection (%d %d R)", num, gen);
				return ref;
			}
			if (xref->table[num].obj)
				return xref->table[num].obj;
		}
	}
	return ref;
}

/* Replace numbered object -- for use by pdfclean and similar tools */
void
pdf_update_object(pdf_xref *xref, int num, int gen, fz_obj *newobj)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= xref->len)
	{
		fz_warn(xref->ctx, "object out of range (%d %d R); xref size %d", num, gen, xref->len);
		return;
	}

	x = &xref->table[num];

	if (x->obj)
		fz_drop_obj(x->obj);

	x->obj = fz_keep_obj(newobj);
	x->type = 'n';
	x->ofs = 0;
}

/*
 * Convenience function to open a file then call pdf_open_xref_with_stream.
 */

pdf_xref *
pdf_open_xref(fz_context *ctx, const char *filename, char *password)
{
	fz_stream *file = NULL;
	pdf_xref *xref;

	fz_var(file);
	fz_try(ctx)
	{
		file = fz_open_file(ctx, filename);
		xref = pdf_open_xref_with_stream(file, password);
	}
	fz_catch(ctx)
	{
		fz_close(file);
		fz_throw(ctx, "cannot load document '%s'", filename);
	}

	fz_close(file);
	return xref;
}

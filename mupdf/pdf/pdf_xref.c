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

static fz_error
pdf_load_version(pdf_xref *xref)
{
	char buf[20];

	fz_seek(xref->file, 0, 0);
	fz_read_line(xref->file, buf, sizeof buf);
	if (memcmp(buf, "%PDF-", 5) != 0)
		return fz_throw("cannot recognize version marker");

	xref->version = fz_atof(buf + 5) * 10 + 0.1; /* SumatraPDF: don't accidentally round down */

	return fz_okay;
}

static fz_error
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
		return fz_rethrow(n, "cannot read from file");

	for (i = n - 9; i >= 0; i--)
	{
		if (memcmp(buf + i, "startxref", 9) == 0)
		{
			i += 9;
			while (iswhite(buf[i]) && i < n)
				i ++;
			xref->startxref = atoi((char*)(buf + i));
			return fz_okay;
		}
	}

	return fz_throw("cannot find startxref");
}

/*
 * trailer dictionary
 */

static fz_error
pdf_read_old_trailer(pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	int len;
	char *s;
	int n;
	int t;
	int tok;
	int c;

	fz_read_line(xref->file, buf, cap);
	if (strncmp(buf, "xref", 4) != 0)
		return fz_throw("cannot find xref marker");

	while (1)
	{
		c = fz_peek_byte(xref->file);
		if (!(c >= '0' && c <= '9'))
			break;

		fz_read_line(xref->file, buf, cap);
		s = buf;
		fz_strsep(&s, " "); /* ignore ofs */
		if (!s)
			return fz_throw("invalid range marker in xref");
		len = atoi(fz_strsep(&s, " "));

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
			fz_seek(xref->file, -(2 + (int)strlen(s)), 1);

		t = fz_tell(xref->file);
		if (t < 0)
			return fz_throw("cannot tell in file");

		fz_seek(xref->file, t + 20 * len, 0);
	}

	error = pdf_lex(&tok, xref->file, buf, cap, &n);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	if (tok != PDF_TOK_TRAILER)
		return fz_throw("expected trailer marker");

	error = pdf_lex(&tok, xref->file, buf, cap, &n);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	if (tok != PDF_TOK_OPEN_DICT)
		return fz_throw("expected trailer dictionary");

	error = pdf_parse_dict(&xref->trailer, xref, xref->file, buf, cap);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	return fz_okay;
}

static fz_error
pdf_read_new_trailer(pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	error = pdf_parse_ind_obj(&xref->trailer, xref, xref->file, buf, cap, NULL, NULL, NULL);
	if (error)
		return fz_rethrow(error, "cannot parse trailer (compressed)");
	return fz_okay;
}

static fz_error
pdf_read_trailer(pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	int c;

	fz_seek(xref->file, xref->startxref, 0);

	while (iswhite(fz_peek_byte(xref->file)))
		fz_read_byte(xref->file);

	c = fz_peek_byte(xref->file);
	if (c == 'x')
	{
		error = pdf_read_old_trailer(xref, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot read trailer");
	}
	else if (c >= '0' && c <= '9')
	{
		error = pdf_read_new_trailer(xref, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot read trailer");
	}
	else
	{
		return fz_throw("cannot recognize xref format: '%c'", c);
	}

	return fz_okay;
}

/*
 * xref tables
 */

void
pdf_resize_xref(pdf_xref *xref, int newlen)
{
	int i;

	xref->table = fz_realloc(xref->table, newlen, sizeof(pdf_xref_entry));
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

static fz_error
pdf_read_old_xref(fz_obj **trailerp, pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	int ofs, len;
	char *s;
	int n;
	int tok;
	int i;
	int c;

	fz_read_line(xref->file, buf, cap);
	if (strncmp(buf, "xref", 4) != 0)
		return fz_throw("cannot find xref marker");

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
			fz_warn("broken xref section. proceeding anyway.");
			fz_seek(xref->file, -(2 + (int)strlen(s)), 1);
		}

		/* broken pdfs where size in trailer undershoots entries in xref sections */
		if (ofs + len > xref->len)
		{
			fz_warn("broken xref section, proceeding anyway.");
			pdf_resize_xref(xref, ofs + len);
		}

		for (i = ofs; i < ofs + len; i++)
		{
			n = fz_read(xref->file, (unsigned char *) buf, 20);
			if (n < 0)
				return fz_rethrow(n, "cannot read xref table");
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
					return fz_throw("unexpected xref type: %#x (%d %d R)", s[17], i, xref->table[i].gen);
			}
		}
	}

	error = pdf_lex(&tok, xref->file, buf, cap, &n);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	if (tok != PDF_TOK_TRAILER)
		return fz_throw("expected trailer marker");

	error = pdf_lex(&tok, xref->file, buf, cap, &n);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	if (tok != PDF_TOK_OPEN_DICT)
		return fz_throw("expected trailer dictionary");

	error = pdf_parse_dict(trailerp, xref, xref->file, buf, cap);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	return fz_okay;
}

static fz_error
pdf_read_new_xref_section(pdf_xref *xref, fz_stream *stm, int i0, int i1, int w0, int w1, int w2)
{
	int i, n;

	if (i0 < 0 || i0 + i1 > xref->len)
		return fz_throw("xref stream has too many entries");

	for (i = i0; i < i0 + i1; i++)
	{
		int a = 0;
		int b = 0;
		int c = 0;

		if (fz_is_eof(stm))
			return fz_throw("truncated xref stream");

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

	return fz_okay;
}

static fz_error
pdf_read_new_xref(fz_obj **trailerp, pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *trailer;
	fz_obj *index;
	fz_obj *obj;
	int num, gen, stm_ofs;
	int size, w0, w1, w2;
	int t;

	error = pdf_parse_ind_obj(&trailer, xref, xref->file, buf, cap, &num, &gen, &stm_ofs);
	if (error)
		return fz_rethrow(error, "cannot parse compressed xref stream object");

	obj = fz_dict_gets(trailer, "Size");
	if (!obj)
	{
		fz_drop_obj(trailer);
		return fz_throw("xref stream missing Size entry (%d %d R)", num, gen);
	}
	size = fz_to_int(obj);

	if (size > xref->len)
	{
		pdf_resize_xref(xref, size);
	}

	if (num < 0 || num >= xref->len)
	{
		fz_drop_obj(trailer);
		return fz_throw("object id (%d %d R) out of range (0..%d)", num, gen, xref->len - 1);
	}

	obj = fz_dict_gets(trailer, "W");
	if (!obj) {
		fz_drop_obj(trailer);
		return fz_throw("xref stream missing W entry (%d %d R)", num, gen);
	}
	w0 = fz_to_int(fz_array_get(obj, 0));
	w1 = fz_to_int(fz_array_get(obj, 1));
	w2 = fz_to_int(fz_array_get(obj, 2));

	index = fz_dict_gets(trailer, "Index");

	error = pdf_open_stream_at(&stm, xref, num, gen, trailer, stm_ofs);
	if (error)
	{
		fz_drop_obj(trailer);
		return fz_rethrow(error, "cannot open compressed xref stream (%d %d R)", num, gen);
	}

	if (!index)
	{
		error = pdf_read_new_xref_section(xref, stm, 0, size, w0, w1, w2);
		if (error)
		{
			fz_close(stm);
			fz_drop_obj(trailer);
			return fz_rethrow(error, "cannot read xref stream (%d %d R)", num, gen);
		}
	}
	else
	{
		for (t = 0; t < fz_array_len(index); t += 2)
		{
			int i0 = fz_to_int(fz_array_get(index, t + 0));
			int i1 = fz_to_int(fz_array_get(index, t + 1));
			error = pdf_read_new_xref_section(xref, stm, i0, i1, w0, w1, w2);
			if (error)
			{
				fz_close(stm);
				fz_drop_obj(trailer);
				return fz_rethrow(error, "cannot read xref stream section (%d %d R)", num, gen);
			}
		}
	}

	fz_close(stm);

	*trailerp = trailer;

	return fz_okay;
}

static fz_error
pdf_read_xref(fz_obj **trailerp, pdf_xref *xref, int ofs, char *buf, int cap)
{
	fz_error error;
	int c;

	fz_seek(xref->file, ofs, 0);

	while (iswhite(fz_peek_byte(xref->file)))
		fz_read_byte(xref->file);

	c = fz_peek_byte(xref->file);
	if (c == 'x')
	{
		error = pdf_read_old_xref(trailerp, xref, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot read xref (ofs=%d)", ofs);
	}
	else if (c >= '0' && c <= '9')
	{
		error = pdf_read_new_xref(trailerp, xref, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot read xref (ofs=%d)", ofs);
	}
	else
	{
		return fz_throw("cannot recognize xref format");
	}

	return fz_okay;
}

static fz_error
pdf_read_xref_sections(pdf_xref *xref, int ofs, char *buf, int cap)
{
	fz_error error;
	fz_obj *trailer;
	fz_obj *prev;
	fz_obj *xrefstm;

	error = pdf_read_xref(&trailer, xref, ofs, buf, cap);
	if (error)
		return fz_rethrow(error, "cannot read xref section");

	/* FIXME: do we overwrite free entries properly? */
	xrefstm = fz_dict_gets(trailer, "XRefStm");
	if (xrefstm)
	{
		error = pdf_read_xref_sections(xref, fz_to_int(xrefstm), buf, cap);
		if (error)
		{
			fz_drop_obj(trailer);
			return fz_rethrow(error, "cannot read /XRefStm xref section");
		}
	}

	prev = fz_dict_gets(trailer, "Prev");
	if (prev)
	{
		error = pdf_read_xref_sections(xref, fz_to_int(prev), buf, cap);
		if (error)
		{
			fz_drop_obj(trailer);
			return fz_rethrow(error, "cannot read /Prev xref section");
		}
	}

	fz_drop_obj(trailer);
	return fz_okay;
}

/*
 * load xref tables from pdf
 */

static fz_error
pdf_load_xref(pdf_xref *xref, char *buf, int bufsize)
{
	fz_error error;
	fz_obj *size;
	int i;

	error = pdf_load_version(xref);
	if (error)
		return fz_rethrow(error, "cannot read version marker");

	error = pdf_read_start_xref(xref);
	if (error)
		return fz_rethrow(error, "cannot read startxref");

	error = pdf_read_trailer(xref, buf, bufsize);
	if (error)
		return fz_rethrow(error, "cannot read trailer");

	size = fz_dict_gets(xref->trailer, "Size");
	if (!size)
		return fz_throw("trailer missing Size entry");

	pdf_resize_xref(xref, fz_to_int(size));

	error = pdf_read_xref_sections(xref, xref->startxref, buf, bufsize);
	if (error)
		return fz_rethrow(error, "cannot read xref");

	/* broken pdfs where first object is not free */
	if (xref->table[0].type != 'f')
		return fz_throw("first object in xref is not free");

	/* broken pdfs where object offsets are out of range */
	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].type == 'n')
			if (xref->table[i].ofs <= 0 || xref->table[i].ofs >= xref->file_size)
				return fz_throw("object offset out of range: %d (%d 0 R)", xref->table[i].ofs, i);
		if (xref->table[i].type == 'o')
			if (xref->table[i].ofs <= 0 || xref->table[i].ofs >= xref->len || xref->table[xref->table[i].ofs].type != 'n')
				return fz_throw("invalid reference to an objstm that does not exist: %d (%d 0 R)", xref->table[i].ofs, i);
	}

	return fz_okay;
}

/*
 * Initialize and load xref tables.
 * If password is not null, try to decrypt.
 */

fz_error
pdf_open_xref_with_stream(pdf_xref **xrefp, fz_stream *file, char *password)
{
	pdf_xref *xref;
	fz_error error;
	fz_obj *encrypt, *id;
	fz_obj *dict, *obj;
	int i, repaired = 0;

	/* install pdf specific callback */
	fz_resolve_indirect = pdf_resolve_indirect;

	xref = fz_malloc(sizeof(pdf_xref));

	memset(xref, 0, sizeof(pdf_xref));

	xref->file = fz_keep_stream(file);

	error = pdf_load_xref(xref, xref->scratch, sizeof xref->scratch);
	if (error)
	{
		fz_catch(error, "trying to repair");
		if (xref->table)
		{
			fz_free(xref->table);
			xref->table = NULL;
			xref->len = 0;
		}
		if (xref->trailer)
		{
			fz_drop_obj(xref->trailer);
			xref->trailer = NULL;
		}
		error = pdf_repair_xref(xref, xref->scratch, sizeof xref->scratch);
		if (error)
		{
			pdf_free_xref(xref);
			return fz_rethrow(error, "cannot repair document");
		}
		repaired = 1;
	}

	encrypt = fz_dict_gets(xref->trailer, "Encrypt");
	id = fz_dict_gets(xref->trailer, "ID");
	if (fz_is_dict(encrypt))
	{
		error = pdf_new_crypt(&xref->crypt, encrypt, id);
		if (error)
		{
			pdf_free_xref(xref);
			return fz_rethrow(error, "cannot decrypt document");
		}
	}

	if (pdf_needs_password(xref))
	{
		/* Only care if we have a password */
		if (password)
		{
			int okay = pdf_authenticate_password(xref, password);
			if (!okay)
			{
				pdf_free_xref(xref);
				return fz_throw("invalid password");
			}
		}
	}

	if (repaired)
	{
		int hasroot, hasinfo;

		error = pdf_repair_obj_stms(xref);
		if (error)
		{
			pdf_free_xref(xref);
			return fz_rethrow(error, "cannot repair document");
		}

		hasroot = fz_dict_gets(xref->trailer, "Root") != NULL;
		hasinfo = fz_dict_gets(xref->trailer, "Info") != NULL;

		for (i = 1; i < xref->len; i++)
		{
			if (xref->table[i].type == 0 || xref->table[i].type == 'f')
				continue;

			error = pdf_load_object(&dict, xref, i, 0);
			if (error)
			{
				fz_catch(error, "ignoring broken object (%d 0 R)", i);
				continue;
			}

			if (!hasroot)
			{
				obj = fz_dict_gets(dict, "Type");
				if (fz_is_name(obj) && !strcmp(fz_to_name(obj), "Catalog"))
				{
					obj = fz_new_indirect(i, 0, xref);
					fz_dict_puts(xref->trailer, "Root", obj);
					fz_drop_obj(obj);
				}
			}

			if (!hasinfo)
			{
				if (fz_dict_gets(dict, "Creator") || fz_dict_gets(dict, "Producer"))
				{
					obj = fz_new_indirect(i, 0, xref);
					fz_dict_puts(xref->trailer, "Info", obj);
					fz_drop_obj(obj);
				}
			}

			fz_drop_obj(dict);
		}
	}

	*xrefp = xref;
	return fz_okay;
}

void
pdf_free_xref(pdf_xref *xref)
{
	int i;

	if (xref->store)
		pdf_free_store(xref->store);

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
		fz_free(xref->table);
	}

	if (xref->page_objs)
	{
		for (i = 0; i < xref->page_len; i++)
			fz_drop_obj(xref->page_objs[i]);
		fz_free(xref->page_objs);
	}

	if (xref->page_refs)
	{
		for (i = 0; i < xref->page_len; i++)
			fz_drop_obj(xref->page_refs[i]);
		fz_free(xref->page_refs);
	}

	if (xref->file)
		fz_close(xref->file);
	if (xref->trailer)
		fz_drop_obj(xref->trailer);
	if (xref->crypt)
		pdf_free_crypt(xref->crypt);

	fz_free(xref);
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

static fz_error
pdf_load_obj_stm(pdf_xref *xref, int num, int gen, char *buf, int cap)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *objstm;
	int *numbuf;
	int *ofsbuf;

	fz_obj *obj;
	int first;
	int count;
	int i, n;
	int tok;

	error = pdf_load_object(&objstm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load object stream object (%d %d R)", num, gen);

	count = fz_to_int(fz_dict_gets(objstm, "N"));
	first = fz_to_int(fz_dict_gets(objstm, "First"));

	numbuf = fz_calloc(count, sizeof(int));
	ofsbuf = fz_calloc(count, sizeof(int));

	error = pdf_open_stream(&stm, xref, num, gen);
	if (error)
	{
		error = fz_rethrow(error, "cannot open object stream (%d %d R)", num, gen);
		goto cleanupbuf;
	}

	for (i = 0; i < count; i++)
	{
		error = pdf_lex(&tok, stm, buf, cap, &n);
		if (error || tok != PDF_TOK_INT)
		{
			error = fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
			goto cleanupstm;
		}
		numbuf[i] = atoi(buf);

		error = pdf_lex(&tok, stm, buf, cap, &n);
		if (error || tok != PDF_TOK_INT)
		{
			error = fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
			goto cleanupstm;
		}
		ofsbuf[i] = atoi(buf);
	}

	fz_seek(stm, first, 0);

	for (i = 0; i < count; i++)
	{
		fz_seek(stm, first + ofsbuf[i], 0);

		error = pdf_parse_stm_obj(&obj, xref, stm, buf, cap);
		if (error)
		{
			error = fz_rethrow(error, "cannot parse object %d in stream (%d %d R)", i, num, gen);
			goto cleanupstm;
		}

		if (numbuf[i] < 1 || numbuf[i] >= xref->len)
		{
			fz_drop_obj(obj);
			error = fz_throw("object id (%d 0 R) out of range (0..%d)", numbuf[i], xref->len - 1);
			goto cleanupstm;
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

	fz_close(stm);
	fz_free(ofsbuf);
	fz_free(numbuf);
	fz_drop_obj(objstm);
	return fz_okay;

cleanupstm:
	fz_close(stm);
cleanupbuf:
	fz_free(ofsbuf);
	fz_free(numbuf);
	fz_drop_obj(objstm);
	return error; /* already rethrown */
}

/*
 * object loading
 */

fz_error
pdf_cache_object(pdf_xref *xref, int num, int gen)
{
	fz_error error;
	pdf_xref_entry *x;
	int rnum, rgen;

	if (num < 0 || num >= xref->len)
		return fz_throw("object out of range (%d %d R); xref size %d", num, gen, xref->len);

	x = &xref->table[num];

	if (x->obj)
		return fz_okay;

	if (x->type == 'f')
	{
		x->obj = fz_new_null();
		return fz_okay;
	}
	else if (x->type == 'n')
	{
		fz_seek(xref->file, x->ofs, 0);

		error = pdf_parse_ind_obj(&x->obj, xref, xref->file, xref->scratch, sizeof xref->scratch,
			&rnum, &rgen, &x->stm_ofs);
		if (error)
			return fz_rethrow(error, "cannot parse object (%d %d R)", num, gen);

		if (rnum != num)
			return fz_throw("found object (%d %d R) instead of (%d %d R)", rnum, rgen, num, gen);

		if (xref->crypt)
			pdf_crypt_obj(xref->crypt, x->obj, num, gen);
	}
	else if (x->type == 'o')
	{
		if (!x->obj)
		{
			error = pdf_load_obj_stm(xref, x->ofs, 0, xref->scratch, sizeof xref->scratch);
			if (error)
				return fz_rethrow(error, "cannot load object stream containing object (%d %d R)", num, gen);
			if (!x->obj)
				return fz_throw("object (%d %d R) was not found in its object stream", num, gen);
		}
	}
	else
	{
		return fz_throw("assert: corrupt xref struct");
	}

	return fz_okay;
}

fz_error
pdf_load_object(fz_obj **objp, pdf_xref *xref, int num, int gen)
{
	fz_error error;

	error = pdf_cache_object(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load object (%d %d R) into cache", num, gen);

	assert(xref->table[num].obj);

	*objp = fz_keep_obj(xref->table[num].obj);

	return fz_okay;
}

fz_obj *
pdf_resolve_indirect(fz_obj *ref)
{
	if (fz_is_indirect(ref))
	{
		pdf_xref *xref = fz_get_indirect_xref(ref);
		int num = fz_to_num(ref);
		int gen = fz_to_gen(ref);
		if (xref)
		{
			fz_error error = pdf_cache_object(xref, num, gen);
			if (error)
			{
				fz_catch(error, "cannot load object (%d %d R) into cache", num, gen);
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
		fz_warn("object out of range (%d %d R); xref size %d", num, gen, xref->len);
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

fz_error
pdf_open_xref(pdf_xref **xrefp, const char *filename, char *password)
{
	fz_error error;
	fz_stream *file;

	file = fz_open_file(filename);
	if (!file)
		return fz_throw("cannot open file '%s': %s", filename, strerror(errno));

	error = pdf_open_xref_with_stream(xrefp, file, password);
	if (error)
		return fz_rethrow(error, "cannot load document '%s'", filename);

	fz_close(file);
	return fz_okay;
}

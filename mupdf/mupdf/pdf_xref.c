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
pdf_loadversion(pdf_xref *xref)
{
	fz_error error;
	char buf[20];

	error = fz_seek(xref->file, 0, 0);
	if (error)
		return fz_rethrow(error, "cannot seek to beginning of file");

	error = fz_readline(xref->file, buf, sizeof buf);
	if (error)
		return fz_rethrow(error, "cannot read version marker");
	if (memcmp(buf, "%PDF-", 5) != 0)
		return fz_throw("cannot recognize version marker");

	xref->version = atof(buf + 5) * 10;

	pdf_logxref("version %d.%d\n", xref->version / 10, xref->version % 10);

	return fz_okay;
}

static fz_error
pdf_readstartxref(pdf_xref *xref)
{
	fz_error error;
	unsigned char buf[1024];
	int t, n;
	int i;

	error = fz_seek(xref->file, 0, 2);
	if (error)
		return fz_rethrow(error, "cannot seek to end of file");

	xref->filesize = fz_tell(xref->file);

	t = MAX(0, xref->filesize - (int)sizeof buf);
	error = fz_seek(xref->file, t, 0);
	if (error)
		return fz_rethrow(error, "cannot seek to offset %d", t);

	error = fz_read(&n, xref->file, buf, sizeof buf);
	if (error)
		return fz_rethrow(error, "cannot read from file");

	for (i = n - 9; i >= 0; i--)
	{
		if (memcmp(buf + i, "startxref", 9) == 0)
		{
			i += 9;
			while (iswhite(buf[i]) && i < n)
				i ++;
			xref->startxref = atoi((char*)(buf + i));
			pdf_logxref("startxref %d\n", xref->startxref);
			return fz_okay;
		}
	}

	return fz_throw("cannot find startxref");
}

/*
 * trailer dictionary
 */

static fz_error
pdf_readoldtrailer(pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	int len;
	char *s;
	int n;
	int t;
	pdf_token_e tok;
	int c;

	pdf_logxref("load old xref format trailer\n");

	error = fz_readline(xref->file, buf, cap);
	if (error)
		return fz_rethrow(error, "cannot read xref marker");
	if (strncmp(buf, "xref", 4) != 0)
		return fz_throw("cannot find xref marker");

	while (1)
	{
		c = fz_peekbyte(xref->file);
		if (!(c >= '0' && c <= '9'))
			break;

		error = fz_readline(xref->file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot read xref count");

		s = buf;
		fz_strsep(&s, " "); /* ignore ofs */
		if (!s)
			return fz_throw("invalid range marker in xref");
		len = atoi(fz_strsep(&s, " "));

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
		{
			error = fz_seek(xref->file, -(2 + (int)strlen(s)), 1);
			if (error)
				return fz_rethrow(error, "cannot seek in file");
		}

		t = fz_tell(xref->file);
		if (t < 0)
			return fz_throw("cannot tell in file");

		error = fz_seek(xref->file, t + 20 * len, 0);
		if (error)
			return fz_rethrow(error, "cannot seek in file");
	}

	error = fz_readerror(xref->file);
	if (error)
		return fz_rethrow(error, "cannot read from file");

	error = pdf_lex(&tok, xref->file, buf, cap, &n);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	if (tok != PDF_TTRAILER)
		return fz_throw("expected trailer marker");

	error = pdf_lex(&tok, xref->file, buf, cap, &n);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	if (tok != PDF_TODICT)
		return fz_throw("expected trailer dictionary");

	error = pdf_parsedict(&xref->trailer, xref, xref->file, buf, cap);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	return fz_okay;
}

static fz_error
pdf_readnewtrailer(pdf_xref *xref, char *buf, int cap)
{
	fz_error error;

	pdf_logxref("load new xref format trailer\n");

	error = pdf_parseindobj(&xref->trailer, xref, xref->file, buf, cap, nil, nil, nil);
	if (error)
		return fz_rethrow(error, "cannot parse trailer (compressed)");
	return fz_okay;
}

static fz_error
pdf_readtrailer(pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	int c;

	error = fz_seek(xref->file, xref->startxref, 0);
	if (error)
		return fz_rethrow(error, "cannot seek to startxref");

	while (iswhite(fz_peekbyte(xref->file)))
		fz_readbyte(xref->file);

	c = fz_peekbyte(xref->file);
	error = fz_readerror(xref->file);
	if (error)
		return fz_rethrow(error, "cannot read trailer");

	if (c == 'x')
	{
		error = pdf_readoldtrailer(xref, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot read trailer");
	}
	else if (c >= '0' && c <= '9')
	{
		error = pdf_readnewtrailer(xref, buf, cap);
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

static fz_error
pdf_readoldxref(fz_obj **trailerp, pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	int ofs, len;
	char *s;
	int n;
	pdf_token_e tok;
	int i;
	int c;

	pdf_logxref("load old xref format\n");

	error = fz_readline(xref->file, buf, cap);
	if (error)
		return fz_rethrow(error, "cannot read xref marker");
	if (strncmp(buf, "xref", 4) != 0)
		return fz_throw("cannot find xref marker");

	while (1)
	{
		c = fz_peekbyte(xref->file);
		if (!(c >= '0' && c <= '9'))
			break;

		error = fz_readline(xref->file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot read xref count");

		s = buf;
		ofs = atoi(fz_strsep(&s, " "));
		len = atoi(fz_strsep(&s, " "));

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
		{
			fz_warn("broken xref section. proceeding anyway.");
			error = fz_seek(xref->file, -(2 + (int)strlen(s)), 1);
			if (error)
				return fz_rethrow(error, "cannot seek to xref");
		}

		/* broken pdfs where size in trailer undershoots
		entries in xref sections */
		if ((ofs + len) > xref->cap)
		{
			fz_warn("broken xref section, proceeding anyway.");
			xref->cap = ofs + len;
			xref->table = fz_realloc(xref->table, xref->cap * sizeof(pdf_xrefentry));
		}

		if ((ofs + len) > xref->len)
		{
			for (i = xref->len; i < (ofs + len); i++)
			{
				xref->table[i].ofs = 0;
				xref->table[i].gen = 0;
				xref->table[i].stmofs = 0;
				xref->table[i].obj = nil;
				xref->table[i].type = 0;
			}
			xref->len = ofs + len;
		}

		for (i = ofs; i < ofs + len; i++)
		{
			error = fz_read(&n, xref->file, (unsigned char *) buf, 20);
			if (error)
				return fz_rethrow(error, "cannot read xref table");
			if (!xref->table[i].type)
			{
				s = buf;

				/* broken pdfs where line start with white space */
				while (*s != '\0' && iswhite(*s))
					s++;

				xref->table[i].ofs = atoi(s);
				xref->table[i].gen = atoi(s + 11);
				xref->table[i].type = s[17];

				if (xref->table[i].ofs < 0 || xref->table[i].ofs >= xref->filesize)
					return fz_throw("object offset out of range: %d", xref->table[i].ofs);
			}
		}
	}

	error = pdf_lex(&tok, xref->file, buf, cap, &n);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	if (tok != PDF_TTRAILER)
		return fz_throw("expected trailer marker");

	error = pdf_lex(&tok, xref->file, buf, cap, &n);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	if (tok != PDF_TODICT)
		return fz_throw("expected trailer dictionary");

	error = pdf_parsedict(trailerp, xref, xref->file, buf, cap);
	if (error)
		return fz_rethrow(error, "cannot parse trailer");
	return fz_okay;
}

static fz_error
pdf_readnewxrefsection(pdf_xref *xref, fz_stream *stm, int i0, int i1, int w0, int w1, int w2)
{
	fz_error error;
	int i, n;

	if (i0 < 0 || i0 + i1 > xref->len)
		return fz_throw("xref stream has too many entries");

	for (i = i0; i < i0 + i1; i++)
	{
		int a = 0;
		int b = 0;
		int c = 0;

		if (fz_peekbyte(stm) == EOF)
		{
			error = fz_readerror(stm);
			if (error)
				return fz_rethrow(error, "truncated xref stream");
			return fz_throw("truncated xref stream");
		}

		for (n = 0; n < w0; n++)
			a = (a << 8) + fz_readbyte(stm);
		for (n = 0; n < w1; n++)
			b = (b << 8) + fz_readbyte(stm);
		for (n = 0; n < w2; n++)
			c = (c << 8) + fz_readbyte(stm);

		error = fz_readerror(stm);
		if (error)
			return fz_rethrow(error, "truncated xref stream");

		if (!xref->table[i].type)
		{
			int t = w0 ? a : 1;
			xref->table[i].type = t == 0 ? 'f' : t == 1 ? 'n' : t == 2 ? 'o' : 0;
			xref->table[i].ofs = w1 ? b : 0;
			xref->table[i].gen = w2 ? c : 0;

			if (xref->table[i].ofs < 0 || xref->table[i].ofs >= xref->filesize)
				return fz_throw("object offset out of range: %d", xref->table[i].ofs);
		}
	}

	return fz_okay;
}

static fz_error
pdf_readnewxref(fz_obj **trailerp, pdf_xref *xref, char *buf, int cap)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *trailer;
	fz_obj *index;
	fz_obj *obj;
	int num, gen, stmofs;
	int size, w0, w1, w2;
	int t;
	int i;

	pdf_logxref("load new xref format\n");

	error = pdf_parseindobj(&trailer, xref, xref->file, buf, cap, &num, &gen, &stmofs);
	if (error)
		return fz_rethrow(error, "cannot parse compressed xref stream object");

	obj = fz_dictgets(trailer, "Size");
	if (!obj)
	{
		fz_dropobj(trailer);
		return fz_throw("xref stream missing Size entry (%d %d R)", num, gen);
	}
	size = fz_toint(obj);

	if (size >= xref->cap)
	{
		xref->cap = size + 1; /* for hack to allow broken pdf generators with off-by-one errors */
		xref->table = fz_realloc(xref->table, xref->cap * sizeof(pdf_xrefentry));
	}

	if (size > xref->len)
	{
		for (i = xref->len; i < xref->cap; i++)
		{
			xref->table[i].ofs = 0;
			xref->table[i].gen = 0;
			xref->table[i].stmofs = 0;
			xref->table[i].obj = nil;
			xref->table[i].type = 0;
		}
		xref->len = size;
	}

	if (num < 0 || num >= xref->len)
	{
		if (num == xref->len && num < xref->cap)
		{
			/* allow broken pdf files that have off-by-one errors in the xref */
			fz_warn("object id (%d %d R) out of range (0..%d)", num, gen, xref->len - 1);
			xref->len ++;
		}
		else
		{
			fz_dropobj(trailer);
			return fz_throw("object id (%d %d R) out of range (0..%d)", num, gen, xref->len - 1);
		}
	}

	pdf_logxref("\tnum=%d gen=%d size=%d\n", num, gen, size);

	obj = fz_dictgets(trailer, "W");
	if (!obj) {
		fz_dropobj(trailer);
		return fz_throw("xref stream missing W entry (%d %d R)", num, gen);
	}
	w0 = fz_toint(fz_arrayget(obj, 0));
	w1 = fz_toint(fz_arrayget(obj, 1));
	w2 = fz_toint(fz_arrayget(obj, 2));

	index = fz_dictgets(trailer, "Index");

	error = pdf_openstreamat(&stm, xref, num, gen, trailer, stmofs);
	if (error)
	{
		fz_dropobj(trailer);
		return fz_rethrow(error, "cannot open compressed xref stream (%d %d R)", num, gen);
	}

	if (!index)
	{
		error = pdf_readnewxrefsection(xref, stm, 0, size, w0, w1, w2);
		if (error)
		{
			fz_dropstream(stm);
			fz_dropobj(trailer);
			return fz_rethrow(error, "cannot read xref stream (%d %d R)", num, gen);
		}
	}
	else
	{
		for (t = 0; t < fz_arraylen(index); t += 2)
		{
			int i0 = fz_toint(fz_arrayget(index, t + 0));
			int i1 = fz_toint(fz_arrayget(index, t + 1));
			error = pdf_readnewxrefsection(xref, stm, i0, i1, w0, w1, w2);
			if (error)
			{
				fz_dropstream(stm);
				fz_dropobj(trailer);
				return fz_rethrow(error, "cannot read xref stream section (%d %d R)", num, gen);
			}
		}
	}

	fz_dropstream(stm);

	*trailerp = trailer;

	return fz_okay;
}

static fz_error
pdf_readxref(fz_obj **trailerp, pdf_xref *xref, int ofs, char *buf, int cap)
{
	fz_error error;
	int c;

	error = fz_seek(xref->file, ofs, 0);
	if (error)
		return fz_rethrow(error, "cannot seek to xref");

	while (iswhite(fz_peekbyte(xref->file)))
		fz_readbyte(xref->file);

	c = fz_peekbyte(xref->file);
	error = fz_readerror(xref->file);
	if (error)
		return fz_rethrow(error, "cannot read trailer");

	if (c == 'x')
	{
		error = pdf_readoldxref(trailerp, xref, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot read xref (ofs=%d)", ofs);
	}
	else if (c >= '0' && c <= '9')
	{
		error = pdf_readnewxref(trailerp, xref, buf, cap);
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
pdf_readxrefsections(pdf_xref *xref, int ofs, char *buf, int cap)
{
	fz_error error;
	fz_obj *trailer;
	fz_obj *prev;
	fz_obj *xrefstm;

	error = pdf_readxref(&trailer, xref, ofs, buf, cap);
	if (error)
		return fz_rethrow(error, "cannot read xref section");

	/* FIXME: do we overwrite free entries properly? */
	xrefstm = fz_dictgets(trailer, "XRefStm");
	if (xrefstm)
	{
		pdf_logxref("load xrefstm\n");
		error = pdf_readxrefsections(xref, fz_toint(xrefstm), buf, cap);
		if (error)
		{
			fz_dropobj(trailer);
			return fz_rethrow(error, "cannot read /XRefStm xref section");
		}
	}

	prev = fz_dictgets(trailer, "Prev");
	if (prev)
	{
		pdf_logxref("load prev at 0x%x\n", fz_toint(prev));
		error = pdf_readxrefsections(xref, fz_toint(prev), buf, cap);
		if (error)
		{
			fz_dropobj(trailer);
			return fz_rethrow(error, "cannot read /Prev xref section");
		}
	}

	fz_dropobj(trailer);
	return fz_okay;
}

/*
 * load xref tables from pdf
 */

static fz_error
pdf_loadxref(pdf_xref *xref, char *buf, int bufsize)
{
	fz_error error;
	fz_obj *size;
	int i;

	error = pdf_loadversion(xref);
	if (error)
		return fz_rethrow(error, "cannot read version marker");

	error = pdf_readstartxref(xref);
	if (error)
		return fz_rethrow(error, "cannot read startxref");

	error = pdf_readtrailer(xref, buf, bufsize);
	if (error)
		return fz_rethrow(error, "cannot read trailer");

	size = fz_dictgets(xref->trailer, "Size");
	if (!size)
		return fz_throw("trailer missing Size entry");

	pdf_logxref("\tsize %d at 0x%x\n", fz_toint(size), xref->startxref);

	xref->len = fz_toint(size);
	xref->cap = xref->len + 1; /* for hack to allow broken pdf generators with off-by-one errors */
	xref->table = fz_malloc(xref->cap * sizeof(pdf_xrefentry));
	for (i = 0; i < xref->cap; i++)
	{
		xref->table[i].ofs = 0;
		xref->table[i].gen = 0;
		xref->table[i].stmofs = 0;
		xref->table[i].obj = nil;
		xref->table[i].type = 0;
	}

	error = pdf_readxrefsections(xref, xref->startxref, buf, bufsize);
	if (error)
		return fz_rethrow(error, "cannot read xref");

	/* broken pdfs where first object is not free */
	if (xref->table[0].type != 'f')
	{
		fz_warn("first object in xref is not free");
		xref->table[0].type = 'f';
	}

	/* broken pdfs where freed objects have offset and gen set to 0 but still exist */
	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].type == 'n' && xref->table[i].ofs == 0 &&
			xref->table[i].gen == 0 && xref->table[i].obj == nil)
		{
			fz_warn("object (%d %d R) has invalid offset, assumed missing", i, xref->table[i].gen);
			xref->table[i].type = 'f';
		}
	}

	return fz_okay;
}

/*
 * Initialize and load xref tables.
 * If password is not null, try to decrypt.
 */

fz_error
pdf_openxrefwithstream(pdf_xref **xrefp, fz_stream *file, char *password)
{
	pdf_xref *xref;
	fz_error error;
	fz_obj *encrypt;
	fz_obj *id;

	xref = fz_malloc(sizeof(pdf_xref));

	memset(xref, 0, sizeof(pdf_xref));

	pdf_logxref("openxref %p\n", xref);

	xref->file = fz_keepstream(file);

	error = pdf_loadxref(xref, xref->scratch, sizeof xref->scratch);
	if (error)
	{
		fz_catch(error, "trying to repair");
		if (xref->table)
		{
			fz_free(xref->table);
			xref->table = NULL;
			xref->len = 0;
			xref->cap = 0;
		}
		error = pdf_repairxref(xref, xref->scratch, sizeof xref->scratch);
		if (error)
		{
			pdf_freexref(xref);
			return fz_rethrow(error, "cannot repair document");
		}
	}

	encrypt = fz_dictgets(xref->trailer, "Encrypt");
	id = fz_dictgets(xref->trailer, "ID");
	if (fz_isdict(encrypt))
	{
		error = pdf_newcrypt(&xref->crypt, encrypt, id);
		if (error)
		{
			pdf_freexref(xref);
			return fz_rethrow(error, "cannot decrypt document");
		}
	}

	if (pdf_needspassword(xref))
	{
		/* Only care if we have a password */
		if (password)
		{
			int okay = pdf_authenticatepassword(xref, password);
			if (!okay)
			{
				pdf_freexref(xref);
				return fz_throw("invalid password");
			}
		}
	}

	*xrefp = xref;
	return fz_okay;
}

void
pdf_freexref(pdf_xref *xref)
{
	int i;

	pdf_logxref("freexref %p\n", xref);

	if (xref->store)
		pdf_freestore(xref->store);

	if (xref->table)
	{
		for (i = 0; i < xref->len; i++)
		{
			if (xref->table[i].obj)
			{
				fz_dropobj(xref->table[i].obj);
				xref->table[i].obj = nil;
			}
		}
		fz_free(xref->table);
	}

	if (xref->pageobjs)
	{
		for (i = 0; i < xref->pagelen; i++)
			fz_dropobj(xref->pageobjs[i]);
		fz_free(xref->pageobjs);
	}

	if (xref->pagerefs)
	{
		for (i = 0; i < xref->pagelen; i++)
			fz_dropobj(xref->pagerefs[i]);
		fz_free(xref->pagerefs);
	}

	if (xref->file)
		fz_dropstream(xref->file);
	if (xref->trailer)
		fz_dropobj(xref->trailer);
	if (xref->crypt)
		pdf_freecrypt(xref->crypt);

	fz_free(xref);
}

void
pdf_debugxref(pdf_xref *xref)
{
	int i;
	printf("xref\n0 %d\n", xref->len);
	for (i = 0; i < xref->len; i++)
	{
		printf("%05d: %010d %05d %c (refs=%d, stmofs=%d)\n", i,
			xref->table[i].ofs,
			xref->table[i].gen,
			xref->table[i].type ? xref->table[i].type : '-',
			xref->table[i].obj ? xref->table[i].obj->refs : 0,
			xref->table[i].stmofs);
	}
}

/*
 * compressed object streams
 */

static fz_error
pdf_loadobjstm(pdf_xref *xref, int num, int gen, char *buf, int cap)
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
	pdf_token_e tok;

	pdf_logxref("loadobjstm (%d %d R)\n", num, gen);

	error = pdf_loadobject(&objstm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load object stream object (%d %d R)", num, gen);

	count = fz_toint(fz_dictgets(objstm, "N"));
	first = fz_toint(fz_dictgets(objstm, "First"));

	pdf_logxref("\tcount %d\n", count);

	numbuf = fz_malloc(count * sizeof(int));
	ofsbuf = fz_malloc(count * sizeof(int));

	error = pdf_openstream(&stm, xref, num, gen);
	if (error)
	{
		error = fz_rethrow(error, "cannot open object stream (%d %d R)", num, gen);
		goto cleanupbuf;
	}

	for (i = 0; i < count; i++)
	{
		error = pdf_lex(&tok, stm, buf, cap, &n);
		if (error || tok != PDF_TINT)
		{
			error = fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
			goto cleanupstm;
		}
		numbuf[i] = atoi(buf);

		error = pdf_lex(&tok, stm, buf, cap, &n);
		if (error || tok != PDF_TINT)
		{
			error = fz_rethrow(error, "corrupt object stream (%d %d R)", num, gen);
			goto cleanupstm;
		}
		ofsbuf[i] = atoi(buf);
	}

	error = fz_seek(stm, first, 0);
	if (error)
	{
		error = fz_rethrow(error, "cannot seek in object stream (%d %d R)", num, gen);
		goto cleanupstm;
	}

	for (i = 0; i < count; i++)
	{
		error = fz_seek(stm, first + ofsbuf[i], 0);
		if (error)
		{
			error = fz_rethrow(error, "cannot seek in object stream (%d %d R)", num, gen);
			goto cleanupstm;
		}

		error = pdf_parsestmobj(&obj, xref, stm, buf, cap);
		if (error)
		{
			error = fz_rethrow(error, "cannot parse object %d in stream (%d %d R)", i, num, gen);
			goto cleanupstm;
		}

		if (numbuf[i] < 1 || numbuf[i] >= xref->len)
		{
			fz_dropobj(obj);
			error = fz_throw("object id (%d 0 R) out of range (0..%d)", numbuf[i], xref->len - 1);
			goto cleanupstm;
		}

		if (xref->table[numbuf[i]].type == 'o' && xref->table[numbuf[i]].ofs == num)
		{
			if (xref->table[numbuf[i]].obj)
				fz_dropobj(xref->table[numbuf[i]].obj);
			xref->table[numbuf[i]].obj = obj;
		}
		else
		{
			fz_dropobj(obj);
		}
	}

	fz_dropstream(stm);
	fz_free(ofsbuf);
	fz_free(numbuf);
	fz_dropobj(objstm);
	return fz_okay;

cleanupstm:
	fz_dropstream(stm);
cleanupbuf:
	fz_free(ofsbuf);
	fz_free(numbuf);
	fz_dropobj(objstm);
	return error; /* already rethrown */
}

/*
 * object loading
 */

fz_error
pdf_cacheobject(pdf_xref *xref, int num, int gen)
{
	fz_error error;
	pdf_xrefentry *x;
	int rnum, rgen;

	if (num < 0 || num >= xref->len)
		return fz_throw("object out of range (%d %d R); xref size %d", num, gen, xref->len);

	x = &xref->table[num];

	if (x->obj)
		return fz_okay;

	if (x->type == 'f')
	{
		x->obj = fz_newnull();
		return fz_okay;
	}
	else if (x->type == 'n')
	{
		error = fz_seek(xref->file, x->ofs, 0);
		if (error)
			return fz_rethrow(error, "cannot seek to object (%d %d R) offset %d", num, gen, x->ofs);

		error = pdf_parseindobj(&x->obj, xref, xref->file, xref->scratch, sizeof xref->scratch,
			&rnum, &rgen, &x->stmofs);
		if (error)
			return fz_rethrow(error, "cannot parse object (%d %d R)", num, gen);

		if (rnum != num)
			return fz_throw("found object (%d %d R) instead of (%d %d R)", rnum, rgen, num, gen);

		if (xref->crypt)
			pdf_cryptobj(xref->crypt, x->obj, num, gen);
	}
	else if (x->type == 'o')
	{
		if (!x->obj)
		{
			error = pdf_loadobjstm(xref, x->ofs, 0, xref->scratch, sizeof xref->scratch);
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
pdf_loadobject(fz_obj **objp, pdf_xref *xref, int num, int gen)
{
	fz_error error;

	error = pdf_cacheobject(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load object (%d %d R) into cache", num, gen);

	assert(xref->table[num].obj);

	*objp = fz_keepobj(xref->table[num].obj);

	return fz_okay;
}

/* Replace numbered object -- for use by pdfclean and similar tools */
void
pdf_updateobject(pdf_xref *xref, int num, int gen, fz_obj *newobj)
{
	pdf_xrefentry *x;

	if (num < 0 || num >= xref->len)
	{
		fz_warn("object out of range (%d %d R); xref size %d", num, gen, xref->len);
		return;
	}

	x = &xref->table[num];

	if (x->obj)
		fz_dropobj(x->obj);

	x->obj = fz_keepobj(newobj);
	x->type = 'n';
	x->ofs = 0;
}

/*
 * Convenience function to open a file then call pdf_openxrefwithstream.
  */

fz_error
pdf_openxref(pdf_xref **xrefp, char *filename, char *password)
{
	fz_error error;
	pdf_xref *xref;
	fz_stream *file;
	int fd;

	fd = open(filename, O_BINARY | O_RDONLY);
	if (fd < 0)
		return fz_throw("cannot open file '%s': %s", filename, strerror(errno));

	file = fz_openfile(fd);
	error = pdf_openxrefwithstream(&xref, file, password);
	if (error)
		return fz_rethrow(error, "cannot load document '%s'", filename);
	fz_dropstream(file);

	*xrefp = xref;
	return fz_okay;
}

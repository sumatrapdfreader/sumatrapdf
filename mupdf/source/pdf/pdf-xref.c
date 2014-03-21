#include "mupdf/pdf.h"
#include "mupdf/fitz/document.h"

#undef DEBUG_PROGESSIVE_ADVANCE

#ifdef DEBUG_PROGESSIVE_ADVANCE
#define DEBUGMESS(A) do { fz_warn A; } while (0)
#else
#define DEBUGMESS(A) do { } while (0)
#endif

static inline int iswhite(int ch)
{
	return
		ch == '\000' || ch == '\011' || ch == '\012' ||
		ch == '\014' || ch == '\015' || ch == '\040';
}

/*
 * xref tables
 */

static void pdf_free_xref_sections(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;
	int x, e;

	for (x = 0; x < doc->num_xref_sections; x++)
	{
		pdf_xref *xref = &doc->xref_sections[x];

		for (e = 0; e < xref->len; e++)
		{
			pdf_xref_entry *entry = &xref->table[e];

			if (entry->obj)
			{
				pdf_drop_obj(entry->obj);
				fz_drop_buffer(ctx, entry->stm_buf);
			}
		}

		fz_free(ctx, xref->table);
		pdf_drop_obj(xref->pre_repair_trailer);
		pdf_drop_obj(xref->trailer);
	}

	fz_free(ctx, doc->xref_sections);
	doc->xref_sections = NULL;
	doc->num_xref_sections = 0;
}

static void pdf_resize_xref(fz_context *ctx, pdf_xref *xref, int newlen)
{
	int i;

	xref->table = fz_resize_array(ctx, xref->table, newlen, sizeof(pdf_xref_entry));
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

static void pdf_populate_next_xref_level(pdf_document *doc)
{
	pdf_xref *xref;
	doc->xref_sections = fz_resize_array(doc->ctx, doc->xref_sections, doc->num_xref_sections + 1, sizeof(pdf_xref));
	doc->num_xref_sections++;

	xref = &doc->xref_sections[doc->num_xref_sections - 1];
	xref->len = 0;
	xref->table = NULL;
	xref->trailer = NULL;
	xref->pre_repair_trailer = NULL;
}

pdf_obj *pdf_trailer(pdf_document *doc)
{
	/* Return the document's final trailer */
	pdf_xref *xref = &doc->xref_sections[0];

	return xref->trailer;
}

void pdf_set_populating_xref_trailer(pdf_document *doc, pdf_obj *trailer)
{
	/* Update the trailer of the xref section being populated */
	pdf_xref *xref = &doc->xref_sections[doc->num_xref_sections - 1];
	if (xref->trailer)
	{
		pdf_drop_obj(xref->pre_repair_trailer);
		xref->pre_repair_trailer = xref->trailer;
	}
	xref->trailer = pdf_keep_obj(trailer);
}

int pdf_xref_len(pdf_document *doc)
{
	/* Return the length of the document's final xref section */
	pdf_xref *xref = &doc->xref_sections[0];

	return xref == NULL ? 0 : xref->len;
}

/* Used while reading the individual xref sections from a file */
pdf_xref_entry *pdf_get_populating_xref_entry(pdf_document *doc, int num)
{
	/* Return an entry within the xref currently being populated */
	pdf_xref *xref;
	int i;

	if (doc->num_xref_sections == 0)
	{
		doc->xref_sections = fz_calloc(doc->ctx, 1, sizeof(pdf_xref));
		doc->num_xref_sections = 1;
	}

	/* Prevent accidental heap underflow */
	if (num < 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "object number must not be negative (%d)", num);

	/* Ensure all xref sections map this entry */
	for (i = doc->num_xref_sections - 1; i >= 0; i--)
	{
		xref = &doc->xref_sections[i];

		if (num >= xref->len)
			pdf_resize_xref(doc->ctx, xref, num+1);
		else
			break; /* Remaining sections already of sufficient size */
	}

	/* Loop leaves xref pointing at the populating section */
	return &doc->xref_sections[doc->num_xref_sections-1].table[num];
}

/* Used after loading a document to access entries */
pdf_xref_entry *pdf_get_xref_entry(pdf_document *doc, int i)
{
	int j;

	/* Find the first xref section where the entry is defined. */
	for (j = 0; j < doc->num_xref_sections; j++)
	{
		pdf_xref *xref = &doc->xref_sections[j];

		if (i >= 0 && i < xref->len)
		{
			pdf_xref_entry *entry = &xref->table[i];

			if (entry->type)
				return entry;
		}
	}

	/*
		Didn't find the entry in any section. Return the entry from the final
		section.
	*/
	return &doc->xref_sections[0].table[i];
}

/*
		Ensure we have an incremental xref section where we can store
		updated versions of indirect objects
*/
static void ensure_incremental_xref(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;

	if (!doc->xref_altered)
	{
		pdf_xref *xref = &doc->xref_sections[0];
		pdf_xref *pxref;
		pdf_xref_entry *new_table = fz_calloc(ctx, xref->len, sizeof(pdf_xref_entry));
		pdf_obj *trailer = NULL;

		fz_var(trailer);
		fz_try(ctx)
		{
			trailer = pdf_copy_dict(xref->trailer);
			doc->xref_sections = fz_resize_array(ctx, doc->xref_sections, doc->num_xref_sections + 1, sizeof(pdf_xref));
			xref = &doc->xref_sections[0];
			pxref = &doc->xref_sections[1];
			memmove(pxref, xref, doc->num_xref_sections * sizeof(pdf_xref));
			/* xref->len is already correct */
			xref->table = new_table;
			xref->trailer = trailer;
			xref->pre_repair_trailer = NULL;
			doc->num_xref_sections++;
			doc->xref_altered = 1;
		}
		fz_catch(ctx)
		{
			fz_free(ctx, new_table);
			pdf_drop_obj(trailer);
			fz_rethrow(ctx);
		}
	}
}

/* Used when altering a document */
static pdf_xref_entry *pdf_get_incremental_xref_entry(pdf_document *doc, int i)
{
	fz_context *ctx = doc->ctx;
	pdf_xref *xref;

	/* Make a new final xref section if we haven't already */
	ensure_incremental_xref(doc);

	xref = &doc->xref_sections[0];
	if (i >= xref->len)
		pdf_resize_xref(ctx, xref, i + 1);

	return &xref->table[i];
}

int pdf_xref_is_incremental(pdf_document *doc, int num)
{
	pdf_xref *xref = &doc->xref_sections[0];

	return doc->xref_altered && num < xref->len && xref->table[num].type;
}

/* Ensure that an object has been cloned into the incremental xref section */
void pdf_xref_ensure_incremental_object(pdf_document *doc, int num)
{
	pdf_xref_entry *new_entry, *old_entry;
	int i;

	/* Make sure we have created an xref section for incremental updates */
	ensure_incremental_xref(doc);

	/* Search for the section that contains this object */
	for (i = 0; i < doc->num_xref_sections; i++)
	{
		pdf_xref *xref = &doc->xref_sections[i];
		if (num >= 0 && num < xref->len && xref->table[num].type)
			break;
	}

	/* If we don't find it, or it's already in the incremental section, return */
	if (i == 0 || i == doc->num_xref_sections)
		return;

	/* Move the object to the incremental section */
	old_entry = &doc->xref_sections[i].table[num];
	new_entry = pdf_get_incremental_xref_entry(doc, num);
	*new_entry = *old_entry;
	old_entry->obj = NULL;
	old_entry->stm_buf = NULL;
}

void pdf_replace_xref(pdf_document *doc, pdf_xref_entry *entries, int n)
{
	fz_context *ctx = doc->ctx;
	pdf_xref *xref;
	pdf_obj *trailer = pdf_keep_obj(pdf_trailer(doc));

	/* The new table completely replaces the previous separate sections */
	pdf_free_xref_sections(doc);

	fz_var(trailer);
	fz_try(ctx)
	{
		xref = fz_calloc(ctx, 1, sizeof(pdf_xref));
		xref->table = entries;
		xref->len = n;
		xref->trailer = trailer;
		trailer = NULL;

		doc->xref_sections = xref;
		doc->num_xref_sections = 1;
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(trailer);
		fz_rethrow(ctx);
	}
}

/*
 * magic version tag and startxref
 */

static void
pdf_load_version(pdf_document *doc)
{
	char buf[20];

	fz_seek(doc->file, 0, SEEK_SET);
	fz_read_line(doc->file, buf, sizeof buf);
	if (memcmp(buf, "%PDF-", 5) != 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "cannot recognize version marker");

	doc->version = 10 * (fz_atof(buf+5) + 0.05);
}

static void
pdf_read_start_xref(pdf_document *doc)
{
	unsigned char buf[1024];
	int t, n;
	int i;

	fz_seek(doc->file, 0, SEEK_END);

	doc->file_size = fz_tell(doc->file);

	t = fz_maxi(0, doc->file_size - (int)sizeof buf);
	fz_seek(doc->file, t, SEEK_SET);

	n = fz_read(doc->file, buf, sizeof buf);

	for (i = n - 9; i >= 0; i--)
	{
		if (memcmp(buf + i, "startxref", 9) == 0)
		{
			i += 9;
			while (iswhite(buf[i]) && i < n)
				i ++;
			doc->startxref = atoi((char*)(buf + i));
			if (doc->startxref != 0)
				return;
			break;
		}
	}

	fz_throw(doc->ctx, FZ_ERROR_GENERIC, "cannot find startxref");
}

/*
 * trailer dictionary
 */

static int
pdf_xref_size_from_old_trailer(pdf_document *doc, pdf_lexbuf *buf)
{
	int len;
	char *s;
	int t;
	pdf_token tok;
	int c;
	int size;
	int ofs;
	pdf_obj *trailer = NULL;

	fz_var(trailer);

	/* Record the current file read offset so that we can reinstate it */
	ofs = fz_tell(doc->file);

	fz_read_line(doc->file, buf->scratch, buf->size);
	if (strncmp(buf->scratch, "xref", 4) != 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "cannot find xref marker");

	while (1)
	{
		c = fz_peek_byte(doc->file);
		if (!(c >= '0' && c <= '9'))
			break;

		fz_read_line(doc->file, buf->scratch, buf->size);
		s = buf->scratch;
		fz_strsep(&s, " "); /* ignore ofs */
		if (!s)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "invalid range marker in xref");
		len = fz_atoi(fz_strsep(&s, " "));
		if (len <= 0)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "xref range marker must be positive");

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
			fz_seek(doc->file, -(2 + (int)strlen(s)), SEEK_CUR);

		t = fz_tell(doc->file);
		if (t < 0)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "cannot tell in file");
		if (len > (INT_MAX - t) / 20)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "xref has too many entries");

		fz_seek(doc->file, t + 20 * len, SEEK_SET);
	}

	fz_try(doc->ctx)
	{
		tok = pdf_lex(doc->file, buf);
		if (tok != PDF_TOK_TRAILER)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "expected trailer marker");

		tok = pdf_lex(doc->file, buf);
		if (tok != PDF_TOK_OPEN_DICT)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "expected trailer dictionary");

		trailer = pdf_parse_dict(doc, doc->file, buf);

		size = pdf_to_int(pdf_dict_gets(trailer, "Size"));
		if (!size)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "trailer missing Size entry");
	}
	fz_always(doc->ctx)
	{
		pdf_drop_obj(trailer);
	}
	fz_catch(doc->ctx)
	{
		fz_rethrow_message(doc->ctx, "cannot parse trailer");
	}

	fz_seek(doc->file, ofs, SEEK_SET);

	return size;
}

pdf_obj *
pdf_new_ref(pdf_document *doc, pdf_obj *obj)
{
	int num = pdf_create_object(doc);
	pdf_update_object(doc, num, obj);
	return pdf_new_indirect(doc, num, 0);
}

static pdf_obj *
pdf_read_old_xref(pdf_document *doc, pdf_lexbuf *buf)
{
	int ofs, len;
	char *s;
	int n;
	pdf_token tok;
	int i;
	int c;
	pdf_obj *trailer;
	int xref_len = pdf_xref_size_from_old_trailer(doc, buf);

	/* Access last entry to ensure xref size up front and avoid reallocs */
	if (xref_len > 0)
		(void)pdf_get_populating_xref_entry(doc, xref_len - 1);

	fz_read_line(doc->file, buf->scratch, buf->size);
	if (strncmp(buf->scratch, "xref", 4) != 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "cannot find xref marker");

	while (1)
	{
		c = fz_peek_byte(doc->file);
		if (!(c >= '0' && c <= '9'))
			break;

		fz_read_line(doc->file, buf->scratch, buf->size);
		s = buf->scratch;
		ofs = fz_atoi(fz_strsep(&s, " "));
		len = fz_atoi(fz_strsep(&s, " "));

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
		{
			fz_warn(doc->ctx, "broken xref section. proceeding anyway.");
			fz_seek(doc->file, -(2 + (int)strlen(s)), SEEK_CUR);
		}

		if (ofs < 0)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "out of range object num in xref: %d", ofs);

		/* broken pdfs where size in trailer undershoots entries in xref sections */
		if (ofs + len > xref_len)
		{
			fz_warn(doc->ctx, "broken xref section, proceeding anyway.");
			/* Access last entry to ensure size */
			(void)pdf_get_populating_xref_entry(doc, ofs + len - 1);
		}

		for (i = ofs; i < ofs + len; i++)
		{
			pdf_xref_entry *entry = pdf_get_populating_xref_entry(doc, i);
			n = fz_read(doc->file, (unsigned char *) buf->scratch, 20);
			if (n != 20)
				fz_throw(doc->ctx, FZ_ERROR_GENERIC, "unexpected EOF in xref table");
			if (!entry->type)
			{
				s = buf->scratch;

				/* broken pdfs where line start with white space */
				while (*s != '\0' && iswhite(*s))
					s++;

				entry->ofs = atoi(s);
				entry->gen = atoi(s + 11);
				entry->type = s[17];
				if (s[17] != 'f' && s[17] != 'n' && s[17] != 'o')
					fz_throw(doc->ctx, FZ_ERROR_GENERIC, "unexpected xref type: %#x (%d %d R)", s[17], i, entry->gen);
			}
		}
	}

	fz_try(doc->ctx)
	{
		tok = pdf_lex(doc->file, buf);
		if (tok != PDF_TOK_TRAILER)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "expected trailer marker");

		tok = pdf_lex(doc->file, buf);
		if (tok != PDF_TOK_OPEN_DICT)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "expected trailer dictionary");

		trailer = pdf_parse_dict(doc, doc->file, buf);
	}
	fz_catch(doc->ctx)
	{
		fz_rethrow_message(doc->ctx, "cannot parse trailer");
	}
	return trailer;
}

static void
pdf_read_new_xref_section(pdf_document *doc, fz_stream *stm, int i0, int i1, int w0, int w1, int w2)
{
	int i, n;

	if (i0 < 0 || i1 < 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "negative xref stream entry index");
	if (i0 + i1 > pdf_xref_len(doc))
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "xref stream has too many entries");

	for (i = i0; i < i0 + i1; i++)
	{
		pdf_xref_entry *entry = pdf_get_populating_xref_entry(doc, i);
		int a = 0;
		int b = 0;
		int c = 0;

		if (fz_is_eof(stm))
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "truncated xref stream");

		for (n = 0; n < w0; n++)
			a = (a << 8) + fz_read_byte(stm);
		for (n = 0; n < w1; n++)
			b = (b << 8) + fz_read_byte(stm);
		for (n = 0; n < w2; n++)
			c = (c << 8) + fz_read_byte(stm);

		if (!entry->type)
		{
			int t = w0 ? a : 1;
			entry->type = t == 0 ? 'f' : t == 1 ? 'n' : t == 2 ? 'o' : 0;
			entry->ofs = w1 ? b : 0;
			entry->gen = w2 ? c : 0;
		}
	}

	doc->has_xref_streams = 1;
}

/* Entered with file locked, remains locked throughout. */
static pdf_obj *
pdf_read_new_xref(pdf_document *doc, pdf_lexbuf *buf)
{
	fz_stream *stm = NULL;
	pdf_obj *trailer = NULL;
	pdf_obj *index = NULL;
	pdf_obj *obj = NULL;
	int num, gen, stm_ofs;
	int size, w0, w1, w2;
	int t;
	fz_context *ctx = doc->ctx;

	fz_var(trailer);
	fz_var(stm);

	fz_try(ctx)
	{
		pdf_xref_entry *entry;
		int ofs = fz_tell(doc->file);
		trailer = pdf_parse_ind_obj(doc, doc->file, buf, &num, &gen, &stm_ofs, NULL);
		entry = pdf_get_populating_xref_entry(doc, num);
		entry->ofs = ofs;
		entry->gen = gen;
		entry->stm_ofs = stm_ofs;
		pdf_drop_obj(entry->obj);
		entry->obj = pdf_keep_obj(trailer);
		entry->type = 'n';
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(trailer);
		fz_rethrow_message(ctx, "cannot parse compressed xref stream object");
	}

	fz_try(ctx)
	{
		obj = pdf_dict_gets(trailer, "Size");
		if (!obj)
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref stream missing Size entry (%d %d R)", num, gen);

		size = pdf_to_int(obj);
		/* Access xref entry to assure table size */
		if (size > 0)
			(void)pdf_get_populating_xref_entry(doc, size-1);

		obj = pdf_dict_gets(trailer, "W");
		if (!obj)
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref stream missing W entry (%d %d R)", num, gen);
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

		stm = pdf_open_stream_with_offset(doc, num, gen, trailer, stm_ofs);

		if (!index)
		{
			pdf_read_new_xref_section(doc, stm, 0, size, w0, w1, w2);
		}
		else
		{
			int n = pdf_array_len(index);
			for (t = 0; t < n; t += 2)
			{
				int i0 = pdf_to_int(pdf_array_get(index, t + 0));
				int i1 = pdf_to_int(pdf_array_get(index, t + 1));
				pdf_read_new_xref_section(doc, stm, i0, i1, w0, w1, w2);
			}
		}
	}
	fz_always(ctx)
	{
		fz_close(stm);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(trailer);
		fz_rethrow(ctx);
	}

	return trailer;
}

static pdf_obj *
pdf_read_xref(pdf_document *doc, int ofs, pdf_lexbuf *buf)
{
	int c;
	fz_context *ctx = doc->ctx;
	pdf_obj *trailer;

	fz_seek(doc->file, ofs, SEEK_SET);

	while (iswhite(fz_peek_byte(doc->file)))
		fz_read_byte(doc->file);

	fz_try(ctx)
	{
		c = fz_peek_byte(doc->file);
		if (c == 'x')
			trailer = pdf_read_old_xref(doc, buf);
		else if (c >= '0' && c <= '9')
			trailer = pdf_read_new_xref(doc, buf);
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot recognize xref format");
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot read xref (ofs=%d)", ofs);
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

static int
read_xref_section(pdf_document *doc, int ofs, pdf_lexbuf *buf, ofs_list *offsets)
{
	pdf_obj *trailer = NULL;
	fz_context *ctx = doc->ctx;
	int xrefstmofs = 0;
	int prevofs = 0;

	fz_var(trailer);

	fz_try(ctx)
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

		trailer = pdf_read_xref(doc, ofs, buf);

		pdf_set_populating_xref_trailer(doc, trailer);

		/* FIXME: do we overwrite free entries properly? */
		/* FIXME: Does this work properly with progression? */
		xrefstmofs = pdf_to_int(pdf_dict_gets(trailer, "XRefStm"));
		if (xrefstmofs)
		{
			if (xrefstmofs < 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "negative xref stream offset");

			/*
				Read the XRefStm stream, but throw away the resulting trailer. We do not
				follow any Prev tag therein, as specified on Page 108 of the PDF reference
				1.7
			*/
			pdf_drop_obj(pdf_read_xref(doc, xrefstmofs, buf));
		}

		prevofs = pdf_to_int(pdf_dict_gets(trailer, "Prev"));
		if (prevofs < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "negative xref stream offset for previous xref stream");
	}
	fz_always(ctx)
	{
		pdf_drop_obj(trailer);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot read xref at offset %d", ofs);
	}

	return prevofs;
}

static void
pdf_read_xref_sections(pdf_document *doc, int ofs, pdf_lexbuf *buf, int read_previous)
{
	fz_context *ctx = doc->ctx;
	ofs_list list;

	list.len = 0;
	list.max = 10;
	list.list = fz_malloc_array(ctx, 10, sizeof(int));
	fz_try(ctx)
	{
		while(ofs)
		{
			pdf_populate_next_xref_level(doc);
			ofs = read_xref_section(doc, ofs, buf, &list);
			if (!read_previous)
				break;
		}
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
pdf_load_xref(pdf_document *doc, pdf_lexbuf *buf)
{
	int i;
	int xref_len;
	pdf_xref_entry *entry;
	fz_context *ctx = doc->ctx;

	pdf_read_start_xref(doc);

	pdf_read_xref_sections(doc, doc->startxref, buf, 1);

	if (pdf_xref_len(doc) == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "found xref was empty");

	entry = pdf_get_xref_entry(doc, 0);
	/* broken pdfs where first object is missing */
	if (!entry->type)
	{
		entry->type = 'f';
		entry->gen = 65535;
	}
	/* broken pdfs where first object is not free */
	else if (entry->type != 'f')
		fz_throw(ctx, FZ_ERROR_GENERIC, "first object in xref is not free");

	/* broken pdfs where object offsets are out of range */
	xref_len = pdf_xref_len(doc);
	for (i = 0; i < xref_len; i++)
	{
		pdf_xref_entry *entry = pdf_get_xref_entry(doc, i);
		if (entry->type == 'n')
		{
			/* Special case code: "0000000000 * n" means free,
			 * according to some producers (inc Quartz) */
			if (entry->ofs == 0)
				entry->type = 'f';
			else if (entry->ofs <= 0 || entry->ofs >= doc->file_size)
				fz_throw(ctx, FZ_ERROR_GENERIC, "object offset out of range: %d (%d 0 R)", entry->ofs, i);
		}
		if (entry->type == 'o')
			if (entry->ofs <= 0 || entry->ofs >= xref_len || pdf_get_xref_entry(doc, entry->ofs)->type != 'n')
				fz_throw(ctx, FZ_ERROR_GENERIC, "invalid reference to an objstm that does not exist: %d (%d 0 R)", entry->ofs, i);
	}
}

static void
pdf_load_linear(pdf_document *doc)
{
	pdf_obj *dict = NULL;
	pdf_obj *hint = NULL;
	pdf_obj *o;
	int num, gen, stmofs, lin, len;
	fz_context *ctx = doc->ctx;

	fz_var(dict);
	fz_var(hint);

	fz_try(ctx)
	{
		pdf_xref_entry *entry;

		dict = pdf_parse_ind_obj(doc, doc->file, &doc->lexbuf.base, &num, &gen, &stmofs, NULL);
		if (!pdf_is_dict(dict))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to read linearized dictionary");
		o = pdf_dict_gets(dict, "Linearized");
		if (o == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to read linearized dictionary");
		lin = pdf_to_int(o);
		if (lin != 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Unexpected version of Linearized tag (%d)", lin);
		len = pdf_to_int(pdf_dict_gets(dict, "L"));
		if (len != doc->file_length)
			fz_throw(ctx, FZ_ERROR_GENERIC, "File has been updated since linearization");

		pdf_read_xref_sections(doc, fz_tell(doc->file), &doc->lexbuf.base, 0);

		doc->page_count = pdf_to_int(pdf_dict_gets(dict, "N"));
		doc->linear_page_refs = fz_resize_array(ctx, doc->linear_page_refs, doc->page_count, sizeof(pdf_obj *));
		memset(doc->linear_page_refs, 0, doc->page_count * sizeof(pdf_obj*));
		doc->linear_obj = dict;
		doc->linear_pos = fz_tell(doc->file);
		doc->linear_page1_obj_num = pdf_to_int(pdf_dict_gets(dict, "O"));
		doc->linear_page_refs[0] = pdf_new_indirect(doc, doc->linear_page1_obj_num, 0);
		doc->linear_page_num = 0;
		hint = pdf_dict_gets(dict, "H");
		doc->hint_object_offset = pdf_to_int(pdf_array_get(hint, 0));
		doc->hint_object_length = pdf_to_int(pdf_array_get(hint, 1));

		entry = pdf_get_populating_xref_entry(doc, 0);
		entry->type = 'f';
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(dict);
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* Drop back to non linearized reading mode */
		doc->file_reading_linearly = 0;
	}
}

void
pdf_ocg_set_config(pdf_document *doc, int config)
{
	int i, j, len, len2;
	pdf_ocg_descriptor *desc = doc->ocg;
	pdf_obj *obj, *cobj;
	char *name;

	obj = pdf_dict_gets(pdf_dict_gets(pdf_trailer(doc), "Root"), "OCProperties");
	if (!obj)
	{
		if (config == 0)
			return;
		else
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "Unknown OCG config (None known!)");
	}
	if (config == 0)
	{
		cobj = pdf_dict_gets(obj, "D");
		if (!cobj)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "No default OCG config");
	}
	else
	{
		cobj = pdf_array_get(pdf_dict_gets(obj, "Configs"), config);
		if (!cobj)
			fz_throw(doc->ctx, FZ_ERROR_GENERIC, "Illegal OCG config");
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
pdf_read_ocg(pdf_document *doc)
{
	pdf_obj *obj, *ocg;
	int len, i;
	pdf_ocg_descriptor *desc;
	fz_context *ctx = doc->ctx;

	fz_var(desc);

	obj = pdf_dict_gets(pdf_dict_gets(pdf_trailer(doc), "Root"), "OCProperties");
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
			desc->ocgs[i].state = 1;
		}
		doc->ocg = desc;
	}
	fz_catch(ctx)
	{
		if (desc)
			fz_free(ctx, desc->ocgs);
		fz_free(ctx, desc);
		fz_rethrow(ctx);
	}

	pdf_ocg_set_config(doc, 0);
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
pdf_init_document(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *encrypt, *id;
	pdf_obj *dict = NULL;
	pdf_obj *obj;
	pdf_obj *nobj = NULL;
	int i, repaired = 0;

	fz_var(dict);
	fz_var(nobj);

	fz_try(ctx)
	{
		pdf_load_version(doc);

		doc->file_length = fz_stream_meta(doc->file, FZ_STREAM_META_LENGTH, 0, NULL);
		if (doc->file_length < 0)
			doc->file_length = 0;

		/* Check to see if we should work in progressive mode */
		if (fz_stream_meta(doc->file, FZ_STREAM_META_PROGRESSIVE, 0, NULL) > 0)
			doc->file_reading_linearly = 1;

		/* Try to load the linearized file if we are in progressive
		 * mode. */
		if (doc->file_reading_linearly)
			pdf_load_linear(doc);

		/* If we aren't in progressive mode (or the linear load failed
		 * and has set us back to non-progressive mode), load normally.
		 */
		if (!doc->file_reading_linearly)
			pdf_load_xref(doc, &doc->lexbuf.base);
	}
	fz_catch(ctx)
	{
		pdf_free_xref_sections(doc);
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "trying to repair broken xref");
		repaired = 1;
	}

	fz_try(ctx)
	{
		int hasroot, hasinfo;

		if (repaired)
			pdf_repair_xref(doc, &doc->lexbuf.base);

		encrypt = pdf_dict_gets(pdf_trailer(doc), "Encrypt");
		id = pdf_dict_gets(pdf_trailer(doc), "ID");
		if (pdf_is_dict(encrypt))
			doc->crypt = pdf_new_crypt(ctx, encrypt, id);

		/* Allow lazy clients to read encrypted files with a blank password */
		pdf_authenticate_password(doc, "");

		if (repaired)
		{
			int xref_len = pdf_xref_len(doc);
			pdf_repair_obj_stms(doc);

			hasroot = (pdf_dict_gets(pdf_trailer(doc), "Root") != NULL);
			hasinfo = (pdf_dict_gets(pdf_trailer(doc), "Info") != NULL);

			for (i = 1; i < xref_len; i++)
			{
				pdf_xref_entry *entry = pdf_get_xref_entry(doc, i);
				if (entry->type == 0 || entry->type == 'f')
					continue;

				fz_try(ctx)
				{
					dict = pdf_load_object(doc, i, 0);
				}
				fz_catch(ctx)
				{
					fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
					fz_warn(ctx, "ignoring broken object (%d 0 R)", i);
					continue;
				}

				if (!hasroot)
				{
					obj = pdf_dict_gets(dict, "Type");
					if (pdf_is_name(obj) && !strcmp(pdf_to_name(obj), "Catalog"))
					{
						nobj = pdf_new_indirect(doc, i, 0);
						pdf_dict_puts(pdf_trailer(doc), "Root", nobj);
						pdf_drop_obj(nobj);
						nobj = NULL;
					}
				}

				if (!hasinfo)
				{
					if (pdf_dict_gets(dict, "Creator") || pdf_dict_gets(dict, "Producer"))
					{
						nobj = pdf_new_indirect(doc, i, 0);
						pdf_dict_puts(pdf_trailer(doc), "Info", nobj);
						pdf_drop_obj(nobj);
						nobj = NULL;
					}
				}

				pdf_drop_obj(dict);
				dict = NULL;
			}
		}
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(dict);
		pdf_drop_obj(nobj);
		fz_rethrow_message(ctx, "cannot open document");
	}

	fz_try(ctx)
	{
		pdf_read_ocg(doc);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "Ignoring Broken Optional Content");
	}

	fz_try(ctx)
	{
		char *version_str;
		obj = pdf_dict_getp(pdf_trailer(doc), "Root/Version");
		version_str = pdf_to_name(obj);
		if (*version_str)
		{
			int version = 10 * (fz_atof(version_str) + 0.05);
			if (version > doc->version)
				doc->version = version;
		}
	}
	fz_catch(ctx) { }
}

void
pdf_close_document(pdf_document *doc)
{
	fz_context *ctx;
	pdf_unsaved_sig *usig;
	int i;

	if (!doc)
		return;
	ctx = doc->ctx;

	/* Type3 glyphs in the glyph cache can contain pdf_obj pointers
	 * that we are about to destroy. Simplest solution is to bin the
	 * glyph cache at this point. */
	fz_purge_glyph_cache(ctx);

	if (doc->js)
		doc->drop_js(doc->js);

	pdf_free_xref_sections(doc);

	if (doc->focus_obj)
		pdf_drop_obj(doc->focus_obj);
	if (doc->file)
		fz_close(doc->file);
	if (doc->crypt)
		pdf_free_crypt(ctx, doc->crypt);

	pdf_drop_obj(doc->linear_obj);
	if (doc->linear_page_refs)
	{
		for (i=0; i < doc->page_count; i++)
		{
			pdf_drop_obj(doc->linear_page_refs[i]);
		}
		fz_free(ctx, doc->linear_page_refs);
	}
	fz_free(ctx, doc->hint_page);
	fz_free(ctx, doc->hint_shared_ref);
	fz_free(ctx, doc->hint_shared);
	fz_free(ctx, doc->hint_obj_offsets);

	while ((usig = doc->unsaved_sigs) != NULL)
	{
		doc->unsaved_sigs = usig->next;
		pdf_drop_obj(usig->field);
		pdf_drop_signer(usig->signer);
		fz_free(ctx, usig);
	}

	for (i=0; i < doc->num_type3_fonts; i++)
	{
		fz_decouple_type3_font(ctx, doc->type3_fonts[i], (void *)doc);
		fz_drop_font(ctx, doc->type3_fonts[i]);
	}
	fz_free(ctx, doc->type3_fonts);

	pdf_free_ocg(ctx, doc->ocg);

	fz_empty_store(ctx);

	pdf_lexbuf_fin(&doc->lexbuf.base);

	fz_free(ctx, doc);
}

void
pdf_print_xref(pdf_document *doc)
{
	int i;
	int xref_len = pdf_xref_len(doc);
	printf("xref\n0 %d\n", xref_len);
	for (i = 0; i < xref_len; i++)
	{
		pdf_xref_entry *entry = pdf_get_xref_entry(doc, i);
		printf("%05d: %010d %05d %c (stm_ofs=%d; stm_buf=%p)\n", i,
			entry->ofs,
			entry->gen,
			entry->type ? entry->type : '-',
			entry->stm_ofs,
			entry->stm_buf);
	}
}

/*
 * compressed object streams
 */

static void
pdf_load_obj_stm(pdf_document *doc, int num, int gen, pdf_lexbuf *buf)
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
	fz_context *ctx = doc->ctx;

	fz_var(numbuf);
	fz_var(ofsbuf);
	fz_var(objstm);
	fz_var(stm);

	fz_try(ctx)
	{
		objstm = pdf_load_object(doc, num, gen);

		count = pdf_to_int(pdf_dict_gets(objstm, "N"));
		first = pdf_to_int(pdf_dict_gets(objstm, "First"));

		if (count < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "negative number of objects in object stream");
		if (first < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "first object in object stream resides outside stream");

		numbuf = fz_calloc(ctx, count, sizeof(int));
		ofsbuf = fz_calloc(ctx, count, sizeof(int));

		stm = pdf_open_stream(doc, num, gen);
		for (i = 0; i < count; i++)
		{
			tok = pdf_lex(stm, buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, FZ_ERROR_GENERIC, "corrupt object stream (%d %d R)", num, gen);
			numbuf[i] = buf->i;

			tok = pdf_lex(stm, buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, FZ_ERROR_GENERIC, "corrupt object stream (%d %d R)", num, gen);
			ofsbuf[i] = buf->i;
		}

		fz_seek(stm, first, SEEK_SET);

		for (i = 0; i < count; i++)
		{
			int xref_len = pdf_xref_len(doc);
			pdf_xref_entry *entry;
			fz_seek(stm, first + ofsbuf[i], SEEK_SET);

			obj = pdf_parse_stm_obj(doc, stm, buf);

			if (numbuf[i] < 1 || numbuf[i] >= xref_len)
			{
				pdf_drop_obj(obj);
				fz_throw(ctx, FZ_ERROR_GENERIC, "object id (%d 0 R) out of range (0..%d)", numbuf[i], xref_len - 1);
			}

			entry = pdf_get_xref_entry(doc, numbuf[i]);

			pdf_set_obj_parent(obj, numbuf[i]);

			if (entry->type == 'o' && entry->ofs == num)
			{
				/* If we already have an entry for this object,
				 * we'd like to drop it and use the new one -
				 * but this means that anyone currently holding
				 * a pointer to the old one will be left with a
				 * stale pointer. Instead, we drop the new one
				 * and trust that the old one is correct. */
				if (entry->obj) {
					if (pdf_objcmp(entry->obj, obj))
						fz_warn(ctx, "Encountered new definition for object %d - keeping the original one", numbuf[i]);
					pdf_drop_obj(obj);
				} else
					entry->obj = obj;
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
		fz_free(ctx, ofsbuf);
		fz_free(ctx, numbuf);
		pdf_drop_obj(objstm);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot open object stream (%d %d R)", num, gen);
	}
}

/*
 * object loading
 */
static int
pdf_obj_read(pdf_document *doc, int *offset, int *nump, pdf_obj **page)
{
	int num, numofs, gen, genofs, stmofs, tmpofs, tok;
	pdf_lexbuf *buf = &doc->lexbuf.base;
	fz_context *ctx = doc->ctx;
	int xref_len;
	pdf_xref_entry *entry;
	int newtmpofs;

	numofs = *offset;
	fz_seek(doc->file, numofs, SEEK_SET);

	/* We expect to read 'num' here */
	tok = pdf_lex(doc->file, buf);
	genofs = fz_tell(doc->file);
	if (tok != PDF_TOK_INT)
	{
		/* Failed! */
		DEBUGMESS((ctx, "skipping unexpected data (tok=%d) at %d", tok, *offset));
		*offset = genofs;
		return tok == PDF_TOK_EOF;
	}
	*nump = num = buf->i;

	/* We expect to read 'gen' here */
	tok = pdf_lex(doc->file, buf);
	tmpofs = fz_tell(doc->file);
	if (tok != PDF_TOK_INT)
	{
		/* Failed! */
		DEBUGMESS((ctx, "skipping unexpected data after \"%d\" (tok=%d) at %d", num, tok, *offset));
		*offset = tmpofs;
		return tok == PDF_TOK_EOF;
	}
	gen = buf->i;

	/* We expect to read 'obj' here */
	do
	{
		tmpofs = fz_tell(doc->file);
		tok = pdf_lex(doc->file, buf);
		if (tok == PDF_TOK_OBJ)
			break;
		if (tok != PDF_TOK_INT)
		{
			DEBUGMESS((ctx, "skipping unexpected data (tok=%d) at %d", tok, tmpofs));
			*offset = fz_tell(doc->file);
			return tok == PDF_TOK_EOF;
		}
		DEBUGMESS((ctx, "skipping unexpected int %d at %d", num, numofs));
		*nump = num = gen;
		numofs = genofs;
		gen = buf->i;
		genofs = tmpofs;
	}
	while (1);

	/* Now we read the actual object */
	xref_len = pdf_xref_len(doc);

	/* When we are reading a progressive file, we typically see:
	 *    File Header
	 *    obj m (Linearization params)
	 *    xref #1 (refers to objects m-n)
	 *    obj m+1
	 *    ...
	 *    obj n
	 *    obj 1
	 *    ...
	 *    obj n-1
	 *    xref #2
	 *
	 * The linearisation params are read elsewhere, hence
	 * whenever we read an object it should just go into the
	 * previous xref.
	 */
	tok = pdf_repair_obj(doc, buf, &stmofs, NULL, NULL, NULL, page, &newtmpofs);

	do /* So we can break out of it */
	{
		if (num <= 0 || num >= xref_len)
		{
			fz_warn(ctx, "Not a valid object number (%d %d obj)", num, gen);
			break;
		}
		if (gen != 0)
		{
			fz_warn(ctx, "Unexpected non zero generation number in linearized file");
		}
		entry = pdf_get_populating_xref_entry(doc, num);
		if (entry->type != 0)
		{
			DEBUGMESS((ctx, "Duplicate object found (%d %d obj)", num, gen));
			break;
		}
		if (page && *page)
		{
			DEBUGMESS((ctx, "Successfully read object %d @ %d - and found page %d!", num, numofs, doc->linear_page_num));
			if (!entry->obj)
				entry->obj = pdf_keep_obj(*page);

			if (doc->linear_page_refs[doc->linear_page_num] == NULL)
				doc->linear_page_refs[doc->linear_page_num] = pdf_new_indirect(doc, num, gen);
		}
		else
		{
			DEBUGMESS((ctx, "Successfully read object %d @ %d", num, numofs));
		}
		entry->type = 'n';
		entry->gen = 0;
		entry->ofs = numofs;
		entry->stm_ofs = stmofs;
	}
	while (0);
	if (page && *page)
		doc->linear_page_num++;

	if (tok == PDF_TOK_ENDOBJ)
	{
		*offset = fz_tell(doc->file);
	}
	else
	{
		*offset = newtmpofs;
	}
	return 0;
}

static void
pdf_load_hinted_page(pdf_document *doc, int pagenum)
{
	fz_context *ctx = doc->ctx;

	if (!doc->hints_loaded || !doc->linear_page_refs)
		return;

	if (doc->linear_page_refs[pagenum])
		return;

	fz_try(ctx)
	{
		int num = doc->hint_page[pagenum].number;
		pdf_obj *page = pdf_load_object(doc, num, 0);
		if (!strcmp("Page", pdf_to_name(pdf_dict_gets(page, "Type"))))
		{
			/* We have found the page object! */
			DEBUGMESS((ctx, "LoadHintedPage pagenum=%d num=%d", pagenum, num));
			doc->linear_page_refs[pagenum] = pdf_new_indirect(doc, num, 0);
		}
		pdf_drop_obj(page);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* Silently swallow the error and proceed as normal */
	}

}

static int
read_hinted_object(pdf_document *doc, int num)
{
	/* Try to find the object using our hint table. Find the closest
	 * object <= the one we want that has a hint and read forward from
	 * there. */
	fz_context *ctx = doc->ctx;
	int expected = num;
	int curr_pos;
	int start, offset;

	while (doc->hint_obj_offsets[expected] == 0 && expected > 0)
		expected--;
	if (expected != num)
		DEBUGMESS((ctx, "object %d is unhinted, will search forward from %d", expected, num));
	if (expected == 0)	/* No hints found, just bale */
		return 0;

	curr_pos = fz_tell(doc->file);
	offset = doc->hint_obj_offsets[expected];

	fz_var(expected);

	fz_try(ctx)
	{
		int found;

		/* Try to read forward from there */
		do
		{
			start = offset;
			DEBUGMESS((ctx, "Searching for object %d @ %d", expected, offset));
			pdf_obj_read(doc, &offset, &found, 0);
			DEBUGMESS((ctx, "Found object %d - next will be @ %d", found, offset));
			if (found <= expected)
			{
				/* We found the right one (or one earlier than
				 * we expected). Update the hints. */
				doc->hint_obj_offsets[expected] = offset;
				doc->hint_obj_offsets[found] = start;
				doc->hint_obj_offsets[found+1] = offset;
				/* Retry with the next one */
				expected = found+1;
			}
			else
			{
				/* We found one later than we expected. */
				doc->hint_obj_offsets[expected] = 0;
				doc->hint_obj_offsets[found] = start;
				doc->hint_obj_offsets[found+1] = offset;
				while (doc->hint_obj_offsets[expected] == 0 && expected > 0)
					expected--;
				if (expected == 0)	/* No hints found, just bale */
					return 0;
			}
		}
		while (found != num);
	}
	fz_always(ctx)
	{
		fz_seek(doc->file, curr_pos, SEEK_SET);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* FIXME: Currently we ignore the hint. Perhaps we should
		 * drop back to non-hinted operation here. */
		doc->hint_obj_offsets[expected] = 0;
		fz_rethrow(ctx);
	}
	return 1;
}

void
pdf_cache_object(pdf_document *doc, int num, int gen)
{
	pdf_xref_entry *x;
	int rnum, rgen, try_repair;
	fz_context *ctx = doc->ctx;

	fz_var(try_repair);

	if (num < 0 || num >= pdf_xref_len(doc))
		fz_throw(ctx, FZ_ERROR_GENERIC, "object out of range (%d %d R); xref size %d", num, gen, pdf_xref_len(doc));

object_updated:
	try_repair = 0;
	rnum = num;

	x = pdf_get_xref_entry(doc, num);

	if (x->obj)
		return;

	if (x->type == 'f')
	{
		x->obj = pdf_new_null(doc);
	}
	else if (x->type == 'n')
	{
		fz_seek(doc->file, x->ofs, SEEK_SET);

		fz_try(ctx)
		{
			x->obj = pdf_parse_ind_obj(doc, doc->file, &doc->lexbuf.base,
					&rnum, &rgen, &x->stm_ofs, &try_repair);
		}
		fz_catch(ctx)
		{
			if (!try_repair || fz_caught(ctx) == FZ_ERROR_TRYLATER)
				fz_rethrow(ctx);
		}

		if (!try_repair && rnum != num)
		{
			pdf_drop_obj(x->obj);
			x->obj = NULL;
			try_repair = 1;
		}

		if (try_repair)
		{
			fz_try(ctx)
			{
				pdf_repair_xref(doc, &doc->lexbuf.base);
			}
			fz_catch(ctx)
			{
				if (rnum == num)
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot parse object (%d %d R)", num, gen);
				else
					fz_throw(ctx, FZ_ERROR_GENERIC, "found object (%d %d R) instead of (%d %d R)", rnum, rgen, num, gen);
			}
			goto object_updated;
		}

		if (doc->crypt)
			pdf_crypt_obj(ctx, doc->crypt, x->obj, num, gen);
	}
	else if (x->type == 'o')
	{
		if (!x->obj)
		{
			fz_try(ctx)
			{
				pdf_load_obj_stm(doc, x->ofs, 0, &doc->lexbuf.base);
			}
			fz_catch(ctx)
			{
				fz_rethrow_message(ctx, "cannot load object stream containing object (%d %d R)", num, gen);
			}
			x = pdf_get_xref_entry(doc, num);
			if (!x->obj)
				fz_throw(ctx, FZ_ERROR_GENERIC, "object (%d %d R) was not found in its object stream", num, gen);
		}
	}
	else if (doc->hint_obj_offsets && read_hinted_object(doc, num))
	{
		goto object_updated;
	}
	else if (doc->file_length && doc->linear_pos < doc->file_length)
	{
		fz_throw(ctx, FZ_ERROR_TRYLATER, "cannot find object in xref (%d %d R) - not loaded yet?", num, gen);
	}
	else
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find object in xref (%d %d R)", num, gen);
	}

	pdf_set_obj_parent(x->obj, num);
}

pdf_obj *
pdf_load_object(pdf_document *doc, int num, int gen)
{
	fz_context *ctx = doc->ctx;
	pdf_xref_entry *entry;

	fz_try(ctx)
	{
		pdf_cache_object(doc, num, gen);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot load object (%d %d R) into cache", num, gen);
	}

	entry = pdf_get_xref_entry(doc, num);

	assert(entry->obj);

	return pdf_keep_obj(entry->obj);
}

pdf_obj *
pdf_resolve_indirect(pdf_obj *ref)
{
	int sanity = 10;
	int num;
	int gen;
	fz_context *ctx = NULL; /* Avoid warning for stupid compilers */
	pdf_document *doc;
	pdf_xref_entry *entry;

	while (pdf_is_indirect(ref))
	{
		if (--sanity == 0)
		{
			fz_warn(ctx, "Too many indirections (possible indirection cycle involving %d %d R)", num, gen);
			return NULL;
		}
		doc = pdf_get_indirect_document(ref);
		if (!doc)
			return NULL;
		ctx = doc->ctx;
		num = pdf_to_num(ref);
		gen = pdf_to_gen(ref);
		fz_try(ctx)
		{
			pdf_cache_object(doc, num, gen);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_warn(ctx, "cannot load object (%d %d R) into cache", num, gen);
			return NULL;
		}
		entry = pdf_get_xref_entry(doc, num);
		if (!entry->obj)
			return NULL;
		ref = entry->obj;
	}

	return ref;
}

int
pdf_count_objects(pdf_document *doc)
{
	return pdf_xref_len(doc);
}

int
pdf_create_object(pdf_document *doc)
{
	/* TODO: reuse free object slots by properly linking free object chains in the ofs field */
	pdf_xref_entry *entry;
	int num = pdf_xref_len(doc);
	entry = pdf_get_incremental_xref_entry(doc, num);
	entry->type = 'f';
	entry->ofs = -1;
	entry->gen = 0;
	entry->stm_ofs = 0;
	entry->stm_buf = NULL;
	entry->obj = NULL;
	return num;
}

void
pdf_delete_object(pdf_document *doc, int num)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= pdf_xref_len(doc))
	{
		fz_warn(doc->ctx, "object out of range (%d 0 R); xref size %d", num, pdf_xref_len(doc));
		return;
	}

	x = pdf_get_incremental_xref_entry(doc, num);

	fz_drop_buffer(doc->ctx, x->stm_buf);
	pdf_drop_obj(x->obj);

	x->type = 'f';
	x->ofs = 0;
	x->gen = 0;
	x->stm_ofs = 0;
	x->stm_buf = NULL;
	x->obj = NULL;
}

void
pdf_update_object(pdf_document *doc, int num, pdf_obj *newobj)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= pdf_xref_len(doc))
	{
		fz_warn(doc->ctx, "object out of range (%d 0 R); xref size %d", num, pdf_xref_len(doc));
		return;
	}

	x = pdf_get_incremental_xref_entry(doc, num);

	pdf_drop_obj(x->obj);

	x->type = 'n';
	x->ofs = 0;
	x->obj = pdf_keep_obj(newobj);

	pdf_set_obj_parent(newobj, num);
}

void
pdf_update_stream(pdf_document *doc, int num, fz_buffer *newbuf)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= pdf_xref_len(doc))
	{
		fz_warn(doc->ctx, "object out of range (%d 0 R); xref size %d", num, pdf_xref_len(doc));
		return;
	}

	x = pdf_get_xref_entry(doc, num);

	fz_drop_buffer(doc->ctx, x->stm_buf);
	x->stm_buf = fz_keep_buffer(doc->ctx, newbuf);
}

int
pdf_meta(pdf_document *doc, int key, void *ptr, int size)
{
	switch (key)
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
		pdf_obj *info = pdf_dict_gets(pdf_trailer(doc), "Info");
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

static void
pdf_rebind(pdf_document *doc, fz_context *ctx)
{
	doc->ctx = ctx;
	fz_rebind_stream(doc->file, ctx);
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

	doc->super.close = (fz_document_close_fn *)pdf_close_document;
	doc->super.needs_password = (fz_document_needs_password_fn *)pdf_needs_password;
	doc->super.authenticate_password = (fz_document_authenticate_password_fn *)pdf_authenticate_password;
	doc->super.load_outline = (fz_document_load_outline_fn *)pdf_load_outline;
	doc->super.count_pages = (fz_document_count_pages_fn *)pdf_count_pages;
	doc->super.load_page = (fz_document_load_page_fn *)pdf_load_page;
	doc->super.load_links = (fz_document_load_links_fn *)pdf_load_links;
	doc->super.bound_page = (fz_document_bound_page_fn *)pdf_bound_page;
	doc->super.first_annot = (fz_document_first_annot_fn *)pdf_first_annot;
	doc->super.next_annot = (fz_document_next_annot_fn *)pdf_next_annot;
	doc->super.bound_annot = (fz_document_bound_annot_fn *)pdf_bound_annot;
	doc->super.run_page_contents = NULL; /* see pdf_xref_aux.c */
	doc->super.run_annot = NULL; /* see pdf_xref_aux.c */
	doc->super.free_page = (fz_document_free_page_fn *)pdf_free_page;
	doc->super.meta = (fz_document_meta_fn *)pdf_meta;
	doc->super.page_presentation = (fz_document_page_presentation_fn *)pdf_page_presentation;
	doc->super.write = (fz_document_write_fn *)pdf_write_document;
	doc->super.rebind = (fz_document_rebind_fn *)pdf_rebind;

	pdf_lexbuf_init(ctx, &doc->lexbuf.base, PDF_LEXBUF_LARGE);
	doc->file = fz_keep_stream(file);
	doc->ctx = ctx;

	return doc;
}

pdf_document *
pdf_open_document_no_run_with_stream(fz_context *ctx, fz_stream *file)
{
	pdf_document *doc = pdf_new_document(ctx, file);

	fz_var(doc);

	fz_try(ctx)
	{
		pdf_init_document(doc);
	}
	fz_catch(ctx)
	{
		pdf_close_document(doc);
		fz_rethrow_message(ctx, "cannot load document from stream");
	}
	return doc;
}

pdf_document *
pdf_open_document_no_run(fz_context *ctx, const char *filename)
{
	fz_stream *file = NULL;
	pdf_document *doc = NULL;

	fz_var(file);
	fz_var(doc);

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
		pdf_close_document(doc);
		fz_rethrow_message(ctx, "cannot load document '%s'", filename);
	}
	return doc;
}

static void
pdf_load_hints(pdf_document *doc, int objnum, int gennum)
{
	fz_stream *stream = NULL;
	pdf_obj *dict;
	fz_context *ctx = doc->ctx;

	fz_var(stream);
	fz_var(dict);

	fz_try(ctx)
	{
		int i, j, least_num_page_objs, page_obj_num_bits;
		int least_page_len, page_len_num_bits, shared_hint_offset;
		/* int least_page_offset, page_offset_num_bits; */
		/* int least_content_stream_len, content_stream_len_num_bits; */
		int num_shared_obj_num_bits, shared_obj_num_bits;
		/* int numerator_bits, denominator_bits; */
		int shared;
		int shared_obj_num, shared_obj_offset, shared_obj_count_page1;
		int shared_obj_count_total;
		int least_shared_group_len, shared_group_len_num_bits;
		int max_object_num = pdf_xref_len(doc);

		stream = pdf_open_stream(doc, objnum, gennum);
		dict = pdf_get_xref_entry(doc, objnum)->obj;
		if (dict == NULL || !pdf_is_dict(dict))
			fz_throw(ctx, FZ_ERROR_GENERIC, "malformed hint object");

		shared_hint_offset = pdf_to_int(pdf_dict_gets(dict, "S"));

		/* Malloc the structures (use realloc to cope with the fact we
		 * may try this several times before enough data is loaded) */
		doc->hint_page = fz_resize_array(ctx, doc->hint_page, doc->page_count+1, sizeof(*doc->hint_page));
		memset(doc->hint_page, 0, sizeof(*doc->hint_page) * (doc->page_count+1));
		doc->hint_obj_offsets = fz_resize_array(ctx, doc->hint_obj_offsets, max_object_num, sizeof(*doc->hint_obj_offsets));
		memset(doc->hint_obj_offsets, 0, sizeof(*doc->hint_obj_offsets) * max_object_num);
		doc->hint_obj_offsets_max = max_object_num;

		/* Read the page object hints table: Header first */
		least_num_page_objs = fz_read_bits(stream, 32);
		/* The following is sometimes a lie, but we read this version,
		 * as other table values are built from it. In
		 * pdf_reference17.pdf, this points to 2 objects before the
		 * first pages page object. */
		doc->hint_page[0].offset = fz_read_bits(stream, 32);
		if (doc->hint_page[0].offset > doc->hint_object_offset)
			doc->hint_page[0].offset += doc->hint_object_length;
		page_obj_num_bits = fz_read_bits(stream, 16);
		least_page_len = fz_read_bits(stream, 32);
		page_len_num_bits = fz_read_bits(stream, 16);
		/* least_page_offset = */ (void) fz_read_bits(stream, 32);
		/* page_offset_num_bits = */ (void) fz_read_bits(stream, 16);
		/* least_content_stream_len = */ (void) fz_read_bits(stream, 32);
		/* content_stream_len_num_bits = */ (void) fz_read_bits(stream, 16);
		num_shared_obj_num_bits = fz_read_bits(stream, 16);
		shared_obj_num_bits = fz_read_bits(stream, 16);
		/* numerator_bits = */ (void) fz_read_bits(stream, 16);
		/* denominator_bits = */ (void) fz_read_bits(stream, 16);

		/* Item 1: Page object numbers */
		doc->hint_page[0].number = doc->linear_page1_obj_num;
		/* We don't care about the number of objects in the first page */
		(void)fz_read_bits(stream, page_obj_num_bits);
		j = 1;
		for (i = 1; i < doc->page_count; i++)
		{
			int delta_page_objs = fz_read_bits(stream, page_obj_num_bits);

			doc->hint_page[i].number = j;
			j += least_num_page_objs + delta_page_objs;
		}
		doc->hint_page[i].number = j; /* Not a real page object */
		fz_sync_bits(stream);
		/* Item 2: Page lengths */
		j = doc->hint_page[0].offset;
		for (i = 0; i < doc->page_count; i++)
		{
			int delta_page_len = fz_read_bits(stream, page_len_num_bits);
			int old = j;

			doc->hint_page[i].offset = j;
			j += least_page_len + delta_page_len;
			if (old <= doc->hint_object_offset && j > doc->hint_object_offset)
				j += doc->hint_object_length;
		}
		doc->hint_page[i].offset = j;
		fz_sync_bits(stream);
		/* Item 3: Shared references */
		shared = 0;
		for (i = 0; i < doc->page_count; i++)
		{
			int num_shared_objs = fz_read_bits(stream, num_shared_obj_num_bits);
			doc->hint_page[i].index = shared;
			shared += num_shared_objs;
		}
		doc->hint_page[i].index = shared;
		doc->hint_shared_ref = fz_resize_array(ctx, doc->hint_shared_ref, shared, sizeof(*doc->hint_shared_ref));
		memset(doc->hint_shared_ref, 0, sizeof(*doc->hint_shared_ref) * shared);
		fz_sync_bits(stream);
		/* Item 4: Shared references */
		for (i = 0; i < shared; i++)
		{
			int ref = fz_read_bits(stream, shared_obj_num_bits);
			doc->hint_shared_ref[i] = ref;
		}
		/* Skip items 5,6,7 as we don't use them */

		fz_seek(stream, shared_hint_offset, SEEK_SET);

		/* Read the shared object hints table: Header first */
		shared_obj_num = fz_read_bits(stream, 32);
		shared_obj_offset = fz_read_bits(stream, 32);
		if (shared_obj_offset > doc->hint_object_offset)
			shared_obj_offset += doc->hint_object_length;
		shared_obj_count_page1 = fz_read_bits(stream, 32);
		shared_obj_count_total = fz_read_bits(stream, 32);
		shared_obj_num_bits = fz_read_bits(stream, 16);
		least_shared_group_len = fz_read_bits(stream, 32);
		shared_group_len_num_bits = fz_read_bits(stream, 16);

		/* Sanity check the references in Item 4 above to ensure we
		 * don't access out of range with malicious files. */
		for (i = 0; i < shared; i++)
		{
			if (doc->hint_shared_ref[i] >= shared_obj_count_total)
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "malformed hint stream (shared refs)");
			}
		}

		doc->hint_shared = fz_resize_array(ctx, doc->hint_shared, shared_obj_count_total+1, sizeof(*doc->hint_shared));
		memset(doc->hint_shared, 0, sizeof(*doc->hint_shared) * (shared_obj_count_total+1));

		/* Item 1: Shared references */
		j = doc->hint_page[0].offset;
		for (i = 0; i < shared_obj_count_page1; i++)
		{
			int off = fz_read_bits(stream, shared_group_len_num_bits);
			int old = j;
			doc->hint_shared[i].offset = j;
			j += off + least_shared_group_len;
			if (old <= doc->hint_object_offset && j > doc->hint_object_offset)
				j += doc->hint_object_length;
		}
		/* FIXME: We would have problems recreating the length of the
		 * last page 1 shared reference group. But we'll never need
		 * to, so ignore it. */
		j = shared_obj_offset;
		for (; i < shared_obj_count_total; i++)
		{
			int off = fz_read_bits(stream, shared_group_len_num_bits);
			int old = j;
			doc->hint_shared[i].offset = j;
			j += off + least_shared_group_len;
			if (old <= doc->hint_object_offset && j > doc->hint_object_offset)
				j += doc->hint_object_length;
		}
		doc->hint_shared[i].offset = j;
		fz_sync_bits(stream);
		/* Item 2: Signature flags: read these just so we can skip */
		for (i = 0; i < shared_obj_count_total; i++)
		{
			doc->hint_shared[i].number = fz_read_bits(stream, 1);
		}
		fz_sync_bits(stream);
		/* Item 3: Signatures: just skip */
		for (i = 0; i < shared_obj_count_total; i++)
		{
			if (doc->hint_shared[i].number)
			{
				(void) fz_read_bits(stream, 128);
			}
		}
		fz_sync_bits(stream);
		/* Item 4: Shared object object numbers */
		j = doc->linear_page1_obj_num; /* FIXME: This is a lie! */
		for (i = 0; i < shared_obj_count_page1; i++)
		{
			doc->hint_shared[i].number = j;
			j += fz_read_bits(stream, shared_obj_num_bits) + 1;
		}
		j = shared_obj_num;
		for (; i < shared_obj_count_total; i++)
		{
			doc->hint_shared[i].number = j;
			j += fz_read_bits(stream, shared_obj_num_bits) + 1;
		}
		doc->hint_shared[i].number = j;

		/* Now, actually use the data we have gathered. */
		for (i = 0 /*shared_obj_count_page1*/; i < shared_obj_count_total; i++)
		{
			doc->hint_obj_offsets[doc->hint_shared[i].number] = doc->hint_shared[i].offset;
		}
		for (i = 0; i < doc->page_count; i++)
		{
			doc->hint_obj_offsets[doc->hint_page[i].number] = doc->hint_page[i].offset;
		}
	}
	fz_always(ctx)
	{
		fz_close(stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* Don't try to load hints again */
		doc->hints_loaded = 1;
		/* We won't use the linearized object any more. */
		doc->file_reading_linearly = 0;
		/* Any other error becomes a TRYLATER */
		fz_throw(ctx, FZ_ERROR_TRYLATER, "malformed hints object");
	}
	doc->hints_loaded = 1;
}

static void
pdf_load_hint_object(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;
	pdf_lexbuf *buf = &doc->lexbuf.base;
	int curr_pos;

	curr_pos = fz_tell(doc->file);
	fz_seek(doc->file, doc->hint_object_offset, SEEK_SET);
	fz_try(ctx)
	{
		while (1)
		{
			pdf_obj *page = NULL;
			int tmpofs, num, gen, tok;

			tok = pdf_lex(doc->file, buf);
			if (tok != PDF_TOK_INT)
				break;
			num = buf->i;
			tok = pdf_lex(doc->file, buf);
			if (tok != PDF_TOK_INT)
				break;
			gen = buf->i;
			tok = pdf_lex(doc->file, buf);
			if (tok != PDF_TOK_OBJ)
				break;
			(void)pdf_repair_obj(doc, buf, &tmpofs, NULL, NULL, NULL, &page, &tmpofs);
			pdf_load_hints(doc, num, gen);
		}
	}
	fz_always(ctx)
	{
		fz_seek(doc->file, curr_pos, SEEK_SET);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

pdf_obj *pdf_progressive_advance(pdf_document *doc, int pagenum)
{
	fz_context *ctx = doc->ctx;
	pdf_lexbuf *buf = &doc->lexbuf.base;
	int curr_pos;
	pdf_obj *page;

	pdf_load_hinted_page(doc, pagenum);

	if (pagenum < 0 || pagenum >= doc->page_count)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "page load out of range (%d of %d)", pagenum, doc->page_count);

	if (doc->linear_pos == doc->file_length)
		return doc->linear_page_refs[pagenum];

	/* Only load hints once, and then only after we have got page 0 */
	if (pagenum > 0 && !doc->hints_loaded && doc->hint_object_offset > 0 && doc->linear_pos >= doc->hint_object_offset)
	{
		/* Found hint object */
		pdf_load_hint_object(doc);
	}

	DEBUGMESS((ctx, "continuing to try to advance from %d", doc->linear_pos));
	curr_pos = fz_tell(doc->file);

	fz_var(page);

	fz_try(ctx)
	{
		int eof;
		do
		{
			int num;
			page = NULL;
			eof = pdf_obj_read(doc, &doc->linear_pos, &num, &page);
			pdf_drop_obj(page);
			page = NULL;
		}
		while (!eof);

		{
			pdf_obj *catalog;
			pdf_obj *pages;
			doc->linear_pos = doc->file_length;
			pdf_load_xref(doc, buf);
			catalog = pdf_dict_gets(pdf_trailer(doc), "Root");
			pages = pdf_dict_gets(catalog, "Pages");

			if (!pdf_is_dict(pages))
				fz_throw(ctx, FZ_ERROR_GENERIC, "missing page tree");
			break;
		}
	}
	fz_always(ctx)
	{
		fz_seek(doc->file, curr_pos, SEEK_SET);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(page);
		if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
		{
			if (doc->linear_page_refs[pagenum] == NULL)
			{
				/* Still not got a page */
				fz_rethrow(ctx);
			}
		}
		else
			fz_rethrow(ctx);
	}

	return doc->linear_page_refs[pagenum];
}

pdf_document *pdf_specifics(fz_document *doc)
{
	return (pdf_document *)((doc && doc->close == (fz_document_close_fn *)pdf_close_document) ? doc : NULL);
}

pdf_document *pdf_create_document(fz_context *ctx)
{
	pdf_document *doc;
	pdf_obj *o = NULL;
	pdf_obj *root;
	pdf_obj *pages;
	pdf_obj *trailer = NULL;

	fz_var(o);
	fz_var(trailer);

	doc = pdf_new_document(ctx, NULL);
	fz_try(ctx)
	{
		doc->version = 14;
		doc->file_size = 0;
		doc->startxref = 0;
		doc->num_xref_sections = 0;
		pdf_get_populating_xref_entry(doc, 0);
		doc->xref_altered = 1;
		trailer = pdf_new_dict(doc, 2);
		pdf_dict_puts_drop(trailer, "Size", pdf_new_int(doc, 3));
		o = root = pdf_new_dict(doc, 2);
		pdf_dict_puts_drop(trailer, "Root", pdf_new_ref(doc, o));
		pdf_drop_obj(o);
		o = NULL;
		pdf_dict_puts_drop(root, "Type", pdf_new_name(doc, "Catalog"));
		o = pages = pdf_new_dict(doc, 3);
		pdf_dict_puts_drop(root, "Pages", pdf_new_ref(doc, o));
		pdf_drop_obj(o);
		o = NULL;
		pdf_dict_puts_drop(pages, "Type", pdf_new_name(doc, "Pages"));
		pdf_dict_puts_drop(pages, "Count", pdf_new_int(doc, 0));
		pdf_dict_puts_drop(pages, "Kids", pdf_new_array(doc, 1));
		pdf_set_populating_xref_trailer(doc, trailer);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(trailer);
		pdf_drop_obj(o);
		fz_rethrow_message(ctx, "Failed to create empty document");
	}
	return doc;
}

int
pdf_recognize(fz_context *doc, const char *magic)
{
	char *ext = strrchr(magic, '.');

	if (ext)
	{
		if (!fz_strcasecmp(ext, ".pdf"))
			return 100;
	}
	if (!strcmp(magic, "pdf") || !strcmp(magic, "application/pdf"))
		return 100;

	return 1;
}

fz_document_handler pdf_no_run_document_handler =
{
	(fz_document_recognize_fn *)&pdf_recognize,
	(fz_document_open_fn *)&pdf_open_document_no_run,
	(fz_document_open_with_stream_fn *)&pdf_open_document_no_run_with_stream
};

void pdf_mark_xref(pdf_document *doc)
{
	int x, e;

	for (x = 0; x < doc->num_xref_sections; x++)
	{
		pdf_xref *xref = &doc->xref_sections[x];

		for (e = 0; e < xref->len; e++)
		{
			pdf_xref_entry *entry = &xref->table[e];

			if (entry->obj)
			{
				entry->flags |= PDF_OBJ_FLAG_MARK;
			}
		}
	}
}

void pdf_clear_xref(pdf_document *doc)
{
	int x, e;

	for (x = 0; x < doc->num_xref_sections; x++)
	{
		pdf_xref *xref = &doc->xref_sections[x];

		for (e = 0; e < xref->len; e++)
		{
			pdf_xref_entry *entry = &xref->table[e];

			/* We cannot drop objects if the stream buffer has
			 * been updated */
			if (entry->obj != NULL && entry->stm_buf == NULL)
			{
				if (pdf_obj_refs(entry->obj) == 1)
				{
					pdf_drop_obj(entry->obj);
					entry->obj = NULL;
				}
			}
		}
	}
}

void pdf_clear_xref_to_mark(pdf_document *doc)
{
	int x, e;

	for (x = 0; x < doc->num_xref_sections; x++)
	{
		pdf_xref *xref = &doc->xref_sections[x];

		for (e = 0; e < xref->len; e++)
		{
			pdf_xref_entry *entry = &xref->table[e];

			/* We cannot drop objects if the stream buffer has
			 * been updated */
			if (entry->obj != NULL && entry->stm_buf == NULL)
			{
				if ((entry->flags & PDF_OBJ_FLAG_MARK) == 0 && pdf_obj_refs(entry->obj) == 1)
				{
					pdf_drop_obj(entry->obj);
					entry->obj = NULL;
				}
			}
		}
	}
}

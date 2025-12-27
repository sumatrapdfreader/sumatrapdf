// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PDF_MAKE_NAME(STRING,NAME) STRING,
static const char *PDF_NAME_LIST[] = {
	"", "", "", /* dummy slots for null, true, and false */
#include "mupdf/pdf/name-table.h"
};
#undef PDF_MAKE_NAME

typedef enum pdf_objkind_e
{
	PDF_INT = 'i',
	PDF_REAL = 'f',
	PDF_STRING = 's',
	PDF_NAME = 'n',
	PDF_ARRAY = 'a',
	PDF_DICT = 'd',
	PDF_INDIRECT = 'r'
} pdf_objkind;

struct keyval
{
	pdf_obj *k;
	pdf_obj *v;
};

enum
{
	PDF_FLAGS_MARKED = 1,
	PDF_FLAGS_SORTED = 2,
	PDF_FLAGS_DIRTY = 4,
	PDF_FLAGS_MEMO_BASE = 8,
	PDF_FLAGS_MEMO_BASE_BOOL = 16
};

struct pdf_obj
{
	short refs;
	unsigned char kind;
	unsigned char flags;
};

typedef struct
{
	pdf_obj super;
	union
	{
		int64_t i;
		float f;
	} u;
} pdf_obj_num;

typedef struct
{
	pdf_obj super;
	char *text; /* utf8 encoded text string */
	size_t len;
	char buf[FZ_FLEXIBLE_ARRAY];
} pdf_obj_string;

typedef struct
{
	pdf_obj super;
	char n[FZ_FLEXIBLE_ARRAY];
} pdf_obj_name;

typedef struct
{
	pdf_obj super;
	pdf_document *doc;
	int parent_num;
	int len;
	int cap;
	pdf_obj **items;
} pdf_obj_array;

typedef struct
{
	pdf_obj super;
	pdf_document *doc;
	int parent_num;
	int len;
	int cap;
	struct keyval *items;
} pdf_obj_dict;

typedef struct
{
	pdf_obj super;
	pdf_document *doc; /* Only needed for arrays, dicts and indirects */
	int num;
	int gen;
} pdf_obj_ref;

/* Each journal fragment represents a change to a PDF xref object. */
typedef struct pdf_journal_fragment
{
	struct pdf_journal_fragment *next;
	struct pdf_journal_fragment *prev;

	int obj_num;
	int newobj;
	pdf_obj *inactive;
	fz_buffer *stream;
} pdf_journal_fragment;

/* A journal entry represents a single notional 'change' to the
 * document, such as 'signing it' or 'filling in a field'. Each such
 * change consists of 1 or more 'fragments'. */
typedef struct pdf_journal_entry
{
	struct pdf_journal_entry *prev;
	struct pdf_journal_entry *next;

	char *title;
#ifdef PDF_DEBUG_JOURNAL
	int changed_since_last_dumped;
#endif
	pdf_journal_fragment *head;
	pdf_journal_fragment *tail;
} pdf_journal_entry;

/* A journal consists of a list of journal entries, rooted at head.
 * current is either NULL, or points to somewhere in the list. Anything
 * between head and current inclusive represents a journalled change
 * that is currently in force. Anything after current represents a
 * journalled change that has been 'undone'. If current is NULL, then
 * ALL changes in the list have been undone. */
struct pdf_journal
{
	pdf_journal_entry *head;
	pdf_journal_entry *current;
	int nesting;
	pdf_journal_entry *pending;
	pdf_journal_entry *pending_tail;
};

#define NAME(obj) ((pdf_obj_name *)(obj))
#define NUM(obj) ((pdf_obj_num *)(obj))
#define STRING(obj) ((pdf_obj_string *)(obj))
#define DICT(obj) ((pdf_obj_dict *)(obj))
#define ARRAY(obj) ((pdf_obj_array *)(obj))
#define REF(obj) ((pdf_obj_ref *)(obj))

pdf_obj *
pdf_new_int(fz_context *ctx, int64_t i)
{
	pdf_obj_num *obj;
	obj = Memento_label(fz_malloc(ctx, sizeof(pdf_obj_num)), "pdf_obj(int)");
	obj->super.refs = 1;
	obj->super.kind = PDF_INT;
	obj->super.flags = 0;
	obj->u.i = i;
	return &obj->super;
}

pdf_obj *
pdf_new_real(fz_context *ctx, float f)
{
	pdf_obj_num *obj;
	obj = Memento_label(fz_malloc(ctx, sizeof(pdf_obj_num)), "pdf_obj(real)");
	obj->super.refs = 1;
	obj->super.kind = PDF_REAL;
	obj->super.flags = 0;
	obj->u.f = f;
	return &obj->super;
}

pdf_obj *
pdf_new_string(fz_context *ctx, const char *str, size_t len)
{
	pdf_obj_string *obj;
	unsigned int l = (unsigned int)len;

	if ((size_t)l != len)
		fz_throw(ctx, FZ_ERROR_LIMIT, "Overflow in pdf string");

	obj = Memento_label(fz_malloc(ctx, offsetof(pdf_obj_string, buf) + len + 1), "pdf_obj(string)");
	obj->super.refs = 1;
	obj->super.kind = PDF_STRING;
	obj->super.flags = 0;
	obj->text = NULL;
	obj->len = l;
	memcpy(obj->buf, str, len);
	obj->buf[len] = '\0';
	return &obj->super;
}

pdf_obj *
pdf_new_name(fz_context *ctx, const char *str)
{
	pdf_obj_name *obj;
	int l = 3; /* skip dummy slots */
	int r = nelem(PDF_NAME_LIST) - 1;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = strcmp(str, PDF_NAME_LIST[m]);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return (pdf_obj*)(intptr_t)m;
	}

	obj = Memento_label(fz_malloc(ctx, offsetof(pdf_obj_name, n) + strlen(str) + 1), "pdf_obj(name)");
	obj->super.refs = 1;
	obj->super.kind = PDF_NAME;
	obj->super.flags = 0;
	strcpy(obj->n, str);
	return &obj->super;
}

pdf_obj *
pdf_new_indirect(fz_context *ctx, pdf_document *doc, int num, int gen)
{
	pdf_obj_ref *obj;
	if (num < 0 || num > PDF_MAX_OBJECT_NUMBER)
	{
		fz_warn(ctx, "invalid object number (%d)", num);
		return PDF_NULL;
	}
	if (gen < 0 || gen > PDF_MAX_GEN_NUMBER)
	{
		fz_warn(ctx, "invalid generation number (%d)", gen);
		return PDF_NULL;
	}
	obj = Memento_label(fz_malloc(ctx, sizeof(pdf_obj_ref)), "pdf_obj(indirect)");
	obj->super.refs = 1;
	obj->super.kind = PDF_INDIRECT;
	obj->super.flags = 0;
	obj->doc = doc;
	obj->num = num;
	obj->gen = gen;
	return &obj->super;
}

#define OBJ_IS_NULL(obj) (obj == PDF_NULL)
#define OBJ_IS_BOOL(obj) (obj == PDF_TRUE || obj == PDF_FALSE)
#define OBJ_IS_NAME(obj) ((obj > PDF_FALSE && obj < PDF_LIMIT) || (obj >= PDF_LIMIT && obj->kind == PDF_NAME))
#define OBJ_IS_INT(obj) \
	(obj >= PDF_LIMIT && obj->kind == PDF_INT)
#define OBJ_IS_REAL(obj) \
	(obj >= PDF_LIMIT && obj->kind == PDF_REAL)
#define OBJ_IS_NUMBER(obj) \
	(obj >= PDF_LIMIT && (obj->kind == PDF_REAL || obj->kind == PDF_INT))
#define OBJ_IS_STRING(obj) \
	(obj >= PDF_LIMIT && obj->kind == PDF_STRING)
#define OBJ_IS_ARRAY(obj) \
	(obj >= PDF_LIMIT && obj->kind == PDF_ARRAY)
#define OBJ_IS_DICT(obj) \
	(obj >= PDF_LIMIT && obj->kind == PDF_DICT)
#define OBJ_IS_INDIRECT(obj) \
	(obj >= PDF_LIMIT && obj->kind == PDF_INDIRECT)

#define RESOLVE(obj) \
	if (OBJ_IS_INDIRECT(obj)) \
		obj = pdf_resolve_indirect_chain(ctx, obj); \

int pdf_is_indirect(fz_context *ctx, pdf_obj *obj)
{
	return OBJ_IS_INDIRECT(obj);
}

int pdf_is_null(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_NULL(obj);
}

int pdf_is_bool(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_BOOL(obj);
}

int pdf_is_int(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_INT(obj);
}

int pdf_is_real(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_REAL(obj);
}

int pdf_is_number(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_NUMBER(obj);
}

int pdf_is_string(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_STRING(obj);
}

int pdf_is_name(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_NAME(obj);
}

int pdf_is_array(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_ARRAY(obj);
}

int pdf_is_dict(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return OBJ_IS_DICT(obj);
}

/* safe, silent failure, no error reporting on type mismatches */
int pdf_to_bool(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	return obj == PDF_TRUE;
}

int pdf_to_bool_default(fz_context *ctx, pdf_obj *obj, int def)
{
	RESOLVE(obj);
	return obj == PDF_TRUE ? 1 : obj == PDF_FALSE ? 0 : def;
}

int pdf_to_int(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return 0;
	if (obj->kind == PDF_INT)
		return (int)NUM(obj)->u.i;
	if (obj->kind == PDF_REAL)
		return (int)floorf(NUM(obj)->u.f + 0.5);
	return 0;
}

int pdf_to_int_default(fz_context *ctx, pdf_obj *obj, int def)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return def;
	if (obj->kind == PDF_INT)
		return (int)NUM(obj)->u.i;
	if (obj->kind == PDF_REAL)
		return (int)floorf(NUM(obj)->u.f + 0.5);
	return def;
}

int64_t pdf_to_int64(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return 0;
	if (obj->kind == PDF_INT)
		return NUM(obj)->u.i;
	if (obj->kind == PDF_REAL)
		return (int64_t)floorf(NUM(obj)->u.f + 0.5);
	return 0;
}

float pdf_to_real(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return 0;
	if (obj->kind == PDF_REAL)
		return NUM(obj)->u.f;
	if (obj->kind == PDF_INT)
		return NUM(obj)->u.i;
	return 0;
}

float pdf_to_real_default(fz_context *ctx, pdf_obj *obj, float def)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return def;
	if (obj->kind == PDF_REAL)
		return NUM(obj)->u.f;
	if (obj->kind == PDF_INT)
		return NUM(obj)->u.i;
	return def;
}

const char *pdf_to_name(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return PDF_NAME_LIST[((intptr_t)obj)];
	if (obj->kind == PDF_NAME)
		return NAME(obj)->n;
	return "";
}

char *pdf_to_str_buf(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (OBJ_IS_STRING(obj))
		return STRING(obj)->buf;
	return "";
}

size_t pdf_to_str_len(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (OBJ_IS_STRING(obj))
		return STRING(obj)->len;
	return 0;
}

const char *pdf_to_string(fz_context *ctx, pdf_obj *obj, size_t *sizep)
{
	RESOLVE(obj);
	if (OBJ_IS_STRING(obj))
	{
		if (sizep)
			*sizep = STRING(obj)->len;
		return STRING(obj)->buf;
	}
	if (sizep)
		*sizep = 0;
	return "";
}

const char *pdf_to_text_string(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (OBJ_IS_STRING(obj))
	{
		if (!STRING(obj)->text)
			STRING(obj)->text = pdf_new_utf8_from_pdf_string(ctx, STRING(obj)->buf, STRING(obj)->len);
		return STRING(obj)->text;
	}
	return "";
}

void pdf_set_int(fz_context *ctx, pdf_obj *obj, int64_t i)
{
	if (OBJ_IS_INT(obj))
		NUM(obj)->u.i = i;
}

void pdf_set_str_len(fz_context *ctx, pdf_obj *obj, size_t newlen)
{
	RESOLVE(obj);
	if (!OBJ_IS_STRING(obj))
		return; /* This should never happen */
	if (newlen > STRING(obj)->len)
		return; /* This should never happen */
	STRING(obj)->buf[newlen] = 0;
	STRING(obj)->len = newlen;
}

int pdf_to_num(fz_context *ctx, pdf_obj *obj)
{
	if (OBJ_IS_INDIRECT(obj))
		return REF(obj)->num;
	return 0;
}

int pdf_to_gen(fz_context *ctx, pdf_obj *obj)
{
	if (OBJ_IS_INDIRECT(obj))
		return REF(obj)->gen;
	return 0;
}

/*
	DEPRECATED: Do not use in new code.
*/
pdf_document *pdf_get_indirect_document(fz_context *ctx, pdf_obj *obj)
{
	if (OBJ_IS_INDIRECT(obj))
		return REF(obj)->doc;
	return NULL;
}

/*
	DEPRECATED: Do not use in new code.
*/
pdf_document *pdf_get_bound_document(fz_context *ctx, pdf_obj *obj)
{
	if (obj < PDF_LIMIT)
		return NULL;
	if (obj->kind == PDF_INDIRECT)
		return REF(obj)->doc;
	if (obj->kind == PDF_ARRAY)
		return ARRAY(obj)->doc;
	if (obj->kind == PDF_DICT)
		return DICT(obj)->doc;
	return NULL;
}

/*
	This implementation will do to provide the required
	API change in advance of the rewrite to use weak references
	in the next version.
*/
pdf_document *pdf_pin_document(fz_context *ctx, pdf_obj *obj)
{
	return pdf_keep_document(ctx, pdf_get_bound_document(ctx, obj));
}

int pdf_objcmp_resolve(fz_context *ctx, pdf_obj *a, pdf_obj *b)
{
	RESOLVE(a);
	RESOLVE(b);
	return pdf_objcmp(ctx, a, b);
}

static int
do_objcmp(fz_context *ctx, pdf_obj *a, pdf_obj *b, int check_streams)
{
	int i, j;

	if (a == b)
		return 0;

	/* a or b is null, true, or false */
	if (a <= PDF_FALSE || b <= PDF_FALSE)
		return 1;

	/* a is a constant name */
	if (a < PDF_LIMIT)
	{
		if (b < PDF_LIMIT)
			return a != b;
		if (b->kind != PDF_NAME)
			return 1;
		return strcmp(PDF_NAME_LIST[(intptr_t)a], NAME(b)->n);
	}

	/* b is a constant name */
	if (b < PDF_LIMIT)
	{
		if (a->kind != PDF_NAME)
			return 1;
		return strcmp(NAME(a)->n, PDF_NAME_LIST[(intptr_t)b]);
	}

	/* both a and b are allocated objects */
	if (a->kind != b->kind)
		return 1;

	switch (a->kind)
	{
	case PDF_INT:
		return NUM(a)->u.i - NUM(b)->u.i;

	case PDF_REAL:
		if (NUM(a)->u.f < NUM(b)->u.f)
			return -1;
		if (NUM(a)->u.f > NUM(b)->u.f)
			return 1;
		return 0;

	case PDF_STRING:
		if (STRING(a)->len < STRING(b)->len)
		{
			if (memcmp(STRING(a)->buf, STRING(b)->buf, STRING(a)->len) <= 0)
				return -1;
			return 1;
		}
		if (STRING(a)->len > STRING(b)->len)
		{
			if (memcmp(STRING(a)->buf, STRING(b)->buf, STRING(b)->len) >= 0)
				return 1;
			return -1;
		}
		return memcmp(STRING(a)->buf, STRING(b)->buf, STRING(a)->len);

	case PDF_NAME:
		return strcmp(NAME(a)->n, NAME(b)->n);

	case PDF_INDIRECT:
		if (REF(a)->num == REF(b)->num)
			return REF(a)->gen - REF(b)->gen;
		return REF(a)->num - REF(b)->num;

	case PDF_ARRAY:
		if (ARRAY(a)->len != ARRAY(b)->len)
			return ARRAY(a)->len - ARRAY(b)->len;
		for (i = 0; i < ARRAY(a)->len; i++)
			if (pdf_objcmp(ctx, ARRAY(a)->items[i], ARRAY(b)->items[i]))
				return 1;
		return 0;

	case PDF_DICT:
		if (DICT(a)->len != DICT(b)->len)
			return DICT(a)->len - DICT(b)->len;
		if ((a->flags & b->flags) & PDF_FLAGS_SORTED)
		{
			/* Both a and b are sorted. Easy. */
			for (i = 0; i < DICT(a)->len; i++)
			{
				if (pdf_objcmp(ctx, DICT(a)->items[i].k, DICT(b)->items[i].k))
					return 1;
				if (pdf_objcmp(ctx, DICT(a)->items[i].v, DICT(b)->items[i].v))
					return 1;
			}
		}
		else
		{
			/* Either a or b is not sorted. We need to work harder. */
			int len = DICT(a)->len;
			for (i = 0; i < len; i++)
			{
				pdf_obj *key = DICT(a)->items[i].k;
				pdf_obj *val = DICT(a)->items[i].v;
				for (j = 0; j < len; j++)
				{
					if (pdf_objcmp(ctx, key, DICT(b)->items[j].k) == 0 &&
						pdf_objcmp(ctx, val, DICT(b)->items[j].v) == 0)
						break; /* Match */
				}
				if (j == len)
					return 1;
			}
		}
		/* Dicts are identical, but if they are streams, we can only be sure
		 * they are identical if the stream contents match. If '!check_streams',
		 * then don't test for identical stream contents - only match if a == b.
		 * Otherwise, do the full, painful, comparison. */
		{
			/* Slightly convoluted to know if something is a stream. */
			pdf_document *doca = DICT(a)->doc;
			pdf_document *docb = DICT(b)->doc;
			int ap = pdf_obj_parent_num(ctx, a);
			int bp;
			int a_is_stream = 0;
			pdf_xref_entry *entrya = pdf_get_xref_entry_no_change(ctx, doca, ap);
			pdf_xref_entry *entryb;
			if (entrya != NULL && entrya->obj == a && pdf_obj_num_is_stream(ctx, doca, ap))
			{
				/* It's a stream, and we know a != b from above. */
				if (!check_streams)
					return 1; /* mismatch */
				a_is_stream = 1;
			}
			bp = pdf_obj_parent_num(ctx, b);
			entryb = pdf_get_xref_entry_no_change(ctx, docb, bp);
			if (entryb != NULL && entryb->obj == b && pdf_obj_num_is_stream(ctx, docb, bp))
			{
				/* It's a stream, and we know a != b from above. So mismatch. */
				if (!check_streams || !a_is_stream)
					return 1; /* mismatch */
			}
			else
			{
				/* b is not a stream. We match, iff a is not a stream. */
				return a_is_stream;
			}
			/* So, if we get here, we know check_streams is true, and that both
			 * a and b are streams. */
			{
				fz_buffer *sa = NULL;
				fz_buffer *sb = NULL;
				int differ = 1;

				fz_var(sa);
				fz_var(sb);

				fz_try(ctx)
				{
					unsigned char *dataa, *datab;
					size_t lena, lenb;
					sa = pdf_load_raw_stream_number(ctx, doca, ap);
					sb = pdf_load_raw_stream_number(ctx, docb, bp);
					lena = fz_buffer_storage(ctx, sa, &dataa);
					lenb = fz_buffer_storage(ctx, sb, &datab);
					if (lena == lenb && memcmp(dataa, datab, lena) == 0)
						differ = 0;
				}
				fz_always(ctx)
				{
					fz_drop_buffer(ctx, sa);
					fz_drop_buffer(ctx, sb);
				}
				fz_catch(ctx)
				{
					fz_rethrow(ctx);
				}
				return differ;
			}
		}
	}
	return 1;
}

int
pdf_objcmp(fz_context *ctx, pdf_obj *a, pdf_obj *b)
{
	return do_objcmp(ctx, a, b, 0);
}

int
pdf_objcmp_deep(fz_context *ctx, pdf_obj *a, pdf_obj *b)
{
	return do_objcmp(ctx, a, b, 1);
}

int pdf_name_eq(fz_context *ctx, pdf_obj *a, pdf_obj *b)
{
	RESOLVE(a);
	RESOLVE(b);
	if (a <= PDF_FALSE || b <= PDF_FALSE)
		return 0;
	if (a < PDF_LIMIT || b < PDF_LIMIT)
		return (a == b);
	if (a->kind == PDF_NAME && b->kind == PDF_NAME)
		return !strcmp(NAME(a)->n, NAME(b)->n);
	return 0;
}

static char *
pdf_objkindstr(pdf_obj *obj)
{
	if (obj == PDF_NULL)
		return "null";
	if (obj == PDF_TRUE || obj == PDF_FALSE)
		return "boolean";
	if (obj < PDF_LIMIT)
		return "name";
	switch (obj->kind)
	{
	case PDF_INT: return "integer";
	case PDF_REAL: return "real";
	case PDF_STRING: return "string";
	case PDF_NAME: return "name";
	case PDF_ARRAY: return "array";
	case PDF_DICT: return "dictionary";
	case PDF_INDIRECT: return "reference";
	}
	return "<unknown>";
}

pdf_obj *
pdf_new_array(fz_context *ctx, pdf_document *doc, int initialcap)
{
	pdf_obj_array *obj;
	int i;

	if (doc == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot create array without a document");

	obj = Memento_label(fz_malloc(ctx, sizeof(pdf_obj_array)), "pdf_obj(array)");
	obj->super.refs = 1;
	obj->super.kind = PDF_ARRAY;
	obj->super.flags = 0;
	obj->doc = doc;
	obj->parent_num = 0;

	obj->len = 0;
	obj->cap = initialcap > 1 ? initialcap : 6;

	fz_try(ctx)
	{
		obj->items = Memento_label(fz_malloc_array(ctx, obj->cap, pdf_obj*), "pdf_array_items");
	}
	fz_catch(ctx)
	{
		fz_free(ctx, obj);
		fz_rethrow(ctx);
	}
	for (i = 0; i < obj->cap; i++)
		obj->items[i] = NULL;

	return &obj->super;
}

static void
pdf_array_grow(fz_context *ctx, pdf_obj_array *obj)
{
	int i;
	int new_cap = (obj->cap * 3) / 2;

	obj->items = fz_realloc_array(ctx, obj->items, new_cap, pdf_obj*);
	obj->cap = new_cap;

	for (i = obj->len ; i < obj->cap; i++)
		obj->items[i] = NULL;
}

pdf_obj *
pdf_copy_array(fz_context *ctx, pdf_obj *obj)
{
	pdf_document *doc;
	pdf_obj *arr;
	int i;
	int n;

	RESOLVE(obj);
	if (!OBJ_IS_ARRAY(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an array (%s)", pdf_objkindstr(obj));

	doc = ARRAY(obj)->doc;

	n = pdf_array_len(ctx, obj);
	arr = pdf_new_array(ctx, doc, n);
	fz_try(ctx)
		for (i = 0; i < n; i++)
			pdf_array_push(ctx, arr, pdf_array_get(ctx, obj, i));
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, arr);
		fz_rethrow(ctx);
	}

	return arr;
}

int
pdf_array_len(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (!OBJ_IS_ARRAY(obj))
		return 0;
	return ARRAY(obj)->len;
}

pdf_obj *
pdf_array_get(fz_context *ctx, pdf_obj *obj, int i)
{
	RESOLVE(obj);
	if (!OBJ_IS_ARRAY(obj))
		return NULL;
	if (i < 0 || i >= ARRAY(obj)->len)
		return NULL;
	return ARRAY(obj)->items[i];
}

/* Call this to enable journalling on a given document. */
void pdf_enable_journal(fz_context *ctx, pdf_document *doc)
{
	if (ctx == NULL || doc == NULL)
		return;

	if (doc->journal == NULL)
		doc->journal = fz_malloc_struct(ctx, pdf_journal);
}

static void
discard_fragments(fz_context *ctx, pdf_journal_fragment *head)
{
	while (head)
	{
		pdf_journal_fragment *next = head->next;

		pdf_drop_obj(ctx, head->inactive);
		fz_drop_buffer(ctx, head->stream);
		fz_free(ctx, head);
		head = next;
	}
}

static void
discard_journal_entries(fz_context *ctx, pdf_journal_entry **journal_entry)
{
	pdf_journal_entry *entry = *journal_entry;

	if (entry == NULL)
		return;

	*journal_entry = NULL;
	while (entry)
	{
		pdf_journal_entry *next = entry->next;

		discard_fragments(ctx, entry->head);
		fz_free(ctx, entry->title);
		fz_free(ctx, entry);
		entry = next;
	}
}

static void
new_entry(fz_context *ctx, pdf_document *doc, char *operation)
{
	fz_try(ctx)
	{
		pdf_journal_entry *entry;

		/* We create a new entry, and link it into the middle of
		 * the chain. If we actually come to put anything into
		 * it later, then the call to pdf_add_journal_fragment
		 * during that addition will discard everything in the
		 * history that follows it. */
		entry = fz_malloc_struct(ctx, pdf_journal_entry);

		if (doc->journal->current == NULL)
		{
			entry->prev = NULL;
			entry->next = doc->journal->head;
			doc->journal->head = entry;
		}
		else
		{
			entry->prev = doc->journal->current;
			entry->next = doc->journal->current->next;
			if (doc->journal->current->next)
				doc->journal->current->next->prev = entry;
			doc->journal->current->next = entry;
		}
		doc->journal->current = entry;
		entry->title = operation;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, operation);
		fz_rethrow(ctx);
	}
}

/* Call this to start an operation. Undo/redo works at 'operation'
 * granularity. Nested operations are all counted within the outermost
 * operation. Any modification performed on a journalled PDF without an
 * operation having been started will throw an error. */
static void
do_begin_operation(fz_context *ctx, pdf_document *doc, const char *operation_)
{
	char *operation;

	/* If we aren't journalling this doc, just give up now. */
	if (ctx == NULL || doc == NULL || doc->journal == NULL)
		return;

	/* Always increment nesting. */
	doc->journal->nesting++;

	operation = operation_ ? fz_strdup(ctx, operation_) : NULL;

#ifdef PDF_DEBUG_JOURNAL
	fz_write_printf(ctx, fz_stddbg(ctx), "Beginning: (->%d) %s\n", doc->journal->nesting, operation ? operation : "<implicit>");
#endif

	fz_try(ctx)
	{
		pdf_journal_entry *entry;

		/* We create a new entry, and link it into the middle of
		 * the chain. If we actually come to put anything into
		 * it later, then the call to pdf_add_journal_fragment
		 * during that addition will discard everything in the
		 * history that follows it. */
		entry = fz_malloc_struct(ctx, pdf_journal_entry);

		if (doc->journal->pending_tail == NULL)
		{
			entry->prev = NULL;
			entry->next = doc->journal->pending;
			doc->journal->pending = entry;
		}
		else
		{
			entry->prev = doc->journal->pending_tail;
			entry->next = doc->journal->pending_tail->next;
			if (doc->journal->pending_tail->next)
				doc->journal->pending_tail->next->prev = entry;
			doc->journal->pending_tail->next = entry;
		}
		doc->journal->pending_tail = entry;
		entry->title = operation;
	}
	fz_catch(ctx)
	{
		doc->journal->nesting--;
		fz_free(ctx, operation);
		fz_rethrow(ctx);
	}
}

void pdf_begin_operation(fz_context *ctx, pdf_document *doc, const char *operation)
{
	if (operation == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "All operations must be named");

	do_begin_operation(ctx, doc, operation);
}

void pdf_begin_implicit_operation(fz_context *ctx, pdf_document *doc)
{
	do_begin_operation(ctx, doc, NULL);
}

void pdf_drop_journal(fz_context *ctx, pdf_journal *journal)
{
	if (ctx == NULL || journal == NULL)
		return;

	discard_journal_entries(ctx, &journal->head);
	/* Shouldn't be any pending ones, but be safe. */
	discard_journal_entries(ctx, &journal->pending);

	fz_free(ctx, journal);
}

#ifdef PDF_DEBUG_JOURNAL
static void
dump_changes(fz_context *ctx, pdf_document *doc, pdf_journal_entry *entry)
{
	pdf_journal_fragment *frag;

	if (entry == NULL || entry->changed_since_last_dumped == 0)
		return;

	for (frag = entry->head; frag; frag = frag->next)
	{
		pdf_obj *obj;
		fz_write_printf(ctx, fz_stddbg(ctx), "Changing obj %d:\n", frag->obj_num);
		pdf_debug_obj(ctx, frag->inactive);
		fz_write_printf(ctx, fz_stddbg(ctx), " To:\n");
		obj = pdf_load_object(ctx, doc, frag->obj_num);
		pdf_debug_obj(ctx, obj);
		pdf_drop_obj(ctx, obj);
	}

	entry->changed_since_last_dumped = 0;
}
#endif

/* We build up journal entries as being a list of changes (fragments) that
 * happen all together as part of a single step. When we reach pdf_end_operation
 * we have all the changes that have happened during this operation in a list
 * that basically boils down to being:
 *
 *     change object x from being A to the value in the xref.
 *     change object y from being B to the value in the xref.
 *     change object z from being C to the value in the xref.
 *     etc.
 *
 * The idea is that we can undo, or redo by stepping through that list.
 * Every object can only be mentioned once in a fragment (otherwise we
 * get very confused when undoing and redoing).
 *
 * When we come to glue 2 entries together (as happens when we end a
 * nested or implicit operation), we need to be sure that the 2 entries
 * don't both mention the same object.
 *
 * Imagine we've edited a text field from being empty to containing
 * 'he' by typing each char at a time:
 *
 *     Entry 1:
 *        change object x from being ''.
 *     Entry 2 (implicit):
 *        change object x from being 'h'.
 *
 * with current xref entry for x being 'he'.
 *
 * When we come to combine the two, we can't simply go to:
 *
 *     change object x from being ''.
 *     change object x from being 'h'.
 *
 * If we 'undo' that, however, because we run forwards through the list for
 * both undo and redo, we get it wrong.
 *
 * First we replace 'he' by ''.
 * Then we replace '' by 'h'.
 *
 * i.e. leaving us only partly undone.
 *
 * Either we need to run in different directions for undo and redo, or we need to
 * resolve the changes down to a single change for each object. Given that we don't
 * really want more than one change for each object in each changeset (needless memory
 * etc), let's resolve the changesets.
 **/
static void resolve_undo(fz_context *ctx, pdf_journal_entry *entry)
{
	pdf_journal_fragment *start, *current;
	pdf_journal_fragment *tail = NULL;

	/* Slightly nasty that this is n^2, but any alternative involves
	 * sorting. Shouldn't be huge lists anyway. */
	for (start = entry->head; start; start = start->next)
	{
		pdf_journal_fragment *next;
		tail = start;

		for (current = start->next; current; current = next)
		{
			next = current->next;

			if (start->obj_num == current->obj_num)
			{
				pdf_drop_obj(ctx, current->inactive);
				fz_drop_buffer(ctx, current->stream);
				/* start->newobj should not change */
				/* Now drop current */
				if (next)
					next->prev = current->prev;
				current->prev->next = next;
				fz_free(ctx, current);
			}
		}
	}
	entry->tail = tail;
}

/* Call this to end an operation. */
void pdf_end_operation(fz_context *ctx, pdf_document *doc)
{
	pdf_journal_entry *entry;

	if (ctx == NULL || doc == NULL || doc->journal == NULL)
		return;

	/* Decrement the operation nesting count. */
	if (--doc->journal->nesting > 0)
	{
		/* We need to move the contents of doc->pending_tail down to
		 * be on doc->pending_tail->prev. i.e. we combine fragments
		 * as these operations become one. */
		entry = doc->journal->pending_tail;

		/* An implicit operation before we start the file can result in us getting here
		 * with no entry at all! */
		if (entry && entry->prev)
		{
			if (entry->tail == NULL)
			{
				/* Nothing to move. */
			}
			else if (entry->prev->tail == NULL)
			{
				/* Nothing where we want to move it. */
				entry->prev->head = entry->head;
				entry->prev->tail = entry->tail;
			}
			else
			{
				/* Append one list to the other. */
				entry->prev->tail->next = entry->head;
				entry->head->prev = entry->prev->tail;
				entry->prev->tail = entry->tail;
				/* And resolve any clashing objects */
				resolve_undo(ctx, entry->prev);
			}
#ifdef PDF_DEBUG_JOURNAL
			fz_write_printf(ctx, fz_stddbg(ctx), "Ending! (->%d) \"%s\" <= \"%s\"\n", doc->journal->nesting,
					entry->prev->title ? entry->prev->title : "<implicit>",
					entry->title ? entry->title : "<implicit>");
#endif
			doc->journal->pending_tail = entry->prev;
			entry->prev->next = NULL;
			fz_free(ctx, entry->title);
			fz_free(ctx, entry);
		}
		else
		{
#ifdef PDF_DEBUG_JOURNAL
			fz_write_printf(ctx, fz_stddbg(ctx), "Ending! (->%d) no entry\n", doc->journal->nesting);
#endif
		}
		return;
	}

	/* Now, check to see whether we have actually stored any changes
	 * (fragments) into our entry. If we have, we need to move these
	 * changes from pending onto current. */
	entry = doc->journal->pending;
	assert(entry);

	/* We really ought to have just a single pending entry at this point,
	 * but implicit operations when we've just loaded a file can mean
	 * that we don't have an entry at all. */
	if (entry == NULL)
	{
		/* Never happens! */
	}
	else if (entry->head == NULL)
	{
		/* Didn't actually change anything! Remove the empty entry. */
#ifdef PDF_DEBUG_JOURNAL
		fz_write_printf(ctx, fz_stddbg(ctx), "Ending Empty!\n");
#endif
		discard_journal_entries(ctx, &doc->journal->pending);
	}
	else if (entry->title != NULL)
	{
		/* Explicit operation. Move the entry off the pending list. */
		assert(entry->next == NULL);
		if (doc->journal->current)
		{
			doc->journal->current->next = entry;
			entry->prev = doc->journal->current;
			doc->journal->current = entry;
		}
		else
		{
			doc->journal->head = entry;
			doc->journal->current = entry;
		}
#ifdef PDF_DEBUG_JOURNAL
		fz_write_printf(ctx, fz_stddbg(ctx), "Ending!\n");
#endif
	}
	else if (doc->journal->current == NULL)
	{
		/* Implicit operation, with no previous one. */
#ifdef PDF_DEBUG_JOURNAL
		fz_write_printf(ctx, fz_stddbg(ctx), "Ending implicit with no previous!\n");
#endif
		/* Just drop the record of the changes. */
		discard_journal_entries(ctx, &doc->journal->pending);
	}
	else
	{
		/* Implicit operation. Roll these changes into the previous one.*/
#ifdef PDF_DEBUG_JOURNAL
		fz_write_printf(ctx, fz_stddbg(ctx), "Ending implicit!\n");
#endif
		doc->journal->current->tail->next = entry->head;
		entry->head->prev = doc->journal->current->tail;
		doc->journal->current->tail = entry->tail;
		entry->head = NULL;
		entry->tail = NULL;
		fz_free(ctx, entry->title);
		fz_free(ctx, entry);
		/* And resolve any clashing objects */
		resolve_undo(ctx, doc->journal->current);
	}
	doc->journal->pending = NULL;
	doc->journal->pending_tail = NULL;
}

/* Call this to find out how many undo/redo steps there are, and the
 * current position we are within those. 0 = original document,
 * *steps = final edited version. */
int pdf_undoredo_state(fz_context *ctx, pdf_document *doc, int *steps)
{
	int i, c;
	pdf_journal_entry *entry;

	if (ctx == NULL || doc == NULL || doc->journal == NULL)
	{
		*steps = 0;
		return 0;
	}

	if (doc->journal->pending != NULL || doc->journal->nesting > 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't undo/redo during an operation");

	i = 0;
	c = 0;
	for (entry = doc->journal->head; entry != NULL; entry = entry->next)
	{
		i++;
		if (entry == doc->journal->current)
			c = i;
	}

	*steps = i;

	return c;
}

int pdf_can_undo(fz_context *ctx, pdf_document *doc)
{
	int steps, step;

	step = pdf_undoredo_state(ctx, doc, &steps);

	return step > 0;
}

int pdf_can_redo(fz_context *ctx, pdf_document *doc)
{
	int steps, step;

	step = pdf_undoredo_state(ctx, doc, &steps);

	return step != steps;
}

/* Call this to find the title of the operation within the undo state. */
const char *pdf_undoredo_step(fz_context *ctx, pdf_document *doc, int step)
{
	pdf_journal_entry *entry;

	if (ctx == NULL || doc == NULL || doc->journal == NULL)
		return NULL;

	if (doc->journal->pending != NULL || doc->journal->nesting > 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't undo/redo during an operation");

	for (entry = doc->journal->head; step > 0 && entry != NULL; step--, entry = entry->next);

	if (step != 0 || entry == NULL)
		return NULL;

	return entry->title;
}

static void
swap_fragments(fz_context *ctx, pdf_document *doc, pdf_journal_entry *entry)
{
	pdf_journal_fragment *frag;

#ifdef PDF_DEBUG_JOURNAL
	entry->changed_since_last_dumped = 1;
#endif
	if (doc->local_xref_nesting != 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't undo/redo within an operation");

	pdf_drop_local_xref_and_resources(ctx, doc);

	for (frag = entry->head; frag != NULL; frag = frag->next)
	{
		pdf_xref_entry *xre;
		pdf_obj *old;
		fz_buffer *obuf;
		int type;
		xre = pdf_get_incremental_xref_entry(ctx, doc, frag->obj_num);
		old = xre->obj;
		obuf = xre->stm_buf;
		xre->obj = frag->inactive;
		type = xre->type;
		xre->type = frag->newobj ? 0 : 'n';
		frag->newobj = type == 0;
		xre->stm_buf = frag->stream;
		frag->inactive = old;
		frag->stream = obuf;
	}
}

/* Abandon an operation - unwind back to the previous begin. */
void pdf_abandon_operation(fz_context *ctx, pdf_document *doc)
{
	pdf_journal_entry *entry;

	if (ctx == NULL || doc == NULL || doc->journal == NULL)
		return;

	if (doc->journal->nesting == 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't abandon a non-existent operation!");

	doc->journal->nesting--;

	entry = doc->journal->pending_tail;
	assert(entry);

	/* Undo the changes we are about the discard. */
	swap_fragments(ctx, doc, entry);

	/* And discard entry. */
	if (entry->prev == NULL)
	{
		doc->journal->pending = NULL;
		doc->journal->pending_tail = NULL;
	}
	else
	{
		doc->journal->pending_tail = entry->prev;
		entry->prev->next = NULL;
		entry->prev = NULL;
	}
#ifdef PDF_DEBUG_JOURNAL
	fz_write_printf(ctx, fz_stddbg(ctx), "Abandoning!\n");
#endif
	discard_journal_entries(ctx, &entry);
}

/* Move backwards in the undo history. Throws an error if we are at the
 * start. Any edits to the document at this point will discard all
 * subsequent history. */
void pdf_undo(fz_context *ctx, pdf_document *doc)
{
	pdf_journal_entry *entry;
	pdf_journal_fragment *frag;

	if (ctx == NULL || doc == NULL)
		return;

	if (doc->journal == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot undo on unjournaled PDF");

	if (doc->journal->nesting != 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't undo during an operation!");

	entry = doc->journal->current;
	if (entry == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Already at start of history");

#ifdef PDF_DEBUG_JOURNAL
	fz_write_printf(ctx, fz_stddbg(ctx), "Undo!\n");
#endif

	doc->journal->current = entry->prev;

	swap_fragments(ctx, doc, entry);

	// nuke all caches
	pdf_drop_page_tree_internal(ctx, doc);
	pdf_sync_open_pages(ctx, doc);
	for (frag = entry->head; frag; frag = frag->next)
		pdf_purge_object_from_store(ctx, doc, frag->obj_num);
}

/* Move forwards in the undo history. Throws an error if we are at the
 * end. */
void pdf_redo(fz_context *ctx, pdf_document *doc)
{
	pdf_journal_entry *entry;
	pdf_journal_fragment *frag;

	if (ctx == NULL || doc == NULL)
		return;

	if (doc->journal == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot redo on unjournaled PDF");

	if (doc->journal->nesting != 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't redo during an operation!");

#ifdef PDF_DEBUG_JOURNAL
	fz_write_printf(ctx, fz_stddbg(ctx), "Redo!\n");
#endif

	entry = doc->journal->current;
	if (entry == NULL)
	{
		/* If journal->current is null then everything has been undone. */
		/* Go to the first change in journal->head if it exists. */
		entry = doc->journal->head;
	}
	else
	{
		entry = entry->next;
	}

	if (entry == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Already at end of history");

	doc->journal->current = entry;

	swap_fragments(ctx, doc, entry);

	// nuke all caches
	pdf_drop_page_tree_internal(ctx, doc);
	pdf_sync_open_pages(ctx, doc);
	for (frag = entry->head; frag; frag = frag->next)
		pdf_purge_object_from_store(ctx, doc, frag->obj_num);
}

void pdf_discard_journal(fz_context *ctx, pdf_journal *journal)
{
	if (ctx == NULL || journal == NULL)
		return;

	discard_journal_entries(ctx, &journal->head);
	/* Should be NULL, but belt and braces. */
	discard_journal_entries(ctx, &journal->pending);
	journal->head = NULL;
	journal->current = NULL;
	journal->pending = NULL;
	journal->pending_tail = NULL;
}

static void
pdf_fingerprint_file(fz_context *ctx, pdf_document *doc, unsigned char digest[16], int i)
{
	fz_md5 state;

	fz_md5_init(&state);
	fz_md5_update_int64(&state, doc->num_xref_sections-i);
	for (; i < doc->num_xref_sections; i++)
	{
		pdf_xref_subsec *subsec = doc->xref_sections[i].subsec;
		fz_md5_update_int64(&state, doc->xref_sections[i].num_objects);
		while (subsec)
		{
			fz_md5_update_int64(&state, subsec->start);
			fz_md5_update_int64(&state, subsec->len);
			subsec = subsec->next;
		}
	}
	fz_md5_final(&state, digest);
}

void
pdf_serialise_journal(fz_context *ctx, pdf_document *doc, fz_output *out)
{
	pdf_journal_entry *entry;
	int currentpos = 0;
	unsigned char digest[16];
	int i;
	int nis = doc->num_incremental_sections;

	pdf_fingerprint_file(ctx, doc, digest, nis);

	if (!pdf_has_unsaved_changes(ctx, doc))
		nis = 0;

	fz_write_printf(ctx, out, "%!MuPDF-Journal-100\n");
	fz_write_string(ctx, out, "\njournal\n<<\n");
	fz_write_printf(ctx, out, "/NumSections %d\n", nis);
	fz_write_printf(ctx, out, "/FileSize %ld\n", doc->file_size);
	fz_write_printf(ctx, out, "/Fingerprint <");
	for (i = 0; i < 16; i++)
		fz_write_printf(ctx, out, "%02x", digest[i]);
	fz_write_printf(ctx, out, ">\n");

	if (doc->journal->current != NULL)
		for (entry = doc->journal->head; entry != NULL; entry = entry->next)
		{
			currentpos++;
			if (entry == doc->journal->current)
				break;
		}
	fz_write_printf(ctx, out, "/HistoryPos %d\n", currentpos);
	fz_write_string(ctx, out, ">>\n");

	for (entry = doc->journal->head; entry != NULL; entry = entry->next)
	{
		pdf_journal_fragment *frag;
		fz_write_printf(ctx, out, "entry\n%(\n", entry->title);
		for (frag = entry->head; frag != NULL; frag = frag->next)
		{
			if (frag->newobj)
			{
				fz_write_printf(ctx, out, "%d 0 newobj\n", frag->obj_num);
				continue;
			}
			fz_write_printf(ctx, out, "%d 0 obj\n", frag->obj_num);
			pdf_print_encrypted_obj(ctx, out, frag->inactive, 1, 0, NULL, frag->obj_num, 0, NULL);
			if (frag->stream)
			{
				fz_write_printf(ctx, out, "\nstream\n");
				fz_write_data(ctx, out, frag->stream->data, frag->stream->len);
				fz_write_string(ctx, out, "\nendstream");
			}
			fz_write_string(ctx, out, "\nendobj\n");
		}
	}
	fz_write_printf(ctx, out, "endjournal\n");
}

void
pdf_add_journal_fragment(fz_context *ctx, pdf_document *doc, int parent, pdf_obj *copy, fz_buffer *copy_stream, int newobj)
{
	pdf_journal_entry *entry;
	pdf_journal_fragment *frag;

	if (doc->journal == NULL)
		return;

	entry = doc->journal->pending_tail;
	/* We must be in an operation. */
	assert(entry != NULL);
	if (entry == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't add a journal fragment absent an operation");

	/* This should never happen, as we should always be appending to the end of
	 * the pending list. */
	assert(entry->next == NULL);
	if (entry->next)
	{
		discard_journal_entries(ctx, &entry->next);
		doc->journal->pending_tail = NULL;
	}

#ifdef PDF_DEBUG_JOURNAL
	entry->changed_since_last_dumped = 1;
#endif

	fz_try(ctx)
	{
		frag = fz_malloc_struct(ctx, pdf_journal_fragment);
		frag->obj_num = parent;
		if (entry->tail == NULL)
		{
			frag->prev = NULL;
			entry->head = frag;
		}
		else
		{
			frag->prev = entry->tail;
			entry->tail->next = frag;
		}
		entry->tail = frag;
		frag->newobj = newobj;
		frag->inactive = copy;
		frag->stream = copy_stream;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void pdf_deserialise_journal(fz_context *ctx, pdf_document *doc, fz_stream *stm)
{
	int num, version, c, nis, pos;
	pdf_obj *obj = NULL, *fingerprint_obj;
	fz_buffer *buffer;
	unsigned char digest[16];
	int64_t file_size;
	int digests_match = 0;
	pdf_token tok;

	if (!doc || !stm)
		return;

	if (doc->journal)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't load a journal over another one");

	if (fz_skip_string(ctx, stm, "%!MuPDF-Journal-"))
		fz_throw(ctx, FZ_ERROR_FORMAT, "Bad journal format");

	fz_var(obj);
	fz_var(digests_match);

	fz_try(ctx)
	{
		version = 0;
		while (1)
		{
			c = fz_peek_byte(ctx, stm);
			if (c < '0' || c > '9')
				break;
			version = (version*10) + c - '0';
			(void)fz_read_byte(ctx, stm);
		}
		if (version != 100)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad journal format");

		fz_skip_space(ctx, stm);
		if (fz_skip_string(ctx, stm, "journal\n"))
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad journal format");

		tok = pdf_lex(ctx, stm, &doc->lexbuf.base);
		if (tok != PDF_TOK_OPEN_DICT)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad journal format");
		obj = pdf_parse_dict(ctx, doc, stm, &doc->lexbuf.base);

		nis = pdf_dict_get_int(ctx, obj, PDF_NAME(NumSections));
		if (nis < 0 || nis > doc->num_xref_sections)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad journal format");
		pdf_fingerprint_file(ctx, doc, digest, nis);

		file_size = pdf_dict_get_int(ctx, obj, PDF_NAME(FileSize));

		fingerprint_obj = pdf_dict_get(ctx, obj, PDF_NAME(Fingerprint));
		if (pdf_to_str_len(ctx, fingerprint_obj) != 16)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad journal fingerprint");

		digests_match = (memcmp(pdf_to_str_buf(ctx, fingerprint_obj), digest, 16) == 0);

		pos = pdf_dict_get_int(ctx, obj, PDF_NAME(HistoryPos));
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, obj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	if (!digests_match)
		return;

	if (doc->file_size < file_size)
		return;

	doc->journal = fz_malloc_struct(ctx, pdf_journal);

	while (1)
	{
		int newobj;
		fz_skip_space(ctx, stm);

		if (fz_skip_string(ctx, stm, "entry\n") == 0)
		{
			/* Read the fragment title. */
			char *title;
			tok = pdf_lex(ctx, stm, &doc->lexbuf.base);

			if (tok != PDF_TOK_STRING)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Bad string in journal");
			title = fz_malloc(ctx, doc->lexbuf.base.len+1);
			memcpy(title, doc->lexbuf.base.buffer, doc->lexbuf.base.len);
			title[doc->lexbuf.base.len] = 0;

			new_entry(ctx, doc, title);
			continue;
		}
		if (fz_skip_string(ctx, stm, /*en*/"djournal") == 0)
			break;

		if (doc->journal->current == NULL)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Badly formed journal");

		/* Read the object/stream for the next fragment. */
		obj = pdf_parse_journal_obj(ctx, doc, stm, &num, &buffer, &newobj);

		pdf_add_journal_fragment(ctx, doc, num, obj, buffer, newobj);
	}

	fz_skip_space(ctx, stm);

	doc->journal->current = NULL;
	if (pos > 0)
	{
		if (doc->journal->head == NULL)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Badly formed journal");

		doc->journal->current = doc->journal->head;
		while (--pos)
		{
			doc->journal->current = doc->journal->current->next;
			if (doc->journal->current == NULL)
				break;
		}
	}

	doc->file_size = file_size;
	/* We're about to make the last xref an incremental one. All incremental
	 * ones MUST be solid, but the snapshot might not have saved it as such,
	 * so solidify it now. */
	pdf_ensure_solid_xref(ctx, doc, pdf_xref_len(ctx, doc));
	doc->num_incremental_sections = nis;

	if (nis > 0)
	{
		/* Ditch the trailer object out of the xref. Keep the direct
		 * trailer reference. */
		pdf_delete_object(ctx, doc, pdf_obj_parent_num(ctx, doc->xref_sections[0].trailer));
		pdf_set_obj_parent(ctx, doc->xref_sections[0].trailer, 0);
	}
}

static void prepare_object_for_alteration(fz_context *ctx, pdf_obj *obj, pdf_obj *val)
{
	pdf_document *doc, *val_doc;
	int parent;
	pdf_journal_fragment *frag;
	pdf_journal_entry *entry;
	pdf_obj *copy = NULL;
	pdf_obj *orig;
	fz_buffer *copy_stream = NULL;
	int was_empty;

	/*
		obj should be a dict or an array. We don't care about
		any other types, as they aren't 'containers'.
	*/
	if (obj < PDF_LIMIT)
		return;

	switch (obj->kind)
	{
	case PDF_DICT:
		doc = DICT(obj)->doc;
		parent = DICT(obj)->parent_num;
		break;
	case PDF_ARRAY:
		doc = ARRAY(obj)->doc;
		parent = ARRAY(obj)->parent_num;
		break;
	default:
		return;
	}

	assert(doc != NULL);

	/* Do we need to drop the page maps? */
	if (doc->rev_page_map || doc->fwd_page_map)
	{
		if (doc->non_structural_change)
		{
			/* No need to drop the reverse page map on a non-structural change. */
		}
		else if (parent == 0)
		{
			/* This object isn't linked into the document - can't change the
			 * pagemap. */
		}
		else if (doc->local_xref && doc->local_xref_nesting > 0)
		{
			/* We have a local_xref and it's in force. By convention, we
			 * never do structural changes in local_xrefs. */
		}
		else
			pdf_drop_page_tree_internal(ctx, doc);
	}

	if (val)
	{
		val_doc = pdf_get_bound_document(ctx, val);
		if (val_doc && val_doc != doc)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "container and item belong to different documents");
	}

	/*
		The newly linked object needs to record the parent_num.
	*/
	if (parent != 0)
		pdf_set_obj_parent(ctx, val, parent);

	/*
		parent_num == 0 while an object is being parsed from the file.
		No further action is necessary.
	*/
	if (parent == 0 || doc->save_in_progress || doc->repair_in_progress)
		return;

	if (doc->journal && doc->journal->nesting == 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't alter an object other than in an operation");

	if (doc->local_xref)
	{
		/* We have a local_xref. If it's in force, then we're
		 * ready for alteration already. */
		if (doc->local_xref_nesting > 0)
		{
			pdf_xref_ensure_local_object(ctx, doc, parent);
			return;
		}
		else
		{
			/* The local xref isn't in force, and we're about
			 * to edit the document. This invalidates it, so
			 * throw it away. */
			pdf_drop_local_xref_and_resources(ctx, doc);
		}
	}

	// Empty store of items keyed on the object being changed.
	if (parent != 0)
		pdf_purge_object_from_store(ctx, doc, parent);

	entry = NULL;
	if (doc->journal)
	{
		/* We are about to add a fragment. Everything after 'current' in the
		 * history must be thrown away. If current is NULL, then *everything*
		 * must be thrown away. */
		discard_journal_entries(ctx, doc->journal->current ? &doc->journal->current->next : &doc->journal->head);

		/* We should be collating into a pending block. */
		entry = doc->journal->pending_tail;
		assert(entry);

		/* If we've already stashed a value for this object in this fragment,
		 * we don't need to stash another one. It'll only confuse us later. */
		for (frag = entry->head; frag != NULL; frag = frag->next)
			if (frag->obj_num == parent)
			{
				entry = NULL;
				break; /* Already stashed this one! */
			}
	}

	/*
		We need to ensure that the containing hierarchy of objects
		has been moved to the incremental xref section.
	*/
	was_empty = pdf_xref_ensure_incremental_object(ctx, doc, parent);

	/* If we're not journalling, or we've already stashed an 'old' value for this
	 * object, just exit now. */
	if (entry == NULL)
		return;

	/* Load the 'old' value and store it in a fragment. */
	orig = pdf_load_object(ctx, doc, parent);

	fz_var(copy);
	fz_var(copy_stream);

	fz_try(ctx)
	{
		if (was_empty)
		{
			/* was_empty = 1 iff, the the entry in the incremental xref was empty,
			 * and we copied any older value for that object forwards from an old xref.
			 * When we undo, we just want to blank the one in the incremental section.
			 * Effectively this is a "new object". */
			copy = NULL;
			copy_stream = NULL;
		}
		else
		{
			copy = pdf_deep_copy_obj(ctx, orig);
			pdf_set_obj_parent(ctx, copy, parent);
			if (pdf_obj_num_is_stream(ctx, doc, parent))
				copy_stream = pdf_load_raw_stream_number(ctx, doc, parent);
		}
		pdf_add_journal_fragment(ctx, doc, parent, copy, copy_stream, was_empty);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, orig);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, copy_stream);
		pdf_drop_obj(ctx, copy);
		fz_rethrow(ctx);
	}
}

void
pdf_array_put(fz_context *ctx, pdf_obj *obj, int i, pdf_obj *item)
{
	RESOLVE(obj);
	if (!OBJ_IS_ARRAY(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an array (%s)", pdf_objkindstr(obj));
	if (i == ARRAY(obj)->len)
	{
		pdf_array_push(ctx, obj, item);
		return;
	}
	if (i < 0 || i > ARRAY(obj)->len)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "index out of bounds");
	prepare_object_for_alteration(ctx, obj, item);
	pdf_drop_obj(ctx, ARRAY(obj)->items[i]);
	ARRAY(obj)->items[i] = pdf_keep_obj(ctx, item);
}

void
pdf_array_put_drop(fz_context *ctx, pdf_obj *obj, int i, pdf_obj *item)
{
	fz_try(ctx)
		pdf_array_put(ctx, obj, i, item);
	fz_always(ctx)
		pdf_drop_obj(ctx, item);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_array_push(fz_context *ctx, pdf_obj *obj, pdf_obj *item)
{
	RESOLVE(obj);
	if (!OBJ_IS_ARRAY(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an array (%s)", pdf_objkindstr(obj));
	prepare_object_for_alteration(ctx, obj, item);
	if (ARRAY(obj)->len + 1 > ARRAY(obj)->cap)
		pdf_array_grow(ctx, ARRAY(obj));
	ARRAY(obj)->items[ARRAY(obj)->len] = pdf_keep_obj(ctx, item);
	ARRAY(obj)->len++;
}

void
pdf_array_push_drop(fz_context *ctx, pdf_obj *obj, pdf_obj *item)
{
	fz_try(ctx)
		pdf_array_push(ctx, obj, item);
	fz_always(ctx)
		pdf_drop_obj(ctx, item);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_array_insert(fz_context *ctx, pdf_obj *obj, pdf_obj *item, int i)
{
	RESOLVE(obj);
	if (!OBJ_IS_ARRAY(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an array (%s)", pdf_objkindstr(obj));
	if (i < 0 || i > ARRAY(obj)->len)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "index out of bounds");
	prepare_object_for_alteration(ctx, obj, item);
	if (ARRAY(obj)->len + 1 > ARRAY(obj)->cap)
		pdf_array_grow(ctx, ARRAY(obj));
	memmove(ARRAY(obj)->items + i + 1, ARRAY(obj)->items + i, (ARRAY(obj)->len - i) * sizeof(pdf_obj*));
	ARRAY(obj)->items[i] = pdf_keep_obj(ctx, item);
	ARRAY(obj)->len++;
}

void
pdf_array_insert_drop(fz_context *ctx, pdf_obj *obj, pdf_obj *item, int i)
{
	fz_try(ctx)
		pdf_array_insert(ctx, obj, item, i);
	fz_always(ctx)
		pdf_drop_obj(ctx, item);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_array_delete(fz_context *ctx, pdf_obj *obj, int i)
{
	RESOLVE(obj);
	if (!OBJ_IS_ARRAY(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an array (%s)", pdf_objkindstr(obj));
	if (i < 0 || i >= ARRAY(obj)->len)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "index out of bounds");
	prepare_object_for_alteration(ctx, obj, NULL);
	pdf_drop_obj(ctx, ARRAY(obj)->items[i]);
	ARRAY(obj)->items[i] = 0;
	ARRAY(obj)->len--;
	memmove(ARRAY(obj)->items + i, ARRAY(obj)->items + i + 1, (ARRAY(obj)->len - i) * sizeof(pdf_obj*));
}

int
pdf_array_contains(fz_context *ctx, pdf_obj *arr, pdf_obj *obj)
{
	int i, len;

	len = pdf_array_len(ctx, arr);
	for (i = 0; i < len; i++)
		if (!pdf_objcmp(ctx, pdf_array_get(ctx, arr, i), obj))
			return 1;

	return 0;
}

int
pdf_array_find(fz_context *ctx, pdf_obj *arr, pdf_obj *obj)
{
	int i, len;

	len = pdf_array_len(ctx, arr);
	for (i = 0; i < len; i++)
		if (!pdf_objcmp(ctx, pdf_array_get(ctx, arr, i), obj))
			return i;

	return -1;
}

pdf_obj *pdf_new_point(fz_context *ctx, pdf_document *doc, fz_point point)
{
	pdf_obj *arr = pdf_new_array(ctx, doc, 2);
	fz_try(ctx)
	{
		pdf_array_push_real(ctx, arr, point.x);
		pdf_array_push_real(ctx, arr, point.y);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, arr);
		fz_rethrow(ctx);
	}
	return arr;
}

pdf_obj *pdf_new_rect(fz_context *ctx, pdf_document *doc, fz_rect rect)
{
	pdf_obj *arr = pdf_new_array(ctx, doc, 4);
	fz_try(ctx)
	{
		pdf_array_push_real(ctx, arr, rect.x0);
		pdf_array_push_real(ctx, arr, rect.y0);
		pdf_array_push_real(ctx, arr, rect.x1);
		pdf_array_push_real(ctx, arr, rect.y1);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, arr);
		fz_rethrow(ctx);
	}
	return arr;
}

pdf_obj *pdf_new_matrix(fz_context *ctx, pdf_document *doc, fz_matrix mtx)
{
	pdf_obj *arr = pdf_new_array(ctx, doc, 6);
	fz_try(ctx)
	{
		pdf_array_push_real(ctx, arr, mtx.a);
		pdf_array_push_real(ctx, arr, mtx.b);
		pdf_array_push_real(ctx, arr, mtx.c);
		pdf_array_push_real(ctx, arr, mtx.d);
		pdf_array_push_real(ctx, arr, mtx.e);
		pdf_array_push_real(ctx, arr, mtx.f);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, arr);
		fz_rethrow(ctx);
	}
	return arr;
}


pdf_obj *pdf_new_date(fz_context *ctx, pdf_document *doc, int64_t time)
{
	char s[40];
	if (!pdf_format_date(ctx, time, s, nelem(s)))
		return NULL;
	return pdf_new_string(ctx, s, strlen(s));
}

/* dicts may only have names as keys! */

static int keyvalcmp(const void *ap, const void *bp)
{
	const struct keyval *a = ap;
	const struct keyval *b = bp;
	const char *an;
	const char *bn;

	/* We should never get a->k == NULL or b->k == NULL. If we
	 * do, then they match. */
	if (a->k < PDF_LIMIT)
		an = PDF_NAME_LIST[(intptr_t)a->k];
	else if (a->k >= PDF_LIMIT && a->k->kind == PDF_NAME)
		an = NAME(a->k)->n;
	else
		return 0;

	if (b->k < PDF_LIMIT)
		bn = PDF_NAME_LIST[(intptr_t)b->k];
	else if (b->k >= PDF_LIMIT && b->k->kind == PDF_NAME)
		bn = NAME(b->k)->n;
	else
		return 0;

	return strcmp(an, bn);
}

pdf_obj *
pdf_new_dict(fz_context *ctx, pdf_document *doc, int initialcap)
{
	pdf_obj_dict *obj;
	int i;

	if (doc == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot create dictionary without a document");

	obj = Memento_label(fz_malloc(ctx, sizeof(pdf_obj_dict)), "pdf_obj(dict)");
	obj->super.refs = 1;
	obj->super.kind = PDF_DICT;
	obj->super.flags = 0;
	obj->doc = doc;
	obj->parent_num = 0;

	obj->len = 0;
	obj->cap = initialcap > 1 ? initialcap : 10;

	fz_try(ctx)
	{
		DICT(obj)->items = Memento_label(fz_malloc_array(ctx, DICT(obj)->cap, struct keyval), "dict_items");
	}
	fz_catch(ctx)
	{
		fz_free(ctx, obj);
		fz_rethrow(ctx);
	}
	for (i = 0; i < DICT(obj)->cap; i++)
	{
		DICT(obj)->items[i].k = NULL;
		DICT(obj)->items[i].v = NULL;
	}

	return &obj->super;
}

static void
pdf_dict_grow(fz_context *ctx, pdf_obj *obj)
{
	int i;
	int new_cap = (DICT(obj)->cap * 3) / 2;

	DICT(obj)->items = fz_realloc_array(ctx, DICT(obj)->items, new_cap, struct keyval);
	DICT(obj)->cap = new_cap;

	for (i = DICT(obj)->len; i < DICT(obj)->cap; i++)
	{
		DICT(obj)->items[i].k = NULL;
		DICT(obj)->items[i].v = NULL;
	}
}

pdf_obj *
pdf_copy_dict(fz_context *ctx, pdf_obj *obj)
{
	pdf_document *doc;
	pdf_obj *dict;
	int i, n;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not a dict (%s)", pdf_objkindstr(obj));

	doc = DICT(obj)->doc;
	n = pdf_dict_len(ctx, obj);
	dict = pdf_new_dict(ctx, doc, n);
	fz_try(ctx)
		for (i = 0; i < n; i++)
			pdf_dict_put(ctx, dict, pdf_dict_get_key(ctx, obj, i), pdf_dict_get_val(ctx, obj, i));
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, dict);
		fz_rethrow(ctx);
	}

	return dict;
}

int
pdf_dict_len(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		return 0;
	return DICT(obj)->len;
}

pdf_obj *
pdf_dict_get_key(fz_context *ctx, pdf_obj *obj, int i)
{
	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		return NULL;
	if (i < 0 || i >= DICT(obj)->len)
		return NULL;
	return DICT(obj)->items[i].k;
}

pdf_obj *
pdf_dict_get_val(fz_context *ctx, pdf_obj *obj, int i)
{
	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		return NULL;
	if (i < 0 || i >= DICT(obj)->len)
		return NULL;
	return DICT(obj)->items[i].v;
}

void
pdf_dict_put_val_null(fz_context *ctx, pdf_obj *obj, int idx)
{
	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not a dict (%s)", pdf_objkindstr(obj));
	if (idx < 0 || idx >= DICT(obj)->len)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "index out of bounds");

	prepare_object_for_alteration(ctx, obj, NULL);
	pdf_drop_obj(ctx, DICT(obj)->items[idx].v);
	DICT(obj)->items[idx].v = PDF_NULL;
}

/* Returns 0 <= i < len for key found. Returns -1-len < i <= -1 for key
 * not found, but with insertion point -1-i. */
static int
pdf_dict_finds(fz_context *ctx, pdf_obj *obj, const char *key)
{
	int len = DICT(obj)->len;
	if ((obj->flags & PDF_FLAGS_SORTED) && len > 0)
	{
		int l = 0;
		int r = len - 1;

		if (strcmp(pdf_to_name(ctx, DICT(obj)->items[r].k), key) < 0)
		{
			return -1 - (r+1);
		}

		while (l <= r)
		{
			int m = (l + r) >> 1;
			int c = -strcmp(pdf_to_name(ctx, DICT(obj)->items[m].k), key);
			if (c < 0)
				r = m - 1;
			else if (c > 0)
				l = m + 1;
			else
				return m;
		}
		return -1 - l;
	}

	else
	{
		int i;
		for (i = 0; i < len; i++)
			if (strcmp(pdf_to_name(ctx, DICT(obj)->items[i].k), key) == 0)
				return i;

		return -1 - len;
	}
}

static int
pdf_dict_find(fz_context *ctx, pdf_obj *obj, pdf_obj *key)
{
	int len = DICT(obj)->len;
	if ((obj->flags & PDF_FLAGS_SORTED) && len > 0)
	{
		int l = 0;
		int r = len - 1;
		pdf_obj *k = DICT(obj)->items[r].k;

		if (k == key || (k >= PDF_LIMIT && strcmp(NAME(k)->n, PDF_NAME_LIST[(intptr_t)key]) < 0))
		{
			return -1 - (r+1);
		}

		while (l <= r)
		{
			int m = (l + r) >> 1;
			int c;

			k = DICT(obj)->items[m].k;
			c = (k < PDF_LIMIT ? (char *)key-(char *)k : -strcmp(NAME(k)->n, PDF_NAME_LIST[(intptr_t)key]));
			if (c < 0)
				r = m - 1;
			else if (c > 0)
				l = m + 1;
			else
				return m;
		}
		return -1 - l;
	}
	else
	{
		int i;
		for (i = 0; i < len; i++)
		{
			pdf_obj *k = DICT(obj)->items[i].k;
			if (k < PDF_LIMIT)
			{
				if (k == key)
					return i;
			}
			else
			{
				if (!strcmp(PDF_NAME_LIST[(intptr_t)key], NAME(k)->n))
					return i;
			}
		}

		return -1 - len;
	}
}

pdf_obj *
pdf_dict_gets(fz_context *ctx, pdf_obj *obj, const char *key)
{
	int i;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		return NULL;
	if (!key)
		return NULL;

	i = pdf_dict_finds(ctx, obj, key);
	if (i >= 0)
		return DICT(obj)->items[i].v;
	return NULL;
}

pdf_obj *
pdf_dict_getp(fz_context *ctx, pdf_obj *obj, const char *keys)
{
	char buf[256];
	char *k, *e;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		return NULL;
	if (strlen(keys)+1 > 256)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "path too long");

	strcpy(buf, keys);

	e = buf;
	while (*e && obj)
	{
		k = e;
		while (*e != '/' && *e != '\0')
			e++;

		if (*e == '/')
		{
			*e = '\0';
			e++;
		}

		obj = pdf_dict_gets(ctx, obj, k);
	}

	return obj;
}

pdf_obj *
pdf_dict_getl(fz_context *ctx, pdf_obj *obj, ...)
{
	va_list keys;
	pdf_obj *key;

	va_start(keys, obj);

	while (obj != NULL && (key = va_arg(keys, pdf_obj *)) != NULL)
	{
		obj = pdf_dict_get(ctx, obj, key);
	}

	va_end(keys);
	return obj;
}

pdf_obj *
pdf_dict_get(fz_context *ctx, pdf_obj *obj, pdf_obj *key)
{
	int i;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		return NULL;
	if (!OBJ_IS_NAME(key))
		return NULL;

	if (key < PDF_LIMIT)
		i = pdf_dict_find(ctx, obj, key);
	else
		i = pdf_dict_finds(ctx, obj, pdf_to_name(ctx, key));
	if (i >= 0)
		return DICT(obj)->items[i].v;
	return NULL;
}

pdf_obj *
pdf_dict_getsa(fz_context *ctx, pdf_obj *obj, const char *key, const char *abbrev)
{
	pdf_obj *v;
	v = pdf_dict_gets(ctx, obj, key);
	if (v)
		return v;
	return pdf_dict_gets(ctx, obj, abbrev);
}

pdf_obj *
pdf_dict_geta(fz_context *ctx, pdf_obj *obj, pdf_obj *key, pdf_obj *abbrev)
{
	pdf_obj *v;
	/* ISO 32000-2:2020 (PDF 2.0) - abbreviated names take precedence. */
	v = pdf_dict_get(ctx, obj, abbrev);
	if (v)
		return v;
	return pdf_dict_get(ctx, obj, key);
}

static void
pdf_dict_get_put(fz_context *ctx, pdf_obj *obj, pdf_obj *key, pdf_obj *val, pdf_obj **old_val)
{
	int i;

	if (old_val)
		*old_val = NULL;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not a dict (%s)", pdf_objkindstr(obj));
	if (!OBJ_IS_NAME(key))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "key is not a name (%s)", pdf_objkindstr(obj));

	if (DICT(obj)->len > 100 && !(obj->flags & PDF_FLAGS_SORTED))
		pdf_sort_dict(ctx, obj);

	if (key < PDF_LIMIT)
		i = pdf_dict_find(ctx, obj, key);
	else
		i = pdf_dict_finds(ctx, obj, pdf_to_name(ctx, key));

	prepare_object_for_alteration(ctx, obj, val);

	if (i >= 0 && i < DICT(obj)->len)
	{
		if (DICT(obj)->items[i].v != val)
		{
			pdf_obj *d = DICT(obj)->items[i].v;
			DICT(obj)->items[i].v = pdf_keep_obj(ctx, val);
			if (old_val)
				*old_val = d;
			else
				pdf_drop_obj(ctx, d);
		}
	}
	else
	{
		if (DICT(obj)->len + 1 > DICT(obj)->cap)
			pdf_dict_grow(ctx, obj);

		i = -1-i;
		if ((obj->flags & PDF_FLAGS_SORTED) && DICT(obj)->len > 0)
			memmove(&DICT(obj)->items[i + 1],
					&DICT(obj)->items[i],
					(DICT(obj)->len - i) * sizeof(struct keyval));

		DICT(obj)->items[i].k = pdf_keep_obj(ctx, key);
		DICT(obj)->items[i].v = pdf_keep_obj(ctx, val);
		DICT(obj)->len ++;
	}
}

void
pdf_dict_put(fz_context *ctx, pdf_obj *obj, pdf_obj *key, pdf_obj *val)
{
	pdf_dict_get_put(ctx, obj, key, val, NULL);
}

void
pdf_dict_put_drop(fz_context *ctx, pdf_obj *obj, pdf_obj *key, pdf_obj *val)
{
	fz_try(ctx)
		pdf_dict_get_put(ctx, obj, key, val, NULL);
	fz_always(ctx)
		pdf_drop_obj(ctx, val);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_dict_get_put_drop(fz_context *ctx, pdf_obj *obj, pdf_obj *key, pdf_obj *val, pdf_obj **old_val)
{
	fz_try(ctx)
		pdf_dict_get_put(ctx, obj, key, val, old_val);
	fz_always(ctx)
		pdf_drop_obj(ctx, val);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_dict_puts(fz_context *ctx, pdf_obj *obj, const char *key, pdf_obj *val)
{
	pdf_obj *keyobj;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not a dict (%s)", pdf_objkindstr(obj));

	keyobj = pdf_new_name(ctx, key);

	fz_try(ctx)
		pdf_dict_put(ctx, obj, keyobj, val);
	fz_always(ctx)
		pdf_drop_obj(ctx, keyobj);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_dict_puts_drop(fz_context *ctx, pdf_obj *obj, const char *key, pdf_obj *val)
{
	pdf_obj *keyobj;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not a dict (%s)", pdf_objkindstr(obj));

	keyobj = pdf_new_name(ctx, key);

	fz_var(keyobj);

	fz_try(ctx)
		pdf_dict_put(ctx, obj, keyobj, val);
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, keyobj);
		pdf_drop_obj(ctx, val);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
pdf_dict_putp(fz_context *ctx, pdf_obj *obj, const char *keys, pdf_obj *val)
{
	pdf_document *doc;
	char buf[256];
	char *k, *e;
	pdf_obj *cobj = NULL;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not a dict (%s)", pdf_objkindstr(obj));
	if (strlen(keys)+1 > 256)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "path too long");

	doc = DICT(obj)->doc;
	strcpy(buf, keys);

	e = buf;
	while (*e)
	{
		k = e;
		while (*e != '/' && *e != '\0')
			e++;

		if (*e == '/')
		{
			*e = '\0';
			e++;
		}

		if (*e)
		{
			/* Not the last key in the key path. Create subdict if not already there. */
			cobj = pdf_dict_gets(ctx, obj, k);
			if (!pdf_is_dict(ctx, cobj))
			{
				cobj = pdf_new_dict(ctx, doc, 1);
				fz_try(ctx)
					pdf_dict_puts(ctx, obj, k, cobj);
				fz_always(ctx)
					pdf_drop_obj(ctx, cobj);
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
			/* Move to subdict */
			obj = cobj;
		}
		else
		{
			/* Last key. Use it to store the value */
			/* Use val = NULL to request delete */
			if (val)
				pdf_dict_puts(ctx, obj, k, val);
			else
				pdf_dict_dels(ctx, obj, k);
		}
	}
}

void
pdf_dict_putp_drop(fz_context *ctx, pdf_obj *obj, const char *keys, pdf_obj *val)
{
	fz_try(ctx)
		pdf_dict_putp(ctx, obj, keys, val);
	fz_always(ctx)
		pdf_drop_obj(ctx, val);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_dict_vputl(fz_context *ctx, pdf_obj *obj, pdf_obj *val, va_list keys)
{
	pdf_obj *key;
	pdf_obj *next_key;
	pdf_obj *next_obj;
	pdf_document *doc;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not a dict (%s)", pdf_objkindstr(obj));

	doc = DICT(obj)->doc;

	key = va_arg(keys, pdf_obj *);
	if (key == NULL)
		return;

	while ((next_key = va_arg(keys, pdf_obj *)) != NULL)
	{
		next_obj = pdf_dict_get(ctx, obj, key);
		if (next_obj == NULL)
			goto new_obj;
		obj = next_obj;
		key = next_key;
	}

	pdf_dict_put(ctx, obj, key, val);
	return;

new_obj:
	/* We have to create entries */
	do
	{
		next_obj = pdf_new_dict(ctx, doc, 1);
		pdf_dict_put_drop(ctx, obj, key, next_obj);
		obj = next_obj;
		key = next_key;
	}
	while ((next_key = va_arg(keys, pdf_obj *)) != NULL);

	pdf_dict_put(ctx, obj, key, val);
	return;
}

void
pdf_dict_putl(fz_context *ctx, pdf_obj *obj, pdf_obj *val, ...)
{
	va_list keys;
	va_start(keys, val);

	fz_try(ctx)
		pdf_dict_vputl(ctx, obj, val, keys);
	fz_always(ctx)
		va_end(keys);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_dict_putl_drop(fz_context *ctx, pdf_obj *obj, pdf_obj *val, ...)
{
	va_list keys;
	va_start(keys, val);

	fz_try(ctx)
		pdf_dict_vputl(ctx, obj, val, keys);
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, val);
		va_end(keys);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_dict_dels(fz_context *ctx, pdf_obj *obj, const char *key)
{
	int i;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not a dict (%s)", pdf_objkindstr(obj));
	if (!key)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "key is null");

	prepare_object_for_alteration(ctx, obj, NULL);
	i = pdf_dict_finds(ctx, obj, key);
	if (i >= 0)
	{
		pdf_drop_obj(ctx, DICT(obj)->items[i].k);
		pdf_drop_obj(ctx, DICT(obj)->items[i].v);
		obj->flags &= ~PDF_FLAGS_SORTED;
		DICT(obj)->items[i] = DICT(obj)->items[DICT(obj)->len-1];
		DICT(obj)->len --;
	}
}

void
pdf_dict_del(fz_context *ctx, pdf_obj *obj, pdf_obj *key)
{
	if (!OBJ_IS_NAME(key))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "key is not a name (%s)", pdf_objkindstr(key));

	if (key < PDF_LIMIT)
		pdf_dict_dels(ctx, obj, PDF_NAME_LIST[(intptr_t)key]);
	else
		pdf_dict_dels(ctx, obj, NAME(key)->n);
}

void
pdf_sort_dict(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		return;
	if (!(obj->flags & PDF_FLAGS_SORTED))
	{
		qsort(DICT(obj)->items, DICT(obj)->len, sizeof(struct keyval), keyvalcmp);
		obj->flags |= PDF_FLAGS_SORTED;
	}
}

pdf_obj *
pdf_deep_copy_obj(fz_context *ctx, pdf_obj *obj)
{
	if (obj < PDF_LIMIT)
	{
		return obj;
	}
	if (obj->kind == PDF_DICT)
	{
		pdf_document *doc = DICT(obj)->doc;
		int n = pdf_dict_len(ctx, obj);
		pdf_obj *dict = pdf_new_dict(ctx, doc, n);
		int i;

		fz_try(ctx)
			for (i = 0; i < n; i++)
			{
				pdf_obj *obj_copy = pdf_deep_copy_obj(ctx, pdf_dict_get_val(ctx, obj, i));
				pdf_dict_put_drop(ctx, dict, pdf_dict_get_key(ctx, obj, i), obj_copy);
			}
		fz_catch(ctx)
		{
			pdf_drop_obj(ctx, dict);
			fz_rethrow(ctx);
		}

		DICT(dict)->parent_num = DICT(obj)->parent_num;
		return dict;
	}
	else if (obj->kind == PDF_ARRAY)
	{
		pdf_document *doc = ARRAY(obj)->doc;
		int n = pdf_array_len(ctx, obj);
		pdf_obj *arr = pdf_new_array(ctx, doc, n);
		int i;

		fz_try(ctx)
			for (i = 0; i < n; i++)
			{
				pdf_obj *obj_copy = pdf_deep_copy_obj(ctx, pdf_array_get(ctx, obj, i));
				pdf_array_push_drop(ctx, arr, obj_copy);
			}
		fz_catch(ctx)
		{
			pdf_drop_obj(ctx, arr);
			fz_rethrow(ctx);
		}

		ARRAY(arr)->parent_num = ARRAY(obj)->parent_num;
		return arr;
	}
	else
	{
		return pdf_keep_obj(ctx, obj);
	}
}

/* obj marking and unmarking functions - to avoid infinite recursions. */
int
pdf_obj_marked(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return 0;
	return !!(obj->flags & PDF_FLAGS_MARKED);
}

int
pdf_mark_obj(fz_context *ctx, pdf_obj *obj)
{
	int marked;
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return 0;
	marked = !!(obj->flags & PDF_FLAGS_MARKED);
	obj->flags |= PDF_FLAGS_MARKED;
	return marked;
}

void
pdf_unmark_obj(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return;
	obj->flags &= ~PDF_FLAGS_MARKED;
}

int
pdf_cycle(fz_context *ctx, pdf_cycle_list *here, pdf_cycle_list *up, pdf_obj *obj)
{
	int num = pdf_to_num(ctx, obj);
	if (num > 0)
	{
		pdf_cycle_list *x = up;
		while (x)
		{
			if (x->num == num)
				return 1;
			x = x->up;
		}
	}
	here->up = up;
	here->num = num;
	return 0;
}

pdf_mark_bits *
pdf_new_mark_bits(fz_context *ctx, pdf_document *doc)
{
	int n = pdf_xref_len(ctx, doc);
	int nb = (n + 7) >> 3;
	pdf_mark_bits *marks = fz_calloc(ctx, offsetof(pdf_mark_bits, bits) + nb, 1);
	marks->len = n;
	return marks;
}

void
pdf_drop_mark_bits(fz_context *ctx, pdf_mark_bits *marks)
{
	fz_free(ctx, marks);
}

void pdf_mark_bits_reset(fz_context *ctx, pdf_mark_bits *marks)
{
	memset(marks->bits, 0, (marks->len + 7) >> 3);
}

int pdf_mark_bits_set(fz_context *ctx, pdf_mark_bits *marks, pdf_obj *obj)
{
	int num = pdf_to_num(ctx, obj);
	if (num > 0 && num < marks->len)
	{
		int x = num >> 3;
		int m = 1 << (num & 7);
		if (marks->bits[x] & m)
			return 1;
		marks->bits[x] |= m;
	}
	return 0;
}

void pdf_mark_bits_clear(fz_context *ctx, pdf_mark_bits *marks, pdf_obj *obj)
{
	int num = pdf_to_num(ctx, obj);
	if (num > 0 && num < marks->len)
	{
		int x = num >> 3;
		int m = 0xff ^ (1 << (num & 7));
		marks->bits[x] &= m;
	}
}

int
pdf_mark_list_push(fz_context *ctx, pdf_mark_list *marks, pdf_obj *obj)
{
	int num = pdf_to_num(ctx, obj);
	int i;

	/* If object is not an indirection, then no record to check.
	 * We must still push it to allow pops to stay in sync. */
	if (num > 0)
	{
		/* Note: this is slow, if the mark list is expected to be big use pdf_mark_bits instead! */
		for (i = 0; i < marks->len; ++i)
			if (marks->list[i] == num)
				return 1;
	}

	if (marks->len == marks->max)
	{
		int newsize = marks->max << 1;
		if (marks->list == marks->local_list)
		{
			marks->list = fz_malloc_array(ctx, newsize, int);
			memcpy(marks->list, marks->local_list, sizeof(marks->local_list));
		}
		else
			marks->list = fz_realloc_array(ctx, marks->list, newsize, int);
		marks->max = newsize;
	}

	marks->list[marks->len++] = num;
	return 0;
}

void
pdf_mark_list_pop(fz_context *ctx, pdf_mark_list *marks)
{
	--marks->len;
}

int
pdf_mark_list_check(fz_context *ctx, pdf_mark_list *marks, pdf_obj *obj)
{
	if (pdf_mark_list_push(ctx, marks, obj))
		return 1;
	pdf_mark_list_pop(ctx, marks);

	return 0;
}

void
pdf_mark_list_init(fz_context *ctx, pdf_mark_list *marks)
{
	marks->len = 0;
	marks->max = nelem(marks->local_list);
	marks->list = marks->local_list;
}

void
pdf_mark_list_free(fz_context *ctx, pdf_mark_list *marks)
{
	if (marks->list != marks->local_list)
		fz_free(ctx, marks->list);
	marks->len = 0;
	marks->max = 0;
	marks->list = NULL;
}

void
pdf_set_obj_memo(fz_context *ctx, pdf_obj *obj, int bit, int memo)
{
	if (obj < PDF_LIMIT)
		return;
	bit <<= 1;
	obj->flags |= PDF_FLAGS_MEMO_BASE << bit;
	if (memo)
		obj->flags |= PDF_FLAGS_MEMO_BASE_BOOL << bit;
	else
		obj->flags &= ~(PDF_FLAGS_MEMO_BASE_BOOL << bit);
}

int
pdf_obj_memo(fz_context *ctx, pdf_obj *obj, int bit, int *memo)
{
	if (obj < PDF_LIMIT)
		return 0;
	bit <<= 1;
	if (!(obj->flags & (PDF_FLAGS_MEMO_BASE<<bit)))
		return 0;
	*memo = !!(obj->flags & (PDF_FLAGS_MEMO_BASE_BOOL<<bit));
	return 1;
}

/* obj dirty bit support. */
int pdf_obj_is_dirty(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return 0;
	return !!(obj->flags & PDF_FLAGS_DIRTY);
}

void pdf_dirty_obj(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return;
	obj->flags |= PDF_FLAGS_DIRTY;
}

void pdf_clean_obj(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return;
	obj->flags &= ~PDF_FLAGS_DIRTY;
}

static void
pdf_drop_array(fz_context *ctx, pdf_obj *obj)
{
	int i;

	for (i = 0; i < DICT(obj)->len; i++)
		pdf_drop_obj(ctx, ARRAY(obj)->items[i]);

	fz_free(ctx, DICT(obj)->items);
	fz_free(ctx, obj);
}

static void
pdf_drop_dict(fz_context *ctx, pdf_obj *obj)
{
	int i;

	for (i = 0; i < DICT(obj)->len; i++) {
		pdf_drop_obj(ctx, DICT(obj)->items[i].k);
		pdf_drop_obj(ctx, DICT(obj)->items[i].v);
	}

	fz_free(ctx, DICT(obj)->items);
	fz_free(ctx, obj);
}

pdf_obj *
pdf_keep_obj(fz_context *ctx, pdf_obj *obj)
{
	if (obj >= PDF_LIMIT)
		return fz_keep_imp16(ctx, obj, &obj->refs);
	return obj;
}

void
pdf_drop_obj(fz_context *ctx, pdf_obj *obj)
{
	if (obj >= PDF_LIMIT)
	{
		if (fz_drop_imp16(ctx, obj, &obj->refs))
		{
			if (obj->kind == PDF_ARRAY)
				pdf_drop_array(ctx, obj);
			else if (obj->kind == PDF_DICT)
				pdf_drop_dict(ctx, obj);
			else if (obj->kind == PDF_STRING)
			{
				fz_free(ctx, STRING(obj)->text);
				fz_free(ctx, obj);
			}
			else
				fz_free(ctx, obj);
		}
	}
}

pdf_obj *
pdf_drop_singleton_obj(fz_context *ctx, pdf_obj *obj)
{
	int drop;

	/* If an object is < PDF_LIMIT, then it's a 'common' name or
	 * true or false. No point in dropping these as it
	 * won't save any memory. */
	if (obj < PDF_LIMIT)
		return obj;

	/* See if it's a singleton object. We can only drop if
	 * it's a singleton object. If not, just exit leaving
	 * everything unchanged. */
	fz_lock(ctx, FZ_LOCK_ALLOC);
	drop = (obj->refs == 1);
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (!drop)
		return obj;

	/* So drop the object! */
	if (obj->kind == PDF_ARRAY)
		pdf_drop_array(ctx, obj);
	else if (obj->kind == PDF_DICT)
		pdf_drop_dict(ctx, obj);
	else if (obj->kind == PDF_STRING)
	{
		fz_free(ctx, STRING(obj)->text);
		fz_free(ctx, obj);
	}
	else
		fz_free(ctx, obj);

	return NULL;
}

/*
	Recurse through the object structure setting the node's parent_num to num.
	parent_num is used when a subobject is to be changed during a document edit.
	The whole containing hierarchy is moved to the incremental xref section, so
	to be later written out as an incremental file update.
*/
void
pdf_set_obj_parent(fz_context *ctx, pdf_obj *obj, int num)
{
	int n, i;

	if (obj < PDF_LIMIT)
		return;

	switch (obj->kind)
	{
	case PDF_ARRAY:
		ARRAY(obj)->parent_num = num;
		n = pdf_array_len(ctx, obj);
		for (i = 0; i < n; i++)
			pdf_set_obj_parent(ctx, pdf_array_get(ctx, obj, i), num);
		break;
	case PDF_DICT:
		DICT(obj)->parent_num = num;
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
			pdf_set_obj_parent(ctx, pdf_dict_get_val(ctx, obj, i), num);
		break;
	}
}

int pdf_obj_parent_num(fz_context *ctx, pdf_obj *obj)
{
	if (obj < PDF_LIMIT)
		return 0;

	switch (obj->kind)
	{
	case PDF_INDIRECT:
		return REF(obj)->num;
	case PDF_ARRAY:
		return ARRAY(obj)->parent_num;
	case PDF_DICT:
		return DICT(obj)->parent_num;
	default:
		return 0;
	}
}

/* Pretty printing objects */

struct fmt
{
	char *buf; /* original static buffer */
	char *ptr; /* buffer we're writing to, maybe dynamically reallocated */
	size_t cap;
	size_t len;
	int indent;
	int tight;
	int ascii;
	int col;
	int sep;
	int last;
	pdf_crypt *crypt;
	int num;
	int gen;
};

static void fmt_obj(fz_context *ctx, struct fmt *fmt, pdf_obj *obj);

static inline int iswhite(int ch)
{
	return
		ch == '\000' ||
		ch == '\011' ||
		ch == '\012' ||
		ch == '\014' ||
		ch == '\015' ||
		ch == '\040';
}

static inline int isdelim(int ch)
{
	return
		ch == '(' || ch == ')' ||
		ch == '<' || ch == '>' ||
		ch == '[' || ch == ']' ||
		ch == '{' || ch == '}' ||
		ch == '/' ||
		ch == '%';
}

static inline void fmt_putc(fz_context *ctx, struct fmt *fmt, int c)
{
	if (fmt->sep && !isdelim(fmt->last) && !iswhite(fmt->last) && !isdelim(c) && !iswhite(c)) {
		fmt->sep = 0;
		fmt_putc(ctx, fmt, ' ');
	}
	fmt->sep = 0;

	if (fmt->len >= fmt->cap)
	{
		fmt->cap *= 2;
		if (fmt->buf == fmt->ptr)
		{
			fmt->ptr = Memento_label(fz_malloc(ctx, fmt->cap), "fmt_ptr");
			memcpy(fmt->ptr, fmt->buf, fmt->len);
		}
		else
		{
			fmt->ptr = fz_realloc(ctx, fmt->ptr, fmt->cap);
		}
	}

	fmt->ptr[fmt->len] = c;

	if (c == '\n')
		fmt->col = 0;
	else
		fmt->col ++;

	fmt->len ++;

	fmt->last = c;
}

static inline void fmt_indent(fz_context *ctx, struct fmt *fmt)
{
	int i = fmt->indent;
	while (i--) {
		fmt_putc(ctx, fmt, ' ');
		fmt_putc(ctx, fmt, ' ');
	}
}

static inline void fmt_puts(fz_context *ctx, struct fmt *fmt, char *s)
{
	while (*s)
		fmt_putc(ctx, fmt, *s++);
}

static inline void fmt_sep(fz_context *ctx, struct fmt *fmt)
{
	fmt->sep = 1;
}

static int is_binary_string(fz_context *ctx, pdf_obj *obj)
{
	unsigned char *s = (unsigned char *)pdf_to_str_buf(ctx, obj);
	size_t i, n = pdf_to_str_len(ctx, obj);
	for (i = 0; i < n; ++i)
	{
		if (s[i] > 126) return 1;
		if (s[i] < 32 && (s[i] != '\t' && s[i] != '\n' && s[i] != '\r')) return 1;
	}
	return 0;
}

static int is_longer_than_hex(fz_context *ctx, pdf_obj *obj)
{
	unsigned char *s = (unsigned char *)pdf_to_str_buf(ctx, obj);
	size_t i, n = pdf_to_str_len(ctx, obj);
	size_t m = 0;
	for (i = 0; i < n; ++i)
	{
		if (s[i] > 126)
			m += 4;
		else if (s[i] == 0)
			m += 4;
		else if (strchr("\n\r\t\b\f()\\", s[i]))
			m += 2;
		else if (s[i] < 32)
			m += 4;
		else
			m += 1;
	}
	return m > (n * 2);
}

static void fmt_str_out(fz_context *ctx, void *fmt_, const unsigned char *s, size_t n)
{
	struct fmt *fmt = (struct fmt *)fmt_;
	int c;
	size_t i;

	for (i = 0; i < n; i++)
	{
		c = (unsigned char)s[i];
		if (c == '\n')
			fmt_puts(ctx, fmt, "\\n");
		else if (c == '\r')
			fmt_puts(ctx, fmt, "\\r");
		else if (c == '\t')
			fmt_puts(ctx, fmt, "\\t");
		else if (c == '\b')
			fmt_puts(ctx, fmt, "\\b");
		else if (c == '\f')
			fmt_puts(ctx, fmt, "\\f");
		else if (c == '(')
			fmt_puts(ctx, fmt, "\\(");
		else if (c == ')')
			fmt_puts(ctx, fmt, "\\)");
		else if (c == '\\')
			fmt_puts(ctx, fmt, "\\\\");
		else if (c < 32 || c >= 127) {
			fmt_putc(ctx, fmt, '\\');
			fmt_putc(ctx, fmt, '0' + ((c / 64) & 7));
			fmt_putc(ctx, fmt, '0' + ((c / 8) & 7));
			fmt_putc(ctx, fmt, '0' + ((c) & 7));
		}
		else
			fmt_putc(ctx, fmt, c);
	}
}

static void fmt_str(fz_context *ctx, struct fmt *fmt, pdf_obj *obj)
{
	unsigned char *s = (unsigned char *)pdf_to_str_buf(ctx, obj);
	size_t n = pdf_to_str_len(ctx, obj);

	fmt_putc(ctx, fmt, '(');
	pdf_encrypt_data(ctx, fmt->crypt, fmt->num, fmt->gen, fmt_str_out, fmt, s, n);
	fmt_putc(ctx, fmt, ')');
}

static void fmt_hex_out(fz_context *ctx, void *arg, const unsigned char *s, size_t n)
{
	struct fmt *fmt = (struct fmt *)arg;
	size_t i;
	int b, c;

	for (i = 0; i < n; i++) {
		b = (unsigned char) s[i];
		c = (b >> 4) & 0x0f;
		fmt_putc(ctx, fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
		c = (b) & 0x0f;
		fmt_putc(ctx, fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
	}
}

static void fmt_hex(fz_context *ctx, struct fmt *fmt, pdf_obj *obj)
{
	unsigned char *s = (unsigned char *)pdf_to_str_buf(ctx, obj);
	size_t n = pdf_to_str_len(ctx, obj);

	fmt_putc(ctx, fmt, '<');
	pdf_encrypt_data(ctx, fmt->crypt, fmt->num, fmt->gen, fmt_hex_out, fmt, s, n);
	fmt_putc(ctx, fmt, '>');
}

static void fmt_name(fz_context *ctx, struct fmt *fmt, pdf_obj *obj)
{
	unsigned char *s = (unsigned char *) pdf_to_name(ctx, obj);
	int i, c;

	fmt_putc(ctx, fmt, '/');

	for (i = 0; s[i]; i++)
	{
		if (isdelim(s[i]) || iswhite(s[i]) ||
			s[i] == '#' || s[i] < 32 || s[i] >= 127)
		{
			fmt_putc(ctx, fmt, '#');
			c = (s[i] >> 4) & 0xf;
			fmt_putc(ctx, fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
			c = s[i] & 0xf;
			fmt_putc(ctx, fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
		}
		else
		{
			fmt_putc(ctx, fmt, s[i]);
		}
	}

	fmt->sep = 1;
}

static void fmt_array(fz_context *ctx, struct fmt *fmt, pdf_obj *obj)
{
	int i, n;

	n = pdf_array_len(ctx, obj);
	if (fmt->tight) {
		fmt_putc(ctx, fmt, '[');
		for (i = 0; i < n; i++) {
			fmt_obj(ctx, fmt, pdf_array_get(ctx, obj, i));
		}
		fmt_putc(ctx, fmt, ']');
	}
	else {
		fmt_putc(ctx, fmt, '[');
		fmt->indent ++;
		for (i = 0; i < n; i++) {
			if (fmt->col > 60) {
				fmt_putc(ctx, fmt, '\n');
				fmt_indent(ctx, fmt);
			} else {
				fmt_putc(ctx, fmt, ' ');
			}
			fmt_obj(ctx, fmt, pdf_array_get(ctx, obj, i));
		}
		fmt->indent --;
		fmt_putc(ctx, fmt, ' ');
		fmt_putc(ctx, fmt, ']');
		fmt_sep(ctx, fmt);
	}
}

static int is_signature(fz_context *ctx, pdf_obj *obj)
{
	if (pdf_dict_get(ctx, obj, PDF_NAME(Type)) ==  PDF_NAME(Sig))
		if (pdf_dict_get(ctx, obj, PDF_NAME(Contents)) && pdf_dict_get(ctx, obj, PDF_NAME(ByteRange)) && pdf_dict_get(ctx, obj, PDF_NAME(Filter)))
			return 1;
	return 0;
}

static void fmt_dict(fz_context *ctx, struct fmt *fmt, pdf_obj *obj)
{
	int i, n;
	pdf_obj *key, *val;
	int skip = 0;
	pdf_obj *type = pdf_dict_get(ctx, obj, PDF_NAME(Type));

	n = pdf_dict_len(ctx, obj);

	/* Open the dictionary.
	 * We spot /Type and /Subtype here so we can sent those first,
	 * in order. The hope is this will improve compression, because
	 * we'll be consistently sending those first. */
	if (fmt->tight) {
		fmt_puts(ctx, fmt, "<<");
		if (type)
		{
			pdf_obj *subtype = pdf_dict_get(ctx, obj, PDF_NAME(Subtype));
			fmt_obj(ctx, fmt, PDF_NAME(Type));
			fmt_obj(ctx, fmt, type);
			if (subtype)
			{
				fmt_obj(ctx, fmt, PDF_NAME(Subtype));
				fmt_obj(ctx, fmt, subtype);
				skip |= 2; /* Skip Subtype */
			}
			skip |= 1; /* Skip Type */
		}

		/* Now send all the key/value pairs except the ones we have decided to
		 * skip. */
		for (i = 0; i < n; i++) {
			key = pdf_dict_get_key(ctx, obj, i);
			if (skip)
			{
				if ((skip & 1) != 0 && key == PDF_NAME(Type))
					continue;
				if ((skip & 2) != 0 && key == PDF_NAME(Subtype))
					continue;
			}
			val = pdf_dict_get_val(ctx, obj, i);
			fmt_obj(ctx, fmt, key);
			if (key == PDF_NAME(Contents) && is_signature(ctx, obj))
			{
				pdf_crypt *crypt = fmt->crypt;
				fz_try(ctx)
				{
					fmt->crypt = NULL;
					fmt_obj(ctx, fmt, val);
				}
				fz_always(ctx)
					fmt->crypt = crypt;
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
			else
				fmt_obj(ctx, fmt, val);
		}

		fmt_puts(ctx, fmt, ">>");
	}
	else /* Not tight, send it simply. */
	{
		fmt_puts(ctx, fmt, "<<\n");
		fmt->indent ++;
		for (i = 0; i < n; i++) {
			key = pdf_dict_get_key(ctx, obj, i);
			val = pdf_dict_get_val(ctx, obj, i);
			fmt_indent(ctx, fmt);
			fmt_obj(ctx, fmt, key);
			fmt_putc(ctx, fmt, ' ');
			if (!pdf_is_indirect(ctx, val) && pdf_is_array(ctx, val))
				fmt->indent ++;
			if (key == PDF_NAME(Contents) && is_signature(ctx, obj))
			{
				pdf_crypt *crypt = fmt->crypt;
				fz_try(ctx)
				{
					fmt->crypt = NULL;
					fmt_obj(ctx, fmt, val);
				}
				fz_always(ctx)
					fmt->crypt = crypt;
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
			else
				fmt_obj(ctx, fmt, val);
			fmt_putc(ctx, fmt, '\n');
			if (!pdf_is_indirect(ctx, val) && pdf_is_array(ctx, val))
				fmt->indent --;
		}
		fmt->indent --;
		fmt_indent(ctx, fmt);
		fmt_puts(ctx, fmt, ">>");
	}
}

static void fmt_obj(fz_context *ctx, struct fmt *fmt, pdf_obj *obj)
{
	char buf[256];

	if (obj == PDF_NULL)
	{
		fmt_puts(ctx, fmt, "null");
		fmt->sep = 1;
		return;
	}
	else if (obj == PDF_TRUE)
	{
		fmt_puts(ctx, fmt, "true");
		fmt->sep = 1;
		return;
	}
	else if (obj == PDF_FALSE)
	{
		fmt_puts(ctx, fmt, "false");
		fmt->sep = 1;
		return;
	}
	else if (pdf_is_indirect(ctx, obj))
	{
		int n = pdf_to_num(ctx, obj);
		int g = pdf_to_gen(ctx, obj);
		fz_snprintf(buf, sizeof buf, "%d %d R", n, g);
		fmt_puts(ctx, fmt, buf);
		fmt->sep = 1;
		return;
	}
	else if (pdf_is_int(ctx, obj))
	{
		fz_snprintf(buf, sizeof buf, "%ld", pdf_to_int64(ctx, obj));
		fmt_puts(ctx, fmt, buf);
		fmt->sep = 1;
		return;
	}
	else if (pdf_is_real(ctx, obj))
	{
		float f = pdf_to_real(ctx, obj);
		if (f == (int)f)
			fz_snprintf(buf, sizeof buf, "%d", (int)f);
		else
			fz_snprintf(buf, sizeof buf, "%g", f);
		fmt_puts(ctx, fmt, buf);
		fmt->sep = 1;
		return;
	}
	else if (pdf_is_string(ctx, obj))
	{
		unsigned char *str = (unsigned char *)pdf_to_str_buf(ctx, obj);
		if (fmt->crypt
			|| (fmt->ascii && is_binary_string(ctx, obj))
			|| (str[0]==0xff && str[1]==0xfe)
			|| (str[0]==0xfe && str[1] == 0xff)
			|| is_longer_than_hex(ctx, obj)
			)
			fmt_hex(ctx, fmt, obj);
		else
			fmt_str(ctx, fmt, obj);
	}
	else if (pdf_is_name(ctx, obj))
		fmt_name(ctx, fmt, obj);
	else if (pdf_is_array(ctx, obj))
		fmt_array(ctx, fmt, obj);
	else if (pdf_is_dict(ctx, obj))
		fmt_dict(ctx, fmt, obj);
	else
		fmt_puts(ctx, fmt, "<unknown object>");
}

static char *
pdf_sprint_encrypted_obj(fz_context *ctx, char *buf, size_t cap, size_t *len, pdf_obj *obj, int tight, int ascii, pdf_crypt *crypt, int num, int gen, int *sep)
{
	struct fmt fmt;

	fmt.indent = 0;
	fmt.col = 0;
	fmt.sep = sep ? *sep : 0;
	fmt.last = 0;

	if (!buf || cap == 0)
	{
		fmt.cap = 1024;
		fmt.buf = NULL;
		fmt.ptr = Memento_label(fz_malloc(ctx, fmt.cap), "fmt_buf");
	}
	else
	{
		fmt.cap = cap;
		fmt.buf = buf;
		fmt.ptr = buf;
	}

	fmt.tight = tight;
	fmt.ascii = ascii;
	fmt.len = 0;
	fmt.crypt = crypt;
	fmt.num = num;
	fmt.gen = gen;

	fz_try(ctx)
	{
		fmt_obj(ctx, &fmt, obj);
		if (sep)
			*sep = fmt.sep;
		fmt.sep = 0;
		fmt_putc(ctx, &fmt, 0);
	}
	fz_catch(ctx)
	{
		if (!buf || cap == 0)
			fz_free(ctx, fmt.ptr);
		fz_rethrow(ctx);
	}

	return *len = fmt.len-1, fmt.ptr;
}

char *
pdf_sprint_obj(fz_context *ctx, char *buf, size_t cap, size_t *len, pdf_obj *obj, int tight, int ascii)
{
	return pdf_sprint_encrypted_obj(ctx, buf, cap, len, obj, tight, ascii, NULL, 0, 0, NULL);
}

void pdf_print_encrypted_obj(fz_context *ctx, fz_output *out, pdf_obj *obj, int tight, int ascii, pdf_crypt *crypt, int num, int gen, int *sep)
{
	char buf[1024];
	char *ptr;
	size_t n;

	ptr = pdf_sprint_encrypted_obj(ctx, buf, sizeof buf, &n, obj, tight, ascii, crypt, num, gen, sep);
	fz_try(ctx)
		fz_write_data(ctx, out, ptr, n);
	fz_always(ctx)
		if (ptr != buf)
			fz_free(ctx, ptr);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void pdf_print_obj(fz_context *ctx, fz_output *out, pdf_obj *obj, int tight, int ascii)
{
	pdf_print_encrypted_obj(ctx, out, obj, tight, ascii, NULL, 0, 0, NULL);
}

void pdf_debug_obj(fz_context *ctx, pdf_obj *obj)
{
	pdf_print_obj(ctx, fz_stddbg(ctx), pdf_resolve_indirect(ctx, obj), 0, 0);
}

void pdf_debug_ref(fz_context *ctx, pdf_obj *obj)
{
	fz_output *out = fz_stddbg(ctx);
	pdf_print_obj(ctx, out, obj, 0, 0);
	fz_write_byte(ctx, out, '\n');
}

int pdf_obj_refs(fz_context *ctx, pdf_obj *obj)
{
	if (obj < PDF_LIMIT)
		return 0;
	return obj->refs;
}

/* Convenience functions */

/*
	Uses Floyd's cycle finding algorithm, modified to avoid starting
	the 'slow' pointer for a while.

	https://www.geeksforgeeks.org/floyds-cycle-finding-algorithm/
*/
pdf_obj *
pdf_dict_get_inheritable(fz_context *ctx, pdf_obj *node, pdf_obj *key)
{
	pdf_obj *slow = node;
	int halfbeat = 11; /* Don't start moving slow pointer for a while. */

	while (node)
	{
		pdf_obj *val = pdf_dict_get(ctx, node, key);
		if (val)
			return val;
		node = pdf_dict_get(ctx, node, PDF_NAME(Parent));
		if (node == slow)
			fz_throw(ctx, FZ_ERROR_FORMAT, "cycle in resources");
		if (--halfbeat == 0)
		{
			slow = pdf_dict_get(ctx, slow, PDF_NAME(Parent));
			halfbeat = 2;
		}
	}

	return NULL;
}

pdf_obj *
pdf_dict_getp_inheritable(fz_context *ctx, pdf_obj *node, const char *path)
{
	pdf_obj *slow = node;
	int halfbeat = 11; /* Don't start moving slow pointer for a while. */

	while (node)
	{
		pdf_obj *val = pdf_dict_getp(ctx, node, path);
		if (val)
			return val;
		node = pdf_dict_get(ctx, node, PDF_NAME(Parent));
		if (node == slow)
			fz_throw(ctx, FZ_ERROR_FORMAT, "cycle in resources");
		if (--halfbeat == 0)
		{
			slow = pdf_dict_get(ctx, slow, PDF_NAME(Parent));
			halfbeat = 2;
		}
	}

	return NULL;
}

pdf_obj *
pdf_dict_gets_inheritable(fz_context *ctx, pdf_obj *node, const char *key)
{
	pdf_obj *slow = node;
	int halfbeat = 11; /* Don't start moving slow pointer for a while. */

	while (node)
	{
		pdf_obj *val = pdf_dict_gets(ctx, node, key);
		if (val)
			return val;
		node = pdf_dict_get(ctx, node, PDF_NAME(Parent));
		if (node == slow)
			fz_throw(ctx, FZ_ERROR_FORMAT, "cycle in resources");
		if (--halfbeat == 0)
		{
			slow = pdf_dict_get(ctx, slow, PDF_NAME(Parent));
			halfbeat = 2;
		}
	}

	return NULL;
}


void pdf_dict_put_bool(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int x)
{
	pdf_dict_put(ctx, dict, key, x ? PDF_TRUE : PDF_FALSE);
}

void pdf_dict_put_int(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int64_t x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_int(ctx, x));
}

void pdf_dict_put_real(fz_context *ctx, pdf_obj *dict, pdf_obj *key, double x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_real(ctx, x));
}

void pdf_dict_put_name(fz_context *ctx, pdf_obj *dict, pdf_obj *key, const char *x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_name(ctx, x));
}

void pdf_dict_put_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, const char *x, size_t n)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_string(ctx, x, n));
}

void pdf_dict_put_text_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, const char *x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_text_string(ctx, x));
}

void pdf_dict_put_indirect(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int num)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_indirect(ctx, pdf_get_bound_document(ctx, dict), num, 0));
}

void pdf_dict_put_point(fz_context *ctx, pdf_obj *dict, pdf_obj *key, fz_point x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_point(ctx, pdf_get_bound_document(ctx, dict), x));
}

void pdf_dict_put_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key, fz_rect x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_rect(ctx, pdf_get_bound_document(ctx, dict), x));
}

void pdf_dict_put_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key, fz_matrix x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_matrix(ctx, pdf_get_bound_document(ctx, dict), x));
}

void pdf_dict_put_date(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int64_t time)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_date(ctx, pdf_get_bound_document(ctx, dict), time));
}

pdf_obj *pdf_dict_put_array(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int initial)
{
	pdf_obj *obj = pdf_new_array(ctx, pdf_get_bound_document(ctx, dict), initial);
	pdf_dict_put_drop(ctx, dict, key, obj);
	return obj;
}

pdf_obj *pdf_dict_put_dict(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int initial)
{
	pdf_obj *obj = pdf_new_dict(ctx, pdf_get_bound_document(ctx, dict), initial);
	pdf_dict_put_drop(ctx, dict, key, obj);
	return obj;
}

pdf_obj *pdf_dict_puts_dict(fz_context *ctx, pdf_obj *dict, const char *key, int initial)
{
	pdf_obj *obj = pdf_new_dict(ctx, pdf_get_bound_document(ctx, dict), initial);
	pdf_dict_puts_drop(ctx, dict, key, obj);
	return obj;
}

void pdf_array_push_bool(fz_context *ctx, pdf_obj *array, int x)
{
	pdf_array_push(ctx, array, x ? PDF_TRUE : PDF_FALSE);
}

void pdf_array_push_int(fz_context *ctx, pdf_obj *array, int64_t x)
{
	pdf_array_push_drop(ctx, array, pdf_new_int(ctx, x));
}

void pdf_array_push_real(fz_context *ctx, pdf_obj *array, double x)
{
	pdf_array_push_drop(ctx, array, pdf_new_real(ctx, x));
}

void pdf_array_push_name(fz_context *ctx, pdf_obj *array, const char *x)
{
	pdf_array_push_drop(ctx, array, pdf_new_name(ctx, x));
}

void pdf_array_push_string(fz_context *ctx, pdf_obj *array, const char *x, size_t n)
{
	pdf_array_push_drop(ctx, array, pdf_new_string(ctx, x, n));
}

void pdf_array_push_text_string(fz_context *ctx, pdf_obj *array, const char *x)
{
	pdf_array_push_drop(ctx, array, pdf_new_text_string(ctx, x));
}

pdf_obj *pdf_array_push_array(fz_context *ctx, pdf_obj *array, int initial)
{
	pdf_obj *obj = pdf_new_array(ctx, pdf_get_bound_document(ctx, array), initial);
	pdf_array_push_drop(ctx, array, obj);
	return obj;
}

pdf_obj *pdf_array_push_dict(fz_context *ctx, pdf_obj *array, int initial)
{
	pdf_obj *obj = pdf_new_dict(ctx, pdf_get_bound_document(ctx, array), initial);
	pdf_array_push_drop(ctx, array, obj);
	return obj;
}

void pdf_array_put_bool(fz_context *ctx, pdf_obj *array, int i, int x)
{
	pdf_array_put(ctx, array, i, x ? PDF_TRUE : PDF_FALSE);
}

void pdf_array_put_int(fz_context *ctx, pdf_obj *array, int i, int64_t x)
{
	pdf_array_put_drop(ctx, array, i, pdf_new_int(ctx, x));
}

void pdf_array_put_real(fz_context *ctx, pdf_obj *array, int i, double x)
{
	pdf_array_put_drop(ctx, array, i, pdf_new_real(ctx, x));
}

void pdf_array_put_name(fz_context *ctx, pdf_obj *array, int i, const char *x)
{
	pdf_array_put_drop(ctx, array, i, pdf_new_name(ctx, x));
}

void pdf_array_put_string(fz_context *ctx, pdf_obj *array, int i, const char *x, size_t n)
{
	pdf_array_put_drop(ctx, array, i, pdf_new_string(ctx, x, n));
}

void pdf_array_put_text_string(fz_context *ctx, pdf_obj *array, int i, const char *x)
{
	pdf_array_put_drop(ctx, array, i, pdf_new_text_string(ctx, x));
}

pdf_obj *pdf_array_put_array(fz_context *ctx, pdf_obj *array, int i, int initial)
{
	pdf_obj *obj = pdf_new_array(ctx, pdf_get_bound_document(ctx, array), initial);
	pdf_array_put_drop(ctx, array, i, obj);
	return obj;
}

pdf_obj *pdf_array_put_dict(fz_context *ctx, pdf_obj *array, int i, int initial)
{
	pdf_obj *obj = pdf_new_dict(ctx, pdf_get_bound_document(ctx, array), initial);
	pdf_array_put_drop(ctx, array, i, obj);
	return obj;
}

int pdf_dict_get_bool(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_bool(ctx, pdf_dict_get(ctx, dict, key));
}

int pdf_dict_get_bool_default(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int def)
{
	return pdf_to_bool_default(ctx, pdf_dict_get(ctx, dict, key), def);
}

int pdf_dict_get_int(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_int(ctx, pdf_dict_get(ctx, dict, key));
}

int pdf_dict_get_int_default(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int def)
{
	return pdf_to_int_default(ctx, pdf_dict_get(ctx, dict, key), def);
}

int64_t pdf_dict_get_int64(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_int64(ctx, pdf_dict_get(ctx, dict, key));
}

float pdf_dict_get_real(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_real(ctx, pdf_dict_get(ctx, dict, key));
}

float pdf_dict_get_real_default(fz_context *ctx, pdf_obj *dict, pdf_obj *key, float def)
{
	return pdf_to_real_default(ctx, pdf_dict_get(ctx, dict, key), def);
}

const char *pdf_dict_get_name(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_name(ctx, pdf_dict_get(ctx, dict, key));
}

const char *pdf_dict_get_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, size_t *sizep)
{
	return pdf_to_string(ctx, pdf_dict_get(ctx, dict, key), sizep);
}

const char *pdf_dict_get_text_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_text_string(ctx, pdf_dict_get(ctx, dict, key));
}

const char *pdf_dict_get_text_string_opt(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, key);
	if (!pdf_is_string(ctx, obj))
		return NULL;
	return pdf_to_text_string(ctx, obj);
}

fz_point pdf_dict_get_point(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_point(ctx, pdf_dict_get(ctx, dict, key), 0);
}

fz_rect pdf_dict_get_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_rect(ctx, pdf_dict_get(ctx, dict, key));
}

fz_matrix pdf_dict_get_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_matrix(ctx, pdf_dict_get(ctx, dict, key));
}

int pdf_dict_get_inheritable_bool(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_bool(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

int pdf_dict_get_inheritable_int(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

int64_t pdf_dict_get_inheritable_int64(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_int64(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

float pdf_dict_get_inheritable_real(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_real(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

const char *pdf_dict_get_inheritable_name(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_name(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

const char *pdf_dict_get_inheritable_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, size_t *sizep)
{
	return pdf_to_string(ctx, pdf_dict_get_inheritable(ctx, dict, key), sizep);
}

const char *pdf_dict_get_inheritable_text_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_text_string(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

fz_rect pdf_dict_get_inheritable_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_rect(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

fz_matrix pdf_dict_get_inheritable_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_matrix(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

int64_t pdf_dict_get_inheritable_date(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_date(ctx, pdf_dict_get_inheritable(ctx, dict, key));
}

int64_t pdf_dict_get_date(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_date(ctx, pdf_dict_get(ctx, dict, key));
}

int pdf_array_get_bool(fz_context *ctx, pdf_obj *array, int index)
{
	return pdf_to_bool(ctx, pdf_array_get(ctx, array, index));
}

int pdf_array_get_int(fz_context *ctx, pdf_obj *array, int index)
{
	return pdf_to_int(ctx, pdf_array_get(ctx, array, index));
}

float pdf_array_get_real(fz_context *ctx, pdf_obj *array, int index)
{
	return pdf_to_real(ctx, pdf_array_get(ctx, array, index));
}

const char *pdf_array_get_name(fz_context *ctx, pdf_obj *array, int index)
{
	return pdf_to_name(ctx, pdf_array_get(ctx, array, index));
}

const char *pdf_array_get_string(fz_context *ctx, pdf_obj *array, int index, size_t *sizep)
{
	return pdf_to_string(ctx, pdf_array_get(ctx, array, index), sizep);
}

const char *pdf_array_get_text_string(fz_context *ctx, pdf_obj *array, int index)
{
	return pdf_to_text_string(ctx, pdf_array_get(ctx, array, index));
}

fz_rect pdf_array_get_rect(fz_context *ctx, pdf_obj *array, int index)
{
	return pdf_to_rect(ctx, pdf_array_get(ctx, array, index));
}

fz_matrix pdf_array_get_matrix(fz_context *ctx, pdf_obj *array, int index)
{
	return pdf_to_matrix(ctx, pdf_array_get(ctx, array, index));
}

#ifndef NDEBUG
void pdf_verify_name_table_sanity(void)
{
	int i;

	for (i = PDF_ENUM_FALSE+1; i < PDF_ENUM_LIMIT-1; i++)
	{
		assert(strcmp(PDF_NAME_LIST[i], PDF_NAME_LIST[i+1]) < 0);
	}
}
#endif

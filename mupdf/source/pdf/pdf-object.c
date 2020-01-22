#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "../fitz/fitz-imp.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

struct pdf_obj_s
{
	short refs;
	unsigned char kind;
	unsigned char flags;
};

typedef struct pdf_obj_num_s
{
	pdf_obj super;
	union
	{
		int64_t i;
		float f;
	} u;
} pdf_obj_num;

typedef struct pdf_obj_string_s
{
	pdf_obj super;
	char *text; /* utf8 encoded text string */
	size_t len;
	char buf[1];
} pdf_obj_string;

typedef struct pdf_obj_name_s
{
	pdf_obj super;
	char n[1];
} pdf_obj_name;

typedef struct pdf_obj_array_s
{
	pdf_obj super;
	pdf_document *doc;
	int parent_num;
	int len;
	int cap;
	pdf_obj **items;
} pdf_obj_array;

typedef struct pdf_obj_dict_s
{
	pdf_obj super;
	pdf_document *doc;
	int parent_num;
	int len;
	int cap;
	struct keyval *items;
} pdf_obj_dict;

typedef struct pdf_obj_ref_s
{
	pdf_obj super;
	pdf_document *doc; /* Only needed for arrays, dicts and indirects */
	int num;
	int gen;
} pdf_obj_ref;

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
		fz_throw(ctx, FZ_ERROR_GENERIC, "Overflow in pdf string");

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
		fz_throw(ctx, FZ_ERROR_SYNTAX, "invalid object number (%d)", num);
	if (gen < 0 || gen > PDF_MAX_GEN_NUMBER)
		fz_throw(ctx, FZ_ERROR_SYNTAX, "invalid generation number (%d)", gen);
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

int pdf_to_int(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return 0;
	if (obj->kind == PDF_INT)
		return (int)NUM(obj)->u.i;
	if (obj->kind == PDF_REAL)
		return (int)(NUM(obj)->u.f + 0.5f); /* No roundf in MSVC */
	return 0;
}

int64_t pdf_to_int64(fz_context *ctx, pdf_obj *obj)
{
	RESOLVE(obj);
	if (obj < PDF_LIMIT)
		return 0;
	if (obj->kind == PDF_INT)
		return NUM(obj)->u.i;
	if (obj->kind == PDF_REAL)
		return (((double)NUM(obj)->u.f) + 0.5f); /* No roundf in MSVC */
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

/* for use by pdf_crypt_obj_imp to decrypt AES string in place */
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

pdf_document *pdf_get_indirect_document(fz_context *ctx, pdf_obj *obj)
{
	if (OBJ_IS_INDIRECT(obj))
		return REF(obj)->doc;
	return NULL;
}

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

int pdf_objcmp_resolve(fz_context *ctx, pdf_obj *a, pdf_obj *b)
{
	RESOLVE(a);
	RESOLVE(b);
	return pdf_objcmp(ctx, a, b);
}

int
pdf_objcmp(fz_context *ctx, pdf_obj *a, pdf_obj *b)
{
	int i;

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
		for (i = 0; i < DICT(a)->len; i++)
		{
			if (pdf_objcmp(ctx, DICT(a)->items[i].k, DICT(b)->items[i].k))
				return 1;
			if (pdf_objcmp(ctx, DICT(a)->items[i].v, DICT(b)->items[i].v))
				return 1;
		}
		return 0;
	}
	return 1;
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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not an array (%s)", pdf_objkindstr(obj));

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

static void prepare_object_for_alteration(fz_context *ctx, pdf_obj *obj, pdf_obj *val)
{
	pdf_document *doc, *val_doc;
	int parent;

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

	if (val)
	{
		val_doc = pdf_get_bound_document(ctx, val);
		if (val_doc && val_doc != doc)
			fz_throw(ctx, FZ_ERROR_GENERIC, "container and item belong to different documents");
	}

	/*
		parent_num == 0 while an object is being parsed from the file.
		No further action is necessary.
	*/
	if (parent == 0 || doc->save_in_progress || doc->repair_attempted)
		return;

	/*
		Otherwise we need to ensure that the containing hierarchy of objects
		has been moved to the incremental xref section and the newly linked
		object needs to record the parent_num
	*/
	pdf_xref_ensure_incremental_object(ctx, doc, parent);
	pdf_set_obj_parent(ctx, val, parent);
}

void
pdf_array_put(fz_context *ctx, pdf_obj *obj, int i, pdf_obj *item)
{
	RESOLVE(obj);
	if (!OBJ_IS_ARRAY(obj))
		fz_throw(ctx, FZ_ERROR_GENERIC, "not an array (%s)", pdf_objkindstr(obj));
	if (i == ARRAY(obj)->len)
	{
		pdf_array_push(ctx, obj, item);
		return;
	}
	if (i < 0 || i > ARRAY(obj)->len)
		fz_throw(ctx, FZ_ERROR_GENERIC, "index out of bounds");
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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not an array (%s)", pdf_objkindstr(obj));
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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not an array (%s)", pdf_objkindstr(obj));
	if (i < 0 || i > ARRAY(obj)->len)
		fz_throw(ctx, FZ_ERROR_GENERIC, "index out of bounds");
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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not an array (%s)", pdf_objkindstr(obj));
	if (i < 0 || i >= ARRAY(obj)->len)
		fz_throw(ctx, FZ_ERROR_GENERIC, "index out of bounds");
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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a dict (%s)", pdf_objkindstr(obj));

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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a dict (%s)", pdf_objkindstr(obj));
	if (idx < 0 || idx >= DICT(obj)->len)
		fz_throw(ctx, FZ_ERROR_GENERIC, "index out of bounds");

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
		fz_throw(ctx, FZ_ERROR_GENERIC, "path too long");

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
	v = pdf_dict_get(ctx, obj, key);
	if (v)
		return v;
	return pdf_dict_get(ctx, obj, abbrev);
}

static void
pdf_dict_get_put(fz_context *ctx, pdf_obj *obj, pdf_obj *key, pdf_obj *val, pdf_obj **old_val)
{
	int i;

	if (old_val)
		*old_val = NULL;

	RESOLVE(obj);
	if (!OBJ_IS_DICT(obj))
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a dict (%s)", pdf_objkindstr(obj));
	if (!OBJ_IS_NAME(key))
		fz_throw(ctx, FZ_ERROR_GENERIC, "key is not a name (%s)", pdf_objkindstr(obj));

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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a dict (%s)", pdf_objkindstr(obj));

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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a dict (%s)", pdf_objkindstr(obj));

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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a dict (%s)", pdf_objkindstr(obj));
	if (strlen(keys)+1 > 256)
		fz_throw(ctx, FZ_ERROR_GENERIC, "buffer overflow in pdf_dict_putp");

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
			if (cobj == NULL)
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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a dict (%s)", pdf_objkindstr(obj));

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
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a dict (%s)", pdf_objkindstr(obj));
	if (!key)
		fz_throw(ctx, FZ_ERROR_GENERIC, "key is null");

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
		fz_throw(ctx, FZ_ERROR_GENERIC, "key is not a name (%s)", pdf_objkindstr(key));

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
	if (fmt->sep && !isdelim(fmt->last) && !isdelim(c)) {
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
}

static void fmt_array(fz_context *ctx, struct fmt *fmt, pdf_obj *obj)
{
	int i, n;

	n = pdf_array_len(ctx, obj);
	if (fmt->tight) {
		fmt_putc(ctx, fmt, '[');
		for (i = 0; i < n; i++) {
			fmt_obj(ctx, fmt, pdf_array_get(ctx, obj, i));
			fmt_sep(ctx, fmt);
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

static void fmt_dict(fz_context *ctx, struct fmt *fmt, pdf_obj *obj)
{
	int i, n;
	pdf_obj *key, *val;

	n = pdf_dict_len(ctx, obj);
	if (fmt->tight) {
		fmt_puts(ctx, fmt, "<<");
		for (i = 0; i < n; i++) {
			fmt_obj(ctx, fmt, pdf_dict_get_key(ctx, obj, i));
			fmt_sep(ctx, fmt);
			fmt_obj(ctx, fmt, pdf_dict_get_val(ctx, obj, i));
			fmt_sep(ctx, fmt);
		}
		fmt_puts(ctx, fmt, ">>");
	}
	else {
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
		fmt_puts(ctx, fmt, "null");
	else if (obj == PDF_TRUE)
		fmt_puts(ctx, fmt, "true");
	else if (obj == PDF_FALSE)
		fmt_puts(ctx, fmt, "false");
	else if (pdf_is_indirect(ctx, obj))
	{
		fz_snprintf(buf, sizeof buf, "%d %d R", pdf_to_num(ctx, obj), pdf_to_gen(ctx, obj));
		fmt_puts(ctx, fmt, buf);
	}
	else if (pdf_is_int(ctx, obj))
	{
		fz_snprintf(buf, sizeof buf, "%d", pdf_to_int(ctx, obj));
		fmt_puts(ctx, fmt, buf);
	}
	else if (pdf_is_real(ctx, obj))
	{
		fz_snprintf(buf, sizeof buf, "%g", pdf_to_real(ctx, obj));
		fmt_puts(ctx, fmt, buf);
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
pdf_sprint_encrypted_obj(fz_context *ctx, char *buf, size_t cap, size_t *len, pdf_obj *obj, int tight, int ascii, pdf_crypt *crypt, int num, int gen)
{
	struct fmt fmt;

	fmt.indent = 0;
	fmt.col = 0;
	fmt.sep = 0;
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
	fmt_obj(ctx, &fmt, obj);

	fmt_putc(ctx, &fmt, 0);

	return *len = fmt.len-1, fmt.ptr;
}

char *
pdf_sprint_obj(fz_context *ctx, char *buf, size_t cap, size_t *len, pdf_obj *obj, int tight, int ascii)
{
	return pdf_sprint_encrypted_obj(ctx, buf, cap, len, obj, tight, ascii, NULL, 0, 0);
}

void pdf_print_encrypted_obj(fz_context *ctx, fz_output *out, pdf_obj *obj, int tight, int ascii, pdf_crypt *crypt, int num, int gen)
{
	char buf[1024];
	char *ptr;
	size_t n;

	ptr = pdf_sprint_encrypted_obj(ctx, buf, sizeof buf, &n, obj, tight, ascii,crypt, num, gen);
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
	pdf_print_encrypted_obj(ctx, out, obj, tight, ascii, NULL, 0, 0);
}

static void pdf_debug_encrypted_obj(fz_context *ctx, pdf_obj *obj, int tight, pdf_crypt *crypt, int num, int gen)
{
	char buf[1024];
	char *ptr;
	size_t n;
	int ascii = 1;

	ptr = pdf_sprint_encrypted_obj(ctx, buf, sizeof buf, &n, obj, tight, ascii, crypt, num, gen);
	fwrite(ptr, 1, n, stdout);
	if (ptr != buf)
		fz_free(ctx, ptr);
}

void pdf_debug_obj(fz_context *ctx, pdf_obj *obj)
{
	pdf_debug_encrypted_obj(ctx, pdf_resolve_indirect(ctx, obj), 0, NULL, 0, 0);
	putchar('\n');
}

void pdf_debug_ref(fz_context *ctx, pdf_obj *obj)
{
	pdf_debug_encrypted_obj(ctx, obj, 0, NULL, 0, 0);
	putchar('\n');
}

int pdf_obj_refs(fz_context *ctx, pdf_obj *obj)
{
	if (obj < PDF_LIMIT)
		return 0;
	return obj->refs;
}

/* Convenience functions */

pdf_obj *
pdf_dict_get_inheritable(fz_context *ctx, pdf_obj *node, pdf_obj *key)
{
	pdf_obj *node2 = node;
	pdf_obj *val = NULL;
	pdf_obj *marked = NULL;

	fz_var(node);
	fz_var(marked);
	fz_try(ctx)
	{
		do
		{
			val = pdf_dict_get(ctx, node, key);
			if (val)
				break;
			if (pdf_mark_obj(ctx, node))
				fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in tree (parents)");
			marked = node;
			node = pdf_dict_get(ctx, node, PDF_NAME(Parent));
		}
		while (node);
	}
	fz_always(ctx)
	{
		/* We assume that if we have marked an object, without an exception
		 * being thrown, that we can always unmark the same object again
		 * without an exception being thrown. */
		if (marked)
		{
			do
			{
				pdf_unmark_obj(ctx, node2);
				if (node2 == marked)
					break;
				node2 = pdf_dict_get(ctx, node2, PDF_NAME(Parent));
			}
			while (node2);
		}
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return val;
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

void pdf_dict_put_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key, fz_rect x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_rect(ctx, NULL, x));
}

void pdf_dict_put_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key, fz_matrix x)
{
	pdf_dict_put_drop(ctx, dict, key, pdf_new_matrix(ctx, NULL, x));
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

int pdf_dict_get_bool(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_bool(ctx, pdf_dict_get(ctx, dict, key));
}

int pdf_dict_get_int(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_int(ctx, pdf_dict_get(ctx, dict, key));
}

float pdf_dict_get_real(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_real(ctx, pdf_dict_get(ctx, dict, key));
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

fz_rect pdf_dict_get_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_rect(ctx, pdf_dict_get(ctx, dict, key));
}

fz_matrix pdf_dict_get_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	return pdf_to_matrix(ctx, pdf_dict_get(ctx, dict, key));
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

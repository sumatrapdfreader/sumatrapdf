#include "fitz.h"

extern void fz_free_array(fz_obj *array);
extern void fz_free_dict(fz_obj *dict);

static fz_obj *fz_resolve_indirect_null(fz_obj *ref)
{
	return ref;
}

fz_obj* (*fz_resolve_indirect)(fz_obj*) = fz_resolve_indirect_null;

fz_obj *
fz_new_null(void)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_NULL;
	return o;
}

fz_obj *
fz_new_bool(int b)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_BOOL;
	o->u.b = b;
	return o;
}

fz_obj *
fz_new_int(int i)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_INT;
	o->u.i = i;
	return o;
}

fz_obj *
fz_new_real(float f)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_REAL;
	o->u.f = f;
	return o;
}

fz_obj *
fz_new_string(char *str, int len)
{
	fz_obj *o = fz_malloc(offsetof(fz_obj, u.s.buf) + len + 1);
	o->refs = 1;
	o->kind = FZ_STRING;
	o->u.s.len = len;
	memcpy(o->u.s.buf, str, len);
	o->u.s.buf[len] = '\0';
	return o;
}

fz_obj *
fz_new_name(char *str)
{
	fz_obj *o = fz_malloc(offsetof(fz_obj, u.n) + strlen(str) + 1);
	o->refs = 1;
	o->kind = FZ_NAME;
	strcpy(o->u.n, str);
	return o;
}

fz_obj *
fz_new_indirect(int num, int gen, void *xref)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_INDIRECT;
	o->u.r.num = num;
	o->u.r.gen = gen;
	o->u.r.xref = xref;
	return o;
}

fz_obj *
fz_keep_obj(fz_obj *o)
{
	assert(o != NULL);
	o->refs ++;
	return o;
}

void
fz_drop_obj(fz_obj *o)
{
	assert(o != NULL);
	if (--o->refs == 0)
	{
		if (o->kind == FZ_ARRAY)
			fz_free_array(o);
		else if (o->kind == FZ_DICT)
			fz_free_dict(o);
		else
			fz_free(o);
	}
}

int fz_is_indirect(fz_obj *obj)
{
	return obj ? obj->kind == FZ_INDIRECT : 0;
}

int fz_is_null(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	return obj ? obj->kind == FZ_NULL : 0;
}

int fz_is_bool(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	return obj ? obj->kind == FZ_BOOL : 0;
}

int fz_is_int(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	return obj ? obj->kind == FZ_INT : 0;
}

int fz_is_real(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	return obj ? obj->kind == FZ_REAL : 0;
}

int fz_is_string(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	return obj ? obj->kind == FZ_STRING : 0;
}

int fz_is_name(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	return obj ? obj->kind == FZ_NAME : 0;
}

int fz_is_array(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	return obj ? obj->kind == FZ_ARRAY : 0;
}

int fz_is_dict(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	return obj ? obj->kind == FZ_DICT : 0;
}

int fz_to_bool(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (fz_is_bool(obj))
		return obj->u.b;
	return 0;
}

int fz_to_int(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (fz_is_int(obj))
		return obj->u.i;
	if (fz_is_real(obj))
		return obj->u.f;
	return 0;
}

float fz_to_real(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (fz_is_real(obj))
		return obj->u.f;
	if (fz_is_int(obj))
		return obj->u.i;
	return 0;
}

char *fz_to_name(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (fz_is_name(obj))
		return obj->u.n;
	return "";
}

char *fz_to_str_buf(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (fz_is_string(obj))
		return obj->u.s.buf;
	return "";
}

int fz_to_str_len(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (fz_is_string(obj))
		return obj->u.s.len;
	return 0;
}

int fz_to_num(fz_obj *obj)
{
	if (fz_is_indirect(obj))
		return obj->u.r.num;
	return 0;
}

int fz_to_gen(fz_obj *obj)
{
	if (fz_is_indirect(obj))
		return obj->u.r.gen;
	return 0;
}

int
fz_objcmp(fz_obj *a, fz_obj *b)
{
	int i;

	if (a == b)
		return 0;

	if (!a || !b)
		return 1;

	if (a->kind != b->kind)
		return 1;

	switch (a->kind)
	{
	case FZ_NULL:
		return 0;

	case FZ_BOOL:
		return a->u.b - b->u.b;

	case FZ_INT:
		return a->u.i - b->u.i;

	case FZ_REAL:
		if (a->u.f < b->u.f)
			return -1;
		if (a->u.f > b->u.f)
			return 1;
		return 0;

	case FZ_STRING:
		if (a->u.s.len < b->u.s.len)
		{
			if (memcmp(a->u.s.buf, b->u.s.buf, a->u.s.len) <= 0)
				return -1;
			return 1;
		}
		if (a->u.s.len > b->u.s.len)
		{
			if (memcmp(a->u.s.buf, b->u.s.buf, b->u.s.len) >= 0)
				return 1;
			return -1;
		}
		return memcmp(a->u.s.buf, b->u.s.buf, a->u.s.len);

	case FZ_NAME:
		return strcmp(a->u.n, b->u.n);

	case FZ_INDIRECT:
		if (a->u.r.num == b->u.r.num)
			return a->u.r.gen - b->u.r.gen;
		return a->u.r.num - b->u.r.num;

	case FZ_ARRAY:
		if (a->u.a.len != b->u.a.len)
			return a->u.a.len - b->u.a.len;
		for (i = 0; i < a->u.a.len; i++)
			if (fz_objcmp(a->u.a.items[i], b->u.a.items[i]))
				return 1;
		return 0;

	case FZ_DICT:
		if (a->u.d.len != b->u.d.len)
			return a->u.d.len - b->u.d.len;
		for (i = 0; i < a->u.d.len; i++)
		{
			if (fz_objcmp(a->u.d.items[i].k, b->u.d.items[i].k))
				return 1;
			if (fz_objcmp(a->u.d.items[i].v, b->u.d.items[i].v))
				return 1;
		}
		return 0;

	}
	return 1;
}

char *fz_objkindstr(fz_obj *obj)
{
	if (obj == NULL)
		return "<NULL>";
	switch (obj->kind)
	{
	case FZ_NULL: return "null";
	case FZ_BOOL: return "boolean";
	case FZ_INT: return "integer";
	case FZ_REAL: return "real";
	case FZ_STRING: return "string";
	case FZ_NAME: return "name";
	case FZ_ARRAY: return "array";
	case FZ_DICT: return "dictionary";
	case FZ_INDIRECT: return "reference";
	}
	return "<unknown>";
}

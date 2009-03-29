#include "fitz_base.h"
#include "fitz_stream.h"

extern void fz_droparray(fz_obj *array);
extern void fz_dropdict(fz_obj *dict);

#define NEWOBJ(KIND,SIZE) \
	fz_obj *o; \
	o = *op = fz_malloc(SIZE); \
	if (!o) return fz_rethrow(-1, "out of memory: dynamic object"); \
	o->refs = 1; \
	o->kind = KIND;

fz_error
fz_newnull(fz_obj **op)
{
	NEWOBJ(FZ_NULL, sizeof (fz_obj));
	return fz_okay;
}

fz_error
fz_newbool(fz_obj **op, int b)
{
	NEWOBJ(FZ_BOOL, sizeof (fz_obj));
	o->u.b = b;
	return fz_okay;
}

fz_error
fz_newint(fz_obj **op, int i)
{
	NEWOBJ(FZ_INT, sizeof (fz_obj));
	o->u.i = i;
	return fz_okay;
}

fz_error
fz_newreal(fz_obj **op, float f)
{
	NEWOBJ(FZ_REAL, sizeof (fz_obj));
	o->u.f = f;
	return fz_okay;
}

fz_error
fz_newstring(fz_obj **op, char *str, int len)
{
	NEWOBJ(FZ_STRING, offsetof(fz_obj, u.s.buf) + len + 1);
	o->u.s.len = len;
	memcpy(o->u.s.buf, str, len);
	o->u.s.buf[len] = '\0';
	return fz_okay;
}

fz_error
fz_newname(fz_obj **op, char *str)
{
	NEWOBJ(FZ_NAME, offsetof(fz_obj, u.n) + strlen(str) + 1);
	strcpy(o->u.n, str);
	return fz_okay;
}

fz_error
fz_newindirect(fz_obj **op, int objid, int genid)
{
	NEWOBJ(FZ_INDIRECT, sizeof (fz_obj));
	o->u.r.oid = objid;
	o->u.r.gid = genid;
	return fz_okay;
}

fz_error
fz_newpointer(fz_obj **op, void *p)
{
	NEWOBJ(FZ_POINTER, sizeof (fz_obj));
	o->u.p = p;
	return fz_okay;
}

fz_obj *
fz_keepobj(fz_obj *o)
{
	assert(o != nil);
	o->refs ++;
	return o;
}

void
fz_dropobj(fz_obj *o)
{
	assert(o != nil);
	if (--o->refs == 0)
	{
		if (o->kind == FZ_ARRAY)
			fz_droparray(o);
		else if (o->kind == FZ_DICT)
			fz_dropdict(o);
		else
			fz_free(o);
	}
}

int
fz_isnull(fz_obj *obj)
{
	return obj ? obj->kind == FZ_NULL : 0;
}

int
fz_isbool(fz_obj *obj)
{
	return obj ? obj->kind == FZ_BOOL : 0;
}

int
fz_isint(fz_obj *obj)
{
	return obj ? obj->kind == FZ_INT : 0;
}

int
fz_isreal(fz_obj *obj)
{
	return obj ? obj->kind == FZ_REAL : 0;
}

int
fz_isstring(fz_obj *obj)
{
	return obj ? obj->kind == FZ_STRING : 0;
}

int
fz_isname(fz_obj *obj)
{
	return obj ? obj->kind == FZ_NAME : 0;
}

int
fz_isarray(fz_obj *obj)
{
	return obj ? obj->kind == FZ_ARRAY : 0;
}

int
fz_isdict(fz_obj *obj)
{
	return obj ? obj->kind == FZ_DICT : 0;
}

int
fz_isindirect(fz_obj *obj)
{
	return obj ? obj->kind == FZ_INDIRECT : 0;
}

int
fz_ispointer(fz_obj *obj)
{
	return obj ? obj->kind == FZ_POINTER : 0;
}

int
fz_tobool(fz_obj *obj)
{
	if (fz_isbool(obj))
		return obj->u.b;
	return 0;
}

int
fz_toint(fz_obj *obj)
{
	if (fz_isint(obj))
		return obj->u.i;
	if (fz_isreal(obj))
		return obj->u.f;
	return 0;
}

float
fz_toreal(fz_obj *obj)
{
	if (fz_isreal(obj))
		return obj->u.f;
	if (fz_isint(obj))
		return obj->u.i;
	return 0;
}

char *
fz_toname(fz_obj *obj)
{
	if (fz_isname(obj))
		return obj->u.n;
	return "";
}

char *
fz_tostrbuf(fz_obj *obj)
{
	if (fz_isstring(obj))
		return obj->u.s.buf;
	return "";
}

int
fz_tostrlen(fz_obj *obj)
{
	if (fz_isstring(obj))
		return obj->u.s.len;
	return 0;
}

int
fz_tonum(fz_obj *obj)
{
	if (fz_isindirect(obj))
		return obj->u.r.oid;
	return 0;
}

int
fz_togen(fz_obj *obj)
{
	if (fz_isindirect(obj))
		return obj->u.r.gid;
	return 0;
}

void *
fz_topointer(fz_obj *obj)
{
	if (fz_ispointer(obj))
		return obj->u.p;
	return fz_okay;
}

fz_error
fz_newnamefromstring(fz_obj **op, fz_obj *str)
{
	NEWOBJ(FZ_NAME, offsetof(fz_obj, u.n) + fz_tostrlen(str) + 1);
	memcpy(o->u.n, fz_tostrbuf(str), fz_tostrlen(str));
	o->u.n[fz_tostrlen(str)] = '\0';
	return fz_okay;
}

int
fz_objcmp(fz_obj *a, fz_obj *b)
{
	int i;

	if (a == b)
		return 0;
	if (a->kind != b->kind)
		return 1;

	switch (a->kind)
	{
	case FZ_NULL: return 0;
	case FZ_BOOL: return a->u.b - b->u.b;
	case FZ_INT: return a->u.i - b->u.i;
	case FZ_REAL: return a->u.f - b->u.f;

	case FZ_STRING:
		if (a->u.s.len != b->u.s.len)
			return a->u.s.len - b->u.s.len;
		return memcmp(a->u.s.buf, b->u.s.buf, a->u.s.len);

	case FZ_NAME:
		return strcmp(a->u.n, b->u.n);

	case FZ_INDIRECT:
		if (a->u.r.oid == b->u.r.oid)
			return a->u.r.gid - b->u.r.gid;
		return a->u.r.oid - b->u.r.oid;

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

	case FZ_POINTER:
		return (char *) a->u.p - (char *) b->u.p;
	}
	return 1;
}

char *fz_objkindstr(fz_obj *obj)
{
	if (obj == nil)
		return "<nil>";
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
	case FZ_POINTER: return "pointer";
	}
	return "<unknown>";
}


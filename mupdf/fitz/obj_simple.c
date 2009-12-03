#include "fitz.h"
#include "mupdf.h"

extern void fz_freearray(fz_obj *array);
extern void fz_freedict(fz_obj *dict);

fz_obj *
fz_newnull(void)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_NULL;
	return o;
}

fz_obj *
fz_newbool(int b)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_BOOL;
	o->u.b = b;
	return o;
}

fz_obj *
fz_newint(int i)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_INT;
	o->u.i = i;
	return o;
}

fz_obj *
fz_newreal(float f)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_REAL;
	o->u.f = f;
	return o;
}

fz_obj *
fz_newstring(char *str, int len)
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
fz_newname(char *str)
{
	fz_obj *o = fz_malloc(offsetof(fz_obj, u.n) + strlen(str) + 1);
	o->refs = 1;
	o->kind = FZ_NAME;
	strcpy(o->u.n, str);
	return o;
}

fz_obj *
fz_newindirect(int num, int gen, pdf_xref *xref)
{
	fz_obj *o = fz_malloc(sizeof(fz_obj));
	o->refs = 1;
	o->kind = FZ_INDIRECT;
	o->u.r.num = num;
	o->u.r.gen = gen;
	o->u.r.xref = xref;
	o->u.r.obj = nil;
	return o;
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
		{
			fz_freearray(o);
		}
		else if (o->kind == FZ_DICT)
		{
			fz_freedict(o);
		}
		else if (o->kind == FZ_INDIRECT)
		{
			if (o->u.r.obj)
				fz_dropobj(o->u.r.obj);
			fz_free(o);
		}
		else
		{
			fz_free(o);
		}
	}
}

int fz_isindirect(fz_obj *obj)
{
	return obj ? obj->kind == FZ_INDIRECT : 0;
}

int fz_isnull(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	return obj ? obj->kind == FZ_NULL : 0;
}

int fz_isbool(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	return obj ? obj->kind == FZ_BOOL : 0;
}

int fz_isint(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	return obj ? obj->kind == FZ_INT : 0;
}

int fz_isreal(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	return obj ? obj->kind == FZ_REAL : 0;
}

int fz_isstring(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	return obj ? obj->kind == FZ_STRING : 0;
}

int fz_isname(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	return obj ? obj->kind == FZ_NAME : 0;
}

int fz_isarray(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	return obj ? obj->kind == FZ_ARRAY : 0;
}

int fz_isdict(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	return obj ? obj->kind == FZ_DICT : 0;
}

int fz_tobool(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (fz_isbool(obj))
		return obj->u.b;
	return 0;
}

int fz_toint(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (fz_isint(obj))
		return obj->u.i;
	if (fz_isreal(obj))
		return obj->u.f;
	return 0;
}

float fz_toreal(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (fz_isreal(obj))
		return obj->u.f;
	if (fz_isint(obj))
		return obj->u.i;
	return 0;
}

char *fz_toname(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (fz_isname(obj))
		return obj->u.n;
	return "";
}

char *fz_tostrbuf(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (fz_isstring(obj))
		return obj->u.s.buf;
	return "";
}

int fz_tostrlen(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (fz_isstring(obj))
		return obj->u.s.len;
	return 0;
}

int fz_tonum(fz_obj *obj)
{
	if (fz_isindirect(obj))
		return obj->u.r.num;
	return 0;
}

int fz_togen(fz_obj *obj)
{
	if (fz_isindirect(obj))
		return obj->u.r.gen;
	return 0;
}

fz_obj *fz_resolveindirect(fz_obj *ref)
{
	int error;

	if (fz_isindirect(ref))
	{
		if (!ref->u.r.obj && ref->u.r.xref)
		{
			error = pdf_loadobject(&ref->u.r.obj, ref->u.r.xref, fz_tonum(ref), fz_togen(ref));
			if (error)
			{
				fz_catch(error, "cannot resolve reference (%d %d R); ignoring error", fz_tonum(ref), fz_togen(ref));
				ref->u.r.obj = fz_keepobj(ref);
			}
		}
		return ref->u.r.obj;
	}

	return ref;
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
	}
	return "<unknown>";
}


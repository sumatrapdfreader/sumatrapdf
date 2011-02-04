#include "fitz.h"

fz_obj *
fz_newarray(int initialcap)
{
	fz_obj *obj;
	int i;

	obj = fz_malloc(sizeof(fz_obj));
	obj->refs = 1;
	obj->kind = FZ_ARRAY;

	obj->u.a.len = 0;
	obj->u.a.cap = initialcap > 1 ? initialcap : 6;

	obj->u.a.items = fz_calloc(obj->u.a.cap, sizeof(fz_obj*));
	for (i = 0; i < obj->u.a.cap; i++)
		obj->u.a.items[i] = nil;

	return obj;
}

fz_obj *
fz_copyarray(fz_obj *obj)
{
	fz_obj *new;
	int i;

	if (fz_isindirect(obj) || !fz_isarray(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));

	new = fz_newarray(fz_arraylen(obj));
	for (i = 0; i < fz_arraylen(obj); i++)
		fz_arraypush(new, fz_arrayget(obj, i));

	return new;
}

int
fz_arraylen(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (!fz_isarray(obj))
		return 0;
	return obj->u.a.len;
}

fz_obj *
fz_arrayget(fz_obj *obj, int i)
{
	obj = fz_resolveindirect(obj);

	if (!fz_isarray(obj))
		return nil;

	if (i < 0 || i >= obj->u.a.len)
		return nil;

	return obj->u.a.items[i];
}

void
fz_arrayput(fz_obj *obj, int i, fz_obj *item)
{
	obj = fz_resolveindirect(obj);

	if (!fz_isarray(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));
	else if (i < 0)
		fz_warn("assert: index %d < 0", i);
	else if (i >= obj->u.a.len)
		fz_warn("assert: index %d > length %d", i, obj->u.a.len);
	else
	{
		if (obj->u.a.items[i])
			fz_dropobj(obj->u.a.items[i]);
		obj->u.a.items[i] = fz_keepobj(item);
	}
}

void
fz_arraypush(fz_obj *obj, fz_obj *item)
{
	obj = fz_resolveindirect(obj);

	if (!fz_isarray(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));
	else
	{
		if (obj->u.a.len + 1 > obj->u.a.cap)
		{
			int i;
			obj->u.a.cap = (obj->u.a.cap * 3) / 2;
			obj->u.a.items = fz_realloc(obj->u.a.items, obj->u.a.cap, sizeof(fz_obj*));
			for (i = obj->u.a.len ; i < obj->u.a.cap; i++)
				obj->u.a.items[i] = nil;
		}
		obj->u.a.items[obj->u.a.len] = fz_keepobj(item);
		obj->u.a.len++;
	}
}

void
fz_arraydrop(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);

	if (!fz_isarray(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));
	else
	{
		if (obj->u.a.len > 0)
		{
			fz_dropobj(obj->u.a.items[--obj->u.a.len]);
		}
	}
}

void
fz_arrayinsert(fz_obj *obj, fz_obj *item)
{
	obj = fz_resolveindirect(obj);

	if (!fz_isarray(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));
	else
	{
		if (obj->u.a.len + 1 > obj->u.a.cap)
		{
			int i;
			obj->u.a.cap = (obj->u.a.cap * 3) / 2;
			obj->u.a.items = fz_realloc(obj->u.a.items, obj->u.a.cap, sizeof(fz_obj*));
			for (i = obj->u.a.len ; i < obj->u.a.cap; i++)
				obj->u.a.items[i] = nil;
		}
		memmove(obj->u.a.items + 1, obj->u.a.items, obj->u.a.len * sizeof(fz_obj*));
		obj->u.a.items[0] = fz_keepobj(item);
		obj->u.a.len++;
	}
}

void
fz_freearray(fz_obj *obj)
{
	int i;

	assert(obj->kind == FZ_ARRAY);

	for (i = 0; i < obj->u.a.len; i++)
		if (obj->u.a.items[i])
			fz_dropobj(obj->u.a.items[i]);

	fz_free(obj->u.a.items);
	fz_free(obj);
}

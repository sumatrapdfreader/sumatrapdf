#include "fitz.h"

fz_obj *
fz_new_array(int initialcap)
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
		obj->u.a.items[i] = NULL;

	return obj;
}

fz_obj *
fz_copy_array(fz_obj *obj)
{
	fz_obj *new;
	int i;

	if (fz_is_indirect(obj) || !fz_is_array(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));

	new = fz_new_array(fz_array_len(obj));
	for (i = 0; i < fz_array_len(obj); i++)
		fz_array_push(new, fz_array_get(obj, i));

	return new;
}

int
fz_array_len(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (!fz_is_array(obj))
		return 0;
	return obj->u.a.len;
}

fz_obj *
fz_array_get(fz_obj *obj, int i)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_array(obj))
		return NULL;

	if (i < 0 || i >= obj->u.a.len)
		return NULL;

	return obj->u.a.items[i];
}

void
fz_array_put(fz_obj *obj, int i, fz_obj *item)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_array(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));
	else if (i < 0)
		fz_warn("assert: index %d < 0", i);
	else if (i >= obj->u.a.len)
		fz_warn("assert: index %d > length %d", i, obj->u.a.len);
	else
	{
		if (obj->u.a.items[i])
			fz_drop_obj(obj->u.a.items[i]);
		obj->u.a.items[i] = fz_keep_obj(item);
	}
}

void
fz_array_push(fz_obj *obj, fz_obj *item)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_array(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));
	else
	{
		if (obj->u.a.len + 1 > obj->u.a.cap)
		{
			int i;
			obj->u.a.cap = (obj->u.a.cap * 3) / 2;
			obj->u.a.items = fz_realloc(obj->u.a.items, obj->u.a.cap, sizeof(fz_obj*));
			for (i = obj->u.a.len ; i < obj->u.a.cap; i++)
				obj->u.a.items[i] = NULL;
		}
		obj->u.a.items[obj->u.a.len] = fz_keep_obj(item);
		obj->u.a.len++;
	}
}

void
fz_array_drop(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_array(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));
	else
	{
		if (obj->u.a.len > 0)
		{
			fz_drop_obj(obj->u.a.items[--obj->u.a.len]);
		}
	}
}

void
fz_array_insert(fz_obj *obj, fz_obj *item)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_array(obj))
		fz_warn("assert: not an array (%s)", fz_objkindstr(obj));
	else
	{
		if (obj->u.a.len + 1 > obj->u.a.cap)
		{
			int i;
			obj->u.a.cap = (obj->u.a.cap * 3) / 2;
			obj->u.a.items = fz_realloc(obj->u.a.items, obj->u.a.cap, sizeof(fz_obj*));
			for (i = obj->u.a.len ; i < obj->u.a.cap; i++)
				obj->u.a.items[i] = NULL;
		}
		memmove(obj->u.a.items + 1, obj->u.a.items, obj->u.a.len * sizeof(fz_obj*));
		obj->u.a.items[0] = fz_keep_obj(item);
		obj->u.a.len++;
	}
}

void
fz_free_array(fz_obj *obj)
{
	int i;

	assert(obj->kind == FZ_ARRAY);

	for (i = 0; i < obj->u.a.len; i++)
		if (obj->u.a.items[i])
			fz_drop_obj(obj->u.a.items[i]);

	fz_free(obj->u.a.items);
	fz_free(obj);
}

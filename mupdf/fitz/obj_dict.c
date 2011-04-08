#include "fitz.h"

/* dicts may only have names as keys! */

static int keyvalcmp(const void *ap, const void *bp)
{
	const fz_keyval *a = ap;
	const fz_keyval *b = bp;
	return strcmp(fz_to_name(a->k), fz_to_name(b->k));
}

fz_obj *
fz_new_dict(int initialcap)
{
	fz_obj *obj;
	int i;

	obj = fz_malloc(sizeof(fz_obj));
	obj->refs = 1;
	obj->kind = FZ_DICT;

	obj->u.d.sorted = 1;
	obj->u.d.len = 0;
	obj->u.d.cap = initialcap > 1 ? initialcap : 10;

	obj->u.d.items = fz_calloc(obj->u.d.cap, sizeof(fz_keyval));
	for (i = 0; i < obj->u.d.cap; i++)
	{
		obj->u.d.items[i].k = NULL;
		obj->u.d.items[i].v = NULL;
	}

	return obj;
}

fz_obj *
fz_copy_dict(fz_obj *obj)
{
	fz_obj *new;
	int i;

	if (fz_is_indirect(obj) || !fz_is_dict(obj))
		fz_throw("assert: not a dict (%s)", fz_objkindstr(obj));

	new = fz_new_dict(fz_dict_len(obj));
	for (i = 0; i < fz_dict_len(obj); i++)
		fz_dict_put(new, fz_dict_get_key(obj, i), fz_dict_get_val(obj, i));

	return new;
}

int
fz_dict_len(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (!fz_is_dict(obj))
		return 0;
	return obj->u.d.len;
}

fz_obj *
fz_dict_get_key(fz_obj *obj, int i)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_dict(obj))
		return NULL;

	if (i < 0 || i >= obj->u.d.len)
		return NULL;

	return obj->u.d.items[i].k;
}

fz_obj *
fz_dict_get_val(fz_obj *obj, int i)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_dict(obj))
		return NULL;

	if (i < 0 || i >= obj->u.d.len)
		return NULL;

	return obj->u.d.items[i].v;
}

static int
fz_dict_finds(fz_obj *obj, char *key)
{
	if (obj->u.d.sorted)
	{
		int l = 0;
		int r = obj->u.d.len - 1;
		while (l <= r)
		{
			int m = (l + r) >> 1;
			int c = -strcmp(fz_to_name(obj->u.d.items[m].k), key);
			if (c < 0)
				r = m - 1;
			else if (c > 0)
				l = m + 1;
			else
				return m;
		}
	}

	else
	{
		int i;
		for (i = 0; i < obj->u.d.len; i++)
			if (strcmp(fz_to_name(obj->u.d.items[i].k), key) == 0)
				return i;
	}

	return -1;
}

fz_obj *
fz_dict_gets(fz_obj *obj, char *key)
{
	int i;

	obj = fz_resolve_indirect(obj);

	if (!fz_is_dict(obj))
		return NULL;

	i = fz_dict_finds(obj, key);
	if (i >= 0)
		return obj->u.d.items[i].v;

	return NULL;
}

fz_obj *
fz_dict_get(fz_obj *obj, fz_obj *key)
{
	if (fz_is_name(key))
		return fz_dict_gets(obj, fz_to_name(key));
	return NULL;
}

fz_obj *
fz_dict_getsa(fz_obj *obj, char *key, char *abbrev)
{
	fz_obj *v;
	v = fz_dict_gets(obj, key);
	if (v)
		return v;
	return fz_dict_gets(obj, abbrev);
}

void
fz_dict_put(fz_obj *obj, fz_obj *key, fz_obj *val)
{
	char *s;
	int i;

	obj = fz_resolve_indirect(obj);

	if (!fz_is_dict(obj))
	{
		fz_warn("assert: not a dict (%s)", fz_objkindstr(obj));
		return;
	}

	if (fz_is_name(key))
		s = fz_to_name(key);
	else
	{
		fz_warn("assert: key is not a name (%s)", fz_objkindstr(obj));
		return;
	}

	if (!val)
	{
		fz_warn("assert: val does not exist for key (%s)", s);
		return;
	}

	i = fz_dict_finds(obj, s);
	if (i >= 0)
	{
		fz_drop_obj(obj->u.d.items[i].v);
		obj->u.d.items[i].v = fz_keep_obj(val);
		return;
	}

	if (obj->u.d.len + 1 > obj->u.d.cap)
	{
		obj->u.d.cap = (obj->u.d.cap * 3) / 2;
		obj->u.d.items = fz_realloc(obj->u.d.items, obj->u.d.cap, sizeof(fz_keyval));
		for (i = obj->u.d.len; i < obj->u.d.cap; i++)
		{
			obj->u.d.items[i].k = NULL;
			obj->u.d.items[i].v = NULL;
		}
	}

	/* borked! */
	if (obj->u.d.len)
		if (strcmp(fz_to_name(obj->u.d.items[obj->u.d.len - 1].k), s) > 0)
			obj->u.d.sorted = 0;

	obj->u.d.items[obj->u.d.len].k = fz_keep_obj(key);
	obj->u.d.items[obj->u.d.len].v = fz_keep_obj(val);
	obj->u.d.len ++;
}

void
fz_dict_puts(fz_obj *obj, char *key, fz_obj *val)
{
	fz_obj *keyobj = fz_new_name(key);
	fz_dict_put(obj, keyobj, val);
	fz_drop_obj(keyobj);
}

void
fz_dict_dels(fz_obj *obj, char *key)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_dict(obj))
		fz_warn("assert: not a dict (%s)", fz_objkindstr(obj));
	else
	{
		int i = fz_dict_finds(obj, key);
		if (i >= 0)
		{
			fz_drop_obj(obj->u.d.items[i].k);
			fz_drop_obj(obj->u.d.items[i].v);
			obj->u.d.sorted = 0;
			obj->u.d.items[i] = obj->u.d.items[obj->u.d.len-1];
			obj->u.d.len --;
		}
	}
}

void
fz_dict_del(fz_obj *obj, fz_obj *key)
{
	if (fz_is_name(key))
		fz_dict_dels(obj, fz_to_name(key));
	else
		fz_warn("assert: key is not a name (%s)", fz_objkindstr(obj));
}

void
fz_free_dict(fz_obj *obj)
{
	int i;

	obj = fz_resolve_indirect(obj);

	if (!fz_is_dict(obj))
		return;

	for (i = 0; i < obj->u.d.len; i++) {
		if (obj->u.d.items[i].k)
			fz_drop_obj(obj->u.d.items[i].k);
		if (obj->u.d.items[i].v)
			fz_drop_obj(obj->u.d.items[i].v);
	}

	fz_free(obj->u.d.items);
	fz_free(obj);
}

void
fz_sort_dict(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (!fz_is_dict(obj))
		return;
	if (!obj->u.d.sorted)
	{
		qsort(obj->u.d.items, obj->u.d.len, sizeof(fz_keyval), keyvalcmp);
		obj->u.d.sorted = 1;
	}
}

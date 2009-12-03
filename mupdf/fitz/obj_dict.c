#include "fitz_base.h"
#include "fitz_stream.h"

/* dicts may only have names as keys! */

static int keyvalcmp(const void *ap, const void *bp)
{
	const fz_keyval *a = ap;
	const fz_keyval *b = bp;
	if (fz_isname(a->k) && fz_isname(b->k))
		return strcmp(fz_toname(a->k), fz_toname(b->k));
	return -1;
}

static inline int keystrcmp(fz_obj *key, char *s)
{
	if (fz_isname(key))
		return strcmp(fz_toname(key), s);
	return -1;
}

fz_obj *
fz_newdict(int initialcap)
{
	fz_obj *obj;
	int i;

	obj = fz_malloc(sizeof (fz_obj));
	obj->refs = 1;
	obj->kind = FZ_DICT;

	obj->u.d.sorted = 1;
	obj->u.d.len = 0;
	obj->u.d.cap = initialcap > 0 ? initialcap : 10;

	obj->u.d.items = fz_malloc(sizeof(fz_keyval) * obj->u.d.cap);
	for (i = 0; i < obj->u.d.cap; i++)
	{
		obj->u.d.items[i].k = nil;
		obj->u.d.items[i].v = nil;
	}

	return obj;
}

fz_obj *
fz_copydict(fz_obj *obj)
{
	fz_obj *new;
	int i;

	if (!fz_isdict(obj))
		fz_throw("assert: not a dict (%s)", fz_objkindstr(obj));

	new = fz_newdict(obj->u.d.cap);
	for (i = 0; i < fz_dictlen(obj); i++)
		fz_dictput(new, fz_dictgetkey(obj, i), fz_dictgetval(obj, i));

	return new;
}

int
fz_dictlen(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (!fz_isdict(obj))
		return 0;
	return obj->u.d.len;
}

fz_obj *
fz_dictgetkey(fz_obj *obj, int i)
{
	obj = fz_resolveindirect(obj);

	if (!fz_isdict(obj))
		return nil;

	if (i < 0 || i >= obj->u.d.len)
		return nil;

	return obj->u.d.items[i].k;
}

fz_obj *
fz_dictgetval(fz_obj *obj, int i)
{
	obj = fz_resolveindirect(obj);

	if (!fz_isdict(obj))
		return nil;

	if (i < 0 || i >= obj->u.d.len)
		return nil;

	return obj->u.d.items[i].v;
}

static inline int
fz_dictfinds(fz_obj *obj, char *key)
{
	if (obj->u.d.sorted)
	{
		int l = 0;
		int r = obj->u.d.len - 1;
		while (l <= r)
		{
			int m = (l + r) >> 1;
			int c = -keystrcmp(obj->u.d.items[m].k, key);
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
			if (keystrcmp(obj->u.d.items[i].k, key) == 0)
				return i;
	}

	return -1;
}

fz_obj *
fz_dictgets(fz_obj *obj, char *key)
{
	int i;

	obj = fz_resolveindirect(obj);

	if (!fz_isdict(obj))
		return nil;

	i = fz_dictfinds(obj, key);
	if (i >= 0)
		return obj->u.d.items[i].v;

	return nil;
}

fz_obj *
fz_dictget(fz_obj *obj, fz_obj *key)
{
	if (fz_isname(key))
		return fz_dictgets(obj, fz_toname(key));
	return nil;
}

fz_obj *
fz_dictgetsa(fz_obj *obj, char *key, char *abbrev)
{
	fz_obj *v;
	v = fz_dictgets(obj, key);
	if (v)
		return v;
	return fz_dictgets(obj, abbrev);
}

void
fz_dictput(fz_obj *obj, fz_obj *key, fz_obj *val)
{
	char *s;
	int i;

	obj = fz_resolveindirect(obj);

	if (!fz_isdict(obj))
	{
		fz_warn("assert: not a dict (%s)", fz_objkindstr(obj));
		return;
	}

	if (fz_isname(key))
		s = fz_toname(key);
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

	i = fz_dictfinds(obj, s);
	if (i >= 0)
	{
		fz_dropobj(obj->u.d.items[i].v);
		obj->u.d.items[i].v = fz_keepobj(val);
		return;
	}

	if (obj->u.d.len + 1 > obj->u.d.cap)
	{
		obj->u.d.cap = (obj->u.d.cap * 3) / 2;
		obj->u.d.items = fz_realloc(obj->u.d.items, sizeof(fz_keyval) * obj->u.d.cap);
		for (i = obj->u.d.len; i < obj->u.d.cap; i++)
		{
			obj->u.d.items[i].k = nil;
			obj->u.d.items[i].v = nil;
		}
	}

	/* borked! */
	if (obj->u.d.len)
		if (keystrcmp(obj->u.d.items[obj->u.d.len - 1].k, s) > 0)
			obj->u.d.sorted = 0;

	obj->u.d.items[obj->u.d.len].k = fz_keepobj(key);
	obj->u.d.items[obj->u.d.len].v = fz_keepobj(val);
	obj->u.d.len ++;
}

void
fz_dictputs(fz_obj *obj, char *key, fz_obj *val)
{
	fz_obj *keyobj = fz_newname(key);
	fz_dictput(obj, keyobj, val);
	fz_dropobj(keyobj);
}

void
fz_dictdels(fz_obj *obj, char *key)
{
	obj = fz_resolveindirect(obj);

	if (!fz_isdict(obj))
		fz_warn("assert: not a dict (%s)", fz_objkindstr(obj));
	else
	{
		int i = fz_dictfinds(obj, key);
		if (i >= 0)
		{
			fz_dropobj(obj->u.d.items[i].k);
			fz_dropobj(obj->u.d.items[i].v);
			obj->u.d.sorted = 0;
			obj->u.d.items[i] = obj->u.d.items[obj->u.d.len-1];
			obj->u.d.len --;
		}
	}
}

void
fz_dictdel(fz_obj *obj, fz_obj *key)
{
	if (fz_isname(key))
		fz_dictdels(obj, fz_toname(key));
	else
		fz_warn("assert: key is not a name (%s)", fz_objkindstr(obj));
}

void
fz_freedict(fz_obj *obj)
{
	int i;

	obj = fz_resolveindirect(obj);

	if (!fz_isdict(obj))
		return;

	for (i = 0; i < obj->u.d.len; i++) {
		if (obj->u.d.items[i].k)
			fz_dropobj(obj->u.d.items[i].k);
		if (obj->u.d.items[i].v)
			fz_dropobj(obj->u.d.items[i].v);
	}

	fz_free(obj->u.d.items);
	fz_free(obj);
}

void
fz_sortdict(fz_obj *obj)
{
	obj = fz_resolveindirect(obj);
	if (!fz_isdict(obj))
		return;
	if (!obj->u.d.sorted)
	{
		qsort(obj->u.d.items, obj->u.d.len, sizeof(fz_keyval), keyvalcmp);
		obj->u.d.sorted = 1;
	}
}


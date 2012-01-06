#include "fitz.h"

typedef enum fz_objkind_e
{
	FZ_NULL,
	FZ_BOOL,
	FZ_INT,
	FZ_REAL,
	FZ_STRING,
	FZ_NAME,
	FZ_ARRAY,
	FZ_DICT,
	FZ_INDIRECT
} fz_objkind;

struct keyval
{
	fz_obj *k;
	fz_obj *v;
};

struct fz_obj_s
{
	int refs;
	fz_objkind kind;
	fz_context *ctx;
	union
	{
		int b;
		int i;
		float f;
		struct {
			unsigned short len;
			char buf[1];
		} s;
		char n[1];
		struct {
			int len;
			int cap;
			fz_obj **items;
		} a;
		struct {
			char sorted;
			char marked;
			int len;
			int cap;
			struct keyval *items;
		} d;
		struct {
			int num;
			int gen;
			struct pdf_xref_s *xref;
		} r;
	} u;
};

fz_obj *
fz_new_null(fz_context *ctx)
{
	fz_obj *obj;
	obj = Memento_label(fz_malloc(ctx, sizeof(fz_obj)), "fz_obj(null)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_NULL;
	return obj;
}

fz_obj *
fz_new_bool(fz_context *ctx, int b)
{
	fz_obj *obj;
	obj = Memento_label(fz_malloc(ctx, sizeof(fz_obj)), "fz_obj(bool)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_BOOL;
	obj->u.b = b;
	return obj;
}

fz_obj *
fz_new_int(fz_context *ctx, int i)
{
	fz_obj *obj;
	obj = Memento_label(fz_malloc(ctx, sizeof(fz_obj)), "fz_obj(int)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_INT;
	obj->u.i = i;
	return obj;
}

fz_obj *
fz_new_real(fz_context *ctx, float f)
{
	fz_obj *obj;
	obj = Memento_label(fz_malloc(ctx, sizeof(fz_obj)), "fz_obj(real)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_REAL;
	obj->u.f = f;
	return obj;
}

fz_obj *
fz_new_string(fz_context *ctx, char *str, int len)
{
	fz_obj *obj;
	obj = Memento_label(fz_malloc(ctx, offsetof(fz_obj, u.s.buf) + len + 1), "fz_obj(string)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_STRING;
	obj->u.s.len = len;
	memcpy(obj->u.s.buf, str, len);
	obj->u.s.buf[len] = '\0';
	return obj;
}

fz_obj *
fz_new_name(fz_context *ctx, char *str)
{
	fz_obj *obj;
	obj = Memento_label(fz_malloc(ctx, offsetof(fz_obj, u.n) + strlen(str) + 1), "fz_obj(name)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_NAME;
	strcpy(obj->u.n, str);
	return obj;
}

fz_obj *
fz_new_indirect(fz_context *ctx, int num, int gen, void *xref)
{
	fz_obj *obj;
	obj = Memento_label(fz_malloc(ctx, sizeof(fz_obj)), "fz_obj(indirect)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_INDIRECT;
	obj->u.r.num = num;
	obj->u.r.gen = gen;
	obj->u.r.xref = xref;
	return obj;
}

fz_obj *
fz_keep_obj(fz_obj *obj)
{
	assert(obj);
	obj->refs ++;
	return obj;
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
		return (int)(obj->u.f + 0.5f); /* No roundf in MSVC */
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

/* for use by pdf_crypt_obj_imp to decrypt AES string in place */
void fz_set_str_len(fz_obj *obj, int newlen)
{
	obj = fz_resolve_indirect(obj);
	if (fz_is_string(obj))
		if (newlen < obj->u.s.len)
			obj->u.s.len = newlen;
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

void *fz_get_indirect_xref(fz_obj *obj)
{
	if (fz_is_indirect(obj))
		return obj->u.r.xref;
	return NULL;
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

static char *
fz_objkindstr(fz_obj *obj)
{
	if (!obj)
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

fz_obj *
fz_new_array(fz_context *ctx, int initialcap)
{
	fz_obj *obj;
	int i;

	obj = Memento_label(fz_malloc(ctx, sizeof(fz_obj)), "fz_obj(array)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_ARRAY;

	obj->u.a.len = 0;
	obj->u.a.cap = initialcap > 1 ? initialcap : 6;

	fz_try(ctx)
	{
		obj->u.a.items = Memento_label(fz_malloc_array(ctx, obj->u.a.cap, sizeof(fz_obj*)), "fz_obj(array items)");
	}
	fz_catch(ctx)
	{
		fz_free(ctx, obj);
		fz_rethrow(ctx);
	}
	for (i = 0; i < obj->u.a.cap; i++)
		obj->u.a.items[i] = NULL;

	return obj;
}

static void
fz_array_grow(fz_obj *obj)
{
	int i;

	obj->u.a.cap = (obj->u.a.cap * 3) / 2;
	obj->u.a.items = fz_resize_array(obj->ctx, obj->u.a.items, obj->u.a.cap, sizeof(fz_obj*));

	for (i = obj->u.a.len ; i < obj->u.a.cap; i++)
		obj->u.a.items[i] = NULL;
}

fz_obj *
fz_copy_array(fz_context *ctx, fz_obj *obj)
{
	fz_obj *new;
	int i;
	int n;

	if (fz_is_indirect(obj) || !fz_is_array(obj))
		fz_warn(obj->ctx, "assert: not an array (%s)", fz_objkindstr(obj));

	new = fz_new_array(ctx, fz_array_len(obj));
	n = fz_array_len(obj);
	for (i = 0; i < n; i++)
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
		fz_warn(obj->ctx, "assert: not an array (%s)", fz_objkindstr(obj));
	else if (i < 0)
		fz_warn(obj->ctx, "assert: index %d < 0", i);
	else if (i >= obj->u.a.len)
		fz_warn(obj->ctx, "assert: index %d > length %d", i, obj->u.a.len);
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
		fz_warn(obj->ctx, "assert: not an array (%s)", fz_objkindstr(obj));
	else
	{
		if (obj->u.a.len + 1 > obj->u.a.cap)
			fz_array_grow(obj);
		obj->u.a.items[obj->u.a.len] = fz_keep_obj(item);
		obj->u.a.len++;
	}
}

void
fz_array_insert(fz_obj *obj, fz_obj *item)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_array(obj))
		fz_warn(obj->ctx, "assert: not an array (%s)", fz_objkindstr(obj));
	else
	{
		if (obj->u.a.len + 1 > obj->u.a.cap)
			fz_array_grow(obj);
		memmove(obj->u.a.items + 1, obj->u.a.items, obj->u.a.len * sizeof(fz_obj*));
		obj->u.a.items[0] = fz_keep_obj(item);
		obj->u.a.len++;
	}
}

int
fz_array_contains(fz_obj *arr, fz_obj *obj)
{
	int i;

	for (i = 0; i < fz_array_len(arr); i++)
		if (!fz_objcmp(fz_array_get(arr, i), obj))
			return 1;

	return 0;
}

/* dicts may only have names as keys! */

static int keyvalcmp(const void *ap, const void *bp)
{
	const struct keyval *a = ap;
	const struct keyval *b = bp;
	return strcmp(fz_to_name(a->k), fz_to_name(b->k));
}

fz_obj *
fz_new_dict(fz_context *ctx, int initialcap)
{
	fz_obj *obj;
	int i;

	obj = Memento_label(fz_malloc(ctx, sizeof(fz_obj)), "fz_obj(dict)");
	obj->ctx = ctx;
	obj->refs = 1;
	obj->kind = FZ_DICT;

	obj->u.d.sorted = 0;
	obj->u.d.marked = 0;
	obj->u.d.len = 0;
	obj->u.d.cap = initialcap > 1 ? initialcap : 10;

	fz_try(ctx)
	{
		obj->u.d.items = Memento_label(fz_malloc_array(ctx, obj->u.d.cap, sizeof(struct keyval)), "fz_obj(dict items)");
	}
	fz_catch(ctx)
	{
		fz_free(ctx, obj);
		fz_rethrow(ctx);
	}
	for (i = 0; i < obj->u.d.cap; i++)
	{
		obj->u.d.items[i].k = NULL;
		obj->u.d.items[i].v = NULL;
	}

	return obj;
}

static void
fz_dict_grow(fz_obj *obj)
{
	int i;

	obj->u.d.cap = (obj->u.d.cap * 3) / 2;
	obj->u.d.items = fz_resize_array(obj->ctx, obj->u.d.items, obj->u.d.cap, sizeof(struct keyval));

	for (i = obj->u.d.len; i < obj->u.d.cap; i++)
	{
		obj->u.d.items[i].k = NULL;
		obj->u.d.items[i].v = NULL;
	}
}

fz_obj *
fz_copy_dict(fz_context *ctx, fz_obj *obj)
{
	fz_obj *new;
	int i, n;

	if (fz_is_indirect(obj) || !fz_is_dict(obj))
		fz_warn(ctx, "assert: not a dict (%s)", fz_objkindstr(obj));

	n = fz_dict_len(obj);
	new = fz_new_dict(ctx, n);
	for (i = 0; i < n; i++)
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
fz_dict_finds(fz_obj *obj, char *key, int *location)
{
	if (obj->u.d.sorted && obj->u.d.len > 0)
	{
		int l = 0;
		int r = obj->u.d.len - 1;

		if (strcmp(fz_to_name(obj->u.d.items[r].k), key) < 0)
		{
			if (location)
				*location = r + 1;
			return -1;
		}

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

			if (location)
				*location = l;
		}
	}

	else
	{
		int i;
		for (i = 0; i < obj->u.d.len; i++)
			if (strcmp(fz_to_name(obj->u.d.items[i].k), key) == 0)
				return i;

		if (location)
			*location = obj->u.d.len;
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

	i = fz_dict_finds(obj, key, NULL);
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
	int location;
	char *s;
	int i;

	obj = fz_resolve_indirect(obj);

	if (!fz_is_dict(obj))
	{
		fz_warn(obj->ctx, "assert: not a dict (%s)", fz_objkindstr(obj));
		return;
	}

	if (fz_is_name(key))
		s = fz_to_name(key);
	else
	{
		fz_warn(obj->ctx, "assert: key is not a name (%s)", fz_objkindstr(obj));
		return;
	}

	if (!val)
	{
		fz_warn(obj->ctx, "assert: val does not exist for key (%s)", s);
		return;
	}

	if (obj->u.d.len > 100 && !obj->u.d.sorted)
		fz_sort_dict(obj);

	i = fz_dict_finds(obj, s, &location);
	if (i >= 0 && i < obj->u.d.len)
	{
		fz_drop_obj(obj->u.d.items[i].v);
		obj->u.d.items[i].v = fz_keep_obj(val);
	}
	else
	{
		if (obj->u.d.len + 1 > obj->u.d.cap)
			fz_dict_grow(obj);

		i = location;
		if (obj->u.d.sorted && obj->u.d.len > 0)
			memmove(&obj->u.d.items[i + 1],
				&obj->u.d.items[i],
				(obj->u.d.len - i) * sizeof(struct keyval));

		obj->u.d.items[i].k = fz_keep_obj(key);
		obj->u.d.items[i].v = fz_keep_obj(val);
		obj->u.d.len ++;
	}
}

void
fz_dict_puts(fz_obj *obj, char *key, fz_obj *val)
{
	fz_obj *keyobj = fz_new_name(obj->ctx, key);
	fz_dict_put(obj, keyobj, val);
	fz_drop_obj(keyobj);
}

void
fz_dict_dels(fz_obj *obj, char *key)
{
	obj = fz_resolve_indirect(obj);

	if (!fz_is_dict(obj))
		fz_warn(obj->ctx, "assert: not a dict (%s)", fz_objkindstr(obj));
	else
	{
		int i = fz_dict_finds(obj, key, NULL);
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
		fz_warn(obj->ctx, "assert: key is not a name (%s)", fz_objkindstr(obj));
}

void
fz_sort_dict(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (!fz_is_dict(obj))
		return;
	if (!obj->u.d.sorted)
	{
		qsort(obj->u.d.items, obj->u.d.len, sizeof(struct keyval), keyvalcmp);
		obj->u.d.sorted = 1;
	}
}

int
fz_dict_marked(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (!fz_is_dict(obj))
		return 0;
	return obj->u.d.marked;
}

int
fz_dict_mark(fz_obj *obj)
{
	int marked;
	obj = fz_resolve_indirect(obj);
	if (!fz_is_dict(obj))
		return 0;
	marked = obj->u.d.marked;
	obj->u.d.marked = 1;
	return marked;
}

void
fz_dict_unmark(fz_obj *obj)
{
	obj = fz_resolve_indirect(obj);
	if (!fz_is_dict(obj))
		return;
	obj->u.d.marked = 0;
}

static void
fz_free_array(fz_obj *obj)
{
	int i;

	for (i = 0; i < obj->u.a.len; i++)
		if (obj->u.a.items[i])
			fz_drop_obj(obj->u.a.items[i]);

	fz_free(obj->ctx, obj->u.a.items);
	fz_free(obj->ctx, obj);
}

static void
fz_free_dict(fz_obj *obj)
{
	int i;

	for (i = 0; i < obj->u.d.len; i++) {
		if (obj->u.d.items[i].k)
			fz_drop_obj(obj->u.d.items[i].k);
		if (obj->u.d.items[i].v)
			fz_drop_obj(obj->u.d.items[i].v);
	}

	fz_free(obj->ctx, obj->u.d.items);
	fz_free(obj->ctx, obj);
}

void
fz_drop_obj(fz_obj *obj)
{
	if (!obj)
		return;
	if (--obj->refs)
		return;
	if (obj->kind == FZ_ARRAY)
		fz_free_array(obj);
	else if (obj->kind == FZ_DICT)
		fz_free_dict(obj);
	else
		fz_free(obj->ctx, obj);
}

/* Pretty printing objects */

struct fmt
{
	char *buf;
	int cap;
	int len;
	int indent;
	int tight;
	int col;
	int sep;
	int last;
};

static void fmt_obj(struct fmt *fmt, fz_obj *obj);

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
	return	ch == '(' || ch == ')' ||
		ch == '<' || ch == '>' ||
		ch == '[' || ch == ']' ||
		ch == '{' || ch == '}' ||
		ch == '/' ||
		ch == '%';
}

static inline void fmt_putc(struct fmt *fmt, int c)
{
	if (fmt->sep && !isdelim(fmt->last) && !isdelim(c)) {
		fmt->sep = 0;
		fmt_putc(fmt, ' ');
	}
	fmt->sep = 0;

	if (fmt->buf && fmt->len < fmt->cap)
		fmt->buf[fmt->len] = c;

	if (c == '\n')
		fmt->col = 0;
	else
		fmt->col ++;

	fmt->len ++;

	fmt->last = c;
}

static inline void fmt_indent(struct fmt *fmt)
{
	int i = fmt->indent;
	while (i--) {
		fmt_putc(fmt, ' ');
		fmt_putc(fmt, ' ');
	}
}

static inline void fmt_puts(struct fmt *fmt, char *s)
{
	while (*s)
		fmt_putc(fmt, *s++);
}

static inline void fmt_sep(struct fmt *fmt)
{
	fmt->sep = 1;
}

static void fmt_str(struct fmt *fmt, fz_obj *obj)
{
	char *s = fz_to_str_buf(obj);
	int n = fz_to_str_len(obj);
	int i, c;

	fmt_putc(fmt, '(');
	for (i = 0; i < n; i++)
	{
		c = (unsigned char)s[i];
		if (c == '\n')
			fmt_puts(fmt, "\\n");
		else if (c == '\r')
			fmt_puts(fmt, "\\r");
		else if (c == '\t')
			fmt_puts(fmt, "\\t");
		else if (c == '\b')
			fmt_puts(fmt, "\\b");
		else if (c == '\f')
			fmt_puts(fmt, "\\f");
		else if (c == '(')
			fmt_puts(fmt, "\\(");
		else if (c == ')')
			fmt_puts(fmt, "\\)");
		else if (c < 32 || c >= 127) {
			char buf[16];
			fmt_putc(fmt, '\\');
			sprintf(buf, "%03o", c);
			fmt_puts(fmt, buf);
		}
		else
			fmt_putc(fmt, c);
	}
	fmt_putc(fmt, ')');
}

static void fmt_hex(struct fmt *fmt, fz_obj *obj)
{
	char *s = fz_to_str_buf(obj);
	int n = fz_to_str_len(obj);
	int i, b, c;

	fmt_putc(fmt, '<');
	for (i = 0; i < n; i++) {
		b = (unsigned char) s[i];
		c = (b >> 4) & 0x0f;
		fmt_putc(fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
		c = (b) & 0x0f;
		fmt_putc(fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
	}
	fmt_putc(fmt, '>');
}

static void fmt_name(struct fmt *fmt, fz_obj *obj)
{
	unsigned char *s = (unsigned char *) fz_to_name(obj);
	int i, c;

	fmt_putc(fmt, '/');

	for (i = 0; s[i]; i++)
	{
		if (isdelim(s[i]) || iswhite(s[i]) ||
			s[i] == '#' || s[i] < 32 || s[i] >= 127)
		{
			fmt_putc(fmt, '#');
			c = (s[i] >> 4) & 0xf;
			fmt_putc(fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
			c = s[i] & 0xf;
			fmt_putc(fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
		}
		else
		{
			fmt_putc(fmt, s[i]);
		}
	}
}

static void fmt_array(struct fmt *fmt, fz_obj *obj)
{
	int i, n;

	n = fz_array_len(obj);
	if (fmt->tight) {
		fmt_putc(fmt, '[');
		for (i = 0; i < n; i++) {
			fmt_obj(fmt, fz_array_get(obj, i));
			fmt_sep(fmt);
		}
		fmt_putc(fmt, ']');
	}
	else {
		fmt_puts(fmt, "[ ");
		for (i = 0; i < n; i++) {
			if (fmt->col > 60) {
				fmt_putc(fmt, '\n');
				fmt_indent(fmt);
			}
			fmt_obj(fmt, fz_array_get(obj, i));
			fmt_putc(fmt, ' ');
		}
		fmt_putc(fmt, ']');
		fmt_sep(fmt);
	}
}

static void fmt_dict(struct fmt *fmt, fz_obj *obj)
{
	int i, n;
	fz_obj *key, *val;

	n = fz_dict_len(obj);
	if (fmt->tight) {
		fmt_puts(fmt, "<<");
		for (i = 0; i < n; i++) {
			fmt_obj(fmt, fz_dict_get_key(obj, i));
			fmt_sep(fmt);
			fmt_obj(fmt, fz_dict_get_val(obj, i));
			fmt_sep(fmt);
		}
		fmt_puts(fmt, ">>");
	}
	else {
		fmt_puts(fmt, "<<\n");
		fmt->indent ++;
		for (i = 0; i < n; i++) {
			key = fz_dict_get_key(obj, i);
			val = fz_dict_get_val(obj, i);
			fmt_indent(fmt);
			fmt_obj(fmt, key);
			fmt_putc(fmt, ' ');
			if (!fz_is_indirect(val) && fz_is_array(val))
				fmt->indent ++;
			fmt_obj(fmt, val);
			fmt_putc(fmt, '\n');
			if (!fz_is_indirect(val) && fz_is_array(val))
				fmt->indent --;
		}
		fmt->indent --;
		fmt_indent(fmt);
		fmt_puts(fmt, ">>");
	}
}

static void fmt_obj(struct fmt *fmt, fz_obj *obj)
{
	char buf[256];

	if (!obj)
		fmt_puts(fmt, "<NULL>");
	else if (fz_is_indirect(obj))
	{
		sprintf(buf, "%d %d R", fz_to_num(obj), fz_to_gen(obj));
		fmt_puts(fmt, buf);
	}
	else if (fz_is_null(obj))
		fmt_puts(fmt, "null");
	else if (fz_is_bool(obj))
		fmt_puts(fmt, fz_to_bool(obj) ? "true" : "false");
	else if (fz_is_int(obj))
	{
		sprintf(buf, "%d", fz_to_int(obj));
		fmt_puts(fmt, buf);
	}
	else if (fz_is_real(obj))
	{
		sprintf(buf, "%g", fz_to_real(obj));
		if (strchr(buf, 'e')) /* bad news! */
			sprintf(buf, fabsf(fz_to_real(obj)) > 1 ? "%1.1f" : "%1.8f", fz_to_real(obj));
		fmt_puts(fmt, buf);
	}
	else if (fz_is_string(obj))
	{
		char *str = fz_to_str_buf(obj);
		int len = fz_to_str_len(obj);
		int added = 0;
		int i, c;
		for (i = 0; i < len; i++) {
			c = (unsigned char)str[i];
			if (strchr("()\\\n\r\t\b\f", c))
				added ++;
			else if (c < 32 || c >= 127)
				added += 3;
		}
		if (added < len)
			fmt_str(fmt, obj);
		else
			fmt_hex(fmt, obj);
	}
	else if (fz_is_name(obj))
		fmt_name(fmt, obj);
	else if (fz_is_array(obj))
		fmt_array(fmt, obj);
	else if (fz_is_dict(obj))
		fmt_dict(fmt, obj);
	else
		fmt_puts(fmt, "<unknown object>");
}

static int
fz_sprint_obj(char *s, int n, fz_obj *obj, int tight)
{
	struct fmt fmt;

	fmt.indent = 0;
	fmt.col = 0;
	fmt.sep = 0;
	fmt.last = 0;

	fmt.tight = tight;
	fmt.buf = s;
	fmt.cap = n;
	fmt.len = 0;
	fmt_obj(&fmt, obj);

	if (fmt.buf && fmt.len < fmt.cap)
		fmt.buf[fmt.len] = '\0';

	return fmt.len;
}

int
fz_fprint_obj(FILE *fp, fz_obj *obj, int tight)
{
	char buf[1024];
	char *ptr;
	int n;

	n = fz_sprint_obj(NULL, 0, obj, tight);
	if ((n + 1) < sizeof buf)
	{
		fz_sprint_obj(buf, sizeof buf, obj, tight);
		fputs(buf, fp);
		fputc('\n', fp);
	}
	else
	{
		ptr = fz_malloc(obj->ctx, n + 1);
		fz_sprint_obj(ptr, n + 1, obj, tight);
		fputs(ptr, fp);
		fputc('\n', fp);
		fz_free(obj->ctx, ptr);
	}
	return n;
}

void
fz_debug_obj(fz_obj *obj)
{
	fz_fprint_obj(stdout, obj, 0);
}

void
fz_debug_ref(fz_obj *ref)
{
	fz_debug_obj(fz_resolve_indirect(ref));
}

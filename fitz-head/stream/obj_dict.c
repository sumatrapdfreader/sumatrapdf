#include "fitz-base.h"
#include "fitz-stream.h"

/* keep either names or strings in the dict. don't mix & match. */

static int keyvalcmp(const void *ap, const void *bp)
{
    const fz_keyval *a = ap;
    const fz_keyval *b = bp;
    if (fz_isname(a->k))
        return strcmp(fz_toname(a->k), fz_toname(b->k));
    if (fz_isstring(a->k))
        return strcmp(fz_tostrbuf(a->k), fz_tostrbuf(b->k));
    return -1;
}

static inline int keystrcmp(fz_obj *key, char *s)
{
    if (fz_isname(key))
        return strcmp(fz_toname(key), s);
    if (fz_isstring(key))
        return strcmp(fz_tostrbuf(key), s);
    return -1;
}

fz_error *
fz_newdict(fz_obj **op, int initialcap)
{
    fz_obj *obj;
    int i;

    obj = *op = fz_malloc(sizeof (fz_obj));
    if (!obj)
        return fz_throw("outofmem: dict struct");

    obj->refs = 1;  
    obj->kind = FZ_DICT;

    obj->u.d.sorted = 1;
    obj->u.d.len = 0;
    obj->u.d.cap = initialcap > 0 ? initialcap : 10;

    obj->u.d.items = fz_malloc(sizeof(fz_keyval) * obj->u.d.cap);
    if (!obj->u.d.items)
    {
        fz_free(obj);
        return fz_throw("outofmem: dict item buffer");
    }

    for (i = 0; i < obj->u.d.cap; i++)
    {
        obj->u.d.items[i].k = nil;
        obj->u.d.items[i].v = nil;
    }

    *op = obj;
    return fz_okay;
}

fz_error *
fz_copydict(fz_obj **op, fz_obj *obj)
{
    fz_error *error;
    fz_obj *new;
    int i;

    if (!fz_isdict(obj))
        return fz_throw("assert: not a dict (%s)", fz_objkindstr(obj));

    error = fz_newdict(&new, obj->u.d.cap);
    if (error)
        return fz_rethrow(error, "cannot create new dict");

    for (i = 0; i < fz_dictlen(obj); i++)
    {
        error = fz_dictput(new, fz_dictgetkey(obj, i), fz_dictgetval(obj, i));
        if (error)
        {
            fz_dropobj(new);
            return fz_rethrow(error, "cannot copy dict entry");
        }
    }

    *op = new;
    return fz_okay;
}

fz_error *
fz_deepcopydict(fz_obj **op, fz_obj *obj)
{
    fz_error *error;
    fz_obj *new;
    fz_obj *val;
    int i;

    if (!fz_isdict(obj))
        return fz_throw("assert: not a dict (%s)", fz_objkindstr(obj));

    error = fz_newdict(&new, obj->u.d.cap);
    if (error)
        return fz_rethrow(error, "cannot create new dict");

    for (i = 0; i < fz_dictlen(obj); i++)
    {
        val = fz_dictgetval(obj, i);

        if (fz_isarray(val))
        {
            error = fz_deepcopyarray(&val, val);
            if (error)
            {
                fz_dropobj(new);
                return fz_rethrow(error, "cannot deep copy item");
            }

            error = fz_dictput(new, fz_dictgetkey(obj, i), val);
            if (error)
            {
                fz_dropobj(val);
                fz_dropobj(new);
                return fz_rethrow(error, "cannot add dict entry");
            }

            fz_dropobj(val);
        }

        else if (fz_isdict(val))
        {
            error = fz_deepcopydict(&val, val);
            if (error)
            {
                fz_dropobj(new);
                return fz_rethrow(error, "cannot deep copy item");
            }

            error = fz_dictput(new, fz_dictgetkey(obj, i), val);
            if (error)
            {
                fz_dropobj(val);
                fz_dropobj(new);
                return fz_rethrow(error, "cannot add dict entry");
            }

            fz_dropobj(val);
        }

        else
        {
            error = fz_dictput(new, fz_dictgetkey(obj, i), val);
            if (error)
            {
                fz_dropobj(new);
                return fz_rethrow(error, "cannot copy dict entry");
            }
        }
    }

    *op = new;
    return fz_okay;
}

static fz_error *
growdict(fz_obj *obj)
{
    fz_keyval *newitems;
    int newcap;
    int i;

    newcap = obj->u.d.cap * 2;

    newitems = fz_realloc(obj->u.d.items, sizeof(fz_keyval) * newcap);
    if (!newitems)
        return fz_throw("outofmem: resize item buffer");

    obj->u.d.items = newitems;
    for (i = obj->u.d.cap; i < newcap; i++)
    {
        obj->u.d.items[i].k = nil;
        obj->u.d.items[i].v = nil;
    }
    obj->u.d.cap = newcap;

    return fz_okay;
}

int
fz_dictlen(fz_obj *obj)
{
    if (!fz_isdict(obj))
        return 0;
    return obj->u.d.len;
}

fz_obj *
fz_dictgetkey(fz_obj *obj, int i)
{
    if (!fz_isdict(obj))
        return nil;

    if (i < 0 || i >= obj->u.d.len)
        return nil;

    return obj->u.d.items[i].k;
}

fz_obj *
fz_dictgetval(fz_obj *obj, int i)
{
    if (!fz_isdict(obj))
        return nil;

    if (i < 0 || i >= obj->u.d.len)
        return nil;

    return obj->u.d.items[i].v;
}

static inline int dictfinds(fz_obj *obj, char *key)
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

    if (!fz_isdict(obj))
        return nil;

    i = dictfinds(obj, key);
    if (i >= 0)
        return obj->u.d.items[i].v;

    return nil;
}

fz_obj *
fz_dictget(fz_obj *obj, fz_obj *key)
{
    if (fz_isname(key))
        return fz_dictgets(obj, fz_toname(key));
    if (fz_isstring(key))
        return fz_dictgets(obj, fz_tostrbuf(key));
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

fz_error *
fz_dictput(fz_obj *obj, fz_obj *key, fz_obj *val)
{
    fz_error *error;
    char *s;
    int i;

    if (!fz_isdict(obj))
        return fz_throw("assert: not a dict (%s)", fz_objkindstr(obj));

    if (fz_isname(key))
        s = fz_toname(key);
    else if (fz_isstring(key))
        s = fz_tostrbuf(key);
    else
        return fz_throw("assert: key is not string or name (%s)", fz_objkindstr(obj));

    i = dictfinds(obj, s);
    if (i >= 0)
    {
        fz_dropobj(obj->u.d.items[i].v);
        obj->u.d.items[i].v = fz_keepobj(val);
        return fz_okay;
    }

    if (obj->u.d.len + 1 > obj->u.d.cap)
    {
        error = growdict(obj);
        if (error)
            return fz_rethrow(error, "cannot grow dict");
    }

    /* borked! */
    if (obj->u.d.len)
        if (keystrcmp(obj->u.d.items[obj->u.d.len - 1].k, s) > 0)
            obj->u.d.sorted = 0;

    obj->u.d.items[obj->u.d.len].k = fz_keepobj(key);
    obj->u.d.items[obj->u.d.len].v = fz_keepobj(val);
    obj->u.d.len ++;

    return fz_okay;
}

fz_error *
fz_dictputs(fz_obj *obj, char *key, fz_obj *val)
{
    fz_error *error;
    fz_obj *keyobj;

    error = fz_newname(&keyobj, key);
    if (error)
        return fz_rethrow(error, "cannot create key object");

    error = fz_dictput(obj, keyobj, val);
    if (error)
    {
        fz_dropobj(keyobj);
        return fz_rethrow(error, "cannot insert dict entry");
    }

    fz_dropobj(keyobj);
    return fz_okay;
}

fz_error *
fz_dictdels(fz_obj *obj, char *key)
{
    int i;

    if (!fz_isdict(obj))
        return fz_throw("assert: not a dict (%s)", fz_objkindstr(obj));

    i = dictfinds(obj, key);
    if (i >= 0)
    {
        fz_dropobj(obj->u.d.items[i].k);
        fz_dropobj(obj->u.d.items[i].v);
        obj->u.d.sorted = 0;
        obj->u.d.items[i] = obj->u.d.items[obj->u.d.len-1];
        obj->u.d.len --;
    }

    return fz_okay;
}

fz_error *
fz_dictdel(fz_obj *obj, fz_obj *key)
{
    if (fz_isname(key))
        return fz_dictdels(obj, fz_toname(key));
    else if (fz_isstring(key))
        return fz_dictdels(obj, fz_tostrbuf(key));
    else
        return fz_throw("assert: key is not string or name (%s)", fz_objkindstr(obj));
}

void
fz_dropdict(fz_obj *obj)
{
    int i;

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
    if (!fz_isdict(obj))
        return;
    if (!obj->u.d.sorted)
    {
        qsort(obj->u.d.items, obj->u.d.len, sizeof(fz_keyval), keyvalcmp);
        obj->u.d.sorted = 1;
    }
}


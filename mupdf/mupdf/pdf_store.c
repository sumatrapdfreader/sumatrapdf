#include "fitz.h"
#include "mupdf.h"

typedef struct pdf_item_s pdf_item;

struct pdf_item_s
{
	void *dropfunc;
	fz_obj *key;
	void *val;
	int age;
	pdf_item *next;
};

struct refkey
{
	void *dropfunc;
	int num;
	int gen;
};

struct pdf_store_s
{
	fz_hashtable *hash;	/* hash for num/gen keys */
	pdf_item *root;		/* linked list for everything else */
};

pdf_store *
pdf_newstore(void)
{
	pdf_store *store;
	store = fz_malloc(sizeof(pdf_store));
	store->hash = fz_newhash(4096, sizeof(struct refkey));
	store->root = nil;
	return store;
}

void
pdf_storeitem(pdf_store *store, void *keepfunc, void *dropfunc, fz_obj *key, void *val)
{
	pdf_item *item;

	if (!store)
		return;

	item = fz_malloc(sizeof(pdf_item));
	item->dropfunc = dropfunc;
	item->key = fz_keepobj(key);
	item->val = ((void*(*)(void*))keepfunc)(val);
	item->age = 0;
	item->next = nil;

	if (fz_isindirect(key))
	{
		struct refkey refkey;
		pdf_logrsrc("store item (%d %d R) ptr=%p\n", fz_tonum(key), fz_togen(key), val);
		refkey.dropfunc = dropfunc;
		refkey.num = fz_tonum(key);
		refkey.gen = fz_togen(key);
		fz_hashinsert(store->hash, &refkey, item);
	}
	else
	{
		pdf_logrsrc("store item (...) = %p\n", val);
		item->next = store->root;
		store->root = item;
	}
}

void *
pdf_finditem(pdf_store *store, void *dropfunc, fz_obj *key)
{
	struct refkey refkey;
	pdf_item *item;

	if (!store)
		return nil;

	if (key == nil)
		return nil;

	if (fz_isindirect(key))
	{
		refkey.dropfunc = dropfunc;
		refkey.num = fz_tonum(key);
		refkey.gen = fz_togen(key);
		item = fz_hashfind(store->hash, &refkey);
		if (item)
		{
			item->age = 0;
			return item->val;
		}
	}
	else
	{
		for (item = store->root; item; item = item->next)
		{
			if (item->dropfunc == dropfunc && !fz_objcmp(item->key, key))
			{
				item->age = 0;
				return item->val;
			}
		}
	}

	return nil;
}

void
pdf_removeitem(pdf_store *store, void *dropfunc, fz_obj *key)
{
	struct refkey refkey;
	pdf_item *item, *prev, *next;

	if (fz_isindirect(key))
	{
		refkey.dropfunc = dropfunc;
		refkey.num = fz_tonum(key);
		refkey.gen = fz_togen(key);
		item = fz_hashfind(store->hash, &refkey);
		if (item)
		{
			fz_hashremove(store->hash, &refkey);
			((void(*)(void*))item->dropfunc)(item->val);
			fz_dropobj(item->key);
			fz_free(item);
		}
	}
	else
	{
		prev = nil;
		for (item = store->root; item; item = next)
		{
			next = item->next;
			if (item->dropfunc == dropfunc && !fz_objcmp(item->key, key))
			{
				if (!prev)
					store->root = next;
				else
					prev->next = next;
				((void(*)(void*))item->dropfunc)(item->val);
				fz_dropobj(item->key);
				fz_free(item);
			}
			else
				prev = item;
		}
	}
}

void
pdf_agestore(pdf_store *store, int maxage)
{
	struct refkey *refkey;
	pdf_item *item, *prev, *next;
	int i;

	for (i = 0; i < fz_hashlen(store->hash); i++)
	{
		refkey = fz_hashgetkey(store->hash, i);
		item = fz_hashgetval(store->hash, i);
		if (item && ++item->age > maxage)
		{
			fz_hashremove(store->hash, refkey);
			((void(*)(void*))item->dropfunc)(item->val);
			fz_dropobj(item->key);
			fz_free(item);
			i--; /* items with same hash may move into place */
		}
	}

	prev = nil;
	for (item = store->root; item; item = next)
	{
		next = item->next;
		if (++item->age > maxage)
		{
			if (!prev)
				store->root = next;
			else
				prev->next = next;
			((void(*)(void*))item->dropfunc)(item->val);
			fz_dropobj(item->key);
			fz_free(item);
		}
		else
			prev = item;
	}
}

void
pdf_freestore(pdf_store *store)
{
	pdf_agestore(store, 0);
	fz_freehash(store->hash);
	fz_free(store);
}

void
pdf_debugstore(pdf_store *store)
{
	pdf_item *item;
	pdf_item *next;
	struct refkey *refkey;
	int i;

	printf("-- resource store contents --\n");

	for (i = 0; i < fz_hashlen(store->hash); i++)
	{
		refkey = fz_hashgetkey(store->hash, i);
		item = fz_hashgetval(store->hash, i);
		if (item)
			printf("store[%d] (%d %d R) = %p\n", i, refkey->num, refkey->gen, item->val);
	}

	for (item = store->root; item; item = next)
	{
		next = item->next;
		printf("store[*] ");
		fz_debugobj(item->key);
		printf(" = %p\n", item->val);
	}
}

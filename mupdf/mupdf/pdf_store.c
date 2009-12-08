#include "fitz.h"
#include "mupdf.h"

typedef struct pdf_item_s pdf_item;

struct pdf_item_s
{
	pdf_itemkind kind;
	fz_obj *key;
	void *val;
	int age;
	pdf_item *next;
};

struct refkey
{
	pdf_itemkind kind;
	int oid;
	int gen;
};

struct pdf_store_s
{
	fz_hashtable *hash;	/* hash for oid/gen keys */
	pdf_item *root;		/* linked list for everything else */
};

static char *kindstr(pdf_itemkind kind)
{
	switch (kind)
	{
	case PDF_KCOLORSPACE: return "colorspace";
	case PDF_KFUNCTION: return "function";
	case PDF_KXOBJECT: return "xobject";
	case PDF_KIMAGE: return "image";
	case PDF_KPATTERN: return "pattern";
	case PDF_KSHADE: return "shade";
	case PDF_KCMAP: return "cmap";
	case PDF_KFONT: return "font";
	}

	return "unknown";
}

static int itemmaxage(pdf_itemkind kind)
{
	switch (kind)
	{
	case PDF_KCOLORSPACE: return 10;
	case PDF_KFUNCTION: return 2;
	case PDF_KXOBJECT: return 2;
	case PDF_KIMAGE: return 10;
	case PDF_KPATTERN: return 2;
	case PDF_KSHADE: return 2;
	case PDF_KCMAP: return 10;
	case PDF_KFONT: return 10;
	}

	return 0;
}

static void keepitem(pdf_itemkind kind, void *val)
{
	switch (kind)
	{
	case PDF_KCOLORSPACE: fz_keepcolorspace(val); break;
	case PDF_KFUNCTION: pdf_keepfunction(val); break;
	case PDF_KXOBJECT: pdf_keepxobject(val); break;
	case PDF_KIMAGE: fz_keepimage(val); break;
	case PDF_KPATTERN: pdf_keeppattern(val); break;
	case PDF_KSHADE: fz_keepshade(val); break;
	case PDF_KCMAP: pdf_keepcmap(val); break;
	case PDF_KFONT: pdf_keepfont(val); break;
	}
}

static void dropitem(pdf_itemkind kind, void *val)
{
	switch (kind)
	{
	case PDF_KCOLORSPACE: fz_dropcolorspace(val); break;
	case PDF_KFUNCTION: pdf_dropfunction(val); break;
	case PDF_KXOBJECT: pdf_dropxobject(val); break;
	case PDF_KIMAGE: fz_dropimage(val); break;
	case PDF_KPATTERN: pdf_droppattern(val); break;
	case PDF_KSHADE: fz_dropshade(val); break;
	case PDF_KCMAP: pdf_dropcmap(val); break;
	case PDF_KFONT: pdf_dropfont(val); break;
	}
}

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
pdf_emptystore(pdf_store *store)
{
	pdf_item *item;
	pdf_item *next;
	struct refkey *key;
	int i;

	for (i = 0; i < fz_hashlen(store->hash); i++)
	{
		key = fz_hashgetkey(store->hash, i);
		item = fz_hashgetval(store->hash, i);
		if (item)
		{
			fz_dropobj(item->key);
			dropitem(key->kind, item->val);
			fz_free(item);
		}
	}

	fz_emptyhash(store->hash);

	for (item = store->root; item; item = next)
	{
		next = item->next;
		fz_dropobj(item->key);
		dropitem(item->kind, item->val);
		fz_free(item);
	}

	store->root = nil;
}

void
pdf_dropstore(pdf_store *store)
{
	pdf_emptystore(store);
	fz_drophash(store->hash);
	fz_free(store);
}

static void evictitem(pdf_item *item)
{
	pdf_logrsrc("evicting item %s (%d %d R) at age %d\n", kindstr(item->kind), fz_tonum(item->key), fz_togen(item->key), item->age);
	fz_dropobj(item->key);
	dropitem(item->kind, item->val);
	fz_free(item);
}

void
pdf_evictageditems(pdf_store *store)
{
	pdf_item *item;
	pdf_item *next;
	pdf_item *prev;
	struct refkey *key;
	int i;

	for (i = 0; i < fz_hashlen(store->hash); i++)
	{
		key = fz_hashgetkey(store->hash, i);
		item = fz_hashfind(store->hash, key);
		if (item && item->age > itemmaxage(item->kind))
		{
			fz_hashremove(store->hash, key);
			evictitem(item);
		}
	}

	prev = nil;
	for (item = store->root; item; item = next)
	{
		next = item->next;
		if (item->age > itemmaxage(item->kind))
		{
			if (!prev)
				store->root = next;
			else
				prev->next = next;
			evictitem(item);
		}
		else
			prev = item;
	}
}

void
pdf_agestoreditems(pdf_store *store)
{
	pdf_item *item;
	int i;

	for (i = 0; i < fz_hashlen(store->hash); i++)
	{
		item = fz_hashgetval(store->hash, i);
		if (item)
			item->age++;
	}

	for (item = store->root; item; item = item->next)
		item->age++;
}

void
pdf_storeitem(pdf_store *store, pdf_itemkind kind, fz_obj *key, void *val)
{
	pdf_item *item;

	item = fz_malloc(sizeof(pdf_item));
	item->kind = kind;
	item->key = fz_keepobj(key);
	item->val = val;
	item->age = 0;
	item->next = nil;

	if (fz_isindirect(key))
	{
		struct refkey refkey;

		pdf_logrsrc("store item %s (%d %d R) ptr=%p\n", kindstr(kind), fz_tonum(key), fz_togen(key), val);

		refkey.kind = kind;
		refkey.oid = fz_tonum(key);
		refkey.gen = fz_togen(key);

		fz_hashinsert(store->hash, &refkey, item);
	}
	else
	{
		pdf_logrsrc("store item %s: ... = %p\n", kindstr(kind), val);

		item->next = store->root;
		store->root = item;
	}

	keepitem(kind, val);
}

void *
pdf_finditem(pdf_store *store, pdf_itemkind kind, fz_obj *key)
{
	pdf_item *item;
	struct refkey refkey;

	if (key == nil)
		return nil;

	if (fz_isindirect(key))
	{
		refkey.kind = kind;
		refkey.oid = fz_tonum(key);
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
			if (item->kind == kind && !fz_objcmp(item->key, key))
		{
			item->age = 0;
			return item->val;
		}
	}

	return nil;
}

void
pdf_removeitem(pdf_store *store, pdf_itemkind kind, fz_obj *key)
{
	pdf_item *item, *prev;
	struct refkey refkey;

	if (key == nil)
		return;

	if (fz_isindirect(key))
	{
		refkey.kind = kind;
		refkey.oid = fz_tonum(key);
		refkey.gen = fz_togen(key);

		item = fz_hashfind(store->hash, &refkey);
		if (!item)
			return;
		fz_hashremove(store->hash, &refkey);

		pdf_logrsrc("remove item %s (%d %d R) ptr=%p\n", kindstr(kind), fz_tonum(key), fz_togen(key), item->val);

		dropitem(kind, item->val);
		fz_dropobj(item->key);
		fz_free(item);
	}

	else
	{
		prev = nil;
		for (item = store->root; item; item = item->next)
		{
			if (item->kind == kind && !fz_objcmp(item->key, key))
			{
				if (!prev)
					store->root = item->next;
				else
					prev->next = item->next;
				dropitem(kind, item->val);
				fz_dropobj(item->key);
				fz_free(item);
				break;
			}
			prev = item;
		}
	}
}

void
pdf_debugstore(pdf_store *store)
{
	pdf_item *item;
	pdf_item *next;
	struct refkey *key;
	int i;

	printf("-- resource store contents --\n");

	for (i = 0; i < fz_hashlen(store->hash); i++)
	{
		key = fz_hashgetkey(store->hash, i);
		item = fz_hashgetval(store->hash, i);
		if (key && item)
		{
			printf("store[%d] (%d %d R) = %p\n", i, key->oid, key->gen, item->val);
		}
	}

	for (item = store->root; item; item = next)
	{
		next = item->next;
		printf("store[*] ");
		fz_debugobj(item->key);
		printf(" = %p\n", item->val);
	}

	store->root = nil;
}

/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* This is a dictionary with intentionally unorthodox design.
It's called dictionary, not a hashtable, to emphasize it's
about the API, not implementation technique.
Currently it's imlemented on top of a hash table, but it could
just as well be a tree of some kind.

Usually those things are done as a templated hash table class,
but I want to avoid code bloat and awful syntax.

The classes are based on a generic, untyped uintptr => uintptr
hash table. The actual dict class is a wrapper that provides
a type-safe API and handles policy decisions like allocations
(if they are necessary).
*/

#include "BaseUtil.h"
#include "Dict.h"

namespace dict {

struct HashTableEntry {
    uintptr_t key;
    uintptr_t val;
    struct HashTableEntry *next;
};

// not a class so that it can be allocated with an allocator
struct HashTable {
    HashTableEntry **entries;
    size_t entriesCount;
    size_t used;

    // for debugging
    size_t nRehashes;
    size_t nCollisions;
};

static uint32_t hash_function_seed = 5381;

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
static unsigned int murmur_hash2(const void *key, int len) {
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    uint32_t seed = hash_function_seed;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;

    while(len >= 4) {
        uint32_t k = *(uint32_t*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch(len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int)h;
}

static HashTable *NewHashTable(size_t size, Allocator *allocator)
{
    CrashIf(!allocator); // we'll leak otherwise
    HashTable *h = (HashTable*)Allocator::AllocZero(allocator, sizeof(HashTable));
    // entries are not allocated with allocator since those are large blocks
    // and we don't want to waste their memory after 
    h->entries = AllocArray<HashTableEntry*>(size);
    h->entriesCount = size;
    return h;
}

static void DeleteHashTable(HashTable *h)
{
    free(h->entries);
    // the rest is freed by allocator
}

static HashTableEntry **HashTableEntryPtrForHash(HashTable *h, size_t hash)
{
    size_t pos = hash % h->entriesCount;
    return &h->entries[pos];
}

MapStrToInt::MapStrToInt(size_t initialSize)
{
    // we use PoolAllocator to allocate HashTableEntry entries
    // and copies of string keys
    allocator = new PoolAllocator(4);
    h = NewHashTable(initialSize, allocator);
}

MapStrToInt::~MapStrToInt()
{
    DeleteHashTable(h);
    delete allocator;
}

// TODO: pass some adapter object that provides Hash() and Compare() functions to make
// this re-usable for classes with different keys
static HashTableEntry *FindEntryForKey(MapStrToInt *dict, const char *key, bool forInsert)
{
    size_t hash = murmur_hash2((const void*)key, (int)str::Len(key));
    HashTableEntry **firstEntryPtr = HashTableEntryPtrForHash(dict->h, hash);
    HashTableEntry *e = *firstEntryPtr;
    HashTableEntry **prevPtr = firstEntryPtr;
    const char *entryKey;
    while (e) {
        entryKey = (const char*)e->key;
        if (str::Eq(key, entryKey)) {
            if (forInsert)
                return NULL;
            return e;
        }
        prevPtr = &e->next;
        e = e->next;
    }
    if (!forInsert)
        return NULL;
    *prevPtr = (HashTableEntry*)Allocator::AllocZero(dict->allocator, sizeof(HashTableEntry));
    // TODO: rehash if needed
    if (firstEntryPtr == prevPtr)
        dict->h->used++;
    else
        dict->h->nCollisions++;
    return *prevPtr;
}

bool MapStrToInt::Insert(const char *key, int val)
{
    HashTableEntry *e = FindEntryForKey(this, key, true);
    if (!e)
        return false;
    e->key = (intptr_t)Allocator::Dup(allocator, (void*)key, str::Len(key) + 1);
    e->val = (intptr_t)val;
    return true;
}

bool MapStrToInt::GetValue(const char *key, int* valOut)
{
    HashTableEntry *e = FindEntryForKey(this, key, false);
    if (!e)
        return false;
    *valOut = (int)e->val;
    return true;
}

}

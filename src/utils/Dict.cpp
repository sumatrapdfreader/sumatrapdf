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

Our hash table uses the same parameters as the one in redis
(but without complexity of incremental hashing):
- we use chaining on collisions
- size of the hash table is power of two

TODO:
- remove (make sure to recycle HashTableEntries via free list)
- would hash function be faster if we got bytes one at a time
  but only with a single pass vs. getting 4 at a time but
  doing 2 passes (the first to calculate the length)?
- would be marginally faster if used a single global instance
  of StrHasherComparator and WStrHasherComparator. In which case
  could use a C style function pointer approach (to avoid C++
  static initializers, which might be called if we use global
  C++ classes)
*/

#include "BaseUtil.h"
#include "Dict.h"

namespace dict {

class HasherComparator {
public:
    virtual size_t Hash(uintptr_t key) = 0;
    virtual bool Equal(uintptr_t k1, uintptr_t k2) = 0;
};

struct HashTableEntry {
    uintptr_t key;
    uintptr_t val;
    struct HashTableEntry *next;
};

// not a class so that it can be allocated with an allocator
struct HashTable {
    HashTableEntry **entries;
    size_t nEntries;
    size_t nUsed; // total number of inserted entries

    // for debugging
    size_t nResizes;
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

// number of hash table entries should be power of 2
static size_t roundToPowerOf2(size_t size)
{
    size_t n = 4;
    while (size < LONG_MAX) {
        if (n >= size)
            return n;
        n *= 2;
    }
    return LONG_MAX;
}

static HashTable *NewHashTable(size_t size, Allocator *allocator)
{
    CrashIf(!allocator); // we'll leak otherwise
    HashTable *h = (HashTable*)Allocator::AllocZero(allocator, sizeof(HashTable));
    size = roundToPowerOf2(size);
    // entries are not allocated with allocator since those are large blocks
    // and we don't want to waste their memory after 
    h->entries = AllocArray<HashTableEntry*>(size);
    h->nEntries = size;
    return h;
}

static void DeleteHashTable(HashTable *h)
{
    free(h->entries);
    // the rest is freed by allocator
}

static void HashTableResizeIfNeeded(HashTable *h, HasherComparator *hc)
{
    // per http://stackoverflow.com/questions/1603712/when-should-i-do-rehashing-of-entire-hash-table/1604428#1604428
    // when using collision chaining, load factor can be 150%
    if (h->nUsed < (h->nEntries * 3) / 2)
        return;

    size_t newSize = roundToPowerOf2(h->nEntries + 1);
    CrashIf(newSize <= h->nEntries);
    HashTableEntry **newEntries = AllocArray<HashTableEntry*>(newSize);
    HashTableEntry *e, *next;
    size_t hash, pos;
    for (size_t i = 0; i < h->nEntries; i++) {
        e = h->entries[i];
        while (e) {
            next = e->next;
            hash = hc->Hash(e->key);
            pos = hash % newSize;
            e->next = newEntries[pos];
            newEntries[pos] = e;
            e = next;
        }
    }
    free(h->entries);
    h->entries = newEntries;
    h->nEntries = newSize;
    h->nResizes += 1;

    CrashIf(h->nUsed >= (h->nEntries * 3) / 2);
}

// note: allocator must be NULL for get, non-NULL for create
static HashTableEntry *GetOrCreateEntry(HashTable *h, HasherComparator *hc, uintptr_t key, Allocator *allocator, bool& newEntry)
{
    bool shouldCreate = (allocator != NULL);
    size_t hash = hc->Hash(key);
    size_t pos = hash % h->nEntries;
    HashTableEntry *e = h->entries[pos];
    newEntry = false;
    while (e) {
        if (hc->Equal(key, e->key))
            return e;
        e = e->next;
    }
    if (!shouldCreate)
        return NULL;

    e = (HashTableEntry*)Allocator::AllocZero(allocator, sizeof(HashTableEntry));
    e->next = h->entries[pos];
    h->entries[pos] = e;
    h->nUsed++;
    if (e->next != NULL)
        h->nCollisions++;
    newEntry = true;
    return e;
}

class StrKeyHasherComparator : public HasherComparator {
    virtual size_t Hash(uintptr_t key) { return (size_t)murmur_hash2((const void*)key, (int)str::Len((const char*)key)); }
    virtual bool Equal(uintptr_t k1, uintptr_t k2) { return str::Eq((const char*)k1, (const char*)k2); }
};

MapStrToInt::MapStrToInt(size_t initialSize)
{
    // we use PoolAllocator to allocate HashTableEntry entries
    // and copies of string keys
    allocator = new PoolAllocator();
    allocator->SetAllocRounding(4);
    h = NewHashTable(initialSize, allocator);
}

MapStrToInt::~MapStrToInt()
{
    DeleteHashTable(h);
    delete allocator;
}

bool MapStrToInt::Insert(const char *key, int val, int *prevVal)
{
    StrKeyHasherComparator hc;
    bool newEntry;
    HashTableEntry *e = GetOrCreateEntry(h, &hc, (uintptr_t)key, allocator, newEntry);
    if (!newEntry) {
        if (prevVal)
            *prevVal = e->val;
        return false;
    }
    e->key = (intptr_t)Allocator::Dup(allocator, (void*)key, str::Len(key) + 1);
    e->val = (intptr_t)val;

    HashTableResizeIfNeeded(h, &hc);
    return true;
}

bool MapStrToInt::GetValue(const char *key, int* valOut)
{
    StrKeyHasherComparator hc;
    bool newEntry;
    HashTableEntry *e = GetOrCreateEntry(h, &hc, (uintptr_t)key, NULL, newEntry);
    if (!e)
        return false;
    *valOut = (int)e->val;
    return true;
}

class TStrKeyHasherComparator : public HasherComparator {
    virtual size_t Hash(uintptr_t key) {
        size_t cbLen = str::Len((const TCHAR *)key) * sizeof(TCHAR);
        return (size_t)murmur_hash2((const void *)key, (int)cbLen);
    }
    virtual bool Equal(uintptr_t k1, uintptr_t k2) {
        const TCHAR *s1 = (const TCHAR *)k1;
        const TCHAR *s2 = (const TCHAR *)k2;
        return str::Eq(s1, s2);
    }
};

MapTStrToInt::MapTStrToInt(size_t initialSize)
{
    // we use PoolAllocator to allocate HashTableEntry entries
    // and copies of string keys
    allocator = new PoolAllocator();
    allocator->SetAllocRounding(4);
    h = NewHashTable(initialSize, allocator);
}

MapTStrToInt::~MapTStrToInt()
{
    DeleteHashTable(h);
    delete allocator;
}

bool MapTStrToInt::Insert(const TCHAR *key, int val, int *prevVal)
{
    TStrKeyHasherComparator hc;
    bool newEntry;
    HashTableEntry *e = GetOrCreateEntry(h, &hc, (uintptr_t)key, allocator, newEntry);
    if (!newEntry) {
        if (prevVal)
            *prevVal = e->val;
        return false;
    }
    size_t keyCbLen = (str::Len(key) + 1) * sizeof(TCHAR);
    e->key = (intptr_t)Allocator::Dup(allocator, (void*)key, keyCbLen);
    e->val = (intptr_t)val;

    HashTableResizeIfNeeded(h, &hc);
    return true;
}

bool MapTStrToInt::GetValue(const TCHAR *key, int *valOut)
{
    TStrKeyHasherComparator hc;
    bool newEntry;
    HashTableEntry *e = GetOrCreateEntry(h, &hc, (uintptr_t)key, NULL, newEntry);
    if (!e)
        return false;
    *valOut = (int)e->val;
    return true;
}

}

/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
- add iterator for keys/values
- would hash function be faster if we got bytes one at a time
  but only with a single pass vs. getting 4 at a time but
  doing 2 passes (the first to calculate the length)?
*/

#include "utils/BaseUtil.h"
#include "utils/Dict.h"

namespace dict {

class HasherComparator {
  public:
    virtual size_t Hash(uintptr_t key) = 0;
    virtual bool Equal(uintptr_t k1, uintptr_t k2) = 0;
    virtual ~HasherComparator() {
    }
};

class StrKeyHasherComparator : public HasherComparator {
    virtual size_t Hash(uintptr_t key) {
        return MurmurHash2((const void*)key, str::Len((const char*)key));
    }
    virtual bool Equal(uintptr_t k1, uintptr_t k2) {
        const char* s1 = (const char*)k1;
        const char* s2 = (const char*)k2;
        return str::Eq(s1, s2);
    }
};

class WStrKeyHasherComparator : public HasherComparator {
    virtual size_t Hash(uintptr_t key) {
        size_t cbLen = str::Len((const WCHAR*)key) * sizeof(WCHAR);
        return MurmurHash2((const void*)key, cbLen);
    }
    virtual bool Equal(uintptr_t k1, uintptr_t k2) {
        const WCHAR* s1 = (const WCHAR*)k1;
        const WCHAR* s2 = (const WCHAR*)k2;
        return str::Eq(s1, s2);
    }
};

static StrKeyHasherComparator gStrKeyHasherComparator;
static WStrKeyHasherComparator gWStrKeyHasherComparator;

struct HashTableEntry {
    uintptr_t key;
    uintptr_t val;
    struct HashTableEntry* next;
};

// not a class so that it can be allocated with an allocator
struct HashTable {
    HashTableEntry** entries;
    HashTableEntry* freeList;

    size_t nEntries;
    size_t nUsed; // total number of inserted entries

    // for debugging
    size_t nResizes;
    size_t nCollisions;
};

static HashTable* NewHashTable(size_t size, Allocator* allocator) {
    CrashIf(!allocator); // we'll leak otherwise
    HashTable* h = (HashTable*)Allocator::AllocZero(allocator, sizeof(HashTable));
    // number of hash table entries should be power of 2
    size = RoundToPowerOf2(size);
    // entries are not allocated with allocator since those are large blocks
    // and we don't want to waste their memory after
    h->entries = AllocArray<HashTableEntry*>(size);
    h->nEntries = size;
    return h;
}

static void DeleteHashTable(HashTable* h) {
    free(h->entries);
    // the rest is freed by allocator
}

static void HashTableResize(HashTable* h, HasherComparator* hc) {
    size_t newSize = RoundToPowerOf2(h->nEntries + 1);
    CrashIf(newSize <= h->nEntries);
    HashTableEntry** newEntries = AllocArray<HashTableEntry*>(newSize);
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

// micro optimization: this is called often, so we want this check inlined. Resizing logic
// is called rarely, so doesn't need to be inlined
static inline void HashTableResizeIfNeeded(HashTable* h, HasherComparator* hc) {
    // per http://stackoverflow.com/questions/1603712/when-should-i-do-rehashing-of-entire-hash-table/1604428#1604428
    // when using collision chaining, load factor can be 150%
    if (h->nUsed < (h->nEntries * 3) / 2)
        return;
    HashTableResize(h, hc);
}

// note: allocator must be nullptr for get, non-nullptr for create
static HashTableEntry* GetOrCreateEntry(HashTable* h, HasherComparator* hc, uintptr_t key, Allocator* allocator,
                                        bool& newEntry) {
    bool shouldCreate = (allocator != nullptr);
    size_t hash = hc->Hash(key);
    size_t pos = hash % h->nEntries;
    HashTableEntry* e = h->entries[pos];
    newEntry = false;
    while (e) {
        if (hc->Equal(key, e->key))
            return e;
        e = e->next;
    }
    if (!shouldCreate)
        return nullptr;

    if (h->freeList) {
        e = h->freeList;
        h->freeList = h->freeList->next;
    } else {
        e = (HashTableEntry*)Allocator::AllocZero(allocator, sizeof(HashTableEntry));
    }
    e->next = h->entries[pos];
    h->entries[pos] = e;
    h->nUsed++;
    if (e->next != nullptr)
        h->nCollisions++;
    newEntry = true;
    return e;
}

static bool RemoveEntry(HashTable* h, HasherComparator* hc, uintptr_t key, uintptr_t* removedValOut) {
    size_t hash = hc->Hash(key);
    size_t pos = hash % h->nEntries;
    HashTableEntry* e = h->entries[pos];
    while (e) {
        if (hc->Equal(key, e->key))
            break;
        e = e->next;
    }
    if (!e)
        return false;

    // remove the the entry from the list
    HashTableEntry* e2 = h->entries[pos];
    if (e2 == e) {
        h->entries[pos] = e->next;
    } else {
        while (e2->next != e) {
            e2 = e2->next;
        }
        e2->next = e->next;
    }
    e->next = h->freeList;
    h->freeList = e;
    *removedValOut = e->val;
    CrashIf(0 == h->nUsed);
    h->nUsed -= 1;
    return true;
}

MapStrToInt::MapStrToInt(size_t initialSize) {
    // we use PoolAllocator to allocate HashTableEntry entries
    // and copies of string keys
    allocator = new PoolAllocator();
    allocator->SetAllocRounding(4);
    h = NewHashTable(initialSize, allocator);
}

MapStrToInt::~MapStrToInt() {
    DeleteHashTable(h);
    delete allocator;
}

size_t MapStrToInt::Count() const {
    return h->nUsed;
}

// if a key exists:
//   * returns false
//   * sets existingValOut to existing value
//   * sets existingKeyOut to (interned) existing key

// if a key doesn't exist:
//   * returns true
//   * inserts a copy of the key allocated with allocator
//   * sets existingKeyOut to (interned) key
bool MapStrToInt::Insert(const char* key, int val, int* existingValOut, const char** existingKeyOut) {
    bool newEntry;
    HashTableEntry* e = GetOrCreateEntry(h, &gStrKeyHasherComparator, (uintptr_t)key, allocator, newEntry);
    if (!newEntry) {
        if (existingValOut)
            *existingValOut = (int)e->val;
        if (existingKeyOut)
            *existingKeyOut = (const char*)e->key;
        return false;
    }
    e->key = (intptr_t)Allocator::StrDup(allocator, key);
    e->val = (intptr_t)val;
    if (existingKeyOut)
        *existingKeyOut = (const char*)e->key;

    HashTableResizeIfNeeded(h, &gStrKeyHasherComparator);
    return true;
}

bool MapStrToInt::Remove(const char* key, int* removedValOut) {
    uintptr_t removedVal;
    bool removed = RemoveEntry(h, &gStrKeyHasherComparator, (uintptr_t)key, &removedVal);
    if (removed && removedValOut)
        *removedValOut = (int)removedVal;
    return removed;
}

bool MapStrToInt::Get(const char* key, int* valOut) {
    StrKeyHasherComparator hc;
    bool newEntry;
    HashTableEntry* e = GetOrCreateEntry(h, &hc, (uintptr_t)key, nullptr, newEntry);
    if (!e)
        return false;
    *valOut = (int)e->val;
    return true;
}

MapWStrToInt::MapWStrToInt(size_t initialSize) {
    // we use PoolAllocator to allocate HashTableEntry entries
    // and copies of string keys
    allocator = new PoolAllocator();
    allocator->SetAllocRounding(4);
    h = NewHashTable(initialSize, allocator);
}

MapWStrToInt::~MapWStrToInt() {
    DeleteHashTable(h);
    delete allocator;
}

size_t MapWStrToInt::Count() const {
    return h->nUsed;
}

bool MapWStrToInt::Insert(const WCHAR* key, int val, int* prevVal) {
    bool newEntry;
    HashTableEntry* e = GetOrCreateEntry(h, &gWStrKeyHasherComparator, (uintptr_t)key, allocator, newEntry);
    if (!newEntry) {
        if (prevVal)
            *prevVal = (int)e->val;
        return false;
    }
    e->key = (intptr_t)Allocator::StrDup(allocator, key);
    e->val = (intptr_t)val;

    HashTableResizeIfNeeded(h, &gWStrKeyHasherComparator);
    return true;
}

bool MapWStrToInt::Remove(const WCHAR* key, int* removedValOut) {
    uintptr_t removedVal;
    bool removed = RemoveEntry(h, &gStrKeyHasherComparator, (uintptr_t)key, &removedVal);
    if (removed && removedValOut)
        *removedValOut = (int)removedVal;
    return removed;
}

bool MapWStrToInt::Get(const WCHAR* key, int* valOut) {
    WStrKeyHasherComparator hc;
    bool newEntry;
    HashTableEntry* e = GetOrCreateEntry(h, &hc, (uintptr_t)key, nullptr, newEntry);
    if (!e)
        return false;
    *valOut = (int)e->val;
    return true;
}

} // namespace dict

int StringInterner::Intern(const char* s, bool* alreadyPresent) {
    nInternCalls++;
    int idx = (int)intToStr.size();
    const char* internedString;
    bool inserted = strToInt.Insert(s, idx, &idx, &internedString);
    if (!inserted) {
        if (alreadyPresent)
            *alreadyPresent = true;
        return idx;
    }

    intToStr.Append(internedString);
    if (alreadyPresent)
        *alreadyPresent = false;
    return idx;
}

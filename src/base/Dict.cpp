/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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

#include "base/Base.h"
#include "base/Dict.h"

namespace dict {

class HasherComparator {
  public:
    virtual size_t Hash(uintptr_t key) = 0;
    virtual bool Equal(uintptr_t k1, uintptr_t k2) = 0;
    virtual ~HasherComparator() = default;
};

static Str KeyAsStr(uintptr_t key) {
    // interned hash table keys are NUL-terminated
    return Str((char*)key);
}

static WStr KeyAsWStr(uintptr_t key) {
    return WStr((wchar_t*)key);
}

class StrKeyHasherComparator : public HasherComparator {
    size_t Hash(uintptr_t key) override {
        Str s = KeyAsStr(key);
        return MurmurHash2(s);
    }
    bool Equal(uintptr_t k1, uintptr_t k2) override { return str::Eq(KeyAsStr(k1), KeyAsStr(k2)); }
};

class WStrKeyHasherComparator : public HasherComparator {
    size_t Hash(uintptr_t key) override {
        WStr s = KeyAsWStr(key);
        return MurmurHash2(s);
    }
    bool Equal(uintptr_t k1, uintptr_t k2) override { return wstr::Eq(KeyAsWStr(k1), KeyAsWStr(k2)); }
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

static HashTable* NewHashTable(size_t size, Arena* allocator) {
    ReportIf(!allocator); // we'll leak otherwise
    HashTable* h = AllocArray<HashTable>(allocator, 1);
    // number of hash table entries should be power of 2
    size = RoundToPowerOf2((int)size);
    // entries are not allocated with allocator since those are large blocks
    // and we don't want to waste their memory after
    h->entries = AllocArray<HashTableEntry*>((int)size);
    h->nEntries = size;
    return h;
}

static void DeleteHashTable(HashTable* h) {
    free(h->entries);
    // the rest is freed by allocator
}

static void HashTableResize(HashTable* h, HasherComparator* hc) {
    size_t newSize = RoundToPowerOf2((int)(h->nEntries + 1));
    ReportIf(newSize <= h->nEntries);
    HashTableEntry** newEntries = AllocArray<HashTableEntry*>((int)newSize);
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

    ReportIf(h->nUsed >= (h->nEntries * 3) / 2);
}

// micro optimization: this is called often, so we want this check inlined. Resizing logic
// is called rarely, so doesn't need to be inlined
static inline void HashTableResizeIfNeeded(HashTable* h, HasherComparator* hc) {
    // per http://stackoverflow.com/questions/1603712/when-should-i-do-rehashing-of-entire-hash-table/1604428#1604428
    // when using collision chaining, load factor can be 150%
    if (h->nUsed < (h->nEntries * 3) / 2) {
        return;
    }
    HashTableResize(h, hc);
}

// note: allocator must be nullptr for get, non-nullptr for create
static HashTableEntry* GetOrCreateEntry(HashTable* h, HasherComparator* hc, uintptr_t key, Arena* allocator,
                                        bool& newEntry) {
    bool shouldCreate = (allocator != nullptr);
    size_t hash = hc->Hash(key);
    size_t pos = hash % h->nEntries;
    HashTableEntry* e = h->entries[pos];
    newEntry = false;
    while (e) {
        if (hc->Equal(key, e->key)) {
            return e;
        }
        e = e->next;
    }
    if (!shouldCreate) {
        return nullptr;
    }

    if (h->freeList) {
        e = h->freeList;
        h->freeList = h->freeList->next;
    } else {
        e = AllocArray<HashTableEntry>(allocator, 1);
    }
    e->next = h->entries[pos];
    h->entries[pos] = e;
    h->nUsed++;
    if (e->next != nullptr) {
        h->nCollisions++;
    }
    newEntry = true;
    return e;
}

static bool RemoveEntry(HashTable* h, HasherComparator* hc, uintptr_t key, uintptr_t* removedValOut) {
    size_t hash = hc->Hash(key);
    size_t pos = hash % h->nEntries;
    HashTableEntry* e = h->entries[pos];
    while (e) {
        if (hc->Equal(key, e->key)) {
            break;
        }
        e = e->next;
    }
    if (!e) {
        return false;
    }

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
    ReportIf(0 == h->nUsed);
    h->nUsed -= 1;
    return true;
}

MapStrToInt::MapStrToInt(size_t initialSize) {
    // arena-allocate HashTableEntry entries and copies of string keys
    allocator = ArenaNew();
    h = NewHashTable(initialSize, allocator);
}

MapStrToInt::~MapStrToInt() {
    DeleteHashTable(h);
    ArenaDelete(allocator);
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
bool MapStrToInt::Insert(Str key, int val, int* existingValOut, Str* existingKeyOut) {
    if (len(key) == 0) {
        return false;
    }
    bool newEntry;
    HashTableEntry* e = GetOrCreateEntry(h, &gStrKeyHasherComparator, (uintptr_t)CStrTemp(key), allocator, newEntry);
    if (!newEntry) {
        if (existingValOut) {
            *existingValOut = (int)e->val;
        }
        if (existingKeyOut) {
            *existingKeyOut = KeyAsStr(e->key);
        }
        return false;
    }
    e->key = (intptr_t)str::Dup(allocator, key).s;
    e->val = (intptr_t)val;
    if (existingKeyOut) {
        *existingKeyOut = KeyAsStr(e->key);
    }

    HashTableResizeIfNeeded(h, &gStrKeyHasherComparator);
    return true;
}

bool MapStrToInt::Remove(Str key, int* removedValOut) const {
    if (len(key) == 0) {
        return false;
    }
    uintptr_t removedVal;
    bool removed = RemoveEntry(h, &gStrKeyHasherComparator, (uintptr_t)CStrTemp(key), &removedVal);
    if (removed && removedValOut) {
        *removedValOut = (int)removedVal;
    }
    return removed;
}

bool MapStrToInt::Get(Str key, int* valOut) const {
    if (len(key) == 0) {
        return false;
    }
    StrKeyHasherComparator hc;
    bool newEntry;
    HashTableEntry* e = GetOrCreateEntry(h, &hc, (uintptr_t)CStrTemp(key), nullptr, newEntry);
    if (!e) {
        return false;
    }
    *valOut = (int)e->val;
    return true;
}

} // namespace dict

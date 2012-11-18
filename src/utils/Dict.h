/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Dict_h
#define Dict_h

namespace dict {

struct HashTable;

// we are very generous with default initial size. It's a trade-off
// between memory used by hash table and how often we need to resize it.
// We allocate size*sizeof(ptr) which is 64k on 32-bit for 16k entries.
// 64k is very little on today's machines, especially for short-lived
// hash tables.
// Should use smaller values for long-lived hash tables, especially
// if there are many of them.
enum { DEFAULT_HASH_TABLE_INITIAL_SIZE = 16*1024 };

// a dictionary whose keys are char * strings and the values are integers
// note: StrToInt would be more natural name but it's re-#define'd in <shlwapi.h>
class MapStrToInt {
public:
    PoolAllocator *allocator;
    HashTable *h;

    MapStrToInt(size_t initialSize = DEFAULT_HASH_TABLE_INITIAL_SIZE);
    ~MapStrToInt();

    // if a key doesn't exist, inserts a key with a given value and return true
    // if a key exists, returns false and sets prevValOut to existing value
    bool Insert(const char *key, int val, int *prevValOut);

    bool GetValue(const char *key, int *valOut);
};

class MapTStrToInt {
public:
    PoolAllocator *allocator;
    HashTable *h;

    MapTStrToInt(size_t initialSize=DEFAULT_HASH_TABLE_INITIAL_SIZE);
    ~MapTStrToInt();

    // if a key doesn't exist, inserts a key with a given value and return true
    // if a key exists, returns false and sets prevValOut to existing value
    bool Insert(const TCHAR *key, int val, int *prevValOut);

    bool GetValue(const TCHAR *key, int *valOut);
};

}

#endif

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace dict {

struct HashTable;

// we are very generous with default initial size. It's a trade-off
// between memory used by hash table and how often we need to resize it.
// We allocate size*sizeof(ptr) which is 64k on 32-bit for 16k entries.
// 64k is very little on today's machines, especially for short-lived
// hash tables.
// Should use smaller values for long-lived hash tables, especially
// if there are many of them.
enum {
    DEFAULT_HASH_TABLE_INITIAL_SIZE = 16 * 1024
};

// a dictionary whose keys are strings and the values are integers
// note: StrToInt would be more natural name but it's re-#define'd in <shlwapi.h>
class MapStrToInt {
  public:
    Arena* a = nullptr;
    HashTable* h = nullptr;

    explicit MapStrToInt(int initialSize = DEFAULT_HASH_TABLE_INITIAL_SIZE);
    ~MapStrToInt();

    int Count() const;

    bool Insert(Str key, int val, int* existingValOut = nullptr, Str* existingKeyOut = nullptr);

    bool Remove(Str key, int* removedValOut) const;
    bool Get(Str key, int* valOut) const;
};

} // namespace dict

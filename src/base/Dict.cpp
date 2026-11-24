/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
A string -> int dictionary, built to use little memory.

Open addressing with linear probing: one array of 12-byte slots
(hash, key offset, value), kept at most 3/4 full. The keys live back to back
in one growable buffer, each as a varint length and its bytes, so there is no
per-entry allocation, padding or pointer.

Lookups compare the stored hash first, then the length, then the bytes; a
resize re-places slots by their stored hash without hashing keys again.
Remove() uses backward-shift deletion (no tombstones); a removed key's bytes
stay in the buffer until the dictionary is deleted.

        slots (power of 2)                 keys
   +------+--------+-----+
   | hash | keyOff | val | --keyOff--> [len][k e y b y t e s][len][...]
   +------+--------+-----+
   |  0   |   0    |  -  |  <- empty
*/

#include "base/Base.h"
#include "base/Dict.h"

namespace dict {

struct MapSlot {
    u32 hash;
    u32 keyOff; // 0: the slot is empty (the keys buffer starts with a spare byte)
    int val;
};

// a hint above this doesn't pre-allocate more: the table grows when needed
constexpr int kMaxInitialEntries = 1024;
constexpr int kMinSlots = 8;
constexpr int kMaxVarintLen = 5;

static int VarintEncode(u8* d, u32 v) {
    int n = 0;
    while (v >= 0x80) {
        d[n++] = (u8)(v | 0x80);
        v >>= 7;
    }
    d[n++] = (u8)v;
    return n;
}

static const u8* VarintDecode(const u8* p, u32* v) {
    u32 res = 0;
    int shift = 0;
    for (;;) {
        u8 b = *p++;
        res |= (u32)(b & 0x7f) << shift;
        if (b < 0x80) {
            break;
        }
        shift += 7;
    }
    *v = res;
    return p;
}

StrToInt::StrToInt(int initialSize) {
    int entries = limitValue(initialSize, 1, kMaxInitialEntries);
    // room for that many entries at the 3/4 load limit
    firstSlots = std::max(kMinSlots, RoundToPowerOf2(entries + (entries / 3) + 1));
}

StrToInt::~StrToInt() {
    free(slots);
    free(keys);
}

int StrToInt::Count() const {
    return nUsed;
}

bool StrToInt::KeyEq(u32 keyOff, Str key) const {
    u32 n = 0;
    const u8* p = VarintDecode((const u8*)keys + keyOff, &n);
    return n == (u32)len(key) && memcmp(p, key.s, n) == 0;
}

// the slot holding key, or the empty slot where it would go
int StrToInt::Find(Str key, u32 hash) const {
    u32 mask = (u32)nSlots - 1;
    for (u32 i = hash & mask;; i = (i + 1) & mask) {
        const MapSlot& s = slots[i];
        if (s.keyOff == 0) {
            return (int)i;
        }
        if (s.hash == hash && KeyEq(s.keyOff, key)) {
            return (int)i;
        }
    }
}

// the offset of the copy of key, 0 if out of memory
u32 StrToInt::AppendKey(Str key) {
    u32 n = (u32)len(key);
    u64 need = (u64)keysLen + kMaxVarintLen + n + (keysLen == 0 ? 1 : 0);
    if (need > UINT32_MAX) {
        return 0;
    }
    if (need > keysCap) {
        // 1.5x: less slack than doubling, still amortized
        u64 newCap = std::max<u64>(need, (u64)keysCap + (keysCap / 2) + 64);
        newCap = std::min<u64>(newCap, UINT32_MAX);
        char* newKeys = (char*)realloc(keys, (size_t)newCap);
        if (!newKeys) {
            return 0;
        }
        keys = newKeys;
        keysCap = (u32)newCap;
    }
    if (keysLen == 0) {
        keysLen = 1; // offset 0 marks an empty slot
    }
    u32 off = keysLen;
    keysLen += VarintEncode((u8*)keys + keysLen, n);
    memcpy(keys + keysLen, key.s, n);
    keysLen += n;
    return off;
}

bool StrToInt::Grow() {
    int newN = nSlots == 0 ? firstSlots : nSlots * 2;
    MapSlot* newSlots = AllocArray<MapSlot>(newN);
    if (!newSlots) {
        return false;
    }
    u32 mask = (u32)newN - 1;
    for (int i = 0; i < nSlots; i++) {
        const MapSlot& s = slots[i];
        if (s.keyOff == 0) {
            continue;
        }
        u32 j = s.hash & mask;
        while (newSlots[j].keyOff != 0) {
            j = (j + 1) & mask;
        }
        newSlots[j] = s;
    }
    free(slots);
    slots = newSlots;
    nSlots = newN;
    return true;
}

// if key exists: returns false and sets existingValOut to its value
// if it doesn't: inserts a copy of key with val and returns true
bool StrToInt::Insert(Str key, int val, int* existingValOut) {
    if (len(key) == 0) {
        return false;
    }
    u32 hash = MurmurHash2(key);
    int i = -1;
    if (nSlots > 0) {
        i = Find(key, hash);
        if (slots[i].keyOff != 0) {
            if (existingValOut) {
                *existingValOut = slots[i].val;
            }
            return false;
        }
    }
    // keep at most 3/4 of the slots used, so a probe always ends
    if (nSlots == 0 || (nUsed + 1) * 4 > nSlots * 3) {
        if (!Grow()) {
            return false;
        }
        i = Find(key, hash);
    }
    u32 off = AppendKey(key);
    if (off == 0) {
        return false;
    }
    slots[i] = MapSlot{hash, off, val};
    nUsed++;
    return true;
}

bool StrToInt::Remove(Str key, int* removedValOut) {
    if (len(key) == 0 || nSlots == 0) {
        return false;
    }
    u32 i = (u32)Find(key, MurmurHash2(key));
    if (slots[i].keyOff == 0) {
        return false;
    }
    if (removedValOut) {
        *removedValOut = slots[i].val;
    }
    // backward-shift: move later entries of the run into the hole when their
    // home slot is at or before it, so every probe still finds them
    u32 mask = (u32)nSlots - 1;
    u32 j = i;
    for (;;) {
        j = (j + 1) & mask;
        if (slots[j].keyOff == 0) {
            break;
        }
        u32 home = slots[j].hash & mask;
        bool homeInHoleToJ = (i <= j) ? (i < home && home <= j) : (i < home || home <= j);
        if (!homeInHoleToJ) {
            slots[i] = slots[j];
            i = j;
        }
    }
    slots[i] = MapSlot{};
    nUsed--;
    return true;
}

bool StrToInt::Get(Str key, int* valOut) const {
    if (len(key) == 0 || nSlots == 0) {
        return false;
    }
    const MapSlot& s = slots[Find(key, MurmurHash2(key))];
    if (s.keyOff == 0) {
        return false;
    }
    *valOut = s.val;
    return true;
}

} // namespace dict

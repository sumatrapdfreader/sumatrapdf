/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* This is a trie-hash for strings. It maps a binary string (i.e. can
contain embedded 0) to a value of arbitrary type.

Trie-hash means that it's actually crit-bit tree where the keys
are 32-bit hash values of the string. The advantage of this over
regular hash tree is that it never requires expensive rehashing
when resizing.

For maximum compactness, we have restrictions:
- we can't insert 2 strings with the same values
- instead of storing pointers, we store 16-bit offsets into memory we
  own, so there's a limit on number of nodes in the tree and total
  size of strings we can store

Implementation notes.
---------------------

We have 2 kinds of nodes in the tree. Internal node is a binary
tree node which has 2 children: one for bit 0 and another for bit 1,
32 nodes for each bit of 32-bit hash value. A node is 4-byte value, 2
16-bit offsets into array of nodes.

The last internal node points to a value node which (logically) consists
of string value (16-bit offset of the array of strings and 16-bit lenght
of the string) and value.

TODO: this is meant to be used for cache of string measurements in
HtmlFormatter.
*/

// non-templated base class where value is a blob of bytes of fixed
// size. We do that so we can have the implementation in cpp file
class StrHashNT {
    // 0 index means there's no child, otherwise it's 1-based index
    // into nodes array
    struct StrHashNode {
        uint16 leftIdx;
        uint16 rightIdx;
    };
    struct StrKey {
        uint16 idx; // points withing stringAllocator
        uint16 len;
    };

    PoolAllocator             stringAllocator;
    VecSegmented<StrHashNode> nodes;
    VecSegmented<StrKey>      strKeys;
    VecSegmented<char>        values;
    size_t                    valueSize;
public:
    explicit StrHashNT(size_t valueSize) : valueSize(valueSize)
    { }
    void *Create(const char *s, size_t strLen, uint32 hash, bool& createdOut) {
        return NULL; // TODO: write me
    }

    void *Lookup(const char *s, size_t strLen, bool createIfNotExists, bool& createdOut) {
        createdOut = false;
        uint32 hash = CalcStrHash(s, strLen);
        if (0 == nodex.Count()) {
            if (createIfNotExists)
                return Create(s, strLen, hash, createdOut);
            return NULL;
        }
        size_t nodeIdx = 0;
        StrHashNode node;
        for (size_t i = 0; i < 32; i++) {
            node = nodes.At(nodeIdx);
            bool left = ((hash & 1) == 1);
            hash >>= 1;
            if (left)
                nodeIdx = node.leftIdx;
            else
                nodeIdx = node.rightIdx;
            if (0 == nodeIdx) {
                if (createIfNotExists)
                    return Create(s, strLen, hash, createdOut);
                return NULL;
            }
        }
        // at this point nodeIdx is index within values and strKey arrays
        size_t valueIdx = nodeIdx * valueSize;
        CrashIf(valueIdx + valueSize >= values.Size());
        char *ret = values.AtPtr(valueIdx);
        return (void*)ret;
    }
};

<template V>
class StrHash : public StrHashNT {
public:
    // TODO: sizeof(V) should probably be rounded to 4 or 8
    StrHash() : StrHashNT(sizeof(V))
    { }
    V *Lookup(const char *s, size_t strLen, bool createIfNotExists, bool& createdOut) {
        V *res = (V*)Lookup(s, strLen, createIfNotExists, createdOut);
        return res;
    }
};

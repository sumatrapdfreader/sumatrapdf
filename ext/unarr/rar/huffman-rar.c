/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

/* adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/XADPrefixCode.m */

#include "rar.h"

bool rar_new_node(struct huffman_code *code)
{
    if (!code->tree) {
        code->minlength = INT_MAX;
        code->maxlength = INT_MIN;
    }
    if (code->numentries + 1 >= code->capacity) {
        /* in my small file sample, 1024 is the value needed most often */
        int new_capacity = code->capacity ? code->capacity * 2 : 1024;
        void *new_tree = calloc(new_capacity, sizeof(*code->tree));
        if (!new_tree) {
            warn("OOM during decompression");
            return false;
        }
        memcpy(new_tree, code->tree, code->capacity * sizeof(*code->tree));
        free(code->tree);
        code->tree = new_tree;
        code->capacity = new_capacity;
    }
    code->tree[code->numentries].branches[0] = -1;
    code->tree[code->numentries].branches[1] = -2;
    code->numentries++;
    return true;
}

bool rar_add_value(struct huffman_code *code, int value, int codebits, int length)
{
    int lastnode, bitpos, bit;

    free(code->table);
    code->table = NULL;

    if (length > code->maxlength)
        code->maxlength = length;
    if (length < code->minlength)
        code->minlength = length;

    lastnode = 0;
    for (bitpos = length - 1; bitpos >= 0; bitpos--) {
        bit = (codebits >> bitpos) & 1;
        if (rar_is_leaf_node(code, lastnode)) {
            warn("Invalid data in bitstream"); /* prefix found */
            return false;
        }
        if (code->tree[lastnode].branches[bit] < 0) {
            if (!rar_new_node(code))
                return false;
            code->tree[lastnode].branches[bit] = code->numentries - 1;
        }
        lastnode = code->tree[lastnode].branches[bit];
    }

    if (code->tree[lastnode].branches[0] != -1 || code->tree[lastnode].branches[1] != -2) {
        warn("Invalid data in bitstream"); /* prefix found */
        return false;
    }
    code->tree[lastnode].branches[0] = code->tree[lastnode].branches[1] = value;
    return true;
}

bool rar_create_code(struct huffman_code *code, uint8_t *lengths, int numsymbols)
{
    int symbolsleft = numsymbols;
    int codebits = 0;
    int i, j;

    if (!rar_new_node(code))
        return false;

    for (i = 1; i <= 0x0F; i++) {
        for (j = 0; j < numsymbols; j++) {
            if (lengths[j] != i)
                continue;
            if (!rar_add_value(code, j, codebits, i))
                return false;
            if (--symbolsleft <= 0)
                return true;
            codebits++;
        }
        codebits <<= 1;
    }
    return true;
}

static bool rar_make_table_rec(struct huffman_code *code, int node, int offset, int depth, int maxdepth)
{
    int currtablesize = 1 << (maxdepth - depth);

    if (node < 0 || code->numentries <= node) {
        warn("Invalid data in bitstream"); /* invalid location to Huffman tree specified */
        return false;
    }

    if (rar_is_leaf_node(code, node)) {
        int i;
        for (i = 0; i < currtablesize; i++) {
            code->table[offset + i].length = depth;
            code->table[offset + i].value = code->tree[node].branches[0];
        }
    }
    else if (depth == maxdepth) {
        code->table[offset].length = maxdepth + 1;
        code->table[offset].value = node;
    }
    else {
        if (!rar_make_table_rec(code, code->tree[node].branches[0], offset, depth + 1, maxdepth))
            return false;
        if (!rar_make_table_rec(code, code->tree[node].branches[1], offset + currtablesize / 2, depth + 1, maxdepth))
            return false;
    }
    return true;
}

bool rar_make_table(struct huffman_code *code)
{
    if (code->minlength <= code->maxlength && code->maxlength <= 10)
        code->tablesize = code->maxlength;
    else
        code->tablesize = 10;

    code->table = calloc(1ULL << code->tablesize, sizeof(*code->table));
    if (!code->table) {
        warn("OOM during decompression");
        return false;
    }

    return rar_make_table_rec(code, 0, 0, 0, code->tablesize);
}

void rar_free_code(struct huffman_code *code)
{
    free(code->tree);
    free(code->table);
    memset(code, 0, sizeof(*code));
}

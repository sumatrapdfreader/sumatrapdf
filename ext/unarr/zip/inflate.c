/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "inflate.h"
#include "../common/allocator.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef _MSC_VER
#define __forceinline inline
#endif

#define MAX_BITS 16
#define TREE_FAST_BITS 10
#define MAX_TREE_NODES 288

enum inflate_step {
    STEP_NEXT_BLOCK = 0,
    STEP_COPY_INIT, STEP_COPY,
    STEP_INFLATE_STATIC_INIT, STEP_INFLATE_DYNAMIC_INIT, STEP_INFLATE_DYNAMIC_INIT_PRETREE, STEP_INFLATE_DYNAMIC_INIT_TREES,
    STEP_INFLATE_CODE, STEP_INFLATE, STEP_INFLATE_DISTANCE_CODE, STEP_INFLATE_DISTANCE, STEP_INFLATE_REPEAT,
};
enum { RESULT_EOS = -1, RESULT_NOT_DONE = 0, RESULT_ERROR = 1 };

#if defined(_MSC_VER) || defined(__GNUC__)
#define RESULT_ERROR (RESULT_ERROR + __COUNTER__)
#endif

struct tree {
    struct {
        unsigned value : 11;
        unsigned is_value : 1;
        unsigned length : 4;
    } nodes[(1 << TREE_FAST_BITS) + MAX_TREE_NODES * 2];
    int next_node;
};

struct inflate_state_s {
    enum inflate_step step;
    struct {
        int value;
        int length;
        int dist;
        int tree_idx;
    } state;
    struct {
        int hlit;
        int hdist;
        int hclen;
        int idx;
        int clens[288 + 32];
    } prepare;
    bool inflate64;
    bool is_final_block;
    struct tree tree_lengths;
    struct tree tree_dists;
    struct {
        const uint8_t *data_in;
        size_t *avail_in;
        uint64_t bits;
        int available;
    } in;
    struct {
        uint8_t *data_out;
        size_t *avail_out;
        uint8_t window[1 << 16];
        size_t offset;
    } out;
};

static const struct {
    int bits;
    int length;
} table_lengths[30] = {
    { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 7 }, { 0, 8 }, { 0, 9 }, { 0, 10 },
    { 1, 11 }, { 1, 13 }, { 1, 15 }, { 1, 17 }, { 2, 19 }, { 2, 23 }, { 2, 27 }, { 2, 31 },
    { 3, 35 }, { 3, 43 }, { 3, 51 }, { 3, 59 }, { 4, 67 }, { 4, 83 }, { 4, 99 }, { 4, 115 },
    { 5, 131 }, { 5, 163 }, { 5, 195 }, { 5, 227 },
    { 0, 258 }, /* Deflate64 (replaces { 0, 258 }) */ { 16, 3 }
};

static const struct {
    int bits;
    int dist;
} table_dists[32] = {
    { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 1, 5 }, { 1, 7 },
    { 2, 9 }, { 2, 13 }, { 3, 17 }, { 3, 25 }, { 4, 33 }, { 4, 49 },
    { 5, 65 }, { 5, 97 }, { 6, 129 }, { 6, 193 }, { 7, 257 }, { 7, 385 },
    { 8, 513 }, { 8, 769 }, { 9, 1025 }, { 9, 1537 }, { 10, 2049 }, { 10, 3073 },
    { 11, 4097 }, { 11, 6145 }, { 12, 8193 }, { 12, 12289 }, { 13, 16385 }, { 13, 24577 },
    /* Deflate64 */ { 14, 32769 }, { 14, 49153 }
};

static const int table_code_length_idxs[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static __forceinline bool br_ensure(inflate_state *state, int bits)
{
    while (state->in.available < bits) {
        if (*state->in.avail_in == 0)
            return false;
        state->in.bits |= ((uint64_t)*state->in.data_in++ << state->in.available);
        (*state->in.avail_in)--;
        state->in.available += 8;
    }
    return true;
}

static __forceinline uint64_t br_bits(inflate_state *state, int bits)
{
    uint64_t res = state->in.bits & (((uint64_t)1 << bits) - 1);
    state->in.available -= bits;
    state->in.bits >>= bits;
    return res;
}

static __forceinline void output(inflate_state *state, uint8_t value)
{
    *state->out.data_out++ = value;
    (*state->out.avail_out)--;
    state->out.window[state->out.offset++ & (sizeof(state->out.window) - 1)] = value;
}

static bool tree_add_value(struct tree *tree, int key, int bits, int value)
{
    int rkey = 0, i;
    for (i = 0; i < bits; i++)
        rkey = (rkey << 1) | ((key >> i) & 1);

    if (bits <= TREE_FAST_BITS) {
        if (tree->nodes[rkey].length)
            return false;
        tree->nodes[rkey].length = bits;
        tree->nodes[rkey].value = value;
        tree->nodes[rkey].is_value = true;
        for (i = 1; i < (1 << (TREE_FAST_BITS - bits)); i++) {
            if (tree->nodes[rkey | (i << bits)].length)
                return false;
            tree->nodes[rkey | (i << bits)] = tree->nodes[rkey];
        }
        return true;
    }

    rkey &= (1 << TREE_FAST_BITS) - 1;
    if (tree->nodes[rkey].is_value)
        return false;
    tree->nodes[rkey].length = TREE_FAST_BITS + 1;
    if (!tree->nodes[rkey].value)
        tree->nodes[rkey].value = (1 << TREE_FAST_BITS) + tree->next_node++ * 2;
    i = tree->nodes[rkey].value;
    bits -= TREE_FAST_BITS;

    while (bits > 1) {
        i |= (key >> (bits - 1)) & 1;
        if (tree->nodes[i].is_value)
            return false;
        if (!tree->nodes[i].value) {
            if (tree->next_node == MAX_TREE_NODES)
                return false;
            tree->nodes[i].value = (1 << TREE_FAST_BITS) + tree->next_node++ * 2;
        }
        i = tree->nodes[i].value;
        bits--;
    }
    i |= key & 1;
    if (tree->nodes[i].value || tree->nodes[i].is_value)
        return false;
    tree->nodes[i].value = value;
    tree->nodes[i].is_value = true;

    return true;
}

static __forceinline int tree_get_value(inflate_state *state, const struct tree *tree, bool not_fast)
{
    if (state->state.tree_idx == 0) {
        int key = state->in.bits & ((1 << TREE_FAST_BITS) - 1);
        while (not_fast && state->in.available < TREE_FAST_BITS && state->in.available < (int)tree->nodes[key].length) {
            if (!br_ensure(state, tree->nodes[key].length))
                return RESULT_NOT_DONE;
            key = state->in.bits & ((1 << TREE_FAST_BITS) - 1);
        }
        if (tree->nodes[key].is_value) {
            state->state.value = tree->nodes[key].value;
            (void)br_bits(state, tree->nodes[key].length);
            return RESULT_EOS;
        }
        if (tree->nodes[key].length == 0)
            return RESULT_ERROR;
        (void)br_bits(state, TREE_FAST_BITS);
        state->state.tree_idx = tree->nodes[key].value;
    }
    while (state->state.value == -1) {
        int idx;
        if (not_fast && !br_ensure(state, 1))
            return RESULT_NOT_DONE;
        idx = state->state.tree_idx | (int)br_bits(state, 1);
        if (tree->nodes[idx].is_value)
            state->state.value = tree->nodes[idx].value;
        else if (tree->nodes[idx].value)
            state->state.tree_idx = tree->nodes[idx].value;
        else
            return RESULT_ERROR;
    }
    state->state.tree_idx = 0;
    return RESULT_EOS;
}

static void setup_static_trees(inflate_state *state)
{
    int i;

    memset(&state->tree_lengths, 0, sizeof(state->tree_lengths));
    for (i = 0; i < 144; i++)
        tree_add_value(&state->tree_lengths, i + 48, 8, i);
    for (i = 144; i < 256; i++)
        tree_add_value(&state->tree_lengths, i + 256, 9, i);
    for (i = 256; i < 280; i++)
        tree_add_value(&state->tree_lengths, i - 256, 7, i);
    for (i = 280; i < 288; i++)
        tree_add_value(&state->tree_lengths, i - 88, 8, i);

    memset(&state->tree_dists, 0, sizeof(state->tree_dists));
    for (i = 0; i < 32; i++)
        tree_add_value(&state->tree_dists, i, 5, i);
}

static bool setup_dynamic_tree(struct tree *tree, int *clens, int count)
{
    int code, i;
    int bl_count[MAX_BITS];
    int next_code[MAX_BITS];

    memset(bl_count, 0, sizeof(bl_count));
    for (i = 0; i < count; i++)
        bl_count[clens[i]]++;
    bl_count[0] = 0;

    code = 0;
    for (i = 1; i < MAX_BITS; i++) {
        code = (code + bl_count[i - 1]) << 1;
        next_code[i] = code;
    }

    memset(tree, 0, sizeof(*tree));
    for (i = 0; i < count; i++) {
        if (clens[i] != 0) {
            if (!tree_add_value(tree, next_code[clens[i]], clens[i], i))
                return false;
            next_code[clens[i]]++;
        }
    }

    return true;
}

inflate_state *inflate_create(bool inflate64)
{
    inflate_state *state = calloc(1, sizeof(inflate_state));
    if (state)
        state->inflate64 = inflate64;
    return state;
}

void inflate_free(inflate_state *state)
{
    free(state);
}

int inflate_process(inflate_state *state, const void *data_in, size_t *avail_in, void *data_out, size_t *avail_out)
{
    bool not_fast = true;
    int res;

    if (!state || !data_in || !avail_in || !data_out || !avail_out)
        return RESULT_ERROR;

    state->in.data_in = data_in;
    state->in.avail_in = avail_in;
    state->out.data_out = data_out;
    state->out.avail_out = avail_out;

    for (;;) {
        switch (state->step) {
        case STEP_NEXT_BLOCK:
            if (state->is_final_block)
                return RESULT_EOS;

            if (!br_ensure(state, 3))
                return RESULT_NOT_DONE;
            state->is_final_block = br_bits(state, 1) != 0;
            switch (br_bits(state, 2)) {
            case 0:
                state->step = STEP_COPY_INIT;
                break;
            case 1:
                state->step = STEP_INFLATE_STATIC_INIT;
                break;
            case 2:
                state->step = STEP_INFLATE_DYNAMIC_INIT;
                break;
            default:
                return RESULT_ERROR;
            }
            break;

        case STEP_COPY_INIT:
            if (!br_ensure(state, 32))
                return RESULT_NOT_DONE;
            (void)br_bits(state, state->in.available & 0x7);
            state->state.length = (uint16_t)br_bits(state, 16);
            if (state->state.length != 0xFFFF - (uint16_t)br_bits(state, 16))
                return RESULT_ERROR;
            state->step = STEP_COPY;
            /* fall through */

        case STEP_COPY:
            while (state->state.length > 0) {
                if (!br_ensure(state, 8) || *avail_out == 0)
                    return RESULT_NOT_DONE;
                output(state, (uint8_t)br_bits(state, 8));
                state->state.length--;
            }
            state->step = STEP_NEXT_BLOCK;
            break;

        case STEP_INFLATE_STATIC_INIT:
            setup_static_trees(state);
            /* fall through */

        STEP_INFLATE_START:
            not_fast = !br_ensure(state, state->inflate64 ? 49 : 48);
            state->state.value = -1;
            /* fall through */

        case STEP_INFLATE_CODE:
            res = tree_get_value(state, &state->tree_lengths, not_fast);
            if (res != RESULT_EOS) {
                state->step = STEP_INFLATE_CODE;
                return res;
            }
            /* fall through */

        case STEP_INFLATE:
            if (state->state.value < 256) {
                if (*avail_out == 0) {
                    state->step = STEP_INFLATE;
                    return RESULT_NOT_DONE;
                }
                output(state, (uint8_t)state->state.value);
                goto STEP_INFLATE_START;
            }
            if (state->state.value == 256) {
                state->step = STEP_NEXT_BLOCK;
                break;
            }
            if (state->state.value > 285)
                return RESULT_ERROR;
            if (state->inflate64 && state->state.value == 285) {
                not_fast = !br_ensure(state, 45);
                state->state.value = 286;
            }
            if (not_fast && !br_ensure(state, table_lengths[state->state.value - 257].bits)) {
                state->step = STEP_INFLATE;
                return RESULT_NOT_DONE;
            }
            state->state.length = table_lengths[state->state.value - 257].length + (int)br_bits(state, table_lengths[state->state.value - 257].bits);
            state->state.value = -1;
            /* fall through */

        case STEP_INFLATE_DISTANCE_CODE:
            res = tree_get_value(state, &state->tree_dists, not_fast);
            if (res != RESULT_EOS) {
                state->step = STEP_INFLATE_DISTANCE_CODE;
                return res;
            }
            /* fall through */

        case STEP_INFLATE_DISTANCE:
            if (not_fast && !br_ensure(state, table_dists[state->state.value].bits)) {
                state->step = STEP_INFLATE_DISTANCE;
                return RESULT_NOT_DONE;
            }
            state->state.dist = table_dists[state->state.value].dist + (int)br_bits(state, table_dists[state->state.value].bits);
            if ((size_t)state->state.dist > state->out.offset || (state->state.value > 30 && !state->inflate64))
                return RESULT_ERROR;
            state->step = STEP_INFLATE_REPEAT;
            /* fall through */

        case STEP_INFLATE_REPEAT:
            while (state->state.length > 0) {
                if (*avail_out == 0)
                    return RESULT_NOT_DONE;
                output(state, state->out.window[(state->out.offset - state->state.dist) & (sizeof(state->out.window) - 1)]);
                state->state.length--;
            }
            goto STEP_INFLATE_START;

        case STEP_INFLATE_DYNAMIC_INIT:
            if (!br_ensure(state, 14))
                return RESULT_NOT_DONE;
            state->prepare.hlit = (int)br_bits(state, 5) + 257;
            state->prepare.hdist = (int)br_bits(state, 5) + 1;
            state->prepare.hclen = (int)br_bits(state, 4) + 4;
            memset(state->prepare.clens, 0, sizeof(state->prepare.clens));
            state->prepare.idx = 0;
            state->step = STEP_INFLATE_DYNAMIC_INIT_PRETREE;
            /* fall through */

        case STEP_INFLATE_DYNAMIC_INIT_PRETREE:
            while (state->prepare.idx < state->prepare.hclen) {
                if (!br_ensure(state, 3))
                    return RESULT_NOT_DONE;
                state->prepare.clens[table_code_length_idxs[state->prepare.idx]] = (int)br_bits(state, 3);
                state->prepare.idx++;
            }
            if (!setup_dynamic_tree(&state->tree_lengths, state->prepare.clens, 19))
                return RESULT_ERROR;
            memset(state->prepare.clens, 0, sizeof(state->prepare.clens));
            state->prepare.idx = 0;
            state->state.value = -1;
            state->step = STEP_INFLATE_DYNAMIC_INIT_TREES;
            /* fall through */

        case STEP_INFLATE_DYNAMIC_INIT_TREES:
            while (state->prepare.idx < state->prepare.hlit + state->prepare.hdist) {
                int value = 0, repeat = 0;
                if (state->state.value == -1) {
                    res = tree_get_value(state, &state->tree_lengths, true);
                    if (res != RESULT_EOS)
                        return res;
                }
                if (state->state.value < 16) {
                    state->prepare.clens[state->prepare.idx++] = state->state.value;
                }
                else if (state->state.value == 16) {
                    if (state->prepare.idx == 0)
                        return RESULT_ERROR;
                    if (!br_ensure(state, 2))
                        return RESULT_NOT_DONE;
                    value = state->prepare.clens[state->prepare.idx - 1];
                    repeat = (int)br_bits(state, 2) + 3;
                }
                else if (state->state.value == 17) {
                    if (!br_ensure(state, 3))
                        return RESULT_NOT_DONE;
                    value = 0;
                    repeat = (int)br_bits(state, 3) + 3;
                }
                else {
                    if (!br_ensure(state, 7))
                        return RESULT_NOT_DONE;
                    value = 0;
                    repeat = (int)br_bits(state, 7) + 11;
                }
                if (repeat) {
                    if (state->prepare.idx + repeat > state->prepare.hlit + state->prepare.hdist)
                        return RESULT_ERROR;
                    while (repeat-- > 0)
                        state->prepare.clens[state->prepare.idx++] = value;
                }
                state->state.value = -1;
            }
            if (!setup_dynamic_tree(&state->tree_lengths, state->prepare.clens, state->prepare.hlit))
                return RESULT_ERROR;
            if (!setup_dynamic_tree(&state->tree_dists, state->prepare.clens + state->prepare.hlit, state->prepare.hdist))
                return RESULT_ERROR;
            goto STEP_INFLATE_START;
        }
    }
}

int inflate_flush(inflate_state *state, unsigned char data_in[8])
{
    int count = 0;
    int keep = state->in.available & 0x7;
    while (count < state->in.available / 8) {
        data_in[count] = (state->in.bits >> (count * 8 + keep)) & 0xFF;
        count++;
    }
    state->in.available = keep;
    return count;
}

/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

// adapted from https://github.com/libarchive/libarchive/blob/master/libarchive/archive_read_support_format_rar.c

/*-
* Copyright (c) 2003-2007 Tim Kientzle
* Copyright (c) 2011 Andres Mejia
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "rar.h"

static void *gSzAlloc_Alloc(void *self, size_t size) { (void)self; return malloc(size); }
static void gSzAlloc_Free(void *self, void *ptr) { (void)self; free(ptr); }
static ISzAlloc gSzAlloc = { gSzAlloc_Alloc, gSzAlloc_Free };

static inline size_t next_power_of_2(size_t value)
{
    size_t pow2;
    for (pow2 = 1; pow2 < value && pow2 != 0; pow2 <<= 1);
    return pow2;
}

static bool rar_br_fill(ar_archive_rar *rar, int bits)
{
    uint8_t bytes[8];
    int count, i;
    /* read as many bits as possible */
    count = (64 - rar->uncomp.br.available) / 8;
    if (rar->progr.data_left < (size_t)count)
        count = (int)rar->progr.data_left;

    if (bits > rar->uncomp.br.available + 8 * count || ar_read(rar->super.stream, bytes, count) != (size_t)count) {
        warn("Unexpected EOF during decompression (truncated file?)");
        rar->uncomp.at_eof = true;
        return false;
    }
    rar->progr.data_left -= count;
    for (i = 0; i < count; i++) {
        rar->uncomp.br.bits = (rar->uncomp.br.bits << 8) | bytes[i];
    }
    rar->uncomp.br.available += 8 * count;
    return true;
}

static inline bool rar_br_check(ar_archive_rar *rar, int bits)
{
    return bits <= rar->uncomp.br.available || rar_br_fill(rar, bits);
}

static inline uint64_t rar_br_bits(ar_archive_rar *rar, int bits)
{
    uint64_t value = (rar->uncomp.br.bits >> (rar->uncomp.br.available - bits)) & (((uint64_t)1 << bits) - 1);
    rar->uncomp.br.available -= bits;
    return value;
}

static Byte ByteIn_Read(void *p)
{
    struct ByteReader *self = p;
    return rar_br_check(self->rar, 8) ? (Byte)rar_br_bits(self->rar, 8) : 0;
}

static void ByteIn_CreateVTable(struct ByteReader *br, ar_archive_rar *rar)
{
    br->super.Read = ByteIn_Read;
    br->rar = rar;
}

/* Ppmd7 range decoder differs between 7z and RAR */
static Bool PpmdRAR_RangeDec_Init(struct CPpmdRAR_RangeDec *p)
{
    int i;
    p->Low = 0;
    p->Bottom = 0x8000;
    p->Range = 0xFFFFFFFF;
    for (i = 0; i < 4; i++) {
        p->Code = (p->Code << 8) | p->Stream->Read(p->Stream);
    }
    return (p->Code < 0xFFFFFFFF) != 0;
}

static UInt32 Range_GetThreshold(void *p, UInt32 total)
{
    struct CPpmdRAR_RangeDec *self = p;
    return (self->Code - self->Low) / (self->Range /= total);
}

static void Range_Decode_RAR(void *p, UInt32 start, UInt32 size)
{
    struct CPpmdRAR_RangeDec *self = p;
    self->Low += start * self->Range;
    self->Range *= size;
    for (;;) {
        if ((self->Low ^ (self->Low + self->Range)) >= (1 << 24)) {
            if (self->Range >= self->Bottom)
                break;
            self->Range = ((uint32_t)(-(int32_t)self->Low)) & (self->Bottom - 1);
        }
        self->Code = (self->Code << 8) | self->Stream->Read(self->Stream);
        self->Range <<= 8;
        self->Low <<= 8;
    }
}

static UInt32 Range_DecodeBit_RAR(void *p, UInt32 size0)
{
    UInt32 value = Range_GetThreshold(p, PPMD_BIN_SCALE);
    UInt32 bit = value < size0 ? 0 : 1;
    if (value < size0)
        Range_Decode_RAR(p, 0, size0);
    else
        Range_Decode_RAR(p, size0, PPMD_BIN_SCALE - size0);
    return bit;
}

static void PpmdRAR_RangeDec_CreateVTable(struct CPpmdRAR_RangeDec *p, IByteIn *stream)
{
    p->super.GetThreshold = Range_GetThreshold;
    p->super.Decode = Range_Decode_RAR;
    p->super.DecodeBit = Range_DecodeBit_RAR;
    p->Stream = stream;
}

static void rar_init_uncompress(struct ar_archive_rar_uncomp *uncomp)
{
    if (uncomp->initialized)
        return;
    memset(uncomp, 0, sizeof(*uncomp));
    uncomp->start_new_table = true;
    uncomp->filterstart = SIZE_MAX;
    uncomp->initialized = true;
}

static void rar_free_codes(struct ar_archive_rar_uncomp *uncomp);

void rar_clear_uncompress(struct ar_archive_rar_uncomp *uncomp)
{
    if (!uncomp->initialized)
        return;
    rar_free_codes(uncomp);
    lzss_cleanup(&uncomp->lzss);
    Ppmd7_Free(&uncomp->ppmd7_context, &gSzAlloc);
    uncomp->initialized = false;
}

static bool rar_new_node(struct huffman_code *code)
{
    if (!code->tree) {
        code->minlength = INT_MAX;
        code->maxlength = INT_MIN;
    }
    if (code->numentries + 1 >= code->capacity) {
        /* in my small file sample, 1024 is the value needed most often */
        int new_capacity = code->capacity ? code->capacity * 2 : 1024;
        void *new_tree = realloc(code->tree, new_capacity * sizeof(*code->tree));
        if (!new_tree) {
            warn("Unable to allocate memory for node data");
            return false;
        }
        code->tree = new_tree;
        code->capacity = new_capacity;
    }
    code->tree[code->numentries].branches[0] = -1;
    code->tree[code->numentries].branches[1] = -2;
    code->numentries++;
    return true;
}

static bool rar_add_value(ar_archive_rar *rar, struct huffman_code *code, int value, int codebits, int length)
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
        /* check for leaf node */
        if (code->tree[lastnode].branches[0] == code->tree[lastnode].branches[1]) {
            warn("Prefix found");
            return false;
        }
        /* check for open branch */
        if (code->tree[lastnode].branches[bit] < 0) {
            if (!rar_new_node(code))
                return false;
            code->tree[lastnode].branches[bit] = code->numentries - 1;
        }
        /* select branch */
        lastnode = code->tree[lastnode].branches[bit];
    }
    /* check for empty leaf */
    if (code->tree[lastnode].branches[0] != -1 || code->tree[lastnode].branches[1] != -2) {
        warn("Prefix found");
        return false;
    }
    /* set leaf value */
    code->tree[lastnode].branches[0] = value;
    code->tree[lastnode].branches[1] = value;
    return true;
}

static bool rar_create_code(ar_archive_rar *rar, struct huffman_code *code, uint8_t *lengths, int numsymbols)
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
            if (!rar_add_value(rar, code, j, codebits, i))
                return false;
            if (--symbolsleft <= 0)
                return true;
            codebits++;
        }
        codebits <<= 1;
    }
    return true;
}

static bool rar_make_table_rec(struct huffman_code *code, int node, struct huffman_table_entry *table, int depth, int maxdepth)
{
    int currtablesize = 1 << (maxdepth - depth);
    int i;

    if (node < 0 || code->numentries <= node) {
        warn("Invalid location to Huffman tree specified");
        return false;
    }

    if (code->tree[node].branches[0] == code->tree[node].branches[1]) {
        for (i = 0; i < currtablesize; i++) {
            table[i].length = depth;
            table[i].value = code->tree[node].branches[0];
        }
    }
    else if (node < 0) {
        for (i = 0; i < currtablesize; i++)
            table[i].length = -1;
    }
    else if (depth == maxdepth) {
        table[0].length = maxdepth + 1;
        table[0].value = node;
    }
    else {
        if (!rar_make_table_rec(code, code->tree[node].branches[0], table, depth + 1, maxdepth))
            return false;
        if (!rar_make_table_rec(code, code->tree[node].branches[1], table + currtablesize / 2, depth + 1, maxdepth))
            return false;
    }
    return true;
}

static bool rar_make_table(struct huffman_code *code)
{
    if (code->minlength <= code->maxlength && code->maxlength <= 10)
        code->tablesize = code->maxlength;
    else
        code->tablesize = 10;

    code->table = calloc(1 << code->tablesize, sizeof(*code->table));
    if (!code->table) {
        warn("Unable to allocate memory for table data");
        return false;
    }

    return rar_make_table_rec(code, 0, code->table, 0, code->tablesize);
}

static int rar_read_next_symbol(ar_archive_rar *rar, struct huffman_code *code)
{
    uint8_t bit;
    uint32_t bits;
    int length, value, node;

    if (!code->table && !rar_make_table(code))
        return -1;

    if (!rar_br_check(rar, code->tablesize))
        return -1;
    bits = (uint32_t)rar_br_bits(rar, code->tablesize);

    length = code->table[bits].length;
    value = code->table[bits].value;

    if (length < 0) {
        warn("Invalid prefix code in bitstream");
        return -1;
    }

    if (length <= code->tablesize) {
        /* Skip only length bits */
        rar->uncomp.br.available += code->tablesize - length;
        return value;
    }


    node = value;
    while (code->tree[node].branches[0] != code->tree[node].branches[1]) {
        if (!rar_br_check(rar, 1))
            return -1;
        bit = (uint8_t)rar_br_bits(rar, 1);

        if (code->tree[node].branches[bit] < 0) {
            warn("Invalid prefix code in bitstream");
            return -1;
        }
        node = code->tree[node].branches[bit];
    }

    return code->tree[node].branches[0];
}

static bool rar_parse_codes(ar_archive_rar *rar)
{
    struct ar_archive_rar_uncomp *uncomp = &rar->uncomp;

    rar_free_codes(uncomp);

    /* skip to next byte */
    uncomp->br.available &= ~0x07;

    if (!rar_br_check(rar, 1))
        return false;
    uncomp->is_ppmd_block = rar_br_bits(rar, 1) != 0;
    if (uncomp->is_ppmd_block) {
        uint8_t ppmd_flags;
        if (!rar_br_check(rar, 7))
            return false;
        ppmd_flags = (uint8_t)rar_br_bits(rar, 7);
        /* memory is allocated in MB */
        if ((ppmd_flags & 0x20)) {
            if (!rar_br_check(rar, 8))
                return false;
            uncomp->dict_size = ((uint8_t)rar_br_bits(rar, 8) + 1) << 20;
        }
        if ((ppmd_flags & 0x40)) {
            if (!rar_br_check(rar, 8))
                return false;
            uncomp->ppmd_escape = uncomp->ppmd7_context.InitEsc = (uint8_t)rar_br_bits(rar, 8);
        }
        else
            uncomp->ppmd_escape = 2;
        if ((ppmd_flags & 0x20)) {
            uint32_t maxorder = (ppmd_flags & 0x1F) + 1;
            if (maxorder == 1)
                return false;
            if (maxorder > 16)
                maxorder = 16 + (maxorder - 16) * 3;

            Ppmd7_Free(&uncomp->ppmd7_context, &gSzAlloc);
            Ppmd7_Construct(&uncomp->ppmd7_context);
            if (!Ppmd7_Alloc(&uncomp->ppmd7_context, uncomp->dict_size, &gSzAlloc)) {
                warn("OOM in Ppmd7_Alloc");
                return false;
            }
            ByteIn_CreateVTable(&uncomp->bytein, rar);
            PpmdRAR_RangeDec_CreateVTable(&uncomp->range_dec, &uncomp->bytein.super);
            if (!PpmdRAR_RangeDec_Init(&uncomp->range_dec)) {
                warn("Unable to initialize PPMd range decoder");
                return false;
            }
            Ppmd7_Init(&uncomp->ppmd7_context, maxorder);
            uncomp->ppmd_valid = true;
        }
        else {
            if (!uncomp->ppmd_valid) {
                warn("Invalid PPMd sequence");
                return false;
            }
            if (!PpmdRAR_RangeDec_Init(&uncomp->range_dec)) {
                warn("Unable to initialize PPMd range decoder");
                return false;
            }
        }
    }
    else {
        struct huffman_code precode;
        uint8_t bitlengths[20];
        uint8_t zerocount;
        int i, j, val, n;
        bool ok = false;

        /* keep existing table flag */
        if (!rar_br_check(rar, 1))
            return false;
        if (!rar_br_bits(rar, 1))
            memset(uncomp->lengthtable, 0, sizeof(uncomp->lengthtable));
        memset(&bitlengths, 0, sizeof(bitlengths));
        for (i = 0; i < sizeof(bitlengths); ) {
            if (!rar_br_check(rar, 4))
                return false;
            bitlengths[i++] = (uint8_t)rar_br_bits(rar, 4);
            if (bitlengths[i - 1] == 0x0F) {
                if (!rar_br_check(rar, 4))
                    return false;
                zerocount = (uint8_t)rar_br_bits(rar, 4);
                if (zerocount) {
                    i--;
                    for (j = 0; j < zerocount + 2 && i < sizeof(bitlengths); j++) {
                        bitlengths[i++] = 0;
                    }
                }
            }
        }

        memset(&precode, 0, sizeof(precode));
        if (!rar_create_code(rar, &precode, bitlengths, sizeof(bitlengths)))
            goto PrecodeError;
        for (i = 0; i < HUFFMAN_TABLE_SIZE; ) {
            val = rar_read_next_symbol(rar, &precode);
            if (val < 0)
                goto PrecodeError;
            if (val < 16) {
                uncomp->lengthtable[i] = (uncomp->lengthtable[i] + val) & 0x0F;
                i++;
            }
            else if (val < 18) {
                if (i == 0) {
                    warn("Internal error extracting RAR file");
                    goto PrecodeError;
                }
                if (val == 16) {
                    if (!rar_br_check(rar, 3))
                        goto PrecodeError;
                    n = (uint8_t)rar_br_bits(rar, 3) + 3;
                }
                else {
                    if (!rar_br_check(rar, 7))
                        goto PrecodeError;
                    n = (uint8_t)rar_br_bits(rar, 7) + 11;
                }
                for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; i++, j++) {
                    uncomp->lengthtable[i] = uncomp->lengthtable[i - 1];
                }
            }
            else {
                if (val == 18) {
                    if (!rar_br_check(rar, 3))
                        goto PrecodeError;
                    n = (uint8_t)rar_br_bits(rar, 3) + 3;
                }
                else {
                    if (!rar_br_check(rar, 7))
                        goto PrecodeError;
                    n = (uint8_t)rar_br_bits(rar, 7) + 11;
                }
                for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; i++, j++) {
                    uncomp->lengthtable[i] = 0;
                }
            }
        }
        ok = true;
PrecodeError:
        free(precode.tree);
        free(precode.table);
        if (!ok)
            return false;

        if (!rar_create_code(rar, &uncomp->maincode, uncomp->lengthtable, MAINCODE_SIZE))
            return false;
        if (!rar_create_code(rar, &uncomp->offsetcode, uncomp->lengthtable + MAINCODE_SIZE, OFFSETCODE_SIZE))
            return false;
        if (!rar_create_code(rar, &uncomp->lowoffsetcode, uncomp->lengthtable + MAINCODE_SIZE + OFFSETCODE_SIZE, LOWOFFSETCODE_SIZE))
            return false;
        if (!rar_create_code(rar, &uncomp->lengthcode, uncomp->lengthtable + MAINCODE_SIZE + OFFSETCODE_SIZE + LOWOFFSETCODE_SIZE, LENGTHCODE_SIZE))
            return false;
    }

    if (!uncomp->dict_size || !uncomp->lzss.window) {
        /* libarchive tries to minimize memory usage in this way */
        if (rar->super.entry_size_uncompressed > 0x200000)
            uncomp->dict_size = 0x400000;
        else
            uncomp->dict_size = (uint32_t)next_power_of_2(rar->super.entry_size_uncompressed);
        if (!lzss_initialize(&rar->uncomp.lzss, rar->uncomp.dict_size)) {
            warn("OOM in lzss_initialize");
            return false;
        }
    }

    uncomp->start_new_table = false;
    return true;
}

static void rar_free_codes(struct ar_archive_rar_uncomp *uncomp)
{
    free(uncomp->maincode.tree);
    free(uncomp->maincode.table);
    memset(&uncomp->maincode, 0, sizeof(uncomp->maincode));
    free(uncomp->offsetcode.tree);
    free(uncomp->offsetcode.table);
    memset(&uncomp->offsetcode, 0, sizeof(uncomp->offsetcode));
    free(uncomp->lowoffsetcode.tree);
    free(uncomp->lowoffsetcode.table);
    memset(&uncomp->lowoffsetcode, 0, sizeof(uncomp->lowoffsetcode));
    free(uncomp->lengthcode.tree);
    free(uncomp->lengthcode.table);
    memset(&uncomp->lengthcode, 0, sizeof(uncomp->lengthcode));
}

static bool rar_read_filter(ar_archive_rar *rar, bool (* decode_byte)(ar_archive_rar *rar, uint8_t *byte), int64_t *end)
{
    uint8_t flags, val, *code;
    uint16_t length, i;

    if (!decode_byte(rar, &flags))
        return false;
    length = (flags & 0x07) + 1;
    if (length == 7) {
        if (!decode_byte(rar, &val))
            return false;
        length = val + 7;
    }
    else if (length == 8) {
        if (!decode_byte(rar, &val))
            return false;
        length = val << 8;
        if (!decode_byte(rar, &val))
            return false;
        length |= val;
    }

    code = malloc(length);
    if (!code) {
        warn("Unable to allocate memory for parsing filter");
        return false;
    }
    for (i = 0; i < length; i++) {
        if (!decode_byte(rar, &code[i])) {
            free(code);
            return false;
        }
    }

    free(code);
    todo("ParseFilter(code, %d, %#02x)", length, flags);

    if (rar->uncomp.filterstart < *end)
        *end = rar->uncomp.filterstart;

    return false;
}

static inline bool rar_decode_ppmd7_symbol(struct ar_archive_rar_uncomp *uncomp, Byte *symbol)
{
    int value = Ppmd7_DecodeSymbol(&uncomp->ppmd7_context, &uncomp->range_dec.super);
    if (value < 0) {
        warn("Invalid PPMd symbol");
        return false;
    }
    *symbol = (Byte)value;
    return true;
}

static bool rar_decode_byte(ar_archive_rar *rar, uint8_t *byte)
{
    if (!rar_br_check(rar, 8))
        return false;
    *byte = (uint8_t)rar_br_bits(rar, 8);
    return true;
}

static bool rar_decode_ppmd7_byte(ar_archive_rar *rar, uint8_t *byte)
{
    return rar_decode_ppmd7_symbol(&rar->uncomp, byte);
}

static bool rar_handle_ppmd_sequence(ar_archive_rar *rar, int64_t *end)
{
    struct ar_archive_rar_uncomp *uncomp = &rar->uncomp;
    Byte sym, code, length;
    int lzss_offset;

    if (!rar_decode_ppmd7_symbol(uncomp, &sym))
        return false;
    if (sym != uncomp->ppmd_escape) {
        lzss_emit_literal(&uncomp->lzss, sym);
        return true;
    }

    if (!rar_decode_ppmd7_symbol(uncomp, &code))
        return false;
    switch (code) {
    case 0:
        return rar_parse_codes(rar);

    case 2:
        uncomp->start_new_table = true;
        return true;

    case 3:
        return rar_read_filter(rar, rar_decode_ppmd7_byte, end);

    case 4:
        if (!rar_decode_ppmd7_symbol(uncomp, &code))
            return false;
        lzss_offset = code << 16;
        if (!rar_decode_ppmd7_symbol(uncomp, &code))
            return false;
        lzss_offset |= code << 8;
        if (!rar_decode_ppmd7_symbol(uncomp, &code))
            return false;
        lzss_offset |= code;
        if (!rar_decode_ppmd7_symbol(uncomp, &length))
            return false;
        lzss_emit_match(&uncomp->lzss, lzss_offset + 2, length + 32);
        return true;

    case 5:
        if (!rar_decode_ppmd7_symbol(uncomp, &length))
            return false;
        lzss_emit_match(&uncomp->lzss, 1, length + 4);
        return true;

    default:
        lzss_emit_literal(&uncomp->lzss, sym);
        return true;
    }
}

static int64_t rar_expand(ar_archive_rar *rar, int64_t end)
{
    static const uint8_t lengthbases[] =
        {   0,   1,   2,   3,   4,   5,   6,
            7,   8,  10,  12,  14,  16,  20,
           24,  28,  32,  40,  48,  56,  64,
           80,  96, 112, 128, 160, 192, 224 };
    static const uint8_t lengthbits[] =
        { 0, 0, 0, 0, 0, 0, 0,
          0, 1, 1, 1, 1, 2, 2,
          2, 2, 3, 3, 3, 3, 4,
          4, 4, 4, 5, 5, 5, 5 };
    static const int32_t offsetbases[] =
        {       0,       1,       2,       3,       4,       6,
                8,      12,      16,      24,      32,      48,
               64,      96,     128,     192,     256,     384,
              512,     768,    1024,    1536,    2048,    3072,
             4096,    6144,    8192,   12288,   16384,   24576,
            32768,   49152,   65536,   98304,  131072,  196608,
           262144,  327680,  393216,  458752,  524288,  589824,
           655360,  720896,  786432,  851968,  917504,  983040,
          1048576, 1310720, 1572864, 1835008, 2097152, 2359296,
          2621440, 2883584, 3145728, 3407872, 3670016, 3932160 };
    static const uint8_t offsetbits[] =
        {  0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,
           5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10,
          11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
          16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
          18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18 };
    static const uint8_t shortbases[] =
        { 0, 4, 8, 16, 32, 64, 128, 192 };
    static const uint8_t shortbits[] =
        { 2, 2, 3, 4, 5, 6, 6, 6 };

    struct ar_archive_rar_uncomp *uncomp = &rar->uncomp;
    int symbol, offs, len, i;

    for (;;) {
        if (lzss_position(&uncomp->lzss) >= end)
            return end;

        if (uncomp->is_ppmd_block) {
            if (!rar_handle_ppmd_sequence(rar, &end))
                return -1;
            if (uncomp->start_new_table)
                return lzss_position(&uncomp->lzss);
            continue;
        }

        symbol = rar_read_next_symbol(rar, &uncomp->maincode);
        if (symbol < 0)
            return -1;
        if (symbol < 256) {
            lzss_emit_literal(&uncomp->lzss, (uint8_t)symbol);
            continue;
        }
        if (symbol == 256) {
            if (!rar_br_check(rar, 1))
                return -1;
            if (!rar_br_bits(rar, 1)) {
                if (!rar_br_check(rar, 1))
                    return -1;
                uncomp->start_new_table = rar_br_bits(rar, 1) != 0;
                return lzss_position(&uncomp->lzss);
            }
            if (!rar_parse_codes(rar))
                return -1;
            continue;
        }
        if (symbol == 257) {
            if (!rar_read_filter(rar, rar_decode_byte, &end))
                return -1;
            continue;
        }
        if (symbol == 258) {
            if (uncomp->lastlength == 0)
                continue;
            offs = uncomp->lastoffset;
            len = uncomp->lastlength;
        }
        else if (symbol <= 262) {
            int idx = symbol - 259;
            int lensymbol = rar_read_next_symbol(rar, &uncomp->lengthcode);
            offs = uncomp->oldoffset[idx];
            if (lensymbol < 0 || lensymbol > (int)(sizeof(lengthbases) / sizeof(lengthbases[0])) || lensymbol > (int)(sizeof(lengthbits) / sizeof(lengthbits[0]))) {
                warn("Bad RAR file data");
                return -1;
            }
            len = lengthbases[lensymbol] + 2;
            if (lengthbits[lensymbol] > 0) {
                if (!rar_br_check(rar, lengthbits[lensymbol]))
                    return -1;
                len += (int)rar_br_bits(rar, lengthbits[lensymbol]);
            }
            for (i = idx; i > 0; i--)
                uncomp->oldoffset[i] = uncomp->oldoffset[i - 1];
            uncomp->oldoffset[0] = offs;
        }
        else if (symbol <= 270) {
            int idx = symbol - 263;
            offs = shortbases[idx] + 1;
            if (shortbits[idx] > 0) {
                if (!rar_br_check(rar, shortbits[idx]))
                    return -1;
                offs += (int)rar_br_bits(rar, shortbits[idx]);
            }
            len = 2;
            for (i = 3; i > 0; i--)
                uncomp->oldoffset[i] = uncomp->oldoffset[i - 1];
            uncomp->oldoffset[0] = offs;
        }
        else {
            int idx = symbol - 271;
            int offssymbol;
            if (idx > (int)(sizeof(lengthbases) / sizeof(lengthbases[0])) || idx > (int)(sizeof(lengthbits) / sizeof(lengthbits[0]))) {
                warn("Bad RAR file data");
                return -1;
            }
            len = lengthbases[idx] + 3;
            if (lengthbits[idx] > 0) {
                if (!rar_br_check(rar, lengthbits[idx]))
                    return -1;
                len += (int)rar_br_bits(rar, lengthbits[idx]);
            }
            offssymbol = rar_read_next_symbol(rar, &uncomp->offsetcode);
            if (offssymbol < 0 || offssymbol > (int)(sizeof(offsetbases) / sizeof(offsetbases[0])) || offssymbol > (int)(sizeof(offsetbits) / sizeof(offsetbits[0]))) {
                warn("Bad RAR file data");
                return -1;
            }
            offs = offsetbases[offssymbol] + 1;
            if (offsetbits[offssymbol] > 0) {
                if (offssymbol > 9) {
                    if (offsetbits[offssymbol] > 4) {
                        if (!rar_br_check(rar, offsetbits[offssymbol] - 4))
                            return -1;
                        offs += (int)rar_br_bits(rar, offsetbits[offssymbol] - 4) << 4;
                    }
                    if (uncomp->numlowoffsetrepeats > 0) {
                        uncomp->numlowoffsetrepeats--;
                        offs += uncomp->lastlowoffset;
                    }
                    else {
                        int lowoffsetsymbol = rar_read_next_symbol(rar, &uncomp->lowoffsetcode);
                        if (lowoffsetsymbol < 0)
                            return -1;
                        if (lowoffsetsymbol == 16) {
                            uncomp->numlowoffsetrepeats = 15;
                            offs += uncomp->lastlowoffset;
                        }
                        else {
                            offs += lowoffsetsymbol;
                            uncomp->lastlowoffset = lowoffsetsymbol;
                        }
                    }
                }
                else {
                    if (!rar_br_check(rar, offsetbits[offssymbol]))
                        return -1;
                    offs += (int)rar_br_bits(rar, offsetbits[offssymbol]);
                }
            }

            if (offs >= 0x40000)
                len++;
            if (offs >= 0x2000)
                len++;

            for (i = 3; i > 0; i--)
                uncomp->oldoffset[i] = uncomp->oldoffset[i - 1];
            uncomp->oldoffset[0] = offs;
        }

        uncomp->lastoffset = offs;
        uncomp->lastlength = len;

        lzss_emit_match(&uncomp->lzss, offs, len);
    }
}

bool rar_uncompress_part(ar_archive_rar *rar, void *buffer, size_t buffer_size)
{
    struct ar_archive_rar_uncomp *uncomp = &rar->uncomp;
    int64_t end;

    rar_init_uncompress(uncomp);

    for (;;) {
        if (uncomp->bytes_ready > 0) {
            size_t count = min(uncomp->bytes_ready, buffer_size);
            lzss_copy_bytes_from_window(&uncomp->lzss, buffer, rar->progr.bytes_done, count);
            rar->progr.bytes_done += count;
            uncomp->bytes_ready -= count;
            buffer_size -= count;
            buffer = (uint8_t *)buffer + count;
        }

        if (buffer_size == 0)
            return true;

        if (uncomp->at_eof)
            return false;

        if (uncomp->start_new_table && !rar_parse_codes(rar))
            return false;

        end = rar->progr.bytes_done + uncomp->dict_size;
        if (uncomp->filterstart < end)
            end = uncomp->filterstart;
        end = rar_expand(rar, end);
        if (end < rar->progr.bytes_done)
            return false;
        uncomp->bytes_ready = (size_t)end - rar->progr.bytes_done;

        if (uncomp->is_ppmd_block && uncomp->start_new_table && !rar_parse_codes(rar))
            return false;
    }
}

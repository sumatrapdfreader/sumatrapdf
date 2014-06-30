/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

// adapted from https://code.google.com/p/libarchive/source/browse/libarchive/archive_read_support_format_rar.c

#include "rar.h"

#define MAX_SYMBOL_LENGTH 0xF
#define MAX_SYMBOLS 20

static void *gSzAlloc_Alloc(void *self, size_t size) { return malloc(size); }
static void gSzAlloc_Free(void *self, void *ptr) { free(ptr); }
static ISzAlloc gSzAlloc = { gSzAlloc_Alloc, gSzAlloc_Free };

static inline size_t next_power_of_2(size_t value)
{
    size_t pow2;
    for (pow2 = 1; pow2 < value && pow2 != 0; pow2 <<= 1);
    return pow2;
}

static bool rar_br_fill(ar_archive_rar *rar, int bits)
{
    uint8_t bytes[8], count, i;
    /* read as many bits as possible */
    count = (64 - rar->uncomp.br.available) / 8;
    if (rar->super.entry_size_block - rar->progr.offset_in < count)
        count = rar->super.entry_size_block - rar->progr.offset_in;

    if (ar_read(rar->super.stream, bytes, count) != count)
        return false;
    rar->progr.offset_in += count;
    for (i = 0; i < count; i++) {
        rar->uncomp.br.bits = (rar->uncomp.br.bits << 8) | bytes[i];
    }
    rar->uncomp.br.available += 8 * count;
    rar->uncomp.at_eof = bits > rar->uncomp.br.available;

    return !rar->uncomp.at_eof;
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

static Byte ByteIn_Read(struct ByteReader *self)
{
    if (!rar_br_check(self->rar, 8)) {
        warn("Unexpected EOF in ByteIn_Read");
        return 0;
    }
    return (Byte)rar_br_bits(self->rar, 8);
}

static void rar_init_uncompress(ar_archive_rar *rar)
{
    if (rar->uncomp.initialized)
        return;
    memset(&rar->uncomp, 0, sizeof(rar->uncomp));
    rar->uncomp.start_new_table = true;
    rar->uncomp.bytein.rar = rar;
    rar->uncomp.bytein.super.Read = ByteIn_Read;
    rar->uncomp.initialized = true;
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
    if (code->numentries + 1 >= code->capacity) {
        int new_capacity = code->capacity ? code->capacity * 2 : 16;
        void *new_tree = realloc(code->tree, new_capacity * sizeof(*code->tree));
        if (!new_tree)
            return false;
        code->tree = new_tree;
        code->capacity = new_capacity;
    }
    code->tree[code->numentries].branches[0] = -1;
    code->tree[code->numentries].branches[1] = -2;
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

        /* Leaf node check */
        if (code->tree[lastnode].branches[0] == code->tree[lastnode].branches[1]) {
            warn("Prefix found");
            return false;
        }

        /* Open branch check */
        if (code->tree[lastnode].branches[bit] < 0) {
            if (!rar_new_node(code)) {
                warn("Unable to allocate memory for node data.");
                return false;
            }
            code->tree[lastnode].branches[bit] = code->numentries++;
        }

        /* set to branch */
        lastnode = code->tree[lastnode].branches[bit];
    }

    if (code->tree[lastnode].branches[0] != -1 || code->tree[lastnode].branches[1] != -2) {
        warn("Prefix found");
        return false;
    }

    /* Set leaf value */
    code->tree[lastnode].branches[0] = value;
    code->tree[lastnode].branches[1] = value;
    return true;
}

static bool rar_create_code(ar_archive_rar *rar, struct huffman_code *code, unsigned char *lengths, int numsymbols, char maxlength)
{
    int codebits = 0;
    int symbolsleft = numsymbols;
    int i, j;

    if (!rar_new_node(code)) {
        warn("Unable to allocate memory for node data.");
        return false;
    }
    code->numentries = 1;
    code->minlength = INT_MAX;
    code->maxlength = INT_MIN;

    for (i = 1; i <= maxlength; i++) {
        for (j = 0; j < numsymbols; j++) {
            if (lengths[j] != i)
                continue;
            if (!rar_add_value(rar, code, j, codebits, i))
                return false;
            codebits++;
            if (--symbolsleft <= 0) {
                break;
                break;
            }
        }
        codebits <<= 1;
    }
    return true;
}

static bool rar_make_table_recurse(struct huffman_code *code, int node, struct huffman_table_entry *table, int depth, int maxdepth)
{
    int currtablesize, i;
    bool ok = true;

    if (!code->tree) {
        warn("Huffman tree was not created.");
        return false;
    }
    if (node < 0 || node >= code->numentries) {
        warn("Invalid location to Huffman tree specified.");
        return false;
    }

    currtablesize = 1 << (maxdepth - depth);

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
        ok |= rar_make_table_recurse(code, code->tree[node].branches[0], table, depth + 1, maxdepth);
        ok |= rar_make_table_recurse(code, code->tree[node].branches[1], table + currtablesize / 2, depth + 1, maxdepth);
    }
    return ok;
}

static bool rar_make_table(struct huffman_code *code)
{
    if (code->maxlength < code->minlength || code->maxlength > 10)
        code->tablesize = 10;
    else
        code->tablesize = code->maxlength;

    code->table = calloc((size_t)1 << code->tablesize, sizeof(*code->table));
    return rar_make_table_recurse(code, 0, code->table, 0, code->tablesize);
}

static int rar_read_next_symbol(ar_archive_rar *rar, struct huffman_code *code)
{
    uint8_t bit;
    uint32_t bits;
    int length, value, node;

    if (!code->table && !rar_make_table(code))
        return -1;

    if (!rar_br_check(rar, code->tablesize)) {
        warn("Truncated RAR file data");
        return -1;
    }
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
        if (!rar_br_check(rar, 1)) {
            warn("Truncated RAR file data");
            return -1;
        }
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
#define ensure_bits(n) if (!rar_br_check(rar, n)) { warn("Unexpected EOF in rar_parse_codes"); return false; } else ((void)0)

    struct ar_archive_rar_uncomp *uncomp = &rar->uncomp;

    rar_free_codes(uncomp);

    /* skip to next byte */
    uncomp->br.available &= ~0x07;

    ensure_bits(1);
    uncomp->is_ppmd_block = rar_br_bits(rar, 1) != 0;
    if (uncomp->is_ppmd_block) {
        uint8_t ppmd_flags;
        ensure_bits(7);
        ppmd_flags = (uint8_t)rar_br_bits(rar, 7);
        /* memory is allocated in MB */
        if ((ppmd_flags & 0x20)) {
            ensure_bits(8);
            uncomp->dict_size = (rar_br_bits(rar, 8) + 1) << 20;
        }
        if ((ppmd_flags & 0x40)) {
            ensure_bits(8);
            uncomp->ppmd_escape = uncomp->ppmd7_context.InitEsc = rar_br_bits(rar, 8);
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
            Ppmd7z_RangeDec_CreateVTable(&uncomp->range_dec);
            uncomp->range_dec.Stream = &uncomp->bytein.super;
            Ppmd7_Construct(&uncomp->ppmd7_context);
            if (!Ppmd7_Alloc(&uncomp->ppmd7_context, uncomp->dict_size, &gSzAlloc)) {
                warn("OOM in Ppmd7_Alloc");
                return false;
            }
            // TODO: RAR and 7z are slightly incompatible
            if (!Ppmd7z_RangeDec_Init(&uncomp->range_dec)) {
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
            if (!Ppmd7z_RangeDec_Init(&uncomp->range_dec)) {
                warn("Unable to initialize PPMd range decoder");
                return false;
            }
        }
    }
    else {
        struct huffman_code precode;
        uint8_t bitlengths[MAX_SYMBOLS];
        uint8_t zerocount;
        int i, j, val, n;

        /* keep existing table flag */
        ensure_bits(1);
        if (!rar_br_bits(rar, 1))
            memset(uncomp->lengthtable, 0, sizeof(uncomp->lengthtable));
        memset(&bitlengths, 0, sizeof(bitlengths));
        for (i = 0; i < sizeof(bitlengths); ) {
            ensure_bits(4);
            bitlengths[i++] = (uint8_t)rar_br_bits(rar, 4);
            if (bitlengths[i - 1] == 0x0F) {
                ensure_bits(4);
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
        if (!rar_create_code(rar, &precode, bitlengths, sizeof(bitlengths), MAX_SYMBOL_LENGTH)) {
            free(precode.tree);
            free(precode.table);
            return false;
        }
        for (i = 0; i < HUFFMAN_TABLE_SIZE; ) {
            val = rar_read_next_symbol(rar, &precode);
            if (val < 0) {
                free(precode.tree);
                free(precode.table);
                return false;
            }
            if (val < 16) {
                uncomp->lengthtable[i] = (uncomp->lengthtable[i] + val) & 0x0F;
                i++;
            }
            else if (val < 18) {
                if (i == 0) {
                    warn("Internal error extracting RAR file.");
                    free(precode.tree);
                    free(precode.table);
                    return false;
                }
                if (val == 16) {
                    if (!rar_br_check(rar, 3)) {
                        warn("Unexpected EOF in rar_parse_codes");
                        free(precode.tree);
                        free(precode.table);
                        return false;
                    }
                    n = rar_br_bits(rar, 3) + 3;
                }
                else {
                    if (!rar_br_check(rar, 7)) {
                        warn("Unexpected EOF in rar_parse_codes");
                        free(precode.tree);
                        free(precode.table);
                        return false;
                    }
                    n = rar_br_bits(rar, 7) + 11;
                }

                for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; j++) {
                    uncomp->lengthtable[i] = uncomp->lengthtable[i - 1];
                    i++;
                }
            }
            else {
                if (val == 18) {
                    if (!rar_br_check(rar, 3)) {
                        warn("Unexpected EOF in rar_parse_codes");
                        free(precode.tree);
                        free(precode.table);
                        return false;
                    }
                    n = rar_br_bits(rar, 3) + 3;
                }
                else {
                    if (!rar_br_check(rar, 7)) {
                        warn("Unexpected EOF in rar_parse_codes");
                        free(precode.tree);
                        free(precode.table);
                        return false;
                    }
                    n = rar_br_bits(rar, 7) + 11;
                }

                for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; j++) {
                    uncomp->lengthtable[i] = 0;
                    i++;
                }
            }
        }
        free(precode.tree);
        free(precode.table);

        if (!rar_create_code(rar, &uncomp->maincode, uncomp->lengthtable, MAINCODE_SIZE, MAX_SYMBOL_LENGTH))
            return false;
        if (!rar_create_code(rar, &uncomp->offsetcode, uncomp->lengthtable + MAINCODE_SIZE, OFFSETCODE_SIZE, MAX_SYMBOL_LENGTH))
            return false;
        if (!rar_create_code(rar, &uncomp->lowoffsetcode, uncomp->lengthtable + MAINCODE_SIZE + OFFSETCODE_SIZE, LOWOFFSETCODE_SIZE, MAX_SYMBOL_LENGTH))
            return false;
        if (!rar_create_code(rar, &uncomp->lengthcode, uncomp->lengthtable + MAINCODE_SIZE + OFFSETCODE_SIZE + LOWOFFSETCODE_SIZE, LENGTHCODE_SIZE, MAX_SYMBOL_LENGTH))
            return false;
    }

    if (!uncomp->dict_size || !uncomp->lzss.window) {
        /* libarchive tries to minimize memory usage in this way */
        if (rar->super.entry_size_uncompressed > 0x200000)
            uncomp->dict_size = 0x400000;
        else
            uncomp->dict_size = (uint32_t)next_power_of_2(rar->super.entry_size_uncompressed);
        lzss_initialize(&rar->uncomp.lzss, rar->uncomp.dict_size);
    }

    uncomp->start_new_table = 0;
    return true;

#undef ensure_bits
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

static int64_t rar_expand(ar_archive_rar *rar, int64_t end)
{
    static const unsigned char lengthbases[] =
        {   0,   1,   2,   3,   4,   5,   6,
            7,   8,  10,  12,  14,  16,  20,
           24,  28,  32,  40,  48,  56,  64,
           80,  96, 112, 128, 160, 192, 224 };
    static const unsigned char lengthbits[] =
        { 0, 0, 0, 0, 0, 0, 0,
          0, 1, 1, 1, 1, 2, 2,
          2, 2, 3, 3, 3, 3, 4,
          4, 4, 4, 5, 5, 5, 5 };
    static const unsigned int offsetbases[] =
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
    static const unsigned char offsetbits[] =
        {  0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,
           5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10,
          11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
          16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
          18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18 };
    static const unsigned char shortbases[] =
        { 0, 4, 8, 16, 32, 64, 128, 192 };
    static const unsigned char shortbits[] =
        { 2, 2, 3, 4, 5, 6, 6, 6 };

    int symbol, offs, len, offsindex, lensymbol, i, offssymbol, lowoffsetsymbol;
    struct ar_archive_rar_uncomp *uncomp = &rar->uncomp;
    bool output_last_match = false;

    if (uncomp->filterstart < end)
        end = uncomp->filterstart;

    for (;;) {
        if (output_last_match && lzss_position(&uncomp->lzss) + uncomp->lastlength <= end) {
            lzss_emit_match(&uncomp->lzss, uncomp->lastoffset, uncomp->lastlength);
            output_last_match = false;
        }
        if (uncomp->is_ppmd_block || output_last_match || lzss_position(&uncomp->lzss) >= end)
            return lzss_position(&uncomp->lzss);

        symbol = rar_read_next_symbol(rar, &uncomp->maincode);
        if (symbol < 0)
            return -1;
        if (symbol < 256) {
            lzss_emit_literal(&uncomp->lzss, symbol);
            continue;
        }
        if (symbol == 256) {
            if (!rar_br_check(rar, 1)) {
                warn("Unexpected EOF in rar_expand");
                return -1;
            }
            if (!rar_br_bits(rar, 1)) {
                if (!rar_br_check(rar, 1)) {
                    warn("Unexpected EOF in rar_expand");
                    return -1;
                }
                uncomp->start_new_table = rar_br_bits(rar, 1) != 0;
                return lzss_position(&uncomp->lzss);
            }
            if (!rar_parse_codes(rar))
                return -1;
            continue;
        }
        if (symbol == 257) {
            todo("Parsing filters are unsupported");
            return -1;
        }
        if (symbol == 258) {
            if (uncomp->lastlength == 0)
                continue;
            offs = uncomp->lastoffset;
            len = uncomp->lastlength;
        }
        else if (symbol <= 262) {
            offsindex = symbol - 259;
            offs = uncomp->oldoffset[offsindex];
            lensymbol = rar_read_next_symbol(rar, &uncomp->lengthcode);
            if (lensymbol < 0 || lensymbol > (int)(sizeof(lengthbases) / sizeof(lengthbases[0])) || lensymbol > (int)(sizeof(lengthbits) / sizeof(lengthbits[0]))) {
                warn("Bad RAR file data");
                return -1;
            }
            len = lengthbases[lensymbol] + 2;
            if (lengthbits[lensymbol] > 0) {
                if (!rar_br_check(rar, lengthbits[lensymbol])) {
                    warn("Unexpected EOF in rar_expand");
                    return -1;
                }
                len += rar_br_bits(rar, lengthbits[lensymbol]);
            }
            for (i = offsindex; i > 0; i--)
                uncomp->oldoffset[i] = uncomp->oldoffset[i - 1];
            uncomp->oldoffset[0] = offs;
        }
        else if (symbol <= 270) {
            offs = shortbases[symbol - 263] + 1;
            if (shortbits[symbol - 263] > 0) {
                if (!rar_br_check(rar, shortbits[symbol - 263])) {
                    warn("Unexpected EOF in rar_expand");
                    return -1;
                }
                offs += rar_br_bits(rar, shortbits[symbol - 263]);
            }
            len = 2;
            for (i = 3; i > 0; i--)
                uncomp->oldoffset[i] = uncomp->oldoffset[i - 1];
            uncomp->oldoffset[0] = offs;
        }
        else {
            if (symbol - 271 > (int)(sizeof(lengthbases) / sizeof(lengthbases[0])) || symbol - 271 > (int)(sizeof(lengthbits) / sizeof(lengthbits[0]))) {
                warn("Bad RAR file data");
                return -1;
            }
            len = lengthbases[symbol - 271] + 3;
            if (lengthbits[symbol - 271] > 0) {
                if (!rar_br_check(rar, lengthbits[symbol - 271])) {
                    warn("Unexpected EOF in rar_expand");
                    return -1;
                }
                len += rar_br_bits(rar, lengthbits[symbol - 271]);
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
                        if (!rar_br_check(rar, offsetbits[offssymbol] - 4)) {
                            warn("Unexpected EOF in rar_expand");
                            return -1;
                        }
                        offs += rar_br_bits(rar, offsetbits[offssymbol] - 4) << 4;
                    }
                    if (uncomp->numlowoffsetrepeats > 0) {
                        uncomp->numlowoffsetrepeats--;
                        offs += uncomp->lastlowoffset;
                    }
                    else {
                        lowoffsetsymbol = rar_read_next_symbol(rar, &uncomp->lowoffsetcode);
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
                    if (!rar_br_check(rar, offsetbits[offssymbol])) {
                        warn("Unexpected EOF in rar_expand");
                        return -1;
                    }
                    offs += rar_br_bits(rar, offsetbits[offssymbol]);
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
        output_last_match = true;
    }
}

bool rar_uncompress_part(ar_archive_rar *rar, void *buffer, size_t buffer_size)
{
    struct ar_archive_rar_uncomp *data = &rar->uncomp;
    size_t buffer_offset = 0;

    rar_init_uncompress(rar);

    while (buffer_offset < buffer_size && !data->at_eof) {
        int count;
        if (data->ppmd_eod || (data->dict_size && rar->progr.offset_out >= rar->super.entry_size_uncompressed)) {
            warn("Requesting too much data (%Iu < %Iu)", buffer_offset, buffer_size);
            return false;
        }

        if (!data->is_ppmd_block && data->dict_size && data->bytes_uncopied > 0) {
            count = (int)min(data->bytes_uncopied, buffer_size - buffer_offset);
            lzss_copy_bytes_from_window(&data->lzss, (uint8_t *)buffer + buffer_offset, rar->progr.offset_out, count);
            buffer_offset += count;
            rar->progr.offset_out += count;
            data->bytes_uncopied -= count;
            continue;
        }

        if (data->start_new_table && !rar_parse_codes(rar))
            return false;

        if (data->is_ppmd_block) {
            int lzss_offset, i, length;
            int sym = Ppmd7_DecodeSymbol(&data->ppmd7_context, &data->range_dec.p);
            if (sym < 0) {
                warn("Invalid symbol");
                return false;
            }
            if (sym != data->ppmd_escape) {
                lzss_emit_literal(&data->lzss, sym);
                data->bytes_uncopied++;
            }
            else {
                int code = Ppmd7_DecodeSymbol(&data->ppmd7_context, &data->range_dec.p);
                if (code < 0) {
                    warn("Invalid symbol");
                    return false;
                }

                switch (code) {
                case 0:
                    data->start_new_table = true;
                    continue;

                case 2:
                    data->ppmd_eod = true;
                    continue;

                case 3:
                    todo("Parsing filters are unsupported");
                    return false;

                case 4:
                    lzss_offset = 0;
                    for (i = 2; i >= 0; i--) {
                        code = Ppmd7_DecodeSymbol(&data->ppmd7_context, &data->range_dec.p);
                        if (code < 0) {
                            warn("Invalid symbol");
                            return false;
                        }
                        lzss_offset |= code << (i * 8);
                    }
                    length = Ppmd7_DecodeSymbol(&data->ppmd7_context, &data->range_dec.p);
                    if (length < 0) {
                        warn("Invalid symbol");
                        return false;
                    }
                    lzss_emit_match(&data->lzss, lzss_offset + 2, length + 32);
                    data->bytes_uncopied += length + 32;
                    break;

                case 5:
                    length = Ppmd7_DecodeSymbol(&data->ppmd7_context, &data->range_dec.p);
                    if (length < 0) {
                        warn("Invalid symbol");
                        return false;
                    }
                    lzss_emit_match(&data->lzss, 1, length + 4);
                    data->bytes_uncopied += length + 4;
                    break;

                default:
                    lzss_emit_literal(&data->lzss, sym);
                    data->bytes_uncopied++;
                    break;
                }
            }
        }
        else {
            int64_t start, end, actualend;

            start = buffer_offset;
            end = start + data->dict_size;
            data->filterstart = INT64_MAX;

            actualend = rar_expand(rar, end);
            if (actualend < 0)
                return false;

            data->bytes_uncopied = actualend - start;
            if (data->bytes_uncopied == 0) {
                warn("Internal error extracting RAR file");
                return false;
            }
        }

        count = (int)min(data->bytes_uncopied, buffer_size - buffer_offset);
        lzss_copy_bytes_from_window(&data->lzss, (uint8_t *)buffer + buffer_offset, rar->progr.offset_out, count);
        buffer_offset += count;
        rar->progr.offset_out += count;
        data->bytes_uncopied -= count;
    }

    return !data->at_eof;
}

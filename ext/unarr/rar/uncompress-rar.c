/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

/* adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/XADRAR30Handle.m */
/* adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/XADRAR20Handle.m */

#include "rar.h"

static void *gSzAlloc_Alloc(void *self, size_t size) { (void)self; return malloc(size); }
static void gSzAlloc_Free(void *self, void *ptr) { (void)self; free(ptr); }
static ISzAlloc gSzAlloc = { gSzAlloc_Alloc, gSzAlloc_Free };

static bool br_fill(ar_archive_rar *rar, int bits)
{
    uint8_t bytes[8];
    int count, i;
    /* read as many bits as possible */
    count = (64 - rar->uncomp.br.available) / 8;
    if (rar->progress.data_left < (size_t)count)
        count = (int)rar->progress.data_left;

    if (bits > rar->uncomp.br.available + 8 * count || ar_read(rar->super.stream, bytes, count) != (size_t)count) {
        if (!rar->uncomp.br.at_eof) {
            warn("Unexpected EOF during decompression (truncated file?)");
            rar->uncomp.br.at_eof = true;
        }
        return false;
    }
    rar->progress.data_left -= count;
    for (i = 0; i < count; i++) {
        rar->uncomp.br.bits = (rar->uncomp.br.bits << 8) | bytes[i];
    }
    rar->uncomp.br.available += 8 * count;
    return true;
}

static inline bool br_check(ar_archive_rar *rar, int bits)
{
    return bits <= rar->uncomp.br.available || br_fill(rar, bits);
}

static inline uint64_t br_bits(ar_archive_rar *rar, int bits)
{
    return (rar->uncomp.br.bits >> (rar->uncomp.br.available -= bits)) & (((uint64_t)1 << bits) - 1);
}

static Byte ByteIn_Read(void *p)
{
    struct ByteReader *self = p;
    return br_check(self->rar, 8) ? (Byte)br_bits(self->rar, 8) : 0xFF;
}

static void ByteIn_CreateVTable(struct ByteReader *br, ar_archive_rar *rar)
{
    br->super.Read = ByteIn_Read;
    br->rar = rar;
}

/* Ppmd7 range decoder differs between 7z and RAR */
static void PpmdRAR_RangeDec_Init(struct CPpmdRAR_RangeDec *p)
{
    int i;
    p->Code = 0;
    p->Low = 0;
    p->Range = 0xFFFFFFFF;
    for (i = 0; i < 4; i++) {
        p->Code = (p->Code << 8) | p->Stream->Read(p->Stream);
    }
}

static UInt32 Range_GetThreshold(void *p, UInt32 total)
{
    struct CPpmdRAR_RangeDec *self = p;
    return self->Code / (self->Range /= total);
}

static void Range_Decode_RAR(void *p, UInt32 start, UInt32 size)
{
    struct CPpmdRAR_RangeDec *self = p;
    self->Low += start * self->Range;
    self->Code -= start * self->Range;
    self->Range *= size;
    for (;;) {
        if ((self->Low ^ (self->Low + self->Range)) >= (1 << 24)) {
            if (self->Range >= (1 << 15))
                break;
            self->Range = ((uint32_t)(-(int32_t)self->Low)) & ((1 << 15) - 1);
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
    if (!bit)
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

static bool rar_init_uncompress(struct ar_archive_rar_uncomp *uncomp, uint8_t version)
{
    /* per XADRARParser.m @handleForSolidStreamWithObject these versions are identical */
    if (version == 29 || version == 36)
        version = 3;
    else if (version == 20 || version == 26)
        version = 2;
    else {
        warn("Unsupported compression version: %d", version);
        return false;
    }
    if (uncomp->version) {
        if (uncomp->version != version) {
            warn("Compression version mismatch: %d != %d", version, uncomp->version);
            return false;
        }
        return true;
    }
    memset(uncomp, 0, sizeof(*uncomp));
    uncomp->start_new_table = true;
    if (!lzss_initialize(&uncomp->lzss, LZSS_WINDOW_SIZE)) {
        warn("OOM during decompression");
        return false;
    }
    if (version == 3) {
        uncomp->state.v3.ppmd_escape = 2;
        uncomp->state.v3.filters.filterstart = SIZE_MAX;
    }
    uncomp->version = version;
    return true;
}

static void rar_free_codes(struct ar_archive_rar_uncomp *uncomp);

void rar_clear_uncompress(struct ar_archive_rar_uncomp *uncomp)
{
    if (!uncomp->version)
        return;
    rar_free_codes(uncomp);
    lzss_cleanup(&uncomp->lzss);
    if (uncomp->version == 3) {
        Ppmd7_Free(&uncomp->state.v3.ppmd7_context, &gSzAlloc);
        rar_clear_filters(&uncomp->state.v3.filters);
    }
    uncomp->version = 0;
}

static int rar_read_next_symbol(ar_archive_rar *rar, struct huffman_code *code)
{
    int node = 0;

    if (!code->table && !rar_make_table(code))
        return -1;

    /* performance optimization */
    if (code->tablesize <= rar->uncomp.br.available) {
        uint16_t bits = (uint16_t)br_bits(rar, code->tablesize);
        int length = code->table[bits].length;
        int value = code->table[bits].value;

        if (length < 0) {
            warn("Invalid data in bitstream"); /* invalid prefix code in bitstream */
            return -1;
        }
        if (length <= code->tablesize) {
            /* Skip only length bits */
            rar->uncomp.br.available += code->tablesize - length;
            return value;
        }

        node = value;
    }

    while (!rar_is_leaf_node(code, node)) {
        uint8_t bit;
        if (!br_check(rar, 1))
            return -1;
        bit = (uint8_t)br_bits(rar, 1);
        if (code->tree[node].branches[bit] < 0) {
            warn("Invalid data in bitstream"); /* invalid prefix code in bitstream */
            return -1;
        }
        node = code->tree[node].branches[bit];
    }

    return code->tree[node].branches[0];
}

/***** RAR version 2 decompression *****/

static void rar_free_codes_v2(struct ar_archive_rar_uncomp_v2 *uncomp_v2)
{
    int i;
    rar_free_code(&uncomp_v2->maincode);
    rar_free_code(&uncomp_v2->offsetcode);
    rar_free_code(&uncomp_v2->lengthcode);
    for (i = 0; i < 4; i++)
        rar_free_code(&uncomp_v2->audiocode[i]);
}

static bool rar_parse_codes_v2(ar_archive_rar *rar)
{
    struct ar_archive_rar_uncomp_v2 *uncomp_v2 = &rar->uncomp.state.v2;
    struct huffman_code precode;
    uint8_t prelengths[19];
    uint16_t i, count;
    int j, val, n;
    bool ok = false;

    rar_free_codes_v2(uncomp_v2);

    if (!br_check(rar, 2))
        return false;
    uncomp_v2->audioblock = br_bits(rar, 1) != 0;
    if (!br_bits(rar, 1))
        memset(uncomp_v2->lengthtable, 0, sizeof(uncomp_v2->lengthtable));

    if (uncomp_v2->audioblock) {
        if (!br_check(rar, 2))
            return false;
        uncomp_v2->numchannels = (uint8_t)br_bits(rar, 2) + 1;
        count = uncomp_v2->numchannels * 257;
        if (uncomp_v2->channel > uncomp_v2->numchannels)
            uncomp_v2->channel = 0;
    }
    else
        count = MAINCODE_SIZE_20 + OFFSETCODE_SIZE_20 + LENGTHCODE_SIZE_20;

    for (i = 0; i < 19; i++) {
        if (!br_check(rar, 4))
            return false;
        prelengths[i] = (uint8_t)br_bits(rar, 4);
    }

    memset(&precode, 0, sizeof(precode));
    if (!rar_create_code(&precode, prelengths, 19))
        goto PrecodeError;
    for (i = 0; i < count; ) {
        val = rar_read_next_symbol(rar, &precode);
        if (val < 0)
            goto PrecodeError;
        if (val < 16) {
            uncomp_v2->lengthtable[i] = (uncomp_v2->lengthtable[i] + val) & 0x0F;
            i++;
        }
        else if (val == 16) {
            if (i == 0) {
                warn("Invalid data in bitstream");
                goto PrecodeError;
            }
            if (!br_check(rar, 2))
                goto PrecodeError;
            n = (uint8_t)br_bits(rar, 2) + 3;
            for (j = 0; j < n && i < count; i++, j++) {
                uncomp_v2->lengthtable[i] = uncomp_v2->lengthtable[i - 1];
            }
        }
        else {
            if (val == 17) {
                if (!br_check(rar, 3))
                    goto PrecodeError;
                n = (uint8_t)br_bits(rar, 3) + 3;
            }
            else {
                if (!br_check(rar, 7))
                    goto PrecodeError;
                n = (uint8_t)br_bits(rar, 7) + 11;
            }
            for (j = 0; j < n && i < count; i++, j++) {
                uncomp_v2->lengthtable[i] = 0;
            }
        }
    }
    ok = true;
PrecodeError:
    rar_free_code(&precode);
    if (!ok)
        return false;

    if (uncomp_v2->audioblock) {
        for (i = 0; i < uncomp_v2->numchannels; i++) {
            if (!rar_create_code(&uncomp_v2->audiocode[i], uncomp_v2->lengthtable + i * 257, 257))
                return false;
        }
    }
    else {
        if (!rar_create_code(&uncomp_v2->maincode, uncomp_v2->lengthtable, MAINCODE_SIZE_20))
            return false;
        if (!rar_create_code(&uncomp_v2->offsetcode, uncomp_v2->lengthtable + MAINCODE_SIZE_20, OFFSETCODE_SIZE_20))
            return false;
        if (!rar_create_code(&uncomp_v2->lengthcode, uncomp_v2->lengthtable + MAINCODE_SIZE_20 + OFFSETCODE_SIZE_20, LENGTHCODE_SIZE_20))
            return false;
    }

    rar->uncomp.start_new_table = false;
    return true;
}

static uint8_t rar_decode_audio(struct AudioState *state, int8_t *channeldelta, int8_t delta)
{
    uint8_t predbyte, byte;
    int prederror;

    state->delta[3] = state->delta[2];
    state->delta[2] = state->delta[1];
    state->delta[1] = state->lastdelta - state->delta[0];
    state->delta[0] = state->lastdelta;

    predbyte = ((8 * state->lastbyte + state->weight[0] * state->delta[0] + state->weight[1] * state->delta[1] + state->weight[2] * state->delta[2] + state->weight[3] * state->delta[3] + state->weight[4] * *channeldelta) >> 3) & 0xFF;
    byte = (predbyte - delta) & 0xFF;

    prederror = delta << 3;
    state->error[0] += abs(prederror);
    state->error[1] += abs(prederror - state->delta[0]); state->error[2] += abs(prederror + state->delta[0]);
    state->error[3] += abs(prederror - state->delta[1]); state->error[4] += abs(prederror + state->delta[1]);
    state->error[5] += abs(prederror - state->delta[2]); state->error[6] += abs(prederror + state->delta[2]);
    state->error[7] += abs(prederror - state->delta[3]); state->error[8] += abs(prederror + state->delta[3]);
    state->error[9] += abs(prederror - *channeldelta); state->error[10] += abs(prederror + *channeldelta);

    *channeldelta = state->lastdelta = (int8_t)(byte - state->lastbyte);
    state->lastbyte = byte;

    if (!(++state->count & 0x1F)) {
        uint8_t i, idx = 0;
        for (i = 1; i < 11; i++) {
            if (state->error[i] < state->error[idx])
                idx = i;
        }
        memset(state->error, 0, sizeof(state->error));

        switch (idx) {
        case 1: if (state->weight[0] >= -16) state->weight[0]--; break;
        case 2: if (state->weight[0] < 16) state->weight[0]++; break;
        case 3: if (state->weight[1] >= -16) state->weight[1]--; break;
        case 4: if (state->weight[1] < 16) state->weight[1]++; break;
        case 5: if (state->weight[2] >= -16) state->weight[2]--; break;
        case 6: if (state->weight[2] < 16) state->weight[2]++; break;
        case 7: if (state->weight[3] >= -16) state->weight[3]--; break;
        case 8: if (state->weight[3] < 16) state->weight[3]++; break;
        case 9: if (state->weight[4] >= -16) state->weight[4]--; break;
        case 10: if (state->weight[4] < 16) state->weight[4]++; break;
        }
    }

    return byte;
}

int64_t rar_expand_v2(ar_archive_rar *rar, int64_t end)
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
           655360,  720896,  786432,  851968,  917504,  983040 };
    static const uint8_t offsetbits[] =
        {  0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,
           5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10,
          11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
          16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 };
    static const uint8_t shortbases[] =
        { 0, 4, 8, 16, 32, 64, 128, 192 };
    static const uint8_t shortbits[] =
        { 2, 2, 3, 4, 5, 6, 6, 6 };

    struct ar_archive_rar_uncomp_v2 *uncomp_v2 = &rar->uncomp.state.v2;
    LZSS *lzss = &rar->uncomp.lzss;
    int symbol, offs, len;

    if ((uint64_t)end > rar->super.entry_size_uncompressed + rar->solid.size_total)
        end = rar->super.entry_size_uncompressed + rar->solid.size_total;

    for (;;) {
        if (lzss_position(lzss) >= end)
            return end;

        if (uncomp_v2->audioblock) {
            uint8_t byte;
            symbol = rar_read_next_symbol(rar, &uncomp_v2->audiocode[uncomp_v2->channel]);
            if (symbol < 0)
                return -1;
            if (symbol == 256) {
                rar->uncomp.start_new_table = true;
                return lzss_position(lzss);
            }
            byte = rar_decode_audio(&uncomp_v2->audiostate[uncomp_v2->channel], &uncomp_v2->channeldelta, (int8_t)(uint8_t)symbol);
            uncomp_v2->channel++;
            if (uncomp_v2->channel == uncomp_v2->numchannels)
                uncomp_v2->channel = 0;
            lzss_emit_literal(lzss, byte);
            continue;
        }

        symbol = rar_read_next_symbol(rar, &uncomp_v2->maincode);
        if (symbol < 0)
            return -1;
        if (symbol < 256) {
            lzss_emit_literal(lzss, (uint8_t)symbol);
            continue;
        }
        if (symbol == 256) {
            offs = uncomp_v2->lastoffset;
            len = uncomp_v2->lastlength;
        }
        else if (symbol <= 260) {
            int idx = symbol - 256;
            int lensymbol = rar_read_next_symbol(rar, &uncomp_v2->lengthcode);
            offs = uncomp_v2->oldoffset[(uncomp_v2->oldoffsetindex - idx) & 0x03];
            if (lensymbol < 0 || lensymbol > (int)(sizeof(lengthbases) / sizeof(lengthbases[0])) || lensymbol > (int)(sizeof(lengthbits) / sizeof(lengthbits[0]))) {
                warn("Invalid data in bitstream");
                return -1;
            }
            len = lengthbases[lensymbol] + 2;
            if (lengthbits[lensymbol] > 0) {
                if (!br_check(rar, lengthbits[lensymbol]))
                    return -1;
                len += (uint8_t)br_bits(rar, lengthbits[lensymbol]);
            }
            if (offs >= 0x40000)
                len++;
            if (offs >= 0x2000)
                len++;
            if (offs >= 0x101)
                len++;
        }
        else if (symbol <= 268) {
            int idx = symbol - 261;
            offs = shortbases[idx] + 1;
            if (shortbits[idx] > 0) {
                if (!br_check(rar, shortbits[idx]))
                    return -1;
                offs += (uint8_t)br_bits(rar, shortbits[idx]);
            }
            len = 2;
        }
        else if (symbol == 269) {
            rar->uncomp.start_new_table = true;
            return lzss_position(lzss);
        }
        else {
            int idx = symbol - 270;
            int offssymbol;
            if (idx > (int)(sizeof(lengthbases) / sizeof(lengthbases[0])) || idx > (int)(sizeof(lengthbits) / sizeof(lengthbits[0]))) {
                warn("Invalid data in bitstream");
                return -1;
            }
            len = lengthbases[idx] + 3;
            if (lengthbits[idx] > 0) {
                if (!br_check(rar, lengthbits[idx]))
                    return -1;
                len += (uint8_t)br_bits(rar, lengthbits[idx]);
            }
            offssymbol = rar_read_next_symbol(rar, &uncomp_v2->offsetcode);
            if (offssymbol < 0 || offssymbol > (int)(sizeof(offsetbases) / sizeof(offsetbases[0])) || offssymbol > (int)(sizeof(offsetbits) / sizeof(offsetbits[0]))) {
                warn("Invalid data in bitstream");
                return -1;
            }
            offs = offsetbases[offssymbol] + 1;
            if (offsetbits[offssymbol] > 0) {
                if (!br_check(rar, offsetbits[offssymbol]))
                    return -1;
                offs += (int)br_bits(rar, offsetbits[offssymbol]);
            }
            if (offs >= 0x40000)
                len++;
            if (offs >= 0x2000)
                len++;
        }

        uncomp_v2->lastoffset = uncomp_v2->oldoffset[uncomp_v2->oldoffsetindex++ & 0x03] = offs;
        uncomp_v2->lastlength = len;

        lzss_emit_match(lzss, offs, len);
    }
}

/***** RAR version 3 decompression *****/

static void rar_free_codes(struct ar_archive_rar_uncomp *uncomp)
{
    struct ar_archive_rar_uncomp_v3 *uncomp_v3 = &uncomp->state.v3;

    if (uncomp->version == 2) {
        rar_free_codes_v2(&uncomp->state.v2);
        return;
    }

    rar_free_code(&uncomp_v3->maincode);
    rar_free_code(&uncomp_v3->offsetcode);
    rar_free_code(&uncomp_v3->lowoffsetcode);
    rar_free_code(&uncomp_v3->lengthcode);
}

static bool rar_parse_codes(ar_archive_rar *rar)
{
    struct ar_archive_rar_uncomp_v3 *uncomp_v3 = &rar->uncomp.state.v3;

    if (rar->uncomp.version == 2)
        return rar_parse_codes_v2(rar);

    rar_free_codes(&rar->uncomp);

    br_clear_leftover_bits(&rar->uncomp);

    if (!br_check(rar, 1))
        return false;
    uncomp_v3->is_ppmd_block = br_bits(rar, 1) != 0;
    if (uncomp_v3->is_ppmd_block) {
        uint8_t ppmd_flags;
        uint32_t max_alloc = 0;

        if (!br_check(rar, 7))
            return false;
        ppmd_flags = (uint8_t)br_bits(rar, 7);
        if ((ppmd_flags & 0x20)) {
            if (!br_check(rar, 8))
                return false;
            max_alloc = ((uint8_t)br_bits(rar, 8) + 1) << 20;
        }
        if ((ppmd_flags & 0x40)) {
            if (!br_check(rar, 8))
                return false;
            uncomp_v3->ppmd_escape = (uint8_t)br_bits(rar, 8);
        }
        if ((ppmd_flags & 0x20)) {
            uint32_t maxorder = (ppmd_flags & 0x1F) + 1;
            if (maxorder == 1)
                return false;
            if (maxorder > 16)
                maxorder = 16 + (maxorder - 16) * 3;

            Ppmd7_Free(&uncomp_v3->ppmd7_context, &gSzAlloc);
            Ppmd7_Construct(&uncomp_v3->ppmd7_context);
            if (!Ppmd7_Alloc(&uncomp_v3->ppmd7_context, max_alloc, &gSzAlloc)) {
                warn("OOM during decompression");
                return false;
            }
            ByteIn_CreateVTable(&uncomp_v3->bytein, rar);
            PpmdRAR_RangeDec_CreateVTable(&uncomp_v3->range_dec, &uncomp_v3->bytein.super);
            PpmdRAR_RangeDec_Init(&uncomp_v3->range_dec);
            Ppmd7_Init(&uncomp_v3->ppmd7_context, maxorder);
        }
        else {
            if (!Ppmd7_WasAllocated(&uncomp_v3->ppmd7_context)) {
                warn("Invalid data in bitstream"); /* invalid PPMd sequence */
                return false;
            }
            PpmdRAR_RangeDec_Init(&uncomp_v3->range_dec);
        }
    }
    else {
        struct huffman_code precode;
        uint8_t bitlengths[20];
        uint8_t zerocount;
        int i, j, val, n;
        bool ok = false;

        if (!br_check(rar, 1))
            return false;
        if (!br_bits(rar, 1))
            memset(uncomp_v3->lengthtable, 0, sizeof(uncomp_v3->lengthtable));
        memset(&bitlengths, 0, sizeof(bitlengths));
        for (i = 0; i < sizeof(bitlengths); i++) {
            if (!br_check(rar, 4))
                return false;
            bitlengths[i] = (uint8_t)br_bits(rar, 4);
            if (bitlengths[i] == 0x0F) {
                if (!br_check(rar, 4))
                    return false;
                zerocount = (uint8_t)br_bits(rar, 4);
                if (zerocount) {
                    for (j = 0; j < zerocount + 2 && i < sizeof(bitlengths); j++) {
                        bitlengths[i++] = 0;
                    }
                    i--;
                }
            }
        }

        memset(&precode, 0, sizeof(precode));
        if (!rar_create_code(&precode, bitlengths, sizeof(bitlengths)))
            goto PrecodeError;
        for (i = 0; i < HUFFMAN_TABLE_SIZE; ) {
            val = rar_read_next_symbol(rar, &precode);
            if (val < 0)
                goto PrecodeError;
            if (val < 16) {
                uncomp_v3->lengthtable[i] = (uncomp_v3->lengthtable[i] + val) & 0x0F;
                i++;
            }
            else if (val < 18) {
                if (i == 0) {
                    warn("Invalid data in bitstream");
                    goto PrecodeError;
                }
                if (val == 16) {
                    if (!br_check(rar, 3))
                        goto PrecodeError;
                    n = (uint8_t)br_bits(rar, 3) + 3;
                }
                else {
                    if (!br_check(rar, 7))
                        goto PrecodeError;
                    n = (uint8_t)br_bits(rar, 7) + 11;
                }
                for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; i++, j++) {
                    uncomp_v3->lengthtable[i] = uncomp_v3->lengthtable[i - 1];
                }
            }
            else {
                if (val == 18) {
                    if (!br_check(rar, 3))
                        goto PrecodeError;
                    n = (uint8_t)br_bits(rar, 3) + 3;
                }
                else {
                    if (!br_check(rar, 7))
                        goto PrecodeError;
                    n = (uint8_t)br_bits(rar, 7) + 11;
                }
                for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; i++, j++) {
                    uncomp_v3->lengthtable[i] = 0;
                }
            }
        }
        ok = true;
PrecodeError:
        rar_free_code(&precode);
        if (!ok)
            return false;

        if (!rar_create_code(&uncomp_v3->maincode, uncomp_v3->lengthtable, MAINCODE_SIZE))
            return false;
        if (!rar_create_code(&uncomp_v3->offsetcode, uncomp_v3->lengthtable + MAINCODE_SIZE, OFFSETCODE_SIZE))
            return false;
        if (!rar_create_code(&uncomp_v3->lowoffsetcode, uncomp_v3->lengthtable + MAINCODE_SIZE + OFFSETCODE_SIZE, LOWOFFSETCODE_SIZE))
            return false;
        if (!rar_create_code(&uncomp_v3->lengthcode, uncomp_v3->lengthtable + MAINCODE_SIZE + OFFSETCODE_SIZE + LOWOFFSETCODE_SIZE, LENGTHCODE_SIZE))
            return false;
    }

    rar->uncomp.start_new_table = false;
    return true;
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
        warn("OOM during decompression");
        return false;
    }
    for (i = 0; i < length; i++) {
        if (!decode_byte(rar, &code[i])) {
            free(code);
            return false;
        }
    }
    if (!rar_parse_filter(rar, code, length, flags)) {
        free(code);
        return false;
    }
    free(code);

    if (rar->uncomp.state.v3.filters.filterstart < (size_t)*end)
        *end = rar->uncomp.state.v3.filters.filterstart;

    return true;
}

static inline bool rar_decode_ppmd7_symbol(struct ar_archive_rar_uncomp_v3 *uncomp_v3, Byte *symbol)
{
    int value = Ppmd7_DecodeSymbol(&uncomp_v3->ppmd7_context, &uncomp_v3->range_dec.super);
    if (value < 0) {
        warn("Invalid data in bitstream"); /* invalid PPMd symbol */
        return false;
    }
    *symbol = (Byte)value;
    return true;
}

static bool rar_decode_byte(ar_archive_rar *rar, uint8_t *byte)
{
    if (!br_check(rar, 8))
        return false;
    *byte = (uint8_t)br_bits(rar, 8);
    return true;
}

static bool rar_decode_ppmd7_byte(ar_archive_rar *rar, uint8_t *byte)
{
    return rar_decode_ppmd7_symbol(&rar->uncomp.state.v3, byte);
}

static bool rar_handle_ppmd_sequence(ar_archive_rar *rar, int64_t *end)
{
    struct ar_archive_rar_uncomp_v3 *uncomp_v3 = &rar->uncomp.state.v3;
    LZSS *lzss = &rar->uncomp.lzss;
    Byte sym, code, length;
    int lzss_offset;

    if (!rar_decode_ppmd7_symbol(uncomp_v3, &sym))
        return false;
    if (sym != uncomp_v3->ppmd_escape) {
        lzss_emit_literal(lzss, sym);
        return true;
    }

    if (!rar_decode_ppmd7_symbol(uncomp_v3, &code))
        return false;
    switch (code) {
    case 0:
        return rar_parse_codes(rar);

    case 2:
        rar->uncomp.start_new_table = true;
        return true;

    case 3:
        return rar_read_filter(rar, rar_decode_ppmd7_byte, end);

    case 4:
        if (!rar_decode_ppmd7_symbol(uncomp_v3, &code))
            return false;
        lzss_offset = code << 16;
        if (!rar_decode_ppmd7_symbol(uncomp_v3, &code))
            return false;
        lzss_offset |= code << 8;
        if (!rar_decode_ppmd7_symbol(uncomp_v3, &code))
            return false;
        lzss_offset |= code;
        if (!rar_decode_ppmd7_symbol(uncomp_v3, &length))
            return false;
        lzss_emit_match(lzss, lzss_offset + 2, length + 32);
        return true;

    case 5:
        if (!rar_decode_ppmd7_symbol(uncomp_v3, &length))
            return false;
        lzss_emit_match(lzss, 1, length + 4);
        return true;

    default:
        lzss_emit_literal(lzss, sym);
        return true;
    }
}

int64_t rar_expand(ar_archive_rar *rar, int64_t end)
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

    struct ar_archive_rar_uncomp_v3 *uncomp_v3 = &rar->uncomp.state.v3;
    LZSS *lzss = &rar->uncomp.lzss;
    int symbol, offs, len, i;

    if (rar->uncomp.version == 2)
        return rar_expand_v2(rar, end);

    for (;;) {
        if (lzss_position(lzss) >= end)
            return end;

        if (uncomp_v3->is_ppmd_block) {
            if (!rar_handle_ppmd_sequence(rar, &end))
                return -1;
            if (rar->uncomp.start_new_table)
                return lzss_position(lzss);
            continue;
        }

        symbol = rar_read_next_symbol(rar, &uncomp_v3->maincode);
        if (symbol < 0)
            return -1;
        if (symbol < 256) {
            lzss_emit_literal(lzss, (uint8_t)symbol);
            continue;
        }
        if (symbol == 256) {
            if (!br_check(rar, 1))
                return -1;
            if (!br_bits(rar, 1)) {
                if (!br_check(rar, 1))
                    return -1;
                rar->uncomp.start_new_table = br_bits(rar, 1) != 0;
                return lzss_position(lzss);
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
            if (uncomp_v3->lastlength == 0)
                continue;
            offs = uncomp_v3->lastoffset;
            len = uncomp_v3->lastlength;
        }
        else if (symbol <= 262) {
            int idx = symbol - 259;
            int lensymbol = rar_read_next_symbol(rar, &uncomp_v3->lengthcode);
            offs = uncomp_v3->oldoffset[idx];
            if (lensymbol < 0 || lensymbol > (int)(sizeof(lengthbases) / sizeof(lengthbases[0])) || lensymbol > (int)(sizeof(lengthbits) / sizeof(lengthbits[0]))) {
                warn("Invalid data in bitstream");
                return -1;
            }
            len = lengthbases[lensymbol] + 2;
            if (lengthbits[lensymbol] > 0) {
                if (!br_check(rar, lengthbits[lensymbol]))
                    return -1;
                len += (uint8_t)br_bits(rar, lengthbits[lensymbol]);
            }
            for (i = idx; i > 0; i--)
                uncomp_v3->oldoffset[i] = uncomp_v3->oldoffset[i - 1];
            uncomp_v3->oldoffset[0] = offs;
        }
        else if (symbol <= 270) {
            int idx = symbol - 263;
            offs = shortbases[idx] + 1;
            if (shortbits[idx] > 0) {
                if (!br_check(rar, shortbits[idx]))
                    return -1;
                offs += (uint8_t)br_bits(rar, shortbits[idx]);
            }
            len = 2;
            for (i = 3; i > 0; i--)
                uncomp_v3->oldoffset[i] = uncomp_v3->oldoffset[i - 1];
            uncomp_v3->oldoffset[0] = offs;
        }
        else {
            int idx = symbol - 271;
            int offssymbol;
            if (idx > (int)(sizeof(lengthbases) / sizeof(lengthbases[0])) || idx > (int)(sizeof(lengthbits) / sizeof(lengthbits[0]))) {
                warn("Invalid data in bitstream");
                return -1;
            }
            len = lengthbases[idx] + 3;
            if (lengthbits[idx] > 0) {
                if (!br_check(rar, lengthbits[idx]))
                    return -1;
                len += (uint8_t)br_bits(rar, lengthbits[idx]);
            }
            offssymbol = rar_read_next_symbol(rar, &uncomp_v3->offsetcode);
            if (offssymbol < 0 || offssymbol > (int)(sizeof(offsetbases) / sizeof(offsetbases[0])) || offssymbol > (int)(sizeof(offsetbits) / sizeof(offsetbits[0]))) {
                warn("Invalid data in bitstream");
                return -1;
            }
            offs = offsetbases[offssymbol] + 1;
            if (offsetbits[offssymbol] > 0) {
                if (offssymbol > 9) {
                    if (offsetbits[offssymbol] > 4) {
                        if (!br_check(rar, offsetbits[offssymbol] - 4))
                            return -1;
                        offs += (int)br_bits(rar, offsetbits[offssymbol] - 4) << 4;
                    }
                    if (uncomp_v3->numlowoffsetrepeats > 0) {
                        uncomp_v3->numlowoffsetrepeats--;
                        offs += uncomp_v3->lastlowoffset;
                    }
                    else {
                        int lowoffsetsymbol = rar_read_next_symbol(rar, &uncomp_v3->lowoffsetcode);
                        if (lowoffsetsymbol < 0)
                            return -1;
                        if (lowoffsetsymbol == 16) {
                            uncomp_v3->numlowoffsetrepeats = 15;
                            offs += uncomp_v3->lastlowoffset;
                        }
                        else {
                            offs += lowoffsetsymbol;
                            uncomp_v3->lastlowoffset = lowoffsetsymbol;
                        }
                    }
                }
                else {
                    if (!br_check(rar, offsetbits[offssymbol]))
                        return -1;
                    offs += (int)br_bits(rar, offsetbits[offssymbol]);
                }
            }

            if (offs >= 0x40000)
                len++;
            if (offs >= 0x2000)
                len++;

            for (i = 3; i > 0; i--)
                uncomp_v3->oldoffset[i] = uncomp_v3->oldoffset[i - 1];
            uncomp_v3->oldoffset[0] = offs;
        }

        uncomp_v3->lastoffset = offs;
        uncomp_v3->lastlength = len;

        lzss_emit_match(lzss, offs, len);
    }
}

bool rar_uncompress_part(ar_archive_rar *rar, void *buffer, size_t buffer_size)
{
    struct ar_archive_rar_uncomp *uncomp = &rar->uncomp;
    struct ar_archive_rar_uncomp_v3 *uncomp_v3 = NULL;
    size_t end;

    if (!rar_init_uncompress(uncomp, rar->entry.version))
        return false;
    if (uncomp->version == 3)
        uncomp_v3 = &uncomp->state.v3;

    for (;;) {
        if (uncomp_v3 && uncomp_v3->filters.bytes_ready > 0) {
            size_t count = smin(uncomp_v3->filters.bytes_ready, buffer_size);
            memcpy(buffer, uncomp_v3->filters.bytes, count);
            uncomp_v3->filters.bytes_ready -= count;
            uncomp_v3->filters.bytes += count;
            rar->progress.bytes_done += count;
            buffer_size -= count;
            buffer = (uint8_t *)buffer + count;
            if (rar->progress.bytes_done == rar->super.entry_size_uncompressed)
                goto FinishBlock;
        }
        else if (uncomp->bytes_ready > 0) {
            int count = (int)smin(uncomp->bytes_ready, buffer_size);
            lzss_copy_bytes_from_window(&uncomp->lzss, buffer, rar->progress.bytes_done + rar->solid.size_total, count);
            uncomp->bytes_ready -= count;
            rar->progress.bytes_done += count;
            buffer_size -= count;
            buffer = (uint8_t *)buffer + count;
        }
        if (buffer_size == 0)
            return true;

        if (uncomp->br.at_eof)
            return false;

        if (uncomp_v3 && uncomp_v3->filters.lastend == uncomp_v3->filters.filterstart) {
            if (!rar_run_filters(rar))
                return false;
            continue;
        }

FinishBlock:
        if (uncomp->start_new_table && !rar_parse_codes(rar))
            return false;

        end = rar->progress.bytes_done + rar->solid.size_total + LZSS_WINDOW_SIZE - LZSS_OVERFLOW_SIZE;
        if (uncomp_v3 && uncomp_v3->filters.filterstart < end)
            end = uncomp_v3->filters.filterstart;
        end = (size_t)rar_expand(rar, end);
        if (end == (size_t)-1 || end < rar->progress.bytes_done + rar->solid.size_total)
            return false;
        uncomp->bytes_ready = end - rar->progress.bytes_done - rar->solid.size_total;
        if (uncomp_v3)
            uncomp_v3->filters.lastend = end;

        if (uncomp_v3 && uncomp_v3->is_ppmd_block && uncomp->start_new_table)
            goto FinishBlock;
    }
}

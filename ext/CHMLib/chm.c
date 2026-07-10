

#ifndef CHM_H
#define CHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*chm_alloc_cb)(void *user, void *ctx, size_t size);
typedef void  (*chm_free_cb)(void *user, void *ctx, void *ptr);

typedef void (*chm_error_cb)(void *user, int severity, const char *msg);

typedef struct chm_ctx chm_ctx;

chm_ctx *chm_ctx_new(chm_alloc_cb alloc, chm_free_cb free_cb,
                     chm_error_cb error, void *user);
void chm_ctx_free(chm_ctx *ctx);

bool chm_open(chm_ctx *ctx, const uint8_t *data, size_t len);
void chm_close(chm_ctx *ctx);

#define CHM_UNCOMPRESSED 0
#define CHM_COMPRESSED 1

struct chm_entry {
    uint64_t start;
    uint64_t length;
    uint32_t space;
    bool is_compressed;
    bool is_dir;
    bool is_file;
    bool is_normal;
    bool is_meta;
    bool is_special;
    char *path;
};

int64_t chm_read_entry(chm_ctx *ctx, struct chm_entry *entry, uint8_t *buf);

int chm_get_entries(chm_ctx *ctx, struct chm_entry ***outEntries);

#ifdef __cplusplus
}
#endif

#endif

#ifndef CHM_INTERNAL_H
#define CHM_INTERNAL_H

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CHM_RESTRICT
#if defined(_MSC_VER)
#define CHM_RESTRICT __restrict
#else
#define CHM_RESTRICT restrict
#endif
#endif

#define DECR_OK (0)
#define DECR_DATAFORMAT (1)
#define DECR_ILLEGALDATA (2)
#define DECR_NOMEMORY (3)

#define LZX_MIN_MATCH (2)
#define LZX_MAX_MATCH (257)
#define LZX_NUM_CHARS (256)
#define LZX_BLOCKTYPE_INVALID (0)
#define LZX_BLOCKTYPE_VERBATIM (1)
#define LZX_BLOCKTYPE_ALIGNED (2)
#define LZX_BLOCKTYPE_UNCOMPRESSED (3)
#define LZX_PRETREE_NUM_ELEMENTS (20)
#define LZX_ALIGNED_NUM_ELEMENTS (8)
#define LZX_NUM_PRIMARY_LENGTHS (7)
#define LZX_NUM_SECONDARY_LENGTHS (249)

#define LZX_PRETREE_MAXSYMBOLS (LZX_PRETREE_NUM_ELEMENTS)
#define LZX_PRETREE_TABLEBITS (6)
#define LZX_MAINTREE_MAXSYMBOLS (LZX_NUM_CHARS + 50 * 8)
#define LZX_MAINTREE_TABLEBITS (12)
#define LZX_LENGTH_MAXSYMBOLS (LZX_NUM_SECONDARY_LENGTHS + 1)
#define LZX_LENGTH_TABLEBITS (12)
#define LZX_ALIGNED_MAXSYMBOLS (LZX_ALIGNED_NUM_ELEMENTS)
#define LZX_ALIGNED_TABLEBITS (7)

#define LZX_LENTABLE_SAFETY (64)

#define LZX_DECLARE_TABLE(tbl) \
    uint16_t tbl##_table[(1 << LZX_##tbl##_TABLEBITS) + (LZX_##tbl##_MAXSYMBOLS << 1)]; \
    uint8_t tbl##_len[LZX_##tbl##_MAXSYMBOLS + LZX_LENTABLE_SAFETY]

struct LZXstate {
    uint8_t *window;
    uint32_t window_size;
    uint32_t actual_size;
    uint32_t window_posn;
    uint32_t R0, R1, R2;
    uint16_t main_elements;
    int header_read;
    uint16_t block_type;
    uint32_t block_length;
    uint32_t block_remaining;
    uint32_t frames_read;
    int32_t intel_filesize;
    int32_t intel_curpos;
    int intel_started;

    LZX_DECLARE_TABLE(PRETREE);
    LZX_DECLARE_TABLE(MAINTREE);
    LZX_DECLARE_TABLE(LENGTH);
    LZX_DECLARE_TABLE(ALIGNED);
};

struct LZXstate *LZXinit(int window);
void LZXteardown(struct LZXstate *pState);
int LZXreset(struct LZXstate *pState);
int LZXdecompress(struct LZXstate *pState, uint8_t *inpos, uint8_t *outpos, int inlen, int outlen);
int LZX_test_pretree_make_decode_table(void);

#define CHM_MAX_BLOCKS_CACHED 5
#define CHM_MAX_DIR_PAGES 65536
#define CHM_DIR_SEEN_BITMAP_BITS CHM_MAX_DIR_PAGES
#define CHM_DIR_SEEN_BITMAP_WORDS (CHM_DIR_SEEN_BITMAP_BITS / 32)

#if defined(_WIN32)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#define _CHM_ITSF_V2_LEN (0x58)
#define _CHM_ITSF_V3_LEN (0x60)

struct chmItsfHeader {
    char signature[4];
    int32_t version;
    int32_t header_len;
    int32_t unknown_000c;
    uint32_t last_modified;
    uint32_t lang_id;
    uint8_t dir_uuid[16];
    uint8_t stream_uuid[16];
    uint64_t unknown_offset;
    uint64_t unknown_len;
    uint64_t dir_offset;
    uint64_t dir_len;
    uint64_t data_offset;
};

#define _CHM_ITSP_V1_LEN (0x54)

struct chmItspHeader {
    char signature[4];
    int32_t version;
    int32_t header_len;
    int32_t unknown_000c;
    uint32_t block_len;
    int32_t blockidx_intvl;
    int32_t index_depth;
    int32_t index_root;
    int32_t index_head;
    int32_t unknown_0024;
    uint32_t num_blocks;
    int32_t unknown_002c;
    uint32_t lang_id;
    uint8_t system_uuid[16];
    uint8_t unknown_0044[16];
};

static const char *_chm_pmgl_marker = "PMGL";
#define _CHM_PMGL_LEN (0x14)

struct chmPmglHeader {
    char signature[4];
    uint32_t free_space;
    uint32_t unknown_0008;
    int32_t block_prev;
    int32_t block_next;
};

static const char *_chm_pmgi_marker = "PMGI";
#define _CHM_PMGI_LEN (0x08)

struct chmPmgiHeader {
    char signature[4];
    uint32_t free_space;
};

#define _CHM_LZXC_RESETTABLE_V1_LEN (0x28)

struct chmLzxcResetTable {
    uint32_t version;
    uint32_t block_count;
    uint32_t unknown;
    uint32_t table_offset;
    uint64_t uncompressed_len;
    uint64_t compressed_len;
    uint64_t block_len;
};

#define _CHM_LZXC_MIN_LEN (0x18)
#define _CHM_LZXC_V2_LEN (0x1c)

struct chmLzxcControlData {
    uint32_t size;
    char signature[4];
    uint32_t version;
    uint32_t resetInterval;
    uint32_t windowSize;
    uint32_t windowsPerReset;
    uint32_t unknown_18;
};

void *chm_alloc(chm_ctx *ctx, size_t size);
void  chm_free(chm_ctx *ctx, void *ptr);
void  chm_errorf(chm_ctx *ctx, int sev, const char *fmt, ...);

struct chmDirSession {
    chm_ctx *ctx;
    uint8_t *page_buf;
    uint8_t *page_buf_end;
};

struct chm_ctx {
    chm_alloc_cb alloc;
    chm_free_cb  free;
    chm_error_cb error;
    void *user;

    const uint8_t *data;
    size_t data_len;

    uint64_t dir_offset;
    uint64_t dir_len;
    uint64_t data_offset;
    int32_t index_root;
    int32_t index_head;
    uint32_t block_len;

    uint64_t span;
    struct chm_entry rt_entry;
    struct chm_entry cn_entry;
    struct chmLzxcResetTable reset_table;

    int compression_enabled;
    uint32_t window_size;
    uint32_t reset_interval;
    uint32_t reset_blkcount;

    struct LZXstate *lzx_state;
    int lzx_last_block;

    uint8_t *cache_blocks[CHM_MAX_BLOCKS_CACHED];
    int64_t cache_block_indices[CHM_MAX_BLOCKS_CACHED];
    int cache_num_blocks;

    uint64_t dir_page_count;
    uint64_t dir_pages_seen;
    uint32_t dir_seen_bitmap[CHM_DIR_SEEN_BITMAP_WORDS];

    struct chm_entry *entries;
    int entry_count;
    struct chm_entry **entry_ptrs;
};

#endif

static const uint8_t extra_bits[51] = {0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,
                                     7,  8,  8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15,
                                     16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17};

static const uint32_t position_base[51] = {
    0,      1,      2,      3,       4,       6,       8,       12,      16,      24,      32,      48,     64,
    96,     128,    192,    256,     384,     512,     768,     1024,    1536,    2048,    3072,    4096,   6144,
    8192,   12288,  16384,  24576,   32768,   49152,   65536,   98304,   131072,  196608,  262144,  393216, 524288,
    655360, 786432, 917504, 1048576, 1179648, 1310720, 1441792, 1572864, 1703936, 1835008, 1966080, 2097152};

struct LZXstate* LZXinit(int window) {
    struct LZXstate* pState = NULL;
    uint32_t wndsize = 1 << window;
    int i, posn_slots;

    if (window < 15 || window > 21) return NULL;

    pState = (struct LZXstate*)malloc(sizeof(struct LZXstate));
    if (pState) {
        pState->window = (uint8_t*)malloc(wndsize);
    }
    if (!pState || !pState->window) {
        free(pState);
        return NULL;
    }
    memset(pState->window, 0, wndsize);
    pState->actual_size = wndsize;
    pState->window_size = wndsize;

    if (window == 20)
        posn_slots = 42;
    else if (window == 21)
        posn_slots = 50;
    else
        posn_slots = window << 1;

    pState->R0 = pState->R1 = pState->R2 = 1;
    pState->main_elements = LZX_NUM_CHARS + (posn_slots << 3);
    pState->header_read = 0;
    pState->frames_read = 0;
    pState->block_remaining = 0;
    pState->block_type = LZX_BLOCKTYPE_INVALID;
    pState->intel_curpos = 0;
    pState->intel_started = 0;
    pState->window_posn = 0;

    for (i = 0; i < LZX_MAINTREE_MAXSYMBOLS; i++) pState->MAINTREE_len[i] = 0;
    for (i = 0; i < LZX_LENGTH_MAXSYMBOLS; i++) pState->LENGTH_len[i] = 0;

    return pState;
}

void LZXteardown(struct LZXstate* pState) {
    if (pState) {
        if (pState->window) free(pState->window);
        free(pState);
    }
}

int LZXreset(struct LZXstate* pState) {
    int i;

    pState->R0 = pState->R1 = pState->R2 = 1;
    pState->header_read = 0;
    pState->frames_read = 0;
    pState->block_remaining = 0;
    pState->block_type = LZX_BLOCKTYPE_INVALID;
    pState->intel_curpos = 0;
    pState->intel_started = 0;
    pState->window_posn = 0;
    memset(pState->window, 0, pState->window_size);

    for (i = 0; i < LZX_MAINTREE_MAXSYMBOLS + LZX_LENTABLE_SAFETY; i++) pState->MAINTREE_len[i] = 0;
    for (i = 0; i < LZX_LENGTH_MAXSYMBOLS + LZX_LENTABLE_SAFETY; i++) pState->LENGTH_len[i] = 0;

    return DECR_OK;
}

#define BITBUF_WIDTH (sizeof(uint32_t) << 3)

#define INIT_BITSTREAM \
    do {               \
        bitsleft = 0;  \
        bitbuf = 0;    \
    } while (0)

#define ENSURE_BITS(n)                                                             \
    while (bitsleft < (n)) {                                                       \
        uint32_t next_bits = 0;                                                       \
        if ((uint8_t*)inpos < (uint8_t*)endinp) next_bits = inpos[0];                  \
        if ((uint8_t*)inpos + 1 < (uint8_t*)endinp) next_bits |= (uint32_t)inpos[1] << 8; \
        if ((uint8_t*)inpos > (uint8_t*)endinp + 2) return DECR_ILLEGALDATA;           \
        bitbuf |= next_bits << (BITBUF_WIDTH - 16 - bitsleft);                       \
        bitsleft += 16;                                                            \
        inpos += 2;                                                                \
    }

#define PEEK_BITS(n) (bitbuf >> (BITBUF_WIDTH - (n)))
#define REMOVE_BITS(n) ((bitbuf <<= (n)), (bitsleft -= (n)))

#define READ_BITS(v, n)     \
    do {                    \
        ENSURE_BITS(n);     \
        (v) = PEEK_BITS(n); \
        REMOVE_BITS(n);     \
    } while (0)

#define TABLEBITS(tbl) (LZX_##tbl##_TABLEBITS)
#define MAXSYMBOLS(tbl) (LZX_##tbl##_MAXSYMBOLS)
#define SYMTABLE(tbl) (pState->tbl##_table)
#define LENTABLE(tbl) (pState->tbl##_len)

#define BUILD_TABLE(tbl)                                                                    \
    if (make_decode_table(MAXSYMBOLS(tbl), TABLEBITS(tbl), LENTABLE(tbl), SYMTABLE(tbl))) { \
        return DECR_ILLEGALDATA;                                                            \
    }

#define READ_HUFFSYM(tbl, var)                                             \
    do {                                                                   \
        ENSURE_BITS(16);                                                   \
        hufftbl = SYMTABLE(tbl);                                           \
        if ((i = hufftbl[PEEK_BITS(TABLEBITS(tbl))]) >= MAXSYMBOLS(tbl)) { \
            j = 1 << (BITBUF_WIDTH - TABLEBITS(tbl));                        \
            do {                                                           \
                j >>= 1;                                                   \
                i <<= 1;                                                   \
                i |= (bitbuf & j) ? 1 : 0;                                 \
                if (!j) {                                                  \
                    return DECR_ILLEGALDATA;                               \
                }                                                          \
            } while ((i = hufftbl[i]) >= MAXSYMBOLS(tbl));                 \
        }                                                                  \
        j = LENTABLE(tbl)[(var) = i];                                      \
        REMOVE_BITS(j);                                                    \
    } while (0)

#define READ_LENGTHS(tbl, first, last)                                    \
    do {                                                                  \
        lb.bb = bitbuf;                                                   \
        lb.bl = bitsleft;                                                 \
        lb.ip = inpos;                                                    \
        lb.end = endinp;                                                  \
        if (lzx_read_lens(pState, LENTABLE(tbl), (first), (last), &lb)) { \
            return DECR_ILLEGALDATA;                                      \
        }                                                                 \
        bitbuf = lb.bb;                                                   \
        bitsleft = lb.bl;                                                 \
        inpos = lb.ip;                                                    \
    } while (0)

static int make_decode_table(uint32_t nsyms, uint32_t nbits, uint8_t* length, uint16_t* table) {
    register uint16_t sym;
    register uint32_t leaf;
    register uint8_t bit_num = 1;
    uint32_t fill;
    uint32_t pos = 0;
    uint32_t table_mask = 1 << nbits;
    uint32_t table_elems = table_mask + (nsyms << 1);
    uint32_t bit_mask = table_mask >> 1;
    uint32_t next_symbol = bit_mask;

    while (bit_num <= nbits) {
        for (sym = 0; sym < nsyms; sym++) {
            if (length[sym] == bit_num) {
                leaf = pos;

                if ((pos += bit_mask) > table_mask) return 1;

                fill = bit_mask;
                while (fill-- > 0) table[leaf++] = sym;
            }
        }
        bit_mask >>= 1;
        bit_num++;
    }

    if (pos != table_mask) {

        for (sym = pos; sym < table_mask; sym++) table[sym] = 0;

        pos <<= 16;
        table_mask <<= 16;
        bit_mask = 1 << 15;

        while (bit_num <= 16) {
            for (sym = 0; sym < nsyms; sym++) {
                if (length[sym] == bit_num) {
                    leaf = pos >> 16;
                    for (fill = 0; fill < bit_num - nbits; fill++) {

                        if (table[leaf] == 0) {
                            if ((next_symbol << 1) + 1 >= table_elems) {
                                return 1;
                            }
                            table[(next_symbol << 1)] = 0;
                            table[(next_symbol << 1) + 1] = 0;
                            table[leaf] = next_symbol++;
                        }

                        leaf = table[leaf] << 1;
                        if ((pos >> (15 - fill)) & 1) leaf++;
                    }
                    table[leaf] = sym;

                    if ((pos += bit_mask) > table_mask) return 1;
                }
            }
            bit_mask >>= 1;
            bit_num++;
        }
    }

    if (pos == table_mask) return 0;

    for (sym = 0; sym < nsyms; sym++)
        if (length[sym]) return 1;
    return 0;
}

int LZX_test_pretree_make_decode_table(void) {
    uint32_t nsyms = LZX_PRETREE_MAXSYMBOLS;
    uint32_t nbits = LZX_PRETREE_TABLEBITS;
    uint32_t table_elems = (1 << nbits) + (nsyms << 1);
    uint16_t* table = (uint16_t*)calloc(table_elems, sizeof(uint16_t));
    uint8_t* length = (uint8_t*)malloc(nsyms);
    uint32_t i;

    if (!table || !length) {
        free(table);
        free(length);
        return -1;
    }

    for (i = 0; i < nsyms; i++) {
        length[i] = 15;
    }

    i = (uint32_t)make_decode_table(nsyms, nbits, length, table);
    free(table);
    free(length);
    return (int)i;
}

struct lzx_bits {
    uint32_t bb;
    int bl;
    uint8_t* ip;
    uint8_t* end;
};

static int lzx_read_lens(struct LZXstate* pState, uint8_t* lens, uint32_t first, uint32_t last, struct lzx_bits* lb) {
    uint32_t i, j, x, y;
    int z;

    register uint32_t bitbuf = lb->bb;
    register int bitsleft = lb->bl;
    uint8_t* inpos = lb->ip;
    uint8_t* endinp = lb->end;
    uint16_t* hufftbl;

    for (x = 0; x < 20; x++) {
        READ_BITS(y, 4);
        LENTABLE(PRETREE)[x] = y;
    }
    BUILD_TABLE(PRETREE);

    for (x = first; x < last;) {
        READ_HUFFSYM(PRETREE, z);
        if (z == 17) {
            READ_BITS(y, 4);
            y += 4;
            if ((uint64_t)y > last - x) return 1;
            while (y--) lens[x++] = 0;
        } else if (z == 18) {
            READ_BITS(y, 5);
            y += 20;
            if ((uint64_t)y > last - x) return 1;
            while (y--) lens[x++] = 0;
        } else if (z == 19) {
            READ_BITS(y, 1);
            y += 4;
            if ((uint64_t)y > last - x) return 1;
            READ_HUFFSYM(PRETREE, z);
            z = lens[x] - z;
            if (z < 0) z += 17;
            while (y--) lens[x++] = z;
        } else {
            z = lens[x] - z;
            if (z < 0) z += 17;
            lens[x++] = z;
        }
    }

    lb->bb = bitbuf;
    lb->bl = bitsleft;
    lb->ip = inpos;
    return 0;
}

int LZXdecompress(struct LZXstate* pState, uint8_t* inpos, uint8_t* outpos, int inlen, int outlen) {
    uint8_t* endinp = inpos + inlen;
    uint8_t* window = pState->window;
    uint8_t *runsrc, *rundest;
    uint16_t* hufftbl;

    uint32_t window_posn = pState->window_posn;
    uint32_t window_size = pState->window_size;
    uint32_t R0 = pState->R0;
    uint32_t R1 = pState->R1;
    uint32_t R2 = pState->R2;

    register uint32_t bitbuf;
    register int bitsleft;
    uint32_t match_offset, i, j, k;
    struct lzx_bits lb;

    int togo = outlen, this_run, main_element, aligned_bits;
    int match_length, length_footer, extra, verbatim_bits;

    INIT_BITSTREAM;

    if (!pState->header_read) {
        i = j = 0;
        READ_BITS(k, 1);
        if (k) {
            READ_BITS(i, 16);
            READ_BITS(j, 16);
        }
        pState->intel_filesize = (i << 16) | j;
        pState->header_read = 1;
    }

    while (togo > 0) {

        if (pState->block_remaining == 0) {
            if (pState->block_type == LZX_BLOCKTYPE_UNCOMPRESSED) {
                if (pState->block_length & 1) inpos++;
                INIT_BITSTREAM;
            }

            READ_BITS(pState->block_type, 3);
            READ_BITS(i, 16);
            READ_BITS(j, 8);
            pState->block_remaining = pState->block_length = (i << 8) | j;

            switch (pState->block_type) {
                case LZX_BLOCKTYPE_ALIGNED:
                    for (i = 0; i < 8; i++) {
                        READ_BITS(j, 3);
                        LENTABLE(ALIGNED)[i] = j;
                    }
                    BUILD_TABLE(ALIGNED);

                case LZX_BLOCKTYPE_VERBATIM:
                    READ_LENGTHS(MAINTREE, 0, 256);
                    READ_LENGTHS(MAINTREE, 256, pState->main_elements);
                    BUILD_TABLE(MAINTREE);
                    if (LENTABLE(MAINTREE)[0xE8] != 0) pState->intel_started = 1;

                    READ_LENGTHS(LENGTH, 0, LZX_NUM_SECONDARY_LENGTHS);
                    BUILD_TABLE(LENGTH);
                    break;

                case LZX_BLOCKTYPE_UNCOMPRESSED:
                    pState->intel_started = 1;
                    ENSURE_BITS(16);
                    if (bitsleft > 16) inpos -= 2;
                    R0 = inpos[0] | (inpos[1] << 8) | (inpos[2] << 16) | (inpos[3] << 24);
                    inpos += 4;
                    R1 = inpos[0] | (inpos[1] << 8) | (inpos[2] << 16) | (inpos[3] << 24);
                    inpos += 4;
                    R2 = inpos[0] | (inpos[1] << 8) | (inpos[2] << 16) | (inpos[3] << 24);
                    inpos += 4;
                    break;

                default:
                    return DECR_ILLEGALDATA;
            }
        }

        if (inpos > endinp) {

            if (inpos > (endinp + 2) || bitsleft < 16) return DECR_ILLEGALDATA;
        }

        while ((this_run = pState->block_remaining) > 0 && togo > 0) {
            if (this_run > togo) this_run = togo;
            togo -= this_run;
            pState->block_remaining -= this_run;

            window_posn &= window_size - 1;

            if ((window_posn + this_run) > window_size) return DECR_DATAFORMAT;

            switch (pState->block_type) {
                case LZX_BLOCKTYPE_VERBATIM:
                case LZX_BLOCKTYPE_ALIGNED:
                    while (this_run > 0) {
                        READ_HUFFSYM(MAINTREE, main_element);

                        if (main_element < LZX_NUM_CHARS) {

                            window[window_posn++] = main_element;
                            this_run--;
                        } else {

                            main_element -= LZX_NUM_CHARS;

                            match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
                            if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
                                READ_HUFFSYM(LENGTH, length_footer);
                                match_length += length_footer;
                            }
                            match_length += LZX_MIN_MATCH;

                            match_offset = main_element >> 3;

                            if (match_offset > 2) {

                                if (pState->block_type == LZX_BLOCKTYPE_ALIGNED) {
                                    extra = extra_bits[match_offset];
                                    match_offset = position_base[match_offset] - 2;
                                    if (extra > 3) {

                                        extra -= 3;
                                        READ_BITS(verbatim_bits, extra);
                                        match_offset += (verbatim_bits << 3);
                                        READ_HUFFSYM(ALIGNED, aligned_bits);
                                        match_offset += aligned_bits;
                                    } else if (extra == 3) {

                                        READ_HUFFSYM(ALIGNED, aligned_bits);
                                        match_offset += aligned_bits;
                                    } else if (extra > 0) {

                                        READ_BITS(verbatim_bits, extra);
                                        match_offset += verbatim_bits;
                                    } else  {

                                        match_offset = 1;
                                    }
                                } else if (match_offset != 3) {
                                    extra = extra_bits[match_offset];
                                    READ_BITS(verbatim_bits, extra);
                                    match_offset = position_base[match_offset] - 2 + verbatim_bits;
                                } else {
                                    match_offset = 1;
                                }

                                R2 = R1;
                                R1 = R0;
                                R0 = match_offset;
                            } else if (match_offset == 0) {
                                match_offset = R0;
                            } else if (match_offset == 1) {
                                match_offset = R1;
                                R1 = R0;
                                R0 = match_offset;
                            } else  {
                                match_offset = R2;
                                R2 = R0;
                                R0 = match_offset;
                            }

                            rundest = window + window_posn;
                            runsrc = rundest - match_offset;
                            window_posn += match_length;
                            if (window_posn > window_size) return DECR_ILLEGALDATA;
                            this_run -= match_length;

                            while ((runsrc < window) && (match_length-- > 0)) {
                                *rundest++ = window[window_size - (size_t)(window - runsrc)];
                                runsrc++;
                            }

                            while (match_length-- > 0) *rundest++ = *runsrc++;
                        }
                    }
                    break;

                case LZX_BLOCKTYPE_UNCOMPRESSED:
                    if ((inpos + this_run) > endinp) return DECR_ILLEGALDATA;
                    memcpy(window + window_posn, inpos, (size_t)this_run);
                    inpos += this_run;
                    window_posn += this_run;
                    break;

                default:
                    return DECR_ILLEGALDATA;
            }
        }
    }

    if (togo != 0) return DECR_ILLEGALDATA;
    memcpy(outpos, window + ((!window_posn) ? window_size : window_posn) - outlen, (size_t)outlen);

    pState->window_posn = window_posn;
    pState->R0 = R0;
    pState->R1 = R1;
    pState->R2 = R2;

    if ((pState->frames_read++ < 32768) && pState->intel_filesize != 0) {
        if (outlen <= 6 || !pState->intel_started) {
            pState->intel_curpos += outlen;
        } else {
            uint8_t* data = outpos;
            uint8_t* dataend = data + outlen - 10;
            int32_t curpos = pState->intel_curpos;
            int32_t filesize = pState->intel_filesize;
            int32_t abs_off, rel_off;

            pState->intel_curpos = curpos + outlen;

            while (data < dataend) {
                if (*data++ != 0xE8) {
                    curpos++;
                    continue;
                }
                abs_off = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
                if ((abs_off >= -curpos) && (abs_off < filesize)) {
                    rel_off = (abs_off >= 0) ? abs_off - curpos : abs_off + filesize;
                    data[0] = (uint8_t)rel_off;
                    data[1] = (uint8_t)(rel_off >> 8);
                    data[2] = (uint8_t)(rel_off >> 16);
                    data[3] = (uint8_t)(rel_off >> 24);
                }
                data += 4;
                curpos += 5;
            }
        }
    }
    return DECR_OK;
}

#ifdef LZX_CHM_TESTDRIVER
int main(int c, char** v) {
    FILE *fin, *fout;
    struct LZXstate state;
    uint8_t ibuf[16384];
    uint8_t obuf[32768];
    int ilen, olen;
    int status;
    int i;
    int count = 0;
    int w = atoi(v[1]);
    LZXinit(&state, w);
    fout = fopen(v[2], "wb");
    for (i = 3; i < c; i++) {
        fin = fopen(v[i], "rb");
        ilen = fread(ibuf, 1, 16384, fin);
        status = LZXdecompress(&state, ibuf, obuf, ilen, 32768);
        switch (status) {
            case DECR_OK:
                printf("ok\n");
                fwrite(obuf, 1, 32768, fout);
                break;
            case DECR_DATAFORMAT:
                printf("bad format\n");
                break;
            case DECR_ILLEGALDATA:
                printf("illegal data\n");
                break;
            case DECR_NOMEMORY:
                printf("no memory\n");
                break;
            default:
                break;
        }
        fclose(fin);
        if (++count == 2) {
            count = 0;
            LZXreset(&state);
        }
    }
    fclose(fout);
}
#endif

static const char _CHMU_RESET_TABLE[] =
    "::DataSpace/Storage/MSCompressed/Transform/"
    "{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/"
    "InstanceData/ResetTable";
static const char _CHMU_LZXC_CONTROLDATA[] = "::DataSpace/Storage/MSCompressed/ControlData";
static const char _CHMU_CONTENT[] = "::DataSpace/Storage/MSCompressed/Content";
#if 0
static const char _CHMU_SPANINFO[] = "::DataSpace/Storage/MSCompressed/SpanInfo";
#endif

static int read_char_array(uint8_t **pData, unsigned int *pLenRemain, char *dest, int count)
{
    if (count <= 0 || (unsigned int)count > *pLenRemain) return 0;
    memcpy(dest, *pData, (size_t)count);
    *pData += count;
    *pLenRemain -= (unsigned int)count;
    return 1;
}

static int read_uchar_array(uint8_t **pData, unsigned int *pLenRemain, uint8_t *dest, int count)
{
    if (count <= 0 || (unsigned int)count > *pLenRemain) return 0;
    memcpy(dest, *pData, (size_t)count);
    *pData += count;
    *pLenRemain -= (unsigned int)count;
    return 1;
}

static int read_i32(uint8_t **pData, unsigned int *pLenRemain, int32_t *dest)
{
    if (4 > *pLenRemain) return 0;
    *dest = (*pData)[0] | ((*pData)[1] << 8) | ((*pData)[2] << 16) | ((*pData)[3] << 24);
    *pData += 4;
    *pLenRemain -= 4;
    return 1;
}

static int read_u32(uint8_t **pData, unsigned int *pLenRemain, uint32_t *dest)
{
    if (4 > *pLenRemain) return 0;
    *dest = (*pData)[0] | ((*pData)[1] << 8) | ((*pData)[2] << 16) | ((*pData)[3] << 24);
    *pData += 4;
    *pLenRemain -= 4;
    return 1;
}

static int read_i64(uint8_t **pData, unsigned int *pLenRemain, int64_t *dest)
{
    int64_t temp = 0;
    if (8 > *pLenRemain) return 0;
    for (int i = 8; i > 0; i--) {
        temp <<= 8;
        temp |= (*pData)[i - 1];
    }
    *dest = temp;
    *pData += 8;
    *pLenRemain -= 8;
    return 1;
}

static int read_u64(uint8_t **pData, unsigned int *pLenRemain, uint64_t *dest)
{
    uint64_t temp = 0;
    if (8 > *pLenRemain) return 0;
    for (int i = 8; i > 0; i--) {
        temp <<= 8;
        temp |= (*pData)[i - 1];
    }
    *dest = temp;
    *pData += 8;
    *pLenRemain -= 8;
    return 1;
}

static int read_uuid(uint8_t **pData, unsigned int *pDataLen, uint8_t *dest)
{
    return read_uchar_array(pData, pDataLen, dest, 16);
}

static int read_itsf_header(uint8_t **pData, unsigned int *pDataLen, struct chmItsfHeader *dest)
{
    if (*pDataLen != _CHM_ITSF_V2_LEN && *pDataLen != _CHM_ITSF_V3_LEN) return 0;

    read_char_array(pData, pDataLen, dest->signature, 4);
    read_i32(pData, pDataLen, &dest->version);
    read_i32(pData, pDataLen, &dest->header_len);
    read_i32(pData, pDataLen, &dest->unknown_000c);
    read_u32(pData, pDataLen, &dest->last_modified);
    read_u32(pData, pDataLen, &dest->lang_id);
    read_uuid(pData, pDataLen, dest->dir_uuid);
    read_uuid(pData, pDataLen, dest->stream_uuid);
    read_u64(pData, pDataLen, &dest->unknown_offset);
    read_u64(pData, pDataLen, &dest->unknown_len);
    read_u64(pData, pDataLen, &dest->dir_offset);
    read_u64(pData, pDataLen, &dest->dir_len);

    if (memcmp(dest->signature, "ITSF", 4) != 0) return 0;
    if (dest->version == 2) {
        if (dest->header_len < _CHM_ITSF_V2_LEN) return 0;
    } else if (dest->version == 3) {
        if (dest->header_len < _CHM_ITSF_V3_LEN) return 0;
    } else {
        return 0;
    }

    if (dest->version == 3) {
        if (*pDataLen != 0)
            read_u64(pData, pDataLen, &dest->data_offset);
        else
            return 0;
    } else {
        dest->data_offset = dest->dir_offset + dest->dir_len;
    }
    if (dest->dir_offset > UINT_MAX || dest->dir_len > UINT_MAX) return 0;
    return 1;
}

static int read_itsp_header(uint8_t **pData, unsigned int *pDataLen, struct chmItspHeader *dest)
{
    if (*pDataLen != _CHM_ITSP_V1_LEN) return 0;

    read_char_array(pData, pDataLen, dest->signature, 4);
    read_i32(pData, pDataLen, &dest->version);
    read_i32(pData, pDataLen, &dest->header_len);
    read_i32(pData, pDataLen, &dest->unknown_000c);
    read_u32(pData, pDataLen, &dest->block_len);
    read_i32(pData, pDataLen, &dest->blockidx_intvl);
    read_i32(pData, pDataLen, &dest->index_depth);
    read_i32(pData, pDataLen, &dest->index_root);
    read_i32(pData, pDataLen, &dest->index_head);
    read_i32(pData, pDataLen, &dest->unknown_0024);
    read_u32(pData, pDataLen, &dest->num_blocks);
    read_i32(pData, pDataLen, &dest->unknown_002c);
    read_u32(pData, pDataLen, &dest->lang_id);
    read_uuid(pData, pDataLen, dest->system_uuid);
    read_uchar_array(pData, pDataLen, dest->unknown_0044, 16);

    if (memcmp(dest->signature, "ITSP", 4) != 0) return 0;
    if (dest->version != 1) return 0;
    if (dest->header_len != _CHM_ITSP_V1_LEN) return 0;

    if (dest->block_len < _CHM_PMGL_LEN) return 0;

    if (dest->block_len > 2097152) return 0;
    return 1;
}

static int read_pmgi_header(uint8_t **pData, unsigned int *pDataLen, unsigned int blockLen,
                                  struct chmPmgiHeader *dest)
{
    if (*pDataLen != _CHM_PMGI_LEN) return 0;
    if (blockLen < _CHM_PMGI_LEN) return 0;

    read_char_array(pData, pDataLen, dest->signature, 4);
    read_u32(pData, pDataLen, &dest->free_space);

    if (memcmp(dest->signature, _chm_pmgi_marker, 4) != 0) return 0;
    if (dest->free_space > blockLen - _CHM_PMGI_LEN) return 0;
    return 1;
}

static int read_lzxc_reset_table(uint8_t **pData, unsigned int *pDataLen, struct chmLzxcResetTable *dest)
{
    if (*pDataLen != _CHM_LZXC_RESETTABLE_V1_LEN) return 0;

    read_u32(pData, pDataLen, &dest->version);
    read_u32(pData, pDataLen, &dest->block_count);
    read_u32(pData, pDataLen, &dest->unknown);
    read_u32(pData, pDataLen, &dest->table_offset);
    read_u64(pData, pDataLen, &dest->uncompressed_len);
    read_u64(pData, pDataLen, &dest->compressed_len);
    read_u64(pData, pDataLen, &dest->block_len);

    if (dest->version != 2) return 0;
    if (dest->block_count == 0) return 0;
    if (dest->uncompressed_len > INT_MAX || dest->compressed_len > INT_MAX) return 0;

    if (dest->block_len == 0 || dest->block_len > 2097152) return 0;
    return 1;
}

static void *default_alloc(void *user, void *ctx, size_t size)
{
    (void)user;
    (void)ctx;
    return malloc(size);
}

static void default_free(void *user, void *ctx, void *ptr)
{
    (void)user;
    (void)ctx;
    free(ptr);
}

void *chm_alloc(chm_ctx *ctx, size_t size)
{
    chm_alloc_cb a = (ctx && ctx->alloc) ? ctx->alloc : default_alloc;
    void *u = ctx ? ctx->user : NULL;
    return a(u, ctx, size);
}

void chm_free(chm_ctx *ctx, void *ptr)
{
    if (!ptr) return;
    chm_free_cb f = (ctx && ctx->free) ? ctx->free : default_free;
    void *u = ctx ? ctx->user : NULL;
    f(u, ctx, ptr);
}

void chm_errorf(chm_ctx *ctx, int sev, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    if (!ctx || !ctx->error) return;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ctx->error(ctx->user, sev, buf);
}

chm_ctx *chm_ctx_new(chm_alloc_cb alloc, chm_free_cb free_cb,
                     chm_error_cb error, void *user)
{
    chm_ctx *ctx;
    chm_alloc_cb a = alloc ? alloc : default_alloc;
    ctx = (chm_ctx *)a(user, NULL, sizeof(chm_ctx));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->alloc = a;
    ctx->free = free_cb ? free_cb : default_free;
    ctx->error = error;
    ctx->user = user;
    return ctx;
}

void chm_ctx_free(chm_ctx *ctx)
{
    if (ctx) {
        chm_close(ctx);
        ctx->free(ctx->user, NULL, ctx);
    }
}

static int read_pmgl_header(uint8_t **pData, unsigned int *pDataLen, unsigned int blockLen,
                                  struct chmPmglHeader *dest)
{
    if (*pDataLen != _CHM_PMGL_LEN) return 0;
    if (blockLen < _CHM_PMGL_LEN) return 0;

    read_char_array(pData, pDataLen, dest->signature, 4);
    read_u32(pData, pDataLen, &dest->free_space);
    read_u32(pData, pDataLen, &dest->unknown_0008);
    read_i32(pData, pDataLen, &dest->block_prev);
    read_i32(pData, pDataLen, &dest->block_next);

    if (memcmp(dest->signature, _chm_pmgl_marker, 4) != 0) return 0;
    if (dest->free_space > blockLen - _CHM_PMGL_LEN) return 0;
    return 1;
}

static int read_lzxc_control_data(uint8_t **pData, unsigned int *pDataLen, struct chmLzxcControlData *dest)
{
    if (*pDataLen < _CHM_LZXC_MIN_LEN) return 0;

    read_u32(pData, pDataLen, &dest->size);
    read_char_array(pData, pDataLen, dest->signature, 4);
    read_u32(pData, pDataLen, &dest->version);
    read_u32(pData, pDataLen, &dest->resetInterval);
    read_u32(pData, pDataLen, &dest->windowSize);
    read_u32(pData, pDataLen, &dest->windowsPerReset);

    if (*pDataLen >= _CHM_LZXC_V2_LEN)
        read_u32(pData, pDataLen, &dest->unknown_18);
    else
        dest->unknown_18 = 0;

    if (dest->version == 2) {
        dest->resetInterval *= 0x8000;
        dest->windowSize *= 0x8000;
    }
    if (dest->windowSize == 0 || dest->resetInterval == 0) return 0;

    if (dest->windowSize < 32768 || dest->windowSize > 2097152) return 0;
    if ((dest->windowSize & (dest->windowSize - 1)) != 0) return 0;
    if ((dest->resetInterval % (dest->windowSize / 2)) != 0) return 0;

    if (memcmp(dest->signature, "LZXC", 4) != 0) return 0;
    return 1;
}

static int64_t fetch_bytes(chm_ctx *ctx, uint8_t* buf, uint64_t offset, int64_t len) {
    if (len <= 0) {
        return 0;
    }
    if (offset > ctx->data_len) {
        return 0;
    }
    if ((uint64_t)len > ctx->data_len - offset) {
        return 0;
    }
    memcpy(buf, ctx->data + offset, (size_t)len);
    return len;
}

static int add_u64(uint64_t a, uint64_t b, uint64_t* result) {
    if (a > UINT64_MAX - b) return 0;
    *result = a + b;
    return 1;
}

static int get_entry_offset(chm_ctx *ctx, const struct chm_entry* entry, uint64_t addr, int64_t len,
                                  uint64_t* offset) {
    uint64_t temp;

    if (len <= 0) return 0;
    if (addr > entry->length) return 0;
    if ((uint64_t)len > entry->length - addr) return 0;
    if (!add_u64((uint64_t)entry->start, addr, &temp)) return 0;
    if (!add_u64(ctx->data_offset, temp, offset)) return 0;
    return 1;
}

static uint64_t dir_page_count(chm_ctx *ctx) {
    if (ctx->block_len == 0) return 0;
    return ctx->dir_len / ctx->block_len;
}

static int is_valid_dir_page(chm_ctx *ctx, int32_t page) {
    uint64_t page_count = dir_page_count(ctx);
    if (page < 0) return 0;
    if (page_count == 0) return 0;
    return (uint64_t)page < page_count;
}

static int dir_page_offset(chm_ctx *ctx, int32_t page, uint64_t* offset) {
    uint64_t page_off;

    if (!is_valid_dir_page(ctx, page)) return 0;
    if ((uint64_t)page > UINT64_MAX / ctx->block_len) return 0;
    page_off = (uint64_t)page * ctx->block_len;
    if (!add_u64(ctx->dir_offset, page_off, offset)) return 0;
    if (*offset > ctx->data_len || (uint64_t)ctx->block_len > ctx->data_len - *offset) return 0;
    return 1;
}

static int dir_fetch_page(chm_ctx *ctx, int32_t page, uint8_t* page_buf) {
    uint64_t offset;

    if (!dir_page_offset(ctx, page, &offset)) return 0;
    return fetch_bytes(ctx, page_buf, offset, ctx->block_len) == ctx->block_len;
}

static void dir_visit_reset(chm_ctx *ctx) {
    ctx->dir_pages_seen = 0;
    memset(ctx->dir_seen_bitmap, 0, sizeof(ctx->dir_seen_bitmap));
}

static int dir_visit_begin(chm_ctx *ctx) {
    if (ctx->dir_page_count == 0) return 0;
    dir_visit_reset(ctx);
    return 1;
}

static int dir_visit_page(chm_ctx *ctx, int32_t page) {
    uint32_t word_idx;
    uint32_t bit_mask;

    if (page < 0) return 0;
    if ((uint64_t)page >= ctx->dir_page_count) return 0;
    if (ctx->dir_pages_seen >= ctx->dir_page_count) return 0;
    if ((uint32_t)page >= CHM_DIR_SEEN_BITMAP_BITS) return 0;

    word_idx = (uint32_t)page >> 5;
    bit_mask = 1u << (page & 31);
    if (ctx->dir_seen_bitmap[word_idx] & bit_mask) return 0;
    ctx->dir_seen_bitmap[word_idx] |= bit_mask;
    ctx->dir_pages_seen++;
    return 1;
}

static int dir_session_begin(chm_ctx *ctx, struct chmDirSession *s)
{
    s->ctx = ctx;
    s->page_buf = NULL;
    s->page_buf_end = NULL;
    if (!dir_visit_begin(ctx)) return 0;
    s->page_buf = (uint8_t *)chm_alloc(ctx, (size_t)ctx->block_len);
    if (!s->page_buf) return 0;
    s->page_buf_end = s->page_buf + ctx->block_len;
    return 1;
}

static void dir_session_end(struct chmDirSession *s)
{
    chm_free(s->ctx, s->page_buf);
    s->page_buf = NULL;
    s->page_buf_end = NULL;
}

static int dir_session_fetch(struct chmDirSession* s, int32_t page) {
    if (!dir_visit_page(s->ctx, page)) return 0;
    return dir_fetch_page(s->ctx, page, s->page_buf);
}

static void skip_cword(uint8_t** pEntry, uint8_t* end) {
    while ((*pEntry < end) && *(*pEntry)++ >= 0x80);
}

static void skip_PMGL_entry_data(uint8_t** pEntry, uint8_t* end) {
    skip_cword(pEntry, end);
    skip_cword(pEntry, end);
    skip_cword(pEntry, end);
}

static int parse_cword(uint8_t** pEntry, uint8_t* end, uint64_t* result) {
    uint64_t accum = 0;
    uint8_t temp = 0;

    while (*pEntry < end) {
        temp = *(*pEntry)++;
        if (temp < 0x80) {
            if (accum > (UINT64_MAX - temp) >> 7) return 0;
            *result = (accum << 7) + temp;
            return 1;
        }
        if (accum > (UINT64_MAX - (temp & 0x7f)) >> 7) return 0;
        accum <<= 7;
        accum += temp & 0x7f;
    }

    return 0;
}

static int parse_UTF8(uint8_t** pEntry, uint8_t* end, uint64_t count, char* path) {

    if (*pEntry > end) return 0;
    if ((uint64_t)(end - *pEntry) < count) return 0;
    memcpy(path, *pEntry, (size_t)count);
    path += count;
    *pEntry += count;

    *path = '\0';
    return 1;
}

static int parse_PMGL_entry(chm_ctx *ctx, uint8_t** pEntry, uint8_t* end, struct chm_entry* entry) {
    uint64_t strLen;

    if (!parse_cword(pEntry, end, &strLen)) return 0;
    if (strLen == 0) return 0;

    if ((uint64_t)(end - *pEntry) < strLen) return 0;

    entry->path = (char *)chm_alloc(ctx, (size_t)strLen + 1);
    if (!entry->path) return 0;
    if (!parse_UTF8(pEntry, end, strLen, entry->path)) {
        chm_free(ctx, entry->path);
        entry->path = NULL;
        return 0;
    }

    if (!parse_cword(pEntry, end, &strLen)) return 0;
    entry->space = (uint32_t)strLen;
    entry->is_compressed = (strLen == CHM_COMPRESSED);
    if (!parse_cword(pEntry, end, &entry->start)) return 0;
    if (!parse_cword(pEntry, end, &entry->length)) return 0;
    return 1;
}

static int collect_entries(chm_ctx *ctx) {
    struct chmDirSession session;
    struct chmPmglHeader header;
    int count = 0;
    uint8_t *cur, *end;
    unsigned int remain;

    if (!ctx || ctx->entry_count > 0) return ctx->entry_count;

    if (!dir_session_begin(ctx, &session)) return 0;

    int32_t page = ctx->index_head;
    while (page != -1) {
        if (!dir_session_fetch(&session, page)) break;
        if (memcmp(session.page_buf, _chm_pmgl_marker, 4) == 0) {
            cur = session.page_buf;
            remain = _CHM_PMGL_LEN;
            if (read_pmgl_header(&cur, &remain, ctx->block_len, &header)) {
                end = session.page_buf + ctx->block_len - (header.free_space);
                while (cur < end) {
                    uint64_t nlen;
                    if (!parse_cword(&cur, end, &nlen)) break;

                    if ((uint64_t)(end - cur) < nlen) break;
                    cur += nlen;
                    skip_PMGL_entry_data(&cur, end);
                    count++;
                }
                page = header.block_next;
                continue;
            }
        }

        page = -1;
    }

    if (count == 0) {
        dir_session_end(&session);
        return 0;
    }

    ctx->entries = (struct chm_entry *)chm_alloc(ctx, sizeof(struct chm_entry) * count);
    ctx->entry_ptrs = (struct chm_entry **)chm_alloc(ctx, sizeof(struct chm_entry *) * count);
    if (!ctx->entries || !ctx->entry_ptrs) {
        chm_free(ctx, ctx->entries);
        chm_free(ctx, ctx->entry_ptrs);
        ctx->entries = NULL;
        ctx->entry_ptrs = NULL;
        dir_session_end(&session);
        return 0;
    }

    dir_visit_reset(ctx);

    int idx = 0;
    page = ctx->index_head;
    while (page != -1 && idx < count) {
        if (!dir_session_fetch(&session, page)) break;
        if (memcmp(session.page_buf, _chm_pmgl_marker, 4) != 0) {
            page = -1; continue;
        }
        cur = session.page_buf;
        remain = _CHM_PMGL_LEN;
        if (!read_pmgl_header(&cur, &remain, ctx->block_len, &header)) break;
        end = session.page_buf + ctx->block_len - (header.free_space);

        while (cur < end && idx < count) {
            struct chm_entry *entry = &ctx->entries[idx];
            memset(entry, 0, sizeof(*entry));

            if (!parse_PMGL_entry(ctx, &cur, end, entry)) {

                break;
            }

            size_t plen = strlen(entry->path);
            if (plen > 0) {
                if (entry->path[plen-1] == '/') {
                    entry->is_dir = true;
                } else {
                    entry->is_file = true;
                }
                if (entry->path[0] == '/') {
                    if (entry->path[1] == '#' || entry->path[1] == '$') {
                        entry->is_special = true;
                    } else {
                        entry->is_normal = true;
                    }
                } else {
                    entry->is_meta = true;
                }
            }

            ctx->entry_ptrs[idx] = entry;
            idx++;
        }
        page = header.block_next;
    }

    ctx->entry_count = idx;
    dir_session_end(&session);
    return ctx->entry_count;
}

int chm_get_entries(chm_ctx *ctx, struct chm_entry ***outEntries) {
    if (!ctx || !ctx->data || ctx->entry_count <= 0) {
        if (outEntries) *outEntries = NULL;
        return 0;
    }
    if (outEntries) *outEntries = ctx->entry_ptrs;
    return ctx->entry_count;
}

static uint8_t* find_in_PMGL(uint8_t* page_buf, uint32_t block_len, const char* objPath) {

    struct chmPmglHeader header;
    unsigned int hremain;
    uint8_t* end;
    uint8_t* cur;
    uint8_t* temp;
    uint64_t strLen;

    cur = page_buf;
    hremain = _CHM_PMGL_LEN;
    if (!read_pmgl_header(&cur, &hremain, block_len, &header)) return NULL;
    end = page_buf + block_len - (header.free_space);

    while (cur < end) {

        temp = cur;
        if (!parse_cword(&cur, end, &strLen)) return NULL;
        if (strLen == 0) return NULL;

        if ((uint64_t)(end - cur) < strLen) return NULL;

        if (strLen == strlen(objPath) &&
            strncasecmp((const char*)cur, objPath, (size_t)strLen) == 0) {
            return temp;
        }

        cur += strLen;
        skip_PMGL_entry_data(&cur, end);
    }

    return NULL;
}

static int cmp_counted_ci(const char* name, size_t len, const char* objPath) {
    for (size_t i = 0; i < len; i++) {
        unsigned char a = (unsigned char)name[i];
        unsigned char b = (unsigned char)objPath[i];
        if (b == '\0') return 1;
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
        if (a != b) return a < b ? -1 : 1;
    }

    return objPath[len] == '\0' ? 0 : -1;
}

static int32_t find_in_PMGI(uint8_t* page_buf, uint32_t block_len, const char* objPath) {

    struct chmPmgiHeader header;
    unsigned int hremain;
    int page = -1;
    uint8_t* end;
    uint8_t* cur;
    uint64_t strLen;

    cur = page_buf;
    hremain = _CHM_PMGI_LEN;
    if (!read_pmgi_header(&cur, &hremain, block_len, &header)) return -1;
    end = page_buf + block_len - (header.free_space);

    while (cur < end) {

        if (!parse_cword(&cur, end, &strLen)) return -1;
        if (strLen == 0) return -1;

        if ((uint64_t)(end - cur) < strLen) return -1;

        if (cmp_counted_ci((const char*)cur, (size_t)strLen, objPath) > 0) return page;

        cur += strLen;

        if (!parse_cword(&cur, end, &strLen) || strLen > INT_MAX) return -1;
        page = (int)strLen;
    }

    return page;
}

static bool chm_resolve_entry(chm_ctx *ctx, const char* objPath, struct chm_entry* entry) {

    struct chmDirSession session;
    int32_t curPage;

    if (!dir_session_begin(ctx, &session)) return false;

    curPage = ctx->index_root;
    while (curPage != -1) {
        int32_t new_page;
        uint8_t* pEntry;

        if (!dir_session_fetch(&session, curPage)) goto cleanup;

        if (ctx->block_len < 4) goto cleanup;

        if (memcmp(session.page_buf, _chm_pmgl_marker, 4) == 0) {
            pEntry = find_in_PMGL(session.page_buf, ctx->block_len, objPath);
            if (pEntry == NULL) goto cleanup;
            if (!parse_PMGL_entry(ctx, &pEntry, session.page_buf_end, entry)) goto cleanup;
            dir_session_end(&session);
            return true;
        } else if (memcmp(session.page_buf, _chm_pmgi_marker, 4) == 0) {
            new_page = find_in_PMGI(session.page_buf, ctx->block_len, objPath);
            curPage = new_page;
        } else {
            goto cleanup;
        }
    }

cleanup:
    dir_session_end(&session);
    return false;
}

static int get_cmpblock_bounds(chm_ctx *ctx, uint64_t block, uint64_t* start, int64_t* len) {
    uint8_t buffer[8], *dummy;
    unsigned int remain;
    uint64_t table_offset;
    uint64_t table_addr;
    uint64_t block_entry_offset;
    uint64_t abs_start;

    if (block > UINT64_MAX / 8) return 0;
    block_entry_offset = block * 8;

    if (block < ctx->reset_table.block_count - 1) {

        dummy = buffer;
        remain = 8;
        if (!add_u64((uint64_t)ctx->reset_table.table_offset, block_entry_offset, &table_addr)) return 0;
        if (!get_entry_offset(ctx, &ctx->rt_entry, table_addr, remain, &table_offset) ||
            fetch_bytes(ctx, buffer, table_offset, remain) != remain || !read_u64(&dummy, &remain, start))
            return 0;

        dummy = buffer;
        remain = 8;
        if (!add_u64(table_addr, 8, &table_addr)) return 0;
        if (!get_entry_offset(ctx, &ctx->rt_entry, table_addr, remain, &table_offset) ||
            fetch_bytes(ctx, buffer, table_offset, remain) != remain || !read_i64(&dummy, &remain, len))
            return 0;
    }

    else {

        dummy = buffer;
        remain = 8;
        if (!add_u64((uint64_t)ctx->reset_table.table_offset, block_entry_offset, &table_addr)) return 0;
        if (!get_entry_offset(ctx, &ctx->rt_entry, table_addr, remain, &table_offset) ||
            fetch_bytes(ctx, buffer, table_offset, remain) != remain || !read_u64(&dummy, &remain, start))
            return 0;

        *len = ctx->reset_table.compressed_len;
    }

    if (*start > (uint64_t)*len) {
        return 0;
    }
    *len -= *start;
    if (!get_entry_offset(ctx, &ctx->cn_entry, *start, *len, &abs_start)) return 0;
    *start = abs_start;

    return 1;
}

static int64_t decompress_block(chm_ctx *ctx, uint64_t block, uint8_t** ubuffer) {
    uint8_t* cbuffer;
    uint64_t cbufferLen;
    uint64_t cmpStart;
    int64_t cmpLen;
    int indexSlot;
    uint8_t* lbuffer;
    uint32_t blockAlign = (uint32_t)(block % ctx->reset_blkcount);
    uint32_t i;
    int ok;

    cbufferLen = ctx->reset_table.block_len + 6144;
    cbuffer = (uint8_t *)chm_alloc(ctx, (size_t)cbufferLen);
    if (!cbuffer) return -1;

    if (block - blockAlign <= ctx->lzx_last_block && block >= ctx->lzx_last_block) blockAlign = (block - ctx->lzx_last_block);

    if (blockAlign != 0) {

        for (i = blockAlign; i > 0; i--) {
            uint32_t curBlockIdx = block - i;

            if (ctx->lzx_last_block != (int)curBlockIdx) {
                if ((curBlockIdx % ctx->reset_blkcount) == 0) {
                    LZXreset(ctx->lzx_state);
                }

                indexSlot = (int)(curBlockIdx % ctx->cache_num_blocks);
                if (!ctx->cache_blocks[indexSlot]) {
                    ctx->cache_blocks[indexSlot] = (uint8_t *)chm_alloc(ctx, (size_t)ctx->reset_table.block_len);
                    if (!ctx->cache_blocks[indexSlot]) {
                        chm_free(ctx, cbuffer);
                        return -1;
                    }

                    memset(ctx->cache_blocks[indexSlot], 0, (size_t)ctx->reset_table.block_len);
                }
                ctx->cache_block_indices[indexSlot] = curBlockIdx;
                lbuffer = ctx->cache_blocks[indexSlot];

                if (!get_cmpblock_bounds(ctx, curBlockIdx, &cmpStart, &cmpLen) || cmpLen < 0 ||
                    cmpLen > cbufferLen || fetch_bytes(ctx, cbuffer, cmpStart, cmpLen) != cmpLen ||
                    LZXdecompress(ctx->lzx_state, cbuffer, lbuffer, (int)cmpLen, (int)ctx->reset_table.block_len) != DECR_OK) {
                    chm_free(ctx, cbuffer);
                    return (int64_t)0;
                }

                ctx->lzx_last_block = (int)curBlockIdx;
            }
        }
    } else {
        if ((block % ctx->reset_blkcount) == 0) {
            LZXreset(ctx->lzx_state);
        }
    }

    if (ctx->cache_num_blocks == 0) {
        chm_free(ctx, cbuffer);
        return -1;
    }

    indexSlot = (int)(block % ctx->cache_num_blocks);
    if (!ctx->cache_blocks[indexSlot]) {
        ctx->cache_blocks[indexSlot] = (uint8_t *)chm_alloc(ctx, (size_t)ctx->reset_table.block_len);
        if (!ctx->cache_blocks[indexSlot]) {
            chm_free(ctx, cbuffer);
            return -1;
        }

        memset(ctx->cache_blocks[indexSlot], 0, (size_t)ctx->reset_table.block_len);
    }
    ctx->cache_block_indices[indexSlot] = block;
    lbuffer = ctx->cache_blocks[indexSlot];
    *ubuffer = lbuffer;

    ok = get_cmpblock_bounds(ctx, block, &cmpStart, &cmpLen);
    if (!ok || cmpLen > cbufferLen || fetch_bytes(ctx, cbuffer, cmpStart, cmpLen) != cmpLen ||
        LZXdecompress(ctx->lzx_state, cbuffer, lbuffer, (int)cmpLen, (int)ctx->reset_table.block_len) != DECR_OK) {
        chm_free(ctx, cbuffer);
        return (int64_t)0;
    }
    ctx->lzx_last_block = (int)block;

    chm_free(ctx, cbuffer);
    return ctx->reset_table.block_len;
}

static int64_t decompress_region(chm_ctx *ctx, uint8_t* buf, uint64_t start, int64_t len) {
    uint64_t nBlock, nOffset;
    uint64_t nLen;
    uint64_t gotLen;
    uint8_t* ubuffer = NULL;

    if (len <= 0) return (int64_t)0;

    if (ctx->reset_table.block_len == 0) {
        return (int64_t)0;
    }

    nBlock = start / ctx->reset_table.block_len;
    nOffset = start % ctx->reset_table.block_len;
    nLen = len;
    if (nLen > (ctx->reset_table.block_len - nOffset)) nLen = ctx->reset_table.block_len - nOffset;

    if (ctx->cache_num_blocks > 0) {
        if (ctx->cache_block_indices[nBlock % ctx->cache_num_blocks] == (int64_t)nBlock &&
            ctx->cache_blocks[nBlock % ctx->cache_num_blocks] != NULL) {
            memcpy(buf, ctx->cache_blocks[nBlock % ctx->cache_num_blocks] + nOffset, (unsigned int)nLen);
            return nLen;
        }
    }

    if (!ctx->lzx_state) {

        int window_size = 0;
        {
            uint32_t w = ctx->window_size;
            while (w > 1) {
                w >>= 1;
                window_size++;
            }
        }
        ctx->lzx_last_block = -1;
        ctx->lzx_state = LZXinit(window_size);
    }

    if (ctx->reset_blkcount == 0) {
        return (int64_t)0;
    }

    gotLen = decompress_block(ctx, nBlock, &ubuffer);

    if (gotLen == (uint64_t)-1 || ubuffer == NULL) {
        return 0;
    }
    if (gotLen < nLen) nLen = gotLen;
    memcpy(buf, ubuffer + nOffset, (unsigned int)nLen);
    return nLen;
}

static int64_t read_entry_range(chm_ctx *ctx, struct chm_entry* entry, uint8_t* buf, uint64_t addr, int64_t len) {
    uint64_t offset;

    if (ctx == NULL || entry == NULL || buf == NULL) return (int64_t)0;
    if (len <= 0) return (int64_t)0;

    if (addr >= entry->length) return (int64_t)0;

    if ((uint64_t)len > entry->length - addr) len = (int64_t)(entry->length - addr);

    if (entry->space == CHM_UNCOMPRESSED) {

        if (!get_entry_offset(ctx, entry, addr, len, &offset)) return (int64_t)0;
        return fetch_bytes(ctx, buf, offset, len);
    }

    else
    {
        int64_t swath = 0, total = 0;

        if (!ctx->compression_enabled) return total;

        do {
            if (!add_u64(entry->start, addr, &offset)) return total;

            swath = decompress_region(ctx, buf, offset, len);

            if (swath == 0) return total;

            total += swath;
            len -= swath;
            addr += swath;
            buf += swath;

        } while (len != 0);

        return total;
    }
}

bool chm_open(chm_ctx *ctx, const uint8_t *data, size_t len)
{
    uint8_t sbuffer[256];
    unsigned int sremain;
    uint8_t *sbufpos;
    struct chmItsfHeader itsfHeader = {0};
    struct chmItspHeader itspHeader = {0};
#if 0
    struct chm_entry uiSpan;
#endif
    struct chm_entry uiLzxc = {0};
    struct chmLzxcControlData ctlData;
    int ok;

    if (!ctx) return false;

    chm_close(ctx);

    ctx->data = data;
    ctx->data_len = len;

    sremain = _CHM_ITSF_V3_LEN;
    sbufpos = sbuffer;
    ok = fetch_bytes(ctx, sbuffer, (uint64_t)0, sremain) == sremain;
    if (ok) {
        ok = read_itsf_header(&sbufpos, &sremain, &itsfHeader);
    }
    if (!ok) {
        chm_close(ctx);
        return false;
    }

    ctx->dir_offset = itsfHeader.dir_offset;
    ctx->dir_len = itsfHeader.dir_len;
    ctx->data_offset = itsfHeader.data_offset;

    sremain = _CHM_ITSP_V1_LEN;
    sbufpos = sbuffer;
    ok = fetch_bytes(ctx, sbuffer, (uint64_t)itsfHeader.dir_offset, sremain) == sremain;
    if (ok) {
        ok = read_itsp_header(&sbufpos, &sremain, &itspHeader);
    }
    if (!ok) {
        chm_close(ctx);
        return false;
    }
    if (itsfHeader.dir_len < (uint64_t)itspHeader.header_len) {
        chm_close(ctx);
        return false;
    }

    ctx->dir_offset += itspHeader.header_len;
    ctx->dir_len -= itspHeader.header_len;
    ctx->index_root = itspHeader.index_root;
    ctx->index_head = itspHeader.index_head;
    ctx->block_len = itspHeader.block_len;
    if (dir_page_count(ctx) > CHM_MAX_DIR_PAGES) {
        chm_close(ctx);
        return false;
    }
    ctx->dir_page_count = dir_page_count(ctx);

    if (ctx->index_root <= -1) ctx->index_root = ctx->index_head;

    collect_entries(ctx);

    ctx->compression_enabled = 1;

#if 0

    if (!chm_resolve_entry(ctx,
                           _CHMU_SPANINFO,
                           &uiSpan) ||
        uiSpan.is_compressed)
    {
        chm_close(ctx);
        return false;
    }

    sremain = 8;
    sbufpos = sbuffer;
    if (read_entry_range(ctx, &uiSpan, sbuffer,
                            0, sremain) != sremain                        ||
        !read_u64(&sbufpos, &sremain, &ctx->span))
    {
        chm_close(ctx);
        return false;
    }
#endif

    if (!chm_resolve_entry(ctx, _CHMU_RESET_TABLE, &ctx->rt_entry) ||
        ctx->rt_entry.is_compressed ||
        !chm_resolve_entry(ctx, _CHMU_CONTENT, &ctx->cn_entry) ||
        ctx->cn_entry.is_compressed ||
        !chm_resolve_entry(ctx, _CHMU_LZXC_CONTROLDATA, &uiLzxc) ||
        uiLzxc.is_compressed) {
        ctx->compression_enabled = 0;
    }

    if (ctx->compression_enabled) {
        sremain = _CHM_LZXC_RESETTABLE_V1_LEN;
        sbufpos = sbuffer;
        ok = read_entry_range(ctx, &ctx->rt_entry, sbuffer, 0, sremain) == sremain;
        if (ok) {
            ok = read_lzxc_reset_table(&sbufpos, &sremain, &ctx->reset_table);
        }
        if (!ok) {
            ctx->compression_enabled = 0;
        }
    }

    if (ctx->compression_enabled) {
        sremain = (unsigned int)uiLzxc.length;
        if (uiLzxc.length > sizeof(sbuffer)) {
            chm_close(ctx);
            return false;
        }

        sbufpos = sbuffer;
        if (read_entry_range(ctx, &uiLzxc, sbuffer, 0, sremain) != sremain ||
            !read_lzxc_control_data(&sbufpos, &sremain, &ctlData)) {
            ctx->compression_enabled = 0;
        } else
        {
            ctx->window_size = ctlData.windowSize;
            ctx->reset_interval = ctlData.resetInterval;

#if 0
        ctx->reset_blkcount = ctx->reset_interval /
                    (ctx->window_size / 2);
#else
            ctx->reset_blkcount =
                ctx->reset_interval / (ctx->window_size / 2) * ctlData.windowsPerReset;
#endif
        }
    }

    return true;
}

void chm_close(chm_ctx *ctx)
{
    if (!ctx) return;
    if (ctx->lzx_state) LZXteardown(ctx->lzx_state);
    ctx->lzx_state = NULL;

    for (int i = 0; i < CHM_MAX_BLOCKS_CACHED; i++) {
        chm_free(ctx, ctx->cache_blocks[i]);
        ctx->cache_blocks[i] = NULL;
        ctx->cache_block_indices[i] = -1;
    }
    ctx->cache_num_blocks = CHM_MAX_BLOCKS_CACHED;

    if (ctx->entries) {
        for (int i = 0; i < ctx->entry_count; i++) {
            chm_free(ctx, ctx->entries[i].path);
        }
        chm_free(ctx, ctx->entries);
        chm_free(ctx, ctx->entry_ptrs);
        ctx->entries = NULL;
        ctx->entry_ptrs = NULL;
        ctx->entry_count = 0;
    }

    ctx->data = NULL;
    ctx->data_len = 0;

    ctx->dir_offset = 0;
    ctx->dir_len = 0;
    ctx->data_offset = 0;
    ctx->index_root = 0;
    ctx->index_head = 0;
    ctx->block_len = 0;
    ctx->span = 0;
    memset(&ctx->rt_entry, 0, sizeof(ctx->rt_entry));
    memset(&ctx->cn_entry, 0, sizeof(ctx->cn_entry));
    memset(&ctx->reset_table, 0, sizeof(ctx->reset_table));
    ctx->compression_enabled = 0;
    ctx->window_size = 0;
    ctx->reset_interval = 0;
    ctx->reset_blkcount = 0;
    ctx->lzx_last_block = 0;
    ctx->dir_page_count = 0;
    ctx->dir_pages_seen = 0;
    memset(ctx->dir_seen_bitmap, 0, sizeof(ctx->dir_seen_bitmap));
}

int64_t chm_read_entry(chm_ctx *ctx, struct chm_entry *entry, uint8_t *buf) {
    if (!entry) return 0;
    return read_entry_range(ctx, entry, buf, 0, entry->length);
}


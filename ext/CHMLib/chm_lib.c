/***************************************************************************
 *             chm_lib.c - CHM archive manipulation routines               *
 *                           -------------------                           *
 *                                                                         *
 *  author:     Jed Wing <jedwin@ugcs.caltech.edu>                         *
 *  version:    0.3                                                        *
 *  notes:      These routines are meant for the manipulation of microsoft *
 *              .chm (compiled html help) files, but may likely be used    *
 *              for the manipulation of any ITSS archive, if ever ITSS     *
 *              archives are used for any other purpose.                   *
 *                                                                         *
 *              Note also that the section names are statically handled.   *
 *              To be entirely correct, the section names should be read   *
 *              from the section names meta-file, and then the various     *
 *              content sections and the "transforms" to apply to the data *
 *              they contain should be inferred from the section name and  *
 *              the meta-files referenced using that name; however, all of *
 *              the files I've been able to get my hands on appear to have *
 *              only two sections: Uncompressed and MSCompressed.          *
 *              Additionally, the ITSS.DLL file included with Windows does *
 *              not appear to handle any different transforms than the     *
 *              simple LZX-transform.  Furthermore, the list of transforms *
 *              to apply is broken, in that only half the required space   *
 *              is allocated for the list.  (It appears as though the      *
 *              space is allocated for ASCII strings, but the strings are  *
 *              written as unicode.  As a result, only the first half of   *
 *              the string appears.)  So this is probably not too big of   *
 *              a deal, at least until CHM v4 (MS .lit files), which also  *
 *              incorporate encryption, of some description.               *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

#include "chm_lib.h"
#include "lzx.h"

#ifdef WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

/*
 * defines related to tuning
 */
#ifndef CHM_MAX_BLOCKS_CACHED
#define CHM_MAX_BLOCKS_CACHED 5
#endif
#define CHM_MAX_DIR_PAGES 65536
#define CHM_DIR_SEEN_BITMAP_BITS CHM_MAX_DIR_PAGES
#define CHM_DIR_SEEN_BITMAP_WORDS (CHM_DIR_SEEN_BITMAP_BITS / 32)

#if defined(WIN32)
static int ffs(unsigned int val) {
    int bit = 1, idx = 1;
    while (bit != 0 && (val & bit) == 0) {
        bit <<= 1;
        ++idx;
    }
    if (bit == 0)
        return 0;
    else
        return idx;
}

#endif

/* utilities for unmarshalling data */
static int _unmarshal_char_array(uint8_t** pData, unsigned int* pLenRemain, char* dest, int count) {
    if (count <= 0 || (unsigned int)count > *pLenRemain) return 0;
    memcpy(dest, (*pData), count);
    *pData += count;
    *pLenRemain -= count;
    return 1;
}

static int _unmarshal_uchar_array(uint8_t** pData, unsigned int* pLenRemain, uint8_t* dest, int count) {
    if (count <= 0 || (unsigned int)count > *pLenRemain) return 0;
    memcpy(dest, (*pData), count);
    *pData += count;
    *pLenRemain -= count;
    return 1;
}

#if 0
static int _unmarshal_int16(uint8_t **pData,
                            unsigned int *pLenRemain,
                            Int16 *dest)
{
    if (2 > *pLenRemain)
        return 0;
    *dest = (*pData)[0] | (*pData)[1]<<8;
    *pData += 2;
    *pLenRemain -= 2;
    return 1;
}

static int _unmarshal_uint16_t(uint8_t **pData,
                             unsigned int *pLenRemain,
                             uint16_t *dest)
{
    if (2 > *pLenRemain)
        return 0;
    *dest = (*pData)[0] | (*pData)[1]<<8;
    *pData += 2;
    *pLenRemain -= 2;
    return 1;
}
#endif

static int _unmarshal_int32(uint8_t** pData, unsigned int* pLenRemain, int32_t* dest) {
    if (4 > *pLenRemain) return 0;
    *dest = (*pData)[0] | (*pData)[1] << 8 | (*pData)[2] << 16 | (*pData)[3] << 24;
    *pData += 4;
    *pLenRemain -= 4;
    return 1;
}

static int _unmarshal_uint32(uint8_t** pData, unsigned int* pLenRemain, uint32_t* dest) {
    if (4 > *pLenRemain) return 0;
    *dest = (*pData)[0] | (*pData)[1] << 8 | (*pData)[2] << 16 | (*pData)[3] << 24;
    *pData += 4;
    *pLenRemain -= 4;
    return 1;
}

static int _unmarshal_int64(uint8_t** pData, unsigned int* pLenRemain, int64_t* dest) {
    int64_t temp;
    int i;
    if (8 > *pLenRemain) return 0;
    temp = 0;
    for (i = 8; i > 0; i--) {
        temp <<= 8;
        temp |= (*pData)[i - 1];
    }
    *dest = temp;
    *pData += 8;
    *pLenRemain -= 8;
    return 1;
}

static int _unmarshal_uint64(uint8_t** pData, unsigned int* pLenRemain, uint64_t* dest) {
    uint64_t temp;
    int i;
    if (8 > *pLenRemain) return 0;
    temp = 0;
    for (i = 8; i > 0; i--) {
        temp <<= 8;
        temp |= (*pData)[i - 1];
    }
    *dest = temp;
    *pData += 8;
    *pLenRemain -= 8;
    return 1;
}

static int _unmarshal_uuid(uint8_t** pData, unsigned int* pDataLen, uint8_t* dest) {
    return _unmarshal_uchar_array(pData, pDataLen, dest, 16);
}

/* names of sections essential to decompression */
static const char _CHMU_RESET_TABLE[] =
    "::DataSpace/Storage/MSCompressed/Transform/"
    "{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/"
    "InstanceData/ResetTable";
static const char _CHMU_LZXC_CONTROLDATA[] = "::DataSpace/Storage/MSCompressed/ControlData";
static const char _CHMU_CONTENT[] = "::DataSpace/Storage/MSCompressed/Content";
static const char _CHMU_SPANINFO[] = "::DataSpace/Storage/MSCompressed/SpanInfo";

/*
 * structures local to this module
 */

/* structure of ITSF headers */
#define _CHM_ITSF_V2_LEN (0x58)
#define _CHM_ITSF_V3_LEN (0x60)
struct chmItsfHeader {
    char signature[4];       /*  0 (ITSF) */
    int32_t version;         /*  4 */
    int32_t header_len;      /*  8 */
    int32_t unknown_000c;    /*  c */
    uint32_t last_modified;  /* 10 */
    uint32_t lang_id;        /* 14 */
    uint8_t dir_uuid[16];    /* 18 */
    uint8_t stream_uuid[16]; /* 28 */
    uint64_t unknown_offset; /* 38 */
    uint64_t unknown_len;    /* 40 */
    uint64_t dir_offset;     /* 48 */
    uint64_t dir_len;        /* 50 */
    uint64_t data_offset;    /* 58 (Not present before V3) */
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_itsf_header(uint8_t** pData, unsigned int* pDataLen, struct chmItsfHeader* dest) {
    /* we only know how to deal with the 0x58 and 0x60 byte structures */
    if (*pDataLen != _CHM_ITSF_V2_LEN && *pDataLen != _CHM_ITSF_V3_LEN) return 0;

    /* unmarshal common fields */
    _unmarshal_char_array(pData, pDataLen, dest->signature, 4);
    _unmarshal_int32(pData, pDataLen, &dest->version);
    _unmarshal_int32(pData, pDataLen, &dest->header_len);
    _unmarshal_int32(pData, pDataLen, &dest->unknown_000c);
    _unmarshal_uint32(pData, pDataLen, &dest->last_modified);
    _unmarshal_uint32(pData, pDataLen, &dest->lang_id);
    _unmarshal_uuid(pData, pDataLen, dest->dir_uuid);
    _unmarshal_uuid(pData, pDataLen, dest->stream_uuid);
    _unmarshal_uint64(pData, pDataLen, &dest->unknown_offset);
    _unmarshal_uint64(pData, pDataLen, &dest->unknown_len);
    _unmarshal_uint64(pData, pDataLen, &dest->dir_offset);
    _unmarshal_uint64(pData, pDataLen, &dest->dir_len);

    /* error check the data */
    /* XXX: should also check UUIDs, probably, though with a version 3 file,
     * current MS tools do not seem to use them.
     */
    if (memcmp(dest->signature, "ITSF", 4) != 0) return 0;
    if (dest->version == 2) {
        if (dest->header_len < _CHM_ITSF_V2_LEN) return 0;
    } else if (dest->version == 3) {
        if (dest->header_len < _CHM_ITSF_V3_LEN) return 0;
    } else
        return 0;

    /* now, if we have a V3 structure, unmarshal the rest.
     * otherwise, compute it
     */
    if (dest->version == 3) {
        if (*pDataLen != 0)
            _unmarshal_uint64(pData, pDataLen, &dest->data_offset);
        else
            return 0;
    } else
        dest->data_offset = dest->dir_offset + dest->dir_len;

    /* SumatraPDF: sanity check (huge values are usually due to broken files) */
    if (dest->dir_offset > UINT_MAX || dest->dir_len > UINT_MAX) return 0;

    return 1;
}

/* structure of ITSP headers */
#define _CHM_ITSP_V1_LEN (0x54)
struct chmItspHeader {
    char signature[4];        /*  0 (ITSP) */
    int32_t version;          /*  4 */
    int32_t header_len;       /*  8 */
    int32_t unknown_000c;     /*  c */
    uint32_t block_len;       /* 10 */
    int32_t blockidx_intvl;   /* 14 */
    int32_t index_depth;      /* 18 */
    int32_t index_root;       /* 1c */
    int32_t index_head;       /* 20 */
    int32_t unknown_0024;     /* 24 */
    uint32_t num_blocks;      /* 28 */
    int32_t unknown_002c;     /* 2c */
    uint32_t lang_id;         /* 30 */
    uint8_t system_uuid[16];  /* 34 */
    uint8_t unknown_0044[16]; /* 44 */
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_itsp_header(uint8_t** pData, unsigned int* pDataLen, struct chmItspHeader* dest) {
    /* we only know how to deal with a 0x54 byte structures */
    if (*pDataLen != _CHM_ITSP_V1_LEN) return 0;

    /* unmarshal fields */
    _unmarshal_char_array(pData, pDataLen, dest->signature, 4);
    _unmarshal_int32(pData, pDataLen, &dest->version);
    _unmarshal_int32(pData, pDataLen, &dest->header_len);
    _unmarshal_int32(pData, pDataLen, &dest->unknown_000c);
    _unmarshal_uint32(pData, pDataLen, &dest->block_len);
    _unmarshal_int32(pData, pDataLen, &dest->blockidx_intvl);
    _unmarshal_int32(pData, pDataLen, &dest->index_depth);
    _unmarshal_int32(pData, pDataLen, &dest->index_root);
    _unmarshal_int32(pData, pDataLen, &dest->index_head);
    _unmarshal_int32(pData, pDataLen, &dest->unknown_0024);
    _unmarshal_uint32(pData, pDataLen, &dest->num_blocks);
    _unmarshal_int32(pData, pDataLen, &dest->unknown_002c);
    _unmarshal_uint32(pData, pDataLen, &dest->lang_id);
    _unmarshal_uuid(pData, pDataLen, dest->system_uuid);
    _unmarshal_uchar_array(pData, pDataLen, dest->unknown_0044, 16);

    /* error check the data */
    if (memcmp(dest->signature, "ITSP", 4) != 0) return 0;
    if (dest->version != 1) return 0;
    if (dest->header_len != _CHM_ITSP_V1_LEN) return 0;
    /* SumatraPDF: sanity check */
    if (dest->block_len == 0) return 0;

    return 1;
}

/* structure of PMGL headers */
static const char* _chm_pmgl_marker = "PMGL";
#define _CHM_PMGL_LEN (0x14)
struct chmPmglHeader {
    char signature[4];     /*  0 (PMGL) */
    uint32_t free_space;   /*  4 */
    uint32_t unknown_0008; /*  8 */
    int32_t block_prev;    /*  c */
    int32_t block_next;    /* 10 */
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_pmgl_header(uint8_t** pData, unsigned int* pDataLen, unsigned int blockLen,
                                  struct chmPmglHeader* dest) {
    /* we only know how to deal with a 0x14 byte structures */
    if (*pDataLen != _CHM_PMGL_LEN) return 0;
    /* SumatraPDF: sanity check */
    if (blockLen < _CHM_PMGL_LEN) return 0;

    /* unmarshal fields */
    _unmarshal_char_array(pData, pDataLen, dest->signature, 4);
    _unmarshal_uint32(pData, pDataLen, &dest->free_space);
    _unmarshal_uint32(pData, pDataLen, &dest->unknown_0008);
    _unmarshal_int32(pData, pDataLen, &dest->block_prev);
    _unmarshal_int32(pData, pDataLen, &dest->block_next);

    /* check structure */
    if (memcmp(dest->signature, _chm_pmgl_marker, 4) != 0) return 0;
    /* SumatraPDF: sanity check */
    if (dest->free_space > blockLen - _CHM_PMGL_LEN) return 0;

    return 1;
}

/* structure of PMGI headers */
static const char* _chm_pmgi_marker = "PMGI";
#define _CHM_PMGI_LEN (0x08)
struct chmPmgiHeader {
    char signature[4];   /*  0 (PMGI) */
    uint32_t free_space; /*  4 */
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_pmgi_header(uint8_t** pData, unsigned int* pDataLen, unsigned int blockLen,
                                  struct chmPmgiHeader* dest) {
    /* we only know how to deal with a 0x8 byte structures */
    if (*pDataLen != _CHM_PMGI_LEN) return 0;
    /* SumatraPDF: sanity check */
    if (blockLen < _CHM_PMGI_LEN) return 0;

    /* unmarshal fields */
    _unmarshal_char_array(pData, pDataLen, dest->signature, 4);
    _unmarshal_uint32(pData, pDataLen, &dest->free_space);

    /* check structure */
    if (memcmp(dest->signature, _chm_pmgi_marker, 4) != 0) return 0;
    /* SumatraPDF: sanity check */
    if (dest->free_space > blockLen - _CHM_PMGI_LEN) return 0;

    return 1;
}

/* structure of LZXC reset table */
#define _CHM_LZXC_RESETTABLE_V1_LEN (0x28)
struct chmLzxcResetTable {
    uint32_t version;
    uint32_t block_count;
    uint32_t unknown;
    uint32_t table_offset;
    uint64_t uncompressed_len;
    uint64_t compressed_len;
    uint64_t block_len;
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_lzxc_reset_table(uint8_t** pData, unsigned int* pDataLen, struct chmLzxcResetTable* dest) {
    /* we only know how to deal with a 0x28 byte structures */
    if (*pDataLen != _CHM_LZXC_RESETTABLE_V1_LEN) return 0;

    /* unmarshal fields */
    _unmarshal_uint32(pData, pDataLen, &dest->version);
    _unmarshal_uint32(pData, pDataLen, &dest->block_count);
    _unmarshal_uint32(pData, pDataLen, &dest->unknown);
    _unmarshal_uint32(pData, pDataLen, &dest->table_offset);
    _unmarshal_uint64(pData, pDataLen, &dest->uncompressed_len);
    _unmarshal_uint64(pData, pDataLen, &dest->compressed_len);
    _unmarshal_uint64(pData, pDataLen, &dest->block_len);

    /* check structure */
    if (dest->version != 2) return 0;
    /* SumatraPDF: sanity check (huge values are usually due to broken files) */
    if (dest->block_count == 0) return 0;
    if (dest->uncompressed_len > INT_MAX || dest->compressed_len > INT_MAX) return 0;
    if (dest->block_len == 0 || dest->block_len > INT_MAX) return 0;

    return 1;
}

/* structure of LZXC control data block */
#define _CHM_LZXC_MIN_LEN (0x18)
#define _CHM_LZXC_V2_LEN (0x1c)
struct chmLzxcControlData {
    uint32_t size;            /*  0        */
    char signature[4];        /*  4 (LZXC) */
    uint32_t version;         /*  8        */
    uint32_t resetInterval;   /*  c        */
    uint32_t windowSize;      /* 10        */
    uint32_t windowsPerReset; /* 14        */
    uint32_t unknown_18;      /* 18        */
};

static int _unmarshal_lzxc_control_data(uint8_t** pData, unsigned int* pDataLen, struct chmLzxcControlData* dest) {
    /* we want at least 0x18 bytes */
    if (*pDataLen < _CHM_LZXC_MIN_LEN) return 0;

    /* unmarshal fields */
    _unmarshal_uint32(pData, pDataLen, &dest->size);
    _unmarshal_char_array(pData, pDataLen, dest->signature, 4);
    _unmarshal_uint32(pData, pDataLen, &dest->version);
    _unmarshal_uint32(pData, pDataLen, &dest->resetInterval);
    _unmarshal_uint32(pData, pDataLen, &dest->windowSize);
    _unmarshal_uint32(pData, pDataLen, &dest->windowsPerReset);

    if (*pDataLen >= _CHM_LZXC_V2_LEN)
        _unmarshal_uint32(pData, pDataLen, &dest->unknown_18);
    else
        dest->unknown_18 = 0;

    if (dest->version == 2) {
        dest->resetInterval *= 0x8000;
        dest->windowSize *= 0x8000;
    }
    if (dest->windowSize == 0 || dest->resetInterval == 0) return 0;

    // https://github.com/sumatrapdfreader/sumatrapdf/issues/5207
    // Validate windowSize is in valid LZX range [2^15, 2^21]
    if (dest->windowSize < 32768 || dest->windowSize > 2097152) {
        return 0;
    }
    // Also ensure it's a power of 2
    if ((dest->windowSize & (dest->windowSize - 1)) != 0) {
        return 0;
    }
    if ((dest->resetInterval % (dest->windowSize / 2)) != 0) return 0;

    /* check structure */
    if (memcmp(dest->signature, "LZXC", 4) != 0) return 0;

    return 1;
}

/* the structure used for chm file handles */
struct chmFile {
    const char* data;
    size_t data_len;

    uint64_t dir_offset;
    uint64_t dir_len;
    uint64_t data_offset;
    int32_t index_root;
    int32_t index_head;
    uint32_t block_len;

    uint64_t span;
    struct chmUnitInfo rt_unit;
    struct chmUnitInfo cn_unit;
    struct chmLzxcResetTable reset_table;

    /* LZX control data */
    int compression_enabled;
    uint32_t window_size;
    uint32_t reset_interval;
    uint32_t reset_blkcount;

    /* decompressor state */
    struct LZXstate* lzx_state;
    int lzx_last_block;

    /* cache for decompressed blocks */
    uint8_t** cache_blocks;
    uint64_t* cache_block_indices;
    int32_t cache_num_blocks;

    /* directory traversal visit state (one bit per page, 8 KiB) */
    uint64_t dir_page_count;
    uint64_t dir_pages_seen;
    uint32_t dir_seen_bitmap[CHM_DIR_SEEN_BITMAP_WORDS];
};

static int64_t _chm_fetch_bytes(struct chmFile* h, uint8_t* buf, uint64_t offset, int64_t len) {
    if (len <= 0) {
        return 0;
    }
    if (offset > h->data_len) {
        return 0;
    }
    if ((uint64_t)len > h->data_len - offset) {
        return 0;
    }
    memcpy(buf, h->data + offset, (size_t)len);
    return len;
}

static int _chm_add_u64(uint64_t a, uint64_t b, uint64_t* result) {
    if (a > UINT64_MAX - b) return 0;
    *result = a + b;
    return 1;
}

static int _chm_get_object_offset(struct chmFile* h, const struct chmUnitInfo* ui, uint64_t addr, int64_t len,
                                  uint64_t* offset) {
    uint64_t temp;

    if (len <= 0) return 0;
    if (addr > ui->length) return 0;
    if ((uint64_t)len > ui->length - addr) return 0;
    if (!_chm_add_u64((uint64_t)ui->start, addr, &temp)) return 0;
    if (!_chm_add_u64(h->data_offset, temp, offset)) return 0;
    return 1;
}

static uint64_t _chm_dir_page_count(struct chmFile* h) {
    if (h->block_len == 0) return 0;
    return h->dir_len / h->block_len;
}

static int _chm_is_valid_dir_page(struct chmFile* h, int32_t page) {
    uint64_t page_count = _chm_dir_page_count(h);
    if (page < 0) return 0;
    if (page_count == 0) return 0;
    return (uint64_t)page < page_count;
}

static int _chm_dir_page_offset(struct chmFile* h, int32_t page, uint64_t* offset) {
    uint64_t page_off;

    if (!_chm_is_valid_dir_page(h, page)) return 0;
    if ((uint64_t)page > UINT64_MAX / h->block_len) return 0;
    page_off = (uint64_t)page * h->block_len;
    if (!_chm_add_u64(h->dir_offset, page_off, offset)) return 0;
    if (*offset > h->data_len || (uint64_t)h->block_len > h->data_len - *offset) return 0;
    return 1;
}

static int _chm_dir_fetch_page(struct chmFile* h, int32_t page, uint8_t* page_buf) {
    uint64_t offset;

    if (!_chm_dir_page_offset(h, page, &offset)) return 0;
    return _chm_fetch_bytes(h, page_buf, offset, h->block_len) == h->block_len;
}

static void _chm_dir_visit_reset(struct chmFile* h) {
    h->dir_pages_seen = 0;
    memset(h->dir_seen_bitmap, 0, sizeof(h->dir_seen_bitmap));
}

static int _chm_dir_visit_begin(struct chmFile* h) {
    if (h->dir_page_count == 0) return 0;
    _chm_dir_visit_reset(h);
    return 1;
}

static int _chm_dir_visit_page(struct chmFile* h, int32_t page) {
    uint32_t word_idx;
    uint32_t bit_mask;

    if (page < 0) return 0;
    if ((uint64_t)page >= h->dir_page_count) return 0;
    if (h->dir_pages_seen >= h->dir_page_count) return 0;
    if ((uint32_t)page >= CHM_DIR_SEEN_BITMAP_BITS) return 0;

    word_idx = (uint32_t)page >> 5;
    bit_mask = 1u << (page & 31);
    if (h->dir_seen_bitmap[word_idx] & bit_mask) return 0;
    h->dir_seen_bitmap[word_idx] |= bit_mask;
    h->dir_pages_seen++;
    return 1;
}

struct chmDirSession {
    struct chmFile* h;
    uint8_t* page_buf;
    uint8_t* page_buf_end;
};

static int _chm_dir_session_begin(struct chmFile* h, struct chmDirSession* s) {
    s->h = h;
    s->page_buf = NULL;
    s->page_buf_end = NULL;
    if (!_chm_dir_visit_begin(h)) return 0;
    s->page_buf = (uint8_t*)malloc((unsigned int)h->block_len);
    if (s->page_buf == NULL) return 0;
    s->page_buf_end = s->page_buf + h->block_len;
    return 1;
}

static void _chm_dir_session_end(struct chmDirSession* s) {
    free(s->page_buf);
    s->page_buf = NULL;
    s->page_buf_end = NULL;
}

static int _chm_dir_session_fetch(struct chmDirSession* s, int32_t page) {
    if (!_chm_dir_visit_page(s->h, page)) return 0;
    return _chm_dir_fetch_page(s->h, page, s->page_buf);
}

/* open an ITS archive */
struct chmFile* chm_open(const char* d, size_t len) {
    uint8_t sbuffer[256];
    unsigned int sremain;
    uint8_t* sbufpos;
    struct chmFile* newHandle = NULL;
    struct chmItsfHeader itsfHeader = {0};
    struct chmItspHeader itspHeader = {0};
#if 0
    struct chmUnitInfo          uiSpan;
#endif
    struct chmUnitInfo uiLzxc = {0};
    struct chmLzxcControlData ctlData;
    int ok;

    /* allocate handle */
    newHandle = (struct chmFile*)calloc(sizeof(struct chmFile), 1);
    if (newHandle == NULL) return NULL;
    newHandle->data = d;
    newHandle->data_len = len;
    newHandle->lzx_state = NULL;
    newHandle->cache_blocks = NULL;
    newHandle->cache_block_indices = NULL;
    newHandle->cache_num_blocks = 0;

    /* read and verify header */
    sremain = _CHM_ITSF_V3_LEN;
    sbufpos = sbuffer;
    ok = _chm_fetch_bytes(newHandle, sbuffer, (uint64_t)0, sremain) == sremain;
    if (ok) {
        ok = _unmarshal_itsf_header(&sbufpos, &sremain, &itsfHeader);
    }
    if (!ok) {
        chm_close(newHandle);
        return NULL;
    }

    /* stash important values from header */
    newHandle->dir_offset = itsfHeader.dir_offset;
    newHandle->dir_len = itsfHeader.dir_len;
    newHandle->data_offset = itsfHeader.data_offset;

    /* now, read and verify the directory header chunk */
    sremain = _CHM_ITSP_V1_LEN;
    sbufpos = sbuffer;
    ok = _chm_fetch_bytes(newHandle, sbuffer, (uint64_t)itsfHeader.dir_offset, sremain) == sremain;
    if (ok) {
        ok = _unmarshal_itsp_header(&sbufpos, &sremain, &itspHeader);
    }
    if (!ok) {
        chm_close(newHandle);
        return NULL;
    }
    if (itsfHeader.dir_len < (uint64_t)itspHeader.header_len) {
        chm_close(newHandle);
        return NULL;
    }

    /* grab essential information from ITSP header */
    newHandle->dir_offset += itspHeader.header_len;
    newHandle->dir_len -= itspHeader.header_len;
    newHandle->index_root = itspHeader.index_root;
    newHandle->index_head = itspHeader.index_head;
    newHandle->block_len = itspHeader.block_len;
    if (_chm_dir_page_count(newHandle) > CHM_MAX_DIR_PAGES) {
        chm_close(newHandle);
        return NULL;
    }
    newHandle->dir_page_count = _chm_dir_page_count(newHandle);

    /* if the index root is -1, this means we don't have any PMGI blocks.
     * as a result, we must use the sole PMGL block as the index root
     */
    if (newHandle->index_root <= -1) newHandle->index_root = newHandle->index_head;

    /* By default, compression is enabled. */
    newHandle->compression_enabled = 1;

/* Jed, Sun Jun 27: 'span' doesn't seem to be used anywhere?! */
#if 0
    /* fetch span */
    if (CHM_RESOLVE_SUCCESS != chm_resolve_object(newHandle,
                                                  _CHMU_SPANINFO,
                                                  &uiSpan)                ||
        uiSpan.space == CHM_COMPRESSED)
    {
        chm_close(newHandle);
        return NULL;
    }

    /* N.B.: we've already checked that uiSpan is in the uncompressed section,
     *       so this should not require attempting to decompress, which may
     *       rely on having a valid "span"
     */
    sremain = 8;
    sbufpos = sbuffer;
    if (chm_retrieve_object(newHandle, &uiSpan, sbuffer,
                            0, sremain) != sremain                        ||
        !_unmarshal_uint64(&sbufpos, &sremain, &newHandle->span))
    {
        chm_close(newHandle);
        return NULL;
    }
#endif

    /* prefetch most commonly needed unit infos */
    if (CHM_RESOLVE_SUCCESS != chm_resolve_object(newHandle, _CHMU_RESET_TABLE, &newHandle->rt_unit) ||
        newHandle->rt_unit.space == CHM_COMPRESSED ||
        CHM_RESOLVE_SUCCESS != chm_resolve_object(newHandle, _CHMU_CONTENT, &newHandle->cn_unit) ||
        newHandle->cn_unit.space == CHM_COMPRESSED ||
        CHM_RESOLVE_SUCCESS != chm_resolve_object(newHandle, _CHMU_LZXC_CONTROLDATA, &uiLzxc) ||
        uiLzxc.space == CHM_COMPRESSED) {
        newHandle->compression_enabled = 0;
    }

    /* read reset table info */
    if (newHandle->compression_enabled) {
        sremain = _CHM_LZXC_RESETTABLE_V1_LEN;
        sbufpos = sbuffer;
        ok = chm_retrieve_object(newHandle, &newHandle->rt_unit, sbuffer, 0, sremain) == sremain;
        if (ok) {
            ok = _unmarshal_lzxc_reset_table(&sbufpos, &sremain, &newHandle->reset_table);
        }
        if (!ok) {
            newHandle->compression_enabled = 0;
        }
    }

    /* read control data */
    if (newHandle->compression_enabled) {
        sremain = (unsigned int)uiLzxc.length;
        if (uiLzxc.length > sizeof(sbuffer)) {
            chm_close(newHandle);
            return NULL;
        }

        sbufpos = sbuffer;
        if (chm_retrieve_object(newHandle, &uiLzxc, sbuffer, 0, sremain) != sremain ||
            !_unmarshal_lzxc_control_data(&sbufpos, &sremain, &ctlData)) {
            newHandle->compression_enabled = 0;
        } else /* SumatraPDF: prevent division by zero */
        {
            newHandle->window_size = ctlData.windowSize;
            newHandle->reset_interval = ctlData.resetInterval;

/* Jed, Mon Jun 28: Experimentally, it appears that the reset block count */
/*       must be multiplied by this formerly unknown ctrl data field in   */
/*       order to decompress some files.                                  */
#if 0
        newHandle->reset_blkcount = newHandle->reset_interval /
                    (newHandle->window_size / 2);
#else
            newHandle->reset_blkcount =
                newHandle->reset_interval / (newHandle->window_size / 2) * ctlData.windowsPerReset;
#endif
        }
    }

    /* initialize cache */
    chm_set_param(newHandle, CHM_PARAM_MAX_BLOCKS_CACHED, CHM_MAX_BLOCKS_CACHED);

    return newHandle;
}

/* close an ITS archive */
void chm_close(struct chmFile* h) {
    if (h == NULL) {
        return;
    }
    if (h->lzx_state) LZXteardown(h->lzx_state);
    h->lzx_state = NULL;

    if (h->cache_blocks) {
        int i;
        for (i = 0; i < h->cache_num_blocks; i++) {
            if (h->cache_blocks[i]) free(h->cache_blocks[i]);
        }
        free(h->cache_blocks);
        h->cache_blocks = NULL;
    }

    if (h->cache_block_indices) free(h->cache_block_indices);
    h->cache_block_indices = NULL;

    free(h);
}

/*
 * set a parameter on the file handle.
 * valid parameter types:
 *          CHM_PARAM_MAX_BLOCKS_CACHED:
 *                 how many decompressed blocks should be cached?  A simple
 *                 caching scheme is used, wherein the index of the block is
 *                 used as a hash value, and hash collision results in the
 *                 invalidation of the previously cached block.
 */
void chm_set_param(struct chmFile* h, int paramType, int paramVal) {
    switch (paramType) {
        case CHM_PARAM_MAX_BLOCKS_CACHED:
            if (paramVal != h->cache_num_blocks) {
                uint8_t** newBlocks;
                uint64_t* newIndices;
                int i;

                /* allocate new cached blocks */
                newBlocks = (uint8_t**)malloc(paramVal * sizeof(uint8_t*));
                if (newBlocks == NULL) return;
                newIndices = (uint64_t*)malloc(paramVal * sizeof(uint64_t));
                if (newIndices == NULL) {
                    free(newBlocks);
                    return;
                }
                for (i = 0; i < paramVal; i++) {
                    newBlocks[i] = NULL;
                    newIndices[i] = 0;
                }

                /* re-distribute old cached blocks */
                if (h->cache_blocks) {
                    for (i = 0; i < h->cache_num_blocks; i++) {
                        int newSlot = (int)(h->cache_block_indices[i] % paramVal);

                        if (h->cache_blocks[i]) {
                            /* in case of collision, destroy newcomer */
                            if (newBlocks[newSlot]) {
                                free(h->cache_blocks[i]);
                                h->cache_blocks[i] = NULL;
                            } else {
                                newBlocks[newSlot] = h->cache_blocks[i];
                                newIndices[newSlot] = h->cache_block_indices[i];
                            }
                        }
                    }

                    free(h->cache_blocks);
                    free(h->cache_block_indices);
                }

                /* now, set new values */
                h->cache_blocks = newBlocks;
                h->cache_block_indices = newIndices;
                h->cache_num_blocks = paramVal;
            }
            break;

        default:
            break;
    }
}

/*
 * helper methods for chm_resolve_object
 */

/* skip a compressed dword */
static void _chm_skip_cword(uint8_t** pEntry, uint8_t* end) {
    while ((*pEntry < end) && *(*pEntry)++ >= 0x80);
}

/* skip the data from a PMGL entry */
static void _chm_skip_PMGL_entry_data(uint8_t** pEntry, uint8_t* end) {
    _chm_skip_cword(pEntry, end);
    _chm_skip_cword(pEntry, end);
    _chm_skip_cword(pEntry, end);
}

/* parse a compressed dword */
static int _chm_parse_cword(uint8_t** pEntry, uint8_t* end, uint64_t* result) {
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

/* parse a utf-8 string into an ASCII char buffer */
static int _chm_parse_UTF8(uint8_t** pEntry, uint8_t* end, uint64_t count, char* path) {
    /* XXX: implement UTF-8 support, including a real mapping onto
     *      ISO-8859-1?  probably there is a library to do this?  As is
     *      immediately apparent from the below code, I'm presently not doing
     *      any special handling for files in which none of the strings contain
     *      UTF-8 multi-byte characters.
     */
    if (*pEntry > end) return 0;
    if ((uint64_t)(end - *pEntry) < count) return 0;
    memcpy(path, *pEntry, (size_t)count);
    path += count;
    *pEntry += count;

    *path = '\0';
    return 1;
}

/* parse a PMGL entry into a chmUnitInfo struct; return 1 on success. */
static int _chm_parse_PMGL_entry(uint8_t** pEntry, uint8_t* end, struct chmUnitInfo* ui) {
    uint64_t strLen;

    /* parse str len */
    if (!_chm_parse_cword(pEntry, end, &strLen)) return 0;
    if (strLen == 0 || strLen > CHM_MAX_PATHLEN) return 0;

    /* parse path */
    if (!_chm_parse_UTF8(pEntry, end, strLen, ui->path)) return 0;

    /* parse info */
    if (!_chm_parse_cword(pEntry, end, &strLen)) return 0;
    ui->space = (int)strLen;
    if (!_chm_parse_cword(pEntry, end, &ui->start)) return 0;
    if (!_chm_parse_cword(pEntry, end, &ui->length)) return 0;
    return 1;
}

/* find an exact entry in PMGL; return NULL if we fail */
static uint8_t* _chm_find_in_PMGL(uint8_t* page_buf, uint32_t block_len, const char* objPath) {
    /* XXX: modify this to do a binary search using the nice index structure
     *      that is provided for us.
     */
    struct chmPmglHeader header;
    unsigned int hremain;
    uint8_t* end;
    uint8_t* cur;
    uint8_t* temp;
    uint64_t strLen;
    char buffer[CHM_MAX_PATHLEN + 1];

    /* figure out where to start and end */
    cur = page_buf;
    hremain = _CHM_PMGL_LEN;
    if (!_unmarshal_pmgl_header(&cur, &hremain, block_len, &header)) return NULL;
    end = page_buf + block_len - (header.free_space);

    /* now, scan progressively */
    while (cur < end) {
        /* grab the name */
        temp = cur;
        if (!_chm_parse_cword(&cur, end, &strLen)) return NULL;
        if (strLen == 0 || strLen > CHM_MAX_PATHLEN) return NULL;
        if (!_chm_parse_UTF8(&cur, end, strLen, buffer)) return NULL;

        /* check if it is the right name */
        if (!_stricmp(buffer, objPath)) return temp;

        _chm_skip_PMGL_entry_data(&cur, end);
    }

    return NULL;
}

/* find which block should be searched next for the entry; -1 if no block */
static int32_t _chm_find_in_PMGI(uint8_t* page_buf, uint32_t block_len, const char* objPath) {
    /* XXX: modify this to do a binary search using the nice index structure
     *      that is provided for us
     */
    struct chmPmgiHeader header;
    unsigned int hremain;
    int page = -1;
    uint8_t* end;
    uint8_t* cur;
    uint64_t strLen;
    char buffer[CHM_MAX_PATHLEN + 1];

    /* figure out where to start and end */
    cur = page_buf;
    hremain = _CHM_PMGI_LEN;
    if (!_unmarshal_pmgi_header(&cur, &hremain, block_len, &header)) return -1;
    end = page_buf + block_len - (header.free_space);

    /* now, scan progressively */
    while (cur < end) {
        /* grab the name */
        if (!_chm_parse_cword(&cur, end, &strLen)) return -1;
        if (strLen == 0 || strLen > CHM_MAX_PATHLEN) return -1;
        if (!_chm_parse_UTF8(&cur, end, strLen, buffer)) return -1;

        /* check if it is the right name */
        if (strcasecmp(buffer, objPath) > 0) return page;

        /* load next value for path */
        if (!_chm_parse_cword(&cur, end, &strLen) || strLen > INT_MAX) return -1;
        page = (int)strLen;
    }

    return page;
}

static void _chm_set_entry_flags(struct chmUnitInfo* ui) {
    size_t path_len = strlen(ui->path);
    uint64_t ui_path_len;

    if (path_len == 0) return;
    ui_path_len = path_len - 1;

    if (ui->path[ui_path_len] == '/') ui->flags |= CHM_ENUMERATE_DIRS;
    if (ui->path[ui_path_len] != '/') ui->flags |= CHM_ENUMERATE_FILES;
    if (ui->path[0] == '/') {
        if (ui->path[1] == '#' || ui->path[1] == '$')
            ui->flags |= CHM_ENUMERATE_SPECIAL;
        else
            ui->flags |= CHM_ENUMERATE_NORMAL;
    } else
        ui->flags |= CHM_ENUMERATE_META;
}

typedef int (*ChmPmglEntryFn)(struct chmFile* h, struct chmUnitInfo* ui, void* ctx);

typedef enum {
    CHM_PMGL_WALK_FAILURE = 0,
    CHM_PMGL_WALK_DONE = 1,
} ChmPmglWalkResult;

static ChmPmglWalkResult _chm_walk_pmgl_pages(struct chmFile* h, int32_t start_page, ChmPmglEntryFn entry_fn,
                                              void* ctx) {
    struct chmDirSession session;
    int32_t curPage;
    ChmPmglWalkResult result = CHM_PMGL_WALK_DONE;

    if (!_chm_dir_session_begin(h, &session)) return CHM_PMGL_WALK_FAILURE;

    curPage = start_page;
    while (curPage != -1) {
        struct chmPmglHeader header;
        uint8_t* cur;
        uint8_t* end;
        unsigned int lenRemain;

        if (!_chm_dir_session_fetch(&session, curPage)) {
            result = CHM_PMGL_WALK_FAILURE;
            goto cleanup;
        }

        cur = session.page_buf;
        lenRemain = _CHM_PMGL_LEN;
        if (!_unmarshal_pmgl_header(&cur, &lenRemain, h->block_len, &header)) {
            result = CHM_PMGL_WALK_FAILURE;
            goto cleanup;
        }
        end = session.page_buf + h->block_len - (header.free_space);

        while (cur < end) {
            struct chmUnitInfo ui;
            int status;

            ui.flags = 0;
            if (!_chm_parse_PMGL_entry(&cur, end, &ui)) {
                result = CHM_PMGL_WALK_FAILURE;
                goto cleanup;
            }

            status = entry_fn(h, &ui, ctx);
            if (status == CHM_ENUMERATOR_FAILURE) {
                result = CHM_PMGL_WALK_FAILURE;
                goto cleanup;
            }
            if (status == CHM_ENUMERATOR_SUCCESS) goto cleanup;
        }

        curPage = header.block_next < 0 ? -1 : header.block_next;
    }

cleanup:
    _chm_dir_session_end(&session);
    return result;
}

/* resolve a particular object from the archive */
int chm_resolve_object(struct chmFile* h, const char* objPath, struct chmUnitInfo* ui) {
    /*
     * XXX: implement caching scheme for dir pages
     */

    struct chmDirSession session;
    int32_t curPage;
    int result = CHM_RESOLVE_FAILURE;

    if (!_chm_dir_session_begin(h, &session)) return CHM_RESOLVE_FAILURE;

    curPage = h->index_root;
    while (curPage != -1) {
        int32_t new_page;
        uint8_t* pEntry;

        if (!_chm_dir_session_fetch(&session, curPage)) goto cleanup;

        if (h->block_len < 4) goto cleanup;

        if (memcmp(session.page_buf, _chm_pmgl_marker, 4) == 0) {
            pEntry = _chm_find_in_PMGL(session.page_buf, h->block_len, objPath);
            if (pEntry == NULL) goto cleanup;
            if (!_chm_parse_PMGL_entry(&pEntry, session.page_buf_end, ui)) goto cleanup;
            result = CHM_RESOLVE_SUCCESS;
            goto cleanup;
        } else if (memcmp(session.page_buf, _chm_pmgi_marker, 4) == 0) {
            new_page = _chm_find_in_PMGI(session.page_buf, h->block_len, objPath);
            curPage = new_page;
        } else {
            goto cleanup;
        }
    }

cleanup:
    _chm_dir_session_end(&session);
    return result;
}

/*
 * utility methods for dealing with compressed data
 */

/* get the bounds of a compressed block.  return 0 on failure */
static int _chm_get_cmpblock_bounds(struct chmFile* h, uint64_t block, uint64_t* start, int64_t* len) {
    uint8_t buffer[8], *dummy;
    unsigned int remain;
    uint64_t table_offset;
    uint64_t table_addr;
    uint64_t block_entry_offset;
    uint64_t abs_start;

    if (block > UINT64_MAX / 8) return 0;
    block_entry_offset = block * 8;

    /* for all but the last block, use the reset table */
    if (block < h->reset_table.block_count - 1) {
        /* unpack the start address */
        dummy = buffer;
        remain = 8;
        if (!_chm_add_u64((uint64_t)h->reset_table.table_offset, block_entry_offset, &table_addr)) return 0;
        if (!_chm_get_object_offset(h, &h->rt_unit, table_addr, remain, &table_offset) ||
            _chm_fetch_bytes(h, buffer, table_offset, remain) != remain || !_unmarshal_uint64(&dummy, &remain, start))
            return 0;

        /* unpack the end address */
        dummy = buffer;
        remain = 8;
        if (!_chm_add_u64(table_addr, 8, &table_addr)) return 0;
        if (!_chm_get_object_offset(h, &h->rt_unit, table_addr, remain, &table_offset) ||
            _chm_fetch_bytes(h, buffer, table_offset, remain) != remain || !_unmarshal_int64(&dummy, &remain, len))
            return 0;
    }

    /* for the last block, use the span in addition to the reset table */
    else {
        /* unpack the start address */
        dummy = buffer;
        remain = 8;
        if (!_chm_add_u64((uint64_t)h->reset_table.table_offset, block_entry_offset, &table_addr)) return 0;
        if (!_chm_get_object_offset(h, &h->rt_unit, table_addr, remain, &table_offset) ||
            _chm_fetch_bytes(h, buffer, table_offset, remain) != remain || !_unmarshal_uint64(&dummy, &remain, start))
            return 0;

        *len = h->reset_table.compressed_len;
    }

    /* compute the length and absolute start address */
    if (*start > (uint64_t)*len) {
        return 0; // Invalid block bounds
    }
    *len -= *start;
    if (!_chm_get_object_offset(h, &h->cn_unit, *start, *len, &abs_start)) return 0;
    *start = abs_start;

    return 1;
}

/* decompress the block.  must have lzx_mutex. */
static int64_t _chm_decompress_block(struct chmFile* h, uint64_t block, uint8_t** ubuffer) {
    uint8_t* cbuffer;
    uint64_t cbufferLen;
    uint64_t cmpStart;                                           /* compressed start  */
    int64_t cmpLen;                                              /* compressed len    */
    int indexSlot;                                               /* cache index slot  */
    uint8_t* lbuffer;                                            /* local buffer ptr  */
    uint32_t blockAlign = (uint32_t)(block % h->reset_blkcount); /* reset intvl. aln. */
    uint32_t i;                                                  /* local loop index  */
    int ok;

    cbufferLen = h->reset_table.block_len + 6144;
    cbuffer = malloc(cbufferLen);

    if (cbuffer == NULL) return -1;

    /* let the caching system pull its weight! */
    if (block - blockAlign <= h->lzx_last_block && block >= h->lzx_last_block) blockAlign = (block - h->lzx_last_block);

    /* check if we need previous blocks */
    if (blockAlign != 0) {
        /* fetch all required previous blocks since last reset */
        for (i = blockAlign; i > 0; i--) {
            uint32_t curBlockIdx = block - i;

            /* check if we most recently decompressed the previous block */
            if (h->lzx_last_block != (int)curBlockIdx) {
                if ((curBlockIdx % h->reset_blkcount) == 0) {
                    LZXreset(h->lzx_state);
                }

                indexSlot = (int)((curBlockIdx) % h->cache_num_blocks);
                if (!h->cache_blocks[indexSlot])
                    h->cache_blocks[indexSlot] = (uint8_t*)malloc((unsigned int)(h->reset_table.block_len));
                if (!h->cache_blocks[indexSlot]) {
                    free(cbuffer);
                    return -1;
                }
                h->cache_block_indices[indexSlot] = curBlockIdx;
                lbuffer = h->cache_blocks[indexSlot];

                /* decompress the previous block */
                if (!_chm_get_cmpblock_bounds(h, curBlockIdx, &cmpStart, &cmpLen) || cmpLen < 0 ||
                    cmpLen > cbufferLen || _chm_fetch_bytes(h, cbuffer, cmpStart, cmpLen) != cmpLen ||
                    LZXdecompress(h->lzx_state, cbuffer, lbuffer, (int)cmpLen, (int)h->reset_table.block_len) !=
                        DECR_OK) {
                    free(cbuffer);
                    return (int64_t)0;
                }

                h->lzx_last_block = (int)curBlockIdx;
            }
        }
    } else {
        if ((block % h->reset_blkcount) == 0) {
            LZXreset(h->lzx_state);
        }
    }

    /* SumatraPDF: prevent division by zero */
    /* https://github.com/sumatrapdfreader/sumatrapdf/issues/5246 */
    if (h->cache_num_blocks == 0) {
        free(cbuffer);
        return -1;
    }

    /* allocate slot in cache */
    indexSlot = (int)(block % h->cache_num_blocks);
    if (!h->cache_blocks[indexSlot])
        h->cache_blocks[indexSlot] = (uint8_t*)malloc(((unsigned int)h->reset_table.block_len));
    if (!h->cache_blocks[indexSlot]) {
        free(cbuffer);
        return -1;
    }
    h->cache_block_indices[indexSlot] = block;
    lbuffer = h->cache_blocks[indexSlot];
    *ubuffer = lbuffer;

    /* decompress the block we actually want */
    ok = _chm_get_cmpblock_bounds(h, block, &cmpStart, &cmpLen);
    if (!ok || cmpLen > cbufferLen || _chm_fetch_bytes(h, cbuffer, cmpStart, cmpLen) != cmpLen ||
        LZXdecompress(h->lzx_state, cbuffer, lbuffer, (int)cmpLen, (int)h->reset_table.block_len) != DECR_OK) {
        free(cbuffer);
        return (int64_t)0;
    }
    h->lzx_last_block = (int)block;

    /* XXX: modify LZX routines to return the length of the data they
     * decompressed and return that instead, for an extra sanity check.
     */
    free(cbuffer);
    return h->reset_table.block_len;
}

/* grab a region from a compressed block */
static int64_t _chm_decompress_region(struct chmFile* h, uint8_t* buf, uint64_t start, int64_t len) {
    uint64_t nBlock, nOffset;
    uint64_t nLen;
    uint64_t gotLen;
    uint8_t* ubuffer;

    if (len <= 0) return (int64_t)0;

    /* SumatraPDF: prevent division by zero */
    /* https://github.com/sumatrapdfreader/sumatrapdf/issues/5246 */
    if (h->reset_table.block_len == 0) {
        return (int64_t)0;
    }

    /* figure out what we need to read */
    nBlock = start / h->reset_table.block_len;
    nOffset = start % h->reset_table.block_len;
    nLen = len;
    if (nLen > (h->reset_table.block_len - nOffset)) nLen = h->reset_table.block_len - nOffset;

    /* SumatraPDF: seen in a crash report */
    if (h->cache_num_blocks > 0) {
        /* if block is cached, return data from it. */
        if (h->cache_block_indices[nBlock % h->cache_num_blocks] == nBlock &&
            h->cache_blocks[nBlock % h->cache_num_blocks] != NULL) {
            memcpy(buf, h->cache_blocks[nBlock % h->cache_num_blocks] + nOffset, (unsigned int)nLen);
            return nLen;
        }
    }

    /* data request not satisfied, so... start up the decompressor machine */
    if (!h->lzx_state) {
        int window_size = ffs(h->window_size) - 1;
        h->lzx_last_block = -1;
        h->lzx_state = LZXinit(window_size);
    }

    /* SumatraPDF: prevent division by zero in _chm_decompress_block */
    /* https://github.com/sumatrapdfreader/sumatrapdf/issues/5246 */
    if (h->reset_blkcount == 0) {
        return (int64_t)0;
    }

    /* decompress some data */
    gotLen = _chm_decompress_block(h, nBlock, &ubuffer);
    /* SumatraPDF: check return value */
    if (gotLen == (uint64_t)-1) {
        return 0;
    }
    if (gotLen < nLen) nLen = gotLen;
    memcpy(buf, ubuffer + nOffset, (unsigned int)nLen);
    return nLen;
}

/* retrieve (part of) an object */
int64_t chm_retrieve_object(struct chmFile* h, struct chmUnitInfo* ui, uint8_t* buf, uint64_t addr, int64_t len) {
    uint64_t offset;

    /* must be valid file handle */
    if (h == NULL || ui == NULL || buf == NULL) return (int64_t)0;
    if (len <= 0) return (int64_t)0;

    /* starting address must be in correct range */
    if (addr >= ui->length) return (int64_t)0;

    /* clip length */
    if ((uint64_t)len > ui->length - addr) len = (int64_t)(ui->length - addr);

    /* if the file is uncompressed, it's simple */
    if (ui->space == CHM_UNCOMPRESSED) {
        /* read data */
        if (!_chm_get_object_offset(h, ui, addr, len, &offset)) return (int64_t)0;
        return _chm_fetch_bytes(h, buf, offset, len);
    }

    /* else if the file is compressed, it's a little trickier */
    else /* ui->space == CHM_COMPRESSED */
    {
        int64_t swath = 0, total = 0;

        /* if compression is not enabled for this file... */
        if (!h->compression_enabled) return total;

        do {
            if (!_chm_add_u64(ui->start, addr, &offset)) return total;

            /* swill another mouthful */
            swath = _chm_decompress_region(h, buf, offset, len);

            /* if we didn't get any... */
            if (swath == 0) return total;

            /* update stats */
            total += swath;
            len -= swath;
            addr += swath;
            buf += swath;

        } while (len != 0);

        return total;
    }
}

struct chmEnumerateState {
    CHM_ENUMERATOR e;
    void* context;
    int type_bits;
    int filter_bits;
};

static int _chm_enumerate_page_entry(struct chmFile* h, struct chmUnitInfo* ui, void* data) {
    struct chmEnumerateState* state = (struct chmEnumerateState*)data;

    _chm_set_entry_flags(ui);
    if (!(state->type_bits & ui->flags)) return CHM_ENUMERATOR_CONTINUE;
    if (state->filter_bits && !(state->filter_bits & ui->flags)) return CHM_ENUMERATOR_CONTINUE;
    return (*state->e)(h, ui, state->context);
}

/* enumerate the objects in the .chm archive */
int chm_enumerate(struct chmFile* h, int what, CHM_ENUMERATOR e, void* context) {
    struct chmEnumerateState state;

    state.e = e;
    state.context = context;
    state.type_bits = (what & 0x7);
    state.filter_bits = (what & 0xF8);
    return _chm_walk_pmgl_pages(h, h->index_head, _chm_enumerate_page_entry, &state);
}

struct chmEnumerateDirState {
    CHM_ENUMERATOR e;
    void* context;
    int type_bits;
    int filter_bits;
    int it_has_begun;
    char prefixRectified[CHM_MAX_PATHLEN + 1];
    int prefixLen;
    char lastPath[CHM_MAX_PATHLEN + 1];
    int lastPathLen;
};

static int _chm_enumerate_dir_page_entry(struct chmFile* h, struct chmUnitInfo* ui, void* data) {
    struct chmEnumerateDirState* state = (struct chmEnumerateDirState*)data;

    if (!state->it_has_begun) {
        if (ui->length == 0 && strncasecmp(ui->path, state->prefixRectified, state->prefixLen) == 0)
            state->it_has_begun = 1;
        else
            return CHM_ENUMERATOR_CONTINUE;

        if (ui->path[state->prefixLen] == '\0') return CHM_ENUMERATOR_CONTINUE;
    } else {
        if (strncasecmp(ui->path, state->prefixRectified, state->prefixLen) != 0) return CHM_ENUMERATOR_SUCCESS;
    }

    if (state->lastPathLen != -1) {
        if (strncasecmp(ui->path, state->lastPath, state->lastPathLen) == 0) return CHM_ENUMERATOR_CONTINUE;
    }
    strncpy(state->lastPath, ui->path, CHM_MAX_PATHLEN);
    state->lastPath[CHM_MAX_PATHLEN] = '\0';
    state->lastPathLen = strlen(state->lastPath);

    _chm_set_entry_flags(ui);
    if (!(state->type_bits & ui->flags)) return CHM_ENUMERATOR_CONTINUE;
    if (state->filter_bits && !(state->filter_bits & ui->flags)) return CHM_ENUMERATOR_CONTINUE;
    return (*state->e)(h, ui, state->context);
}

int chm_enumerate_dir(struct chmFile* h, const char* prefix, int what, CHM_ENUMERATOR e, void* context) {
    /*
     * XXX: do this efficiently (i.e. using the tree index)
     */

    struct chmEnumerateDirState state;

    state.e = e;
    state.context = context;
    state.type_bits = (what & 0x7);
    state.filter_bits = (what & 0xF8);
    state.it_has_begun = 0;
    state.lastPathLen = -1;

    strncpy(state.prefixRectified, prefix, CHM_MAX_PATHLEN);
    state.prefixRectified[CHM_MAX_PATHLEN] = '\0';
    state.prefixLen = strlen(state.prefixRectified);
    if (state.prefixLen != 0) {
        if (state.prefixRectified[state.prefixLen - 1] != '/') {
            state.prefixRectified[state.prefixLen] = '/';
            state.prefixRectified[state.prefixLen + 1] = '\0';
            ++state.prefixLen;
        }
    }
    state.lastPath[0] = '\0';

    return _chm_walk_pmgl_pages(h, h->index_head, _chm_enumerate_dir_page_entry, &state);
}

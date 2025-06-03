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
    if (count <= 0 || (unsigned int)count > *pLenRemain)
        return 0;
    memcpy(dest, (*pData), count);
    *pData += count;
    *pLenRemain -= count;
    return 1;
}

static int _unmarshal_uchar_array(uint8_t** pData, unsigned int* pLenRemain, uint8_t* dest, int count) {
    if (count <= 0 || (unsigned int)count > *pLenRemain)
        return 0;
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
    if (4 > *pLenRemain)
        return 0;
    *dest = (*pData)[0] | (*pData)[1] << 8 | (*pData)[2] << 16 | (*pData)[3] << 24;
    *pData += 4;
    *pLenRemain -= 4;
    return 1;
}

static int _unmarshal_uint32(uint8_t** pData, unsigned int* pLenRemain, uint32_t* dest) {
    if (4 > *pLenRemain)
        return 0;
    *dest = (*pData)[0] | (*pData)[1] << 8 | (*pData)[2] << 16 | (*pData)[3] << 24;
    *pData += 4;
    *pLenRemain -= 4;
    return 1;
}

static int _unmarshal_int64(uint8_t** pData, unsigned int* pLenRemain, int64_t* dest) {
    int64_t temp;
    int i;
    if (8 > *pLenRemain)
        return 0;
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
    if (8 > *pLenRemain)
        return 0;
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
    if (*pDataLen != _CHM_ITSF_V2_LEN && *pDataLen != _CHM_ITSF_V3_LEN)
        return 0;

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
    if (memcmp(dest->signature, "ITSF", 4) != 0)
        return 0;
    if (dest->version == 2) {
        if (dest->header_len < _CHM_ITSF_V2_LEN)
            return 0;
    } else if (dest->version == 3) {
        if (dest->header_len < _CHM_ITSF_V3_LEN)
            return 0;
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
    if (dest->dir_offset > UINT_MAX || dest->dir_len > UINT_MAX)
        return 0;

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
    if (*pDataLen != _CHM_ITSP_V1_LEN)
        return 0;

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
    if (memcmp(dest->signature, "ITSP", 4) != 0)
        return 0;
    if (dest->version != 1)
        return 0;
    if (dest->header_len != _CHM_ITSP_V1_LEN)
        return 0;
    /* SumatraPDF: sanity check */
    if (dest->block_len == 0)
        return 0;

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
    if (*pDataLen != _CHM_PMGL_LEN)
        return 0;
    /* SumatraPDF: sanity check */
    if (blockLen < _CHM_PMGL_LEN)
        return 0;

    /* unmarshal fields */
    _unmarshal_char_array(pData, pDataLen, dest->signature, 4);
    _unmarshal_uint32(pData, pDataLen, &dest->free_space);
    _unmarshal_uint32(pData, pDataLen, &dest->unknown_0008);
    _unmarshal_int32(pData, pDataLen, &dest->block_prev);
    _unmarshal_int32(pData, pDataLen, &dest->block_next);

    /* check structure */
    if (memcmp(dest->signature, _chm_pmgl_marker, 4) != 0)
        return 0;
    /* SumatraPDF: sanity check */
    if (dest->free_space > blockLen - _CHM_PMGL_LEN)
        return 0;

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
    if (*pDataLen != _CHM_PMGI_LEN)
        return 0;
    /* SumatraPDF: sanity check */
    if (blockLen < _CHM_PMGI_LEN)
        return 0;

    /* unmarshal fields */
    _unmarshal_char_array(pData, pDataLen, dest->signature, 4);
    _unmarshal_uint32(pData, pDataLen, &dest->free_space);

    /* check structure */
    if (memcmp(dest->signature, _chm_pmgi_marker, 4) != 0)
        return 0;
    /* SumatraPDF: sanity check */
    if (dest->free_space > blockLen - _CHM_PMGI_LEN)
        return 0;

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
    if (*pDataLen != _CHM_LZXC_RESETTABLE_V1_LEN)
        return 0;

    /* unmarshal fields */
    _unmarshal_uint32(pData, pDataLen, &dest->version);
    _unmarshal_uint32(pData, pDataLen, &dest->block_count);
    _unmarshal_uint32(pData, pDataLen, &dest->unknown);
    _unmarshal_uint32(pData, pDataLen, &dest->table_offset);
    _unmarshal_uint64(pData, pDataLen, &dest->uncompressed_len);
    _unmarshal_uint64(pData, pDataLen, &dest->compressed_len);
    _unmarshal_uint64(pData, pDataLen, &dest->block_len);

    /* check structure */
    if (dest->version != 2)
        return 0;
    /* SumatraPDF: sanity check (huge values are usually due to broken files) */
    if (dest->uncompressed_len > INT_MAX || dest->compressed_len > INT_MAX)
        return 0;
    if (dest->block_len == 0 || dest->block_len > INT_MAX)
        return 0;

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
    if (*pDataLen < _CHM_LZXC_MIN_LEN)
        return 0;

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
    if (dest->windowSize == 0 || dest->resetInterval == 0)
        return 0;

    /* for now, only support resetInterval a multiple of windowSize/2 */
    if (dest->windowSize == 1)
        return 0;
    if ((dest->resetInterval % (dest->windowSize / 2)) != 0)
        return 0;

    /* check structure */
    if (memcmp(dest->signature, "LZXC", 4) != 0)
        return 0;

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
};

static int64_t _chm_fetch_bytes(struct chmFile* h, uint8_t* buf, uint64_t os, int64_t len) {
    if (os + len > h->data_len) {
        return 0;
    }
    if (os + len > h->data_len) {
        len = h->data_len - os;
    }
    memcpy(buf, h->data + os, len);
    return len;
}

/* open an ITS archive */
struct chmFile* chm_open(const char* d, size_t len) {
    uint8_t sbuffer[256];
    unsigned int sremain;
    uint8_t* sbufpos;
    struct chmFile* newHandle = NULL;
    struct chmItsfHeader itsfHeader;
    struct chmItspHeader itspHeader;
#if 0
    struct chmUnitInfo          uiSpan;
#endif
    struct chmUnitInfo uiLzxc = {0};
    struct chmLzxcControlData ctlData;

    /* allocate handle */
    newHandle = (struct chmFile*)malloc(sizeof(struct chmFile));
    if (newHandle == NULL)
        return NULL;
    newHandle->data = d;
    newHandle->data_len = len;
    newHandle->lzx_state = NULL;
    newHandle->cache_blocks = NULL;
    newHandle->cache_block_indices = NULL;
    newHandle->cache_num_blocks = 0;

    /* read and verify header */
    sremain = _CHM_ITSF_V3_LEN;
    sbufpos = sbuffer;
    if (_chm_fetch_bytes(newHandle, sbuffer, (uint64_t)0, sremain) != sremain ||
        !_unmarshal_itsf_header(&sbufpos, &sremain, &itsfHeader)) {
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
    if (_chm_fetch_bytes(newHandle, sbuffer, (uint64_t)itsfHeader.dir_offset, sremain) != sremain ||
        !_unmarshal_itsp_header(&sbufpos, &sremain, &itspHeader)) {
        chm_close(newHandle);
        return NULL;
    }

    /* grab essential information from ITSP header */
    newHandle->dir_offset += itspHeader.header_len;
    newHandle->dir_len -= itspHeader.header_len;
    newHandle->index_root = itspHeader.index_root;
    newHandle->index_head = itspHeader.index_head;
    newHandle->block_len = itspHeader.block_len;

    /* if the index root is -1, this means we don't have any PMGI blocks.
     * as a result, we must use the sole PMGL block as the index root
     */
    if (newHandle->index_root <= -1)
        newHandle->index_root = newHandle->index_head;

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
        if (chm_retrieve_object(newHandle, &newHandle->rt_unit, sbuffer, 0, sremain) != sremain ||
            !_unmarshal_lzxc_reset_table(&sbufpos, &sremain, &newHandle->reset_table)) {
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
    if (h->lzx_state)
        LZXteardown(h->lzx_state);
    h->lzx_state = NULL;

    if (h->cache_blocks) {
        int i;
        for (i = 0; i < h->cache_num_blocks; i++) {
            if (h->cache_blocks[i])
                free(h->cache_blocks[i]);
        }
        free(h->cache_blocks);
        h->cache_blocks = NULL;
    }

    if (h->cache_block_indices)
        free(h->cache_block_indices);
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
                if (newBlocks == NULL)
                    return;
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
static void _chm_skip_cword(uint8_t** pEntry) {
    while (*(*pEntry)++ >= 0x80)
        ;
}

/* skip the data from a PMGL entry */
static void _chm_skip_PMGL_entry_data(uint8_t** pEntry) {
    _chm_skip_cword(pEntry);
    _chm_skip_cword(pEntry);
    _chm_skip_cword(pEntry);
}

/* parse a compressed dword */
static uint64_t _chm_parse_cword(uint8_t** pEntry) {
    uint64_t accum = 0;
    uint8_t temp;
    while ((temp = *(*pEntry)++) >= 0x80) {
        accum <<= 7;
        accum += temp & 0x7f;
    }

    return (accum << 7) + temp;
}

/* parse a utf-8 string into an ASCII char buffer */
static int _chm_parse_UTF8(uint8_t** pEntry, uint64_t count, char* path) {
    /* XXX: implement UTF-8 support, including a real mapping onto
     *      ISO-8859-1?  probably there is a library to do this?  As is
     *      immediately apparent from the below code, I'm presently not doing
     *      any special handling for files in which none of the strings contain
     *      UTF-8 multi-byte characters.
     */
    while (count != 0) {
        *path++ = (char)(*(*pEntry)++);
        --count;
    }

    *path = '\0';
    return 1;
}

/* parse a PMGL entry into a chmUnitInfo struct; return 1 on success. */
static int _chm_parse_PMGL_entry(uint8_t** pEntry, struct chmUnitInfo* ui) {
    uint64_t strLen;

    /* parse str len */
    strLen = _chm_parse_cword(pEntry);
    if (strLen > CHM_MAX_PATHLEN)
        return 0;

    /* parse path */
    if (!_chm_parse_UTF8(pEntry, strLen, ui->path))
        return 0;

    /* parse info */
    ui->space = (int)_chm_parse_cword(pEntry);
    ui->start = _chm_parse_cword(pEntry);
    ui->length = _chm_parse_cword(pEntry);
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
    if (!_unmarshal_pmgl_header(&cur, &hremain, block_len, &header))
        return NULL;
    end = page_buf + block_len - (header.free_space);

    /* now, scan progressively */
    while (cur < end) {
        /* grab the name */
        temp = cur;
        strLen = _chm_parse_cword(&cur);
        if (strLen > CHM_MAX_PATHLEN)
            return NULL;
        if (!_chm_parse_UTF8(&cur, strLen, buffer))
            return NULL;

        /* check if it is the right name */
        if (!_stricmp(buffer, objPath))
            return temp;

        _chm_skip_PMGL_entry_data(&cur);
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
    if (!_unmarshal_pmgi_header(&cur, &hremain, block_len, &header))
        return -1;
    end = page_buf + block_len - (header.free_space);

    /* now, scan progressively */
    while (cur < end) {
        /* grab the name */
        strLen = _chm_parse_cword(&cur);
        if (strLen > CHM_MAX_PATHLEN)
            return -1;
        if (!_chm_parse_UTF8(&cur, strLen, buffer))
            return -1;

        /* check if it is the right name */
        if (strcasecmp(buffer, objPath) > 0)
            return page;

        /* load next value for path */
        page = (int)_chm_parse_cword(&cur);
    }

    return page;
}

/* resolve a particular object from the archive */
int chm_resolve_object(struct chmFile* h, const char* objPath, struct chmUnitInfo* ui) {
    /*
     * XXX: implement caching scheme for dir pages
     */

    int32_t curPage;

    /* buffer to hold whatever page we're looking at */
    /* RWE 6/12/2003 */
    uint8_t* page_buf = malloc(h->block_len);
    if (page_buf == NULL)
        return CHM_RESOLVE_FAILURE;

    /* starting page */
    curPage = h->index_root;

    /* until we have either returned or given up */
    while (curPage != -1) {
        /* try to fetch the index page */
        if (_chm_fetch_bytes(h, page_buf, (uint64_t)h->dir_offset + (uint64_t)curPage * h->block_len, h->block_len) !=
            h->block_len) {
            free(page_buf);
            return CHM_RESOLVE_FAILURE;
        }

        /* now, if it is a leaf node: */
        if (memcmp(page_buf, _chm_pmgl_marker, 4) == 0) {
            /* scan block */
            uint8_t* pEntry = _chm_find_in_PMGL(page_buf, h->block_len, objPath);
            if (pEntry == NULL) {
                free(page_buf);
                return CHM_RESOLVE_FAILURE;
            }

            /* parse entry and return */
            _chm_parse_PMGL_entry(&pEntry, ui);
            free(page_buf);
            return CHM_RESOLVE_SUCCESS;
        }

        /* else, if it is a branch node: */
        else if (memcmp(page_buf, _chm_pmgi_marker, 4) == 0)
            curPage = _chm_find_in_PMGI(page_buf, h->block_len, objPath);

        /* else, we are confused.  give up. */
        else {
            free(page_buf);
            return CHM_RESOLVE_FAILURE;
        }
    }

    /* didn't find anything.  fail. */
    free(page_buf);
    return CHM_RESOLVE_FAILURE;
}

/*
 * utility methods for dealing with compressed data
 */

/* get the bounds of a compressed block.  return 0 on failure */
static int _chm_get_cmpblock_bounds(struct chmFile* h, uint64_t block, uint64_t* start, int64_t* len) {
    uint8_t buffer[8], *dummy;
    unsigned int remain;

    /* for all but the last block, use the reset table */
    if (block < h->reset_table.block_count - 1) {
        /* unpack the start address */
        dummy = buffer;
        remain = 8;
        if (_chm_fetch_bytes(h, buffer,
                             (uint64_t)h->data_offset + (uint64_t)h->rt_unit.start +
                                 (uint64_t)h->reset_table.table_offset + (uint64_t)block * 8,
                             remain) != remain ||
            !_unmarshal_uint64(&dummy, &remain, start))
            return 0;

        /* unpack the end address */
        dummy = buffer;
        remain = 8;
        if (_chm_fetch_bytes(h, buffer,
                             (uint64_t)h->data_offset + (uint64_t)h->rt_unit.start +
                                 (uint64_t)h->reset_table.table_offset + (uint64_t)block * 8 + 8,
                             remain) != remain ||
            !_unmarshal_int64(&dummy, &remain, len))
            return 0;
    }

    /* for the last block, use the span in addition to the reset table */
    else {
        /* unpack the start address */
        dummy = buffer;
        remain = 8;
        if (_chm_fetch_bytes(h, buffer,
                             (uint64_t)h->data_offset + (uint64_t)h->rt_unit.start +
                                 (uint64_t)h->reset_table.table_offset + (uint64_t)block * 8,
                             remain) != remain ||
            !_unmarshal_uint64(&dummy, &remain, start))
            return 0;

        *len = h->reset_table.compressed_len;
    }

    /* compute the length and absolute start address */
    *len -= *start;
    *start += h->data_offset + h->cn_unit.start;

    return 1;
}

/* decompress the block.  must have lzx_mutex. */
static int64_t _chm_decompress_block(struct chmFile* h, uint64_t block, uint8_t** ubuffer) {
    uint8_t* cbuffer = malloc(((unsigned int)h->reset_table.block_len + 6144));
    uint64_t cmpStart;                                           /* compressed start  */
    int64_t cmpLen;                                              /* compressed len    */
    int indexSlot;                                               /* cache index slot  */
    uint8_t* lbuffer;                                            /* local buffer ptr  */
    uint32_t blockAlign = (uint32_t)(block % h->reset_blkcount); /* reset intvl. aln. */
    uint32_t i;                                                  /* local loop index  */

    if (cbuffer == NULL)
        return -1;

    /* let the caching system pull its weight! */
    if (block - blockAlign <= h->lzx_last_block && block >= h->lzx_last_block)
        blockAlign = (block - h->lzx_last_block);

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
                    cmpLen > h->reset_table.block_len + 6144 ||
                    _chm_fetch_bytes(h, cbuffer, cmpStart, cmpLen) != cmpLen ||
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
    if (!_chm_get_cmpblock_bounds(h, block, &cmpStart, &cmpLen) ||
        _chm_fetch_bytes(h, cbuffer, cmpStart, cmpLen) != cmpLen ||
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

    if (len <= 0)
        return (int64_t)0;

    /* figure out what we need to read */
    nBlock = start / h->reset_table.block_len;
    nOffset = start % h->reset_table.block_len;
    nLen = len;
    if (nLen > (h->reset_table.block_len - nOffset))
        nLen = h->reset_table.block_len - nOffset;

    /* if block is cached, return data from it. */
    if (h->cache_block_indices[nBlock % h->cache_num_blocks] == nBlock &&
        h->cache_blocks[nBlock % h->cache_num_blocks] != NULL) {
        memcpy(buf, h->cache_blocks[nBlock % h->cache_num_blocks] + nOffset, (unsigned int)nLen);
        return nLen;
    }

    /* data request not satisfied, so... start up the decompressor machine */
    if (!h->lzx_state) {
        int window_size = ffs(h->window_size) - 1;
        h->lzx_last_block = -1;
        h->lzx_state = LZXinit(window_size);
    }

    /* decompress some data */
    gotLen = _chm_decompress_block(h, nBlock, &ubuffer);
    /* SumatraPDF: check return value */
    if (gotLen == (uint64_t)-1) {
        return 0;
    }
    if (gotLen < nLen)
        nLen = gotLen;
    memcpy(buf, ubuffer + nOffset, (unsigned int)nLen);
    return nLen;
}

/* retrieve (part of) an object */
int64_t chm_retrieve_object(struct chmFile* h, struct chmUnitInfo* ui, uint8_t* buf, uint64_t addr, int64_t len) {
    /* must be valid file handle */
    if (h == NULL)
        return (int64_t)0;

    /* starting address must be in correct range */
    if (addr >= ui->length)
        return (int64_t)0;

    /* clip length */
    if (addr + len > ui->length)
        len = ui->length - addr;

    /* if the file is uncompressed, it's simple */
    if (ui->space == CHM_UNCOMPRESSED) {
        /* read data */
        return _chm_fetch_bytes(h, buf, (uint64_t)h->data_offset + (uint64_t)ui->start + (uint64_t)addr, len);
    }

    /* else if the file is compressed, it's a little trickier */
    else /* ui->space == CHM_COMPRESSED */
    {
        int64_t swath = 0, total = 0;

        /* if compression is not enabled for this file... */
        if (!h->compression_enabled)
            return total;

        do {
            /* swill another mouthful */
            swath = _chm_decompress_region(h, buf, ui->start + addr, len);

            /* if we didn't get any... */
            if (swath == 0)
                return total;

            /* update stats */
            total += swath;
            len -= swath;
            addr += swath;
            buf += swath;

        } while (len != 0);

        return total;
    }
}

/* enumerate the objects in the .chm archive */
int chm_enumerate(struct chmFile* h, int what, CHM_ENUMERATOR e, void* context) {
    int32_t curPage;

    /* buffer to hold whatever page we're looking at */
    uint8_t* page_buf = malloc((unsigned int)h->block_len);
    struct chmPmglHeader header;
    uint8_t* end;
    uint8_t* cur;
    unsigned int lenRemain;
    uint64_t ui_path_len;

    /* the current ui */
    struct chmUnitInfo ui;
    int type_bits = (what & 0x7);
    int filter_bits = (what & 0xF8);

    if (page_buf == NULL)
        return 0;

    /* starting page */
    curPage = h->index_head;

    /* until we have either returned or given up */
    while (curPage != -1) {
        /* try to fetch the index page */
        if (_chm_fetch_bytes(h, page_buf, (uint64_t)h->dir_offset + (uint64_t)curPage * h->block_len, h->block_len) !=
            h->block_len) {
            free(page_buf);
            return 0;
        }

        /* figure out start and end for this page */
        cur = page_buf;
        lenRemain = _CHM_PMGL_LEN;
        if (!_unmarshal_pmgl_header(&cur, &lenRemain, h->block_len, &header)) {
            free(page_buf);
            return 0;
        }
        end = page_buf + h->block_len - (header.free_space);

        /* loop over this page */
        while (cur < end) {
            ui.flags = 0;

            if (!_chm_parse_PMGL_entry(&cur, &ui)) {
                free(page_buf);
                return 0;
            }

            /* get the length of the path */
            ui_path_len = strlen(ui.path) - 1;

            /* check for DIRS */
            if (ui.path[ui_path_len] == '/')
                ui.flags |= CHM_ENUMERATE_DIRS;

            /* check for FILES */
            if (ui.path[ui_path_len] != '/')
                ui.flags |= CHM_ENUMERATE_FILES;

            /* check for NORMAL vs. META */
            if (ui.path[0] == '/') {
                /* check for NORMAL vs. SPECIAL */
                if (ui.path[1] == '#' || ui.path[1] == '$')
                    ui.flags |= CHM_ENUMERATE_SPECIAL;
                else
                    ui.flags |= CHM_ENUMERATE_NORMAL;
            } else
                ui.flags |= CHM_ENUMERATE_META;

            if (!(type_bits & ui.flags))
                continue;

            if (filter_bits && !(filter_bits & ui.flags))
                continue;

            /* call the enumerator */
            {
                int status = (*e)(h, &ui, context);
                switch (status) {
                    case CHM_ENUMERATOR_FAILURE:
                        free(page_buf);
                        return 0;
                    case CHM_ENUMERATOR_CONTINUE:
                        break;
                    case CHM_ENUMERATOR_SUCCESS:
                        free(page_buf);
                        return 1;
                    default:
                        break;
                }
            }
        }

        /* advance to next page */
        curPage = header.block_next;
    }

    free(page_buf);
    return 1;
}

int chm_enumerate_dir(struct chmFile* h, const char* prefix, int what, CHM_ENUMERATOR e, void* context) {
    /*
     * XXX: do this efficiently (i.e. using the tree index)
     */

    int32_t curPage;

    /* buffer to hold whatever page we're looking at */
    /* RWE 6/12/2003 */
    uint8_t* page_buf = malloc((unsigned int)h->block_len);
    struct chmPmglHeader header;
    uint8_t* end;
    uint8_t* cur;
    unsigned int lenRemain;

    /* set to 1 once we've started */
    int it_has_begun = 0;

    /* the current ui */
    struct chmUnitInfo ui;
    int type_bits = (what & 0x7);
    int filter_bits = (what & 0xF8);
    uint64_t ui_path_len;

    /* the length of the prefix */
    char prefixRectified[CHM_MAX_PATHLEN + 1];
    int prefixLen;
    char lastPath[CHM_MAX_PATHLEN + 1];
    int lastPathLen;

    if (page_buf == NULL)
        return 0;

    /* starting page */
    curPage = h->index_head;

    /* initialize pathname state */
    strncpy(prefixRectified, prefix, CHM_MAX_PATHLEN);
    prefixRectified[CHM_MAX_PATHLEN] = '\0';
    prefixLen = strlen(prefixRectified);
    if (prefixLen != 0) {
        if (prefixRectified[prefixLen - 1] != '/') {
            prefixRectified[prefixLen] = '/';
            prefixRectified[prefixLen + 1] = '\0';
            ++prefixLen;
        }
    }
    lastPath[0] = '\0';
    lastPathLen = -1;

    /* until we have either returned or given up */
    while (curPage != -1) {
        /* try to fetch the index page */
        if (_chm_fetch_bytes(h, page_buf, (uint64_t)h->dir_offset + (uint64_t)curPage * h->block_len, h->block_len) !=
            h->block_len) {
            free(page_buf);
            return 0;
        }

        /* figure out start and end for this page */
        cur = page_buf;
        lenRemain = _CHM_PMGL_LEN;
        if (!_unmarshal_pmgl_header(&cur, &lenRemain, h->block_len, &header)) {
            free(page_buf);
            return 0;
        }
        end = page_buf + h->block_len - (header.free_space);

        /* loop over this page */
        while (cur < end) {
            ui.flags = 0;

            if (!_chm_parse_PMGL_entry(&cur, &ui)) {
                free(page_buf);
                return 0;
            }

            /* check if we should start */
            if (!it_has_begun) {
                if (ui.length == 0 && strncasecmp(ui.path, prefixRectified, prefixLen) == 0)
                    it_has_begun = 1;
                else
                    continue;

                if (ui.path[prefixLen] == '\0')
                    continue;
            }

            /* check if we should stop */
            else {
                if (strncasecmp(ui.path, prefixRectified, prefixLen) != 0) {
                    free(page_buf);
                    return 1;
                }
            }

            /* check if we should include this path */
            if (lastPathLen != -1) {
                if (strncasecmp(ui.path, lastPath, lastPathLen) == 0)
                    continue;
            }
            strncpy(lastPath, ui.path, CHM_MAX_PATHLEN);
            lastPath[CHM_MAX_PATHLEN] = '\0';
            lastPathLen = strlen(lastPath);

            /* get the length of the path */
            ui_path_len = strlen(ui.path) - 1;

            /* check for DIRS */
            if (ui.path[ui_path_len] == '/')
                ui.flags |= CHM_ENUMERATE_DIRS;

            /* check for FILES */
            if (ui.path[ui_path_len] != '/')
                ui.flags |= CHM_ENUMERATE_FILES;

            /* check for NORMAL vs. META */
            if (ui.path[0] == '/') {
                /* check for NORMAL vs. SPECIAL */
                if (ui.path[1] == '#' || ui.path[1] == '$')
                    ui.flags |= CHM_ENUMERATE_SPECIAL;
                else
                    ui.flags |= CHM_ENUMERATE_NORMAL;
            } else
                ui.flags |= CHM_ENUMERATE_META;

            if (!(type_bits & ui.flags))
                continue;

            if (filter_bits && !(filter_bits & ui.flags))
                continue;

            /* call the enumerator */
            {
                int status = (*e)(h, &ui, context);
                switch (status) {
                    case CHM_ENUMERATOR_FAILURE:
                        free(page_buf);
                        return 0;
                    case CHM_ENUMERATOR_CONTINUE:
                        break;
                    case CHM_ENUMERATOR_SUCCESS:
                        free(page_buf);
                        return 1;
                    default:
                        break;
                }
            }
        }

        /* advance to next page */
        curPage = header.block_next;
    }

    free(page_buf);
    return 1;
}

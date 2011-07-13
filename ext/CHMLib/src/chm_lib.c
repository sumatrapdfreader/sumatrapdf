/* $Id: chm_lib.c,v 1.19 2003/09/07 13:01:43 jedwin Exp $ */
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
 * switches:    CHM_MT:        compile library with thread-safety          *
 *                                                                         *
 * switches (Linux only):                                                  *
 *              CHM_USE_PREAD: compile library to use pread instead of     *
 *                             lseek/read                                  *
 *              CHM_USE_IO64:  compile library to support full 64-bit I/O  *
 *                             as is needed to properly deal with the      *
 *                             64-bit file offsets.                        *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include "chm_lib.h"

#ifdef CHM_MT
#define _REENTRANT
#endif

#include "lzx.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef CHM_DEBUG
#include <stdio.h>
#endif

#if __sun || __sgi
#include <strings.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <malloc.h>
#ifdef _WIN32_WCE
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif
#else
/* basic Linux system includes */
#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* #include <dmalloc.h> */
#endif

/* includes/defines for threading, if using them */
#ifdef CHM_MT
#ifdef WIN32
#define CHM_ACQUIRE_LOCK(a) do {                        \
        EnterCriticalSection(&(a));                     \
    } while(0)
#define CHM_RELEASE_LOCK(a) do {                        \
        EnterCriticalSection(&(a));                     \
    } while(0)

#else
#include <pthread.h>

#define CHM_ACQUIRE_LOCK(a) do {                        \
        pthread_mutex_lock(&(a));                       \
    } while(0)
#define CHM_RELEASE_LOCK(a) do {                        \
        pthread_mutex_unlock(&(a));                     \
    } while(0)

#endif
#else
#define CHM_ACQUIRE_LOCK(a) /* do nothing */
#define CHM_RELEASE_LOCK(a) /* do nothing */
#endif

#ifdef WIN32
#define CHM_NULL_FD (INVALID_HANDLE_VALUE)
#define CHM_USE_WIN32IO 1
#define CHM_CLOSE_FILE(fd) CloseHandle((fd))
#else
#define CHM_NULL_FD (-1)
#define CHM_CLOSE_FILE(fd) close((fd))
#endif

/*
 * defines related to tuning
 */
#ifndef CHM_MAX_BLOCKS_CACHED
#define CHM_MAX_BLOCKS_CACHED 5
#endif

/*
 * architecture specific defines
 *
 * Note: as soon as C99 is more widespread, the below defines should
 * probably just use the C99 sized-int types.
 *
 * The following settings will probably work for many platforms.  The sizes
 * don't have to be exactly correct, but the types must accommodate at least as
 * many bits as they specify.
 */

/* i386, 32-bit, Windows */
#ifdef WIN32
typedef unsigned char           UChar;
typedef __int16                 Int16;
typedef unsigned __int16        UInt16;
typedef __int32                 Int32;
typedef unsigned __int32        UInt32;
typedef __int64                 Int64;
typedef unsigned __int64        UInt64;

/* x86-64 */
/* Note that these may be appropriate for other 64-bit machines. */
#elif defined(__LP64__)
typedef unsigned char           UChar;
typedef short                   Int16;
typedef unsigned short          UInt16;
typedef int                     Int32;
typedef unsigned int            UInt32;
typedef long                    Int64;
typedef unsigned long           UInt64;

/* I386, 32-bit, non-Windows */
/* Sparc        */
/* MIPS         */
/* PPC          */
#else
typedef unsigned char           UChar;
typedef short                   Int16;
typedef unsigned short          UInt16;
typedef long                    Int32;
typedef unsigned long           UInt32;
typedef long long               Int64;
typedef unsigned long long      UInt64;
#endif

/* GCC */
#ifdef __GNUC__
#define memcmp __builtin_memcmp
#define memcpy __builtin_memcpy
#define strlen __builtin_strlen

#elif defined(WIN32)
static int ffs(unsigned int val)
{
    int bit=1, idx=1;
    while (bit != 0  &&  (val & bit) == 0)
    {
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
static int _unmarshal_char_array(unsigned char **pData,
                                 unsigned int *pLenRemain,
                                 char *dest,
                                 int count)
{
    if (count <= 0  ||  (unsigned int)count > *pLenRemain)
        return 0;
    memcpy(dest, (*pData), count);
    *pData += count;
    *pLenRemain -= count;
    return 1;
}

static int _unmarshal_uchar_array(unsigned char **pData,
                                  unsigned int *pLenRemain,
                                  unsigned char *dest,
                                  int count)
{
        if (count <= 0  ||  (unsigned int)count > *pLenRemain)
        return 0;
    memcpy(dest, (*pData), count);
    *pData += count;
    *pLenRemain -= count;
    return 1;
}

#if 0
static int _unmarshal_int16(unsigned char **pData,
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

static int _unmarshal_uint16(unsigned char **pData,
                             unsigned int *pLenRemain,
                             UInt16 *dest)
{
    if (2 > *pLenRemain)
        return 0;
    *dest = (*pData)[0] | (*pData)[1]<<8;
    *pData += 2;
    *pLenRemain -= 2;
    return 1;
}
#endif

static int _unmarshal_int32(unsigned char **pData,
                            unsigned int *pLenRemain,
                            Int32 *dest)
{
    if (4 > *pLenRemain)
        return 0;
    *dest = (*pData)[0] | (*pData)[1]<<8 | (*pData)[2]<<16 | (*pData)[3]<<24;
    *pData += 4;
    *pLenRemain -= 4;
    return 1;
}

static int _unmarshal_uint32(unsigned char **pData,
                             unsigned int *pLenRemain,
                             UInt32 *dest)
{
    if (4 > *pLenRemain)
        return 0;
    *dest = (*pData)[0] | (*pData)[1]<<8 | (*pData)[2]<<16 | (*pData)[3]<<24;
    *pData += 4;
    *pLenRemain -= 4;
    return 1;
}

static int _unmarshal_int64(unsigned char **pData,
                            unsigned int *pLenRemain,
                            Int64 *dest)
{
    Int64 temp;
    int i;
    if (8 > *pLenRemain)
        return 0;
    temp=0;
    for(i=8; i>0; i--)
    {
        temp <<= 8;
        temp |= (*pData)[i-1];
    }
    *dest = temp;
    *pData += 8;
    *pLenRemain -= 8;
    return 1;
}

static int _unmarshal_uint64(unsigned char **pData,
                             unsigned int *pLenRemain,
                             UInt64 *dest)
{
    UInt64 temp;
    int i;
    if (8 > *pLenRemain)
        return 0;
    temp=0;
    for(i=8; i>0; i--)
    {
        temp <<= 8;
        temp |= (*pData)[i-1];
    }
    *dest = temp;
    *pData += 8;
    *pLenRemain -= 8;
    return 1;
}

static int _unmarshal_uuid(unsigned char **pData,
                           unsigned int *pDataLen,
                           unsigned char *dest)
{
    return _unmarshal_uchar_array(pData, pDataLen, dest, 16);
}

/* names of sections essential to decompression */
static const char _CHMU_RESET_TABLE[] =
        "::DataSpace/Storage/MSCompressed/Transform/"
        "{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/"
        "InstanceData/ResetTable";
static const char _CHMU_LZXC_CONTROLDATA[] =
        "::DataSpace/Storage/MSCompressed/ControlData";
static const char _CHMU_CONTENT[] =
        "::DataSpace/Storage/MSCompressed/Content";
static const char _CHMU_SPANINFO[] =
        "::DataSpace/Storage/MSCompressed/SpanInfo";

/*
 * structures local to this module
 */

/* structure of ITSF headers */
#define _CHM_ITSF_V2_LEN (0x58)
#define _CHM_ITSF_V3_LEN (0x60)
struct chmItsfHeader
{
    char        signature[4];           /*  0 (ITSF) */
    Int32       version;                /*  4 */
    Int32       header_len;             /*  8 */
    Int32       unknown_000c;           /*  c */
    UInt32      last_modified;          /* 10 */
    UInt32      lang_id;                /* 14 */
    UChar       dir_uuid[16];           /* 18 */
    UChar       stream_uuid[16];        /* 28 */
    UInt64      unknown_offset;         /* 38 */
    UInt64      unknown_len;            /* 40 */
    UInt64      dir_offset;             /* 48 */
    UInt64      dir_len;                /* 50 */
    UInt64      data_offset;            /* 58 (Not present before V3) */
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_itsf_header(unsigned char **pData,
                                  unsigned int *pDataLen,
                                  struct chmItsfHeader *dest)
{
    /* we only know how to deal with the 0x58 and 0x60 byte structures */
    if (*pDataLen != _CHM_ITSF_V2_LEN  &&  *pDataLen != _CHM_ITSF_V3_LEN)
        return 0;

    /* unmarshal common fields */
    _unmarshal_char_array(pData, pDataLen,  dest->signature, 4);
    _unmarshal_int32     (pData, pDataLen, &dest->version);
    _unmarshal_int32     (pData, pDataLen, &dest->header_len);
    _unmarshal_int32     (pData, pDataLen, &dest->unknown_000c);
    _unmarshal_uint32    (pData, pDataLen, &dest->last_modified);
    _unmarshal_uint32    (pData, pDataLen, &dest->lang_id);
    _unmarshal_uuid      (pData, pDataLen,  dest->dir_uuid);
    _unmarshal_uuid      (pData, pDataLen,  dest->stream_uuid);
    _unmarshal_uint64    (pData, pDataLen, &dest->unknown_offset);
    _unmarshal_uint64    (pData, pDataLen, &dest->unknown_len);
    _unmarshal_uint64    (pData, pDataLen, &dest->dir_offset);
    _unmarshal_uint64    (pData, pDataLen, &dest->dir_len);

    /* error check the data */
    /* XXX: should also check UUIDs, probably, though with a version 3 file,
     * current MS tools do not seem to use them.
     */
    if (memcmp(dest->signature, "ITSF", 4) != 0)
        return 0;
    if (dest->version == 2)
    {
        if (dest->header_len < _CHM_ITSF_V2_LEN)
            return 0;
    }
    else if (dest->version == 3)
    {
        if (dest->header_len < _CHM_ITSF_V3_LEN)
            return 0;
    }
    else
        return 0;

    /* now, if we have a V3 structure, unmarshal the rest.
     * otherwise, compute it
     */
    if (dest->version == 3)
    {
        if (*pDataLen != 0)
            _unmarshal_uint64(pData, pDataLen, &dest->data_offset);
        else
            return 0;
    }
    else
        dest->data_offset = dest->dir_offset + dest->dir_len;

    return 1;
}

/* structure of ITSP headers */
#define _CHM_ITSP_V1_LEN (0x54)
struct chmItspHeader
{
    char        signature[4];           /*  0 (ITSP) */
    Int32       version;                /*  4 */
    Int32       header_len;             /*  8 */
    Int32       unknown_000c;           /*  c */
    UInt32      block_len;              /* 10 */
    Int32       blockidx_intvl;         /* 14 */
    Int32       index_depth;            /* 18 */
    Int32       index_root;             /* 1c */
    Int32       index_head;             /* 20 */
    Int32       unknown_0024;           /* 24 */
    UInt32      num_blocks;             /* 28 */
    Int32       unknown_002c;           /* 2c */
    UInt32      lang_id;                /* 30 */
    UChar       system_uuid[16];        /* 34 */
    UChar       unknown_0044[16];       /* 44 */
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_itsp_header(unsigned char **pData,
                                  unsigned int *pDataLen,
                                  struct chmItspHeader *dest)
{
    /* we only know how to deal with a 0x54 byte structures */
    if (*pDataLen != _CHM_ITSP_V1_LEN)
        return 0;

    /* unmarshal fields */
    _unmarshal_char_array(pData, pDataLen,  dest->signature, 4);
    _unmarshal_int32     (pData, pDataLen, &dest->version);
    _unmarshal_int32     (pData, pDataLen, &dest->header_len);
    _unmarshal_int32     (pData, pDataLen, &dest->unknown_000c);
    _unmarshal_uint32    (pData, pDataLen, &dest->block_len);
    _unmarshal_int32     (pData, pDataLen, &dest->blockidx_intvl);
    _unmarshal_int32     (pData, pDataLen, &dest->index_depth);
    _unmarshal_int32     (pData, pDataLen, &dest->index_root);
    _unmarshal_int32     (pData, pDataLen, &dest->index_head);
    _unmarshal_int32     (pData, pDataLen, &dest->unknown_0024);
    _unmarshal_uint32    (pData, pDataLen, &dest->num_blocks);
    _unmarshal_int32     (pData, pDataLen, &dest->unknown_002c);
    _unmarshal_uint32    (pData, pDataLen, &dest->lang_id);
    _unmarshal_uuid      (pData, pDataLen,  dest->system_uuid);
    _unmarshal_uchar_array(pData, pDataLen, dest->unknown_0044, 16);

    /* error check the data */
    if (memcmp(dest->signature, "ITSP", 4) != 0)
        return 0;
    if (dest->version != 1)
        return 0;
    if (dest->header_len != _CHM_ITSP_V1_LEN)
        return 0;

    return 1;
}

/* structure of PMGL headers */
static const char _chm_pmgl_marker[4] = "PMGL";
#define _CHM_PMGL_LEN (0x14)
struct chmPmglHeader
{
    char        signature[4];           /*  0 (PMGL) */
    UInt32      free_space;             /*  4 */
    UInt32      unknown_0008;           /*  8 */
    Int32       block_prev;             /*  c */
    Int32       block_next;             /* 10 */
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_pmgl_header(unsigned char **pData,
                                  unsigned int *pDataLen,
                                  struct chmPmglHeader *dest)
{
    /* we only know how to deal with a 0x14 byte structures */
    if (*pDataLen != _CHM_PMGL_LEN)
        return 0;

    /* unmarshal fields */
    _unmarshal_char_array(pData, pDataLen,  dest->signature, 4);
    _unmarshal_uint32    (pData, pDataLen, &dest->free_space);
    _unmarshal_uint32    (pData, pDataLen, &dest->unknown_0008);
    _unmarshal_int32     (pData, pDataLen, &dest->block_prev);
    _unmarshal_int32     (pData, pDataLen, &dest->block_next);

    /* check structure */
    if (memcmp(dest->signature, _chm_pmgl_marker, 4) != 0)
        return 0;

    return 1;
}

/* structure of PMGI headers */
static const char _chm_pmgi_marker[4] = "PMGI";
#define _CHM_PMGI_LEN (0x08)
struct chmPmgiHeader
{
    char        signature[4];           /*  0 (PMGI) */
    UInt32      free_space;             /*  4 */
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_pmgi_header(unsigned char **pData,
                                  unsigned int *pDataLen,
                                  struct chmPmgiHeader *dest)
{
    /* we only know how to deal with a 0x8 byte structures */
    if (*pDataLen != _CHM_PMGI_LEN)
        return 0;

    /* unmarshal fields */
    _unmarshal_char_array(pData, pDataLen,  dest->signature, 4);
    _unmarshal_uint32    (pData, pDataLen, &dest->free_space);

    /* check structure */
    if (memcmp(dest->signature, _chm_pmgi_marker, 4) != 0)
        return 0;

    return 1;
}

/* structure of LZXC reset table */
#define _CHM_LZXC_RESETTABLE_V1_LEN (0x28)
struct chmLzxcResetTable
{
    UInt32      version;
    UInt32      block_count;
    UInt32      unknown;
    UInt32      table_offset;
    UInt64      uncompressed_len;
    UInt64      compressed_len;
    UInt64      block_len;
}; /* __attribute__ ((aligned (1))); */

static int _unmarshal_lzxc_reset_table(unsigned char **pData,
                                       unsigned int *pDataLen,
                                       struct chmLzxcResetTable *dest)
{
    /* we only know how to deal with a 0x28 byte structures */
    if (*pDataLen != _CHM_LZXC_RESETTABLE_V1_LEN)
        return 0;

    /* unmarshal fields */
    _unmarshal_uint32    (pData, pDataLen, &dest->version);
    _unmarshal_uint32    (pData, pDataLen, &dest->block_count);
    _unmarshal_uint32    (pData, pDataLen, &dest->unknown);
    _unmarshal_uint32    (pData, pDataLen, &dest->table_offset);
    _unmarshal_uint64    (pData, pDataLen, &dest->uncompressed_len);
    _unmarshal_uint64    (pData, pDataLen, &dest->compressed_len);
    _unmarshal_uint64    (pData, pDataLen, &dest->block_len);

    /* check structure */
    if (dest->version != 2)
        return 0;

    return 1;
}

/* structure of LZXC control data block */
#define _CHM_LZXC_MIN_LEN (0x18)
#define _CHM_LZXC_V2_LEN (0x1c)
struct chmLzxcControlData
{
    UInt32      size;                   /*  0        */
    char        signature[4];           /*  4 (LZXC) */
    UInt32      version;                /*  8        */
    UInt32      resetInterval;          /*  c        */
    UInt32      windowSize;             /* 10        */
    UInt32      windowsPerReset;        /* 14        */
    UInt32      unknown_18;             /* 18        */
};

static int _unmarshal_lzxc_control_data(unsigned char **pData,
                                        unsigned int *pDataLen,
                                        struct chmLzxcControlData *dest)
{
    /* we want at least 0x18 bytes */
    if (*pDataLen < _CHM_LZXC_MIN_LEN)
        return 0;

    /* unmarshal fields */
    _unmarshal_uint32    (pData, pDataLen, &dest->size);
    _unmarshal_char_array(pData, pDataLen,  dest->signature, 4);
    _unmarshal_uint32    (pData, pDataLen, &dest->version);
    _unmarshal_uint32    (pData, pDataLen, &dest->resetInterval);
    _unmarshal_uint32    (pData, pDataLen, &dest->windowSize);
    _unmarshal_uint32    (pData, pDataLen, &dest->windowsPerReset);

    if (*pDataLen >= _CHM_LZXC_V2_LEN)
        _unmarshal_uint32    (pData, pDataLen, &dest->unknown_18);
    else
        dest->unknown_18 = 0;

    if (dest->version == 2)
    {
        dest->resetInterval *= 0x8000;
        dest->windowSize *= 0x8000;
    }
    if (dest->windowSize == 0  ||  dest->resetInterval == 0)
        return 0;

    /* for now, only support resetInterval a multiple of windowSize/2 */
    if (dest->windowSize == 1)
        return 0;
    if ((dest->resetInterval % (dest->windowSize/2)) != 0)
        return 0;

    /* check structure */
    if (memcmp(dest->signature, "LZXC", 4) != 0)
        return 0;

    return 1;
}

/* the structure used for chm file handles */
struct chmFile
{
#ifdef WIN32
    HANDLE              fd;
#else
    int                 fd;
#endif

#ifdef CHM_MT
#ifdef WIN32
    CRITICAL_SECTION    mutex;
    CRITICAL_SECTION    lzx_mutex;
    CRITICAL_SECTION    cache_mutex;
#else
    pthread_mutex_t     mutex;
    pthread_mutex_t     lzx_mutex;
    pthread_mutex_t     cache_mutex;
#endif
#endif

    UInt64              dir_offset;
    UInt64              dir_len;
    UInt64              data_offset;
    Int32               index_root;
    Int32               index_head;
    UInt32              block_len;

    UInt64              span;
    struct chmUnitInfo  rt_unit;
    struct chmUnitInfo  cn_unit;
    struct chmLzxcResetTable reset_table;

    /* LZX control data */
    int                 compression_enabled;
    UInt32              window_size;
    UInt32              reset_interval;
    UInt32              reset_blkcount;

    /* decompressor state */
    struct LZXstate    *lzx_state;
    int                 lzx_last_block;

    /* cache for decompressed blocks */
    UChar             **cache_blocks;
    UInt64             *cache_block_indices;
    Int32               cache_num_blocks;
};

/*
 * utility functions local to this module
 */

/* utility function to handle differences between {pread,read}(64)? */
static Int64 _chm_fetch_bytes(struct chmFile *h,
                              UChar *buf,
                              UInt64 os,
                              Int64 len)
{
    Int64 readLen=0, oldOs=0;
    if (h->fd  ==  CHM_NULL_FD)
        return readLen;

    CHM_ACQUIRE_LOCK(h->mutex);
#ifdef CHM_USE_WIN32IO
    /* NOTE: this might be better done with CreateFileMapping, et cetera... */
    {
        DWORD origOffsetLo=0, origOffsetHi=0;
        DWORD offsetLo, offsetHi;
        DWORD actualLen=0;

        /* awkward Win32 Seek/Tell */
        offsetLo = (unsigned int)(os & 0xffffffffL);
        offsetHi = (unsigned int)((os >> 32) & 0xffffffffL);
        origOffsetLo = SetFilePointer(h->fd, 0, &origOffsetHi, FILE_CURRENT);
        offsetLo = SetFilePointer(h->fd, offsetLo, &offsetHi, FILE_BEGIN);

        /* read the data */
        if (ReadFile(h->fd,
                     buf,
                     (DWORD)len,
                     &actualLen,
                     NULL) == TRUE)
            readLen = actualLen;
        else
            readLen = 0;

        /* restore original position */
        SetFilePointer(h->fd, origOffsetLo, &origOffsetHi, FILE_BEGIN);
    }
#else
#ifdef CHM_USE_PREAD
#ifdef CHM_USE_IO64
    readLen = pread64(h->fd, buf, (long)len, os);
#else
    readLen = pread(h->fd, buf, (long)len, (unsigned int)os);
#endif
#else
#ifdef CHM_USE_IO64
    oldOs = lseek64(h->fd, 0, SEEK_CUR);
    lseek64(h->fd, os, SEEK_SET);
    readLen = read(h->fd, buf, len);
    lseek64(h->fd, oldOs, SEEK_SET);
#else
    oldOs = lseek(h->fd, 0, SEEK_CUR);
    lseek(h->fd, (long)os, SEEK_SET);
    readLen = read(h->fd, buf, len);
    lseek(h->fd, (long)oldOs, SEEK_SET);
#endif
#endif
#endif
    CHM_RELEASE_LOCK(h->mutex);
    return readLen;
}

/* open an ITS archive */
#ifdef PPC_BSTR
/* RWE 6/12/2003 */
struct chmFile *chm_open(BSTR filename)
#else
struct chmFile *chm_open(const char *filename)
#endif
{
    unsigned char               sbuffer[256];
    unsigned int                sremain;
    unsigned char              *sbufpos;
    struct chmFile             *newHandle=NULL;
    struct chmItsfHeader        itsfHeader;
    struct chmItspHeader        itspHeader;
#if 0
    struct chmUnitInfo          uiSpan;
#endif
    struct chmUnitInfo          uiLzxc;
    struct chmLzxcControlData   ctlData;

    /* allocate handle */
    newHandle = (struct chmFile *)malloc(sizeof(struct chmFile));
    if (newHandle == NULL)
        return NULL;
    newHandle->fd = CHM_NULL_FD;
    newHandle->lzx_state = NULL;
    newHandle->cache_blocks = NULL;
    newHandle->cache_block_indices = NULL;
    newHandle->cache_num_blocks = 0;

    /* open file */
#ifdef WIN32
#ifdef PPC_BSTR
    if ((newHandle->fd=CreateFile(filename,
                                  GENERIC_READ,
                                  FILE_SHARE_READ,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL)) == CHM_NULL_FD)
    {
        free(newHandle);
        return NULL;
    }
#else
    if ((newHandle->fd=CreateFileA(filename,
                                   GENERIC_READ,
                                   0,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL)) == CHM_NULL_FD)
    {
        free(newHandle);
        return NULL;
    }
#endif
#else
    if ((newHandle->fd=open(filename, O_RDONLY)) == CHM_NULL_FD)
    {
        free(newHandle);
        return NULL;
    }
#endif

    /* initialize mutexes, if needed */
#ifdef CHM_MT
#ifdef WIN32
    InitializeCriticalSection(&newHandle->mutex);
    InitializeCriticalSection(&newHandle->lzx_mutex);
    InitializeCriticalSection(&newHandle->cache_mutex);
#else
    pthread_mutex_init(&newHandle->mutex, NULL);
    pthread_mutex_init(&newHandle->lzx_mutex, NULL);
    pthread_mutex_init(&newHandle->cache_mutex, NULL);
#endif
#endif

    /* read and verify header */
    sremain = _CHM_ITSF_V3_LEN;
    sbufpos = sbuffer;
    if (_chm_fetch_bytes(newHandle, sbuffer, (UInt64)0, sremain) != sremain    ||
        !_unmarshal_itsf_header(&sbufpos, &sremain, &itsfHeader))
    {
        chm_close(newHandle);
        return NULL;
    }

    /* stash important values from header */
    newHandle->dir_offset  = itsfHeader.dir_offset;
    newHandle->dir_len     = itsfHeader.dir_len;
    newHandle->data_offset = itsfHeader.data_offset;

    /* now, read and verify the directory header chunk */
    sremain = _CHM_ITSP_V1_LEN;
    sbufpos = sbuffer;
    if (_chm_fetch_bytes(newHandle, sbuffer,
                         (UInt64)itsfHeader.dir_offset, sremain) != sremain       ||
        !_unmarshal_itsp_header(&sbufpos, &sremain, &itspHeader))
    {
        chm_close(newHandle);
        return NULL;
    }

    /* grab essential information from ITSP header */
    newHandle->dir_offset += itspHeader.header_len;
    newHandle->dir_len    -= itspHeader.header_len;
    newHandle->index_root  = itspHeader.index_root;
    newHandle->index_head  = itspHeader.index_head;
    newHandle->block_len   = itspHeader.block_len;

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
    if (CHM_RESOLVE_SUCCESS != chm_resolve_object(newHandle,
                                                  _CHMU_RESET_TABLE,
                                                  &newHandle->rt_unit)    ||
        newHandle->rt_unit.space == CHM_COMPRESSED                        ||
        CHM_RESOLVE_SUCCESS != chm_resolve_object(newHandle,
                                                  _CHMU_CONTENT,
                                                  &newHandle->cn_unit)    ||
        newHandle->cn_unit.space == CHM_COMPRESSED                        ||
        CHM_RESOLVE_SUCCESS != chm_resolve_object(newHandle,
                                                  _CHMU_LZXC_CONTROLDATA,
                                                  &uiLzxc)                ||
        uiLzxc.space == CHM_COMPRESSED)
    {
        newHandle->compression_enabled = 0;
    }

    /* read reset table info */
    if (newHandle->compression_enabled)
    {
        sremain = _CHM_LZXC_RESETTABLE_V1_LEN;
        sbufpos = sbuffer;
        if (chm_retrieve_object(newHandle, &newHandle->rt_unit, sbuffer,
                                0, sremain) != sremain                        ||
            !_unmarshal_lzxc_reset_table(&sbufpos, &sremain,
                                         &newHandle->reset_table))
        {
            newHandle->compression_enabled = 0;
        }
    }

    /* read control data */
    if (newHandle->compression_enabled)
    {
        sremain = (unsigned int)uiLzxc.length;
        if (uiLzxc.length > sizeof(sbuffer))
        {
            chm_close(newHandle);
            return NULL;
        }

        sbufpos = sbuffer;
        if (chm_retrieve_object(newHandle, &uiLzxc, sbuffer,
                                0, sremain) != sremain                       ||
            !_unmarshal_lzxc_control_data(&sbufpos, &sremain,
                                          &ctlData))
        {
            newHandle->compression_enabled = 0;
        }

        newHandle->window_size = ctlData.windowSize;
        newHandle->reset_interval = ctlData.resetInterval;

/* Jed, Mon Jun 28: Experimentally, it appears that the reset block count */
/*       must be multiplied by this formerly unknown ctrl data field in   */
/*       order to decompress some files.                                  */
#if 0
        newHandle->reset_blkcount = newHandle->reset_interval /
                    (newHandle->window_size / 2);
#else
        newHandle->reset_blkcount = newHandle->reset_interval    /
                                    (newHandle->window_size / 2) *
                                    ctlData.windowsPerReset;
#endif
    }

    /* initialize cache */
    chm_set_param(newHandle, CHM_PARAM_MAX_BLOCKS_CACHED,
                  CHM_MAX_BLOCKS_CACHED);

    return newHandle;
}

/* close an ITS archive */
void chm_close(struct chmFile *h)
{
    if (h != NULL)
    {
        if (h->fd != CHM_NULL_FD)
            CHM_CLOSE_FILE(h->fd);
        h->fd = CHM_NULL_FD;

#ifdef CHM_MT
#ifdef WIN32
        DeleteCriticalSection(&h->mutex);
        DeleteCriticalSection(&h->lzx_mutex);
        DeleteCriticalSection(&h->cache_mutex);
#else
        pthread_mutex_destroy(&h->mutex);
        pthread_mutex_destroy(&h->lzx_mutex);
        pthread_mutex_destroy(&h->cache_mutex);
#endif
#endif

        if (h->lzx_state)
            LZXteardown(h->lzx_state);
        h->lzx_state = NULL;

        if (h->cache_blocks)
        {
            int i;
            for (i=0; i<h->cache_num_blocks; i++)
            {
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
void chm_set_param(struct chmFile *h,
                   int paramType,
                   int paramVal)
{
    switch (paramType)
    {
        case CHM_PARAM_MAX_BLOCKS_CACHED:
            CHM_ACQUIRE_LOCK(h->cache_mutex);
            if (paramVal != h->cache_num_blocks)
            {
                UChar **newBlocks;
                UInt64 *newIndices;
                int     i;

                /* allocate new cached blocks */
                newBlocks = (UChar **)malloc(paramVal * sizeof (UChar *));
                if (newBlocks == NULL) return;
                newIndices = (UInt64 *)malloc(paramVal * sizeof (UInt64));
                if (newIndices == NULL) { free(newBlocks); return; }
                for (i=0; i<paramVal; i++)
                {
                    newBlocks[i] = NULL;
                    newIndices[i] = 0;
                }

                /* re-distribute old cached blocks */
                if (h->cache_blocks)
                {
                    for (i=0; i<h->cache_num_blocks; i++)
                    {
                        int newSlot = (int)(h->cache_block_indices[i] % paramVal);

                        if (h->cache_blocks[i])
                        {
                            /* in case of collision, destroy newcomer */
                            if (newBlocks[newSlot])
                            {
                                free(h->cache_blocks[i]);
                                h->cache_blocks[i] = NULL;
                            }
                            else
                            {
                                newBlocks[newSlot] = h->cache_blocks[i];
                                newIndices[newSlot] =
                                            h->cache_block_indices[i];
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
            CHM_RELEASE_LOCK(h->cache_mutex);
            break;

        default:
            break;
    }
}

/*
 * helper methods for chm_resolve_object
 */

/* skip a compressed dword */
static void _chm_skip_cword(UChar **pEntry)
{
    while (*(*pEntry)++ >= 0x80)
        ;
}

/* skip the data from a PMGL entry */
static void _chm_skip_PMGL_entry_data(UChar **pEntry)
{
    _chm_skip_cword(pEntry);
    _chm_skip_cword(pEntry);
    _chm_skip_cword(pEntry);
}

/* parse a compressed dword */
static UInt64 _chm_parse_cword(UChar **pEntry)
{
    UInt64 accum = 0;
    UChar temp;
    while ((temp=*(*pEntry)++) >= 0x80)
    {
        accum <<= 7;
        accum += temp & 0x7f;
    }

    return (accum << 7) + temp;
}

/* parse a utf-8 string into an ASCII char buffer */
static int _chm_parse_UTF8(UChar **pEntry, UInt64 count, char *path)
{
    /* XXX: implement UTF-8 support, including a real mapping onto
     *      ISO-8859-1?  probably there is a library to do this?  As is
     *      immediately apparent from the below code, I'm presently not doing
     *      any special handling for files in which none of the strings contain
     *      UTF-8 multi-byte characters.
     */
    while (count != 0)
    {
        *path++ = (char)(*(*pEntry)++);
        --count;
    }

    *path = '\0';
    return 1;
}

/* parse a PMGL entry into a chmUnitInfo struct; return 1 on success. */
static int _chm_parse_PMGL_entry(UChar **pEntry, struct chmUnitInfo *ui)
{
    UInt64 strLen;

    /* parse str len */
    strLen = _chm_parse_cword(pEntry);
    if (strLen > CHM_MAX_PATHLEN)
        return 0;

    /* parse path */
    if (! _chm_parse_UTF8(pEntry, strLen, ui->path))
        return 0;

    /* parse info */
    ui->space  = (int)_chm_parse_cword(pEntry);
    ui->start  = _chm_parse_cword(pEntry);
    ui->length = _chm_parse_cword(pEntry);
    return 1;
}

/* find an exact entry in PMGL; return NULL if we fail */
static UChar *_chm_find_in_PMGL(UChar *page_buf,
                         UInt32 block_len,
                         const char *objPath)
{
    /* XXX: modify this to do a binary search using the nice index structure
     *      that is provided for us.
     */
    struct chmPmglHeader header;
    unsigned int hremain;
    UChar *end;
    UChar *cur;
    UChar *temp;
    UInt64 strLen;
    char buffer[CHM_MAX_PATHLEN+1];

    /* figure out where to start and end */
    cur = page_buf;
    hremain = _CHM_PMGL_LEN;
    if (! _unmarshal_pmgl_header(&cur, &hremain, &header))
        return NULL;
    end = page_buf + block_len - (header.free_space);

    /* now, scan progressively */
    while (cur < end)
    {
        /* grab the name */
        temp = cur;
        strLen = _chm_parse_cword(&cur);
        if (strLen > CHM_MAX_PATHLEN)
            return NULL;
        if (! _chm_parse_UTF8(&cur, strLen, buffer))
            return NULL;

        /* check if it is the right name */
        if (! strcasecmp(buffer, objPath))
            return temp;

        _chm_skip_PMGL_entry_data(&cur);
    }

    return NULL;
}

/* find which block should be searched next for the entry; -1 if no block */
static Int32 _chm_find_in_PMGI(UChar *page_buf,
                        UInt32 block_len,
                        const char *objPath)
{
    /* XXX: modify this to do a binary search using the nice index structure
     *      that is provided for us
     */
    struct chmPmgiHeader header;
    unsigned int hremain;
    int page=-1;
    UChar *end;
    UChar *cur;
    UInt64 strLen;
    char buffer[CHM_MAX_PATHLEN+1];

    /* figure out where to start and end */
    cur = page_buf;
    hremain = _CHM_PMGI_LEN;
    if (! _unmarshal_pmgi_header(&cur, &hremain, &header))
        return -1;
    end = page_buf + block_len - (header.free_space);

    /* now, scan progressively */
    while (cur < end)
    {
        /* grab the name */
        strLen = _chm_parse_cword(&cur);
        if (strLen > CHM_MAX_PATHLEN)
            return -1;
        if (! _chm_parse_UTF8(&cur, strLen, buffer))
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
int chm_resolve_object(struct chmFile *h,
                       const char *objPath,
                       struct chmUnitInfo *ui)
{
    /*
     * XXX: implement caching scheme for dir pages
     */

    Int32 curPage;

    /* buffer to hold whatever page we're looking at */
    /* RWE 6/12/2003 */
    UChar *page_buf = malloc(h->block_len);
    if (page_buf == NULL)
        return CHM_RESOLVE_FAILURE;

    /* starting page */
    curPage = h->index_root;

    /* until we have either returned or given up */
    while (curPage != -1)
    {

        /* try to fetch the index page */
        if (_chm_fetch_bytes(h, page_buf,
                             (UInt64)h->dir_offset + (UInt64)curPage*h->block_len,
                             h->block_len) != h->block_len)
        {
            free(page_buf);
            return CHM_RESOLVE_FAILURE;
        }

        /* now, if it is a leaf node: */
        if (memcmp(page_buf, _chm_pmgl_marker, 4) == 0)
        {
            /* scan block */
            UChar *pEntry = _chm_find_in_PMGL(page_buf,
                                              h->block_len,
                                              objPath);
            if (pEntry == NULL)
            {
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
        else
        {
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
static int _chm_get_cmpblock_bounds(struct chmFile *h,
                             UInt64 block,
                             UInt64 *start,
                             Int64 *len)
{
    UChar buffer[8], *dummy;
    unsigned int remain;

    /* for all but the last block, use the reset table */
    if (block < h->reset_table.block_count-1)
    {
        /* unpack the start address */
        dummy = buffer;
        remain = 8;
        if (_chm_fetch_bytes(h, buffer,
                             (UInt64)h->data_offset
                                + (UInt64)h->rt_unit.start
                                + (UInt64)h->reset_table.table_offset
                                + (UInt64)block*8,
                             remain) != remain                            ||
            !_unmarshal_uint64(&dummy, &remain, start))
            return 0;

        /* unpack the end address */
        dummy = buffer;
        remain = 8;
        if (_chm_fetch_bytes(h, buffer,
                         (UInt64)h->data_offset
                                + (UInt64)h->rt_unit.start
                                + (UInt64)h->reset_table.table_offset
                                + (UInt64)block*8 + 8,
                         remain) != remain                                ||
            !_unmarshal_int64(&dummy, &remain, len))
            return 0;
    }

    /* for the last block, use the span in addition to the reset table */
    else
    {
        /* unpack the start address */
        dummy = buffer;
        remain = 8;
        if (_chm_fetch_bytes(h, buffer,
                             (UInt64)h->data_offset
                                + (UInt64)h->rt_unit.start
                                + (UInt64)h->reset_table.table_offset
                                + (UInt64)block*8,
                             remain) != remain                            ||
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
static Int64 _chm_decompress_block(struct chmFile *h,
                                   UInt64 block,
                                   UChar **ubuffer)
{
    UChar *cbuffer = malloc(((unsigned int)h->reset_table.block_len + 6144));
    UInt64 cmpStart;                                    /* compressed start  */
    Int64 cmpLen;                                       /* compressed len    */
    int indexSlot;                                      /* cache index slot  */
    UChar *lbuffer;                                     /* local buffer ptr  */
    UInt32 blockAlign = (UInt32)(block % h->reset_blkcount); /* reset intvl. aln. */
    UInt32 i;                                           /* local loop index  */

    if (cbuffer == NULL)
        return -1;

    /* let the caching system pull its weight! */
    if (block - blockAlign <= h->lzx_last_block  &&
        block              >= h->lzx_last_block)
        blockAlign = (block - h->lzx_last_block);

    /* check if we need previous blocks */
    if (blockAlign != 0)
    {
        /* fetch all required previous blocks since last reset */
        for (i = blockAlign; i > 0; i--)
        {
            UInt32 curBlockIdx = block - i;

            /* check if we most recently decompressed the previous block */
            if (h->lzx_last_block != curBlockIdx)
            {
                if ((curBlockIdx % h->reset_blkcount) == 0)
                {
#ifdef CHM_DEBUG
                    fprintf(stderr, "***RESET (1)***\n");
#endif
                    LZXreset(h->lzx_state);
                }

                indexSlot = (int)((curBlockIdx) % h->cache_num_blocks);
                if (! h->cache_blocks[indexSlot])
                    h->cache_blocks[indexSlot] = (UChar *)malloc((unsigned int)(h->reset_table.block_len));
                if (! h->cache_blocks[indexSlot])
                {
                    free(cbuffer);
                    return -1;
                }
                h->cache_block_indices[indexSlot] = curBlockIdx;
                lbuffer = h->cache_blocks[indexSlot];

                /* decompress the previous block */
#ifdef CHM_DEBUG
                fprintf(stderr, "Decompressing block #%4d (EXTRA)\n", curBlockIdx);
#endif
                if (!_chm_get_cmpblock_bounds(h, curBlockIdx, &cmpStart, &cmpLen) ||
                    cmpLen < 0                                                    ||
                    cmpLen > h->reset_table.block_len + 6144                      ||
                    _chm_fetch_bytes(h, cbuffer, cmpStart, cmpLen) != cmpLen      ||
                    LZXdecompress(h->lzx_state, cbuffer, lbuffer, (int)cmpLen,
                                  (int)h->reset_table.block_len) != DECR_OK)
                {
#ifdef CHM_DEBUG
                    fprintf(stderr, "   (DECOMPRESS FAILED!)\n");
#endif
                    free(cbuffer);
                    return (Int64)0;
                }

                h->lzx_last_block = (int)curBlockIdx;
            }
        }
    }
    else
    {
        if ((block % h->reset_blkcount) == 0)
        {
#ifdef CHM_DEBUG
            fprintf(stderr, "***RESET (2)***\n");
#endif
            LZXreset(h->lzx_state);
        }
    }

    /* allocate slot in cache */
    indexSlot = (int)(block % h->cache_num_blocks);
    if (! h->cache_blocks[indexSlot])
        h->cache_blocks[indexSlot] = (UChar *)malloc(((unsigned int)h->reset_table.block_len));
    if (! h->cache_blocks[indexSlot])
    {
        free(cbuffer);
        return -1;
    }
    h->cache_block_indices[indexSlot] = block;
    lbuffer = h->cache_blocks[indexSlot];
    *ubuffer = lbuffer;

    /* decompress the block we actually want */
#ifdef CHM_DEBUG
    fprintf(stderr, "Decompressing block #%4d (REAL )\n", block);
#endif
    if (! _chm_get_cmpblock_bounds(h, block, &cmpStart, &cmpLen)          ||
        _chm_fetch_bytes(h, cbuffer, cmpStart, cmpLen) != cmpLen          ||
        LZXdecompress(h->lzx_state, cbuffer, lbuffer, (int)cmpLen,
                      (int)h->reset_table.block_len) != DECR_OK)
    {
#ifdef CHM_DEBUG
        fprintf(stderr, "   (DECOMPRESS FAILED!)\n");
#endif
        free(cbuffer);
        return (Int64)0;
    }
    h->lzx_last_block = (int)block;

    /* XXX: modify LZX routines to return the length of the data they
     * decompressed and return that instead, for an extra sanity check.
     */
    free(cbuffer);
    return h->reset_table.block_len;
}

/* grab a region from a compressed block */
static Int64 _chm_decompress_region(struct chmFile *h,
                                    UChar *buf,
                                    UInt64 start,
                                    Int64 len)
{
    UInt64 nBlock, nOffset;
    UInt64 nLen;
    UInt64 gotLen;
    UChar *ubuffer;

    if (len <= 0)
        return (Int64)0;

    /* figure out what we need to read */
    nBlock = start / h->reset_table.block_len;
    nOffset = start % h->reset_table.block_len;
    nLen = len;
    if (nLen > (h->reset_table.block_len - nOffset))
        nLen = h->reset_table.block_len - nOffset;

    /* if block is cached, return data from it. */
    CHM_ACQUIRE_LOCK(h->lzx_mutex);
    CHM_ACQUIRE_LOCK(h->cache_mutex);
    if (h->cache_block_indices[nBlock % h->cache_num_blocks] == nBlock    &&
        h->cache_blocks[nBlock % h->cache_num_blocks] != NULL)
    {
        memcpy(buf,
               h->cache_blocks[nBlock % h->cache_num_blocks] + nOffset,
               (unsigned int)nLen);
        CHM_RELEASE_LOCK(h->cache_mutex);
        CHM_RELEASE_LOCK(h->lzx_mutex);
        return nLen;
    }
    CHM_RELEASE_LOCK(h->cache_mutex);

    /* data request not satisfied, so... start up the decompressor machine */
    if (! h->lzx_state)
    {
        int window_size = ffs(h->window_size) - 1;
        h->lzx_last_block = -1;
        h->lzx_state = LZXinit(window_size);
    }

    /* decompress some data */
    gotLen = _chm_decompress_block(h, nBlock, &ubuffer);
    if (gotLen < nLen)
        nLen = gotLen;
    memcpy(buf, ubuffer+nOffset, (unsigned int)nLen);
    CHM_RELEASE_LOCK(h->lzx_mutex);
    return nLen;
}

/* retrieve (part of) an object */
LONGINT64 chm_retrieve_object(struct chmFile *h,
                               struct chmUnitInfo *ui,
                               unsigned char *buf,
                               LONGUINT64 addr,
                               LONGINT64 len)
{
    /* must be valid file handle */
    if (h == NULL)
        return (Int64)0;

    /* starting address must be in correct range */
    if (addr < 0  ||  addr >= ui->length)
        return (Int64)0;

    /* clip length */
    if (addr + len > ui->length)
        len = ui->length - addr;

    /* if the file is uncompressed, it's simple */
    if (ui->space == CHM_UNCOMPRESSED)
    {
        /* read data */
        return _chm_fetch_bytes(h,
                                buf,
                                (UInt64)h->data_offset + (UInt64)ui->start + (UInt64)addr,
                                len);
    }

    /* else if the file is compressed, it's a little trickier */
    else /* ui->space == CHM_COMPRESSED */
    {
        Int64 swath=0, total=0;

        /* if compression is not enabled for this file... */
        if (! h->compression_enabled)
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
int chm_enumerate(struct chmFile *h,
                  int what,
                  CHM_ENUMERATOR e,
                  void *context)
{
    Int32 curPage;

    /* buffer to hold whatever page we're looking at */
    /* RWE 6/12/2003 */
    UChar *page_buf = malloc((unsigned int)h->block_len);
    struct chmPmglHeader header;
    UChar *end;
    UChar *cur;
    unsigned int lenRemain;
    UInt64 ui_path_len;

    /* the current ui */
    struct chmUnitInfo ui;
    int type_bits = (what & 0x7);
    int filter_bits = (what & 0xF8);

    if (page_buf == NULL)
        return 0;

    /* starting page */
    curPage = h->index_head;

    /* until we have either returned or given up */
    while (curPage != -1)
    {

        /* try to fetch the index page */
        if (_chm_fetch_bytes(h,
                             page_buf,
                             (UInt64)h->dir_offset + (UInt64)curPage*h->block_len,
                             h->block_len) != h->block_len)
        {
            free(page_buf);
            return 0;
        }

        /* figure out start and end for this page */
        cur = page_buf;
        lenRemain = _CHM_PMGL_LEN;
        if (! _unmarshal_pmgl_header(&cur, &lenRemain, &header))
        {
            free(page_buf);
            return 0;
        }
        end = page_buf + h->block_len - (header.free_space);

        /* loop over this page */
        while (cur < end)
        {
            ui.flags = 0;

            if (! _chm_parse_PMGL_entry(&cur, &ui))
            {
                free(page_buf);
                return 0;
            }

            /* get the length of the path */
            ui_path_len = strlen(ui.path)-1;

            /* check for DIRS */
            if (ui.path[ui_path_len] == '/')
                ui.flags |= CHM_ENUMERATE_DIRS;

            /* check for FILES */
            if (ui.path[ui_path_len] != '/')
                ui.flags |= CHM_ENUMERATE_FILES;

            /* check for NORMAL vs. META */
            if (ui.path[0] == '/')
            {

                /* check for NORMAL vs. SPECIAL */
                if (ui.path[1] == '#'  ||  ui.path[1] == '$')
                    ui.flags |= CHM_ENUMERATE_SPECIAL;
                else
                    ui.flags |= CHM_ENUMERATE_NORMAL;
            }
            else
                ui.flags |= CHM_ENUMERATE_META;

            if (! (type_bits & ui.flags))
                continue;

            if (filter_bits && ! (filter_bits & ui.flags))
                continue;

            /* call the enumerator */
            {
                int status = (*e)(h, &ui, context);
                switch (status)
                {
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

int chm_enumerate_dir(struct chmFile *h,
                      const char *prefix,
                      int what,
                      CHM_ENUMERATOR e,
                      void *context)
{
    /*
     * XXX: do this efficiently (i.e. using the tree index)
     */

    Int32 curPage;

    /* buffer to hold whatever page we're looking at */
    /* RWE 6/12/2003 */
    UChar *page_buf = malloc((unsigned int)h->block_len);
    struct chmPmglHeader header;
    UChar *end;
    UChar *cur;
    unsigned int lenRemain;

    /* set to 1 once we've started */
    int it_has_begun=0;

    /* the current ui */
    struct chmUnitInfo ui;
    int type_bits = (what & 0x7);
    int filter_bits = (what & 0xF8);
    UInt64 ui_path_len;

    /* the length of the prefix */
    char prefixRectified[CHM_MAX_PATHLEN+1];
    int prefixLen;
    char lastPath[CHM_MAX_PATHLEN+1];
    int lastPathLen;

    if (page_buf == NULL)
        return 0;

    /* starting page */
    curPage = h->index_head;

    /* initialize pathname state */
    strncpy(prefixRectified, prefix, CHM_MAX_PATHLEN);
    prefixRectified[CHM_MAX_PATHLEN] = '\0';
    prefixLen = strlen(prefixRectified);
    if (prefixLen != 0)
    {
        if (prefixRectified[prefixLen-1] != '/')
        {
            prefixRectified[prefixLen] = '/';
            prefixRectified[prefixLen+1] = '\0';
            ++prefixLen;
        }
    }
    lastPath[0] = '\0';
    lastPathLen = -1;

    /* until we have either returned or given up */
    while (curPage != -1)
    {

        /* try to fetch the index page */
        if (_chm_fetch_bytes(h,
                             page_buf,
                             (UInt64)h->dir_offset + (UInt64)curPage*h->block_len,
                             h->block_len) != h->block_len)
        {
            free(page_buf);
            return 0;
        }

        /* figure out start and end for this page */
        cur = page_buf;
        lenRemain = _CHM_PMGL_LEN;
        if (! _unmarshal_pmgl_header(&cur, &lenRemain, &header))
        {
            free(page_buf);
            return 0;
        }
        end = page_buf + h->block_len - (header.free_space);

        /* loop over this page */
        while (cur < end)
        {
            ui.flags = 0;

            if (! _chm_parse_PMGL_entry(&cur, &ui))
            {
                free(page_buf);
                return 0;
            }

            /* check if we should start */
            if (! it_has_begun)
            {
                if (ui.length == 0  &&  strncasecmp(ui.path, prefixRectified, prefixLen) == 0)
                    it_has_begun = 1;
                else
                    continue;

                if (ui.path[prefixLen] == '\0')
                    continue;
            }

            /* check if we should stop */
            else
            {
                if (strncasecmp(ui.path, prefixRectified, prefixLen) != 0)
                {
                    free(page_buf);
                    return 1;
                }
            }

            /* check if we should include this path */
            if (lastPathLen != -1)
            {
                if (strncasecmp(ui.path, lastPath, lastPathLen) == 0)
                    continue;
            }
            strncpy(lastPath, ui.path, CHM_MAX_PATHLEN);
            lastPath[CHM_MAX_PATHLEN] = '\0';
            lastPathLen = strlen(lastPath);

            /* get the length of the path */
            ui_path_len = strlen(ui.path)-1;

            /* check for DIRS */
            if (ui.path[ui_path_len] == '/')
                ui.flags |= CHM_ENUMERATE_DIRS;

            /* check for FILES */
            if (ui.path[ui_path_len] != '/')
                ui.flags |= CHM_ENUMERATE_FILES;

            /* check for NORMAL vs. META */
            if (ui.path[0] == '/')
            {

                /* check for NORMAL vs. SPECIAL */
                if (ui.path[1] == '#'  ||  ui.path[1] == '$')
                    ui.flags |= CHM_ENUMERATE_SPECIAL;
                else
                    ui.flags |= CHM_ENUMERATE_NORMAL;
            }
            else
                ui.flags |= CHM_ENUMERATE_META;

            if (! (type_bits & ui.flags))
                continue;

            if (filter_bits && ! (filter_bits & ui.flags))
                continue;

            /* call the enumerator */
            {
                int status = (*e)(h, &ui, context);
                switch (status)
                {
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

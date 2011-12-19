/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiParse.h"

#include <stdint.h>
#include "BaseUtil.h"

#define PALMDOC_TYPE       "TEXt"
#define PALMDOC_CREATOR    "REAd"
#define MOBI_TYPE          "BOOK"
#define MOBI_CREATOR       "MOBI"

#define kDBNameLength    32
#define kPdbHeaderLen    78

// http://en.wikipedia.org/wiki/PDB_(Palm_OS)
#pragma pack(push)
#pragma pack(1)
struct PdbHeader
{
     /* 31 chars + 1 null terminator */
    char       name[kDBNameLength];
    int16_t    attributes;
    int16_t    version;
    int32_t    createTime;
    int32_t    modifyTime;
    int32_t    backupTime;
    int32_t    modificationNumber;
    int32_t    appInfoID;
    int32_t    sortInfoID;
    char       type[4];
    char       creator[4];
    int32_t    idSeed;
    int32_t    nextRecordList;
    int16_t    numRecords;
};
#pragma pack(pop)

CASSERT(kPdbHeaderLen == sizeof(PdbHeader), validPdbHeader);

#define kPdbRecordHeaderLen 8

#pragma pack(push)
#pragma pack(1)
struct PdbRecordHeader {
    int32_t  offset;
    uint8_t deleted   : 1;
    uint8_t dirty     : 1;
    uint8_t busy      : 1;
    uint8_t secret    : 1;
    uint8_t category  : 4;
    char  uniqueID[3];
};
#pragma pack(pop)

CASSERT(kPdbRecordHeaderLen == sizeof(PdbRecordHeader), validPdbRecordHeader);


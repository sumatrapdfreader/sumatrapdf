/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiParse_h
#define MobiParse_h

#include <stdint.h>

#include "BaseUtil.h"
#include "Vec.h"

class HuffDicDecompressor;

// http://en.wikipedia.org/wiki/PDB_(Palm_OS)
#define kDBNameLength    32
#define kPdbHeaderLen    78

#pragma pack(push)
#pragma pack(1)
struct PdbHeader
{
     /* 31 chars + 1 null terminator */
    char        name[kDBNameLength];
    uint16_t    attributes;
    uint16_t    version;
    uint32_t    createTime;
    uint32_t    modifyTime;
    uint32_t    backupTime;
    uint32_t    modificationNumber;
    uint32_t    appInfoID;
    uint32_t    sortInfoID;
    char        type[4];
    char        creator[4];
    uint32_t    idSeed;
    uint32_t    nextRecordList;
    uint16_t    numRecords;
};
#pragma pack(pop)

STATIC_ASSERT(kPdbHeaderLen == sizeof(PdbHeader), validPdbHeader);

#define kPdbRecordHeaderLen 8

#pragma pack(push)
#pragma pack(1)
struct PdbRecordHeader {
    uint32_t offset;
    uint8_t  deleted   : 1;
    uint8_t  dirty     : 1;
    uint8_t  busy      : 1;
    uint8_t  secret    : 1;
    uint8_t  category  : 4;
    char     uniqueID[3];
};
#pragma pack(pop)

STATIC_ASSERT(kPdbRecordHeaderLen == sizeof(PdbRecordHeader), validPdbRecordHeader);

#define kMaxRecordSize 64*1024

class MobiParse
{
    TCHAR *             fileName;
    HANDLE              fileHandle;

    PdbHeader           pdbHeader;
    PdbRecordHeader *   recHeaders;
    char *              firstRecData;

    bool                isMobi;
    size_t              docRecCount;
    int                 compressionType;
    size_t              docUncompressedSize;
    str::Str<char>      doc;
    bool                multibyte;
    size_t              trailersCount;

    // we use bufStatic if record fits in it, bufDynamic otherwise
    char                bufStatic[kMaxRecordSize];
    char *              bufDynamic;
    size_t              bufDynamicSize;

    HuffDicDecompressor *huffDic;

    MobiParse();

    bool    ParseHeader();
    char *  GetBufForRecordData(size_t size);
    size_t  GetRecordSize(size_t recNo);
    char*   ReadRecord(size_t recNo, size_t& sizeOut);
    bool    LoadDocRecordIntoBuffer(size_t recNo, str::Str<char>& strOut);

public:
    static MobiParse *ParseFile(const TCHAR *fileName);

    ~MobiParse();
    bool LoadDocument();
};

#endif

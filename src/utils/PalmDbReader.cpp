/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "PalmDbReader.h"

#include "ByteReader.h"
#include "FileUtil.h"
#include "WinUtil.h"

#include <pshpack1.h>

// cf. http://wiki.mobileread.com/wiki/PDB
struct PdbHeader {
     /* 31 chars + 1 null terminator */
    char        name[32];
    uint16_t    attributes;
    uint16_t    version;
    uint32_t    createTime;
    uint32_t    modifyTime;
    uint32_t    backupTime;
    uint32_t    modificationNumber;
    uint32_t    appInfoID;
    uint32_t    sortInfoID;
    char        typeCreator[8];
    uint32_t    idSeed;
    uint32_t    nextRecordList;
    uint16_t    numRecords;
};

struct PdbRecordHeader {
    uint32_t offset;
    uint8_t  flags; // deleted, dirty, busy, secret, category
    char     uniqueID[3];
};

#include <poppack.h>

STATIC_ASSERT(sizeof(PdbHeader) == kPdbHeaderLen, pdbHeaderSize);
STATIC_ASSERT(sizeof(PdbRecordHeader) == 8, pdbRecHeaderSize);

PdbReader::PdbReader(const WCHAR *filePath) :
    data(file::ReadAll(filePath, &dataSize))
{
    if (!ParseHeader())
        recOffsets.Reset();
}

PdbReader::PdbReader(IStream *stream) :
    data((char *)GetDataFromStream(stream, &dataSize))
{
    if (!ParseHeader())
        recOffsets.Reset();
}

bool PdbReader::ParseHeader()
{
    CrashIf(recOffsets.Count() > 0);

    PdbHeader pdbHeader;
    if (!data || dataSize < sizeof(pdbHeader))
        return false;
    ByteReader r(data, dataSize);

    bool ok = r.UnpackBE(&pdbHeader, sizeof(pdbHeader), "32b2w6d8b2dw");
    CrashIf(!ok);
    // the spec says it should be zero-terminated anyway, but this
    // comes from untrusted source, so we do our own termination
    pdbHeader.name[dimof(pdbHeader.name) - 1] = '\0';
    str::BufSet(dbType, dimof(dbType), pdbHeader.typeCreator);

    if (0 == pdbHeader.numRecords)
        return false;

    for (int i = 0; i < pdbHeader.numRecords; i++) {
        uint32_t off = r.DWordBE(sizeof(pdbHeader) + i * sizeof(PdbRecordHeader));
        recOffsets.Append(off);
    }
    // add sentinel value to simplify use
    recOffsets.Append((uint32_t)min(dataSize, (uint32_t)-1));

    // validate offsets
    for (int i = 0; i < pdbHeader.numRecords; i++) {
        if (recOffsets.At(i + 1) < recOffsets.At(i))
            return false;
        // technically PDB record size should be less than 64K,
        // but it's not true for mobi files, so we don't validate that
    }

    return true;
}

const char *PdbReader::GetDbType()
{
    if (recOffsets.Count() == 0)
        return NULL;
    return dbType;
}

size_t PdbReader::GetRecordCount()
{
    return recOffsets.Count() - 1;
}

const char *PdbReader::GetRecord(size_t recNo, size_t *sizeOut)
{
    if (recNo + 1 >= recOffsets.Count())
        return NULL;
    size_t offset = recOffsets.At(recNo);
    if (sizeOut)
        *sizeOut = recOffsets.At(recNo + 1) - offset;
    return data + offset;
}

/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
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
    char name[32];
    uint16_t attributes;
    uint16_t version;
    uint32_t createTime;
    uint32_t modifyTime;
    uint32_t backupTime;
    uint32_t modificationNumber;
    uint32_t appInfoID;
    uint32_t sortInfoID;
    char typeCreator[8];
    uint32_t idSeed;
    uint32_t nextRecordList;
    uint16_t numRecords;
};

struct PdbRecordHeader {
    uint32_t offset;
    uint8_t flags; // deleted, dirty, busy, secret, category
    char uniqueID[3];
};

#include <poppack.h>

static_assert(sizeof(PdbHeader) == kPdbHeaderLen, "wrong size of PdbHeader structure");
static_assert(sizeof(PdbRecordHeader) == 8, "wrong size of PdbRecordHeader structure");

bool PdbReader::Parse(OwnedData data) {
    this->data = std::move(data);
    if (!ParseHeader()) {
        recOffsets.clear();
        return false;
    }
    return true;
}

bool PdbReader::ParseHeader() {
    CrashIf(recOffsets.size() > 0);

    PdbHeader pdbHeader = {0};
    if (!data.data || data.size < sizeof(pdbHeader)) {
        return false;
    }
    ByteReader r(data.data, data.size);

    bool ok = r.UnpackBE(&pdbHeader, sizeof(pdbHeader), "32b2w6d8b2dw");
    CrashIf(!ok);
    // the spec says it should be zero-terminated anyway, but this
    // comes from untrusted source, so we do our own termination
    pdbHeader.name[dimof(pdbHeader.name) - 1] = '\0';
    str::BufSet(dbType, dimof(dbType), pdbHeader.typeCreator);

    if (0 == pdbHeader.numRecords) {
        return false;
    }

    for (int i = 0; i < pdbHeader.numRecords; i++) {
        uint32_t off = r.DWordBE(sizeof(pdbHeader) + i * sizeof(PdbRecordHeader));
        recOffsets.push_back(off);
    }
    // add sentinel value to simplify use
    recOffsets.push_back(std::min((uint32_t)data.size, (uint32_t)-1));

    // validate offsets
    for (int i = 0; i < pdbHeader.numRecords; i++) {
        if (recOffsets.at(i + 1) < recOffsets.at(i)) {
            return false;
        }
        // technically PDB record size should be less than 64K,
        // but it's not true for mobi files, so we don't validate that
    }

    return true;
}

const char* PdbReader::GetDbType() {
    if (recOffsets.size() == 0) {
        return nullptr;
    }
    return dbType;
}

size_t PdbReader::GetRecordCount() {
    return recOffsets.size() - 1;
}

// don't free, memory is owned by us
std::string_view PdbReader::GetRecord(size_t recNo) {
    if (recNo + 1 >= recOffsets.size()) {
        return {};
    }
    size_t offset = recOffsets.at(recNo);
    size_t size = recOffsets.at(recNo + 1) - offset;
    return {data.data + offset, size};
}

PdbReader* PdbReader::CreateFromData(OwnedData data) {
    if (!data.data) {
        return nullptr;
    }
    PdbReader* reader = new PdbReader();
    if (!reader->Parse(std::move(data))) {
        delete reader;
        return nullptr;
    }
    return reader;
}

#if OS_WIN
PdbReader* PdbReader::CreateFromFile(const WCHAR *filePath) {
    OwnedData data = file::ReadAll(filePath);
    return CreateFromData(std::move(data));
}

PdbReader* PdbReader::CreateFromStream(IStream *stream) {
    size_t size;
    char* tmp = (char*)GetDataFromStream(stream, &size);
    OwnedData data(tmp, size);
    return CreateFromData(std::move(data));
}
#endif

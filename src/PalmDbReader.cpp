/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
// #include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"
#include "utils/ByteOrderDecoder.h"

#include "PalmDbReader.h"

// size of PdbHeader
#define kPdbHeaderLen 78
// size of PdbRecordHeader
#define kPdbRecordHeaderLen 8

// Takes ownership of d
bool PdbReader::Parse(const ByteSlice& d) {
    data = d.data();
    dataSize = d.size();
    return ParseHeader();
}

PdbReader::~PdbReader() {
    str::Free(data);
}

static bool DecodePdbHeader(ByteOrderDecoder& dec, PdbHeader* hdr) {
    dec.Bytes(hdr->name, 32);
    // the spec says it should be zero-terminated anyway, but this
    // comes from untrusted source, so we do our own termination
    hdr->name[31] = 0;
    hdr->attributes = dec.UInt16();
    hdr->version = dec.UInt16();
    hdr->createTime = dec.UInt32();
    hdr->modifyTime = dec.UInt32();
    hdr->backupTime = dec.UInt32();
    hdr->modificationNumber = dec.UInt32();
    hdr->appInfoID = dec.UInt32();
    hdr->sortInfoID = dec.UInt32();
    ZeroMemory(hdr->typeCreator, dimof(hdr->typeCreator));
    dec.Bytes(hdr->typeCreator, 8);
    hdr->idSeed = dec.UInt32();
    hdr->nextRecordList = dec.UInt32();
    hdr->numRecords = dec.UInt16();
    return dec.IsOk();
}

bool PdbReader::ParseHeader() {
    ReportIf(recInfos.size() > 0);

    ByteOrderDecoder dec(data, dataSize, ByteOrderDecoder::BigEndian);
    bool ok = DecodePdbHeader(dec, &hdr);
    if (!ok) {
        return false;
    }

    if (0 == hdr.numRecords) {
        return false;
    }

    size_t nRecs = hdr.numRecords;
    size_t minOffset = kPdbHeaderLen + (nRecs * kPdbRecordHeaderLen);
    size_t maxOffset = dataSize;

    for (size_t i = 0; i < nRecs; i++) {
        PdbRecordHeader recHdr;
        recHdr.offset = dec.UInt32();
        recHdr.flags = dec.UInt8();
        dec.Bytes(recHdr.uniqueID, dimof(recHdr.uniqueID));
        u32 off = recHdr.offset;
        if ((off < minOffset) || (off > maxOffset)) {
            return false;
        }
        recInfos.Append(recHdr);
    }
    if (!dec.IsOk()) {
        return false;
    }

    // validate offsets
    u32 prevOff = recInfos[0].offset;
    for (size_t i = 0; i < nRecs - 1; i++) {
        if (recInfos[i].offset > recInfos[i + 1].offset) {
            return false;
        }
    }

    // technically PDB record size should be less than 64K,
    // but it's not true for mobi files, so we don't validate that

    return true;
}

const char* PdbReader::GetDbType() {
    return hdr.typeCreator;
}

size_t PdbReader::GetRecordCount() {
    return recInfos.size();
}

// don't free, memory is owned by us
ByteSlice PdbReader::GetRecord(size_t recNo) {
    size_t nRecs = recInfos.size();
    ReportIf(recNo >= nRecs);
    if (recNo >= nRecs) {
        return {};
    }
    size_t off = recInfos[recNo].offset;
    size_t nextOff = dataSize;
    if (recNo != nRecs - 1) {
        nextOff = recInfos[recNo + 1].offset;
    }
    if (off > nextOff) {
        return {};
    }
    size_t size = nextOff - off;
    return {(u8*)data + off, size};
}

PdbReader* PdbReader::CreateFromData(const ByteSlice& d) {
    if (d.empty()) {
        return nullptr;
    }
    PdbReader* reader = new PdbReader();
    if (!reader->Parse(d)) {
        delete reader;
        return nullptr;
    }
    return reader;
}

PdbReader* PdbReader::CreateFromFile(const char* path) {
    ByteSlice d = file::ReadFile(path);
    return CreateFromData(d);
}

PdbReader* PdbReader::CreateFromStream(IStream* stream) {
    ByteSlice d = GetDataFromStream(stream, nullptr);
    return CreateFromData(d);
}

// values for typeCreator
#define MOBI_TYPE_CREATOR "BOOKMOBI"
#define PALMDOC_TYPE_CREATOR "TEXtREAd"
#define TEALDOC_TYPE_CREATOR "TEXtTlDc"
#define PLUCKER_TYPE_CREATOR "DataPlkr"

PdbDocType GetPdbDocType(const char* typeCreator) {
    if (memeq(typeCreator, MOBI_TYPE_CREATOR, 8)) {
        return PdbDocType::Mobipocket;
    }
    if (memeq(typeCreator, PALMDOC_TYPE_CREATOR, 8)) {
        return PdbDocType::PalmDoc;
    }
    if (memeq(typeCreator, TEALDOC_TYPE_CREATOR, 8)) {
        return PdbDocType::TealDoc;
    }
    if (memeq(typeCreator, PLUCKER_TYPE_CREATOR, 8)) {
        return PdbDocType::Plucker;
    }
    return PdbDocType::Unknown;
}

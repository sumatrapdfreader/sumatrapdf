/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/ByteReaderWriter.h"

#include "PalmDbReader.h"

// size of PdbHeader
constexpr int kPdbHeaderLen = 78;
// size of PdbRecordHeader
constexpr int kPdbRecordHeaderLen = 8;

// Takes ownership of d
bool PdbReader::Parse(Str d) {
    data = (u8*)d.s;
    dataSize = d.len;
    return ParseHeader();
}

PdbReader::~PdbReader() {
    free((void*)data);
}

static bool DecodePdbHeader(ByteReader& dec, PdbHeader* hdr) {
    dec.Bytes(hdr->name, 32);
    // the spec says it should be zero-terminated anyway, but this
    // comes from untrusted source, so we do our own termination
    hdr->name[31] = 0;
    hdr->attributes = dec.UInt16BE();
    hdr->version = dec.UInt16BE();
    hdr->createTime = dec.UInt32BE();
    hdr->modifyTime = dec.UInt32BE();
    hdr->backupTime = dec.UInt32BE();
    hdr->modificationNumber = dec.UInt32BE();
    hdr->appInfoID = dec.UInt32BE();
    hdr->sortInfoID = dec.UInt32BE();
    ZeroMemory(hdr->typeCreator, dimof(hdr->typeCreator));
    dec.Bytes(hdr->typeCreator, 8);
    hdr->idSeed = dec.UInt32BE();
    hdr->nextRecordList = dec.UInt32BE();
    hdr->numRecords = dec.UInt16BE();
    return dec.IsOk();
}

bool PdbReader::ParseHeader() {
    ReportIf(len(recInfos) > 0);

    ByteReader dec(data, dataSize);
    bool ok = DecodePdbHeader(dec, &hdr);
    if (!ok) {
        return false;
    }

    if (0 == hdr.numRecords) {
        return false;
    }

    int nRecs = hdr.numRecords;
    int minOffset = kPdbHeaderLen + (nRecs * kPdbRecordHeaderLen);
    int maxOffset = dataSize;

    for (int i = 0; i < nRecs; i++) {
        PdbRecordHeader recHdr;
        recHdr.offset = dec.UInt32BE();
        recHdr.flags = dec.UInt8();
        dec.Bytes(recHdr.uniqueID, dimof(recHdr.uniqueID));
        int off = (int)recHdr.offset;
        if ((off < minOffset) || (off > maxOffset)) {
            return false;
        }
        VecAppend(recInfos, recHdr);
    }
    if (!dec.IsOk()) {
        return false;
    }

    // validate offsets
    for (int i = 0; i < nRecs - 1; i++) {
        if (recInfos[i].offset > recInfos[i + 1].offset) {
            return false;
        }
    }

    // technically PDB record size should be less than 64K,
    // but it's not true for mobi files, so we don't validate that

    return true;
}

Str PdbReader::GetDbType() {
    return Str(hdr.typeCreator, 8);
}

int PdbReader::GetRecordCount() {
    return len(recInfos);
}

// don't free, memory is owned by us
Str PdbReader::GetRecord(int recNo) {
    int nRecs = len(recInfos);
    ReportIf(recNo >= nRecs);
    if (recNo >= nRecs) {
        return {};
    }
    int off = (int)recInfos[recNo].offset;
    int nextOff = dataSize;
    if (recNo != nRecs - 1) {
        nextOff = (int)recInfos[recNo + 1].offset;
    }
    if (off > nextOff) {
        return {};
    }
    int size = nextOff - off;
    return Str((char*)((u8*)data + off), size);
}

PdbReader* PdbReader::CreateFromData(Str d) {
    if (len(d) == 0) {
        return nullptr;
    }
    PdbReader* reader = new PdbReader();
    if (!reader->Parse(d)) {
        delete reader;
        return nullptr;
    }
    return reader;
}

PdbReader* PdbReader::CreateFromFile(Str path) {
    Str d = file::ReadFile(path);
    return CreateFromData(d);
}

// values for typeCreator
constexpr const char* kMobiTypeCreator = "BOOKMOBI";
constexpr const char* kPalmDocTypeCreator = "TEXtREAd";
constexpr const char* kTealDocTypeCreator = "TEXtTlDc";
constexpr const char* kPluckerTypeCreator = "DataPlkr";

PdbDocType GetPdbDocType(Str typeCreator) {
    if (MemEq(typeCreator.s, kMobiTypeCreator, 8)) {
        return PdbDocType::Mobipocket;
    }
    if (MemEq(typeCreator.s, kPalmDocTypeCreator, 8)) {
        return PdbDocType::PalmDoc;
    }
    if (MemEq(typeCreator.s, kTealDocTypeCreator, 8)) {
        return PdbDocType::TealDoc;
    }
    if (MemEq(typeCreator.s, kPluckerTypeCreator, 8)) {
        return PdbDocType::Plucker;
    }
    return PdbDocType::Unknown;
}

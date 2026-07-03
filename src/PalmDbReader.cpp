/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
// #include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/ByteOrderDecoder.h"

#include "PalmDbReader.h"

// size of PdbHeader
#define kPdbHeaderLen 78
// size of PdbRecordHeader
#define kPdbRecordHeaderLen 8

// Takes ownership of d
bool PdbReader::Parse(Str d) {
    data = (u8*)d.s;
    dataSize = (size_t)d.len;
    return ParseHeader();
}

PdbReader::~PdbReader() {
    free((void*)data);
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
    ReportIf(len(recInfos) > 0);

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
    for (size_t i = 0; i < nRecs - 1; i++) {
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

size_t PdbReader::GetRecordCount() {
    return len(recInfos);
}

// don't free, memory is owned by us
Str PdbReader::GetRecord(size_t recNo) {
    size_t nRecs = (size_t)len(recInfos);
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
    return Str((char*)((u8*)data + off), (int)size);
}

PdbReader* PdbReader::CreateFromData(Str d) {
    if (str::IsEmpty(d)) {
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

PdbReader* PdbReader::CreateFromStream(IStream* stream) {
    Str d = GetDataFromStream(stream, nullptr);
    return CreateFromData(d);
}

// values for typeCreator
#define MOBI_TYPE_CREATOR "BOOKMOBI"
#define PALMDOC_TYPE_CREATOR "TEXtREAd"
#define TEALDOC_TYPE_CREATOR "TEXtTlDc"
#define PLUCKER_TYPE_CREATOR "DataPlkr"

PdbDocType GetPdbDocType(Str typeCreator) {
    if (memeq(typeCreator.s, MOBI_TYPE_CREATOR, 8)) {
        return PdbDocType::Mobipocket;
    }
    if (memeq(typeCreator.s, PALMDOC_TYPE_CREATOR, 8)) {
        return PdbDocType::PalmDoc;
    }
    if (memeq(typeCreator.s, TEALDOC_TYPE_CREATOR, 8)) {
        return PdbDocType::TealDoc;
    }
    if (memeq(typeCreator.s, PLUCKER_TYPE_CREATOR, 8)) {
        return PdbDocType::Plucker;
    }
    return PdbDocType::Unknown;
}

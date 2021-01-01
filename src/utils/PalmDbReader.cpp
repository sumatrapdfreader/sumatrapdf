/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "PalmDbReader.h"
#include "ByteOrderDecoder.h"
#include "FileUtil.h"

// size of PdbHeader
#define kPdbHeaderLen 78
// size of PdbRecordHeader
#define kPdbRecordHeaderLen 8

// Takes ownership of d
bool PdbReader::Parse(std::span<u8> d) {
    data = d.data();
    dataSize = d.size();
    if (!ParseHeader()) {
        return false;
    }
    return true;
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
    CrashIf(recInfos.size() > 0);

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
    for (size_t i = 1; i < nRecs - 1; i++) {
        u32 off = recInfos[i].offset;
        if (prevOff > off) {
            return false;
        }
        prevOff = off;
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
std::span<u8> PdbReader::GetRecord(size_t recNo) {
    size_t nRecs = recInfos.size();
    CrashIf(recNo >= nRecs);
    if (recNo >= nRecs) {
        return {};
    }
    size_t off = recInfos[recNo].offset;
    size_t nextOff = dataSize;
    if (recNo != nRecs - 1) {
        nextOff = recInfos[recNo + 1].offset;
    }
    CrashIf(off > nextOff);
    size_t size = nextOff - off;
    return {(u8*)data + off, size};
}

PdbReader* PdbReader::CreateFromData(std::span<u8> d) {
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
    auto d = file::ReadFile(path);
    std::span<u8> ds = {(u8*)d.data(), d.size()};
    return CreateFromData(ds);
}

#if OS_WIN
#include "WinUtil.h"

PdbReader* PdbReader::CreateFromFile(const WCHAR* filePath) {
    std::span<u8> d = file::ReadFile(filePath);
    return CreateFromData(d);
}

PdbReader* PdbReader::CreateFromStream(IStream* stream) {
    std::span<u8> d = GetDataFromStream(stream, nullptr);
    return CreateFromData(d);
}
#endif

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

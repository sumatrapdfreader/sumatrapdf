/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/ByteReaderWriter.h"

#include "PalmDbReader.h"

constexpr int kPdbHeaderLen = 78;
constexpr int kPdbRecordHeaderLen = 8;
constexpr int kPdbTypeCreatorOff = 60;
constexpr int kPdbTypeCreatorLen = 8;

PdbReader::~PdbReader() {
    free((void*)data);
}

// Record boundaries include the file end, so every record has a next offset.
bool PdbReader::Parse(Str d) {
    data = (u8*)d.s;
    int dataSize = len(d);
    ByteReader dec(data, dataSize);
    dec.Skip(kPdbHeaderLen - sizeofi(u16));
    int nRecs = dec.UInt16BE();
    if (!dec.IsOk() || nRecs == 0) {
        return false;
    }

    int previous = kPdbHeaderLen + nRecs * kPdbRecordHeaderLen;
    for (int i = 0; i < nRecs; i++) {
        int off = (int)dec.UInt32BE();
        dec.Skip(kPdbRecordHeaderLen - sizeofi(u32));
        if (off < previous || off > dataSize) {
            return false;
        }
        VecAppend(recordOffsets, off);
        previous = off;
    }
    if (!dec.IsOk()) {
        return false;
    }
    VecAppend(recordOffsets, dataSize);
    return true;
}

Str PdbReader::GetDbType() {
    return Str((char*)data + kPdbTypeCreatorOff, kPdbTypeCreatorLen);
}

int PdbReader::GetRecordCount() {
    return len(recordOffsets) - 1;
}

// don't free, memory is owned by us
Str PdbReader::GetRecord(int recNo) {
    int nRecs = GetRecordCount();
    ReportIf(recNo < 0 || recNo >= nRecs);
    if (recNo < 0 || recNo >= nRecs) {
        return {};
    }
    int off = recordOffsets[recNo];
    return Str((char*)data + off, recordOffsets[recNo + 1] - off);
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

PdbDocType GetPdbDocType(Str typeCreator) {
    static const struct {
        const char* signature;
        PdbDocType type;
    } types[] = {{"BOOKMOBI", PdbDocType::Mobipocket},
                 {"TEXtREAd", PdbDocType::PalmDoc},
                 {"TEXtTlDc", PdbDocType::TealDoc},
                 {"DataPlkr", PdbDocType::Plucker}};
    for (const auto& entry : types) {
        if (MemEq(typeCreator.s, entry.signature, kPdbTypeCreatorLen)) {
            return entry.type;
        }
    }
    return PdbDocType::Unknown;
}

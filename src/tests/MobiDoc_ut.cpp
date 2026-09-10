/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EbookBase.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"

#include "base/tests/UtAssert.h"

// a minimal MOBI: 78-byte PDB header, one 8-byte record header per record,
// then record 0 (PalmDoc header + MOBI header) and one text record
constexpr int kPdbHeaderLen = 78;
constexpr int kRecHeaderLen = 8;
constexpr int kPalmDocLen = 16;
constexpr int kMobiHdrLen = 116;

constexpr int kNumRecs = 2;
constexpr int kRec0Off = kPdbHeaderLen + kNumRecs * kRecHeaderLen;
constexpr int kRec0Len = kPalmDocLen + kMobiHdrLen;
constexpr int kRec1Off = kRec0Off + kRec0Len;
constexpr int kRec1Len = 4;
constexpr int kFileLen = kRec1Off + kRec1Len;

// big-endian writer over a zero-filled buffer, so Zeros() only has to skip
struct BeWriter {
    u8* d;
    int off = 0;

    void Zeros(int n) { off += n; }
    void U16(u16 v) {
        d[off++] = (u8)(v >> 8);
        d[off++] = (u8)(v & 0xff);
    }
    void U32(u32 v) {
        U16((u16)(v >> 16));
        U16((u16)(v & 0xffff));
    }
    void Bytes(const char* s, int n) {
        memcpy(d + off, s, (size_t)n);
        off += n;
    }
};

// PdbReader takes ownership of the returned bytes and free()s them
static Str MkMobi(u32 imageFirstRec) {
    u8* d = AllocArray<u8>(kFileLen);
    BeWriter w{d};

    w.Zeros(32);           // name
    w.Zeros(4);            // attributes, version
    w.Zeros(24);           // times, modification number, appInfoID, sortInfoID
    w.Bytes("BOOKMOBI", 8) // typeCreator
        ;
    w.Zeros(8); // idSeed, nextRecordList
    w.U16(kNumRecs);

    w.U32(kRec0Off);
    w.Zeros(4); // flags, uniqueID
    w.U32(kRec1Off);
    w.Zeros(4);

    // PalmDoc header: no compression, one text record
    w.U16(1); // compressionType: none
    w.Zeros(2);
    w.U32(kRec1Len); // uncompressedDocSize
    w.U16(1);        // recordsCount
    w.U16(kRec1Len); // maxRecSize
    w.Zeros(4);      // currPos

    w.Bytes("MOBI", 4);
    w.U32(kMobiHdrLen);
    w.U32(2);     // type: book
    w.U32(65001); // textEncoding: utf-8
    w.Zeros(76);  // uniqueId .. minRequiredMobiFormatVersion
    w.U32(imageFirstRec);
    w.Zeros(20); // huffman fields, exthFlags

    w.Bytes("hi\r\n", kRec1Len);
    ReportIf(w.off != kFileLen);
    return Str((char*)d, kFileLen);
}

void MobiDoc_UnitTests() {
    // an imageFirstRec that doesn't fit an int used to become a negative record
    // index, which PdbReader::GetRecord() then read out of bounds
    {
        MobiDoc* doc = MobiDoc::CreateFromData(MkMobi(0xfffffff0));
        utassert(doc != nullptr);
        utassert(doc->imageFirstRec == 0);
        utassert(doc->imagesCount == 0);
        delete doc;
    }

    // a record index past the last record is rejected the same way
    {
        MobiDoc* doc = MobiDoc::CreateFromData(MkMobi(kNumRecs));
        utassert(doc != nullptr);
        utassert(doc->imageFirstRec == 0);
        utassert(doc->imagesCount == 0);
        delete doc;
    }
}

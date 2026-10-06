/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ByteReaderWriter.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "gui/UIModels.h"

#include "base/HtmlTags.h"
#include "GumboHtmlParser.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EbookBase.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"

constexpr int kInvalidSize = -1;

// Parse mobi format http://wiki.mobileread.com/wiki/MOBI
constexpr int kCompressionNone = 1;
constexpr int kCompressionPalm = 2;
constexpr int kCompressionHuff = 17480;
constexpr int kCompressionUnsupportedDrm = -1;

constexpr int kEncryptionNone = 0;

struct PalmDocHeader {
    u16 compressionType = 0;
    u32 uncompressedDocSize = 0;
    u16 recordsCount = 0;
    u16 encrType = 0;
};
constexpr int kPalmDocHeaderLen = 16;

// http://wiki.mobileread.com/wiki/MOBI#PalmDOC_Header
static void DecodePalmDocHeader(const u8* buf, PalmDocHeader* hdr) {
    ByteReader d(buf, kPalmDocHeaderLen);
    hdr->compressionType = d.UInt16BE();
    d.Skip(sizeofi(u16));
    hdr->uncompressedDocSize = d.UInt32BE();
    hdr->recordsCount = d.UInt16BE();
    d.Skip(sizeofi(u16));
    hdr->encrType = d.UInt16BE();
    d.Skip(sizeofi(u16));

    ReportIf(kPalmDocHeaderLen != d.Offset());
}

constexpr int kMobiHeaderMinLen = 116;
constexpr int kMobiImageFirstRecOff = 92;
constexpr int kMobiDrmEntriesOff = 152;
constexpr int kMobiDrmHeaderLen = 164;
constexpr int kMobiExtraFlagsOff = 226;
constexpr int kMobiExtraFlagsHeaderLen = 228;
constexpr int kMobiTrailerMinVersion = 5;
constexpr u16 kMobiMultibyteFlag = 1;

struct MobiHeader {
    char id[4];
    u32 hdrLen;
    u32 type;
    u32 textEncoding;
    u32 minRequiredMobiFormatVersion;
    u32 imageFirstRec;
    u32 huffmanFirstRec;
    u32 huffmanRecCount;
    u32 exthFlags;
    u32 drmEntriesCount = (u32)-1;
    u16 extraDataFlags;
};

// Uncompress source data compressed with PalmDoc compression into a buffer.
// http://wiki.mobileread.com/wiki/PalmDOC#Format
// Returns false on decoding errors
static bool PalmdocUncompress(const u8* src, int srcLen, str::Builder& dst) {
    const u8* srcEnd = src + srcLen;
    while (src < srcEnd) {
        u8 c = *src++;
        if ((c >= 1) && (c <= 8)) {
            if (src + c > srcEnd) {
                return false;
            }
            dst.Append(Str((char*)src, (int)c));
            src += c;
        } else if (c < 128) {
            dst.AppendChar((char)c);
        } else if (c < 192) {
            if (src + 1 > srcEnd) {
                return false;
            }
            u16 c2 = (c << 8) | (u8)*src++;
            u16 back = (c2 >> 3) & 0x07ff;
            if (back > len(dst) || 0 == back) {
                return false;
            }
            for (u8 n = (c2 & 7) + 3; n > 0; n--) {
                char ctmp = dst[len(dst) - back];
                dst.AppendChar(ctmp);
            }
        } else {
            // c >= 192
            dst.AppendChar(' ');
            dst.AppendChar((char)(c ^ 0x80));
        }
    }

    return true;
}

constexpr int kHuffHeaderLen = 24;
constexpr int kCdicHeaderLen = 16;
constexpr int kCacheItemCount = 256;
constexpr int kCacheDataLen = kCacheItemCount * (int)sizeof(u32);
constexpr int kBaseTableItemCount = 64;
constexpr int kBaseTableDataLen = kBaseTableItemCount * (int)sizeof(u32);

constexpr int kHuffRecordMinLen = kHuffHeaderLen + kCacheDataLen + kBaseTableDataLen;

constexpr int kCdicsMax = 32;

struct HuffDicDecompressor {
    u32 cacheTable[kCacheItemCount]{};
    u32 baseTable[kBaseTableItemCount]{};

    int dictsCount = 0;
    // owned by the creator (in our case: by the PdbReader)
    u8* dicts[kCdicsMax]{};
    u32 dictSize[kCdicsMax]{};

    u32 codeLength = 0;

    int recursionDepth = 0;

    bool SetHuffData(u8* huffData, int huffDataLen);
    bool AddCdicData(u8* cdicData, u32 cdicDataLen);
    bool Decompress(u8* src, int srcSize, str::Builder& dst);
    bool DecodeOne(u32 code, str::Builder& dst);
};

bool HuffDicDecompressor::DecodeOne(u32 code, str::Builder& dst) {
    u16 dict = (u16)(code >> codeLength);
    if (dict >= dictsCount) {
        logf("invalid dict value\n");
        return false;
    }
    code &= ((1 << (codeLength)) - 1);
    u16 offset = UInt16BE(dicts[dict] + ((size_t)code * 2));

    if ((u32)offset + 2 > dictSize[dict]) {
        logf("invalid offset\n");
        return false;
    }
    u16 symLen = UInt16BE(dicts[dict] + offset);
    u8* p = dicts[dict] + offset + 2;
    if ((u32)(symLen & 0x7fff) > dictSize[dict] - offset - 2) {
        logf("invalid symLen\n");
        return false;
    }

    if (!(symLen & 0x8000)) {
        if (recursionDepth > 20) {
            logf("infinite recursion\n");
            return false;
        }
        recursionDepth++;
        if (!Decompress(p, symLen, dst)) {
            recursionDepth--;
            return false;
        }
        recursionDepth--;
    } else {
        symLen &= 0x7fff;
        if (symLen > 127) {
            logf("symLen too big\n");
            return false;
        }
        dst.Append(Str((char*)p, (int)symLen));
    }
    return true;
}

bool HuffDicDecompressor::Decompress(u8* src, int srcSize, str::Builder& dst) {
    int bitsConsumed = 0;
    u32 bits = 0;

    BitReader br(src, srcSize);

    for (;;) {
        if (bitsConsumed > br.BitsLeft()) {
            logf("not enough data\n");
            return false;
        }
        br.Eat(bitsConsumed);
        if (0 == br.BitsLeft()) {
            break;
        }

        bits = br.Peek(32);
        if (br.BitsLeft() < 8 && 0 == bits) {
            break;
        }
        u32 v = cacheTable[bits >> 24];
        u32 codeLen = v & 0x1f;
        if (!codeLen) {
            logf("corrupted table, zero code len\n");
            return false;
        }
        bool isTerminal = (v & 0x80) != 0;

        u32 code;
        if (isTerminal) {
            code = (v >> 8) - (bits >> (32 - codeLen));
        } else {
            u32 baseVal;
            codeLen -= 1;
            do {
                codeLen++;
                if (codeLen > 32) {
                    logf("code len > 32 bits\n");
                    return false;
                }
                baseVal = baseTable[(codeLen * 2) - 2];
                code = (bits >> (32 - codeLen));
            } while (baseVal > code);
            code = baseTable[(codeLen * 2) - 1] - (bits >> (32 - codeLen));
        }

        if (!DecodeOne(code, dst)) {
            return false;
        }
        bitsConsumed = (int)codeLen;
    }

    if (br.BitsLeft() > 0 && 0 != bits) {
        logf("compressed data left\n");
    }
    return true;
}

bool HuffDicDecompressor::SetHuffData(u8* huffData, int huffDataLen) {
    if (huffDataLen < kHuffRecordMinLen || !MemEq(huffData, "HUFF", 4)) {
        return false;
    }

    ByteReader d(huffData, huffDataLen);
    d.Skip(sizeofi(u32));
    u32 hdrLen = d.UInt32BE();
    u32 cacheOffset = d.UInt32BE();
    u32 baseOffset = d.UInt32BE();
    ReportIf(hdrLen != kHuffHeaderLen);
    if (hdrLen != kHuffHeaderLen || cacheOffset != kHuffHeaderLen || baseOffset != cacheOffset + kCacheDataLen) {
        return false;
    }
    d.Skip(kHuffHeaderLen - d.Offset());
    // Only the big-endian tables are needed.
    for (u32& v : cacheTable) {
        v = d.UInt32BE();
    }
    for (u32& v : baseTable) {
        v = d.UInt32BE();
    }
    ReportIf(d.Offset() != kHuffRecordMinLen);
    return true;
}

bool HuffDicDecompressor::AddCdicData(u8* cdicData, u32 cdicDataLen) {
    if (dictsCount >= kCdicsMax) {
        return false;
    }
    if (cdicDataLen < kCdicHeaderLen) {
        return false;
    }
    if (!str::EqN(StrL("CDIC"), Str((char*)cdicData, 4), 4)) {
        return false;
    }
    u32 hdrLen = UInt32BE(cdicData + 4);
    u32 codeLen = UInt32BE(cdicData + 12);
    if (codeLen == 0 || codeLen > 16) {
        return false;
    }
    if (0 == codeLength) {
        codeLength = codeLen;
    } else {
        ReportIf(codeLen != codeLength);
        codeLength = std::min(codeLength, codeLen);
    }
    ReportIf(hdrLen != kCdicHeaderLen);
    if (hdrLen != kCdicHeaderLen) {
        return false;
    }
    u32 size = cdicDataLen - hdrLen;

    u32 maxSize = 2u * (1u << codeLength);
    if (maxSize > size) {
        return false;
    }
    dicts[dictsCount] = cdicData + hdrLen;
    dictSize[dictsCount] = size;
    ++dictsCount;
    return true;
}

static void DecodeMobiDocHeader(const u8* buf, int bufLen, MobiHeader* hdr) {
    *hdr = {};
    ByteReader d(buf, bufLen);
    d.Bytes(hdr->id, sizeofi(hdr->id));
    hdr->hdrLen = d.UInt32BE();
    hdr->type = d.UInt32BE();
    hdr->textEncoding = d.UInt32BE();
    d.Skip(kMobiImageFirstRecOff - sizeofi(u32) - d.Offset());
    hdr->minRequiredMobiFormatVersion = d.UInt32BE();
    hdr->imageFirstRec = d.UInt32BE();
    hdr->huffmanFirstRec = d.UInt32BE();
    hdr->huffmanRecCount = d.UInt32BE();
    d.Skip(2 * sizeofi(u32));
    hdr->exthFlags = d.UInt32BE();

    if (hdr->hdrLen < kMobiDrmHeaderLen) {
        return;
    }
    d.Skip(kMobiDrmEntriesOff - d.Offset());
    hdr->drmEntriesCount = d.UInt32BE();

    if (hdr->hdrLen < kMobiExtraFlagsHeaderLen) {
        return;
    }
    d.Skip(kMobiExtraFlagsOff - d.Offset());
    hdr->extraDataFlags = d.UInt16BE();
}

static int CountMobiTrailers(u16 flags) {
    int count = 0;
    for (flags >>= 1; flags; flags >>= 1) {
        count += flags & 1;
    }
    return count;
}

static bool IsValidCompression(int comprType) {
    return (kCompressionNone == comprType) || (kCompressionPalm == comprType) || (kCompressionHuff == comprType);
}

MobiDoc::MobiDoc(Str filePath) {
    docTocIndex = -1;
    str::ReplaceWithCopy(&fileName, filePath);
}

MobiDoc::~MobiDoc() {
    FreeProps(props);
    str::Free(fileName);
    free(images);
    delete huffDic;
    delete pdbReader;
}

bool MobiDoc::ParseHeader() {
    ReportIf(!pdbReader);
    if (!pdbReader) {
        return false;
    }

    if (pdbReader->GetRecordCount() == 0) {
        return false;
    }

    docType = GetPdbDocType(pdbReader->GetDbType());
    if (PdbDocType::Unknown == docType) {
        logf("unknown pdb type/creator\n");
        return false;
    }

    auto rec = pdbReader->GetRecord(0);
    u8* firstRecData = (u8*)rec.s;
    int recSize = rec.len;
    if (!firstRecData || recSize < kPalmDocHeaderLen) {
        log(StrL("failed to read record 0\n"));
        return false;
    }

    PalmDocHeader palmDocHdr;
    DecodePalmDocHeader(firstRecData, &palmDocHdr);
    compressionType = palmDocHdr.compressionType;
    if (!IsValidCompression(compressionType)) {
        logf("MobiDoc::ParseHeader: unknown compression type %d\n", (int)compressionType);
        return false;
    }
    if (PdbDocType::Mobipocket == docType) {
        // TODO: this needs to be surfaced to the client so
        // that we can show the right error message
        if (palmDocHdr.encrType != kEncryptionNone) {
            logf("encryption is unsupported\n");
            return false;
        }
    }
    docRecCount = palmDocHdr.recordsCount;
    if (docRecCount == pdbReader->GetRecordCount()) {
        // catch the case where a broken document has an off-by-one error
        // cf. https://code.google.com/archive/p/sumatrapdf/issues/2529
        docRecCount--;
    }
    constexpr u32 kMaxMobiTextSize = 256 * 1024 * 1024;
    if (palmDocHdr.uncompressedDocSize > kMaxMobiTextSize) {
        logf("MOBI text is too large\n");
        return false;
    }
    docUncompressedSize = (int)palmDocHdr.uncompressedDocSize;

    if (kPalmDocHeaderLen == recSize) {
        // TODO: calculate imageFirstRec / imagesCount
        return PdbDocType::Mobipocket != docType;
    }
    if (kPalmDocHeaderLen + kMobiHeaderMinLen > recSize) {
        logf("not enough data for decoding MobiHeader\n");
        // id and hdrLen
        return false;
    }

    MobiHeader mobiHdr;
    int mobiDataLen = recSize - kPalmDocHeaderLen;
    DecodeMobiDocHeader(firstRecData + kPalmDocHeaderLen, mobiDataLen, &mobiHdr);
    if (!str::EqN(StrL("MOBI"), Str(mobiHdr.id, 4), 4)) {
        logf("MobiHeader.id is not 'MOBI'\n");
        return false;
    }
    if (mobiHdr.drmEntriesCount != (u32)-1) {
        logf("DRM is unsupported\n");
        // load an empty document and display a warning
        compressionType = kCompressionUnsupportedDrm;
        Str v = strconv::WStrToCodePage(mobiHdr.textEncoding, WStrL(L"DRM"));
        AddPropOwned(props, DocProp::UnsupportedFeatures, v);
        str::Free(v);
    }
    textEncoding = (int)mobiHdr.textEncoding;

    // compare unsigned: a bogus imageFirstRec of e.g. 0xfffffff0 would pass as
    // a negative int and index records before the first one
    if (mobiHdr.imageFirstRec < (u32)pdbReader->GetRecordCount()) {
        imageFirstRec = (int)mobiHdr.imageFirstRec;
        if (0 == imageFirstRec) {
            // I don't think this should ever happen but I've seen it
            imagesCount = 0;
        } else {
            imagesCount = pdbReader->GetRecordCount() - imageFirstRec;
        }
    }
    // compare unsigned: a hdrLen >= 0x80000000 would pass as a negative int
    // and the EXTH header would be read far past the end of the record
    if (mobiHdr.hdrLen > (u32)(recSize - kPalmDocHeaderLen)) {
        logf("MobiHeader too big\n");
        return false;
    }

    if (mobiHdr.hdrLen >= kMobiExtraFlagsHeaderLen) {
        u16 flags = mobiHdr.extraDataFlags;
        multibyte = ((flags & kMobiMultibyteFlag) != 0);
        trailersCount += CountMobiTrailers(flags);
    }

    if (kCompressionHuff == compressionType) {
        ReportIf(PdbDocType::Mobipocket != docType);
        if (mobiHdr.huffmanFirstRec > (u32)INT_MAX - (u32)kCdicsMax) {
            return false;
        }
        rec = pdbReader->GetRecord((int)mobiHdr.huffmanFirstRec);
        int huffRecSize = rec.len;
        u8* recData = (u8*)rec.s;
        if (!recData) {
            return false;
        }
        ReportIf(nullptr != huffDic);
        huffDic = new HuffDicDecompressor();
        if (!huffDic->SetHuffData(recData, huffRecSize)) {
            return false;
        }
        if (mobiHdr.huffmanRecCount < 1 || mobiHdr.huffmanRecCount > (u32)kCdicsMax + 1) {
            logf("MobiDoc::ParseHeader: huffmanRecCount: %u\n", mobiHdr.huffmanRecCount);
            return false;
        }
        int cdicsCount = (int)mobiHdr.huffmanRecCount - 1;
        for (int i = 0; i < cdicsCount; i++) {
            rec = pdbReader->GetRecord((int)mobiHdr.huffmanFirstRec + 1 + i);
            recData = (u8*)rec.s;
            huffRecSize = rec.len;
            if (!recData) {
                return false;
            }
            if (huffRecSize > (u32)-1) {
                return false;
            }
            if (!huffDic->AddCdicData(recData, (u32)huffRecSize)) {
                return false;
            }
        }
    }

    if ((mobiHdr.exthFlags & 0x40)) {
        u32 offset = kPalmDocHeaderLen + mobiHdr.hdrLen;
        DecodeExthHeader(firstRecData + offset, (int)(recSize - offset));
    }

    LoadImages();
    return true;
}

bool MobiDoc::DecodeExthHeader(const u8* data, int dataLen) {
    if (dataLen < 12 || !MemEq(data, "EXTH", 4)) {
        return false;
    }

    ByteReader d(data, dataLen);
    d.Skip(4);
    u32 hdrLen = d.UInt32BE();
    u32 count = d.UInt32BE();
    if (hdrLen > (u32)dataLen) {
        return false;
    }

    for (u32 i = 0; i < count; i++) {
        if (d.Offset() > dataLen - 8) {
            return false;
        }
        u32 type = d.UInt32BE();
        u32 length = d.UInt32BE();
        int recLen = (int)length;
        if (recLen < 8 || recLen > dataLen - d.Offset() + 8) {
            return false;
        }
        d.Skip(recLen - 8);

        DocProp prop = DocProp::None;
        switch (type) {
            case 100:
                prop = DocProp::Author;
                break;
            case 105:
                prop = DocProp::Subject;
                break;
            case 106:
                prop = DocProp::CreationDate;
                break;
            case 108:
                prop = DocProp::CreatorApp;
                break;
            case 109:
                prop = DocProp::Copyright;
                break;
            case 201:
                if (length == 12 && imageFirstRec) {
                    d.Unskip(4);
                    coverImageRec = imageFirstRec + (int)d.UInt32BE();
                }
                continue;
            case 503:
                prop = DocProp::Title;
                break;
            default:
                continue;
        }
        TempStr value = str::DupTemp(Str((char*)(data + d.Offset() - length + 8), (int)length - 8));
        if (len(value) > 0) {
            AddPropOwned(props, prop, value);
        }
    }

    return true;
}

constexpr u32 kEofRec = 0xe98e0d0a;
constexpr int kFlisRec = 0x464c4953; // 'FLIS'
constexpr int kFcisRec = 0x46434953; // 'FCIS
constexpr int kFdstRec = 0x46445354; // 'FDST'
constexpr int kDatpRec = 0x44415450; // 'DATP'
constexpr int kSrcsRec = 0x53524353; // 'SRCS'
constexpr int kVideRec = 0x56494445; // 'VIDE'
constexpr int kRescRec = 0x52455343; // 'RESC'

static bool IsEofRecord(Str d) {
    return (4 == d.len) && (kEofRec == UInt32BE((u8*)d.s));
}

static bool KnownNonImageRec(Str d) {
    if (d.len < 4) {
        return false;
    }
    u32 sig = UInt32BE((u8*)d.s);

    switch (sig) {
        case kFlisRec:
        case kFcisRec:
        case kFdstRec:
        case kDatpRec:
        case kSrcsRec:
        case kVideRec:
        case kRescRec:
            return true;
    }
    return false;
}

static bool KnownImageFormat(Str d) {
    FileType kind = GuessFileTypeFromData(d);
    return kind != FileType::Unknown;
}

// return false if we should stop loading images (because we
// encountered eof record or ran out of memory)
bool MobiDoc::LoadImage(int imageNo) {
    int imageRec = imageFirstRec + imageNo;

    auto rec = pdbReader->GetRecord(imageRec);
    if (len(rec) < 4) {
        return false;
    }
    if (IsEofRecord(rec)) {
        return false;
    }
    if (KnownNonImageRec(rec)) {
        return true;
    }
    if (!KnownImageFormat(rec)) {
        u32 sig = UInt32BE((u8*)rec.s);
        logf("MobiDoc::LoadImage: unknown record type 0x%08X\n", sig);
        return true;
    }
    images[imageNo] = rec;
    return true;
}

void MobiDoc::LoadImages() {
    if (0 == imagesCount) {
        return;
    }
    images = AllocArray<Str>(imagesCount);

    for (int i = 0; i < imagesCount; i++) {
        if (!LoadImage(i)) {
            return;
        }
    }
}

// imgRecIndex corresponds to recindex attribute of <img> tag
// as far as I can tell, this means: it starts at 1
// returns nullptr if there is no image (e.g. it's not a format we
// recognize)
Str MobiDoc::GetImage(int imgRecIndex) const {
    if ((imgRecIndex > (int)imagesCount) || (imgRecIndex < 1)) {
        return {};
    }
    --imgRecIndex;
    if (len(images[imgRecIndex]) == 0) {
        return {};
    }
    return images[imgRecIndex];
}

Str MobiDoc::GetCoverImage() {
    if (!coverImageRec || coverImageRec < imageFirstRec) {
        return {};
    }
    int imageNo = coverImageRec - imageFirstRec;
    if (imageNo >= imagesCount || len(images[imageNo]) == 0) {
        return {};
    }
    return images[imageNo];
}

// each record can have extra data at the end, which we must discard
// returns kInvalidSize on error
static int GetRealRecordSize(const u8* recData, int recLen, int trailersCount, bool multibyte) {
    for (int i = 0; i < trailersCount; i++) {
        if (recLen < 4) {
            return kInvalidSize;
        }
        u32 n = 0;
        for (int j = 0; j < 4; j++) {
            u8 v = recData[recLen - 4 + j];
            if (0 != (v & 0x80)) {
                n = 0;
            }
            n = (n << 7) | (v & 0x7f);
        }
        if ((int)n > recLen) {
            return kInvalidSize;
        }
        recLen -= (int)n;
    }

    if (multibyte) {
        if (0 == recLen) {
            return kInvalidSize;
        }
        u8 n = (recData[recLen - 1] & 3) + 1;
        if (n > recLen) {
            return kInvalidSize;
        }
        recLen -= n;
    }

    return recLen;
}

// Load a given record of a document into strOut, uncompressing if necessary.
// Returns false if error.
bool MobiDoc::LoadDocRecordIntoBuffer(int recNo, str::Builder& strOut) {
    auto rec = pdbReader->GetRecord(recNo);
    u8* recData = (u8*)rec.s;
    if (nullptr == recData) {
        return false;
    }
    int recSize = GetRealRecordSize((const u8*)recData, rec.len, trailersCount, multibyte);
    if (kInvalidSize == recSize) {
        return false;
    }

    if (kCompressionNone == compressionType) {
        strOut.Append(Str((char*)recData, recSize));
        return true;
    }
    if (kCompressionPalm == compressionType) {
        bool ok = PalmdocUncompress(recData, recSize, strOut);
        if (!ok) {
            logf("PalmDoc decompression failed\n");
        }
        return ok;
    }
    if (kCompressionHuff == compressionType && huffDic) {
        bool ok = huffDic->Decompress(recData, recSize, strOut);
        if (!ok) {
            logf("HuffDic decompression failed\n");
        }
        return ok;
    }
    if (kCompressionUnsupportedDrm == compressionType) {
        // ensure a single blank page
        if (1 == recNo) {
            strOut.Append(StrL("&nbsp;"));
        }
        return true;
    }

    CrashMe();
    return false;
}

bool MobiDoc::LoadForPdbReader(PdbReader* pdbReader) {
    this->pdbReader = pdbReader;
    if (!ParseHeader()) {
        return false;
    }

    // Print Replica / AZW4 is a PDF in a MOBI wrapper. Do not treat the
    // binary records as HTML (issue #1315).
    if (pdbReader->GetRecordCount() >= 2) {
        auto rec1 = pdbReader->GetRecord(1);
        if (len(rec1) >= 4 && MemEq(rec1.s, "%MOP", 4)) {
            logf("MobiDoc: Print Replica / AZW4, not a MOBI ebook\n");
            return false;
        }
    }

    ReportIf(len(doc) != 0);
    doc.Reset();
    doc.cap = docUncompressedSize; // capacity hint, same trick as ByteWriter ctor
    int nFailed = 0;
    for (int i = 1; i <= docRecCount; i++) {
        if (!LoadDocRecordIntoBuffer(i, doc)) {
            nFailed++;
        }
    }

    // TODO: this is a heuristic for https://github.com/sumatrapdfreader/sumatrapdf/issues/1314
    // It has 29 records that fail to decompress because infinite recursion
    // is detected.
    // Figure out if this is a bug in my decoding.
    if (nFailed > docRecCount / 2) {
        if (CountLoadedImages() < 2) {
            return false;
        }
        // KF8 / AZW3 image books often have a tiny or undecompressible PalmDoc
        // stub; keep going so MaybeSynthesizeImagePages can use the JPEGs.
        logf("MobiDoc: %d/%d text records failed, falling back to images\n", nFailed, docRecCount);
        doc.Reset();
    }

    // replace unexpected \0 with spaces
    // https://code.google.com/archive/p/sumatrapdf/issues/2529
    Str docStr = ToStr(doc);
    u8* s = (u8*)docStr.s;
    u8* end = s + len(doc);
    while ((s = (u8*)memchr(s, 0, (size_t)(end - s))) != nullptr) {
        *s = ' ';
    }
    if (textEncoding != CP_UTF8) {
        TempStr docUtf8 = strconv::ToMultiByteTemp(ToStr(doc), textEncoding, CP_UTF8);
        if (docUtf8) {
            doc.Reset();
            doc.Append(docUtf8);
        }
    }
    MaybeSynthesizeImagePages();
    return true;
}

int MobiDoc::CountLoadedImages() const {
    int n = 0;
    for (int i = 0; i < imagesCount; i++) {
        if (len(images[i]) > 0) {
            n++;
        }
    }
    return n;
}

// KF8 <img src="kindle:embed:XXXX"> uses a base-32 resource id (alphabet 0-9A-V).
int KindleEmbedToRecIndex(Str src) {
    Str prefix = StrL("kindle:embed:");
    if (!str::StartsWithI(src, prefix)) {
        return 0;
    }
    const char* p = src.s + len(prefix);
    const char* end = src.s + len(src);
    int n = 0;
    bool any = false;
    while (p < end) {
        char c = *p;
        int digit = -1;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'A' && c <= 'V') {
            digit = 10 + (c - 'A');
        } else if (c >= 'a' && c <= 'v') {
            digit = 10 + (c - 'a');
        } else {
            break;
        }
        n = (n * 32) + digit;
        any = true;
        p++;
    }
    return any ? n : 0;
}

static void CollectKindleEmbedRecIndexes(Str html, Vec<int>& out) {
    Str prefix = StrL("kindle:embed:");
    if (!html.s || len(html) < len(prefix)) {
        return;
    }
    const char* p = html.s;
    const char* end = html.s + len(html);
    while (p + len(prefix) <= end) {
        if (*p != 'k' && *p != 'K') {
            p++;
            continue;
        }
        Str rest(p, (int)(end - p));
        if (!str::StartsWithI(rest, prefix)) {
            p++;
            continue;
        }
        int n = KindleEmbedToRecIndex(rest);
        if (n > 0) {
            VecAppend(out, n);
        }
        p += len(prefix);
    }
}

static void EmitRecindexPages(str::Builder& doc, const Vec<int>& recs) {
    doc.Reset();
    doc.Append(StrL("<html><body>"));
    for (int i = 0; i < len(recs); i++) {
        doc.Append(fmt("<img recindex=\"%d\"/><mbp:pagebreak/>", recs[i]));
    }
    doc.Append(StrL("</body></html>"));
}

// AZW3 / KF8 fixed-layout books (Kindle comics, photo books, some textbooks)
// store each page as a JPEG in the PDB. PalmDoc markup is either empty or KF8
// fragments with kindle:embed (and leftover CSS). EngineMobi would otherwise
// show a blank document or CSS junk (issue #4315). Classic MOBI recindex
// markup is left alone. Reflowable KF8 that uses kindle:embed without a
// viewport stays as-is so MobiFormatter can resolve the images in place.
void MobiDoc::MaybeSynthesizeImagePages() {
    int nImg = CountLoadedImages();
    if (nImg < 2) {
        return;
    }
    Str html = ToStr(doc);
    if (html && str::ContainsI(html, StrL("recindex"))) {
        return;
    }

    Vec<int> embedIdx;
    CollectKindleEmbedRecIndexes(html, embedIdx);
    bool fixedLayout =
        html && (str::ContainsI(html, StrL("name=\"viewport\"")) || str::ContainsI(html, StrL("name='viewport'")));
    if (len(embedIdx) >= 2 && fixedLayout) {
        logf("MobiDoc: synthesizing %d pages from kindle:embed (htmlLen=%d)\n", len(embedIdx), len(html));
        EmitRecindexPages(doc, embedIdx);
        return;
    }
    if (len(embedIdx) >= 2) {
        return;
    }

    int maxImgLen = 0;
    for (int i = 0; i < imagesCount; i++) {
        maxImgLen = std::max(maxImgLen, len(images[i]));
    }
    // Drop HD-media thumbnails (often ~10KB next to 200KB page JPEGs).
    int minKeep = maxImgLen / 8;
    logf("MobiDoc: synthesizing %d image pages (htmlLen=%d, minKeep=%d)\n", nImg, len(html), minKeep);
    int coverNo = -1;
    if (coverImageRec >= imageFirstRec) {
        coverNo = coverImageRec - imageFirstRec;
    }
    Vec<int> recs;
    for (int i = 0; i < imagesCount; i++) {
        int imgLen = len(images[i]);
        if (imgLen == 0 || imgLen < minKeep) {
            continue;
        }
        if (i == coverNo) {
            // MobiFormatter already emits the cover on its own page
            continue;
        }
        VecAppend(recs, i + 1);
    }
    if (len(recs) < 2) {
        return;
    }
    EmitRecindexPages(doc, recs);
}

// don't free the result
Str MobiDoc::GetHtmlData() const {
    if (len(doc) > 0) {
        return ToStr(doc);
    }
    return {};
}

TempStr MobiDoc::GetPropertyTemp(DocProp prop) {
    Str v = GetPropValueTemp(props, prop);
    if (len(v) == 0) {
        return {};
    }
    return strconv::StrToUtf8Temp(v, textEncoding);
}

// First <reference type="toc" filepos="N"/>; scan, don't gumbo-parse the book.
static int FindMobiTocFilepos(Str html) {
    Str kTag = StrL("<reference");
    Str rest = html;
    while (len(rest) > 0) {
        int idx = str::IndexOfI(rest, kTag);
        if (idx < 0) {
            return -1;
        }
        int skip = idx + len(kTag);
        rest = Str(rest.s + skip, len(rest) - skip);
        Str attrs;
        if (!str::CutChar(rest, '>', &attrs, &rest)) {
            return -1;
        }
        bool isToc = false;
        int filepos = -1;
        while (len(attrs) > 0) {
            str::TrimWs(attrs);
            if (len(attrs) == 0 || attrs.s[0] == '/') {
                break;
            }
            int n = 0;
            while (n < len(attrs) && !str::IsWs(attrs.s[n]) && attrs.s[n] != '=') {
                n++;
            }
            Str name(attrs.s, n);
            attrs = Str(attrs.s + n, len(attrs) - n);
            str::TrimWs(attrs);
            if (!str::TrimPrefix(attrs, StrL("="))) {
                continue;
            }
            str::TrimWs(attrs);
            Str val;
            if (len(attrs) > 0 && (attrs.s[0] == '"' || attrs.s[0] == '\'')) {
                char quote = attrs.s[0];
                attrs = Str(attrs.s + 1, len(attrs) - 1);
                str::CutChar(attrs, quote, &val, &attrs);
            } else {
                n = 0;
                while (n < len(attrs) && !str::IsWs(attrs.s[n]) && attrs.s[n] != '/') {
                    n++;
                }
                val = Str(attrs.s, n);
                attrs = Str(attrs.s + n, len(attrs) - n);
            }
            if (str::EqI(name, StrL("type")) && str::EqI(val, StrL("toc"))) {
                isToc = true;
            } else if (str::EqI(name, StrL("filepos"))) {
                unsigned int parsed = 0;
                if (!str::IsNull(str::Parse(val, "%u%$", &parsed))) {
                    filepos = (int)parsed;
                }
            }
        }
        if (isToc && filepos >= 0) {
            return filepos;
        }
    }
    return -1;
}

bool MobiDoc::HasToc() {
    if (docTocIndex != -1) {
        return docTocIndex < len(doc);
    }
    docTocIndex = len(doc); // no ToC
    int filepos = FindMobiTocFilepos(ToStr(doc));
    if (filepos >= 0 && filepos < len(doc)) {
        docTocIndex = filepos;
    }
    return docTocIndex < len(doc);
}

static void AppendDeepText(const GumboNode* root, str::Builder& sb) {
    // iterative pre-order DFS so a deeply nested element can't overflow the stack
    Vec<const GumboNode*> toVisit;
    VecAppend(toVisit, root);
    while (len(toVisit) > 0) {
        const GumboNode* node = VecPop(toVisit);
        if (!node) {
            continue;
        }
        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_CDATA || node->type == GUMBO_NODE_WHITESPACE) {
            sb.Append(Str(node->v.text.text));
            continue;
        }
        if (node->type != GUMBO_NODE_ELEMENT) {
            continue;
        }
        const GumboVector* children = &node->v.element.children;
        // push in reverse so children are visited (and text appended) in document order
        for (unsigned int i = children->length; i > 0; i--) {
            VecAppend(toVisit, (const GumboNode*)children->data[i - 1]);
        }
    }
}

struct MobiTocWalker {
    EbookTocVisitor* visitor = nullptr;

    void Walk(const GumboNode* root);
};

// (node, level) pair for the iterative walk below
struct MobiTocWalkItem {
    const GumboNode* node;
    int level;
};

// Iterative pre-order walk (was recursive) so a deeply nested ToC region can't
// overflow the stack. `level` is carried per node instead of being tracked via
// recursion depth. We stop at the first <mbp:pagebreak> in document order, like
// the recursive version did.
void MobiTocWalker::Walk(const GumboNode* root) {
    Vec<MobiTocWalkItem> stack;
    VecAppend(stack, {root, 0});
    while (len(stack) > 0) {
        MobiTocWalkItem it = VecPop(stack);
        const GumboNode* node = it.node;
        int level = it.level;
        if (!node) {
            continue;
        }
        const GumboVector* children = GumboChildrenOf(node);
        int childLevel = level;
        if (node->type == GUMBO_NODE_ELEMENT) {
            if (GumboTagNameIs(node, StrL("mbp:pagebreak"))) {
                return;
            }
            if (GumboTagNameIs(node, StrL("a"))) {
                const GumboAttribute* attr = gumbo_get_attribute(&node->v.element.attributes, "filepos");
                if (!attr) {
                    attr = gumbo_get_attribute(&node->v.element.attributes, "href");
                }
                if (attr) {
                    str::Builder text;
                    AppendDeepText(node, text);
                    if (len(text) > 0) {
                        visitor->Visit(ToStr(text), Str(attr->value), level);
                    }
                }
                continue; // don't descend into the <a>'s children
            }
            bool isLevel = GumboTagNameIs(node, StrL("blockquote")) || GumboTagNameIs(node, StrL("ul")) ||
                           GumboTagNameIs(node, StrL("ol"));
            childLevel = isLevel ? level + 1 : level;
        }
        if (children) {
            // push in reverse so children are visited in document order
            for (unsigned int i = children->length; i > 0; i--) {
                VecAppend(stack, {(const GumboNode*)children->data[i - 1], childLevel});
            }
        }
    }
}

bool MobiDoc::ParseToc(EbookTocVisitor* visitor) {
    if (!HasToc()) {
        return false;
    }

    // there doesn't seem to be a standard for Mobi ToCs, so we try to
    // determine the author's intentions by looking at commonly used tags
    Str docStr = ToStr(doc);
    int tocLen = len(doc) - docTocIndex;
    Str rest(docStr.s + docTocIndex, tocLen);
    // walker stops at the first pagebreak; don't gumbo-parse the rest of the book
    int pb = str::IndexOfI(rest, StrL("<mbp:pagebreak"));
    if (pb >= 0) {
        tocLen = pb;
    }
    Str tocSlice(docStr.s + docTocIndex, tocLen);
    GumboDoc toc(tocSlice, GumboMode::Html);
    if (!toc.Document()) {
        return false;
    }

    MobiTocWalker walker;
    walker.visitor = visitor;
    walker.Walk(toc.Document());

    return true;
}

bool MobiDoc::IsSupportedFileType(FileType kind) {
    return kind == FileType::Mobi;
}

MobiDoc* MobiDoc::CreateFromFile(Str path) {
    MobiDoc* mb = new MobiDoc(path);
    PdbReader* pdbReader = PdbReader::CreateFromFile(path);
    if (!pdbReader) {
        logf("MobiDoc::CreateFromFile: PdbReader failed for '%s'\n", path);
        delete mb;
        return nullptr;
    }
    if (!mb->LoadForPdbReader(pdbReader)) {
        logf("MobiDoc::CreateFromFile: LoadForPdbReader failed for '%s'\n", path);
        delete mb;
        return nullptr;
    }
    return mb;
}

MobiDoc* MobiDoc::CreateFromData(Str data) {
    MobiDoc* mb = new MobiDoc(Str());
    PdbReader* pdbReader = PdbReader::CreateFromData(str::Dup(data));
    if (!pdbReader || !mb->LoadForPdbReader(pdbReader)) {
        delete mb;
        return nullptr;
    }
    return mb;
}

// First section of the first %MOP table is the PDF (KindleUnpack processPrintReplica).
static Str ExtractPdfFromMopRaw(Str raw) {
    if (len(raw) < 8) {
        return {};
    }
    if (!MemEq(raw.s, "%MOP", 4)) {
        int idx = str::IndexOf(raw, StrL("%PDF-"));
        if (idx < 0) {
            return {};
        }
        return str::Dup(Str(raw.s + idx, raw.len - idx));
    }

    ByteReader d(raw);
    d.Skip(4);
    u32 numTables = d.UInt32BE();
    if (!d.IsOk() || numTables == 0 || numTables > 32) {
        return {};
    }
    // All per-table section counts come first, then the section index.
    for (u32 t = 0; t < numTables; t++) {
        d.UInt32BE();
    }
    if (!d.IsOk()) {
        return {};
    }
    u32 sectionOffset = d.UInt32BE();
    u32 sectionLength = d.UInt32BE();
    if (!d.IsOk()) {
        return {};
    }
    if (sectionOffset > (u32)raw.len || sectionLength > (u32)raw.len - sectionOffset) {
        return {};
    }
    if (sectionLength < 5) {
        return {};
    }
    Str pdf(raw.s + (int)sectionOffset, (int)sectionLength);
    if (!str::StartsWith(pdf, StrL("%PDF-"))) {
        logf("ExtractPdfFromPrintReplica: first %MOP section is not a PDF\n");
        return {};
    }
    return str::Dup(pdf);
}

static Str ExtractPdfFromPrintReplica(PdbReader* pdb) {
    if (!pdb || pdb->GetRecordCount() < 2) {
        return {};
    }
    if (GetPdbDocType(pdb->GetDbType()) != PdbDocType::Mobipocket) {
        return {};
    }

    auto rec0 = pdb->GetRecord(0);
    if (len(rec0) < kPalmDocHeaderLen + 12) {
        return {};
    }

    PalmDocHeader palm;
    DecodePalmDocHeader((const u8*)rec0.s, &palm);
    if (palm.encrType != kEncryptionNone) {
        logf("ExtractPdfFromPrintReplica: encrypted\n");
        return {};
    }

    bool isPrintReplica = false;
    int trailersCount = 0;
    bool multibyte = false;
    if (len(rec0) >= kPalmDocHeaderLen + kMobiHeaderMinLen) {
        MobiHeader mobi{};
        DecodeMobiDocHeader((const u8*)rec0.s + kPalmDocHeaderLen, rec0.len - kPalmDocHeaderLen, &mobi);
        if (str::EqN(StrL("MOBI"), Str(mobi.id, 4), 4)) {
            // MOBI type 8 is Print Replica (AZW4).
            if (mobi.type == 8) {
                isPrintReplica = true;
            }
            // Print Replica version 4 can have a long header without record trailers.
            if (mobi.hdrLen >= kMobiExtraFlagsHeaderLen &&
                mobi.minRequiredMobiFormatVersion >= kMobiTrailerMinVersion) {
                trailersCount = CountMobiTrailers(mobi.extraDataFlags);
                multibyte = (mobi.extraDataFlags & kMobiMultibyteFlag) != 0;
            }
        }
    }

    auto rec1 = pdb->GetRecord(1);
    if (len(rec1) >= 4 && MemEq(rec1.s, "%MOP", 4)) {
        isPrintReplica = true;
    }
    if (!isPrintReplica) {
        return {};
    }

    if (!IsValidCompression(palm.compressionType) || palm.compressionType == kCompressionHuff) {
        logf("ExtractPdfFromPrintReplica: unsupported compression %d\n", (int)palm.compressionType);
        return {};
    }

    int recCount = palm.recordsCount;
    if (recCount >= pdb->GetRecordCount()) {
        recCount = pdb->GetRecordCount() - 1;
    }
    if (recCount < 1) {
        return {};
    }

    constexpr u32 kMaxPrintReplicaRaw = 512 * 1024 * 1024;
    if (palm.uncompressedDocSize > kMaxPrintReplicaRaw) {
        logf("ExtractPdfFromPrintReplica: raw markup too large (%u)\n", palm.uncompressedDocSize);
        return {};
    }

    str::Builder raw;

    str::BuilderReserve(raw, (int)palm.uncompressedDocSize);
    for (int i = 1; i <= recCount; i++) {
        auto rec = pdb->GetRecord(i);
        if (len(rec) == 0) {
            return {};
        }
        int recSize = GetRealRecordSize((const u8*)rec.s, rec.len, trailersCount, multibyte);
        if (kInvalidSize == recSize) {
            recSize = rec.len;
        }
        if (kCompressionNone == palm.compressionType) {
            raw.Append(Str(rec.s, recSize));
        } else if (kCompressionPalm == palm.compressionType) {
            if (!PalmdocUncompress((const u8*)rec.s, recSize, raw)) {
                logf("ExtractPdfFromPrintReplica: PalmDoc decompression failed\n");
                return {};
            }
        }
    }

    Str pdf = ExtractPdfFromMopRaw(ToStr(raw));
    if (len(pdf) > 0) {
        logf("ExtractPdfFromPrintReplica: extracted %d byte PDF\n", len(pdf));
    }
    return pdf;
}

// Owned PDF bytes from an AZW4 / Kindle Print Replica (PDF in a MOBI wrapper).
// Cheap peek so regular MOBI files are not fully re-read just to reject them.
static bool FileMightBePrintReplica(Str path) {
    constexpr int kPeek = 256 * 1024;
    u8 buf[kPeek];
    int n = file::ReadN(path, buf, kPeek);
    if (n < 80) {
        return false;
    }
    if (!MemEq(buf + 0x3c, "BOOKMOBI", 8)) {
        return false;
    }
    ByteReader r(buf, n);
    u16 numRecs = r.UInt16BE(76);
    if (numRecs < 2) {
        return false;
    }
    int tableBytes = (int)numRecs * 8;
    if (78 + tableBytes > n) {
        return true;
    }
    u32 off0 = r.UInt32BE(78);
    u32 off1 = r.UInt32BE(86);
    bool isType8 = false;
    bool sawType = false;
    if (n >= 28 && off0 <= (u32)(n - 28) && MemEq(buf + off0 + 16, "MOBI", 4)) {
        sawType = true;
        isType8 = r.UInt32BE((int)off0 + 24) == 8;
    }
    bool sawRec1 = false;
    bool rec1Mop = false;
    if (n >= 4 && off1 <= (u32)(n - 4)) {
        sawRec1 = true;
        rec1Mop = MemEq(buf + off1, "%MOP", 4);
    }
    if (isType8 || rec1Mop) {
        return true;
    }
    if (sawType && sawRec1) {
        return false;
    }
    return true;
}

Str ExtractPdfFromPrintReplicaFile(Str path) {
    if (!FileMightBePrintReplica(path)) {
        return {};
    }
    PdbReader* pdb = PdbReader::CreateFromFile(path);
    if (!pdb) {
        return {};
    }
    Str pdf = ExtractPdfFromPrintReplica(pdb);
    delete pdb;
    return pdf;
}

Str ExtractPdfFromPrintReplicaData(Str data) {
    PdbReader* pdb = PdbReader::CreateFromData(str::Dup(data));
    if (!pdb) {
        return {};
    }
    Str pdf = ExtractPdfFromPrintReplica(pdb);
    delete pdb;
    return pdf;
}

#if IS_DEBUG
bool MobiDoc_UnitTestHeader() {
    const struct {
        Str html;
        int filepos;
    } tocCases[] = {
        {StrL("<reference type='toc' filepos='12'/>"), 12},
        {StrL("<REFERENCE FILEPOS=21 TYPE=TOC>"), 21},
        {StrL("<reference ignored type = \"toc\" filepos=3>"), 3},
        {StrL("<reference type=other filepos=1><reference type=toc filepos=2>"), 2},
        {StrL("<reference type=toc filepos=1 filepos='9'>"), 9},
        {StrL("<reference type=toc filepos='7>"), 7},
        {StrL("<reference type=toc filepos=bad>"), -1},
        {StrL("<reference type=toc filepos=1"), -1},
    };
    for (const auto& c : tocCases) {
        if (FindMobiTocFilepos(c.html) != c.filepos) {
            return false;
        }
    }
    constexpr int kDrmBoundary = 164;
    constexpr int kFlagsBoundary = 228;
    constexpr int kFullHeaderLen = 232;
    const int lengths[] = {116, kDrmBoundary - 1, kDrmBoundary, kFlagsBoundary - 1, kFlagsBoundary, kFullHeaderLen};
    for (int n : lengths) {
        ByteWriter data;
        data.d.Append(StrL("MOBI"));
        data.Write32((u32)n);
        for (int off = 8; off < kFullHeaderLen; off += 4) {
            data.Write32((u32)off);
        }
        Str bytes = data.AsByteSlice();
        MobiHeader hdr;
        DecodeMobiDocHeader((const u8*)bytes.s, len(bytes), &hdr);
        if (!str::Eq(Str(hdr.id, 4), StrL("MOBI")) || hdr.hdrLen != (u32)n || hdr.type != 8 || hdr.textEncoding != 12 ||
            hdr.minRequiredMobiFormatVersion != 88 || hdr.imageFirstRec != 92 || hdr.huffmanFirstRec != 96 ||
            hdr.huffmanRecCount != 100 || hdr.exthFlags != 112 ||
            hdr.drmEntriesCount != (n >= kDrmBoundary ? 152u : (u32)-1) ||
            hdr.extraDataFlags != (n >= kFlagsBoundary ? 224 : 0)) {
            return false;
        }
    }
    ByteWriter data;
    data.d.Append(StrL("HUFF"));
    data.Write32(kHuffHeaderLen);
    data.Write32(kHuffHeaderLen);
    data.Write32(kHuffHeaderLen + kCacheDataLen);
    data.Write32(0);
    data.Write32(0);
    for (int i = 0; i < kCacheItemCount + kBaseTableItemCount; i++) {
        data.Write32((u32)i);
    }
    Str bytes = data.AsByteSlice();
    HuffDicDecompressor huff;
    return huff.SetHuffData((u8*)bytes.s, len(bytes)) && huff.cacheTable[0] == 0 && huff.cacheTable[255] == 255 &&
           huff.baseTable[0] == 256 && huff.baseTable[63] == 319 && !huff.SetHuffData((u8*)bytes.s, len(bytes) - 1) &&
           CountMobiTrailers(1) == 0 && CountMobiTrailers(6) == 2 && CountMobiTrailers(0x8001) == 1 &&
           CountMobiTrailers(0xffff) == 15;
}
#endif

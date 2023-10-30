/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitReader.h"
#include "utils/ByteOrderDecoder.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"

#include "utils/Log.h"

constexpr size_t kInvalidSize = (size_t)-1;

// Parse mobi format http://wiki.mobileread.com/wiki/MOBI
#define COMPRESSION_NONE 1
#define COMPRESSION_PALM 2
#define COMPRESSION_HUFF 17480
#define COMPRESSION_UNSUPPORTED_DRM -1

#define ENCRYPTION_NONE 0
#define ENCRYPTION_OLD 1
#define ENCRYPTION_NEW 2

struct PalmDocHeader {
    u16 compressionType = 0;
    u16 reserved1 = 0;
    u32 uncompressedDocSize = 0;
    u16 recordsCount = 0;
    u16 maxRecSize = 0; // usually (always?) 4096
    // if it's palmdoc, we have currPos, if mobi, encrType/reserved2
    union {
        u32 currPos = 0;
        struct {
            u16 encrType;
            u16 reserved2;
        } mobi;
    };
};
#define kPalmDocHeaderLen 16

// http://wiki.mobileread.com/wiki/MOBI#PalmDOC_Header
static void DecodePalmDocHeader(const u8* buf, PalmDocHeader* hdr) {
    ByteOrderDecoder d(buf, kPalmDocHeaderLen, ByteOrderDecoder::BigEndian);
    hdr->compressionType = d.UInt16();
    hdr->reserved1 = d.UInt16();
    hdr->uncompressedDocSize = d.UInt32();
    hdr->recordsCount = d.UInt16();
    hdr->maxRecSize = d.UInt16();
    hdr->currPos = d.UInt32();

    CrashIf(kPalmDocHeaderLen != d.Offset());
}

// http://wiki.mobileread.com/wiki/MOBI#MOBI_Header
// Note: the real length of MobiHeader is in MobiHeader.hdrLen. This is just
// the size of the struct
#define kMobiHeaderLen 232
// length up to MobiHeader.exthFlags
#define kMobiHeaderMinLen 116
struct MobiHeader {
    char id[4];
    u32 hdrLen; // including 4 id bytes
    u32 type;
    u32 textEncoding;
    u32 uniqueId;
    u32 mobiFormatVersion;
    u32 ortographicIdxRec; // -1 if no ortographics index
    u32 inflectionIdxRec;
    u32 namesIdxRec;
    u32 keysIdxRec;
    u32 extraIdx0Rec;
    u32 extraIdx1Rec;
    u32 extraIdx2Rec;
    u32 extraIdx3Rec;
    u32 extraIdx4Rec;
    u32 extraIdx5Rec;
    u32 firstNonBookRec;
    u32 fullNameOffset; // offset in record 0
    u32 fullNameLen;
    // Low byte is main language e.g. 09 = English,
    // next byte is dialect, 08 = British, 04 = US.
    // Thus US English is 1033, UK English is 2057
    u32 locale;
    u32 inputDictLanguage;
    u32 outputDictLanguage;
    u32 minRequiredMobiFormatVersion;
    u32 imageFirstRec;
    u32 huffmanFirstRec;
    u32 huffmanRecCount;
    u32 huffmanTableOffset;
    u32 huffmanTableLen;
    u32 exthFlags; // bitfield. if bit 6 (0x40) is set => there's an EXTH record
    char reserved1[32];
    u32 drmOffset;       // -1 if no drm info
    u32 drmEntriesCount; // -1 if no drm
    u32 drmSize;
    u32 drmFlags;
    char reserved2[62];
    // A set of binary flags, some of which indicate extra data at the end of each text block.
    // This only seems to be valid for Mobipocket format version 5 and 6 (and higher?), when
    // the header length is 228 (0xE4) or 232 (0xE8).
    u16 extraDataFlags;
    i32 indxRec;
};

static_assert(kMobiHeaderLen == sizeof(MobiHeader), "wrong size of MobiHeader structure");

// Uncompress source data compressed with PalmDoc compression into a buffer.
// http://wiki.mobileread.com/wiki/PalmDOC#Format
// Returns false on decoding errors
static bool PalmdocUncompress(const u8* src, size_t srcLen, str::Str& dst) {
    const u8* srcEnd = src + srcLen;
    while (src < srcEnd) {
        u8 c = *src++;
        if ((c >= 1) && (c <= 8)) {
            if (src + c > srcEnd) {
                return false;
            }
            dst.Append(src, c);
            src += c;
        } else if (c < 128) {
            dst.AppendChar((char)c);
        } else if (c < 192) {
            if (src + 1 > srcEnd) {
                return false;
            }
            u16 c2 = (c << 8) | (u8)*src++;
            u16 back = (c2 >> 3) & 0x07ff;
            if (back > dst.size() || 0 == back) {
                return false;
            }
            for (u8 n = (c2 & 7) + 3; n > 0; n--) {
                char ctmp = dst.at(dst.size() - back);
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

#define kHuffHeaderLen 24
struct HuffHeader {
    char id[4]; // "HUFF"
    u32 hdrLen; // should be 24
    // offset of 256 4-byte elements of cache data, in big endian
    u32 cacheOffset; // should be 24 as well
    // offset of 64 4-byte elements of base table data, in big endian
    u32 baseTableOffset; // should be 24 + 1024
    // like cacheOffset except data is in little endian
    u32 cacheLEOffset; // should be 24 + 1024 + 256
    // like baseTableOffset except data is in little endian
    u32 baseTableLEOffset; // should be 24 + 1024 + 256 + 1024
};
static_assert(kHuffHeaderLen == sizeof(HuffHeader), "wrong size of HuffHeader structure");

#define kCdicHeaderLen 16
struct CdicHeader {
    char id[4]; // "CIDC"
    u32 hdrLen; // should be 16
    u32 unknown;
    u32 codeLen;
};

static_assert(kCdicHeaderLen == sizeof(CdicHeader), "wrong size of CdicHeader structure");

#define kCacheItemCount 256
#define kCacheDataLen (kCacheItemCount * sizeof(u32))
#define kBaseTableItemCount 64
#define kBaseTableDataLen (kBaseTableItemCount * sizeof(u32))

#define kHuffRecordMinLen (kHuffHeaderLen + kCacheDataLen + kBaseTableDataLen)
#define kHuffRecordLen (kHuffHeaderLen + 2 * kCacheDataLen + 2 * kBaseTableDataLen)

#define kCdicsMax 32

class HuffDicDecompressor {
    u32 cacheTable[kCacheItemCount]{};
    u32 baseTable[kBaseTableItemCount]{};

    size_t dictsCount = 0;
    // owned by the creator (in our case: by the PdbReader)
    u8* dicts[kCdicsMax]{};
    u32 dictSize[kCdicsMax]{};

    u32 codeLength = 0;

    Vec<u32> recursionGuard;

  public:
    HuffDicDecompressor();

    bool SetHuffData(u8* huffData, size_t huffDataLen);
    bool AddCdicData(u8* cdicData, u32 cdicDataLen);
    bool Decompress(u8* src, size_t srcSize, str::Str& dst);
    bool DecodeOne(u32 code, str::Str& dst);
};

HuffDicDecompressor::HuffDicDecompressor() {
}

bool HuffDicDecompressor::DecodeOne(u32 code, str::Str& dst) {
    u16 dict = (u16)(code >> codeLength);
    if (dict >= dictsCount) {
        logf("invalid dict value\n");
        return false;
    }
    code &= ((1 << (codeLength)) - 1);
    u16 offset = UInt16BE(dicts[dict] + code * 2);

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
        if (recursionGuard.Contains(code)) {
            logf("infinite recursion\n");
            return false;
        }
        recursionGuard.Append(code);
        if (!Decompress(p, symLen, dst)) {
            return false;
        }
        recursionGuard.Pop();
    } else {
        symLen &= 0x7fff;
        if (symLen > 127) {
            logf("symLen too big\n");
            return false;
        }
        dst.Append((char*)p, symLen);
    }
    return true;
}

bool HuffDicDecompressor::Decompress(u8* src, size_t srcSize, str::Str& dst) {
    u32 bitsConsumed = 0;
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
                baseVal = baseTable[codeLen * 2 - 2];
                code = (bits >> (32 - codeLen));
            } while (baseVal > code);
            code = baseTable[codeLen * 2 - 1] - (bits >> (32 - codeLen));
        }

        if (!DecodeOne(code, dst)) {
            return false;
        }
        bitsConsumed = codeLen;
    }

    if (br.BitsLeft() > 0 && 0 != bits) {
        logf("compressed data left\n");
    }
    return true;
}

static void ReadHuffReader(HuffHeader& huffHdr, ByteOrderDecoder& d) {
    d.Bytes(huffHdr.id, 4);
    huffHdr.hdrLen = d.UInt32();
    huffHdr.cacheOffset = d.UInt32();
    huffHdr.baseTableOffset = d.UInt32();
    huffHdr.cacheLEOffset = d.UInt32();
    huffHdr.baseTableLEOffset = d.UInt32();
    CrashIf(d.Offset() != kHuffHeaderLen);
}

bool HuffDicDecompressor::SetHuffData(u8* huffData, size_t huffDataLen) {
    // for now catch cases where we don't have both big endian and little endian
    // versions of the data
    CrashIf(kHuffRecordLen != huffDataLen);
    // but conservatively assume we only need big endian version
    if (huffDataLen < kHuffRecordMinLen) {
        return false;
    }

    ByteOrderDecoder d(huffData, huffDataLen, ByteOrderDecoder::BigEndian);
    HuffHeader huffHdr;
    ReadHuffReader(huffHdr, d);

    if (!str::EqN(huffHdr.id, "HUFF", 4)) {
        return false;
    }

    CrashIf(huffHdr.hdrLen != kHuffHeaderLen);
    if (huffHdr.hdrLen != kHuffHeaderLen) {
        return false;
    }
    if (huffHdr.cacheOffset != kHuffHeaderLen) {
        return false;
    }
    if (huffHdr.baseTableOffset != huffHdr.cacheOffset + kCacheDataLen) {
        return false;
    }
    // we conservatively use the big-endian version of the data,
    for (int i = 0; i < kCacheItemCount; i++) {
        cacheTable[i] = d.UInt32();
    }
    for (int i = 0; i < kBaseTableItemCount; i++) {
        baseTable[i] = d.UInt32();
    }
    CrashIf(d.Offset() != kHuffRecordMinLen);
    return true;
}

bool HuffDicDecompressor::AddCdicData(u8* cdicData, u32 cdicDataLen) {
    if (dictsCount >= kCdicsMax) {
        return false;
    }
    if (cdicDataLen < kCdicHeaderLen) {
        return false;
    }
    if (!str::EqN("CDIC", (char*)cdicData, 4)) {
        return false;
    }
    u32 hdrLen = UInt32BE(cdicData + 4);
    u32 codeLen = UInt32BE(cdicData + 12);
    if (0 == codeLength) {
        codeLength = codeLen;
    } else {
        CrashIf(codeLen != codeLength);
        codeLength = std::min(codeLength, codeLen);
    }
    CrashIf(hdrLen != kCdicHeaderLen);
    if (hdrLen != kCdicHeaderLen) {
        return false;
    }
    u32 size = cdicDataLen - hdrLen;

    u32 maxSize = 1 << codeLength;
    if (maxSize >= size) {
        return false;
    }
    dicts[dictsCount] = cdicData + hdrLen;
    dictSize[dictsCount] = size;
    ++dictsCount;
    return true;
}

static void DecodeMobiDocHeader(const u8* buf, MobiHeader* hdr) {
    memset(hdr, 0, sizeof(MobiHeader));
    hdr->drmEntriesCount = (u32)-1;

    ByteOrderDecoder d(buf, kMobiHeaderLen, ByteOrderDecoder::BigEndian);
    d.Bytes(hdr->id, 4);
    hdr->hdrLen = d.UInt32();
    hdr->type = d.UInt32();
    hdr->textEncoding = d.UInt32();
    hdr->uniqueId = d.UInt32();
    hdr->mobiFormatVersion = d.UInt32();
    hdr->ortographicIdxRec = d.UInt32();
    hdr->inflectionIdxRec = d.UInt32();
    hdr->namesIdxRec = d.UInt32();
    hdr->keysIdxRec = d.UInt32();
    hdr->extraIdx0Rec = d.UInt32();
    hdr->extraIdx1Rec = d.UInt32();
    hdr->extraIdx2Rec = d.UInt32();
    hdr->extraIdx3Rec = d.UInt32();
    hdr->extraIdx4Rec = d.UInt32();
    hdr->extraIdx5Rec = d.UInt32();
    hdr->firstNonBookRec = d.UInt32();
    hdr->fullNameOffset = d.UInt32();
    hdr->fullNameLen = d.UInt32();
    hdr->locale = d.UInt32();
    hdr->inputDictLanguage = d.UInt32();
    hdr->outputDictLanguage = d.UInt32();
    hdr->minRequiredMobiFormatVersion = d.UInt32();
    hdr->imageFirstRec = d.UInt32();
    hdr->huffmanFirstRec = d.UInt32();
    hdr->huffmanRecCount = d.UInt32();
    hdr->huffmanTableOffset = d.UInt32();
    hdr->huffmanTableLen = d.UInt32();
    hdr->exthFlags = d.UInt32();
    CrashIf(kMobiHeaderMinLen != d.Offset());

    if (hdr->hdrLen < kMobiHeaderMinLen + 48) {
        return;
    }

    d.Bytes(hdr->reserved1, 32);
    hdr->drmOffset = d.UInt32();
    hdr->drmEntriesCount = d.UInt32();
    hdr->drmSize = d.UInt32();
    hdr->drmFlags = d.UInt32();

    if (hdr->hdrLen < 228) { // magic number at which extraDataFlags becomes valid
        return;
    }

    d.Bytes(hdr->reserved2, 62);
    hdr->extraDataFlags = d.UInt16();
    if (hdr->hdrLen >= 232) {
        hdr->indxRec = d.UInt32();
        CrashIf(kMobiHeaderLen != d.Offset());
    } else {
        CrashIf(kMobiHeaderLen - 4 != d.Offset());
    }
}

static bool IsValidCompression(int comprType) {
    return (COMPRESSION_NONE == comprType) || (COMPRESSION_PALM == comprType) || (COMPRESSION_HUFF == comprType);
}

MobiDoc::MobiDoc(const char* filePath) {
    docTocIndex = kInvalidSize;
    fileName = str::Dup(filePath);
}

MobiDoc::~MobiDoc() {
    free(fileName);
    free(images);
    delete huffDic;
    delete doc;
    delete pdbReader;
    for (size_t i = 0; i < props.size(); i++) {
        free(props.at(i).value);
    }
}

bool MobiDoc::ParseHeader() {
    CrashIf(!pdbReader);
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
    u8* firstRecData = rec.data();
    size_t recSize = rec.size();
    if (!firstRecData || recSize < kPalmDocHeaderLen) {
        log("failed to read record 0\n");
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
        if (palmDocHdr.mobi.encrType != ENCRYPTION_NONE) {
            logf("encryption is unsupported\n");
            return false;
        }
    }
    docRecCount = palmDocHdr.recordsCount;
    if (docRecCount == pdbReader->GetRecordCount()) {
        // catch the case where a broken document has an off-by-one error
        // cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2529
        docRecCount--;
    }
    docUncompressedSize = palmDocHdr.uncompressedDocSize;

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
    DecodeMobiDocHeader(firstRecData + kPalmDocHeaderLen, &mobiHdr);
    if (!str::EqN("MOBI", mobiHdr.id, 4)) {
        logf("MobiHeader.id is not 'MOBI'\n");
        return false;
    }
    if (mobiHdr.drmEntriesCount != (u32)-1) {
        logf("DRM is unsupported\n");
        // load an empty document and display a warning
        compressionType = COMPRESSION_UNSUPPORTED_DRM;
        Metadata prop;
        prop.prop = DocumentProperty::UnsupportedFeatures;
        char* tmp = strconv::WstrToCodePage(mobiHdr.textEncoding, L"DRM");
        prop.value = tmp;
        props.Append(prop);
    }
    textEncoding = mobiHdr.textEncoding;

    if (pdbReader->GetRecordCount() > mobiHdr.imageFirstRec) {
        imageFirstRec = mobiHdr.imageFirstRec;
        if (0 == imageFirstRec) {
            // I don't think this should ever happen but I've seen it
            imagesCount = 0;
        } else {
            imagesCount = pdbReader->GetRecordCount() - imageFirstRec;
        }
    }
    if (kPalmDocHeaderLen + (size_t)mobiHdr.hdrLen > recSize) {
        logf("MobiHeader too big\n");
        return false;
    }

    bool hasExtraFlags = (mobiHdr.hdrLen >= 228); // TODO: also only if mobiFormatVersion >= 5?
    if (hasExtraFlags) {
        u16 flags = mobiHdr.extraDataFlags;
        multibyte = ((flags & 1) != 0);
        while (flags > 1) {
            if (0 != (flags & 2)) {
                trailersCount++;
            }
            flags = flags >> 1;
        }
    }

    if (COMPRESSION_HUFF == compressionType) {
        CrashIf(PdbDocType::Mobipocket != docType);
        rec = pdbReader->GetRecord(mobiHdr.huffmanFirstRec);
        size_t huffRecSize = rec.size();
        u8* recData = rec.data();
        if (!recData) {
            return false;
        }
        CrashIf(nullptr != huffDic);
        huffDic = new HuffDicDecompressor();
        if (!huffDic->SetHuffData((u8*)recData, huffRecSize)) {
            return false;
        }
        size_t cdicsCount = mobiHdr.huffmanRecCount - 1;
        if (cdicsCount > kCdicsMax) {
            logf("MobiDoc::ParseHeader: cdicsCount: %d, kCdicsMax: %d\n", cdicsCount, kCdicsMax);
            ReportIf(true);
            return false;
        }
        for (size_t i = 0; i < cdicsCount; i++) {
            rec = pdbReader->GetRecord(mobiHdr.huffmanFirstRec + 1 + i);
            recData = rec.data();
            huffRecSize = rec.size();
            if (!recData) {
                return false;
            }
            if (huffRecSize > (u32)-1) {
                return false;
            }
            if (!huffDic->AddCdicData((u8*)recData, (u32)huffRecSize)) {
                return false;
            }
        }
    }

    if ((mobiHdr.exthFlags & 0x40)) {
        u32 offset = kPalmDocHeaderLen + mobiHdr.hdrLen;
        DecodeExthHeader(firstRecData + offset, recSize - offset);
    }

    LoadImages();
    return true;
}

bool MobiDoc::DecodeExthHeader(const u8* data, size_t dataLen) {
    if (dataLen < 12 || !memeq(data, "EXTH", 4)) {
        return false;
    }

    ByteOrderDecoder d(data, dataLen, ByteOrderDecoder::BigEndian);
    d.Skip(4);
    u32 hdrLen = d.UInt32();
    u32 count = d.UInt32();
    if (hdrLen > dataLen) {
        return false;
    }

    for (u32 i = 0; i < count; i++) {
        if (d.Offset() > dataLen - 8) {
            return false;
        }
        u32 type = d.UInt32();
        u32 length = d.UInt32();
        if (length < 8 || length > dataLen - d.Offset() + 8) {
            return false;
        }
        d.Skip(length - 8);

        Metadata prop;
        switch (type) {
            case 100:
                prop.prop = DocumentProperty::Author;
                break;
            case 105:
                prop.prop = DocumentProperty::Subject;
                break;
            case 106:
                prop.prop = DocumentProperty::CreationDate;
                break;
            case 108:
                prop.prop = DocumentProperty::CreatorApp;
                break;
            case 109:
                prop.prop = DocumentProperty::Copyright;
                break;
            case 201:
                if (length == 12 && imageFirstRec) {
                    d.Unskip(4);
                    coverImageRec = imageFirstRec + d.UInt32();
                }
                continue;
            case 503:
                prop.prop = DocumentProperty::Title;
                break;
            default:
                continue;
        }
        prop.value = str::Dup((char*)(data + d.Offset() - length + 8), length - 8);
        if (prop.value) {
            props.Append(prop);
        }
    }

    return true;
}

#define EOF_REC 0xe98e0d0a
#define FLIS_REC 0x464c4953 // 'FLIS'
#define FCIS_REC 0x46434953 // 'FCIS
#define FDST_REC 0x46445354 // 'FDST'
#define DATP_REC 0x44415450 // 'DATP'
#define SRCS_REC 0x53524353 // 'SRCS'
#define VIDE_REC 0x56494445 // 'VIDE'

static bool IsEofRecord(const ByteSlice& d) {
    return (4 == d.size()) && (EOF_REC == UInt32BE(d.data()));
}

static bool KnownNonImageRec(const ByteSlice& d) {
    if (d.size() < 4) {
        return false;
    }
    u32 sig = UInt32BE(d.data());

    switch (sig) {
        case FLIS_REC:
        case FCIS_REC:
        case FDST_REC:
        case DATP_REC:
        case SRCS_REC:
        case VIDE_REC:
            return true;
    }
    return false;
}

static bool KnownImageFormat(const ByteSlice& d) {
    return nullptr != GuessFileTypeFromContent(d);
}

// return false if we should stop loading images (because we
// encountered eof record or ran out of memory)
bool MobiDoc::LoadImage(size_t imageNo) {
    size_t imageRec = imageFirstRec + imageNo;

    auto rec = pdbReader->GetRecord(imageRec);
    if (rec.empty()) {
        return false;
    }
    if (IsEofRecord(rec)) {
        return false;
    }
    if (KnownNonImageRec(rec)) {
        return true;
    }
    if (!KnownImageFormat(rec)) {
        logf("MobiDoc::LoadImage: unknown image format\n");
        return true;
    }
    images[imageNo] = rec;
    return true;
}

void MobiDoc::LoadImages() {
    if (0 == imagesCount) {
        return;
    }
    images = AllocArray<ByteSlice>(imagesCount);
    for (size_t i = 0; i < imagesCount; i++) {
        if (!LoadImage(i)) {
            return;
        }
    }
}

// imgRecIndex corresponds to recindex attribute of <img> tag
// as far as I can tell, this means: it starts at 1
// returns nullptr if there is no image (e.g. it's not a format we
// recognize)
ByteSlice* MobiDoc::GetImage(size_t imgRecIndex) const {
    if ((imgRecIndex > imagesCount) || (imgRecIndex < 1)) {
        return nullptr;
    }
    --imgRecIndex;
    if (images[imgRecIndex].empty()) {
        return nullptr;
    }
    return &images[imgRecIndex];
}

ByteSlice* MobiDoc::GetCoverImage() {
    if (!coverImageRec || coverImageRec < imageFirstRec) {
        return nullptr;
    }
    size_t imageNo = coverImageRec - imageFirstRec;
    if (imageNo >= imagesCount || images[imageNo].empty()) {
        return nullptr;
    }
    return &images[imageNo];
}

// each record can have extra data at the end, which we must discard
// returns kInvalidSize on error
static size_t GetRealRecordSize(const u8* recData, size_t recLen, size_t trailersCount, bool multibyte) {
    for (size_t i = 0; i < trailersCount; i++) {
        if (recLen < 4) {
            return kInvalidSize;
        }
        u32 n = 0;
        for (size_t j = 0; j < 4; j++) {
            u8 v = recData[recLen - 4 + j];
            if (0 != (v & 0x80)) {
                n = 0;
            }
            n = (n << 7) | (v & 0x7f);
        }
        if (n > recLen) {
            return kInvalidSize;
        }
        recLen -= n;
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
bool MobiDoc::LoadDocRecordIntoBuffer(size_t recNo, str::Str& strOut) {
    auto rec = pdbReader->GetRecord(recNo);
    u8* recData = rec.data();
    if (nullptr == recData) {
        return false;
    }
    size_t recSize = GetRealRecordSize((const u8*)recData, rec.size(), trailersCount, multibyte);
    if (kInvalidSize == recSize) {
        return false;
    }

    if (COMPRESSION_NONE == compressionType) {
        strOut.Append((const char*)recData, recSize);
        return true;
    }
    if (COMPRESSION_PALM == compressionType) {
        bool ok = PalmdocUncompress(recData, recSize, strOut);
        if (!ok) {
            logf("PalmDoc decompression failed\n");
        }
        return ok;
    }
    if (COMPRESSION_HUFF == compressionType && huffDic) {
        bool ok = huffDic->Decompress((u8*)recData, recSize, strOut);
        if (!ok) {
            logf("HuffDic decompression failed\n");
        }
        return ok;
    }
    if (COMPRESSION_UNSUPPORTED_DRM == compressionType) {
        // ensure a single blank page
        if (1 == recNo) {
            strOut.Append("&nbsp;");
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

    CrashIf(doc != nullptr);
    doc = new str::Str(docUncompressedSize);
    size_t nFailed = 0;
    for (size_t i = 1; i <= docRecCount; i++) {
        if (!LoadDocRecordIntoBuffer(i, *doc)) {
            nFailed++;
        }
    }

    // TODO: this is a heuristic for https://github.com/sumatrapdfreader/sumatrapdf/issues/1314
    // It has 29 records that fail to decompress because infinite recursion
    // is detected.
    // Figure out if this is a bug in my decoding.
    if (nFailed > docRecCount / 2) {
        return false;
    }

    // replace unexpected \0 with spaces
    // cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2529
    char* s = doc->Get();
    char* end = s + doc->size();
    while ((s = (char*)memchr(s, '\0', end - s)) != nullptr) {
        *s = ' ';
    }
    if (textEncoding != CP_UTF8) {
        char* docUtf8 = strconv::ToMultiByte(doc->Get(), textEncoding, CP_UTF8);
        if (docUtf8) {
            doc->Reset();
            doc->AppendAndFree(docUtf8);
        }
    }
    return true;
}

// don't free the result
ByteSlice MobiDoc::GetHtmlData() const {
    if (doc) {
        return doc->AsByteSlice();
    }
    return {};
}

TempStr MobiDoc::GetPropertyTemp(DocumentProperty prop) {
    for (auto& p : props) {
        if (p.prop == prop) {
            char* temp = strconv::StrToUtf8(p.value, textEncoding);
            TempStr res = str::DupTemp(temp);
            str::Free(temp);
            return res;
        }
    }
    return nullptr;
}

bool MobiDoc::HasToc() {
    if (docTocIndex != kInvalidSize) {
        return docTocIndex < doc->size();
    }
    docTocIndex = doc->size(); // no ToC

    // search for <reference type=toc filepos=\d+/>
    HtmlPullParser parser(doc->AsByteSlice());
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!tok->IsStartTag() && !tok->IsEmptyElementEndTag() || !tok->NameIs("reference")) {
            continue;
        }
        AttrInfo* attr = tok->GetAttrByName("type");
        if (!attr) {
            continue;
        }
        AutoFreeWstr val(strconv::FromHtmlUtf8(attr->val, attr->valLen));
        attr = tok->GetAttrByName("filepos");
        if (!str::EqI(val, L"toc") || !attr) {
            continue;
        }
        val.Set(strconv::FromHtmlUtf8(attr->val, attr->valLen));
        unsigned int pos;
        if (str::Parse(val, L"%u%$", &pos)) {
            docTocIndex = pos;
            return docTocIndex < doc->size();
        }
    }
    return false;
}

bool MobiDoc::ParseToc(EbookTocVisitor* visitor) {
    if (!HasToc()) {
        return false;
    }

    AutoFreeWstr itemText;
    AutoFreeWstr itemLink;
    int itemLevel = 0;

    // there doesn't seem to be a standard for Mobi ToCs, so we try to
    // determine the author's intentions by looking at commonly used tags
    HtmlPullParser parser(doc->Get() + docTocIndex, doc->size() - docTocIndex);
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (itemLink && tok->IsText()) {
            AutoFreeWstr linkText(strconv::FromHtmlUtf8(tok->s, tok->sLen));
            if (itemText) {
                itemText.Set(str::Join(itemText, L" ", linkText));
            } else {
                itemText.Set(linkText.StealData());
            }
        } else if (!tok->IsTag()) {
            continue;
        } else if (Tag_Mbp_Pagebreak == tok->tag) {
            break;
        } else if (!itemLink && tok->IsStartTag() && Tag_A == tok->tag) {
            AttrInfo* attr = tok->GetAttrByName("filepos");
            if (!attr) {
                attr = tok->GetAttrByName("href");
            }
            if (attr) {
                itemLink.Set(strconv::FromHtmlUtf8(attr->val, attr->valLen));
            }
        } else if (itemLink && tok->IsEndTag() && Tag_A == tok->tag) {
            if (!itemText) {
                itemLink.Reset();
                continue;
            }
            char* txt = ToUtf8Temp(itemText);
            char* link = ToUtf8Temp(itemLink);
            visitor->Visit(txt, link, itemLevel);
            itemText.Reset();
            itemLink.Reset();
        } else if (Tag_Blockquote == tok->tag || Tag_Ul == tok->tag || Tag_Ol == tok->tag) {
            if (tok->IsStartTag()) {
                itemLevel++;
            } else if (tok->IsEndTag() && itemLevel > 0) {
                itemLevel--;
            }
        }
    }
    return true;
}

bool MobiDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFileMobi;
}

MobiDoc* MobiDoc::CreateFromFile(const char* fileName) {
    MobiDoc* mb = new MobiDoc(fileName);
    PdbReader* pdbReader = PdbReader::CreateFromFile(fileName);
    if (!pdbReader || !mb->LoadForPdbReader(pdbReader)) {
        delete mb;
        return nullptr;
    }
    return mb;
}

MobiDoc* MobiDoc::CreateFromStream(IStream* stream) {
    MobiDoc* mb = new MobiDoc(nullptr);
    PdbReader* pdbReader = PdbReader::CreateFromStream(stream);
    if (!pdbReader || !mb->LoadForPdbReader(pdbReader)) {
        delete mb;
        return nullptr;
    }
    return mb;
}

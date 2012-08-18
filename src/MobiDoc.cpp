/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "MobiDoc.h"

#include "BitReader.h"
#include "ByteOrderDecoder.h"
#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "PdbReader.h"
#include "DebugLog.h"

// Parse mobi format http://wiki.mobileread.com/wiki/MOBI

#define MOBI_TYPE_CREATOR      "BOOKMOBI"
#define PALMDOC_TYPE_CREATOR   "TEXtREAd"
#define TEALDOC_TYPE_CREATOR   "TEXtTlDc"

#define COMPRESSION_NONE 1
#define COMPRESSION_PALM 2
#define COMPRESSION_HUFF 17480

#define ENCRYPTION_NONE 0
#define ENCRYPTION_OLD  1
#define ENCRYPTION_NEW  2

struct PalmDocHeader
{
    uint16      compressionType;
    uint16      reserved1;
    uint32      uncompressedDocSize;
    uint16      recordsCount;
    uint16      maxRecSize;     // usually (always?) 4096
    // if it's palmdoc, we have currPos, if mobi, encrType/reserved2
    union {
        uint32      currPos;
        struct {
          uint16    encrType;
          uint16    reserved2;
        } mobi;
    };
};
#define kPalmDocHeaderLen 16

// http://wiki.mobileread.com/wiki/MOBI#PalmDOC_Header
static void DecodePalmDocHeader(const char *buf, PalmDocHeader* hdr)
{
    ByteOrderDecoder d(buf, kPalmDocHeaderLen, ByteOrderDecoder::BigEndian);
    hdr->compressionType     = d.UInt16();
    hdr->reserved1           = d.UInt16();
    hdr->uncompressedDocSize = d.UInt32();
    hdr->recordsCount        = d.UInt16();
    hdr->maxRecSize          = d.UInt16();
    hdr->currPos             = d.UInt32();

    CrashIf(kPalmDocHeaderLen != d.Offset());
}

enum MobiDocType {
    TypeMobiDoc = 2,
    TypePalmDoc = 3,
    TypeAudio = 4,
    TypeNews = 257,
    TypeNewsFeed = 258,
    TypeNewsMagazin = 259,
    TypePics = 513,
    TypeWord = 514,
    TypeXls = 515,
    TypePpt = 516,
    TypeText = 517,
    TyepHtml = 518
};

// http://wiki.mobileread.com/wiki/MOBI#MOBI_Header
// Note: the real length of MobiHeader is in MobiHeader.hdrLen. This is just
// the size of the struct
#define kMobiHeaderLen 232
// length up to MobiHeader.exthFlags
#define kMobiHeaderMinLen 116
struct MobiHeader {
    char         id[4];
    uint32       hdrLen;   // including 4 id bytes
    uint32       type;     // MobiDocType
    uint32       textEncoding;
    uint32       uniqueId;
    uint32       mobiFormatVersion;
    uint32       ortographicIdxRec; // -1 if no ortographics index
    uint32       inflectionIdxRec;
    uint32       namesIdxRec;
    uint32       keysIdxRec;
    uint32       extraIdx0Rec;
    uint32       extraIdx1Rec;
    uint32       extraIdx2Rec;
    uint32       extraIdx3Rec;
    uint32       extraIdx4Rec;
    uint32       extraIdx5Rec;
    uint32       firstNonBookRec;
    uint32       fullNameOffset; // offset in record 0
    uint32       fullNameLen;
    // Low byte is main language e.g. 09 = English,
    // next byte is dialect, 08 = British, 04 = US.
    // Thus US English is 1033, UK English is 2057
    uint32       locale;
    uint32       inputDictLanguage;
    uint32       outputDictLanguage;
    uint32       minRequiredMobiFormatVersion;
    uint32       imageFirstRec;
    uint32       huffmanFirstRec;
    uint32       huffmanRecCount;
    uint32       huffmanTableOffset;
    uint32       huffmanTableLen;
    uint32       exthFlags;  // bitfield. if bit 6 (0x40) is set => there's an EXTH record
    char         reserved1[32];
    uint32       drmOffset; // -1 if no drm info
    uint32       drmEntriesCount; // -1 if no drm
    uint32       drmSize;
    uint32       drmFlags;
    char         reserved2[62];
    // A set of binary flags, some of which indicate extra data at the end of each text block.
    // This only seems to be valid for Mobipocket format version 5 and 6 (and higher?), when
    // the header length is 228 (0xE4) or 232 (0xE8).
    uint16       extraDataFlags;
    int32        indxRec;
};

STATIC_ASSERT(kMobiHeaderLen == sizeof(MobiHeader), validMobiHeader);

// Uncompress source data compressed with PalmDoc compression into a buffer.
// http://wiki.mobileread.com/wiki/PalmDOC#Format
// Returns false on decoding errors
static bool PalmdocUncompress(const char *src, size_t srcLen, str::Str<char>& dst)
{
    const char *srcEnd = src + srcLen;
    while (src < srcEnd) {
        uint8 c = (uint8)*src++;
        if ((c >= 1) && (c <= 8)) {
            if (src + c > srcEnd)
                return false;
            dst.Append(src, c);
            src += c;
        } else if (c < 128) {
            dst.Append((char)c);
        } else if ((c >= 128) && (c < 192)) {
            if (src + 1 > srcEnd)
                return false;
            uint16 c2 = (c << 8) | (uint8)*src++;
            uint16 back = (c2 >> 3) & 0x07ff;
            if (back > dst.Size() || 0 == back)
                return false;
            for (uint8 n = (c2 & 7) + 3; n > 0; n--) {
                dst.Append(dst.At(dst.Size() - back));
            }
        } else if (c >= 192) {
            dst.Append(' ');
            dst.Append((char)(c ^ 0x80));
        }
        else
            CrashIf(true);
    }

    return true;
}

#define kHuffHeaderLen 24
struct HuffHeader
{
    char         id[4];             // "HUFF"
    uint32       hdrLen;            // should be 24
    // offset of 256 4-byte elements of cache data, in big endian
    uint32       cacheOffset;       // should be 24 as well
    // offset of 64 4-byte elements of base table data, in big endian
    uint32       baseTableOffset;   // should be 24 + 1024
    // like cacheOffset except data is in little endian
    uint32       cacheLEOffset;     // should be 24 + 1024 + 256
    // like baseTableOffset except data is in little endian
    uint32       baseTableLEOffset; // should be 24 + 1024 + 256 + 1024
};
STATIC_ASSERT(kHuffHeaderLen == sizeof(HuffHeader), validHuffHeader);

#define kCdicHeaderLen 16
struct CdicHeader
{
    char        id[4];      // "CIDC"
    uint32      hdrLen;     // should be 16
    uint32      unknown;
    uint32      codeLen;
};

STATIC_ASSERT(kCdicHeaderLen == sizeof(CdicHeader), validCdicHeader);

#define kCacheItemCount     256
#define kCacheDataLen      (kCacheItemCount * sizeof(uint32))
#define kBaseTableItemCount 64
#define kBaseTableDataLen  (kBaseTableItemCount * sizeof(uint32))

#define kHuffRecordMinLen (kHuffHeaderLen +     kCacheDataLen +     kBaseTableDataLen)
#define kHuffRecordLen    (kHuffHeaderLen + 2 * kCacheDataLen + 2 * kBaseTableDataLen)

#define kCdicsMax 32

class HuffDicDecompressor
{
    uint32      cacheTable[kCacheItemCount];
    uint32      baseTable[kBaseTableItemCount];

    size_t      dictsCount;
    // owned by the creator (in our case: by the PdbReader)
    uint8 *     dicts[kCdicsMax];
    uint32      dictSize[kCdicsMax];

    uint32      codeLength;

public:
    HuffDicDecompressor();

    bool SetHuffData(uint8 *huffData, size_t huffDataLen);
    bool AddCdicData(uint8 *cdicData, uint32 cdicDataLen);
    bool Decompress(uint8 *src, size_t octets, str::Str<char>& dst);
    bool DecodeOne(uint32 code, str::Str<char>& dst);
};

HuffDicDecompressor::HuffDicDecompressor() : codeLength(0), dictsCount(0) { }

bool HuffDicDecompressor::DecodeOne(uint32 code, str::Str<char>& dst)
{
    uint16 dict = code >> codeLength;
    if (dict >= dictsCount) {
        lf("invalid dict value");
        return false;
    }
    code &= ((1 << (codeLength)) - 1);
    uint16 offset = UInt16BE(dicts[dict] + code * 2);

    if ((uint32)offset + 2 > dictSize[dict]) {
        lf("invalid offset");
        return false;
    }
    uint16 symLen = UInt16BE(dicts[dict] + offset);
    uint8 *p = dicts[dict] + offset + 2;
    if ((uint32)(symLen & 0x7fff) > dictSize[dict] - offset - 2) {
        lf("invalid symLen");
        return false;
    }

    if (!(symLen & 0x8000)) {
        if (!Decompress(p, symLen, dst))
            return false;
    } else {
        symLen &= 0x7fff;
        if (symLen > 127) {
            lf("symLen too big");
            return false;
        }
        dst.Append((char *)p, symLen);
    }
    return true;
}

bool HuffDicDecompressor::Decompress(uint8 *src, size_t srcSize, str::Str<char>& dst)
{
    uint32    bitsConsumed = 0;
    uint32    bits = 0;

    BitReader br(src, srcSize);

    for (;;) {
        if (bitsConsumed > br.BitsLeft()) {
            lf("not enough data");
            return false;
        }
        br.Eat(bitsConsumed);
        if (0 == br.BitsLeft())
            break;

        bits = br.Peek(32);
        if (br.BitsLeft() < 8 && 0 == bits)
            break;
        uint32 v = cacheTable[bits >> 24];
        uint32 codeLen = v & 0x1f;
        if (!codeLen) {
            lf("corrupted table, zero code len");
            return false;
        }
        bool isTerminal = (v & 0x80) != 0;

        uint32 code;
        if (isTerminal) {
            code = (v >> 8) - (bits >> (32 - codeLen));
        } else {
            uint32 baseVal;
            codeLen -= 1;
            do {
                CrashIf(codeLen > 31);
                baseVal = baseTable[codeLen*2];
                code = (bits >> (32 - (codeLen+1)));
                codeLen++;
                if (codeLen > 32) {
                    lf("code len > 32 bits");
                    return false;
                }
            } while (baseVal > code);
            code = baseTable[codeLen * 2 - 1] - (bits >> (32 - codeLen));
        }

        if (!DecodeOne(code, dst))
            return false;
        bitsConsumed = codeLen;
    }

    if (br.BitsLeft() > 0 && 0 != bits) {
        lf("compressed data left");
    }
    return true;
}

bool HuffDicDecompressor::SetHuffData(uint8 *huffData, size_t huffDataLen)
{
    // for now catch cases where we don't have both big endian and little endian
    // versions of the data
    assert(kHuffRecordLen == huffDataLen);
    // but conservatively assume we only need big endian version
    if (huffDataLen < kHuffRecordMinLen)
        return false;

    ByteOrderDecoder d(huffData, huffDataLen, ByteOrderDecoder::BigEndian);
    HuffHeader huffHdr;
    d.Bytes(huffHdr.id, 4);
    huffHdr.hdrLen = d.UInt32();
    huffHdr.cacheOffset = d.UInt32();
    huffHdr.baseTableOffset = d.UInt32();
    huffHdr.cacheLEOffset = d.UInt32();
    huffHdr.baseTableLEOffset = d.UInt32();
    CrashIf(d.Offset() != kHuffHeaderLen);

    if (!str::EqN(huffHdr.id, "HUFF", 4))
        return false;

    assert(huffHdr.hdrLen == kHuffHeaderLen);
    if (huffHdr.hdrLen != kHuffHeaderLen)
        return false;
    if (huffHdr.cacheOffset != kHuffHeaderLen)
        return false;
    if (huffHdr.baseTableOffset != huffHdr.cacheOffset + kCacheDataLen)
        return false;
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

bool HuffDicDecompressor::AddCdicData(uint8 *cdicData, uint32 cdicDataLen)
{
    if (dictsCount >= kCdicsMax)
        return false;
    if (cdicDataLen < kCdicHeaderLen)
        return false;
    if (!str::EqN("CDIC", (char *)cdicData, 4))
        return false;
    uint32 hdrLen = UInt32BE(cdicData + 4);
    uint32 codeLen = UInt32BE(cdicData + 12);
    if (0 == codeLength)
        codeLength = codeLen;
    else {
        assert(codeLen == codeLength);
        codeLength = min(codeLength, codeLen);
    }
    assert(hdrLen == kCdicHeaderLen);
    if (hdrLen != kCdicHeaderLen)
        return false;
    uint32 size = cdicDataLen - hdrLen;

    uint32 maxSize = 1 << codeLength;
    if (maxSize >= size)
        return false;
    dicts[dictsCount] = cdicData + hdrLen;
    dictSize[dictsCount] = size;
    ++dictsCount;
    return true;
}

static void DecodeMobiDocHeader(const char *buf, MobiHeader* hdr)
{
    ByteOrderDecoder d(buf, kMobiHeaderLen, ByteOrderDecoder::BigEndian);
    d.Bytes(hdr->id, 4);
    hdr->hdrLen =               d.UInt32();
    hdr->type =                 d.UInt32();
    hdr->textEncoding =         d.UInt32();
    hdr->uniqueId =             d.UInt32();
    hdr->mobiFormatVersion =    d.UInt32();
    hdr->ortographicIdxRec =    d.UInt32();
    hdr->inflectionIdxRec =     d.UInt32();
    hdr->namesIdxRec =          d.UInt32();
    hdr->keysIdxRec =           d.UInt32();
    hdr->extraIdx0Rec =         d.UInt32();
    hdr->extraIdx1Rec =         d.UInt32();
    hdr->extraIdx2Rec =         d.UInt32();
    hdr->extraIdx3Rec =         d.UInt32();
    hdr->extraIdx4Rec =         d.UInt32();
    hdr->extraIdx5Rec =         d.UInt32();
    hdr->firstNonBookRec =      d.UInt32();
    hdr->fullNameOffset =       d.UInt32();
    hdr->fullNameLen =          d.UInt32();
    hdr->locale =               d.UInt32();
    hdr->inputDictLanguage =    d.UInt32();
    hdr->outputDictLanguage =   d.UInt32();
    hdr->minRequiredMobiFormatVersion = d.UInt32();
    hdr->imageFirstRec =        d.UInt32();
    hdr->huffmanFirstRec =      d.UInt32();
    hdr->huffmanRecCount =      d.UInt32();
    hdr->huffmanTableOffset =   d.UInt32();
    hdr->huffmanTableLen =      d.UInt32();
    hdr->exthFlags =            d.UInt32();
    CrashIf(kMobiHeaderMinLen != d.Offset());

    if (hdr->hdrLen < kMobiHeaderMinLen + 48)
        return;

    d.Bytes(hdr->reserved1, 32);
    hdr->drmOffset =            d.UInt32();
    hdr->drmEntriesCount =      d.UInt32();
    hdr->drmSize =              d.UInt32();
    hdr->drmFlags =             d.UInt32();

    if (hdr->hdrLen < 228) // magic number at which extraDataFlags becomes valid
        return;

    d.Bytes(hdr->reserved2, 62);
    hdr->extraDataFlags =       d.UInt16();
    hdr->indxRec =              d.UInt32();
    CrashIf(kMobiHeaderLen != d.Offset());
}

static PdbDocType GetPdbDocType(const char *typeCreator)
{
    if (str::Eq(typeCreator, MOBI_TYPE_CREATOR))
        return Pdb_Mobipocket;
    if (str::Eq(typeCreator, PALMDOC_TYPE_CREATOR))
        return Pdb_PalmDoc;
    if (str::Eq(typeCreator, TEALDOC_TYPE_CREATOR))
        return Pdb_TealDoc;
    return Pdb_Unknown;
}

static bool IsValidCompression(int comprType)
{
    return  (COMPRESSION_NONE == comprType) ||
            (COMPRESSION_PALM == comprType) ||
            (COMPRESSION_HUFF == comprType);
}

MobiDoc::MobiDoc(const TCHAR *filePath) :
    fileName(str::Dup(filePath)), pdbReader(NULL),
    docType(Pdb_Unknown), docRecCount(0), compressionType(0), docUncompressedSize(0),
    doc(NULL), multibyte(false), trailersCount(0), imageFirstRec(0),
    imagesCount(0), images(NULL), huffDic(NULL), textEncoding(CP_UTF8)
{
}

MobiDoc::~MobiDoc()
{
    free(fileName);
    free(images);
    delete huffDic;
    delete doc;
    delete pdbReader;
    for (size_t i = 0; i < props.Count(); i++) {
        free(props.At(i).value);
    }
}

bool MobiDoc::ParseHeader()
{
    pdbReader = new PdbReader(fileName);
    if (pdbReader->GetRecordCount() == 0)
        return false;

    docType = GetPdbDocType(pdbReader->GetDbType());
    if (Pdb_Unknown == docType) {
        lf("unknown pdb type/creator");
        return false;
    }

    size_t recSize;
    const char *firstRecData = pdbReader->GetRecord(0, &recSize);
    if (!firstRecData || recSize < kPalmDocHeaderLen) {
        lf("failed to read record 0");
        return false;
    }

    PalmDocHeader palmDocHdr;
    DecodePalmDocHeader(firstRecData, &palmDocHdr);
    compressionType = palmDocHdr.compressionType;
    if (!IsValidCompression(compressionType)) {
        lf("unknown compression type");
        return false;
    }
    if (Pdb_Mobipocket == docType) {
        // TODO: this needs to be surfaced to the client so
        // that we can show the right error message
        if (palmDocHdr.mobi.encrType != ENCRYPTION_NONE) {
            lf("encryption is unsupported");
            return false;
        }
    }
    docRecCount = palmDocHdr.recordsCount;
    docUncompressedSize = palmDocHdr.uncompressedDocSize;

    if (kPalmDocHeaderLen == recSize) {
        CrashIf(Pdb_Mobipocket == docType);
        // TODO: calculate imageFirstRec / imagesCount
        return true;
    }
    if (kPalmDocHeaderLen + kMobiHeaderMinLen > recSize) {
        lf("not enough data for decoding MobiHeader");
        // id and hdrLen
        return false;
    }

    MobiHeader mobiHdr;
    DecodeMobiDocHeader(firstRecData + kPalmDocHeaderLen, &mobiHdr);
    if (!str::EqN("MOBI", mobiHdr.id, 4)) {
        lf("MobiHeader.id is not 'MOBI'");
        return false;
    }
    textEncoding = mobiHdr.textEncoding;

    if (pdbReader->GetRecordCount() > mobiHdr.imageFirstRec) {
        imageFirstRec = mobiHdr.imageFirstRec;
        if (0 == imageFirstRec) {
            // I don't think this should ever happen but I've seen it
            imagesCount = 0;
        } else
            imagesCount = pdbReader->GetRecordCount() - imageFirstRec;
    }
    if (kPalmDocHeaderLen + mobiHdr.hdrLen > recSize) {
        lf("MobiHeader too big");
        return false;
    }

    bool hasExtraFlags = (mobiHdr.hdrLen >= 228); // TODO: also only if mobiFormatVersion >= 5?
    if (hasExtraFlags) {
        uint16 flags = mobiHdr.extraDataFlags;
        multibyte = ((flags & 1) != 0);
        while (flags > 1) {
            if (0 != (flags & 2))
                trailersCount++;
            flags = flags >> 1;
        }
    }

    if (COMPRESSION_HUFF == compressionType) {
        CrashIf(Pdb_Mobipocket != docType);
        size_t recSize;
        const char *recData = pdbReader->GetRecord(mobiHdr.huffmanFirstRec, &recSize);
        if (!recData)
            return false;
        assert(NULL == huffDic);
        huffDic = new HuffDicDecompressor();
        if (!huffDic->SetHuffData((uint8*)recData, recSize))
            return false;
        size_t cdicsCount = mobiHdr.huffmanRecCount - 1;
        assert(cdicsCount <= kCdicsMax);
        if (cdicsCount > kCdicsMax)
            return false;
        for (size_t i = 0; i < cdicsCount; i++) {
            recData = pdbReader->GetRecord(mobiHdr.huffmanFirstRec + 1 + i, &recSize);
            if (!recData)
                return false;
            if (!huffDic->AddCdicData((uint8*)recData, recSize))
                return false;
        }
    }

    if ((mobiHdr.exthFlags & 0x40)) {
        uint32 offset = kPalmDocHeaderLen + mobiHdr.hdrLen;
        DecodeExthHeader(firstRecData + offset, recSize - offset);
    }

    LoadImages();
    return true;
}

bool MobiDoc::DecodeExthHeader(const char *data, size_t dataLen)
{
    if (dataLen < 12 || !str::EqN(data, "EXTH", 4))
        return false;

    ByteOrderDecoder d(data, dataLen, ByteOrderDecoder::BigEndian);
    d.Skip(4);
    uint32 hdrLen = d.UInt32();
    uint32 count = d.UInt32();
    if (hdrLen > dataLen)
        return false;

    for (uint32 i = 0; i < count; i++) {
        uint32 type = d.UInt32();
        uint32 length = d.UInt32();
        if (length > dataLen - d.Offset() + 8)
            return false;
        d.Skip(length - 8);

        Metadata prop;
        switch (type) {
        case 100: prop.prop = Prop_Author; break;
        case 105: prop.prop = Prop_Subject; break;
        case 106: prop.prop = Prop_CreationDate; break;
        case 108: prop.prop = Prop_CreatorApp; break;
        case 109: prop.prop = Prop_Copyright; break;
        case 503: prop.prop = Prop_Title; break;
        default:  continue;
        }
        prop.value = str::DupN(data + d.Offset() - length + 8, length - 8);
        if (prop.value)
            props.Append(prop);
    }

    return true;
}

#define EOF_REC   0xe98e0d0a
#define FLIS_REC  0x464c4953 // 'FLIS'
#define FCIS_REC  0x46434953 // 'FCIS
#define FDST_REC  0x46445354 // 'FDST'
#define DATP_REC  0x44415450 // 'DATP'
#define SRCS_REC  0x53524353 // 'SRCS'
#define VIDE_REC  0x56494445 // 'VIDE'

static bool IsEofRecord(uint8 *data, size_t dataLen)
{
    return (4 == dataLen) && (EOF_REC == UInt32BE(data));
}

static bool KnownNonImageRec(uint8 *data, size_t dataLen)
{
    if (dataLen < 4)
        return false;
    uint32 sig = UInt32BE(data);

    if (FLIS_REC == sig) return true;
    if (FCIS_REC == sig) return true;
    if (FDST_REC == sig) return true;
    if (DATP_REC == sig) return true;
    if (SRCS_REC == sig) return true;
    if (VIDE_REC == sig) return true;
    return false;
}

static bool KnownImageFormat(const char *data, size_t dataLen)
{
    return NULL != GfxFileExtFromData(data, dataLen);
}

// return false if we should stop loading images (because we
// encountered eof record or ran out of memory)
bool MobiDoc::LoadImage(size_t imageNo)
{
    size_t imageRec = imageFirstRec + imageNo;
    size_t imgDataLen;

    const char *imgData = pdbReader->GetRecord(imageRec, &imgDataLen);
    if (!imgData || (0 == imgDataLen))
        return true;
    if (IsEofRecord((uint8 *)imgData, imgDataLen))
        return false;
    if (KnownNonImageRec((uint8 *)imgData, imgDataLen))
        return true;
    if (!KnownImageFormat(imgData, imgDataLen)) {
        lf("Unknown image format");
        return true;
    }
    images[imageNo].data = (char *)imgData;
    if (!images[imageNo].data)
        return false;
    images[imageNo].len = imgDataLen;
    return true;
}

void MobiDoc::LoadImages()
{
    if (0 == imagesCount)
        return;
    images = SAZA(ImageData, imagesCount);
    for (size_t i = 0; i < imagesCount; i++) {
        if (!LoadImage(i))
            return;
    }
}

// imgRecIndex corresponds to recindex attribute of <img> tag
// as far as I can tell, this means: it starts at 1 
// returns NULL if there is no image (e.g. it's not a format we
// recognize)
ImageData *MobiDoc::GetImage(size_t imgRecIndex) const
{
    if ((imgRecIndex > imagesCount) || (imgRecIndex < 1))
        return NULL;
    --imgRecIndex;
    if (!images[imgRecIndex].data || (0 == images[imgRecIndex].len))
        return NULL;
    return &images[imgRecIndex];
}

// first two images seem to be the same picture of the cover
// except at different resolutions
ImageData *MobiDoc::GetCoverImage()
{
    size_t coverImage = 0;
    Size size;
    size_t maxImageNo = min(imagesCount, 2);
    for (size_t i = 0; i < maxImageNo; i++) {
        if (!images[i].data)
            continue;
        Size s = BitmapSizeFromData(images[i].data, images[i].len);
        int32 prevSize = size.Width * size.Height;
        int32 currSize = s.Width * s.Height;
        if (currSize > prevSize) {
            coverImage = i;
            size = s;
        }
    }
    if (size.Empty())
        return NULL;
    return &images[coverImage];
}

// each record can have extra data at the end, which we must discard
// returns (size_t)-1 on error
static size_t GetRealRecordSize(uint8 *recData, size_t recLen, size_t trailersCount, bool multibyte)
{
    for (size_t i = 0; i < trailersCount; i++) {
        if (recLen < 4)
            return (size_t)-1;
        uint32 n = 0;
        for (size_t j = 0; j < 4; j++) {
            uint8 v = recData[recLen - 4 + j];
            if (0 != (v & 0x80))
                n = 0;
            n = (n << 7) | (v & 0x7f);
        }
        if (n > recLen)
            return (size_t)-1;
        recLen -= n;
    }

    if (multibyte) {
        if (0 == recLen)
            return (size_t)-1;
        uint8 n = (recData[recLen-1] & 3) + 1;
        if (n > recLen)
            return (size_t)-1;
        recLen -= n;
    }

    return recLen;
}

// Load a given record of a document into strOut, uncompressing if necessary.
// Returns false if error.
bool MobiDoc::LoadDocRecordIntoBuffer(size_t recNo, str::Str<char>& strOut)
{
    size_t recSize;
    const char *recData = pdbReader->GetRecord(recNo, &recSize);
    if (NULL == recData)
        return false;
    recSize = GetRealRecordSize((uint8*)recData, recSize, trailersCount, multibyte);
    if ((size_t)-1 == recSize)
        return false;

    if (COMPRESSION_NONE == compressionType) {
        strOut.Append(recData, recSize);
        return true;
    }
    if (COMPRESSION_PALM == compressionType) {
        bool ok = PalmdocUncompress(recData, recSize, strOut);
        if (!ok)
            lf("PalmDoc decompression failed");
        return ok;
    }
    if (COMPRESSION_HUFF == compressionType && huffDic) {
        bool ok = huffDic->Decompress((uint8*)recData, recSize, strOut);
        if (!ok)
            lf("HuffDic decompression failed");
        return ok;
    }

    assert(0);
    return false;
}

bool MobiDoc::LoadDocument()
{
    if (!ParseHeader())
        return false;

    assert(!doc);
    doc = new str::Str<char>(docUncompressedSize);
    for (size_t i = 1; i <= docRecCount; i++) {
        if (!LoadDocRecordIntoBuffer(i, *doc))
            return false;
    }
    if (textEncoding != CP_UTF8) {
        char *docUtf8 = str::ToMultiByte(doc->Get(), textEncoding, CP_UTF8);
        if (docUtf8) {
            doc->Reset();
            doc->AppendAndFree(docUtf8);
        }
    }
    return true;
}

char *MobiDoc::GetBookHtmlData(size_t& lenOut) const
{
    lenOut = doc->Size();
    return doc->Get();
}

TCHAR *MobiDoc::GetProperty(DocumentProperty prop)
{
    for (size_t i = 0; i < props.Count(); i++) {
        if (props.At(i).prop == prop)
            return str::conv::FromCodePage(props.At(i).value, textEncoding);
    }
    return NULL;
}

bool MobiDoc::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    // TODO: also accept .prc as MobiEngine::IsSupportedFile ?
    return str::EndsWithI(fileName, _T(".mobi"));
}

MobiDoc *MobiDoc::CreateFromFile(const TCHAR *fileName)
{
    MobiDoc *mb = new MobiDoc(fileName);
    if (!mb->LoadDocument()) {
        delete mb;
        return NULL;
    }
    return mb;
}

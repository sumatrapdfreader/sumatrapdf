/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "MobiDoc.h"

#include "BitReader.h"
#include "ByteOrderDecoder.h"
#include "EbookBase.h"
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
    uint32       exhtFlags;  // bitfield. if bit 6 (0x40) is set => there's an EXTH record
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

// change big-endian int16 to host endianness
static void SwapU16(uint16& i)
{
    i = UInt16BE((uint8*)&i);
}

// change big-endian unt16 to host endianness
static void SwapU32(uint32& i)
{
    i = UInt32BE((uint8*)&i);
}

// Uncompress source data compressed with PalmDoc compression into a buffer.
// http://wiki.mobileread.com/wiki/PalmDOC#Format
// Returns false on decoding errors
static bool PalmdocUncompress(uint8 *src, size_t srcLen, str::Str<char>& dst)
{
    uint8 *srcEnd = src + srcLen;
    while (src < srcEnd) {
        uint8 c = *src++;
        if ((c >= 1) && (c <= 8)) {
            for (uint8 n = c; n > 0; n--) {
                dst.Append((char)*src++);
            }
        } else if (c < 128) {
            dst.Append((char)c);
        } else if ((c >= 128) && (c < 192)) {
            if (src >= srcEnd)
                return false;
            uint16 c2 = (c << 8) | *src++;
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
    uint32       baseTableOffset;   // should be 1024 + 24
    // like cacheOffset except data is in little endian
    uint32       cacheOffsetLE;     // should be 64 + 1024 + 24
    // like baseTableOffset except data is in little endian
    uint32       baseTableOffsetLE; // should be 1024 + 64 + 1024 + 24
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

#define kCacheDataLen      (256*4)
#define kBaseTableDataLen  (64*4)

#define kHuffRecordMinLen (kHuffHeaderLen +     kCacheDataLen +     kBaseTableDataLen)
#define kHuffRecordLen    (kHuffHeaderLen + 2 * kCacheDataLen + 2 * kBaseTableDataLen)

#define kCdicsMax 32

class HuffDicDecompressor
{
    // underlying data for cache and baseTable
    // (an optimization to only do one allocation instead of two)
    uint8 *     huffmanData;

    uint32 *    cacheTable;
    uint32 *    baseTable;

    size_t      dictsCount;
    uint8 *     dicts[kCdicsMax];
    uint32      dictSize[kCdicsMax];

    uint32      codeLength;

public:
    HuffDicDecompressor();
    ~HuffDicDecompressor();
    bool SetHuffData(uint8 *huffData, size_t huffDataLen);
    bool AddCdicData(uint8 *cdicData, uint32 cdicDataLen);
    size_t Decompress(uint8 *src, size_t octets, uint8 *dst, size_t avail_in);
    bool DecodeOne(uint32 code, uint8 *& dst, size_t& dstLeft);
};

HuffDicDecompressor::HuffDicDecompressor() :
    huffmanData(NULL), cacheTable(NULL), baseTable(NULL),
    codeLength(0), dictsCount(0)
{
}

HuffDicDecompressor::~HuffDicDecompressor()
{
    for (size_t i = 0; i < dictsCount; i++) {
        free(dicts[i]);
    }
    free(huffmanData);
}

bool HuffDicDecompressor::DecodeOne(uint32 code, uint8 *& dst, size_t& dstLeft)
{
    uint16 dict = code >> codeLength;
    if ((size_t)dict > dictsCount) {
        lf("invalid dict value");
        return false;
    }
    code &= ((1 << (codeLength)) - 1);
    uint16 offset = UInt16BE(dicts[dict] + code * 2);

    if ((uint32)offset > dictSize[dict]) {
        lf("invalid offset");
        return false;
    }
    uint16 symLen = UInt16BE(dicts[dict] + offset);
    uint8 *p = dicts[dict] + offset + 2;

    if (!(symLen & 0x8000)) {
        size_t res = Decompress(p, symLen, dst, dstLeft);
        if (-1 == res)
            return false;
        dst += res;
        assert(dstLeft >= res);
        dstLeft -= res;
    } else {
        symLen &= 0x7fff;
        if (symLen > 127) {
            lf("symLen too big");
            return false;
        }
        if (symLen > dstLeft) {
            lf("not enough space");
            return false;
        }
        memcpy(dst, p, symLen);
        dst += symLen;
        dstLeft -= symLen;
    }
    return true;
}

size_t HuffDicDecompressor::Decompress(uint8 *src, size_t srcSize, uint8 *dst, size_t dstSize)
{
    uint32    bitsConsumed = 0;
    uint32    bits = 0;

    BitReader br(src, srcSize);
    size_t      dstLeft = dstSize;

    for (;;) {
        if (bitsConsumed > br.BitsLeft()) {
            lf("not enough data");
            return -1;
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
            return -1;
        }
        bool isTerminal = (v & 0x80) != 0;

        uint32 code;
        if (isTerminal) {
            code = (v >> 8) - (bits >> (32 - codeLen));
        } else {
            uint32 baseVal;
            codeLen -= 1;
            do {
                baseVal = baseTable[codeLen*2];
                code = (bits >> (32 - (codeLen+1)));
                codeLen++;
                if (codeLen > 32) {
                    lf("code len > 32 bits");
                    return -1;
                }
            } while (baseVal > code);
            code = baseTable[1 + ((codeLen - 1) * 2)] - (bits >> (32 - codeLen));
        }

        if (!DecodeOne(code, dst, dstLeft))
            return -1;
        bitsConsumed = codeLen;
    }

    if (br.BitsLeft() > 0 && 0 != bits) {
        lf("compressed data left");
    }
    return dstSize - dstLeft;
}

bool HuffDicDecompressor::SetHuffData(uint8 *huffData, size_t huffDataLen)
{
    // for now catch cases where we don't have both big endian and little endian
    // versions of the data
    assert(kHuffRecordLen == huffDataLen);
    // but conservatively assume we only need big endian version
    if (huffDataLen < kHuffRecordMinLen)
        return false;
    HuffHeader *huffHdr = (HuffHeader*)huffData;
    SwapU32(huffHdr->hdrLen);
    SwapU32(huffHdr->cacheOffset);
    SwapU32(huffHdr->baseTableOffset);

    if (!str::EqN("HUFF", huffHdr->id, 4))
        return false;
    assert(huffHdr->hdrLen == kHuffHeaderLen);
    if (huffHdr->hdrLen != kHuffHeaderLen)
        return false;
    if (huffHdr->cacheOffset != kHuffHeaderLen)
        return false;
    if (huffHdr->baseTableOffset != (huffHdr->cacheOffset + kCacheDataLen))
        return false;
    assert(NULL == huffmanData);
    huffmanData = (uint8*)memdup(huffData, huffDataLen);
    if (!huffmanData)
        return false;
    // we conservatively use the big-endian version of the data,
    cacheTable = (uint32*)(huffmanData + huffHdr->cacheOffset);
    for (size_t i = 0; i < 256; i++) {
        SwapU32(cacheTable[i]);
    }
    baseTable = (uint32*)(huffmanData + huffHdr->baseTableOffset);
    for (size_t i = 0; i < 64; i++) {
        SwapU32(baseTable[i]);
    }
    return true;
}

bool HuffDicDecompressor::AddCdicData(uint8 *cdicData, uint32 cdicDataLen)
{
    CdicHeader *cdicHdr = (CdicHeader*)cdicData;
    SwapU32(cdicHdr->hdrLen);
    SwapU32(cdicHdr->codeLen);

    assert((0 == codeLength) || (cdicHdr->codeLen == codeLength));
    codeLength = cdicHdr->codeLen;

    if (!str::EqN("CDIC", cdicHdr->id, 4))
        return false;
    assert(cdicHdr->hdrLen == kCdicHeaderLen);
    if (cdicHdr->hdrLen != kCdicHeaderLen)
        return false;
    uint32 size = cdicDataLen - cdicHdr->hdrLen;

    uint32 maxSize = 1 << codeLength;
    if (maxSize >= size)
        return false;
    dicts[dictsCount] = (uint8*)memdup(cdicData + cdicHdr->hdrLen, size);
    dictSize[dictsCount] = size;
    ++dictsCount;
    return true;
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
}

bool MobiDoc::ParseHeader()
{
    pdbReader = new PdbReader(fileName);
    if (pdbReader->GetRecordCount() == 0)
        return false;

    docType = GetPdbDocType(pdbReader->GetDbType());
    if (Pdb_Unknown == docType) {
        lf(" unknown pdb type/creator");
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

    if (!IsValidCompression(palmDocHdr.compressionType)) {
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
    compressionType = palmDocHdr.compressionType;

    if (kPalmDocHeaderLen == recSize) {
        CrashIf(Pdb_Mobipocket == docType);
        // TODO: calculate imageFirstRec / imagesCount
        return true;
    }
    if (recSize - kPalmDocHeaderLen < 8) // id and hdrLen
        return false;

    MobiHeader *mobiHdr = (MobiHeader *)(firstRecData + kPalmDocHeaderLen);
    if (!str::EqN("MOBI", mobiHdr->id, 4)) {
        lf("MobiHeader.id is not 'MOBI'");
        return false;
    }
    SwapU32(mobiHdr->hdrLen);
    SwapU32(mobiHdr->type);
    SwapU32(mobiHdr->textEncoding);
    SwapU32(mobiHdr->mobiFormatVersion);
    SwapU32(mobiHdr->firstNonBookRec);
    SwapU32(mobiHdr->fullNameOffset);
    SwapU32(mobiHdr->fullNameLen);
    SwapU32(mobiHdr->locale);
    SwapU32(mobiHdr->minRequiredMobiFormatVersion);
    SwapU32(mobiHdr->imageFirstRec);
    SwapU32(mobiHdr->huffmanFirstRec);
    SwapU32(mobiHdr->huffmanRecCount);
    SwapU32(mobiHdr->huffmanTableOffset);
    SwapU32(mobiHdr->huffmanTableLen);
    SwapU32(mobiHdr->exhtFlags);

    textEncoding = mobiHdr->textEncoding;

    if (pdbReader->GetRecordCount() > mobiHdr->imageFirstRec) {
        imageFirstRec = mobiHdr->imageFirstRec;
        if (0 == imageFirstRec) {
            // I don't think this should ever happen but I've seen it
            imagesCount = 0;
        } else
            imagesCount = pdbReader->GetRecordCount() - mobiHdr->imageFirstRec;
    }
    size_t hdrLen = mobiHdr->hdrLen;
    if (kPalmDocHeaderLen + hdrLen > recSize) {
        lf("MobiHeader too big");
        return false;
    }
    bool hasExtraFlags = (hdrLen >= 228); // TODO: also only if mobiFormatVersion >= 5?

    if (hasExtraFlags) {
        SwapU16(mobiHdr->extraDataFlags);
        uint16 flags = mobiHdr->extraDataFlags;
        multibyte = ((flags & 1) != 0);
        while (flags > 1) {
            if (0 != (flags & 2))
                trailersCount++;
            flags = flags >> 1;
        }
    }

    if (palmDocHdr.compressionType == COMPRESSION_HUFF) {
        CrashIf(Pdb_Mobipocket != docType);
        size_t recSize;
        const char *recData = pdbReader->GetRecord(mobiHdr->huffmanFirstRec, &recSize);
        if (!recData)
            return false;
        size_t cdicsCount = mobiHdr->huffmanRecCount - 1;
        assert(cdicsCount <= kCdicsMax);
        if (cdicsCount > kCdicsMax)
            return false;
        assert(NULL == huffDic);
        huffDic = new HuffDicDecompressor();
        if (!huffDic->SetHuffData((uint8*)recData, recSize))
            return false;
        for (size_t i = 0; i < cdicsCount; i++) {
            recData = pdbReader->GetRecord(mobiHdr->huffmanFirstRec + 1 + i, &recSize);
            if (!recData)
                return false;
            if (!huffDic->AddCdicData((uint8*)recData, recSize))
                return false;
        }
    }

    LoadImages();
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
    CrashIf((imgRecIndex > imagesCount) || (imgRecIndex < 1));
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
static size_t ExtraDataSize(uint8 *recData, size_t recLen, size_t trailersCount, bool multibyte)
{
    size_t newLen = recLen;
    for (size_t i = 0; i < trailersCount; i++) {
        assert(newLen > 4);
        uint32 n = 0;
        for (size_t j = 0; j < 4; j++) {
            uint8 v = recData[newLen - 4 + j];
            if (0 != (v & 0x80))
                n = 0;
            n = (n << 7) | (v & 0x7f);
        }
        assert(newLen > n);
        newLen -= n;
    }

    if (multibyte) {
        assert(newLen > 0);
        if (newLen > 0) {
            uint8 n = (recData[newLen-1] & 3) + 1;
            assert(newLen >= n);
            newLen -= n;
        }
    }
    return recLen - newLen;
}

// Load a given record of a document into strOut, uncompressing if necessary.
// Returns false if error.
bool MobiDoc::LoadDocRecordIntoBuffer(size_t recNo, str::Str<char>& strOut)
{
    size_t recSize;
    const char *recData = pdbReader->GetRecord(recNo, &recSize);
    if (NULL == recData)
        return false;
    size_t extraSize = ExtraDataSize((uint8*)recData, recSize, trailersCount, multibyte);
    recSize -= extraSize;
    if (COMPRESSION_NONE == compressionType) {
        strOut.Append(recData, recSize);
        return true;
    }

    if (COMPRESSION_PALM == compressionType) {
        bool ok = PalmdocUncompress((uint8 *)recData, recSize, strOut);
        if (!ok)
            lf("PalmDoc decompression failed");
        return ok;
    }

    if (COMPRESSION_HUFF == compressionType) {
        char buf[6000]; // should be enough to decompress any record
        assert(huffDic);
        if (!huffDic)
            return false;
        size_t uncompressedSize = huffDic->Decompress((uint8*)recData, recSize, (uint8*)buf, sizeof(buf));
        if (-1 == uncompressedSize) {
            lf("HuffDic decompression failed");
            return false;
        }
        strOut.Append(buf, uncompressedSize);
        return true;
    }

    assert(0);
    return false;
}

// assumes that ParseHeader() has been called
bool MobiDoc::LoadDocument()
{
    assert(docUncompressedSize > 0);
    assert(!doc);
    doc = new str::Str<char>(docUncompressedSize);
    for (size_t i = 1; i <= docRecCount; i++) {
        if (!LoadDocRecordIntoBuffer(i, *doc))
            return false;
    }
    // in one PalmDOC file the value is off-by-one (counting the trailing zero?)
    assert(docUncompressedSize == doc->Size() || docUncompressedSize == doc->Size() + 1);
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

bool MobiDoc::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    // TODO: also accept .prc as MobiEngine::IsSupportedFile ?
    return str::EndsWithI(fileName, _T(".mobi"));
}

MobiDoc *MobiDoc::CreateFromFile(const TCHAR *fileName)
{
    MobiDoc *mb = new MobiDoc(fileName);
    if (!mb->ParseHeader())
        goto Error;
    if (!mb->LoadDocument())
        goto Error;
    return mb;
Error:
    delete mb;
    return NULL;
}

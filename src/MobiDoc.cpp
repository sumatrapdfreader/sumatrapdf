/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiDoc.h"
#include <time.h>

#include "BitReader.h"
#include "FileUtil.h"

#include "StrUtil.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

// Parse mobi format http://wiki.mobileread.com/wiki/MOBI

#define NOLOG 0
#include "DebugLog.h"

#define PALMDOC_TYPE_CREATOR   "TEXtREAd"
#define MOBI_TYPE_CREATOR      "BOOKMOBI"

#define COMPRESSION_NONE 1
#define COMPRESSION_PALM 2
#define COMPRESSION_HUFF 17480

#define ENCRYPTION_NONE 0
#define ENCRYPTION_OLD  1
#define ENCRYPTION_NEW  2

// http://wiki.mobileread.com/wiki/MOBI#PalmDOC_Header
#define kPalmDocHeaderLen 16
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
STATIC_ASSERT(kPalmDocHeaderLen == sizeof(PalmDocHeader), validMobiFirstRecord);

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
    char        id[4];
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
    uint16      extraDataFlags;
    int32       indxRec;
};

STATIC_ASSERT(kMobiHeaderLen == sizeof(MobiHeader), validMobiHeader);

// change big-endian int16 to little-endian (our native format)
static void SwapU16(uint16& i)
{
    i = BEtoHs(i);
}

static void SwapU32(uint32& i)
{
    i = BEtoHl(i);
}

// Uncompress source data compressed with PalmDoc compression into a buffer.
// Returns size of uncompressed data or -1 on error (if destination buffer too small)
static size_t PalmdocUncompress(uint8 *src, size_t srcLen, uint8 *dst, size_t dstLen)
{
    uint8 *srcEnd = src + srcLen;
    uint8 *dstEnd = dst + dstLen;
    uint8 *dstOrig = dst;
    size_t dstLeft;
    while (src < srcEnd) {
        dstLeft = dstEnd - dst;
        assert(dstLeft > 0);
        if (0 == dstLeft)
            return -1;

        unsigned c = *src++;

        if ((c >= 1) && (c <= 8)) {
            assert(dstLeft >= c);
            if (dstLeft < c)
                return -1;
            while (c > 0) {
                *dst++ = *src++;
                --c;
            }
        } else if (c < 128) {
            assert(c != 0);
            *dst++ = c;
        } else if (c >= 192) {
            assert(dstLeft >= 2);
            if (dstLeft < 2)
                return -1;
            *dst++ = ' ';
            *dst++ = c ^ 0x80;
        } else {
            assert((c >= 128) && (c < 192));
            assert(src < srcEnd);
            if (src < srcEnd) {
                c = (c << 8) | *src++;
                size_t back = (c >> 3) & 0x07ff;
                size_t n = (c & 7) + 3;
                uint8 *dstBack = dst - back;
                assert(dstBack >= dstOrig);
                assert(dstLeft >= n);
                while (n > 0) {
                    *dst++ = *dstBack++;
                    --n;
                }
            }
        }
    }

    // zero-terminate to make inspecting in the debugger easier
    if (dst < dstEnd)
        *dst = 0;

    return dst - dstOrig;
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

    uint32      code_length;

public:
    HuffDicDecompressor();
    ~HuffDicDecompressor();
    bool SetHuffData(uint8 *huffData, size_t huffDataLen);
    bool AddCdicData(uint8 *cdicData, uint32 cdicDataLen);
    size_t Decompress(uint8 *src, size_t octets, uint8 *dst, size_t avail_in);
    //size_t Decompress2(uint8 *src, size_t srcSize, uint8 *dst, size_t dstSize);
    bool DecodeOne(uint32 code, uint8 *& dst, size_t& dstLeft);
};

HuffDicDecompressor::HuffDicDecompressor() :
    huffmanData(NULL), cacheTable(NULL), baseTable(NULL),
    code_length(0), dictsCount(0)
{
}

HuffDicDecompressor::~HuffDicDecompressor()
{
    for (size_t i = 0; i < dictsCount; i++) {
        free(dicts[i]);
    }
    free(huffmanData);
}

#if 0
// cache is array of 256 values (caller ensures that)
bool HuffDicDecompressor::UnpackCacheData(uint32 *cache)
{
    for (size_t i = 0; i < 256; i++) {
        uint32 v = cache[i];
        SwapU32(v);
        bool isTerminal =  (v & 0x80) != 0;
        uint8 valLen = v & 0x1f;
        uint32 val = v >> 8;
        if (valLen == 0)
            return false;
        if ((valLen <= 8) && !isTerminal)
            return false;
        val = ((val + 1) << (32 - valLen)) - 1;
        dict1[i].isTerminal = isTerminal;
        dict1[i].valLen = valLen;
        dict1[i].val = val;
    }
    return true;
}
#endif

uint16 ReadBeU16(uint8 *d)
{
    uint16 v = *((uint16*)d);
    SwapU16(v);
    return v;
}

#if 0
uint64_t GetNext64Bits(uint8 *& s, size_t& srcSize)
{
    uint64 v = 0;
    for (size_t i = 0; i < 8; i++) {
        v = v << 8;
        if (srcSize > 0) {
            v |= *s++;
            --srcSize;
        }
    }
    return v;
}

bool DecodeCode(uint32 v, bool& isTerminal, uint32& codeLen, uint32& maxCode)
{
    //SwapU32(v);
    isTerminal =  (v & 0x80) != 0;
    codeLen = v & 0x1f;
    maxCode = v >> 8;
    if (codeLen == 0)
        return false;
    if ((codeLen <= 8) && !isTerminal)
        return false;
    maxCode = ((maxCode + 1) << (32 - codeLen)) - 1;
    return true;
}

size_t HuffDicDecompressor::Decompress2(uint8 *src, size_t srcSize, uint8 *dst, size_t dstSize)
{
    int bitsLeft = srcSize * 8;
    uint64_t x = GetNext64Bits(src, srcSize);
    int n = 32;
    bool isTerminal;
    uint32 val, valLen;
    for (;;) {
        if (n <= 0) {
            x = GetNext64Bits(src, srcSize);
            n += 32;
        }
        uint64_t mask = (1ull << 32) - 1;
        uint32 code = (uint32_t)((x >> n) & mask);

        if (!DecodeCode(code, isTerminal, valLen, val))
            return -1;
    }
}
#endif

bool HuffDicDecompressor::DecodeOne(uint32 code, uint8 *& dst, size_t& dstLeft)
{
    uint16 dict = code >> code_length;
    if ((size_t)dict > dictsCount) {
        lf("invalid dict value");
        return false;
    }
    code &= ((1 << (code_length)) - 1);
    uint16 offset = ReadBeU16(dicts[dict] + code * 2);

    if ((uint32)offset > dictSize[dict]) {
        lf("invalid offset");
        return false;
    }
    uint16 symLen = ReadBeU16(dicts[dict] + offset);
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

    assert((0 == code_length) || (cdicHdr->codeLen == code_length));
    code_length = cdicHdr->codeLen;

    if (!str::EqN("CDIC", cdicHdr->id, 4))
        return false;
    assert(cdicHdr->hdrLen == kCdicHeaderLen);
    if (cdicHdr->hdrLen != kCdicHeaderLen)
        return false;
    uint32 size = cdicDataLen - cdicHdr->hdrLen;

    uint32 maxSize = 1 << code_length;
    if (maxSize >= size)
        return false;
    dicts[dictsCount] = (uint8*)memdup(cdicData + cdicHdr->hdrLen, size);
    dictSize[dictsCount] = size;
    ++dictsCount;
    return true;
}

static bool IsMobiPdb(PdbHeader *pdbHdr)
{
    return str::EqN(pdbHdr->type, MOBI_TYPE_CREATOR, 8);
}

static bool IsPalmDocPdb(PdbHeader *pdbHdr)
{
    return str::EqN(pdbHdr->type, PALMDOC_TYPE_CREATOR, 8);
}

static bool IsValidCompression(int comprType)
{
    return  (COMPRESSION_NONE == comprType) ||
            (COMPRESSION_PALM == comprType) ||
            (COMPRESSION_HUFF == comprType);
}

MobiDoc::MobiDoc() :
    fileName(NULL), fileHandle(0), recHeaders(NULL), firstRecData(NULL), isMobi(false),
    docRecCount(0), compressionType(0), docUncompressedSize(0), doc(NULL),
    multibyte(false), trailersCount(0), imageFirstRec(0), imagesCount(0),
    images(NULL), bufDynamic(NULL), bufDynamicSize(0), huffDic(NULL)
{
}

MobiDoc::~MobiDoc()
{
    CloseHandle(fileHandle);
    free(fileName);
    free(firstRecData);
    free(recHeaders);
    free(bufDynamic);
    for (size_t i = 0; i < imagesCount; i++)
        free(images[i].data);
    free(images);
    delete huffDic;
    delete doc;
}

bool MobiDoc::ParseHeader()
{
    DWORD bytesRead;
    BOOL ok = ReadFile(fileHandle, (void*)&pdbHeader, kPdbHeaderLen, &bytesRead, NULL);
    if (!ok || (kPdbHeaderLen != bytesRead))
        return false;

    if (IsMobiPdb(&pdbHeader)) {
        isMobi = true;
    } else if (IsPalmDocPdb(&pdbHeader)) {
        isMobi = false;
    } else {
        // TODO: print type/creator
        lf(" unknown pdb type/creator");
        return false;
    }

    // the values are in big-endian, so convert to host order
    // but only those that we actually access
    SwapU16(pdbHeader.numRecords);
    if (pdbHeader.numRecords < 1)
        return false;

    // allocate one more record as a sentinel to make calculating
    // size of the records easier
    recHeaders = SAZA(PdbRecordHeader, pdbHeader.numRecords + 1);
    if (!recHeaders)
        return false;
    DWORD toRead = kPdbRecordHeaderLen * pdbHeader.numRecords;
    ok = ReadFile(fileHandle, (void*)recHeaders, toRead, &bytesRead, NULL);
    if (!ok || (toRead != bytesRead)) {
        return false;
    }
    for (int i = 0; i < pdbHeader.numRecords; i++) {
        SwapU32(recHeaders[i].offset);
    }
    size_t fileSize = file::GetSize(fileName);
    recHeaders[pdbHeader.numRecords].offset = fileSize;
    // validate offsets
    for (int i = 0; i < pdbHeader.numRecords; i++) {
        if (recHeaders[i + 1].offset < recHeaders[i].offset) {
            lf("invalid offset field");
            return false;
        }
        // technically PDB record size should be less than 64K,
        // but it's not true for mobi files, so we don't validate that
    }

    size_t recLeft;
    char *buf = ReadRecord(0, recLeft);
    if (NULL == buf) {
        lf("failed to read record");
        return false;
    }

    assert(NULL == firstRecData);
    firstRecData = (char*)memdup(buf, recLeft);
    if (!firstRecData)
        return false;
    char *currRecPos = firstRecData;
    PalmDocHeader *palmDocHdr = (PalmDocHeader*)currRecPos;
    currRecPos += sizeof(PalmDocHeader);
    recLeft -= sizeof(PalmDocHeader);

    SwapU16(palmDocHdr->compressionType);
    SwapU32(palmDocHdr->uncompressedDocSize);
    SwapU16(palmDocHdr->recordsCount);
    SwapU16(palmDocHdr->maxRecSize);
    if (!IsValidCompression(palmDocHdr->compressionType)) {
        lf("unknown compression type");
        return false;
    }
    if (isMobi) {
        // TODO: this needs to be surfaced to the client so
        // that we can show the right error message
        if (palmDocHdr->mobi.encrType != ENCRYPTION_NONE) {
            lf("encryption is unsupported");
            return false;
        }
    }

    docRecCount = palmDocHdr->recordsCount;
    docUncompressedSize = palmDocHdr->uncompressedDocSize;
    compressionType = palmDocHdr->compressionType;

    if (0 == recLeft) {
        assert(!isMobi);
        // TODO: calculate imageFirstRec / imagesCount
        return true;
    }
    if (recLeft < 8) // id and hdrLen
        return false;

    MobiHeader *mobiHdr = (MobiHeader*)currRecPos;
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

    if (pdbHeader.numRecords > mobiHdr->imageFirstRec) {
        imageFirstRec = mobiHdr->imageFirstRec;
        if (0 == imageFirstRec) {
            // I don't think this should ever happen but I've seen it
            imagesCount = 0;
        } else
            imagesCount = pdbHeader.numRecords - mobiHdr->imageFirstRec;
    }
    size_t hdrLen = mobiHdr->hdrLen;
    if (hdrLen > recLeft) {
        lf("MobiHeader too big");
        return false;
    }
    currRecPos += hdrLen;
    recLeft -= hdrLen;
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

    if (palmDocHdr->compressionType == COMPRESSION_HUFF) {
        assert(isMobi);
        size_t recSize;
        char *recData = ReadRecord(mobiHdr->huffmanFirstRec, recSize);
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
            recData = ReadRecord(mobiHdr->huffmanFirstRec + 1 + i, recSize);
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

static uint32 GetUpToFour(uint8*& s, size_t& len)
{
    size_t n = 0;
    uint32 v = *s++; len--;
    while ((n < 3) && (len > 0)) {
        v = v << 8;
        v = v | *s++;
        len--; n++;
    }
    return v;
}

static bool IsEofRecord(uint8 *data, size_t dataLen)
{
    return (4 == dataLen) && (EOF_REC == GetUpToFour(data, dataLen));
}

static bool KnownNonImageRec(uint8 *data, size_t dataLen)
{
    uint32 sig = GetUpToFour(data, dataLen);

    if (FLIS_REC == sig) return true;
    if (FCIS_REC == sig) return true;
    if (FDST_REC == sig) return true;
    if (DATP_REC == sig) return true;
    if (SRCS_REC == sig) return true;
    if (VIDE_REC == sig) return true;
    return false;
}

static bool KnownImageFormat(uint8 *data, size_t dataLen)
{
    return NULL != GfxFileExtFromData((char*)data, dataLen);
}

// return false if we should stop loading images (because we
// encountered eof record or ran out of memory)
bool MobiDoc::LoadImage(size_t imageNo)
{
    size_t imageRec = imageFirstRec + imageNo;
    size_t imgDataLen;

    uint8 *imgData = (uint8*)ReadRecord(imageRec, imgDataLen);
    if (!imgData || (0 == imgDataLen))
        return true;
    if (IsEofRecord(imgData, imgDataLen))
        return false;
    if (KnownNonImageRec(imgData, imgDataLen))
        return true;
    if (!KnownImageFormat(imgData, imgDataLen)) {
        lf("Unknown image format");
        return true;
    }
    images[imageNo].data = (char*)memdup(imgData, imgDataLen);
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
    // TODO: remove this before shipping as it probably can happen
    // in malfromed mobi files, but for now we want to know if it happens
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
    Rect size;
    for (size_t i = 0; i < 2; i++) {
        Rect s = BitmapSizeFromData(images[i].data, images[i].len);
        int32 prevSize = size.Width * size.Height;
        int32 currSize = s.Width * s.Height;
        if (currSize > prevSize) {
            coverImage = i;
            size = s;
        }
    }
    if (size.IsEmptyArea())
        return NULL;
    return &images[coverImage];
}

size_t MobiDoc::GetRecordSize(size_t recNo)
{
    size_t size = recHeaders[recNo + 1].offset - recHeaders[recNo].offset;
    return size;
}

// returns NULL if error (failed to allocated)
char *MobiDoc::GetBufForRecordData(size_t size)
{
    if (size <= sizeof(bufStatic))
        return bufStatic;
    if (size <= bufDynamicSize)
        return bufDynamic;
    free(bufDynamic);
    bufDynamic = (char*)malloc(size);
    bufDynamicSize = size;
    return bufDynamic;
}

// read a record and return it's data and size. Return NULL if error
char* MobiDoc::ReadRecord(size_t recNo, size_t& sizeOut)
{
    size_t off = recHeaders[recNo].offset;
    DWORD toRead = GetRecordSize(recNo);
    sizeOut = toRead;
    char *buf = GetBufForRecordData(toRead);
    if (NULL == buf)
        return NULL;
    DWORD bytesRead;
    DWORD res = SetFilePointer(fileHandle, off, NULL, FILE_BEGIN);
    if (INVALID_SET_FILE_POINTER == res)
        return NULL;
    BOOL ok = ReadFile(fileHandle, (void*)buf, toRead, &bytesRead, NULL);
    if (!ok || (toRead != bytesRead))
        return NULL;
    return buf;
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
    char *recData = ReadRecord(recNo, recSize);
    if (NULL == recData)
        return false;
    size_t extraSize = ExtraDataSize((uint8*)recData, recSize, trailersCount, multibyte);
    recSize -= extraSize;
    if (COMPRESSION_NONE == compressionType) {
        strOut.Append(recData, recSize);
        return true;
    }

    if (COMPRESSION_PALM == compressionType) {
        char buf[6000]; // should be enough to decompress any record
        size_t uncompressedSize = PalmdocUncompress((uint8*)recData, recSize, (uint8*)buf, sizeof(buf));
        if (-1 == uncompressedSize) {
            lf("PalmDoc decompression failed");
            return false;
        }
        strOut.Append(buf, uncompressedSize);
        return true;
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
    assert(docUncompressedSize == doc->Size());
    return true;
}

char *MobiDoc::GetBookHtmlData(size_t& lenOut) const
{
    lenOut = doc->Size();
    return doc->Get();
}

MobiDoc *MobiDoc::ParseFile(const TCHAR *fileName)
{
    HANDLE fh = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE)
        return NULL;
    MobiDoc *mb = new MobiDoc();
    mb->fileName = str::Dup(fileName);
    mb->fileHandle = fh;
    if (!mb->ParseHeader())
        goto Error;
    if (!mb->LoadDocument())
        goto Error;
    return mb;
Error:
    delete mb;
    return NULL;
}

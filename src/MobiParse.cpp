/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiParse.h"
#include <time.h>

#include "BitReader.h"
#include "FileUtil.h"
#include "GdiPlusUtil.h"
#include "StrUtil.h"

// Parse mobi format http://wiki.mobileread.com/wiki/MOBI

#define DETAILED_LOGGING 1 // set to 1 for detailed logging during debugging
#if DETAILED_LOGGING
#define l(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define l(fmt, ...) NoOp()
#endif

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
    uint16_t    compressionType;
    uint16_t    reserved1;
    uint32_t    uncompressedDocSize;
    uint16_t    recordsCount;
    uint16_t    maxRecSize;     // usually (always?) 4096
    // if it's palmdoc, we have currPos, if mobi, encrType/reserved2
    union {
        uint32_t    currPos;
        struct {
          uint16_t  encrType;
          uint16_t  reserved2;
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
    uint32_t     hdrLen;   // including 4 id bytes
    uint32_t     type;     // MobiDocType
    uint32_t     textEncoding;
    uint32_t     uniqueId;
    uint32_t     mobiFormatVersion;
    uint32_t     ortographicIdxRec; // -1 if no ortographics index
    uint32_t     inflectionIdxRec;
    uint32_t     namesIdxRec;
    uint32_t     keysIdxRec;
    uint32_t     extraIdx0Rec;
    uint32_t     extraIdx1Rec;
    uint32_t     extraIdx2Rec;
    uint32_t     extraIdx3Rec;
    uint32_t     extraIdx4Rec;
    uint32_t     extraIdx5Rec;
    uint32_t     firstNonBookRec;
    uint32_t     fullNameOffset; // offset in record 0
    uint32_t     fullNameLen;
    // Low byte is main language e.g. 09 = English, 
    // next byte is dialect, 08 = British, 04 = US. 
    // Thus US English is 1033, UK English is 2057
    uint32_t     locale;
    uint32_t     inputDictLanguage;
    uint32_t     outputDictLanguage;
    uint32_t     minRequiredMobiFormatVersion;
    uint32_t     imageFirstRec;
    uint32_t     huffmanFirstRec;
    uint32_t     huffmanRecCount;
    uint32_t     huffmanTableOffset;
    uint32_t     huffmanTableLen;
    uint32_t     exhtFlags;  // bitfield. if bit 6 (0x40) is set => there's an EXTH record
    char        reserved1[32];
    uint32_t     drmOffset; // -1 if no drm info
    uint32_t     drmEntriesCount; // -1 if no drm
    uint32_t     drmSize;
    uint32_t     drmFlags;
    char        reserved2[62];
    // A set of binary flags, some of which indicate extra data at the end of each text block.
    // This only seems to be valid for Mobipocket format version 5 and 6 (and higher?), when
    // the header length is 228 (0xE4) or 232 (0xE8).
    uint16_t    extraDataFlags;
    int32_t     indxRec;
};

STATIC_ASSERT(kMobiHeaderLen == sizeof(MobiHeader), validMobiHeader);

// change big-endian int16 to little-endian (our native format)
static void SwapU16(uint16_t& i)
{
    i = BEtoHs(i);
}

static void SwapU32(uint32_t& i)
{
    i = BEtoHl(i);
}

// Uncompress source data compressed with PalmDoc compression into a buffer.
// Returns size of uncompressed data or -1 on error (if destination buffer too small)
static size_t PalmdocUncompress(uint8_t *src, size_t srcLen, uint8_t *dst, size_t dstLen)
{
    uint8_t *srcEnd = src + srcLen;
    uint8_t *dstEnd = dst + dstLen;
    uint8_t *dstOrig = dst;
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
                uint8_t *dstBack = dst - back;
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
    uint32_t     hdrLen;            // should be 24
    // offset of 256 4-byte elements of cache data, in big endian
    uint32_t     cacheOffset;       // should be 24 as well
    // offset of 64 4-byte elements of base table data, in big endian
    uint32_t     baseTableOffset;   // should be 1024 + 24
    // like cacheOffset except data is in little endian
    uint32_t     cacheOffsetLE;     // should be 64 + 1024 + 24
    // like baseTableOffset except data is in little endian
    uint32_t     baseTableOffsetLE; // should be 1024 + 64 + 1024 + 24
};
STATIC_ASSERT(kHuffHeaderLen == sizeof(HuffHeader), validHuffHeader);

#define kCdicHeaderLen 16
struct CdicHeader
{
    char        id[4];      // "CIDC"
    uint32_t    hdrLen;     // should be 16
    uint32_t    unknown;
    uint32_t    codeLen;
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
    uint8_t *   huffmanData;

    uint32_t *  cacheTable;
    uint32_t *  baseTable;

    size_t      dictsCount;
    uint8_t *   dicts[kCdicsMax];
    uint32_t    dictSize[kCdicsMax];

    uint32_t    code_length;

public:
    HuffDicDecompressor();
    ~HuffDicDecompressor();
    bool SetHuffData(uint8_t *huffData, size_t huffDataLen);
    bool AddCdicData(uint8_t *cdicData, uint32_t cdicDataLen);
    size_t Decompress(uint8_t *src, size_t octets, uint8_t *dst, size_t avail_in);
    //size_t Decompress2(uint8_t *src, size_t srcSize, uint8_t *dst, size_t dstSize);
    bool DecodeOne(uint32_t code, uint8_t *& dst, size_t& dstLeft);
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
bool HuffDicDecompressor::UnpackCacheData(uint32_t *cache)
{
    for (size_t i = 0; i < 256; i++) {
        uint32_t v = cache[i];
        SwapU32(v);
        bool isTerminal =  (v & 0x80) != 0;
        uint8_t valLen = v & 0x1f;
        uint32_t val = v >> 8;
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

uint16_t ReadBeU16(uint8_t *d)
{
    uint16_t v = *((uint16_t*)d);
    SwapU16(v);
    return v;
}

#if 0
uint64_t GetNext64Bits(uint8_t *& s, size_t& srcSize)
{
    uint64_t v = 0;
    for (size_t i = 0; i < 8; i++) {
        v = v << 8;
        if (srcSize > 0) {
            v |= *s++;
            --srcSize;
        }
    }
    return v;
}

bool DecodeCode(uint32_t v, bool& isTerminal, uint32_t& codeLen, uint32_t& maxCode)
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

size_t HuffDicDecompressor::Decompress2(uint8_t *src, size_t srcSize, uint8_t *dst, size_t dstSize)
{
    int bitsLeft = srcSize * 8;
    uint64_t x = GetNext64Bits(src, srcSize);
    int n = 32;
    bool isTerminal;
    uint32_t val, valLen;
    for (;;) {
        if (n <= 0) {
            x = GetNext64Bits(src, srcSize);
            n += 32;
        }
        uint64_t mask = (1ull << 32) - 1;
        uint32_t code = (uint32_t)((x >> n) & mask);

        if (!DecodeCode(code, isTerminal, valLen, val))
            return -1;
    }
}
#endif

bool HuffDicDecompressor::DecodeOne(uint32_t code, uint8_t *& dst, size_t& dstLeft)
{
    uint16_t dict = code >> code_length;
    if ((size_t)dict > dictsCount) {
        l("invalid dict value\n");
        return false;
    }
    code &= ((1 << (code_length)) - 1);
    uint16_t offset = ReadBeU16(dicts[dict] + code * 2);

    if ((uint32_t)offset > dictSize[dict]) {
        l("invalid offset\n");
        return false;
    }
    uint16_t symLen = ReadBeU16(dicts[dict] + offset);
    uint8_t *p = dicts[dict] + offset + 2; 

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
            l("symLen too big\n");
            return false;
        }
        if (symLen > dstLeft) {
            l("not enough space\n");
            return false;
        }
        memcpy(dst, p, symLen);
        dst += symLen;
        dstLeft -= symLen;
    }
    return true;
}

size_t HuffDicDecompressor::Decompress(uint8_t *src, size_t srcSize, uint8_t *dst, size_t dstSize)
{
    uint32_t    bitsConsumed = 0;
    uint32_t    bits = 0;

    BitReader br(src, srcSize);
    size_t      dstLeft = dstSize;

    for (;;) {
        if (bitsConsumed > br.BitsLeft()) {
            l("not enough data\n");
            return -1; 
        }
        br.Eat(bitsConsumed);
        if (0 == br.BitsLeft())
            break;

        bits = br.Peek(32);
        if (br.BitsLeft() < 8 && 0 == bits)
            break;
        uint32_t v = cacheTable[bits >> 24];
        uint32_t codeLen = v & 0x1f;
        if (!codeLen) {
            l("corrupted table, zero code len\n");
            return -1;
        }
        bool isTerminal = (v & 0x80) != 0;

        uint32_t code;
        if (isTerminal) {
            code = (v >> 8) - (bits >> (32 - codeLen));
        } else {
            uint32_t baseVal;
            codeLen -= 1;
            do {
                baseVal = baseTable[codeLen*2];
                code = (bits >> (32 - (codeLen+1)));
                codeLen++;
                if (codeLen > 32) {
                    l("code len > 32 bits\n");
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
        l("compressed data left\n");
    }
    return dstSize - dstLeft;
}

bool HuffDicDecompressor::SetHuffData(uint8_t *huffData, size_t huffDataLen)
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
    huffmanData = (uint8_t*)memdup(huffData, huffDataLen);
    if (!huffmanData)
        return false;
    // we conservatively use the big-endian version of the data, 
    cacheTable = (uint32_t*)(huffmanData + huffHdr->cacheOffset);
    for (size_t i = 0; i < 256; i++) {
        SwapU32(cacheTable[i]);
    }
    baseTable = (uint32_t*)(huffmanData + huffHdr->baseTableOffset);
    for (size_t i = 0; i < 64; i++) {
        SwapU32(baseTable[i]);
    }
    return true;
}

bool HuffDicDecompressor::AddCdicData(uint8_t *cdicData, uint32_t cdicDataLen)
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
    uint32_t size = cdicDataLen - cdicHdr->hdrLen;

    uint32_t maxSize = 1 << code_length;
    if (maxSize >= size)
        return false;
    dicts[dictsCount] = (uint8_t*)memdup(cdicData + cdicHdr->hdrLen, size);
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

MobiParse::MobiParse() : 
    fileName(NULL), fileHandle(0), recHeaders(NULL), firstRecData(NULL), isMobi(false),
    docRecCount(0), compressionType(0), docUncompressedSize(0), doc(NULL),
    multibyte(false), trailersCount(0), imageFirstRec(0), imagesCount(0), validImagesCount(0),
    images(NULL), bufDynamic(NULL), bufDynamicSize(0), huffDic(NULL)
{
}

MobiParse::~MobiParse()
{
    CloseHandle(fileHandle);
    free(fileName);
    free(firstRecData);
    free(recHeaders);
    free(bufDynamic);
    free(images);
    delete huffDic;
    delete doc;
}

bool MobiParse::ParseHeader()
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
        l(" unknown pdb type/creator\n");
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
            l("invalid offset field\n");
            return false;
        }
        // technically PDB record size should be less than 64K,
        // but it's not true for mobi files, so we don't validate that
    }

    size_t recLeft;
    char *buf = ReadRecord(0, recLeft);
    if (NULL == buf) {
        l("failed to read record\n");
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
        l("unknown compression type\n");
        return false;
    }
    if (isMobi) {
        // TODO: this needs to be surfaced to the client so
        // that we can show the right error message
        if (palmDocHdr->mobi.encrType != ENCRYPTION_NONE) {
            l("encryption is unsupported\n");
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
        l("MobiHeader.id is not 'MOBI'\n");
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
        l("MobiHeader too big\n");
        return false;
    }
    currRecPos += hdrLen;
    recLeft -= hdrLen;
    bool hasExtraFlags = (hdrLen >= 228); // TODO: also only if mobiFormatVersion >= 5?

    if (hasExtraFlags) {
        SwapU16(mobiHdr->extraDataFlags);
        uint16_t flags = mobiHdr->extraDataFlags;
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
        if (!huffDic->SetHuffData((uint8_t*)recData, recSize))
            return false;
        for (size_t i = 0; i < cdicsCount; i++) {
            recData = ReadRecord(mobiHdr->huffmanFirstRec + 1 + i, recSize);
            if (!recData)
                return false;
            if (!huffDic->AddCdicData((uint8_t*)recData, recSize))
                return false;
        }
    }

    LoadImages();
    return true;
}

static uint8_t EOF_REC[4]  = { 0xe9, 0x8e, 0x0d, 0x0a };
static uint8_t FLIS_REC[4] = { 'F', 'L', 'I', 'S' };
static uint8_t FCIS_REC[4] = { 'F', 'C', 'I', 'S' };
static uint8_t FDST_REC[4] = { 'F', 'D', 'S', 'T' };
static uint8_t DATP_REC[4] = { 'D', 'A', 'T', 'P' };
static uint8_t SRCS_REC[4] = { 'S', 'R', 'C', 'S' };
// don't know what VIDE record is, probably some sort
// of video
static uint8_t VIDE_REC[4] = { 'V', 'I', 'D', 'E' };

static bool IsEofRecord(uint8_t *data, size_t dataLen)
{
    return (4 == dataLen) && memeq(data, EOF_REC,  4);
}

// TODO: speed up by grabbing 4 bytes as uint32_t and comparing
// with uint32_t constants instead of 4 byte memeq
static bool KnownNonImageRec(uint8_t *data, size_t dataLen)
{
    if (dataLen < 4) return false;
    if (memeq(data, FLIS_REC, 4)) return true;
    if (memeq(data, FCIS_REC, 4)) return true;
    if (memeq(data, FDST_REC, 4)) return true;
    if (memeq(data, DATP_REC, 4)) return true;
    if (memeq(data, SRCS_REC, 4)) return true;
    if (memeq(data, VIDE_REC, 4)) return true;
    return false;
}

static bool KnownImageFormat(uint8_t *data, size_t dataLen)
{
    return NULL != GfxFileExtFromData((char*)data, dataLen);
}

// return false if we should stop loading images (because we 
// encountered eof record or ran out of memory)
bool MobiParse::LoadImage(size_t imageNo)
{
    size_t imageRec = imageFirstRec + imageNo;
    images[imageNo].imgData = 0;
    images[imageNo].imgDataLen = 0;
    size_t imgDataLen;

    uint8_t *imgData = (uint8_t*)ReadRecord(imageRec, imgDataLen);
    if (!imgData)
        return true;
    if (IsEofRecord(imgData, imgDataLen))
        return false;
    if (KnownNonImageRec(imgData, imgDataLen)) {
        imgData[5] = 0;
        return true;
    }
    if (!KnownImageFormat(imgData, imgDataLen)) {
        l("Unknown image format\n");
        return true;
    }
    images[imageNo].imgData = (uint8_t*)memdup(imgData, imgDataLen);
    if (!images[imageNo].imgData)
        return false;
    images[imageNo].imgDataLen = imgDataLen;
    ++validImagesCount;
    return true;
}

void MobiParse::LoadImages()
{
    if (0 == imagesCount)
        return;
    images = SAZA(ImageData, imagesCount);
    for (size_t i = 0; i < imagesCount; i++) {
        if (!LoadImage(i))
            return;
    }
}

size_t MobiParse::GetRecordSize(size_t recNo)
{
    size_t size = recHeaders[recNo + 1].offset - recHeaders[recNo].offset;
    return size;
}

// returns NULL if error (failed to allocated)
char *MobiParse::GetBufForRecordData(size_t size)
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
char* MobiParse::ReadRecord(size_t recNo, size_t& sizeOut)
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
static size_t ExtraDataSize(uint8_t *recData, size_t recLen, size_t trailersCount, bool multibyte)
{
    size_t newLen = recLen;
    for (size_t i = 0; i < trailersCount; i++) {
        assert(newLen > 4);
        uint32_t n = 0;
        for (size_t j = 0; j < 4; j++) {
            uint8_t v = recData[newLen - 4 + j];
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
            uint8_t n = (recData[newLen-1] & 3) + 1;
            assert(newLen >= n);
            newLen -= n;
        }
    }
    return recLen - newLen;
}

// Load a given record of a document into strOut, uncompressing if necessary.
// Returns false if error.
bool MobiParse::LoadDocRecordIntoBuffer(size_t recNo, str::Str<char>& strOut)
{
    size_t recSize;
    char *recData = ReadRecord(recNo, recSize);
    if (NULL == recData)
        return false;
    size_t extraSize = ExtraDataSize((uint8_t*)recData, recSize, trailersCount, multibyte);
    recSize -= extraSize;
    if (COMPRESSION_NONE == compressionType) {
        strOut.Append(recData, recSize);
        return true;
    }

    if (COMPRESSION_PALM == compressionType) {
        char buf[6000]; // should be enough to decompress any record
        size_t uncompressedSize = PalmdocUncompress((uint8_t*)recData, recSize, (uint8_t*)buf, sizeof(buf));
        if (-1 == uncompressedSize) {
            l("PalmDoc decompression failed\n");
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
        size_t uncompressedSize = huffDic->Decompress((uint8_t*)recData, recSize, (uint8_t*)buf, sizeof(buf));
        if (-1 == uncompressedSize) {
            l("HuffDic decompression failed\n");
            return false;
        }
        strOut.Append(buf, uncompressedSize);
        return true;
    }

    assert(0);
    return false;
}

// assumes that ParseHeader() has been called
bool MobiParse::LoadDocument()
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

MobiParse *MobiParse::ParseFile(const TCHAR *fileName)
{
    HANDLE fh = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,  
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE)
        return NULL;
    MobiParse *mb = new MobiParse();
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

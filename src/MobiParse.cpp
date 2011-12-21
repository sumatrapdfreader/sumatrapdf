/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiParse.h"
#include "FileUtil.h"
#include "StrUtil.h"

// Parse mobi format http://wiki.mobileread.com/wiki/MOBI

#define DETAILED_LOGGING 1 // set to 1 for detailed logging during debugging
#if DETAILED_LOGGING
#define l(s) printf(s)
#else
#define l(s) NoOp()
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
    int16_t    compressionType;               // off 0
    int16_t    reserved1;                     // off 2
    int32_t    uncompressedDocSize;           // off 4
    int16_t    recordsCount;                  // off 8
    int16_t    maxUncompressedRecSize;        // off 10, usually 4096
    union {
        int32_t    currPos;                   // off 12, if it's palmdoc
        struct {  // if it's mobi
          int16_t  encrType;                  // off 12
          int16_t  reserved2;                 // off 14
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
    int32_t     hdrLen;   // including 4 id bytes
    int32_t     type;     // MobiDocType
    int32_t     textEncoding;
    int32_t     uniqueId;
    int32_t     mobiFormatVersion;
    int32_t     ortographicIdxRec; // -1 if no ortographics index
    int32_t     inflectionIdxRec;
    int32_t     namesIdxRec;
    int32_t     keysIdxRec;
    int32_t     extraIdx0Rec;
    int32_t     extraIdx1Rec;
    int32_t     extraIdx2Rec;
    int32_t     extraIdx3Rec;
    int32_t     extraIdx4Rec;
    int32_t     extraIdx5Rec;
    int32_t     firstNonBookRec;
    int32_t     fullNameOffset; // offset in record 0
    int32_t     fullNameLen;
    // Low byte is main language e.g. 09 = English, 
    // next byte is dialect, 08 = British, 04 = US. 
    // Thus US English is 1033, UK English is 2057
    int32_t     locale;
    int32_t     inputDictLanguage;
    int32_t     outputDictLanguage;
    int32_t     minRequiredMobiFormatVersion;
    int32_t     firstImageRec;
    int32_t     firstHuffmanRec;
    int32_t     huffmanRecCount;
    int32_t     huffmanTableOffset;
    int32_t     huffmanTableLen;
    int32_t     exhtFlags;  // bitfield. if bit 6 (0x40) is set => there's an EXTH record
    char        reserved1[32];
    int32_t     drmOffset; // -1 if no drm info
    int32_t     drmEntriesCount; // -1 if no drm
    int32_t     drmSize;
    int32_t     drmFlags;
    char        reserved2[62];
    // A set of binary flags, some of which indicate extra data at the end of each text block.
    // This only seems to be valid for Mobipocket format version 5 and 6 (and higher?), when
    // the header length is 228 (0xE4) or 232 (0xE8).
    uint16_t    extraDataFlags;
    int32_t     indxRec;
};

STATIC_ASSERT(kMobiHeaderLen == sizeof(MobiHeader), validMobiHeader);

// change big-endian int16 to little-endian (our native format)
static void SwapI16(int16_t& i)
{
    i = BEtoHs(i);
}

static void SwapU16(uint16_t& i)
{
    i = BEtoHs(i);
}

// change big-endian int32 to little-endian (our native format)
static void SwapI32(int32_t& i)
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

class HuffDicDecompressor 
{
public:
    HuffDicDecompressor();
    ~HuffDicDecompressor();
    bool SetHuffData(uint8_t *huffData, size_t huffDataLen);
    bool AddCdicData(uint8_t *cdicData, size_t cdicDataLen);
};

HuffDicDecompressor::HuffDicDecompressor()
{
}

HuffDicDecompressor::~HuffDicDecompressor()
{
}

bool HuffDicDecompressor::SetHuffData(uint8_t *huffData, size_t huffDataLen)
{
    return false;
}

bool HuffDicDecompressor::AddCdicData(uint8_t *cdicData, size_t cdicDataLen)
{
    return false;
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
    fileName(NULL), fileHandle(0), recHeaders(NULL), isMobi(false),
    docRecCount(0), compressionType(0), docUncompressedSize(0),
    multibyte(false), trailersCount(0)
{
}

MobiParse::~MobiParse()
{
    CloseHandle(fileHandle);
    free(fileName);
    free(recHeaders);
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
    SwapI16(pdbHeader.numRecords);
    if (pdbHeader.numRecords < 1)
        return false;

    // allocate one more record as a sentinel to make calculating
    // size of the records easier
    recHeaders = SAZA(PdbRecordHeader, pdbHeader.numRecords + 1);
    DWORD toRead = kPdbRecordHeaderLen * pdbHeader.numRecords;
    ok = ReadFile(fileHandle, (void*)recHeaders, toRead, &bytesRead, NULL);
    if (!ok || (toRead != bytesRead)) {
        return false;
    }
    for (int i = 0; i < pdbHeader.numRecords; i++) {
        SwapI32(recHeaders[i].offset);
    }
    size_t fileSize = file::GetSize(fileName);
    recHeaders[pdbHeader.numRecords].offset = fileSize;
    // validate offset field
    for (int i = 0; i < pdbHeader.numRecords; i++) {
        if (recHeaders[i + 1].offset < recHeaders[i].offset) {
            l("invalid offset field\n");
            return false;
        }
        // technically PDB record size should be less than 64K,
        // but it's not true for mobi files, so we don't validate that
    }

    if (!ReadRecord(0)) {
        l("failed to read record\n");
        return false;
    }

    char *currRecPos = recordBuf;
    size_t recLeft = GetRecordSize(0);
    PalmDocHeader *palmDocHdr = (PalmDocHeader*)currRecPos;
    currRecPos += sizeof(PalmDocHeader);
    recLeft -= sizeof(PalmDocHeader);

    SwapI16(palmDocHdr->compressionType);
    SwapI32(palmDocHdr->uncompressedDocSize);
    SwapI16(palmDocHdr->recordsCount);
    //SwapI16(palmDocHdr->maxUncompressedRecSize);
    if (!IsValidCompression(palmDocHdr->compressionType)) {
        l("unknown compression type\n");
        return false;
    }
    if (palmDocHdr->compressionType == COMPRESSION_HUFF) {
        l("huff compression not supported yet\n");
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
        return true;
    }
    if (recLeft < 8) // id and hdrLen
        return false;
    MobiHeader *mobiHdr = (MobiHeader*)currRecPos;
    if (!str::EqN("MOBI", mobiHdr->id, 4)) {
        l("MobiHeader.id is not 'MOBI'\n");
        return false;
    }
    SwapI32(mobiHdr->hdrLen);
    SwapI32(mobiHdr->type);
    SwapI32(mobiHdr->textEncoding);
    SwapI32(mobiHdr->mobiFormatVersion);
    SwapI32(mobiHdr->firstNonBookRec);
    SwapI32(mobiHdr->fullNameOffset);
    SwapI32(mobiHdr->fullNameLen);
    SwapI32(mobiHdr->locale);
    SwapI32(mobiHdr->minRequiredMobiFormatVersion);
    SwapI32(mobiHdr->firstImageRec);
    SwapI32(mobiHdr->firstHuffmanRec);
    SwapI32(mobiHdr->huffmanRecCount);
    SwapI32(mobiHdr->huffmanTableOffset);
    SwapI32(mobiHdr->huffmanTableLen);
    SwapI32(mobiHdr->exhtFlags);

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

    return true;
}

size_t MobiParse::GetRecordSize(size_t recNo)
{
    size_t size = recHeaders[recNo + 1].offset - recHeaders[recNo].offset;
    return size;
}

// read a record into recordBuf. Return false if error
bool MobiParse::ReadRecord(size_t recNo)
{
    size_t off = recHeaders[recNo].offset;
    DWORD toRead = GetRecordSize(recNo);
    // TODO: if toRead > sizeof(recordBuf), allocate buffer in memory
    assert(toRead <= sizeof(recordBuf));
    DWORD bytesRead;
    DWORD res = SetFilePointer(fileHandle, off, NULL, FILE_BEGIN);
    if (INVALID_SET_FILE_POINTER == res)
        return false;
    BOOL ok = ReadFile(fileHandle, (void*)recordBuf, toRead, &bytesRead, NULL);
    if (!ok || (toRead != bytesRead))
        return false;
    return true;
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
    if (!ReadRecord(recNo))
        return false;
    size_t recSize = GetRecordSize(recNo);
    size_t extraSize = ExtraDataSize((uint8_t*)recordBuf, recSize, trailersCount, multibyte);
    recSize -= extraSize;
    if (COMPRESSION_NONE == compressionType) {
        strOut.Append(recordBuf, recSize);
        return true;
    }

    if (COMPRESSION_PALM == compressionType) {
        char buf[6000]; // should be enough to decompress any record
        size_t uncompressedSize = PalmdocUncompress((uint8_t*)recordBuf, recSize, (uint8_t*)buf, sizeof(buf));
        if (-1 == uncompressedSize) {
            l("PalmDoc decompression failed\n");
            return false;
        }
        strOut.Append(buf, uncompressedSize);
        return true;
    }

    if (COMPRESSION_HUFF == compressionType) {
        // TODO: implement me
        return false;
    }

    assert(0);
    return false;
}

// assumes that ParseHeader() has been called
bool MobiParse::LoadDocument()
{
    assert(docUncompressedSize > 0);
    assert(0 == doc.Size());
    for (size_t i = 1; i <= docRecCount; i++) {
        if (!LoadDocRecordIntoBuffer(i, doc))
            return false;
    }
    assert(docUncompressedSize == doc.Size());
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

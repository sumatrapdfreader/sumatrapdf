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

#define kMobiFirstRecordLen 16

#define COMPRESSION_NONE 1
#define COMPRESSION_PALM 2
#define COMPRESSION_HUFF 17480

#define ENCRYPTION_NONE 0
#define ENCRYPTION_OLD  1
#define ENCRYPTION_NEW  2

struct MobiFirstRecord
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

STATIC_ASSERT(kMobiFirstRecordLen == sizeof(MobiFirstRecord), validMobiFirstRecord);

// change big-endian int16 to little-endian (our native format)
static void SwapI16(int16_t& i)
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
    size_t dstLeft = dstEnd - dst;
    while (src < srcEnd) {
        if (0 == dstLeft)
            return -1;

        unsigned c = *src++;

        if ((c >= 1) && (c <= 8)) {
            if (dstLeft < c)
                return -1;
            while (c > 0) {
                *dst++ = *src++;
                --c;
            }
        } else if (c < 128) {
            *dst++ = c;
        } else if (c >= 192) {
            if (dstLeft < 2)
                return -1;
            *dst++ = ' ';
            *dst++ = c ^ 0x80;
        } else {
            assert((c >= 128) && (c < 192));
            if (src == srcEnd)
                return -1;
            c = (c << 8) | *src++;
            size_t disp = (c & 0x3FFF) >> 3;
            size_t n = (c & ((1 << 3) - 1)) + 3;
            if (dstLeft < n)
                return -1;
            while (n > 0) {
                *dst = *(dst-disp);
                dst++; --n;
            }
        }
        // must be at the end so that it's valid after we exit loop
        dstLeft = dstEnd - dst;
    }

    // zero-terminate to make inspecting in the debugger easier
    if (dst < dstEnd)
        *dst = 0;

    return dstLen - dstLeft;
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
    docRecCount(0), docUncompressedSize(0), compressionType(0), doc(NULL)
{
}

MobiParse::~MobiParse()
{
    CloseHandle(fileHandle);
    free(fileName);
    free(recHeaders);
    free(doc);
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
        if (recHeaders[i + 1].offset <= recHeaders[i].offset) {
            l("invalid offset field\n");
            return false;
        }
        if (GetRecordSize(i) > kMaxRecordSize) {
            l("invalid size\n");
            return false;
        }
    }

    if (!ReadRecord(0)) {
        l("failed to read record\n");
        return false;
    }

    MobiFirstRecord *firstRec = (MobiFirstRecord*)recordBuf;
    SwapI16(firstRec->compressionType);
    SwapI32(firstRec->uncompressedDocSize);
    SwapI16(firstRec->recordsCount);
    //SwapI16(firstRec->maxUncompressedRecSize);
    if (!IsValidCompression(firstRec->compressionType)) {
        l("unknown compression type\n");
        return false;
    }
    if (firstRec->compressionType == COMPRESSION_HUFF) {
        l("huff compression not supported yet\n");
        return false;
    }
    if (isMobi) {
        // TODO: this needs to be surfaced to the client so
        // that we can show the right error message
        if (firstRec->mobi.encrType != ENCRYPTION_NONE) {
            l("encryption is unsupported\n");
            return false;
        }
    }

    docRecCount = firstRec->recordsCount;
    docUncompressedSize = firstRec->uncompressedDocSize;
    compressionType = firstRec->compressionType;

    // TODO: parse mobi-specific parts of the first record
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
    DWORD bytesRead;
    DWORD res = SetFilePointer(fileHandle, off, NULL, FILE_BEGIN);
    if (INVALID_SET_FILE_POINTER == res)
        return false;
    BOOL ok = ReadFile(fileHandle, (void*)recordBuf, toRead, &bytesRead, NULL);
    if (!ok || (toRead != bytesRead))
        return false;
    return true;
}

// Load a given record of a document into a buffer, uncompressing if necessary.
// bufLeft is the amount of space left if the buffer which can be bigger than
// the size of uncompressed record.
// Updates buf position and bufLeft.
// Returns false if error.
bool MobiParse::LoadDocRecordIntoBuffer(size_t recNo, char*& buf, size_t& bufLeft)
{
    if (!ReadRecord(recNo))
        return false;
    size_t recSize = GetRecordSize(recNo);
    if (recSize > bufLeft) {
        l("record too big to fit in the buffer\n");
        return false;
    }

    if (COMPRESSION_NONE == compressionType) {
        memcpy(buf, recordBuf, recSize);
        bufLeft -= recSize;
        buf += recSize;
        return true;
    }

    if (COMPRESSION_PALM == compressionType) {
        size_t uncompressedSize = PalmdocUncompress((uint8_t*)recordBuf, recSize, (uint8_t*)buf, bufLeft);
        if (-1 == uncompressedSize) {
            l("PalmDoc decompression failed\n");
            return false;
        }
        assert(uncompressedSize <= bufLeft);
        bufLeft -= uncompressedSize;
        buf += uncompressedSize;
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
    assert(NULL == doc);

    char *doc = (char*)malloc(docUncompressedSize + 1); // +1 for zero termination, just in case
    if (!doc) {
        // a potentially big allocation so might fail
        return false;
    }
    doc[docUncompressedSize] = 0;

    char *buf = doc;
    size_t bufLeft = docUncompressedSize;
    for (size_t i = 1; i <= docRecCount; i++) {
        if (!LoadDocRecordIntoBuffer(i, buf, bufLeft))
            return false;
    }
    return bufLeft == 0;
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

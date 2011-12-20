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
    int16_t    recordSize;                    // off 10
    union {
        int32_t    currPos;                   // off 12, if it's palmdoc
        struct mobi {  // if it's mobi
          int16_t  encrType;                  // off 12
          int16_t  reserved2;                 // off 14
        };
    };
};

STATIC_ASSERT(kMobiFirstRecordLen == sizeof(MobiFirstRecord), validMobiFirstRecord);

// change big-endian int16 to little-endian (our native format)
static void SwapI16(int16_t& i)
{
    i = BIG_ENDIAN_16(i);
}

// change big-endian int32 to little-endian (our native format)
static void SwapI32(int32_t& i)
{
    i = BIG_ENDIAN_32(i);
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

MobiParse::MobiParse() : fileName(NULL), fileHandle(0), recHeaders(NULL), isMobi(false)
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
    SwapI16(firstRec->recordSize);
    if (!IsValidCompression(firstRec->compressionType)) {
        l("unknown compression type\n");
        return false;
    }

    // TODO: write more
    return false;
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

MobiParse *MobiParse::ParseFile(const TCHAR *fileName)
{
    HANDLE fh = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,  
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE)
        return NULL;
    MobiParse *mb = new MobiParse();
    mb->fileName = str::Dup(fileName);
    mb->fileHandle = fh;
    if (!mb->ParseHeader()) {
        delete mb;
        return NULL;
    }
    return mb;
}

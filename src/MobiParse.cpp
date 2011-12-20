/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiParse.h"
#include "FileUtil.h"
#include "StrUtil.h"

#define DETAILED_LOGGING 1 // set to 1 for detailed logging during debugging
#if DETAILED_LOGGING
#define l(s) printf(s)
#else
#define l(s) NoOp()
#endif

#define PALMDOC_TYPE_CREATOR   "TEXtREAd"
#define MOBI_TYPE_CREATOR      "BOOKMOBI"

// as used in pdf_fontfile.c, DjVuEngine.cpp and ImagesEngine.cpp:
// TODO: move to BaseUtil.h?
// for converting between big- and little-endian values
#define SWAPWORD(x)    MAKEWORD(HIBYTE(x), LOBYTE(x))
#define SWAPLONG(x)    MAKELONG(SWAPWORD(HIWORD(x)), SWAPWORD(LOWORD(x)))

// change big-endian int16 to little-endian
static void SwapI16(int16_t& i)
{
    i = SWAPWORD(i);
}

// change big-endian int32 to little-endian
static void SwapI32(int32_t& i)
{
    i = SWAPLONG(i);
}

static bool IsMobiPdb(PdbHeader *pdbHdr)
{
    return str::EqN(pdbHdr->type, MOBI_TYPE_CREATOR, 8);
}

MobiParse::MobiParse() : fileName(NULL), fileHandle(0), recHeaders(NULL)
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

    if (!IsMobiPdb(&pdbHeader)) {
        // TODO: print type/creator
        l(" unknown pdb type/creator\n");
        return false;
    }
    // TODO: also allow palmdoc?

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
        if (GetRecordSize(i) > 64 * 1024) {
            l("invalid size\n");
            return false;
        }
    }
    // TODO: write more
    return false;
}

size_t MobiParse::GetRecordSize(size_t recNo)
{
    return recHeaders[recNo + 1].offset - recHeaders[recNo].offset;
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

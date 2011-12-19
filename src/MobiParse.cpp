/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiParse.h"

#include "BaseUtil.h"
#include "StrUtil.h"

#define DETAILED_LOGGING 1 // set to 1 for detailed logging during debugging
#if DETAILED_LOGGING
#define l(s) printf(s)
#else
#define l(s) NoOp()
#endif

#define PALMDOC_TYPE_CREATOR   "TEXtREAd"
#define MOBI_TYPE_CREATOR      "BOOKMOBI"

#pragma comment(lib, "Ws2_32.lib") // for ntohs(), ntohl()

// change big-endian int16 to host (which we assume is little endian)
static void SwapI16(int16_t& i)
{
    u_short *p = (u_short*)&i;
    u_short tmp = ntohs(*p);
    *p = tmp;
}

static void SwapI32(int32_t& i)
{
    u_long *p = (u_long*)&i;
    u_long tmp = ntohl(*p);
    *p = tmp;
}

static bool IsMobiPdb(PdbHeader *pdbHdr)
{
    return str::EqN(pdbHdr->type, MOBI_TYPE_CREATOR, 8);
}

MobiParse::MobiParse() : fileName(NULL), fileHandle(0)
{
}

MobiParse::~MobiParse()
{
    CloseHandle(fileHandle);
    free(fileName);
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

    // TODO: write more
    return false;
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


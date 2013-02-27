/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PalmDbReader_h
#define PalmDbReader_h

// http://en.wikipedia.org/wiki/PDB_(Palm_OS)
#define kPdbHeaderLen 78

class PdbReader {
    ScopedMem<char> data;
    size_t          dataSize;
    // offset of each pdb record within the file + a sentinel
    // value equal to file size to simplify use
    Vec<uint32_t>   recOffsets;
    // cache so that we can compare with str::Eq
    char            dbType[9];

    bool ParseHeader();

public:
    PdbReader(const WCHAR *filePath);

    const char *GetDbType();
    size_t GetRecordCount();
    const char *GetRecord(size_t recNo, size_t *sizeOut);
};

#endif

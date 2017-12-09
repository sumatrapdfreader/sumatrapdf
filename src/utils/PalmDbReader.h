/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// http://en.wikipedia.org/wiki/PDB_(Palm_OS)
#define kPdbHeaderLen 78

class PdbReader {
    // content of pdb file
    OwnedData data;

    // offset of each pdb record within the file + a sentinel
    // value equal to file size to simplify use
    std::vector<uint32_t> recOffsets;

    // cache so that we can compare with str::Eq
    char dbType[9];

    bool ParseHeader();

  public:
    bool Parse(OwnedData data);

    const char* GetDbType();
    size_t GetRecordCount();
    std::string_view GetRecord(size_t recNo);

    static PdbReader* CreateFromData(OwnedData data);
#if OS_WIN
    static PdbReader* CreateFromFile(const WCHAR* filePath);
    static PdbReader* CreateFromStream(IStream* stream);
#endif
};

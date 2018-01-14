/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// http://en.wikipedia.org/wiki/PDB_(Palm_OS)
// http://wiki.mobileread.com/wiki/PDB
struct PdbHeader {
    /* 31 chars + 1 null terminator */
    char name[32];
    uint16_t attributes;
    uint16_t version;
    uint32_t createTime;
    uint32_t modifyTime;
    uint32_t backupTime;
    uint32_t modificationNumber;
    uint32_t appInfoID;
    uint32_t sortInfoID;
    // 8 bytes in the file +1 for zero termination
    char typeCreator[8 + 1];
    uint32_t idSeed;
    uint32_t nextRecordList;
    uint16_t numRecords;
};

struct PdbRecordHeader {
    uint32_t offset;
    uint8_t flags; // deleted, dirty, busy, secret, category
    char uniqueID[3];
};

class PdbReader {
    // content of pdb file
    OwnedData data;

    // offset of each pdb record within the file + a sentinel
    // value equal to file size to simplify use
    std::vector<PdbRecordHeader> recInfos;

    bool ParseHeader();

  public:
    PdbHeader hdr;

    bool Parse(OwnedData data);

    const char* GetDbType();
    size_t GetRecordCount();
    std::string_view GetRecord(size_t recNo);

    static PdbReader* CreateFromData(OwnedData data);
    static PdbReader* CreateFromFile(const char* filePath);

#if OS_WIN
    static PdbReader* CreateFromFile(const WCHAR* filePath);
    static PdbReader* CreateFromStream(IStream* stream);
#endif
};

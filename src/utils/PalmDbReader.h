/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// values for typeCreator
#define MOBI_TYPE_CREATOR "BOOKMOBI"
#define PALMDOC_TYPE_CREATOR "TEXtREAd"
#define TEALDOC_TYPE_CREATOR "TEXtTlDc"
#define PLUCKER_TYPE_CREATOR "DataPlkr"

// http://en.wikipedia.org/wiki/PDB_(Palm_OS)
// http://wiki.mobileread.com/wiki/PDB
struct PdbHeader {
    /* 31 chars + 1 null terminator */
    char name[32] = {0};
    u16 attributes = 0;
    u16 version = 0;
    u32 createTime = 0;
    u32 modifyTime = 0;
    u32 backupTime = 0;
    u32 modificationNumber = 0;
    u32 appInfoID = 0;
    u32 sortInfoID = 0;
    // 8 bytes in the file +1 for zero termination
    char typeCreator[8 + 1] = {0};
    u32 idSeed = 0;
    u32 nextRecordList = 0;
    u16 numRecords = 0;
};

struct PdbRecordHeader {
    u32 offset = 0;
    u8 flags = 0; // deleted, dirty, busy, secret, category
    char uniqueID[3] = {0};
};

class PdbReader {
    // content of pdb file
    const u8* data = nullptr;
    size_t dataSize = 0;

    // offset of each pdb record within the file + a sentinel
    // value equal to file size to simplify use
    Vec<PdbRecordHeader> recInfos;

    bool ParseHeader();

  public:
    PdbHeader hdr;

    PdbReader() = default;
    ~PdbReader();

    bool Parse(std::span<u8>);

    const char* GetDbType();
    size_t GetRecordCount();
    std::span<u8> GetRecord(size_t recNo);

    static PdbReader* CreateFromData(std::span<u8>);
    static PdbReader* CreateFromFile(const char* path);

#if OS_WIN
    static PdbReader* CreateFromFile(const WCHAR* path);
    static PdbReader* CreateFromStream(IStream* stream);
#endif
};

// stuff for mobi format
enum class PdbDocType { Unknown, Mobipocket, PalmDoc, TealDoc, Plucker };
PdbDocType GetPdbDocType(const char* typeCreator);

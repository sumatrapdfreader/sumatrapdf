/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct PdbReader : NonCopyable {
    ~PdbReader();

    Str GetDbType();
    int GetRecordCount();
    Str GetRecord(int recNo);

    static PdbReader* CreateFromData(Str);
    static PdbReader* CreateFromFile(Str path);

  private:
    PdbReader() = default;
    bool Parse(Str);

    const u8* data = nullptr;
    Vec<int> recordOffsets;
};

// stuff for mobi format
enum class PdbDocType {
    Unknown,
    Mobipocket,
    PalmDoc,
    TealDoc,
    Plucker
};
PdbDocType GetPdbDocType(Str typeCreator);

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct ChmFile {
    struct chmFile* chmHandle = nullptr;

    // Data parsed from /#WINDOWS, /#STRINGS, /#SYSTEM files inside CHM file
    AutoFreeStr title;
    AutoFreeStr tocPath;
    AutoFreeStr indexPath;
    AutoFreeStr homePath;
    AutoFreeStr creator;
    AutoFree data;
    uint codepage = 0;

    void ParseWindowsData();
    bool ParseSystemData();
    bool ParseTocOrIndex(EbookTocVisitor* visitor, const char* path, bool isIndex) const;
    void FixPathCodepage(AutoFreeStr& path, uint& fileCP);

    bool Load(const char* fileName);

    ChmFile() = default;
    ~ChmFile();

    bool HasData(const char* fileName) const;
    ByteSlice GetData(const char* fileName) const;
    char* ResolveTopicID(unsigned int id) const;

    TempStr SmartToUtf8Temp(const char* text, uint overrideCP = 0) const;

    TempStr GetPropertyTemp(const char* name) const;
    const char* GetHomePath() const;
    void GetAllPaths(StrVec*) const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor) const;
    bool HasIndex() const;
    bool ParseIndex(EbookTocVisitor* visitor) const;

    static bool IsSupportedFileType(Kind);
    static ChmFile* CreateFromFile(const char* path);
};

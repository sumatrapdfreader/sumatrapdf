/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct ChmFile {
    struct chmFile* chmHandle = nullptr;

    // Data parsed from /#WINDOWS, /#STRINGS, /#SYSTEM files inside CHM file
    AutoFree title;
    AutoFree tocPath;
    AutoFree indexPath;
    AutoFree homePath;
    AutoFree creator;
    AutoFree data;
    uint codepage = 0;

    void ParseWindowsData();
    bool ParseSystemData();
    bool ParseTocOrIndex(EbookTocVisitor* visitor, const char* path, bool isIndex) const;
    void FixPathCodepage(AutoFree& path, uint& fileCP);

    bool Load(const char* fileName);

    ChmFile() = default;
    ~ChmFile();

    bool HasData(const char* fileName) const;
    ByteSlice GetData(const char* fileName) const;
    char* ResolveTopicID(unsigned int id) const;

    char* ToUtf8(const u8* text, uint overrideCP = 0) const;
    WCHAR* ToStr(const char* text) const;

    WCHAR* GetProperty(DocumentProperty prop) const;
    const char* GetHomePath() const;
    Vec<char*>* GetAllPaths() const;

    [[nodiscard]] bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor) const;
    [[nodiscard]] bool HasIndex() const;
    bool ParseIndex(EbookTocVisitor* visitor) const;

    static bool IsSupportedFileType(Kind);
    static ChmFile* CreateFromFile(const WCHAR* path);
};

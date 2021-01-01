/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct ChmDoc {
    struct chmFile* chmHandle = nullptr;

    // Data parsed from /#WINDOWS, /#STRINGS, /#SYSTEM files inside CHM file
    AutoFree title;
    AutoFree tocPath;
    AutoFree indexPath;
    AutoFree homePath;
    AutoFree creator;
    uint codepage = 0;

    void ParseWindowsData();
    bool ParseSystemData();
    bool ParseTocOrIndex(EbookTocVisitor* visitor, const char* path, bool isIndex);
    void FixPathCodepage(AutoFree& path, uint& fileCP);

    bool Load(const WCHAR* fileName);

    ChmDoc() = default;
    ~ChmDoc();

    bool HasData(const char* fileName);
    std::span<u8> GetData(const char* fileName);
    char* ResolveTopicID(unsigned int id);

    char* ToUtf8(const u8* text, uint overrideCP = 0);
    WCHAR* ToStr(const char* text);

    WCHAR* GetProperty(DocumentProperty prop);
    const char* GetHomePath();
    Vec<char*>* GetAllPaths();

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);
    bool HasIndex() const;
    bool ParseIndex(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind);
    static ChmDoc* CreateFromFile(const WCHAR* path);
};

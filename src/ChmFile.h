/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Strip a UTF-8 BOM if present; otherwise convert from `codepage` to UTF-8
// (unless already UTF-8). Returns a TempStr owned by the temp allocator.
TempStr SmartToUtf8Temp(Str s, uint codepage);

enum class DocProp : u8;
enum class FileType : u8;

struct chm_ctx;
struct chm_entry;

struct ChmFile {
    chm_ctx* chmCtx = nullptr;
    // entries and their paths are owned by chmCtx (freed by chm_ctx_free)
    chm_entry** entries = nullptr;
    int nEntries = 0;

    // Data parsed from /#WINDOWS, /#STRINGS, /#SYSTEM files inside CHM file
    Str title;
    Str tocPath;
    Str indexPath;
    Str homePath;
    Str creator;
    Str data;
    uint codepage = 0;

    void ParseWindowsData();
    bool ParseSystemData();
    bool ParseTocOrIndex(EbookTocVisitor* visitor, Str path, bool isIndex) const;
    void FixPathCodepage(Str& path, uint& fileCP);

    bool Load(Str fileName);

    ChmFile() = default;
    ~ChmFile();

    bool HasData(Str fileName) const;
    TempStr GetDataTemp(Str fileName) const;
    TempStr ResolveTopicID(unsigned int id) const;

    TempStr GetPropertyTemp(DocProp prop) const;
    TempStr GetHomePath() const;
    void GetAllPaths(StrVec*) const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor) const;
    bool HasIndex() const;
    bool ParseIndex(EbookTocVisitor* visitor) const;

    static bool IsSupportedFileType(FileType);
    static ChmFile* CreateFromFile(Str path);
};

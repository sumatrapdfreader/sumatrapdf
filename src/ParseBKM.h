/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct VbkmForFile {
    // path of the original file
    AutoFree filePath;

    TocTree* toc = nullptr;
    int nPages = 0;

    EngineBase* engine = nullptr;

    VbkmForFile() = default;
    ~VbkmForFile();
};

// represents .vbkm file
struct VbkmFile {
    char* fileName = nullptr;
    char* path = nullptr;
    EngineBase* engine = nullptr;

    ~VbkmFile();
};

struct ParsedVbkm {
    AutoFree fileContent;

    Vec<VbkmFile*> files;
    ~ParsedVbkm();
};

bool ParseBookmarksFile(std::string_view path, Vec<VbkmForFile*>& bkmsOut);
bool ExportBookmarksToFile(const Vec<VbkmForFile*>&, const char* path);

bool LoadAlterenativeBookmarks(std::string_view baseFileName, Vec<VbkmForFile*>& bkmOut);
ParsedVbkm* ParseVbkmFile(std::string_view d);

bool ParseVbkmFile(std::string_view d, Vec<VbkmForFile*>& bkmOut);

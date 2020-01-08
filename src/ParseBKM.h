/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// represents bookmarks for a single file
struct VbkmForFile {
    // path of the original file
    AutoFree filePath;
    // TODO: serialize nPages after "file:"
    int nPages = 0;

    TocTree* toc = nullptr;

    EngineBase* engine = nullptr;

    VbkmForFile() = default;
    ~VbkmForFile();
};

// represents a .vbkm file which represents one or more
// physical files
struct VbkmFile {
    AutoFree fileContent;
    AutoFree name;
    Vec<VbkmForFile*> vbkms;

    VbkmFile() = default;
    ~VbkmFile();
};

bool ExportBookmarksToFile(const Vec<VbkmForFile*>&, const char* name, const char* path);
bool ExportBookmarksToFile2(const Vec<VbkmForFile*>& bookmarks, TocTree* tocTree, const char* name, const char* bkmPath);

bool LoadAlterenativeBookmarks(std::string_view baseFileName, VbkmFile& vbkm);

bool ParseVbkmFile(std::string_view d, VbkmFile& vbkm);

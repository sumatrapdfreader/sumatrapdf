/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TODO: temporary, rename back to TocTree and TocItem
typedef TocTree BkmTree;
typedef TocItem BkmItem;

// represents a .vbkm file which represents one or more
// physical files
struct VbkmFile {
    AutoFree fileContent;
    char* name;
    BkmTree* tree;

    VbkmFile() = default;
    ~VbkmFile();
};

bool ExportBookmarksToFile(BkmTree*, const char* name, const char* path);

bool LoadAlterenativeBookmarks(std::string_view baseFileName, VbkmFile& vbkm);

bool ParseVbkmFile(std::string_view d, VbkmFile& vbkm);
bool LoadVbkmFile(const char* filePath, VbkmFile& vbkm);

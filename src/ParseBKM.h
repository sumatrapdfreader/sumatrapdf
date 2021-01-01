/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// represents a .vbkm file which represents one or more
// physical files
struct VbkmFile {
    AutoFree fileContent;
    AutoFreeStr name;
    TocTree* tree = nullptr;

    VbkmFile() = default;
    ~VbkmFile();
};

bool ExportBookmarksToFile(TocTree*, const char* name, const char* path);

bool LoadAlterenativeBookmarks(std::string_view baseFileName, VbkmFile& vbkm);

bool ParseVbkmFile(std::string_view sv, VbkmFile& vbkm);
bool LoadVbkmFile(const char* filePath, VbkmFile& vbkm);

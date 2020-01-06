/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Bookmarks {
    // path of the .vbkm file
    AutoFree filePath;

    TocTree* toc = nullptr;
    int nPages = 0;

    Bookmarks() = default;
    ~Bookmarks();
};

struct VbkmFile {
    char* fileName = nullptr;
    char* path = nullptr;
    EngineBase* engine = nullptr;

    ~VbkmFile();
};

// represents .vbkm file
struct ParsedVbkm {
    AutoFree fileContent;

    Vec<VbkmFile*> files;

    ~ParsedVbkm();
};

bool ParseBookmarksFile(std::string_view path, Vec<Bookmarks*>& bkmsOut);
bool ExportBookmarksToFile(const Vec<Bookmarks*>&, const char* path);

bool LoadAlterenativeBookmarks(std::string_view baseFileName, Vec<Bookmarks*>& bkmOut);
ParsedVbkm* ParseVbkmFile(std::string_view d);

bool ParseVbkmFile(std::string_view d, Vec<Bookmarks*>& bkmOut);

/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Bookmarks {
    AutoFree filePath;
    DocTocTree* toc = nullptr;

    Bookmarks() = default;
    ~Bookmarks();
};

bool ParseBookmarksFile(std::string_view path, Vec<Bookmarks*>* bkms);
bool ExportBookmarksToFile(const Vec<Bookmarks*>&, const char* path);

Vec<Bookmarks*>* LoadAlterenativeBookmarks(std::string_view baseFileName);

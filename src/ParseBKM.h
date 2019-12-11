/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Bookmarks {
    char* filePath = nullptr;
    DocTocTree* toc = nullptr;

    Bookmarks() = default;
    ~Bookmarks();
};

void SerializeBookmarksRec(DocTocItem* node, int level, str::Str& s);
bool ParseBookmarksFile(std::string_view path, Vec<Bookmarks*>* bkms);

Vec<Bookmarks*>* LoadAlterenativeBookmarks(std::string_view baseFileName);

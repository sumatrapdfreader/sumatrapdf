/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void SerializeBookmarksRec(DocTocItem* node, int level, str::Str& s);
DocTocTree* ParseBookmarksFile(std::string_view path);

#define MAX_ALT_BOOKMARKS 64

struct AlternativeBookmarks {
    int count = 0;
    DocTocTree* bookmarks[MAX_ALT_BOOKMARKS] = {};
    ~AlternativeBookmarks();
};

AlternativeBookmarks* LoadAlterenativeBookmarks(std::string_view baseFileName);

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// this is a collection of structs and classes that are
// useful for more than one ebook format

// one table-of-contents entry; level starts at 1
struct EbookTocItem {
    Str name;
    Str url;
    int level = 0;
};

// what a document's ParseToc() collects, in document order: iterate items.
// Owns the strings.
struct EbookToc {
    Vec<EbookTocItem> items;

    EbookToc() = default;
    ~EbookToc() {
        for (EbookTocItem& it : items) {
            str::Free(it.name);
            str::Free(it.url);
        }
    }
    EbookToc(const EbookToc&) = delete;
    EbookToc& operator=(const EbookToc&) = delete;

    void Add(Str name, Str url, int level) { VecAppend(items, {str::Dup(name), str::Dup(url), level}); }
};

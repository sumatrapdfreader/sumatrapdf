/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrQueue;

struct DirIterEntry {
#if OS_WIN
    WIN32_FIND_DATAW* fd = nullptr;
#endif
    Str name;
    Str filePath;
    i64 size = 0;
    FILETIME accessTime{};
    FILETIME modificationTime{};
    bool isDir = false;
    bool isFile = false;
    bool stopTraversal = false;
    bool fileMatches = false;
};

struct DirIter {
    Str dir;
    bool includeFiles = true;
    bool includeDirs = false;
    bool recurse = false;

    struct iterator {
        const DirIter* di;
        bool didFinish = false;

        StrVec dirsToVisit;
        TempStr currDir = {};
#if OS_WIN
        WStr pattern;
        WIN32_FIND_DATAW fd{};
        HANDLE h = nullptr;
#else
        void* dirHandle = nullptr;
#endif
        DirIterEntry data;

        iterator(const DirIter*, bool);
        iterator(const iterator&);
        iterator& operator=(const iterator&);
        ~iterator();

        DirIterEntry* operator*();
        iterator& operator++();   // ++it
        iterator operator++(int); // it++
        iterator& operator+(int); // it += n
        friend bool operator==(const iterator& a, const iterator& b);
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    iterator begin() const;
    iterator end() const;
};

void StartDirTraverseAsync(StrQueue* queue, Str dir, bool recurse);

i64 GetFileSize(DirIterEntry*);
bool IsDirectory(DirIterEntry*);
bool IsRegularFile(DirIterEntry*);

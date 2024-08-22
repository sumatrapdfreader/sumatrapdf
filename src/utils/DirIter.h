/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrQueue;

struct DirIterEntry {
    WIN32_FIND_DATAW* fd = nullptr;
    const char* name = nullptr;
    const char* filePath = nullptr;
    bool stopTraversal = false;
    bool fileMatches = false;
};

struct DirIter {
    const char* dir = nullptr;
    bool includeFiles = true;
    bool includeDirs = false;
    bool recurse = false;

    struct iterator {
        const DirIter* di;
        bool didFinish = false;

        StrVec dirsToVisit;
        char* currDir = nullptr;
        WCHAR* pattern = nullptr;
        WIN32_FIND_DATAW fd{};
        HANDLE h = nullptr;
        DirIterEntry data;

        iterator(const DirIter*, bool);
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

void StartDirTraverseAsync(StrQueue* queue, const char* dir, bool recurse);

i64 GetFileSize(WIN32_FIND_DATAW*);
bool IsDirectory(DWORD fileAttr);
bool IsRegularFile(DWORD fileAttr);

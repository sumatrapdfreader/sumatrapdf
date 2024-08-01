/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrQueue;

constexpr u32 kVisitDirIncudeFiles = 0x1;
constexpr u32 kVisitDirIncludeDirs = 0x2;
constexpr u32 kVisitDirRecurse = 0x4;

struct VisitDirData {
    WIN32_FIND_DATAW* fd = nullptr;
    const char* filePath = nullptr;
    bool stopTraversal = false;
};

using VisitDirCb = Func1<VisitDirData*>;

struct DirIter {
    const char* dir = nullptr;
    u32 flags = kVisitDirIncudeFiles;

    struct iterator {
        const DirIter* di;
        bool didFinish = false;

        StrVec dirsToVisit;
        WCHAR* pattern = nullptr;
        WIN32_FIND_DATAW fd{};
        HANDLE h = nullptr;
        VisitDirData data;

        iterator(const DirIter*, bool);
        ~iterator();

        VisitDirData* operator*();
        iterator& operator++();   // ++it
        iterator operator++(int); // it++
        iterator& operator+(int); // it += n
        friend bool operator==(const iterator& a, const iterator& b);
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    iterator begin() const;
    iterator end() const;
};

bool VisitDir(const char* dir, u32 flg, const VisitDirCb& cb);
bool DirTraverse(const char* dir, bool recurse, const VisitDirCb& cb);
bool VisitDirs(const char* dir, bool recurse, const VisitDirCb& cb);
bool CollectPathsFromDirectory(const char* pattern, StrVec& paths);
bool CollectFilesFromDirectory(const char* dir, StrVec& files, const VisitDirCb& fileMatches);
void StartDirTraverseAsync(StrQueue* queue, const char* dir, bool recurse);

i64 GetFileSize(WIN32_FIND_DATAW*);
bool IsDirectory(DWORD fileAttr);
bool IsRegularFile(DWORD fileAttr);

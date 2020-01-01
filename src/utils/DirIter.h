/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* How to use:

DirIter di(dir, recursive);
for (const WCHAR *filePath = di.First(); filePath; filePath = di.Next()) {
    // process filePath
}

*/
class DirIter {
    bool recursive = false;

    WStrVec dirsToVisit;
    AutoFreeWstr startDir;
    AutoFreeWstr currDir;
    AutoFreeWstr currPath;
    HANDLE currFindHandle = nullptr;
    WIN32_FIND_DATAW currFindData{};
    bool foundNext = false;

    bool StartDirIter(const WCHAR* dir);
    bool TryNextDir();

  public:
    DirIter(const WCHAR* dir, bool recur = false) {
        recursive = recur;
        startDir.SetCopy(dir);
    }
    ~DirIter() {
        FindClose(currFindHandle);
    }

    const WCHAR* First();
    const WCHAR* Next();
};

bool CollectPathsFromDirectory(const WCHAR* pattern, WStrVec& paths, bool dirsInsteadOfFiles = false);
std::vector<std::wstring> CollectDirsFromDirectory(const WCHAR*);

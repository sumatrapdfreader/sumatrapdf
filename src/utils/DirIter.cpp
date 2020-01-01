/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/DirIter.h"
#include "utils/FileUtil.h"

// Start directory traversal in a given dir
bool DirIter::StartDirIter(const WCHAR* dir) {
    currDir.SetCopy(dir);
    AutoFreeWstr pattern(path::Join(currDir, L"*"));
    currFindHandle = FindFirstFile(pattern, &currFindData);
    if (INVALID_HANDLE_VALUE == currFindHandle) {
        return false;
    }
    return true;
}

bool DirIter::TryNextDir() {
    while (dirsToVisit.size() > 0) {
        AutoFreeWstr nextDir(dirsToVisit.Pop());
        // it's ok if we fail, this might be an auth problem,
        // we keep going
        bool ok = StartDirIter(nextDir);
        if (ok) {
            return true;
        }
    }
    return false;
}

// Start iteration in a given dir and return fullPath of first
// file found or nullptr if no files
const WCHAR* DirIter::First() {
    foundNext = StartDirIter(startDir);
    if (!foundNext) {
        return nullptr;
    }
    return Next();
}

// try to filter out things that are not files
// or not meant to be used by other applications
static bool IsRegularFile(DWORD fileAttr) {
    if (fileAttr & FILE_ATTRIBUTE_DEVICE) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_OFFLINE) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_TEMPORARY) {
        return false;
    }
    if (fileAttr & FILE_ATTRIBUTE_REPARSE_POINT) {
        return false;
    }
    return true;
}

static bool IsDirectory(DWORD fileAttr) {
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        return true;
    }
    return false;
}

// "." and ".." are special
static bool IsSpecialDir(const WCHAR* s) {
    return str::Eq(s, L".") || str::Eq(s, L"..");
}

// Returns a full path of the next file
// Returns nullptr if finished iteration.
// Returned value is valid only until we call Next() again.
const WCHAR* DirIter::Next() {
    // when we enter here, currFindData has info for an entry
    // we haven't processed yet (filled by StartDirIter() or
    // ourselves at the end) unless foundNext is false
    currPath.Reset();
    while (foundNext && !currPath) {
        WCHAR* f = currFindData.cFileName;
        if ((currFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (recursive && !IsSpecialDir(f)) {
                WCHAR* d = path::Join(currDir, f);
                dirsToVisit.Append(d);
            }
        } else if (IsRegularFile(currFindData.dwFileAttributes)) {
            WCHAR* p = path::Join(currDir, f);
            currPath.Set(p);
        }
        BOOL hasMore = FindNextFile(currFindHandle, &currFindData);
        if (!hasMore) {
            foundNext = TryNextDir();
        }
    }
    return currPath;
}

bool CollectPathsFromDirectory(const WCHAR* pattern, WStrVec& paths, bool dirsInsteadOfFiles) {
    AutoFreeWstr dirPath = path::GetDir(pattern);

    WIN32_FIND_DATA fdata{};
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return false;
    }

    do {
        bool append = !dirsInsteadOfFiles;
        if ((fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            append = dirsInsteadOfFiles && !IsSpecialDir(fdata.cFileName);
        }
        if (append) {
            paths.Append(path::Join(dirPath, fdata.cFileName));
        }
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    return paths.size() > 0;
}

// returns a list of directories (full paths) in a given directory
// TODO: add recursive flag
std::vector<std::wstring> CollectDirsFromDirectory(const WCHAR* dir) {
    AutoFreeWstr pattern = path::Join(dir, L"*");

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFileW(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return {};
    }

    std::vector<std::wstring> res;
    do {
        if (IsDirectory(fdata.dwFileAttributes)) {
            if (!IsSpecialDir(fdata.cFileName)) {
                AutoFreeWstr s = path::Join(dir, fdata.cFileName);
                res.emplace_back(std::move(std::wstring(s)));
            }
        }
    } while (FindNextFileW(hfind, &fdata));
    FindClose(hfind);

    return res;
}

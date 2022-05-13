/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/DirIter.h"
#include "utils/FileUtil.h"

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

bool IsDirectory(DWORD fileAttr) {
    return (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// "." and ".." are special
static bool IsSpecialDir(const WCHAR* s) {
    return str::Eq(s, L".") || str::Eq(s, L"..");
}

bool DirTraverse(const char* dir, bool recurse, const std::function<bool(const char*)>& cb) {
    auto dirW = ToWstrTemp(dir);
    WCHAR* pattern = path::JoinTemp(dirW, L"*");

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFileW(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return false;
    }

    bool isFile;
    bool isDir;
    bool cont = true;
    do {
        isFile = IsRegularFile(fdata.dwFileAttributes);
        isDir = IsDirectory(fdata.dwFileAttributes);
        char* name = ToUtf8Temp(fdata.cFileName);
        const char* path = path::JoinTemp(dir, name);
        if (isFile) {
            cont = cb(path);
        } else if (recurse && isDir) {
            cont = DirTraverse(path, recurse, cb);
        }
    } while (cont && FindNextFileW(hfind, &fdata));
    FindClose(hfind);
    return true;
}

bool DirTraverse(const WCHAR* dir, bool recurse, const std::function<bool(const WCHAR*)>& cb) {
    WCHAR* pattern = path::JoinTemp(dir, L"*");

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFileW(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return false;
    }

    bool isFile;
    bool isDir;
    bool cont = true;
    do {
        isFile = IsRegularFile(fdata.dwFileAttributes);
        isDir = IsDirectory(fdata.dwFileAttributes);
        WCHAR* name = fdata.cFileName;
        const WCHAR* path = path::JoinTemp(dir, name);
        if (isFile) {
            cont = cb(path);
        } else if (recurse && isDir) {
            cont = DirTraverse(path, recurse, cb);
        }
    } while (cont && FindNextFileW(hfind, &fdata));
    FindClose(hfind);
    return true;
}

bool CollectPathsFromDirectory(const WCHAR* pattern, WStrVec& paths, bool dirsInsteadOfFiles) {
    WCHAR* dirPath = path::GetDirTemp(pattern);

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

bool CollectFilesFromDirectory(const char* dir, StrVec& files, const std::function<bool(const char*)>& fileMatchesFn) {
    auto dirW = ToWstrTemp(dir);
    WCHAR* pattern = path::JoinTemp(dirW, L"*");

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFileW(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return false;
    }

    bool isFile;
    do {
        isFile = IsRegularFile(fdata.dwFileAttributes);
        if (isFile) {
            char* name = ToUtf8Temp(fdata.cFileName);
            char* filePath = path::JoinTemp(dir, name);
            bool matches = true;
            if (fileMatchesFn) {
                matches = fileMatchesFn(filePath);
            }
            if (matches) {
                files.Append(filePath);
            }
        }
    } while (FindNextFileW(hfind, &fdata));
    FindClose(hfind);
    return true;
}

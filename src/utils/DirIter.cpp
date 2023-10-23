/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/DirIter.h"

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
    return (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool IsSpecialDir(const char* s) {
    return str::Eq(s, ".") || str::Eq(s, "..");
}

bool DirTraverse(const char* dir, bool recurse, const std::function<bool(WIN32_FIND_DATAW* fd, const char*)>& cb) {
    auto dirW = ToWStrTemp(dir);
    WCHAR* pattern = path::JoinTemp(dirW, L"*");

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (!IsValidHandle(h)) {
        return false;
    }

    bool isFile;
    bool isDir;
    bool cont = true;
    char* name;
    char* path;
    do {
        isFile = IsRegularFile(fd.dwFileAttributes);
        isDir = IsDirectory(fd.dwFileAttributes);
        name = ToUtf8Temp(fd.cFileName);
        path = path::JoinTemp(dir, name);
        if (isFile) {
            cont = cb(&fd, path);
        } else if (recurse && isDir) {
            if (!IsSpecialDir(name)) {
                cont = DirTraverse(path, recurse, cb);
            }
        }
    } while (cont && FindNextFileW(h, &fd));
    FindClose(h);
    return true;
}

bool DirTraverse(const char* dir, bool recurse, const std::function<bool(const char*)>& cb) {
    bool ok = DirTraverse(dir, recurse, [&cb](WIN32_FIND_DATAW*, const char* path) -> bool {
        bool cont = cb(path);
        return cont;
    });
    return ok;
}

bool CollectPathsFromDirectory(const char* pattern, StrVec& paths, bool dirsInsteadOfFiles) {
    char* dir = path::GetDirTemp(pattern);

    WIN32_FIND_DATAW fdata{};
    WCHAR* patternW = ToWstr(pattern);
    HANDLE hfind = FindFirstFileW(patternW, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return false;
    }

    bool append;
    do {
        append = false;
        char* name = ToUtf8Temp(fdata.cFileName);
        DWORD attrs = fdata.dwFileAttributes;
        if (IsRegularFile(attrs)) {
            append = !dirsInsteadOfFiles;
        } else if (IsDirectory(attrs)) {
            append = dirsInsteadOfFiles && !IsSpecialDir(name);
        }
        if (append) {
            char* path = path::JoinTemp(dir, name);
            paths.Append(path);
        }
    } while (FindNextFileW(hfind, &fdata));
    FindClose(hfind);
    return paths.size() > 0;
}

bool CollectFilesFromDirectory(const char* dir, StrVec& files, const std::function<bool(const char*)>& fileMatchesFn) {
    auto dirW = ToWStrTemp(dir);
    WCHAR* pattern = path::JoinTemp(dirW, L"*");

    WIN32_FIND_DATAW fdata;
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

i64 GetFileSize(WIN32_FIND_DATAW* fd) {
    ULARGE_INTEGER ul;
    ul.HighPart = fd->nFileSizeHigh;
    ul.LowPart = fd->nFileSizeLow;
    return (i64)ul.QuadPart;
}

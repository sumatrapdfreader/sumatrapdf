/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void FileUtilTest() {
    const WCHAR* path1 = L"C:\\Program Files\\SumatraPDF\\SumatraPDF.exe";

    const WCHAR* baseName = path::GetBaseNameTemp(path1);
    utassert(str::Eq(baseName, L"SumatraPDF.exe"));

    WCHAR* dirName = path::GetDirTemp(path1);
    utassert(str::Eq(dirName, L"C:\\Program Files\\SumatraPDF"));
    baseName = path::GetBaseNameTemp(dirName);
    utassert(str::Eq(baseName, L"SumatraPDF"));

    dirName = path::GetDirTemp(L"C:\\Program Files");
    utassert(str::Eq(dirName, L"C:\\"));
    dirName = path::GetDirTemp(dirName);
    utassert(str::Eq(dirName, L"C:\\"));
    dirName = path::GetDirTemp(L"\\\\server");
    utassert(str::Eq(dirName, L"\\\\server"));
    dirName = path::GetDirTemp(L"file.exe");
    utassert(str::Eq(dirName, L"."));
    dirName = path::GetDirTemp(L"/etc");
    utassert(str::Eq(dirName, L"/"));

    path1 = L"C:\\Program Files";
    WCHAR* path2 = path::Join(L"C:\\", L"Program Files");
    utassert(str::Eq(path1, path2));
    free(path2);
    path2 = path::Join(path1, L"SumatraPDF");
    utassert(str::Eq(path2, L"C:\\Program Files\\SumatraPDF"));
    free(path2);
    path2 = path::Join(L"C:\\", L"\\Windows");
    utassert(str::Eq(path2, L"C:\\Windows"));
    free(path2);

    utassert(path::Match("C:\\file.pdf", "*.pdf"));
    utassert(path::Match("C:\\file.pdf", "file.*"));
    utassert(path::Match("C:\\file.pdf", "*.xps;*.pdf"));
    utassert(path::Match("C:\\file.pdf", "*.xps;*.pdf;*.djvu"));
    utassert(path::Match("C:\\file.pdf", "f??e.p?f"));
    utassert(!path::Match("C:\\file.pdf", "*.xps;*.djvu"));
    utassert(!path::Match("C:\\dir.xps\\file.pdf", "*.xps;*.djvu"));
    utassert(!path::Match("C:\\file.pdf", "f??f.p?f"));
    utassert(!path::Match("C:\\.pdf", "?.pdf"));
    {
        WCHAR* path = path::JoinTemp(L"foo", L"bar");
        utassert(str::Eq(path, L"foo\\bar"));

        path = path::JoinTemp(L"foo\\", L"bar");
        utassert(str::Eq(path, L"foo\\bar"));

        path = path::JoinTemp(L"foo", L"\\bar");
        utassert(str::Eq(path, L"foo\\bar"));

        path = path::JoinTemp(L"foo\\", L"\\bar");
        utassert(str::Eq(path, L"foo\\bar"));

        path = path::JoinTemp(L"foo\\", L"\\bar\\", L"\\z");
        utassert(str::Eq(path, L"foo\\bar\\z"));
    }
    {
        WCHAR* path = path::Join(L"foo", L"bar");
        utassert(str::Eq(path, L"foo\\bar"));
        str::Free(path);

        path = path::Join(L"foo\\", L"bar");
        utassert(str::Eq(path, L"foo\\bar"));
        str::Free(path);

        path = path::Join(L"foo", L"\\bar");
        utassert(str::Eq(path, L"foo\\bar"));
        str::Free(path);

        path = path::Join(L"foo\\", L"\\bar");
        utassert(str::Eq(path, L"foo\\bar"));
        str::Free(path);

        path = path::Join(L"foo\\", L"\\bar\\", L"\\z");
        utassert(str::Eq(path, L"foo\\bar\\z"));
        str::Free(path);
    }
}

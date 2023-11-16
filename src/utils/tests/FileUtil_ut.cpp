/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void FileUtilTest() {
    const char* path1 = "C:\\Program Files\\SumatraPDF\\SumatraPDF.exe";

    TempStr baseName = path::GetBaseNameTemp(path1);
    utassert(str::Eq(baseName, "SumatraPDF.exe"));

    TempStr dirName = path::GetDirTemp(path1);
    utassert(str::Eq(dirName, "C:\\Program Files\\SumatraPDF"));
    baseName = path::GetBaseNameTemp(dirName);
    utassert(str::Eq(baseName, "SumatraPDF"));

    dirName = path::GetDirTemp("C:\\Program Files");
    utassert(str::Eq(dirName, "C:\\"));
    dirName = path::GetDirTemp(dirName);
    utassert(str::Eq(dirName, "C:\\"));
    dirName = path::GetDirTemp("\\\\server");
    utassert(str::Eq(dirName, "\\\\server"));
    dirName = path::GetDirTemp("file.exe");
    utassert(str::Eq(dirName, "."));
    dirName = path::GetDirTemp("/etc");
    utassert(str::Eq(dirName, "/"));

    path1 = "C:\\Program Files";
    char* path2 = path::Join("C:\\", "Program Files");
    utassert(str::Eq(path1, path2));
    free(path2);
    path2 = path::Join(path1, "SumatraPDF");
    utassert(str::Eq(path2, "C:\\Program Files\\SumatraPDF"));
    free(path2);
    path2 = path::Join("C:\\", "\\Windows");
    utassert(str::Eq(path2, "C:\\Windows"));
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
        char* path = path::JoinTemp("foo", "bar");
        utassert(str::Eq(path, "foo\\bar"));

        path = path::JoinTemp("foo\\", "bar");
        utassert(str::Eq(path, "foo\\bar"));

        path = path::JoinTemp("foo", "\\bar");
        utassert(str::Eq(path, "foo\\bar"));

        path = path::JoinTemp("foo\\", "\\bar");
        utassert(str::Eq(path, "foo\\bar"));

        path = path::JoinTemp("foo\\", "\\bar\\", "\\z");
        utassert(str::Eq(path, "foo\\bar\\z"));
    }
    {
        char* path = path::Join("foo", "bar");
        utassert(str::Eq(path, "foo\\bar"));
        str::Free(path);

        path = path::Join("foo\\", "bar");
        utassert(str::Eq(path, "foo\\bar"));
        str::Free(path);

        path = path::Join("foo", "\\bar");
        utassert(str::Eq(path, "foo\\bar"));
        str::Free(path);

        path = path::Join("foo\\", "\\bar");
        utassert(str::Eq(path, "foo\\bar"));
        str::Free(path);

        // path = path::Join("foo\\", "\\bar\\", "\\z");
        // utassert(str::Eq(path, "foo\\bar\\z"));
        // str::Free(path);
    }
}

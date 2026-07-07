/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

void FileUtilTest() {
#if OS_WIN
    Str path1 = "C:\\Program Files\\SumatraPDF\\SumatraPDF.exe";

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
    Str path2 = path::Join("C:\\", "Program Files");
    utassert(str::Eq(path1, path2));
    str::Free(path2);
    path2 = path::Join(path1, "SumatraPDF");
    utassert(str::Eq(path2, "C:\\Program Files\\SumatraPDF"));
    str::Free(path2);
    path2 = path::Join("C:\\", "\\Windows");
    utassert(str::Eq(path2, "C:\\Windows"));
    str::Free(path2);

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
        TempStr path = path::JoinTemp("foo", "bar");
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
        Str path = path::Join("foo", "bar");
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
    {
        // regression: NormalizeTemp() used GetFullPathNameW's 0-buffer size
        // (which includes the terminating NUL) as the result length, leaving
        // it one char too long so str::EqI() against the real path didn't match
        Str p = "C:\\foo\\prince of persia technical doc.pdf";
        TempStr norm = path::NormalizeTemp(p);
        utassert(str::EqI(norm, p));
    }
#else
    Str path1 = "/Applications/SumatraPDF/SumatraPDF";

    TempStr baseName = path::GetBaseNameTemp(path1);
    utassert(str::Eq(baseName, "SumatraPDF"));

    TempStr dirName = path::GetDirTemp(path1);
    utassert(str::Eq(dirName, "/Applications/SumatraPDF"));
    baseName = path::GetBaseNameTemp(dirName);
    utassert(str::Eq(baseName, "SumatraPDF"));

    dirName = path::GetDirTemp("/etc");
    utassert(str::Eq(dirName, "/"));
    dirName = path::GetDirTemp("file");
    utassert(str::Eq(dirName, "."));

    Str path2 = path::Join("/Applications", "SumatraPDF");
    utassert(str::Eq(path2, "/Applications/SumatraPDF"));
    str::Free(path2);
    path2 = path::Join("/Applications/", "/SumatraPDF");
    utassert(str::Eq(path2, "/Applications/SumatraPDF"));
    str::Free(path2);

    utassert(path::Match("/tmp/file.pdf", "*.pdf"));
    utassert(path::Match("/tmp/file.pdf", "file.*"));
    utassert(path::Match("/tmp/file.pdf", "*.xps;*.pdf"));
    utassert(!path::Match("/tmp/file.pdf", "*.xps;*.djvu"));

    TempStr path = path::JoinTemp("foo", "bar");
    utassert(str::Eq(path, "foo/bar"));
    path = path::JoinTemp("foo/", "/bar");
    utassert(str::Eq(path, "foo/bar"));
    path = path::JoinTemp("foo/", "/bar/", "/z");
    utassert(str::Eq(path, "foo/bar/z"));

    Str joined = path::Join("foo", "bar");
    utassert(str::Eq(joined, "foo/bar"));
    str::Free(joined);
#endif
}

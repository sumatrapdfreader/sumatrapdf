/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileUtil.h"
#include "UtAssert.h"

void FileUtilTest()
{
    WCHAR *path1 = L"C:\\Program Files\\SumatraPDF\\SumatraPDF.exe";

    const WCHAR *baseName = path::GetBaseName(path1);
    assert(str::Eq(baseName, L"SumatraPDF.exe"));

    ScopedMem<WCHAR> dirName(path::GetDir(path1));
    assert(str::Eq(dirName, L"C:\\Program Files\\SumatraPDF"));
    baseName = path::GetBaseName(dirName);
    assert(str::Eq(baseName, L"SumatraPDF"));

    dirName.Set(path::GetDir(L"C:\\Program Files"));
    assert(str::Eq(dirName, L"C:\\"));
    dirName.Set(path::GetDir(dirName));
    assert(str::Eq(dirName, L"C:\\"));
    dirName.Set(path::GetDir(L"\\\\server"));
    assert(str::Eq(dirName, L"\\\\server"));
    dirName.Set(path::GetDir(L"file.exe"));
    assert(str::Eq(dirName, L"."));
    dirName.Set(path::GetDir(L"/etc"));
    assert(str::Eq(dirName, L"/"));

    path1 = L"C:\\Program Files";
    WCHAR *path2 = path::Join(L"C:\\", L"Program Files");
    assert(str::Eq(path1, path2));
    free(path2);
    path2 = path::Join(path1, L"SumatraPDF");
    assert(str::Eq(path2, L"C:\\Program Files\\SumatraPDF"));
    free(path2);
    path2 = path::Join(L"C:\\", L"\\Windows");
    assert(str::Eq(path2, L"C:\\Windows"));
    free(path2);

    assert(path::Match(L"C:\\file.pdf", L"*.pdf"));
    assert(path::Match(L"C:\\file.pdf", L"file.*"));
    assert(path::Match(L"C:\\file.pdf", L"*.xps;*.pdf"));
    assert(path::Match(L"C:\\file.pdf", L"*.xps;*.pdf;*.djvu"));
    assert(path::Match(L"C:\\file.pdf", L"f??e.p?f"));
    assert(!path::Match(L"C:\\file.pdf", L"*.xps;*.djvu"));
    assert(!path::Match(L"C:\\dir.xps\\file.pdf", L"*.xps;*.djvu"));
    assert(!path::Match(L"C:\\file.pdf", L"f??f.p?f"));
    assert(!path::Match(L"C:\\.pdf", L"?.pdf"));
}

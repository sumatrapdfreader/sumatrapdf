/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void FileUtilTest() {
    WCHAR* path1 = L"C:\\Program Files\\SumatraPDF\\SumatraPDF.exe";

    const WCHAR* baseName = path::GetBaseNameNoFree(path1);
    utassert(str::Eq(baseName, L"SumatraPDF.exe"));

    AutoFreeWstr dirName(path::GetDir(path1));
    utassert(str::Eq(dirName, L"C:\\Program Files\\SumatraPDF"));
    baseName = path::GetBaseNameNoFree(dirName);
    utassert(str::Eq(baseName, L"SumatraPDF"));

    dirName.Set(path::GetDir(L"C:\\Program Files"));
    utassert(str::Eq(dirName, L"C:\\"));
    dirName.Set(path::GetDir(dirName));
    utassert(str::Eq(dirName, L"C:\\"));
    dirName.Set(path::GetDir(L"\\\\server"));
    utassert(str::Eq(dirName, L"\\\\server"));
    dirName.Set(path::GetDir(L"file.exe"));
    utassert(str::Eq(dirName, L"."));
    dirName.Set(path::GetDir(L"/etc"));
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

    utassert(path::Match(L"C:\\file.pdf", L"*.pdf"));
    utassert(path::Match(L"C:\\file.pdf", L"file.*"));
    utassert(path::Match(L"C:\\file.pdf", L"*.xps;*.pdf"));
    utassert(path::Match(L"C:\\file.pdf", L"*.xps;*.pdf;*.djvu"));
    utassert(path::Match(L"C:\\file.pdf", L"f??e.p?f"));
    utassert(!path::Match(L"C:\\file.pdf", L"*.xps;*.djvu"));
    utassert(!path::Match(L"C:\\dir.xps\\file.pdf", L"*.xps;*.djvu"));
    utassert(!path::Match(L"C:\\file.pdf", L"f??f.p?f"));
    utassert(!path::Match(L"C:\\.pdf", L"?.pdf"));
}

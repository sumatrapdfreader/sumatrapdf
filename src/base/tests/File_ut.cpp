/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

void FileUtilTest() {
#if OS_WIN
    Str path1 = StrL("C:\\Program Files\\SumatraPDF\\SumatraPDF.exe");

    TempStr baseName = path::GetBaseNameTemp(path1);
    utassert(str::Eq(baseName, StrL("SumatraPDF.exe")));

    TempStr dirName = path::GetDirTemp(path1);
    utassert(str::Eq(dirName, StrL("C:\\Program Files\\SumatraPDF")));
    baseName = path::GetBaseNameTemp(dirName);
    utassert(str::Eq(baseName, StrL("SumatraPDF")));

    dirName = path::GetDirTemp(StrL("C:\\Program Files"));
    utassert(str::Eq(dirName, StrL("C:\\")));
    dirName = path::GetDirTemp(dirName);
    utassert(str::Eq(dirName, StrL("C:\\")));
    dirName = path::GetDirTemp(StrL("\\\\server"));
    utassert(str::Eq(dirName, StrL("\\\\server")));
    dirName = path::GetDirTemp(StrL("file.exe"));
    utassert(str::Eq(dirName, StrL(".")));
    utassert(path::IsDriveRoot(StrL("C:\\")));
    utassert(path::IsDriveRoot(StrL("C:/")));
    utassert(path::IsDriveRoot(StrL("C:\\\\")));
    utassert(!path::IsDriveRoot(StrL("C:")));
    utassert(!path::IsDriveRoot(StrL("C:\\Windows")));
    utassert(!path::IsDriveRoot(StrL("file.exe")));
    dirName = path::GetDirTemp(StrL("/etc"));
    utassert(str::Eq(dirName, StrL("/")));

    path1 = StrL("C:\\Program Files");
    Str path2 = path::Join(StrL("C:\\"), StrL("Program Files"));
    utassert(str::Eq(path1, path2));
    str::Free(path2);
    path2 = path::Join(path1, StrL("SumatraPDF"));
    utassert(str::Eq(path2, StrL("C:\\Program Files\\SumatraPDF")));
    str::Free(path2);
    path2 = path::Join(StrL("C:\\"), StrL("\\Windows"));
    utassert(str::Eq(path2, StrL("C:\\Windows")));
    str::Free(path2);

    utassert(path::Match(StrL("C:\\file.pdf"), StrL("*.pdf")));
    utassert(path::Match(StrL("C:\\file.pdf"), StrL("file.*")));
    utassert(path::Match(StrL("C:\\file.pdf"), StrL("*.xps;*.pdf")));
    utassert(path::Match(StrL("C:\\file.pdf"), StrL("*.xps;*.pdf;*.djvu")));
    utassert(path::Match(StrL("C:\\file.pdf"), StrL("f??e.p?f")));
    utassert(!path::Match(StrL("C:\\file.pdf"), StrL("*.xps;*.djvu")));
    utassert(!path::Match(StrL("C:\\dir.xps\\file.pdf"), StrL("*.xps;*.djvu")));

    utassert(path::IsEphemeralHostFile(StrL("C:\\Users\\x\\AppData\\Local\\Microsoft\\OneNote\\16.0\\cache\\a.pdf")));
    utassert(path::IsEphemeralHostFile(StrL("C:\\Users\\x\\AppData\\Local\\Temp\\OneNote\\tmp\\a.pdf")));
    utassert(path::IsEphemeralHostFile(
        StrL("C:\\Users\\x\\AppData\\Local\\Microsoft\\Windows\\INetCache\\Content.Outlook\\ABC\\a.pdf")));
    utassert(
        path::IsEphemeralHostFile(StrL("C:\\Users\\x\\AppData\\Local\\Microsoft\\Windows\\INetCache\\IE\\xyz\\a.pdf")));
    utassert(path::IsEphemeralHostFile(
        StrL("C:\\Users\\x\\AppData\\Local\\Packages\\Microsoft.Office.OneNote_8wekyb3d8bbwe\\LocalState\\a.pdf")));
    utassert(!path::IsEphemeralHostFile(StrL("C:\\docs\\paper.pdf")));
    utassert(!path::IsEphemeralHostFile(StrL("C:\\docs\\OneNotePDFs\\a.pdf")));
    utassert(!path::IsEphemeralHostFile(StrL("C:\\docs\\my-onenote-export.pdf")));
    utassert(!path::Match(StrL("C:\\file.pdf"), StrL("f??f.p?f")));
    utassert(!path::Match(StrL("C:\\.pdf"), StrL("?.pdf")));
    {
        TempStr path = path::JoinTemp(StrL("foo"), StrL("bar"));
        utassert(str::Eq(path, StrL("foo\\bar")));

        path = path::JoinTemp(StrL("foo\\"), StrL("bar"));
        utassert(str::Eq(path, StrL("foo\\bar")));

        path = path::JoinTemp(StrL("foo"), StrL("\\bar"));
        utassert(str::Eq(path, StrL("foo\\bar")));

        path = path::JoinTemp(StrL("foo\\"), StrL("\\bar"));
        utassert(str::Eq(path, StrL("foo\\bar")));

        path = path::JoinTemp(StrL("foo\\"), StrL("\\bar\\"), StrL("\\z"));
        utassert(str::Eq(path, StrL("foo\\bar\\z")));
    }
    {
        Str path = path::Join(StrL("foo"), StrL("bar"));
        utassert(str::Eq(path, StrL("foo\\bar")));
        str::Free(path);

        path = path::Join(StrL("foo\\"), StrL("bar"));
        utassert(str::Eq(path, StrL("foo\\bar")));
        str::Free(path);

        path = path::Join(StrL("foo"), StrL("\\bar"));
        utassert(str::Eq(path, StrL("foo\\bar")));
        str::Free(path);

        path = path::Join(StrL("foo\\"), StrL("\\bar"));
        utassert(str::Eq(path, StrL("foo\\bar")));
        str::Free(path);

        // path = path::Join("foo\\", StrL("\\bar\\"), "\\z");
        // utassert(str::Eq(path, StrL("foo\\bar\\z")));
        // str::Free(path);
    }
    {
        // regression: NormalizeTemp() used GetFullPathNameW's 0-buffer size
        // (which includes the terminating NUL) as the result length, leaving
        // it one char too long so str::EqI() against the real path didn't match
        Str p = StrL("C:\\foo\\prince of persia technical doc.pdf");
        TempStr norm = path::NormalizeTemp(p);
        utassert(str::EqI(norm, p));
    }
#else
    Str path1 = "/Applications/SumatraPDF/SumatraPDF";

    TempStr baseName = path::GetBaseNameTemp(path1);
    utassert(str::Eq(baseName, StrL("SumatraPDF")));

    TempStr dirName = path::GetDirTemp(path1);
    utassert(str::Eq(dirName, StrL("/Applications/SumatraPDF")));
    baseName = path::GetBaseNameTemp(dirName);
    utassert(str::Eq(baseName, StrL("SumatraPDF")));

    dirName = path::GetDirTemp(StrL("/etc"));
    utassert(str::Eq(dirName, StrL("/")));
    dirName = path::GetDirTemp(StrL("file"));
    utassert(str::Eq(dirName, StrL(".")));

    Str path2 = path::Join("/Applications", StrL("SumatraPDF"));
    utassert(str::Eq(path2, StrL("/Applications/SumatraPDF")));
    str::Free(path2);
    path2 = path::Join("/Applications/", StrL("/SumatraPDF"));
    utassert(str::Eq(path2, StrL("/Applications/SumatraPDF")));
    str::Free(path2);

    utassert(path::Match(StrL("/tmp/file.pdf"), StrL("*.pdf")));
    utassert(path::Match(StrL("/tmp/file.pdf"), StrL("file.*")));
    utassert(path::Match(StrL("/tmp/file.pdf"), StrL("*.xps;*.pdf")));
    utassert(!path::Match(StrL("/tmp/file.pdf"), StrL("*.xps;*.djvu")));

    TempStr path = path::JoinTemp("foo", StrL("bar"));
    utassert(str::Eq(path, StrL("foo/bar")));
    path = path::JoinTemp("foo/", StrL("/bar"));
    utassert(str::Eq(path, StrL("foo/bar")));
    path = path::JoinTemp("foo/", StrL("/bar/"), "/z");
    utassert(str::Eq(path, StrL("foo/bar/z")));

    Str joined = path::Join("foo", StrL("bar"));
    utassert(str::Eq(joined, StrL("foo/bar")));
    str::Free(joined);
#endif

    {
        // a temp dir that doesn't fit the first buffer must come back whole:
        // the retry must run and return the same path as a roomy first try
        TempStr expected = GetTempDirTemp(MAX_PATH);
        utassert(len(expected) > 0);
        int sizes[] = {1, 2, 8};
        for (int cch : sizes) {
            TempStr got = GetTempDirTemp(cch);
            utassert(str::Eq(got, expected));
        }
    }

    {
        // the module path must come back whole no matter how small the first
        // buffer is, i.e. the growing loop must run and not truncate
        TempWStr expected = GetModulePathTemp((HMODULE) nullptr, MAX_PATH + 1);
        utassert(len(expected) > 0);
        int sizes[] = {1, 2, 8, 64};
        for (int cch : sizes) {
            TempWStr got = GetModulePathTemp((HMODULE) nullptr, cch);
            utassert(len(got) == len(expected));
            utassert(wstr::Eq(got, expected));
        }
    }

    {
        // write a temp file, map it and verify the view matches what was written
        TempStr path = GetTempFilePathTemp(StrL("mmap-test"));
        utassert(len(path) > 0);
        Str content = StrL("file::MemoryMap test content 0123456789");
        bool ok = file::WriteFile(path, content);
        utassert(ok);
        file::Mapping m;
        ok = file::MemoryMap(path, &m);
        utassert(ok);
        utassert(m.size == (i64)len(content));
        utassert(m.data && memcmp(m.data, content.s, (size_t)len(content)) == 0);
        file::MemoryUnmap(&m);
        utassert(!m.data && m.size == 0);
        ok = file::Delete(path);
        utassert(ok);
    }
}

#if OS_WIN
static void MakeDirTree(Str root) {
    utassert(dir::CreateAll(path::JoinTemp(root, StrL("a\\b"))));
    utassert(file::WriteFile(path::JoinTemp(root, StrL("top.txt")), StrL("x")));
    utassert(file::WriteFile(path::JoinTemp(root, StrL("a\\mid.txt")), StrL("x")));
    TempStr ro = path::JoinTemp(root, StrL("a\\b\\ro.txt"));
    utassert(file::WriteFile(ro, StrL("x")));
    SetFileAttributesW(CWStrTemp(ro), FILE_ATTRIBUTE_READONLY);
}

struct RemoveAllWorker {
    Str root;
    int rounds = 0;
    bool ok = true;
};

static void RemoveAllWorkerFn(RemoveAllWorker* w) {
    // a fresh path per round: a just-deleted dir can linger as delete-pending
    // while an indexer or scanner still holds it, which breaks a re-create
    for (int i = 0; i < w->rounds; i++) {
        TempStr root = fmt("%s-%d", w->root, i);
        MakeDirTree(root);
        if (!dir::RemoveAll(root) || dir::Exists(root)) {
            w->ok = false;
            return;
        }
    }
}
#endif

// dir::RemoveAll must remove nested and read-only content and must be safe
// to call from several threads at once (the shutdown WebView profile removal
// races the DeleteStaleFilesAsync sweep)
void DirRemoveAllTest() {
#if OS_WIN
    TempStr root = GetTempFilePathTemp(StrL("rmall"));
    file::Delete(root);
    MakeDirTree(root);
    utassert(dir::Empty(root));
    utassert(dir::Exists(root));
    utassert(!file::Exists(path::JoinTemp(root, StrL("top.txt"))));
    utassert(!dir::Exists(path::JoinTemp(root, StrL("a"))));

    MakeDirTree(root);
    utassert(dir::RemoveAll(root));
    utassert(!dir::Exists(root));
    utassert(!dir::RemoveAll(root));

    const int kThreads = 4;
    const int kRounds = 25;
    const DWORD kTimeoutMs = 60 * 1000;
    RemoveAllWorker workers[kThreads];
    ThreadHandle threads[kThreads];
    for (int i = 0; i < kThreads; i++) {
        workers[i].root = str::Dup(fmt("%s-%d", root, i));
        workers[i].rounds = kRounds;
        threads[i] = StartThread(MkFunc0(RemoveAllWorkerFn, &workers[i]), StrL("RemoveAllWorker"));
    }
    DWORD res = WaitForMultipleObjects(kThreads, threads, TRUE, kTimeoutMs);
    utassert(res != WAIT_TIMEOUT && "concurrent dir::RemoveAll deadlocked");
    for (int i = 0; i < kThreads; i++) {
        if (res == WAIT_TIMEOUT) {
            TerminateThread(threads[i], 1);
        }
        SafeCloseThreadHandle(&threads[i]);
        utassert(workers[i].ok);
        str::Free(workers[i].root);
    }
#endif
}

/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "CmdLineParser.h"
#include "FileUtil.h"
#include "LzzaUtil.h"

#define FailIf(cond, msg, ...) if (cond) { fprintf(stderr, msg "\n", __VA_ARGS__); return errorStep; } errorStep++

int main(int argc, char **argv)
{
#ifdef DEBUG
    // report memory leaks on stderr
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    int errorStep = 1;

    WStrVec args;
    ParseCmdLine(GetCommandLine(), args);

    if (args.Count() == 2 && file::Exists(args.At(1))) {
        // verify lzza file
        lzza::Archive lzza;
        ScopedMem<char> lzzaData(file::ReadAll(args.At(1), &lzza.dataLen));
        lzza.data = lzzaData.Get();
        FailIf(!lzza.data, "Failed to read \"%S\"", args.At(1));
        int i = 0;
        for (;;) {
            ScopedMem<lzza::FileData> fd(lzza::GetFileData(&lzza, i++));
            FailIf(!fd, "Managed to extract %d files", i - 1);
        }
    }

    FailIf(args.Count() < 3, "Syntax: %S <archive.lzza> <filename>[:<in-archive name>] [...]", path::GetBaseName(args.At(0)));

    WStrVec names;
    for (size_t i = 2; i < args.Count(); i++) {
        const WCHAR *sep = str::FindChar(args.At(i), ':');
        if (sep) {
            names.Append(str::DupN(args.At(i), sep - args.At(i)));
            names.Append(str::Dup(sep + 1));
        }
        else {
            names.Append(str::Dup(args.At(i)));
            names.Append(NULL);
        }
    }

    WCHAR srcDir[MAX_PATH];
    DWORD srcDirLen = GetCurrentDirectory(dimof(srcDir), srcDir);
    FailIf(!srcDirLen || srcDirLen == dimof(srcDir), "Failed to determine the current directory");

    bool ok = lzza::CreateArchive(args.At(1), srcDir, names);
    FailIf(!ok, "Failed to create \"%S\"", args.At(1));

    return 0;
}

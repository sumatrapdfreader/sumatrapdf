#include "base/Base.h"
#include "base/File.h"
#include "base/DirScan.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/Timer.h"

#include <shlwapi.h>

#include "DocProperties.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "LitDoc.h"
#include "tools/TestEnginesCommon.h"

void _uploadDebugReport(Str, Str, bool) {}

struct EBookUI;
EBookUI* GetEBookUI() {
    return nullptr;
}

struct FileEBookUI;
FileEBookUI* GetFileEBookUI(Str) {
    return nullptr;
}

static void Usage() {
    printf("usage: test_engines <document-or-image-path>\n");
    printf("       test_engines <path> -bench-mediabox   time PageMediabox() for every page\n");
    printf("       test_engines <path> -list-links       list link targets and rectangles\n");
    printf("       test_engines <path> -select-all-text  exercise text selection and extraction\n");
    printf("       test_engines <path> -find-text <term> search all pages for text\n");
    printf("       test_engines <path> -list-toc        list table-of-contents entries\n");
    printf("       test_engines <path> -list-properties list document properties\n");
}

EngineBase* CreateEngineForPath(Str path) {
    if (IsEngineImageDirSupportedFile(path)) {
        return CreateEngineImageDirFromFile(path);
    }

    FileType kind = GuessFileTypeFromName(path);
    if (IsEngineDjVuSupportedFileType(kind)) {
        return CreateEngineDjvuDecFromFile(path);
    }
    if (IsEngineImageSupportedFileType(kind)) {
        return CreateEngineImageFromFile(path);
    }
    if (IsEngineCbxSupportedFileType(kind)) {
        return CreateEngineCbxFromFile(path, nullptr, kind);
    }
    if (kind == FileType::Lit) {
        return CreateEngineLitFromFile(path, nullptr);
    }
    if (IsEngineMupdfSupportedFileType(kind)) {
        return CreateEngineMupdfFromFile(path, kind, 96, nullptr);
    }
    return nullptr;
}

int main(int argc, char** argv) {
    if (argc == 4 && str::Eq(argv[2], StrL("-find-text"))) {
        bool ok = FindText(Str(argv[1]), Str(argv[3]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-list-toc"))) {
        bool ok = ListToc(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-list-properties"))) {
        bool ok = ListProperties(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-bench-readn"))) {
        bool ok = BenchReadN(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 2 && str::Eq(argv[1], StrL("-check-netdrive"))) {
        bool ok = CheckIsOnNetworkDrive();
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-bench-netdrive"))) {
        bool ok = BenchIsOnNetworkDrive(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-bench-mediabox"))) {
        bool ok = BenchMediabox(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-list-links"))) {
        bool ok = ListLinks(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-select-all-text"))) {
        bool ok = SelectAllText(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc == 3 && str::Eq(argv[2], StrL("-clone-after-delete"))) {
        bool ok = CloneAfterDeleteTest(Str(argv[1]));
        DestroyTempArena();
        return ok ? 0 : 1;
    }
    if (argc != 2) {
        Usage();
        return 1;
    }

    Str path(argv[1]);
    bool ok = RenderPath(path);
    DestroyTempArena();
    return ok ? 0 : 1;
}

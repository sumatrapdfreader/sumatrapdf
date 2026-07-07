#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/Timer.h"

#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineAll.h"

void _uploadDebugReport(Str, Str, bool, bool) {}

void log(Str s) {
    if (!s) {
        return;
    }
    fwrite(s.s, 1, (size_t)s.len, stderr);
}

void loga(Str s) {
    log(s);
}

struct EBookUI;
EBookUI* GetEBookUI() {
    return nullptr;
}

static void Usage() {
    printf("usage: test_engines <document-or-image-path>\n");
}

static EngineBase* CreateEngineForPath(Str path) {
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
    if (IsEngineMupdfSupportedFileType(kind)) {
        return CreateEngineMupdfFromFile(path, kind, 96, nullptr);
    }
    return nullptr;
}

static bool RenderPath(Str path) {
    EngineBase* engine = CreateEngineForPath(path);
    if (!engine) {
        printf("failed to load: %.*s\n", path.len, path.s);
        return false;
    }

    int pageCount = engine->PageCount();
    printf("pages: %d\n", pageCount);
    bool ok = true;
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        auto timeStart = TimeGet();
        RenderPageArgs args(pageNo, 1.0f, 0);
        Pixmap* pixmap = engine->RenderPage(args);
        double ms = TimeSinceInMs(timeStart);
        if (!pixmap) {
            printf("page %d: render failed %.2f ms\n", pageNo, ms);
            ok = false;
            continue;
        }
        printf("page %d: %d x %d %.2f ms\n", pageNo, pixmap->width, pixmap->height, ms);
        FreePixmap(pixmap);
    }

    engine->Release();
    return ok;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        Usage();
        return 1;
    }

    Str path(argv[1]);
    bool ok = RenderPath(path);
    DestroyTempArena();
    return ok ? 0 : 1;
}

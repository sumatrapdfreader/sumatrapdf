#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/Timer.h"

#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineAll.h"

Kind kindFileDjVu = "fileDjVu";

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

static void Usage() {
    printf("usage: test_engines <file.djvu>\n");
}

static bool RenderDjvu(Str path) {
    EngineBase* engine = CreateEngineDjvuDecFromFile(path);
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
    bool ok = RenderDjvu(path);
    DestroyTempArena();
    return ok ? 0 : 1;
}

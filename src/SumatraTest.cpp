/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "GlobalPrefs.h"
#include "wingui/UIModels.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "PdfSync.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"

#include <chm_lib.h>
#include "lzx.h"
#include "EbookBase.h"
#include "ChmFile.h"

#include "SumatraTest.h"

const char* CleanRemoteDestName(const char* destName);

static void EnsureTestGlobalPrefs() {
    // engine creation reads a few fields off gGlobalPrefs (e.g. disableAntiAlias)
    if (!gGlobalPrefs) {
        gGlobalPrefs = NewGlobalPrefs(nullptr);
    }
}

// Headless synctex forward-search test for issue #5633. Loads the pdf, builds
// the synctex index (decompressing .synctex/.synctex.gz as needed) and runs a
// SourceToDoc query, returning a machine-readable result line.
char* TestSynctexResult(const char* pdfPath, const char* srcPath, int line) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.AppendFmt("ERROR engine-create-failed pdf=%s\n", pdfPath);
    } else {
        Synchronizer* sync = nullptr;
        int err = Synchronizer::Create(pdfPath, engine, &sync);
        if (err != PDFSYNCERR_SUCCESS || !sync) {
            out.AppendFmt("ERROR sync-create-failed err=%d\n", err);
        } else {
            int page = 0;
            Vec<Rect> rects;
            int ret = sync->SourceToDoc(srcPath, line, 0, &page, rects);
            out.AppendFmt("ret=%d page=%d nrects=%d src=%s line=%d\n", ret, page, rects.Size(), srcPath, line);
            delete sync;
        }
        SafeEngineRelease(&engine);
    }

    return out.StealData();
}

// Headless case-insensitive text-search test for issue #5597. Loads the pdf,
// searches (case-insensitive) for the needle and writes the result -- the page
// it was found on (1-based) or NOTFOUND -- to the output file, then exits.
// Used by tests/issue-5597.ts; not meant for end users.
class TestPasswordUI : public PasswordUI {
    const char* password = nullptr;
    bool triedPassword = false;

  public:
    explicit TestPasswordUI(const char* password) : password(password) {}

    char* GetPassword(const char*, u8*, u8[32], bool* saveKey) override {
        *saveKey = false;
        if (triedPassword || !password) {
            return nullptr;
        }
        triedPassword = true;
        return str::Dup(password);
    }
};

char* TestSearchResult(const char* pdfPath, const char* needle, const char* password) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    TestPasswordUI pwdUI(password);
    EngineBase* engine = CreateEngineFromFile(pdfPath, password ? &pwdUI : nullptr, false);
    if (!engine) {
        out.AppendFmt("ERROR engine-create-failed pdf=%s\n", pdfPath);
    } else {
        TempWStr needleW = ToWStrTemp(needle);
        auto ts = new TextSearch(engine);
        ts->SetDirection(TextSearch::Direction::Forward);
        ts->SetMatchCase(false);
        TextSel* sel = ts->FindFirst(1, needleW);
        if (sel && sel->len > 0) {
            out.AppendFmt("FOUND needle=%s page=%d\n", needle, sel->pages[0]);
        } else {
            out.AppendFmt("NOTFOUND needle=%s\n", needle);
        }
        delete ts;
        SafeEngineRelease(&engine);
    }

    return out.StealData();
}

// walk the outline tree in document order, return the `target`-th (1-based) item
// that has a destination. `counter` tracks how many dests we've seen so far.
static IPageDestination* NthDestInToc(TocItem* item, int target, int& counter) {
    for (; item; item = item->next) {
        if (item->dest) {
            counter++;
            if (counter == target) {
                return item->dest;
            }
        }
        IPageDestination* d = NthDestInToc(item->child, target, counter);
        if (d) {
            return d;
        }
    }
    return nullptr;
}

// Headless test for PDF destination zoom resolution (issue #5537). Resolves the
// <no>-th (1-based) outline destination and returns "page=P zoom=Z". zoom is in
// SumatraPDF units (1.0 == 100%); zoom=0 means "retain current zoom" (what /XYZ
// ... 0 must map to). Used by tests/issue-5537.ts.
char* TestDestResult(const char* pdfPath, int destNo) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.AppendFmt("ERROR engine-create-failed pdf=%s\n", pdfPath);
    } else {
        TocTree* toc = engine->GetToc();
        IPageDestination* dest = nullptr;
        if (toc && toc->root) {
            int counter = 0;
            dest = NthDestInToc(toc->root, destNo, counter);
        }
        if (dest) {
            out.AppendFmt("dest=%d page=%d zoom=%g\n", destNo, PageDestGetPageNo(dest), PageDestGetZoom(dest));
        } else {
            out.AppendFmt("dest=%d NODEST\n", destNo);
        }
        SafeEngineRelease(&engine);
    }

    return out.StealData();
}

// Headless test for remote named-destination resolution (issue #5642). Loads the
// pdf and resolves <name> -- which may carry mupdf's "nameddest=" prefix, as a
// remote GoToR link's name does -- the same way LinkHandler::LaunchFile does
// (CleanRemoteDestName + GetNamedDest), returning the resolved page.
// Used by tests/issue-5642.ts.
char* TestNamedDestResult(const char* pdfPath, const char* destName) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.AppendFmt("ERROR engine-create-failed pdf=%s\n", pdfPath);
    } else {
        const char* name = CleanRemoteDestName(destName);
        IPageDestination* dest = engine->GetNamedDest(name);
        if (dest) {
            out.AppendFmt("name=%s page=%d\n", destName, PageDestGetPageNo(dest));
            delete dest;
        } else {
            out.AppendFmt("name=%s NOTFOUND\n", destName);
        }
        SafeEngineRelease(&engine);
    }

    return out.StealData();
}

static int ChmTestEnumerate(struct chmFile* h, struct chmUnitInfo* ui, void* ctx) {
    if (!ui || str::IsEmpty(ui->path)) {
        return CHM_ENUMERATOR_CONTINUE;
    }
    auto* paths = (StrVec*)ctx;
    paths->Append(ui->path);
    return CHM_ENUMERATOR_CONTINUE;
}

// Headless CHM exercise test. Runs an isolated PRETREE make_decode_table check (so
// ASan can catch the lzx.c overflow on a heap buffer), opens the chm via chm_open,
// enumerates and retrieves objects, and optionally loads ChmFile / EngineChm.
// Used by tests/issue-chm-lzx.ts; not meant for end users.
char* TestChmResult(const char* chmPath, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    bool ok = true;

    int pretreeRes = LZX_test_pretree_make_decode_table();
    if (pretreeRes == 1) {
        out.Append("pretree_isolated=REJECTED\n");
    } else {
        out.AppendFmt("pretree_isolated=UNEXPECTED_%d\n", pretreeRes);
        ok = false;
    }

    ByteSlice fileData = file::ReadFile(chmPath);
    if (!fileData) {
        out.AppendFmt("open=FAILED path=%s\n", chmPath);
        ok = false;
    } else {
        struct chmFile* h = chm_open((const char*)fileData.data(), fileData.size());
        if (!h) {
            out.AppendFmt("chm_open=FAILED path=%s\n", chmPath);
            ok = false;
        } else {
            out.Append("chm_open=OK\n");

            int retrieveOk = 0;
            int retrieveFail = 0;
            StrVec paths;
            chm_enumerate(h, CHM_ENUMERATE_ALL, ChmTestEnumerate, &paths);

            for (int i = 0; i < paths.Size(); i++) {
                const char* path = paths.At(i);
                struct chmUnitInfo ui{};
                if (chm_resolve_object(h, path, &ui) != CHM_RESOLVE_SUCCESS) {
                    continue;
                }
                if (ui.length == 0 || ui.length > 128 * 1024 * 1024) {
                    continue;
                }
                u8* buf = AllocArray<u8>((size_t)ui.length + 1);
                if (!buf) {
                    retrieveFail++;
                    continue;
                }
                int64_t got = chm_retrieve_object(h, &ui, buf, 0, (int64_t)ui.length);
                if (got == (int64_t)ui.length) {
                    retrieveOk++;
                } else {
                    retrieveFail++;
                }
                free(buf);
            }

            struct chmUnitInfo payloadUi{};
            if (chm_resolve_object(h, "/payload", &payloadUi) == CHM_RESOLVE_SUCCESS) {
                u8 payloadBuf[16]{};
                int64_t got = chm_retrieve_object(h, &payloadUi, payloadBuf, 0, 1);
                if (got > 0) {
                    out.Append("payload_retrieve=ATTEMPTED\n");
                } else {
                    out.Append("payload_retrieve=FAILED\n");
                }
            } else {
                out.Append("payload_retrieve=NOTFOUND\n");
            }

            out.AppendFmt("paths=%d retrieve_ok=%d retrieve_fail=%d\n", paths.Size(), retrieveOk, retrieveFail);
            chm_close(h);
        }
    }

    ChmFile* doc = ChmFile::CreateFromFile(chmPath);
    if (doc) {
        out.Append("chmfile=OK\n");
        StrVec allPaths;
        doc->GetAllPaths(&allPaths);
        out.AppendFmt("chmfile_paths=%d\n", allPaths.Size());
        if (doc->HasToc()) {
            out.Append("chmfile_toc=YES\n");
        }
        delete doc;
    } else {
        out.Append("chmfile=FAILED\n");
    }

    EngineBase* engine = CreateEngineChmFromFile(chmPath);
    if (engine) {
        out.AppendFmt("engine=OK pages=%d\n", engine->PageCount());
        SafeEngineRelease(&engine);
    } else {
        out.Append("engine=FAILED\n");
    }

    if (ok) {
        out.Append("result=OK\n");
    } else {
        out.Append("result=FAILED\n");
    }

    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return out.StealData();
}

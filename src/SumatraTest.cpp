/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "wingui/UIModels.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "PdfSync.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "SearchAndDDE.h"
#include "ReadAloudHighlight.h"

#include <chm_lib.h>
#include "lzx.h"
#include "EbookBase.h"
#include "ChmFile.h"

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

static bool FindWordCenter(EngineBase* engine, int pageNo, const char* word, double* xOut, double* yOut);

// Regression test for issue #5718: opening the context menu over text used to
// corrupt an existing selection because ReadAloudCanReadFromCursor() (called
// while building the menu) mutated the live TextSelection's start glyph. As a
// result "Copy Selection" copied from the old selection end to the cursor
// instead of the selected text. Operates on the document loaded into the first
// window (passed on the command line), so it exercises the real menu code path.
char* TestContextMenuSelectionResult(const char* word1, const char* word2, const char* cursorWord, int* exitCodeOut) {
    StrBuilder out;
    auto fail = [&](const char* msg) -> char* {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return out.StealData();
    };

    if (str::IsEmptyOrWhiteSpace(word1) || str::IsEmptyOrWhiteSpace(word2) || str::IsEmptyOrWhiteSpace(cursorWord)) {
        return fail("ERROR missing word1, word2 or cursorWord");
    }
    if (gWindows.IsEmpty()) {
        return fail("NOTREADY no-window");
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail("NOTREADY no-doc");
    }
    EngineBase* engine = dm->GetEngine();
    const int pageNo = 1;
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0, xc = 0, yc = 0;
    if (!FindWordCenter(engine, pageNo, word1, &x1, &y1)) {
        return fail("ERROR word1-not-found");
    }
    if (!FindWordCenter(engine, pageNo, word2, &x2, &y2)) {
        return fail("ERROR word2-not-found");
    }
    if (!FindWordCenter(engine, pageNo, cursorWord, &xc, &yc)) {
        return fail("ERROR cursorWord-not-found");
    }

    // build a text selection spanning word1..word2, like a left-drag would
    dm->textSelection->StartAt(pageNo, x1, y1);
    dm->textSelection->SelectUpTo(pageNo, x2, y2);
    WindowTab* tab = win->CurrentTab();
    DeleteOldSelectionInfo(win);
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = tab->selectionOnPage != nullptr;

    bool isTextOnly = false;
    TempStr original = GetSelectedTextTemp(tab, " ", isTextOnly);
    if (str::IsEmpty(original)) {
        return fail("ERROR empty-selection");
    }
    original = str::DupTemp(original);

    // simulate opening the context menu over cursorWord: this is the read-only
    // check the menu performs; it must not change the selection
    Point screenPt = dm->CvtToScreen(pageNo, PointF(xc, yc));
    ReadAloudCanReadFromCursor(dm, screenPt);

    TempStr after = GetSelectedTextTemp(tab, " ", isTextOnly);
    bool ok = str::Eq(original, after);
    if (ok) {
        out.AppendFmt("OK selected=%s\n", original);
    } else {
        out.AppendFmt("FAIL original=%s after=%s\n", original, after);
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return out.StealData();
}

// find the [start, end) glyph range of the first occurrence of `word` on a page
static bool FindWordGlyphRange(EngineBase* engine, int pageNo, const char* word, int* startOut, int* endOut) {
    if (!engine || !word || !startOut || !endOut) {
        return false;
    }
    int textLen = 0;
    const WCHAR* text = engine->GetTextForPage(pageNo, &textLen);
    if (!text || textLen <= 0) {
        return false;
    }
    AutoFreeWStr wordW = ToWStr(word);
    int wordLen = (int)str::Len(wordW);
    if (wordLen <= 0) {
        return false;
    }
    for (int i = 0; i <= textLen - wordLen; i++) {
        if (memcmp(text + i, wordW, (size_t)wordLen * sizeof(WCHAR)) == 0) {
            *startOut = i;
            *endOut = i + wordLen;
            return true;
        }
    }
    return false;
}

// Regression test for the find-results crash/assert: picking a match from the
// floating results list (GoToFindMatch) used to call SetLastResult() before
// ShowSearchResult(). SetLastResult()->SetText() clears textSearch->result
// whenever the matched text differs from the typed search text (e.g. a
// case-insensitive find where "the" matches "The"), so ShowSearchResult() then
// got an empty result (result->len == 0), tripped a ReportIf, and failed to
// navigate/select the match. Operates on the document loaded into the first
// window. `word` is the (case-different) matched text in the document and
// `typed` is the lowercase search text the user typed.
char* TestGoToFindMatchResult(const char* word, const char* typed, int* exitCodeOut) {
    StrBuilder out;
    auto fail = [&](const char* msg) -> char* {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return out.StealData();
    };

    if (str::IsEmptyOrWhiteSpace(word) || str::IsEmptyOrWhiteSpace(typed)) {
        return fail("ERROR missing word or typed");
    }
    if (gWindows.IsEmpty()) {
        return fail("NOTREADY no-window");
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail("NOTREADY no-doc");
    }
    EngineBase* engine = dm->GetEngine();
    const int pageNo = 1;
    int startGlyph = 0, endGlyph = 0;
    if (!FindWordGlyphRange(engine, pageNo, word, &startGlyph, &endGlyph)) {
        return fail("ERROR word-not-found");
    }

    // mimic a prior find: the typed (lowercase) text becomes textSearch's
    // lastText, so SetLastResult() inside GoToFindMatch() sees a text change
    AutoFreeWStr typedW = ToWStr(typed);
    dm->textSearch->SetText(typedW);

    // start with no selection so we can tell whether GoToFindMatch selected the match
    DeleteOldSelectionInfo(win, true);

    GoToFindMatch(win, pageNo, startGlyph, pageNo, endGlyph);

    bool isTextOnly = false;
    TempStr selected = GetSelectedTextTemp(win->CurrentTab(), " ", isTextOnly);
    bool ok = str::Eq(selected, word);
    if (ok) {
        out.AppendFmt("OK selected=%s\n", selected);
    } else {
        out.AppendFmt("FAIL expected=%s selected=%s\n", word, selected ? selected : "(none)");
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return out.StealData();
}

static bool FindWordCenter(EngineBase* engine, int pageNo, const char* word, double* xOut, double* yOut) {
    if (!engine || !word || !xOut || !yOut) {
        return false;
    }
    int textLen = 0;
    Rect* coords = nullptr;
    const WCHAR* text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (!text || textLen <= 0) {
        return false;
    }
    AutoFreeWStr wordW = ToWStr(word);
    int wordLen = (int)str::Len(wordW);
    if (wordLen <= 0) {
        return false;
    }
    for (int i = 0; i <= textLen - wordLen; i++) {
        if (memcmp(text + i, wordW, (size_t)wordLen * sizeof(WCHAR)) != 0) {
            continue;
        }
        int mid = i + wordLen / 2;
        for (; mid < textLen && !coords[mid].x && !coords[mid].dx; mid++) {
            if (text[mid] == '\n') {
                return false;
            }
        }
        if (mid >= textLen) {
            return false;
        }
        *xOut = coords[mid].x + coords[mid].dx / 2.0;
        *yOut = coords[mid].y + coords[mid].dy / 2.0;
        return true;
    }
    return false;
}

static TempStr ExtractSelectionTextTemp(TextSelection& ts) {
    WCHAR* ws = ts.ExtractText(" ");
    TempStr res = ToUtf8Temp(ws);
    str::Free(ws);
    return res;
}

// Headless triple-click line-selection test (issue #5712). Loads the pdf, clicks
// the middle of <clickWord>, runs the same TextSelection steps as a double-click
// followed by a triple-click (without the mouse-up trim), and checks the result.
char* TestTripleClickLineSelectResult(const char* pdfPath, const char* clickWord, const char* expectedLine,
                                      int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    if (str::IsEmptyOrWhiteSpace(pdfPath) || str::IsEmptyOrWhiteSpace(clickWord) ||
        str::IsEmptyOrWhiteSpace(expectedLine)) {
        out.Append("ERROR missing pdf, clickWord, or expectedLine\n");
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return out.StealData();
    }

    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.AppendFmt("ERROR engine-create-failed pdf=%s\n", pdfPath);
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return out.StealData();
    }

    const int pageNo = 1;
    double x = 0;
    double y = 0;
    if (!FindWordCenter(engine, pageNo, clickWord, &x, &y)) {
        out.AppendFmt("ERROR word-not-found word=%s\n", clickWord);
        SafeEngineRelease(&engine);
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return out.StealData();
    }

    TextSelection ts(engine);
    ts.SelectWordAt(pageNo, x, y);
    ts.SelectLineAt(pageNo, x, y);
    TempStr selected = ExtractSelectionTextTemp(ts);

    // simulate the old mouse-up bug: re-selecting to the click point trims the line
    TextSelection trimmed(engine);
    trimmed.SelectWordAt(pageNo, x, y);
    trimmed.SelectLineAt(pageNo, x, y);
    trimmed.SelectUpTo(pageNo, x, y);
    TempStr trimmedText = ExtractSelectionTextTemp(trimmed);
    if (str::Eq(trimmedText, expectedLine)) {
        out.AppendFmt("ERROR trim-check-failed trimmed=%s\n", trimmedText);
        SafeEngineRelease(&engine);
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return out.StealData();
    }

    bool ok = str::Eq(selected, expectedLine);
    if (ok) {
        out.AppendFmt("OK selected=%s\n", selected);
    } else {
        out.AppendFmt("FAIL selected=%s expected=%s\n", selected, expectedLine);
    }

    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return out.StealData();
}

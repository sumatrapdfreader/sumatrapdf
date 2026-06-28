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
#include "Translations.h"

#include <chm_lib.h>
#include "lzx.h"
#include "EbookBase.h"
#include "ChmFile.h"

Str CleanRemoteDestName(Str destName);

static void EnsureTestGlobalPrefs() {
    // engine creation reads a few fields off gGlobalPrefs (e.g. disableAntiAlias)
    if (!gGlobalPrefs) {
        gGlobalPrefs = NewGlobalPrefs(nullptr);
    }
    // Headless -dbg-control tests don't need form JavaScript. Force it off even
    // when LoadSettings() already ran (the test harness overrides user prefs).
    EngineMupdfSetDisableJavaScript(true);
}

// Headless synctex forward-search test for issue #5633. Loads the pdf, builds
// the synctex index (decompressing .synctex/.synctex.gz as needed) and runs a
// SourceToDoc query, returning a machine-readable result line.
Str TestSynctexResult(Str pdfPath, Str srcPath, int line) {
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
            out.AppendFmt("ret=%d page=%d nrects=%d src=%s line=%d", ret, page, rects.Size(), srcPath, line);
            if (rects.Size() > 0) {
                Rect r = rects.at(0);
                out.AppendFmt(" rect_x=%d rect_y=%d rect_dx=%d rect_dy=%d", r.x, r.y, r.dx, r.dy);
            }
            out.Append("\n");
            delete sync;
        }
        SafeEngineRelease(&engine);
    }

    return out.StealData();
}

// Headless inverse-search test for issue #5702. Loads the pdf, creates a
// Synchronizer, and resolves (page, point) -> (srcfile, line, col) via
// DocToSource, returning a machine-readable result line.
Str TestInverseSearchResult(Str pdfPath, int pageNo, int x, int y) {
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
            AutoFreeStr srcfilepath;
            int line = 0, col = 0;
            Point pt(x, y);
            int ret = sync->DocToSource(pageNo, pt, srcfilepath, &line, &col);
            if (ret != PDFSYNCERR_SUCCESS) {
                out.AppendFmt("ERROR doctosource-failed err=%d\n", ret);
            } else {
                out.AppendFmt("ret=%d srcfile=%s line=%d col=%d\n", ret, srcfilepath.Get(), line, col);
            }
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
    Str password = nullptr;
    bool triedPassword = false;

  public:
    explicit TestPasswordUI(Str password) : password(password) {}

    Str GetPassword(Str, u8*, u8[32], bool* saveKey) override {
        *saveKey = false;
        if (triedPassword || !password) {
            return nullptr;
        }
        triedPassword = true;
        return Str(str::Dup(password));
    }
};

Str TestSearchResult(Str pdfPath, Str needle, Str password) {
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
Str TestDestResult(Str pdfPath, int destNo) {
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
Str TestNamedDestResult(Str pdfPath, Str destName) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.AppendFmt("ERROR engine-create-failed pdf=%s\n", pdfPath);
    } else {
        Str name = CleanRemoteDestName(destName);
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
Str TestChmResult(Str chmPath, int* exitCodeOut) {
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
                Str path = paths.At(i);
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

static bool FindWordCenter(EngineBase* engine, int pageNo, Str word, double* xOut, double* yOut);

// Regression test for issue #5718: opening the context menu over text used to
// corrupt an existing selection because ReadAloudCanReadFromCursor() (called
// while building the menu) mutated the live TextSelection's start glyph. As a
// result "Copy Selection" copied from the old selection end to the cursor
// instead of the selected text. Operates on the document loaded into the first
// window (passed on the command line), so it exercises the real menu code path.
Str TestContextMenuSelectionResult(Str word1, Str word2, Str cursorWord, int* exitCodeOut) {
    StrBuilder out;
    auto fail = [&](Str msg) -> Str {
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
static bool FindWordGlyphRange(EngineBase* engine, int pageNo, Str word, int* startOut, int* endOut) {
    if (!engine || !word || !startOut || !endOut) {
        return false;
    }
    WStr text = engine->GetTextForPage(pageNo);
    if (!text) {
        return false;
    }
    AutoFreeWStr wordW;
    wordW.SetCopy(ToWStr(word).s);
    int wordLen = (int)wordW.size();
    if (wordLen <= 0) {
        return false;
    }
    for (int i = 0; i <= text.len - wordLen; i++) {
        if (memcmp(text.s + i, wordW, (size_t)wordLen * sizeof(WCHAR)) == 0) {
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
// navigate to the match. Operates on the document loaded into the first window.
// `word` is the (case-different) matched text in the document and `typed` is
// the lowercase search text the user typed. Since issue #5737 find no longer
// sets a text selection (matches are highlighted by PaintAllFindMatches), so we
// verify navigation: the picked match becomes textSearch's current position and
// is scrolled into view.
Str TestGoToFindMatchResult(Str word, Str typed, int* exitCodeOut) {
    StrBuilder out;
    auto fail = [&](Str msg) -> Str {
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
    // locate `word` on whichever page holds it (the test PDF puts it on a later
    // page so the initial view doesn't already show it -- navigating to it is
    // then observable)
    int pageNo = 0;
    int startGlyph = 0, endGlyph = 0;
    for (int p = 1; p <= engine->PageCount(); p++) {
        if (FindWordGlyphRange(engine, p, word, &startGlyph, &endGlyph)) {
            pageNo = p;
            break;
        }
    }
    if (pageNo == 0) {
        return fail("ERROR word-not-found");
    }

    // mimic a prior find: the typed (lowercase) text becomes textSearch's
    // lastText, so SetLastResult() inside GoToFindMatch() sees a text change
    AutoFreeWStr typedW;
    typedW.SetCopy(ToWStr(typed).s);
    dm->textSearch->SetText(WStr(typedW.Get()));

    // make sure the match isn't already on screen, so navigating to it is
    // observable: scroll back to the first page and clear any selection
    win->ctrl->GoToPage(1, false);
    DeleteOldSelectionInfo(win, true);

    GoToFindMatch(win, pageNo, startGlyph, pageNo, endGlyph);

    // Find no longer creates a text selection (issue #5737): all matches,
    // including the active one, are highlighted by PaintAllFindMatches instead.
    // The regression we guard is that GoToFindMatch *navigates* to the picked
    // match and records it as textSearch's current result position (which Find
    // Next/Prev and the n/m counter continue from). The old bug cleared the
    // result before ShowSearchResult ran, so it never scrolled to the match.
    // Verify both: the match was recorded as the current position (start/end
    // glyph range maps back to `word`), and it was scrolled into the viewport.
    TextSearch* ts = dm->textSearch;
    int curPage = ts->startPage;
    int curStart = ts->startGlyph;
    int curEnd = ts->endGlyph;

    TempStr matched = nullptr;
    Rect* coords = nullptr;
    WStr pageTxt = engine->GetTextForPage(pageNo, nullptr, &coords);
    if (pageTxt && coords && curPage == pageNo && curStart >= 0 && curEnd <= pageTxt.len && curStart < curEnd) {
        AutoFreeWStr w(str::Dup(WStr(pageTxt.s + curStart, (int)(curEnd - curStart))).s);
        matched = ToUtf8Temp(WStr(w.Get()));
    }

    // is the match rect actually within the visible viewport now? (mirrors
    // DisplayModel::ScrollScreenToRect's own visibility test)
    bool visible = false;
    if (coords && curPage == pageNo && curStart >= 0 && curEnd <= pageTxt.len && curStart < curEnd) {
        Rect pr = coords[curStart];
        for (int i = curStart + 1; i < curEnd; i++) {
            pr = pr.Union(coords[i]);
        }
        Rect sr = dm->CvtToScreen(pageNo, ToRectF(pr));
        Rect vp = Rect(Point(), dm->viewPort.Size());
        visible = !vp.Intersect(sr).IsEmpty();
    }

    bool matchOk = (curPage == pageNo) && (curStart == startGlyph) && (curEnd == endGlyph) && str::Eq(matched, word);
    bool ok = matchOk && visible;
    if (ok) {
        out.AppendFmt("OK match=%s page=%d visible=1\n", matched, pageNo);
    } else {
        out.AppendFmt("FAIL expected=%s match=%s page=%d visible=%d\n", word, matched ? matched.s : "(none)", pageNo,
                      visible ? 1 : 0);
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return out.StealData();
}

static bool FindWordCenter(EngineBase* engine, int pageNo, Str word, double* xOut, double* yOut) {
    if (!engine || !word || !xOut || !yOut) {
        return false;
    }
    Rect* coords = nullptr;
    WStr text = engine->GetTextForPage(pageNo, nullptr, &coords);
    if (!text) {
        return false;
    }
    AutoFreeWStr wordW;
    wordW.SetCopy(ToWStr(word).s);
    int wordLen = (int)wordW.size();
    if (wordLen <= 0) {
        return false;
    }
    for (int i = 0; i <= text.len - wordLen; i++) {
        if (memcmp(text.s + i, wordW, (size_t)wordLen * sizeof(WCHAR)) != 0) {
            continue;
        }
        int mid = i + wordLen / 2;
        for (; mid < text.len && !coords[mid].x && !coords[mid].dx; mid++) {
            if (text.s[mid] == L'\n') {
                return false;
            }
        }
        if (mid >= text.len) {
            return false;
        }
        *xOut = coords[mid].x + coords[mid].dx / 2.0;
        *yOut = coords[mid].y + coords[mid].dy / 2.0;
        return true;
    }
    return false;
}

static TempStr ExtractSelectionTextTemp(TextSelection& ts) {
    WStr ws = ts.ExtractText(" ");
    TempStr res = ToUtf8Temp(ws);
    str::Free(ws.s);
    return res;
}

// Headless triple-click line-selection test (issue #5712). Loads the pdf, clicks
// the middle of <clickWord>, runs the same TextSelection steps as a double-click
// followed by a triple-click (without the mouse-up trim), and checks the result.
Str TestTripleClickLineSelectResult(Str pdfPath, Str clickWord, Str expectedLine, int* exitCodeOut) {
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

static IPageDestination* FirstLinkDestOnPage(EngineBase* engine, int pageNo) {
    if (!engine) {
        return nullptr;
    }
    Vec<IPageElement*> els = engine->GetElements(pageNo);
    for (IPageElement* el : els) {
        if (!el || !el->Is(kindPageElementDest)) {
            continue;
        }
        IPageDestination* dest = el->AsLink();
        if (dest) {
            return dest;
        }
    }
    return nullptr;
}

// Follow the first internal link on page 1 after pinning the viewport to the
// left; used by tests/issue-5064.ts (issue #5064).
Str TestScrollToLinkResult(int minViewportDelta, int* exitCodeOut) {
    StrBuilder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return out.StealData();
    };

    if (gWindows.IsEmpty()) {
        return fail("NOTREADY no-window");
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail("NOTREADY no-doc");
    }

    dm->SetZoomVirtual(200, nullptr);
    dm->Relayout(200, dm->rotation);
    dm->viewPort.x = 0;
    dm->RecalcVisibleParts();
    dm->RenderVisibleParts();

    int before = dm->viewPort.x;
    IPageDestination* dest = FirstLinkDestOnPage(dm->GetEngine(), 1);
    if (!dest) {
        return fail("ERROR no-link");
    }

    win->ctrl->HandleLink(dest, win->linkHandler);

    int after = dm->viewPort.x;
    int delta = after - before;
    bool ok = delta >= minViewportDelta;
    if (ok) {
        out.AppendFmt("OK viewport_before=%d viewport_after=%d delta=%d\n", before, after, delta);
    } else {
        out.AppendFmt("FAIL viewport_before=%d viewport_after=%d delta=%d min=%d\n", before, after, delta,
                      minViewportDelta);
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return out.StealData();
}

// Verifies _TRA resolves error-path strings through the translation table.
Str TestI18nErrorStringResult(int* exitCodeOut) {
    StrBuilder out;
    Str err = _TRA("Error");
    Str crash = _TRA("SumatraPDF crashed");
    Str printers = _TRA("SumatraPDF - Show Printers");
    bool ok = !str::IsEmpty(err) && !str::IsEmpty(crash) && !str::IsEmpty(printers) &&
              str::Eq(err, trans::GetTranslation("Error")) &&
              str::Eq(crash, trans::GetTranslation("SumatraPDF crashed")) &&
              str::Eq(printers, trans::GetTranslation("SumatraPDF - Show Printers"));
    if (ok) {
        out.AppendFmt("OK error=%s crash=%s printers=%s\n", err, crash, printers);
    } else {
        out.AppendFmt("FAIL error=%s crash=%s printers=%s\n", err ? err.s : "(null)", crash ? crash.s : "(null)",
                      printers ? printers.s : "(null)");
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return out.StealData();
}

static void AppendTocItems(StrBuilder& out, TocItem* item) {
    for (; item; item = item->next) {
        if (item->title) {
            out.AppendFmt("%s|page=%d\n", item->title, item->pageNo);
        }
        AppendTocItems(out, item->child);
    }
}

// Headless test for document TOC (e.g. ComicInfo.xml bookmarks in CBZ). Returns
// one line per top-level TOC entry: "title|page=N". Used by tests/issue-1201.ts.
Str TestGetTocResult(Str path, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.AppendFmt("ERROR engine-create-failed path=%s\n", path);
    } else {
        TocTree* toc = engine->GetToc();
        if (!toc || !toc->root || !toc->root->child) {
            if (exitCodeOut) {
                *exitCodeOut = 1;
            }
            out.Append("ERROR no-toc\n");
        } else {
            if (exitCodeOut) {
                *exitCodeOut = 0;
            }
            AppendTocItems(out, toc->root->child);
        }
        SafeEngineRelease(&engine);
    }
    return out.StealData();
}

// Headless test for page link elements. Returns one line per link:
// "kind=<kind> value=<value>". Used by tests/ad-hoc-md-links.ts.
Str TestPageLinksResult(Str path, int pageNo, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    StrBuilder out;
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.AppendFmt("ERROR engine-create-failed path=%s\n", path);
        return out.StealData();
    }

    if (!engine->BenchLoadPage(pageNo)) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.AppendFmt("ERROR page-load-failed page=%d\n", pageNo);
        SafeEngineRelease(&engine);
        return out.StealData();
    }

    int nLinks = 0;
    Vec<IPageElement*> els = engine->GetElements(pageNo);
    for (IPageElement* el : els) {
        if (!el || !el->Is(kindPageElementDest)) {
            continue;
        }
        IPageDestination* dest = el->AsLink();
        if (!dest) {
            continue;
        }
        nLinks++;
        Str value = PageDestGetValue(dest);
        out.AppendFmt("kind=%s value=%s\n", dest->GetKind(), value ? value.s : "");
    }
    if (nLinks == 0) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.AppendFmt("ERROR no-links page=%d\n", pageNo);
    } else if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    SafeEngineRelease(&engine);
    return out.StealData();
}

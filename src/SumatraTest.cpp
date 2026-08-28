/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/ByteReaderWriter.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "Settings.h"
#include "GlobalPrefs.h"
#include "gui/UIModels.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "ImageReader.h"
#include "ImageSaveCropResize.h"
#include "PdfCreator.h"
#include "PdfCadDetect.h"
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
#include "MarkdownModel.h"
#include "TableOfContents.h"
#include "gui/win/BrowserDocView.h"

#include <chm.h>
#include "EbookBase.h"
#include "ChmFile.h"
#include "SumatraTest.h"

// internal LZX test hook, defined in chm.c but not exposed in chm.h
extern "C" int LZX_test_pretree_make_decode_table(void);

static void EnsureTestGlobalPrefs() {
    // engine creation reads a few fields off gSettings (e.g. disableAntiAlias)
    if (!gSettings) {
        gSettings = NewGlobalPrefs({});
    }
    // Headless -dbg-control tests don't need form JavaScript. Force it off even
    // when LoadSettings() already ran (the test harness overrides user prefs).
    EngineMupdfSetDisableJavaScript(true);
}

// Headless synctex forward-search test for issue #5633. Loads the pdf, builds
// the synctex index (decompressing .synctex/.synctex.gz as needed) and runs a
// SourceToDoc query, returning a machine-readable result line.
TempStr SynctexResultTemp(Str pdfPath, Str srcPath, int line) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        Synchronizer* sync = nullptr;
        int err = Synchronizer::Create(pdfPath, engine, &sync);
        if (err != PDFSYNCERR_SUCCESS || !sync) {
            out.Append(fmt("ERROR sync-create-failed err=%d\n", err));
        } else {
            int page = 0;
            Vec<Rect> rects;
            int ret = sync->SourceToDoc(srcPath, line, 0, &page, rects);
            out.Append(fmt("ret=%d page=%d nrects=%d src=%s line=%d", ret, page, len(rects), srcPath, line));
            if (len(rects) > 0) {
                Rect r = rects[0];
                out.Append(fmt(" rect_x=%d rect_y=%d rect_dx=%d rect_dy=%d", r.x, r.y, r.dx, r.dy));
            }
            out.Append(StrL("\n"));
            delete sync;
        }
        SafeEngineRelease(&engine);
    }

    return ToStrTemp(out);
}

// Headless inverse-search test for issue #5702. Loads the pdf, creates a
// Synchronizer, and resolves (page, point) -> (srcfile, line, col) via
// DocToSource, returning a machine-readable result line.
TempStr InverseSearchResultTemp(Str pdfPath, int pageNo, int x, int y) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        Synchronizer* sync = nullptr;
        int err = Synchronizer::Create(pdfPath, engine, &sync);
        if (err != PDFSYNCERR_SUCCESS || !sync) {
            out.Append(fmt("ERROR sync-create-failed err=%d\n", err));
        } else {
            Str srcfilepath;
            int line = 0, col = 0;
            Point pt(x, y);
            int ret = sync->DocToSource(pageNo, pt, srcfilepath, &line, &col);
            if (ret != PDFSYNCERR_SUCCESS) {
                out.Append(fmt("ERROR doctosource-failed err=%d\n", ret));
            } else {
                out.Append(fmt("ret=%d srcfile=%s line=%d col=%d\n", ret, srcfilepath, line, col));
            }
            str::Free(srcfilepath);
            delete sync;
        }
        SafeEngineRelease(&engine);
    }

    return ToStrTemp(out);
}

// Headless case-insensitive text-search test for issue #5597. Loads the pdf,
// searches (case-insensitive) for the needle and writes the result -- the page
// it was found on (1-based) or NOTFOUND -- to the output file, then exits.
// Used by tests/issue-5597.ts; not meant for end users.
class TestPasswordUI : public PasswordUI {
    Str password;
    bool triedPassword = false;

  public:
    explicit TestPasswordUI(Str password) : password(password) {}

    Str GetPassword(Str /*path*/, u8* /*fileDigest*/, u8 /*decryptionKeyOut*/[32], bool* saveKey) override {
        *saveKey = false;
        if (triedPassword || !password) {
            return {};
        }
        triedPassword = true;
        return str::Dup(password);
    }
};

TempStr SearchResultTemp(Str pdfPath, Str needle, Str password) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    TestPasswordUI pwdUI(password);
    EngineBase* engine = CreateEngineFromFile(pdfPath, password ? &pwdUI : nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        auto* ts = new TextSearch(engine);
        ts->SetDirection(TextSearch::Direction::Forward);
        ts->SetMatchCase(false);
        TextSel* sel = ts->FindFirst(1, needle);
        if (sel && sel->len > 0) {
            out.Append(fmt("FOUND needle=%s page=%d\n", needle, sel->pages[0]));
        } else {
            out.Append(fmt("NOTFOUND needle=%s\n", needle));
        }
        delete ts;
        SafeEngineRelease(&engine);
    }

    return ToStrTemp(out);
}

// Headless search restricted to pages first..last (0 = unbounded). Reports
// every match page in document order. Used by tests/issue-5694.ts.
TempStr FindPageRangeResultTemp(Str pdfPath, Str needle, int first, int last, Str spec, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
        return finish(1);
    }
    auto* ts = new TextSearch(engine);
    ts->SetDirection(TextSearch::Direction::Forward);
    ts->SetMatchCase(false);
    if (spec) {
        Vec<bool> allowed;
        if (!ParseFindPageRange(spec, engine->PageCount(), allowed)) {
            allowed.Reset();
        }
        ts->SetAllowedPages(allowed);
    } else {
        ts->SetPageRange(first, last);
    }
    int n = 0;
    TextSel* sel = ts->FindFirst(ts->RestrictFirst(), needle);
    while (sel && sel->len > 0) {
        out.Append(fmt("page=%d\n", sel->pages[0]));
        n++;
        sel = ts->FindNext();
    }
    if (n == 0) {
        out.Append(fmt("NOTFOUND needle=%s first=%d last=%d\n", needle, first, last));
    }
    delete ts;
    SafeEngineRelease(&engine);
    return finish(0);
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

static TocItem* NthTocItemWithDest(TocItem* item, int target, int& counter) {
    for (; item; item = item->next) {
        if (item->dest) {
            counter++;
            if (counter == target) {
                return item;
            }
        }
        TocItem* found = NthTocItemWithDest(item->child, target, counter);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

// Headless test for PDF destination zoom resolution (issue #5537). Resolves the
// <no>-th (1-based) outline destination and returns "page=P zoom=Z". zoom is in
// SumatraPDF units (1.0 == 100%); zoom=0 means "retain current zoom" (what /XYZ
// ... 0 must map to). Used by tests/issue-5537.ts.
TempStr DestResultTemp(Str pdfPath, int destNo) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        TocTree* toc = engine->GetToc();
        IPageDestination* dest = nullptr;
        if (toc && toc->root) {
            int counter = 0;
            dest = NthDestInToc(toc->root, destNo, counter);
        }
        if (dest) {
            out.Append(fmt("dest=%d page=%d zoom=%g\n", destNo, PageDestGetPageNo(dest), PageDestGetZoom(dest)));
        } else {
            out.Append(fmt("dest=%d NODEST\n", destNo));
        }
        SafeEngineRelease(&engine);
    }

    return ToStrTemp(out);
}

// Headless test for remote named-destination resolution (issue #5642). Loads the
// pdf and resolves <name> -- which may carry mupdf's "nameddest=" prefix, as a
// remote GoToR link's name does -- the same way LinkHandler::LaunchFile does
// (CleanRemoteDestName + GetNamedDest), returning the resolved page.
// Used by tests/issue-5642.ts.
TempStr NamedDestResultTemp(Str pdfPath, Str destName) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        Str name = CleanRemoteDestName(destName);
        IPageDestination* dest = engine->GetNamedDest(name);
        if (dest) {
            out.Append(fmt("name=%s page=%d\n", destName, PageDestGetPageNo(dest)));
            delete dest;
        } else {
            out.Append(fmt("name=%s NOTFOUND\n", destName));
        }
        SafeEngineRelease(&engine);
    }

    return ToStrTemp(out);
}

// Headless CHM exercise test. Runs an isolated PRETREE make_decode_table check (so
// ASan can catch the lzx overflow on a heap buffer), opens the chm via chm_open,
// reads every entry, and optionally loads ChmFile / EngineChm.
// Used by tests/issue-chm-lzx.ts; not meant for end users.
TempStr ChmResultTemp(Str chmPath, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    bool ok = true;

    int pretreeRes = LZX_test_pretree_make_decode_table();
    if (pretreeRes == 1) {
        out.Append(StrL("pretree_isolated=REJECTED\n"));
    } else {
        out.Append(fmt("pretree_isolated=UNEXPECTED_%d\n", pretreeRes));
        ok = false;
    }

    Str fileData = file::ReadFile(chmPath);
    if (!fileData) {
        out.Append(fmt("open=FAILED path=%s\n", chmPath));
        ok = false;
    } else {
        chm_ctx* h = chm_ctx_new(nullptr, nullptr, nullptr, nullptr);
        if (!h || !chm_open(h, (const u8*)fileData.s, (size_t)fileData.len)) {
            out.Append(fmt("chm_open=FAILED path=%s\n", chmPath));
            ok = false;
            chm_ctx_free(h);
        } else {
            out.Append(StrL("chm_open=OK\n"));

            int retrieveOk = 0;
            int retrieveFail = 0;
            chm_entry** entries = nullptr;
            int nEntries = chm_get_entries(h, &entries);
            struct chm_entry* payloadEntry = nullptr;

            for (int i = 0; i < nEntries; i++) {
                chm_entry* e = entries[i];
                if (e->path && str::Eq(Str(e->path), StrL("/payload"))) {
                    payloadEntry = e;
                }
                if (e->length == 0 || e->length > 128ULL * 1024 * 1024) {
                    continue;
                }
                u8* buf = AllocArray<u8>((int)e->length + 1);
                if (!buf) {
                    retrieveFail++;
                    continue;
                }
                int64_t got = chm_read_entry(h, e, buf);
                if (got == (int64_t)e->length) {
                    retrieveOk++;
                } else {
                    retrieveFail++;
                }
                free(buf);
            }

            if (payloadEntry && payloadEntry->length > 0 && payloadEntry->length <= 128ULL * 1024 * 1024) {
                // chm_read_entry reads the whole entry, so the buffer must be
                // at least entry->length bytes; this decompresses /payload and
                // lets ASan catch the LZX overflow (issue-chm-lzx)
                u8* payloadBuf = AllocArray<u8>((int)payloadEntry->length);
                int64_t got = payloadBuf ? chm_read_entry(h, payloadEntry, payloadBuf) : 0;
                out.Append(Str(got > 0 ? "payload_retrieve=ATTEMPTED\n" : "payload_retrieve=FAILED\n"));
                free(payloadBuf);
            } else if (payloadEntry) {
                out.Append(StrL("payload_retrieve=FAILED\n"));
            } else {
                out.Append(StrL("payload_retrieve=NOTFOUND\n"));
            }

            out.Append(fmt("paths=%d retrieve_ok=%d retrieve_fail=%d\n", nEntries, retrieveOk, retrieveFail));
            chm_ctx_free(h);
        }
    }

    ChmFile* doc = ChmFile::CreateFromFile(chmPath);
    if (doc) {
        out.Append(StrL("chmfile=OK\n"));
        StrVec allPaths;
        doc->GetAllPaths(&allPaths);
        out.Append(fmt("chmfile_paths=%d\n", len(allPaths)));
        if (doc->HasToc()) {
            out.Append(StrL("chmfile_toc=YES\n"));
        }
        delete doc;
    } else {
        out.Append(StrL("chmfile=FAILED\n"));
    }

    EngineBase* engine = CreateEngineChmFromFile(chmPath);
    if (engine) {
        out.Append(fmt("engine=OK pages=%d\n", engine->PageCount()));
        SafeEngineRelease(&engine);
    } else {
        out.Append(StrL("engine=FAILED\n"));
    }

    if (ok) {
        out.Append(StrL("result=OK\n"));
    } else {
        out.Append(StrL("result=FAILED\n"));
    }

    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

static bool FindWordCenter(EngineBase* engine, int pageNo, Str word, double* xOut, double* yOut);

// Regression test for issue #5718: opening the context menu over text used to
// corrupt an existing selection because ReadAloudCanReadFromCursor() (called
// while building the menu) mutated the live TextSelection's start glyph. As a
// result "Copy Selection" copied from the old selection end to the cursor
// instead of the selected text. Operates on the document loaded into the first
// window (passed on the command line), so it exercises the real menu code path.
TempStr ContextMenuSelectionResultTemp(Str word1, Str word2, Str cursorWord, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word1) || str::IsEmptyOrWhiteSpace(word2) || str::IsEmptyOrWhiteSpace(cursorWord)) {
        return fail(StrL("ERROR missing word1, word2 or cursorWord"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }
    EngineBase* engine = dm->GetEngine();
    const int pageNo = 1;
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0, xc = 0, yc = 0;
    if (!FindWordCenter(engine, pageNo, word1, &x1, &y1)) {
        return fail(StrL("ERROR word1-not-found"));
    }
    if (!FindWordCenter(engine, pageNo, word2, &x2, &y2)) {
        return fail(StrL("ERROR word2-not-found"));
    }
    if (!FindWordCenter(engine, pageNo, cursorWord, &xc, &yc)) {
        return fail(StrL("ERROR cursorWord-not-found"));
    }

    // build a text selection spanning word1..word2, like a left-drag would
    dm->textSelection->StartAt(pageNo, x1, y1);
    dm->textSelection->SelectUpTo(pageNo, x2, y2);
    WindowTab* tab = win->CurrentTab();
    DeleteOldSelectionInfo(win);
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = tab->selectionOnPage != nullptr;

    bool isTextOnly = false;
    TempStr original = GetSelectedTextTemp(tab, StrL(" "), isTextOnly);
    if (len(original) == 0) {
        return fail(StrL("ERROR empty-selection"));
    }
    original = str::DupTemp(original);

    // simulate opening the context menu over cursorWord: this is the read-only
    // check the menu performs; it must not change the selection
    Point screenPt = dm->CvtToScreen(pageNo, PointF((float)xc, (float)yc));
    ReadAloudCanReadFromCursor(dm, screenPt);

    TempStr after = GetSelectedTextTemp(tab, StrL(" "), isTextOnly);
    bool ok = str::Eq(original, after);
    if (ok) {
        out.Append(fmt("OK selected=%s\n", original));
    } else {
        out.Append(fmt("FAIL original=%s after=%s\n", original, after));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// a spot on `pageNo` with no text (and no link / image) under it, in screen
// coords, or an empty Point if the visible part of the page is all text
static Point FindEmptySpotOnPage(MainWindow* win, DisplayModel* dm, int pageNo) {
    Rect client = win->canvasRc;
    constexpr int kStep = 8;
    for (int y = client.y + kStep; y < client.y + client.dy; y += kStep) {
        for (int x = client.x + kStep; x < client.x + client.dx; x += kStep) {
            Point pt{x, y};
            if (dm->GetPageNoByPoint(pt) != pageNo) {
                continue;
            }
            if (dm->IsOverText(pt)) {
                continue;
            }
            // a link or image under the cursor makes the click do something else
            if (dm->GetElementAtPos(pt, nullptr)) {
                continue;
            }
            return pt;
        }
    }
    return Point{};
}

// Regression test for issue #5881: clicking empty space clears the text
// selection. #5737 made find highlights independent of the selection, which
// meant ClearSearchResult() -- what the "click, not a drag" path in
// OnMouseLeftButtonUp called -- no longer cleared the selection, leaving Esc as
// the only way to drop it. Selects `word` on page 1 the way a drag would, then
// sends a real left click (down + up, no movement) at a spot with no text under
// it, so the whole canvas mouse path runs, and reports what is still selected.
TempStr ClickClearsSelectionResultTemp(Str word, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word)) {
        return fail(StrL("ERROR missing word"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }
    EngineBase* engine = dm->GetEngine();
    const int pageNo = 1;
    double wx = 0, wy = 0;
    if (!FindWordCenter(engine, pageNo, word, &wx, &wy)) {
        return fail(StrL("ERROR word-not-found"));
    }

    // select the word, the way a left-drag across it would
    WindowTab* tab = win->CurrentTab();
    DeleteOldSelectionInfo(win, true);
    dm->textSelection->StartAt(pageNo, wx, wy);
    dm->textSelection->SelectUpTo(pageNo, wx, wy);
    dm->textSelection->SelectWordAt(pageNo, wx, wy);
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = tab->selectionOnPage != nullptr;

    bool isTextOnly = false;
    TempStr selected = str::DupTemp(GetSelectedTextTemp(tab, StrL(" "), isTextOnly));
    if (len(selected) == 0) {
        return fail(StrL("ERROR empty-selection"));
    }

    Point pt = FindEmptySpotOnPage(win, dm, pageNo);
    if (pt.IsEmpty()) {
        return fail(StrL("ERROR no-empty-spot"));
    }

    // a real click: down and up at the same point, so it isn't a drag
    LPARAM lp = MAKELPARAM(pt.x, pt.y);
    SendMessageW(win->hwndCanvas, WM_LBUTTONDOWN, 0, lp);
    SendMessageW(win->hwndCanvas, WM_LBUTTONUP, 0, lp);

    TempStr after = GetSelectedTextTemp(tab, StrL(" "), isTextOnly);
    bool cleared = (len(after) == 0) && !win->showSelection;
    if (cleared) {
        out.Append(fmt("OK selected=%s cleared at %d,%d\n", selected, pt.x, pt.y));
    } else {
        out.Append(fmt("FAIL selected=%s still=%s showSelection=%d\n", selected, after, (int)win->showSelection));
    }
    if (exitCodeOut) {
        *exitCodeOut = cleared ? 0 : 1;
    }
    return ToStrTemp(out);
}

// Regression test for the rectangular-selection drag: a Ctrl+drag rectangle is
// normally drawn over text, and the "clicking already selected text starts a
// drag-out" check used to run first and claim every press inside it, so the
// rectangle could never be moved or resized. Builds a rectangle around `word`
// on page 1 and presses the left button in the middle of it (over text), then
// reports what the canvas decided to do.
TempStr RectSelectionDragResultTemp(Str word, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word)) {
        return fail(StrL("ERROR missing word"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }
    EngineBase* engine = dm->GetEngine();
    const int pageNo = 1;
    double wx = 0, wy = 0;
    if (!FindWordCenter(engine, pageNo, word, &wx, &wy)) {
        return fail(StrL("ERROR word-not-found"));
    }
    Point center = dm->CvtToScreen(pageNo, PointF((float)wx, (float)wy));

    // a rectangle around the word, as a Ctrl+drag would leave it. No glyphs in
    // the text selection, which is what makes it a rectangular selection
    WindowTab* tab = win->CurrentTab();
    DeleteOldSelectionInfo(win, true);
    int half = 40;
    Rect rc(center.x - half, center.y - (half / 2), half * 2, half);
    tab->selectionOnPage = SelectionOnPage::FromRectangle(dm, rc);
    win->showSelection = tab->selectionOnPage != nullptr;
    if (!win->showSelection) {
        return fail(StrL("ERROR no-rect-selection"));
    }
    if (!IsRectangularSelection(win)) {
        return fail(StrL("ERROR not-rectangular"));
    }
    // the point we press must be over text, otherwise this doesn't test anything
    if (!dm->IsOverText(center)) {
        return fail(StrL("ERROR press-point-not-over-text"));
    }

    SendMessageW(win->hwndCanvas, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(center.x, center.y));
    SelectionDragEdge edge = win->selectionDragEdge;
    bool dragging = (win->mouseAction == MouseAction::Selecting) && (edge != SelectionDragEdge::None);
    bool textDrag = win->textDragPending;
    SendMessageW(win->hwndCanvas, WM_LBUTTONUP, 0, MAKELPARAM(center.x, center.y));

    bool ok = dragging && !textDrag;
    if (ok) {
        out.Append(fmt("OK moving rect selection, edge=%d\n", (int)edge));
    } else {
        out.Append(
            fmt("FAIL edge=%d mouseAction=%d textDragPending=%d\n", (int)edge, (int)win->mouseAction, (int)textDrag));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// find the [start, end) glyph range of the first occurrence of `word` on a page
static bool FindWordGlyphRange(EngineBase* engine, int pageNo, Str word, int* startOut, int* endOut) {
    if (!engine || !word || !startOut || !endOut) {
        return false;
    }
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen);
    if (!text) {
        return false;
    }
    int wordLen = Utf8CodepointCount(word);
    if (wordLen <= 0) {
        return false;
    }
    for (int i = 0; i <= textLen - wordLen; i++) {
        if (str::Eq(Utf8SliceByCodepoints(text, i, wordLen), word)) {
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
TempStr GoToFindMatchResultTemp(Str word, Str typed, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word) || str::IsEmptyOrWhiteSpace(typed)) {
        return fail(StrL("ERROR missing word or typed"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
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
        return fail(StrL("ERROR word-not-found"));
    }

    // mimic a prior find: the typed (lowercase) text becomes textSearch's
    // lastText, so SetLastResult() inside GoToFindMatch() sees a text change
    dm->textSearch->SetText(typed);

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

    TempStr matched;
    Rect* coords = nullptr;
    int pageTextLen = 0;
    Str pageTxt = engine->GetTextForPage(pageNo, &pageTextLen, &coords);
    if (pageTxt && coords && curPage == pageNo && curStart >= 0 && curEnd <= pageTextLen && curStart < curEnd) {
        matched = Utf8SliceByCodepoints(pageTxt, curStart, curEnd - curStart);
    }

    // is the match rect actually within the visible viewport now? (mirrors
    // DisplayModel::ScrollScreenToRect's own visibility test)
    bool visible = false;
    if (coords && curPage == pageNo && curStart >= 0 && curEnd <= pageTextLen && curStart < curEnd) {
        Rect pr = coords[curStart];
        for (int i = curStart + 1; i < curEnd; i++) {
            pr = pr.Union(coords[i]);
        }
        Rect sr = dm->CvtToScreen(pageNo, ToRectF(pr));
        Rect vp = Rect(Point(), dm->viewPort.Size());
        visible = !vp.Intersect(sr).IsEmpty();
    }

    // PaintAllFindMatches only paints a match in the selection color (rather
    // than as one of the plain matches) while textSearch->result is populated,
    // so an empty result here means the match we navigated to doesn't read as
    // the current one - and with the find UI closed isn't highlighted at all
    // (issue #5889). SetLastResult()->SetText() drops it exactly when the
    // document text differs from what was typed, which is this test's case.
    bool hasResult = ts->result.len > 0;

    bool matchOk = (curPage == pageNo) && (curStart == startGlyph) && (curEnd == endGlyph) && str::Eq(matched, word);
    bool ok = matchOk && visible && hasResult;
    if (ok) {
        out.Append(fmt("OK match=%s page=%d visible=1 highlighted=1\n", matched, pageNo));
    } else {
        out.Append(fmt("FAIL expected=%s match=%s page=%d visible=%d highlighted=%d\n", word,
                       matched ? matched : StrL("(none)"), pageNo, visible ? 1 : 0, hasResult ? 1 : 0));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

static bool FindWordCenter(EngineBase* engine, int pageNo, Str word, double* xOut, double* yOut) {
    if (!engine || !word || !xOut || !yOut) {
        return false;
    }
    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (!text) {
        return false;
    }
    int wordLen = Utf8CodepointCount(word);
    if (wordLen <= 0) {
        return false;
    }
    for (int i = 0; i <= textLen - wordLen; i++) {
        if (!str::Eq(Utf8SliceByCodepoints(text, i, wordLen), word)) {
            continue;
        }
        int mid = i + (wordLen / 2);
        int midByte = Utf8CodepointToByteIndex(text, mid);
        for (; mid < textLen && !coords[mid].x && !coords[mid].dx; mid++) {
            int nextByte = midByte;
            if (Utf8CodepointNext(text, nextByte) == '\n') {
                return false;
            }
            midByte = nextByte;
        }
        if (mid >= textLen) {
            return false;
        }
        *xOut = coords[mid].x + (coords[mid].dx / 2.0);
        *yOut = coords[mid].y + (coords[mid].dy / 2.0);
        return true;
    }
    return false;
}

static TempStr ExtractSelectionTextTemp(TextSelection& ts) {
    Str s = ts.ExtractText(StrL(" "));
    TempStr res = str::DupTemp(s);
    str::Free(s);
    return res;
}

// Headless triple-click line-selection test (issue #5712). Loads the pdf, clicks
// the middle of <clickWord>, runs the same TextSelection steps as a double-click
// followed by a triple-click (without the mouse-up trim), and checks the result.
TempStr TripleClickLineSelectResultTemp(Str pdfPath, Str clickWord, Str expectedLine, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    if (str::IsEmptyOrWhiteSpace(pdfPath) || str::IsEmptyOrWhiteSpace(clickWord) ||
        str::IsEmptyOrWhiteSpace(expectedLine)) {
        out.Append(StrL("ERROR missing pdf, clickWord, or expectedLine\n"));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }

    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }

    const int pageNo = 1;
    double x = 0;
    double y = 0;
    if (!FindWordCenter(engine, pageNo, clickWord, &x, &y)) {
        out.Append(fmt("ERROR word-not-found word=%s\n", clickWord));
        SafeEngineRelease(&engine);
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
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
        out.Append(fmt("ERROR trim-check-failed trimmed=%s\n", trimmedText));
        SafeEngineRelease(&engine);
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }

    bool ok = str::Eq(selected, expectedLine);
    if (ok) {
        out.Append(fmt("OK selected=%s\n", selected));
    } else {
        out.Append(fmt("FAIL selected=%s expected=%s\n", selected, expectedLine));
    }

    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
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

// Navigate to the n-th (1-based) outline destination that has a dest, then
// report CurrentPageNo vs the destination page. Used by tests/issue-2799.ts.
// Expects a document already open in gWindows[0] (withControlledSumatra args).
// Navigate to the n-th (1-based) outline destination in the open document and
// report landed page vs destination page (issue #2799).
TempStr TocNavigateResultTemp(int destNo, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code = 1) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return fail(StrL("NOTREADY not-fixed"), 2);
    }
    EngineBase* engine = dm->GetEngine();
    TocTree* toc = engine ? engine->GetToc() : nullptr;
    if (!toc || !toc->root) {
        return fail(StrL("ERROR no-toc"));
    }
    int counter = 0;
    IPageDestination* dest = NthDestInToc(toc->root, destNo, counter);
    if (!dest) {
        return fail(fmt("ERROR no-dest destNo=%d", destNo));
    }
    int expectPage = PageDestGetPageNo(dest);
    if (expectPage <= 0) {
        return fail(fmt("ERROR bad-dest-page destNo=%d page=%d", destNo, expectPage));
    }

    // Scroll away from page 1 first so a relative-Y bug would shift the land
    // (continuous mode + mid-document start was the #2799 failure mode).
    if (dm->PageCount() >= 2 && expectPage != 1) {
        dm->GoToPage(1, 0, false);
        // nudge down so CurrentPage on-screen offset is non-zero if possible
        dm->ScrollYBy(dm->viewPort.dy / 3, false);
    }

    win->ctrl->HandleLink(dest, win->linkHandler);

    int landed = dm->CurrentPageNo();
    bool ok = landed == expectPage;
    if (ok) {
        out.Append(fmt("OK dest=%d expect=%d landed=%d\n", destNo, expectPage, landed));
    } else {
        out.Append(fmt("FAIL dest=%d expect=%d landed=%d\n", destNo, expectPage, landed));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// Zoom to startZoomPerc, then follow the destNo-th outline destination the way
// a bookmark click does, and report the zoom on both sides of it. That is what
// IgnoreDestinationZoom decides: whether the destination's zoom wins or the
// zoom the reader is at is kept (discussion #5938). With destNo == 0 nothing is
// followed, so the caller can zoom and read back the zoom around a navigation
// it drives itself (a real click in the Bookmarks sidebar).
// Used by tests/issue-5938.ts.
TempStr DestZoomNavResultTemp(int destNo, int startZoomPerc, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code = 1) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return fail(StrL("NOTREADY not-fixed"), 2);
    }
    IPageDestination* dest = nullptr;
    if (destNo > 0) {
        EngineBase* engine = dm->GetEngine();
        TocTree* toc = engine ? engine->GetToc() : nullptr;
        if (!toc || !toc->root) {
            return fail(StrL("ERROR no-toc"));
        }
        int counter = 0;
        dest = NthDestInToc(toc->root, destNo, counter);
        if (!dest) {
            return fail(fmt("ERROR no-dest destNo=%d", destNo));
        }
    }

    if (startZoomPerc > 0) {
        dm->SetZoomVirtual((float)startZoomPerc, nullptr);
    }
    float zoomBefore = dm->GetZoomVirtual();
    if (dest) {
        win->ctrl->HandleLink(dest, win->linkHandler);
    }
    float zoomAfter = dm->GetZoomVirtual();

    out.Append(fmt("OK dest=%d destZoom=%g page=%d landed=%d zoomBefore=%g zoomAfter=%g ignore=%d\n", destNo,
                   dest ? PageDestGetZoom(dest) : 0.f, dest ? PageDestGetPageNo(dest) : 0, dm->CurrentPageNo(),
                   zoomBefore, zoomAfter, gSettings->ignoreDestinationZoom ? 1 : 0));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// With destNo > 0, start navigation to that Markdown TOC item through the real
// deferred TOC path. With destNo == 0, report whether WebView has reached the
// requested vertical scroll position. Used by tests/issue-5842.ts.
TempStr MarkdownTocNavigateResultTemp(int destNo, int minScrollY, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }
    MarkdownModel* mm = win->ctrl ? win->ctrl->AsMarkdown() : nullptr;
    if (!mm || !mm->docView) {
        return finish(StrL("NOTREADY no-markdown-webview"), 2);
    }

    if (destNo > 0) {
        TocTree* toc = mm->GetToc();
        int counter = 0;
        TocItem* item = toc && toc->root ? NthTocItemWithDest(toc->root, destNo, counter) : nullptr;
        if (!item) {
            // headings are filled in on a background thread; the files-only
            // stub TOC is installed first, so dest 3 may not exist yet
            return finish(fmt("NOTREADY no-dest destNo=%d", destNo), 2);
        }
        GoToTocItem(win, item);
        return finish(fmt("NAVIGATING dest=%d name=%s", destNo, PageDestGetName(item->dest)), 0);
    }

    Point pos = mm->docView->GetScrollPos();
    if (pos.y < minScrollY) {
        return finish(fmt("NOTREADY scrollY=%d min=%d", pos.y, minScrollY), 2);
    }
    return finish(fmt("OK scrollX=%d scrollY=%d", pos.x, pos.y), 0);
}

// Click a link in the currently shown markdown/html document: `href` is the url
// WebView2 would report for it (relative to the document, or the full virtual
// host url), and this goes through the same MarkdownModel::OnBeforeNavigate()
// the browser calls. Reports whether the view would navigate to it, then lists
// the tabs of every window so a test can see what got opened where.
// With follow == false nothing is clicked and only the tabs are listed: opening
// a linked document runs from a uitask, so its result has to be read back in a
// later request, by which time the current tab may no longer be the markdown one.
// Used by tests/issue-5924.ts.
TempStr MarkdownFollowLinkResultTemp(Str href, bool follow, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }

    int navigate = -1;
    if (follow) {
        MarkdownModel* mm = win->ctrl ? win->ctrl->AsMarkdown() : nullptr;
        if (!mm) {
            return finish(StrL("NOTREADY no-markdown"), 2);
        }
        if (!href) {
            return finish(StrL("ERROR no-href"), 1);
        }
        navigate = mm->OnBeforeNavigate(href, false) ? 1 : 0;
    }
    out.Append(fmt("OK navigate=%d\n", navigate));
    for (int i = 0; i < len(gWindows); i++) {
        MainWindow* w = gWindows[i];
        for (WindowTab* tab : w->Tabs()) {
            int isCurrent = tab == w->CurrentTab() ? 1 : 0;
            int pageNo = tab->ctrl ? tab->ctrl->CurrentPageNo() : 0;
            out.Append(fmt("tab win=%d current=%d pageNo=%d file=%s\n", i, isCurrent, pageNo, tab->filePath));
        }
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Follow the first internal link on page 1 after pinning the viewport to the
// left; used by tests/issue-5064.ts (issue #5064).
TempStr ScrollToLinkResultTemp(int minViewportDelta, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }

    dm->SetZoomVirtual(200, nullptr);
    dm->Relayout(200, dm->rotation);
    dm->viewPort.x = 0;
    dm->RecalcVisibleParts();
    dm->RenderVisibleParts();

    int before = dm->viewPort.x;
    IPageDestination* dest = FirstLinkDestOnPage(dm->GetEngine(), 1);
    if (!dest) {
        return fail(StrL("ERROR no-link"));
    }

    win->ctrl->HandleLink(dest, win->linkHandler);

    int after = dm->viewPort.x;
    int delta = after - before;
    bool ok = delta >= minViewportDelta;
    if (ok) {
        out.Append(fmt("OK viewport_before=%d viewport_after=%d delta=%d\n", before, after, delta));
    } else {
        out.Append(
            fmt("FAIL viewport_before=%d viewport_after=%d delta=%d min=%d\n", before, after, delta, minViewportDelta));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// Verifies _TRA resolves error-path strings through the translation table.
TempStr I18nErrorStringResultTemp(int* exitCodeOut) {
    str::Builder out;
    Str err = _TRA("Error");
    Str crash = _TRA("SumatraPDF crashed");
    Str printers = _TRA("SumatraPDF - Show Printers");
    bool ok = len(err) > 0 && len(crash) > 0 && len(printers) > 0 &&
              str::Eq(err, trans::GetTranslation(StrL("Error"))) &&
              str::Eq(crash, trans::GetTranslation(StrL("SumatraPDF crashed"))) &&
              str::Eq(printers, trans::GetTranslation(StrL("SumatraPDF - Show Printers")));
    if (ok) {
        out.Append(fmt("OK error=%s crash=%s printers=%s\n", err, crash, printers));
    } else {
        out.Append(fmt("FAIL error=%s crash=%s printers=%s\n", err ? err : StrL("(null)"),
                       crash ? crash : StrL("(null)"), printers ? printers : StrL("(null)")));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

static void AppendTocItems(str::Builder& out, TocItem* item, int depth = 0) {
    for (; item; item = item->next) {
        if (item->title) {
            for (int i = 0; i < depth; i++) {
                out.Append(StrL("  "));
            }
            out.Append(fmt("%s|page=%d\n", item->title, item->pageNo));
        }
        AppendTocItems(out, item->child, depth + 1);
    }
}

// Headless test for document TOC (e.g. ComicInfo.xml bookmarks in CBZ). Returns
// one line per TOC entry: "title|page=N", indented two spaces per nesting
// level. Used by tests/issue-1201.ts and tests/issue-5317.ts.
TempStr GetTocResultTemp(Str path, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR engine-create-failed path=%s\n", path));
    } else {
        TocTree* toc = engine->GetToc();
        if (!toc || !toc->root || !toc->root->child) {
            if (exitCodeOut) {
                *exitCodeOut = 1;
            }
            out.Append(StrL("ERROR no-toc\n"));
        } else {
            if (exitCodeOut) {
                *exitCodeOut = 0;
            }
            AppendTocItems(out, toc->root->child);
        }
        SafeEngineRelease(&engine);
    }
    return ToStrTemp(out);
}

// Headless test for page link elements. Returns one line per link:
// "kind=<kind> value=<value>". Used by tests/ad-hoc-md-links.ts.
TempStr PageLinksResultTemp(Str path, int pageNo, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR engine-create-failed path=%s\n", path));
        return ToStrTemp(out);
    }

    if (!engine->BenchLoadPage(pageNo)) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR page-load-failed page=%d\n", pageNo));
        SafeEngineRelease(&engine);
        return ToStrTemp(out);
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
        TempStr valueShown = str::ReplaceTemp(value, StrL("\r\n"), StrL("|"));
        valueShown = str::ReplaceTemp(valueShown, StrL("\n"), StrL("|"));
        RectF src = el->GetRect();
        RectF destRc = PageDestGetRect(dest);
        out.Append(fmt("kind=%s page=%d src=%g,%g,%g,%g dest=%g,%g,%g,%g value=%s\n", Str(dest->GetKind()),
                       PageDestGetPageNo(dest), src.x, src.y, src.dx, src.dy, destRc.x, destRc.y, destRc.dx, destRc.dy,
                       valueShown));
    }
    if (nLinks == 0) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR no-links page=%d\n", pageNo));
    } else if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    SafeEngineRelease(&engine);
    return ToStrTemp(out);
}

// Hover-tip strings for annotation comments on a page (issue #5329).
// Newlines in a tip are reported as "|".
TempStr PageCommentsResultTemp(Str path, int pageNo, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR engine-create-failed path=%s\n", path));
        return ToStrTemp(out);
    }

    if (!engine->BenchLoadPage(pageNo)) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR page-load-failed page=%d\n", pageNo));
        SafeEngineRelease(&engine);
        return ToStrTemp(out);
    }

    int nComments = 0;
    Vec<IPageElement*> els = engine->GetElements(pageNo);
    for (IPageElement* el : els) {
        if (!el || !el->Is(kindPageElementComment)) {
            continue;
        }
        Str value = el->GetValue();
        TempStr flat = str::ReplaceTemp(value, StrL("\n"), StrL("|"));
        nComments++;
        out.Append(fmt("comment=%s\n", flat));
    }
    if (nComments == 0) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR no-comments page=%d\n", pageNo));
    } else if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    SafeEngineRelease(&engine);
    return ToStrTemp(out);
}

// Color histogram of a page rendered with the CAD/engineering enhancement forced
// on, so a test can tell what that enhancement did to the page's grays. Reports
// the most common neutral grays as "gray=<value> count=<n>" (issue #5937).
TempStr CadEnhanceColorsResultTemp(Str path, int pageNo, int zoomPercent, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    // the enhancement is normally decided per document by CAD detection; force
    // it so the fixture doesn't have to look like a CAD export
    SetEngineeringDrawingEnhanceMode(StrL("on"));
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", path));
    }
    if (pageNo < 1 || pageNo > engine->PageCount()) {
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR bad-page page=%d\n", pageNo));
    }
    if (!EngineMupdfCadEnhanceActive(engine)) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR cad-enhance-not-active\n"));
    }

    // RenderPageArgs::zoom is a scale factor, not a percentage
    float zoom = (float)zoomPercent / 100.f;
    RenderPageArgs args(pageNo, zoom, 0);
    Pixmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR render-failed\n"));
    }
    // the engine may render to a palette DIB we can't read directly
    Pixmap* rgb = (bmp->format == PixmapFormat::BGRA8) ? bmp : PixmapCopyAs32bppDIB(bmp);
    if (!rgb) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR pixmap-convert-failed\n"));
    }

    int counts[256] = {};
    for (int y = 0; y < rgb->height; y++) {
        const u8* row = rgb->data + ((size_t)y * (size_t)rgb->stride);
        for (int x = 0; x < rgb->width; x++) {
            const u8* px = row + ((size_t)x * 4);
            // BGRA8: neutral grays only, which is all this enhancement touches
            if (px[0] == px[1] && px[1] == px[2]) {
                counts[px[0]]++;
            }
        }
    }
    out.Append(fmt("size=%dx%d\n", rgb->width, rgb->height));
    // every gray with a meaningful area, so the test can see what survived
    for (int i = 0; i < 256; i++) {
        if (counts[i] >= 64) {
            out.Append(fmt("gray=%d count=%d\n", i, counts[i]));
        }
    }
    if (rgb != bmp) {
        FreePixmap(rgb);
    }
    FreePixmap(bmp);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Render an image page and report dest size plus the RGB of the left and right
// edge pixels. clipKind=1 uses the slightly-off page rect that Copy Selection
// produces after CvtFromScreen (issue #3434).
TempStr ImageRenderEdgesResultTemp(Str path, int zoomPercent, int clipKind, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", path));
    }
    RectF box = engine->PageMediabox(1);
    float zoom = (float)zoomPercent / 100.f;
    RectF clip;
    RectF* pageRect = nullptr;
    if (clipKind != 0) {
        // same half-pixel pull-back CvtFromScreen applies to a pixel-aligned
        // selection of the whole image
        clip = RectF(-0.499f, -0.499f, box.dx, box.dy);
        pageRect = &clip;
    }
    RenderPageArgs args(1, zoom, 0, pageRect, RenderTarget::Export);
    Pixmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR render-failed box=%gx%g zoom=%g\n", box.dx, box.dy, zoom));
    }
    if (bmp->width < 2 || bmp->height < 1 || !bmp->data) {
        TempStr msg = fmt("ERROR pixmap-too-small bmp=%dx%d fmt=%d box=%gx%g\n", bmp->width, bmp->height,
                          (int)bmp->format, box.dx, box.dy);
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(msg);
    }
    int bpp = PixmapBytesPerPixel(bmp->format);
    if (bpp < 3) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR pixmap-fmt=%d\n", (int)bmp->format));
    }

    auto pixel = [&](int x, int y, int* r, int* g, int* b) {
        const u8* px = bmp->data + ((size_t)y * (size_t)bmp->stride) + ((size_t)x * bpp);
        if (bmp->format == PixmapFormat::RGBA8) {
            *r = px[0];
            *g = px[1];
            *b = px[2];
        } else {
            *b = px[0];
            *g = px[1];
            *r = px[2];
        }
    };
    int lr, lg, lb, rr, rg, rb;
    pixel(0, bmp->height / 2, &lr, &lg, &lb);
    pixel(bmp->width - 1, bmp->height / 2, &rr, &rg, &rb);
    out.Append(fmt("size=%dx%d left=%d,%d,%d right=%d,%d,%d\n", bmp->width, bmp->height, lr, lg, lb, rr, rg, rb));

    FreePixmap(bmp);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Stamp an image file onto page 1 of a PDF (the #1744 "electronic signature"
// path) and report how many annotations the page has plus how many red-ish
// pixels the render shows. The fixture image is solid red so a successful
// stamp lights up a block of red.
TempStr ImageInsertResultTemp(Str pdfPath, Str imagePath, int* exitCodeOut) {
    ScopedGdiPlus gdiPlus;
    EnsureTestGlobalPrefs();

    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", pdfPath));
    }
    if (!EngineSupportsAnnotations(engine)) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR annots-not-supported\n"));
    }
    Str data = file::ReadFile(imagePath);
    Pixmap* image = PixmapFromData(data);
    str::Free(data);
    if (!image) {
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR image-load-failed path=%s\n", imagePath));
    }
    if (!engine->BenchLoadPage(1)) {
        FreePixmap(image);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR page-load-failed\n"));
    }

    AnnotCreateArgs args{AnnotationType::Stamp};
    args.stampImage = image;
    Annotation* annot = EngineMupdfCreateAnnotation(engine, 1, PointF{72.f, 100.f}, &args);
    if (!annot) {
        TempStr msg = fmt("ERROR stamp-create-failed fmt=%d %dx%d\n", (int)image->format, image->width, image->height);
        FreePixmap(image);
        SafeEngineRelease(&engine);
        return fail(msg);
    }
    FreePixmap(image);

    Vec<Annotation*> annots;
    EngineGetAnnotations(engine, annots);
    int nAnnots = len(annots);
    RectF ar = GetRect(annot);
    out.Append(fmt("annot=%s rect=%g,%g,%g,%g\n", AnnotationReadableNameTemp(Type(annot)), ar.x, ar.y, ar.dx, ar.dy));

    RenderPageArgs rargs(1, 1.f, 0, nullptr, RenderTarget::Export);
    Pixmap* bmp = engine->RenderPage(rargs);
    if (!bmp || !bmp->data) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR render-failed\n"));
    }
    Pixmap* rgb = (bmp->format == PixmapFormat::BGRA8) ? bmp : PixmapCopyAs32bppDIB(bmp);
    if (!rgb || !rgb->data) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR pixmap-convert-failed fmt=%d\n", (int)bmp->format));
    }
    int bpp = PixmapBytesPerPixel(rgb->format);
    int red = 0;
    int nonWhite = 0;
    if (bpp >= 3) {
        for (int y = 0; y < rgb->height; y++) {
            const u8* row = rgb->data + ((size_t)y * (size_t)rgb->stride);
            for (int x = 0; x < rgb->width; x++) {
                const u8* px = row + ((size_t)x * bpp);
                int r, g, b;
                if (rgb->format == PixmapFormat::RGBA8) {
                    r = px[0];
                    g = px[1];
                    b = px[2];
                } else {
                    b = px[0];
                    g = px[1];
                    r = px[2];
                }
                if (r < 250 || g < 250 || b < 250) {
                    nonWhite++;
                }
                if (r > 180 && g < 80 && b < 80) {
                    red++;
                }
            }
        }
    }
    out.Append(fmt("annots=%d red=%d nonwhite=%d size=%dx%d\n", nAnnots, red, nonWhite, rgb->width, rgb->height));
    if (rgb != bmp) {
        FreePixmap(rgb);
    }
    FreePixmap(bmp);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = (nAnnots >= 1 && red > 50) ? 0 : 1;
    }
    if (nAnnots < 1 || red <= 50) {
        out.Append(StrL("ERROR stamp-not-visible\n"));
    }
    return ToStrTemp(out);
}

// Open any document, render page 1, and report dest size plus how many
// red-ish / non-white pixels it has. Used to check that a WebP inside an
// EPUB actually paints (issue #3415) instead of the IMAGE placeholder.
TempStr PageRenderColorsResultTemp(Str path, int* exitCodeOut) {
    EnsureTestGlobalPrefs();

    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", path));
    }
    if (!engine->BenchLoadPage(1)) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR page-load-failed\n"));
    }

    RenderPageArgs rargs(1, 1.f, 0, nullptr, RenderTarget::Export);
    Pixmap* bmp = engine->RenderPage(rargs);
    if (!bmp || !bmp->data) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR render-failed\n"));
    }
    Pixmap* rgb = (bmp->format == PixmapFormat::BGRA8) ? bmp : PixmapCopyAs32bppDIB(bmp);
    if (!rgb || !rgb->data) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR pixmap-convert-failed fmt=%d\n", (int)bmp->format));
    }
    int bpp = PixmapBytesPerPixel(rgb->format);
    int red = 0;
    int nonWhite = 0;
    if (bpp >= 3) {
        for (int y = 0; y < rgb->height; y++) {
            const u8* row = rgb->data + ((size_t)y * (size_t)rgb->stride);
            for (int x = 0; x < rgb->width; x++) {
                const u8* px = row + ((size_t)x * bpp);
                int r, g, b;
                if (rgb->format == PixmapFormat::RGBA8) {
                    r = px[0];
                    g = px[1];
                    b = px[2];
                } else {
                    b = px[0];
                    g = px[1];
                    r = px[2];
                }
                if (r < 250 || g < 250 || b < 250) {
                    nonWhite++;
                }
                if (r > 180 && g < 80 && b < 80) {
                    red++;
                }
            }
        }
    }
    out.Append(
        fmt("red=%d nonwhite=%d size=%dx%d pages=%d\n", red, nonWhite, rgb->width, rgb->height, engine->PageCount()));
    if (rgb != bmp) {
        FreePixmap(rgb);
    }
    FreePixmap(bmp);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// SHA-1 thumbprints and drop-down labels of CurrentUser\MY certs that can
// sign, one pair per cert. Used to check the store enumeration for #5965.
TempStr ListSigningCertsResultTemp(int* exitCodeOut) {
    StrVec thumbs;
    StrVec labels;
    ListWindowsSigningCertificates(thumbs, labels);
    str::Builder out;
    out.Append(fmt("n=%d\n", len(thumbs)));
    for (int i = 0; i < len(thumbs); i++) {
        out.Append(fmt("thumb=%s\nlabel=%s\n", thumbs[i], labels[i]));
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Sign pdfPath with a Windows-store cert (thumbprint) or a .pfx (certPath +
// password), write destPath, and report ok=1 on success. The dest file is a
// copy of the source so the signature can be saved incrementally.
TempStr SignDocumentResultTemp(Str pdfPath, Str destPath, Str thumbprint, Str certPath, Str certPassword, Str imagePath,
                               int appearanceFlags, int* exitCodeOut) {
    EnsureTestGlobalPrefs();

    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    if (len(thumbprint) == 0 && len(certPath) == 0) {
        return fail(StrL("ERROR need thumbprint or certPath\n"));
    }
    if (!file::Exists(pdfPath)) {
        return fail(fmt("ERROR pdf-missing path=%s\n", pdfPath));
    }
    if (!file::Copy(destPath, pdfPath, false)) {
        return fail(fmt("ERROR copy-failed dest=%s\n", destPath));
    }

    EngineBase* engine = CreateEngineFromFile(destPath, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", destPath));
    }

    PdfSignArgs args;
    args.certThumbprint = thumbprint;
    args.certPath = certPath;
    args.certPassword = certPassword;
    args.imagePath = imagePath;
    args.appearanceFlags = appearanceFlags;
    args.pageNo = 1;
    StrVec fieldNames;
    Vec<int> fieldPages;
    EngineMupdfGetUnsignedSignatureFields(engine, fieldNames, fieldPages);
    if (len(fieldNames) > 0) {
        args.fieldName = fieldNames[0];
        args.pageNo = fieldPages[0];
    }

    Str err;
    bool ok = EngineMupdfSignDocument(engine, args, &err);
    if (!ok) {
        TempStr msg = fmt("ERROR sign-failed %s\n", err ? err : StrL("(no message)"));
        str::Free(err);
        SafeEngineRelease(&engine);
        return fail(msg);
    }
    str::Free(err);

    if (!EngineMupdfSaveUpdated(engine, destPath, {})) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR save-failed\n"));
    }
    SafeEngineRelease(&engine);
    out.Append(StrL("ok=1\n"));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

static int JpegSofComponents(Str jpeg) {
    ByteReader r(jpeg);
    int n = len(jpeg);
    if (n < 4 || r.UInt8(0) != 0xFF || r.UInt8(1) != 0xD8) {
        return 0;
    }
    int i = 2;
    while (i + 9 < n) {
        if (r.UInt8(i) != 0xFF) {
            return 0;
        }
        u8 marker = r.UInt8(i + 1);
        if (marker == 0xDA || marker == 0xD9) {
            return 0;
        }
        if (marker >= 0xD0 && marker <= 0xD7) {
            i += 2;
            continue;
        }
        if (marker == 0x01) {
            i += 2;
            continue;
        }
        u16 seglen = r.UInt16BE(i + 2);
        if (seglen < 2) {
            return 0;
        }
        if (marker >= 0xC0 && marker <= 0xC3) {
            return r.UInt8(i + 9);
        }
        i += 2 + (int)seglen;
    }
    return 0;
}

static u16 TiffPhotometric(Str tiff) {
    ByteReader r(tiff);
    int n = len(tiff);
    if (n < 8 || r.UInt8(0) != 'I' || r.UInt8(1) != 'I') {
        return 0;
    }
    u32 ifd = r.UInt32LE(4);
    if (ifd + 2 > (u32)n) {
        return 0;
    }
    u16 count = r.UInt16LE((int)ifd);
    for (u16 i = 0; i < count; i++) {
        int e = (int)ifd + 2 + (int)i * 12;
        if (e + 12 > n) {
            break;
        }
        if (r.UInt16LE(e) == 262) {
            return r.UInt16LE(e + 8);
        }
    }
    return 0;
}

// PDF with a DeviceCMYK JPEG (PDF polarity), then Save Image as JPEG/TIFF.
// JPEG must stay CMYK with Adobe invert so it is not a negative; TIFF must
// stay CMYK (not RGB).
TempStr CmykImageSaveResultTemp(Str jpegPath, Str tiffPath, int* exitCodeOut) {
    EnsureTestGlobalPrefs();
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        if (!str::EndsWith(msg, StrL("\n"))) {
            out.AppendChar('\n');
        }
        return ToStrTemp(out);
    };
    if (!jpegPath || !tiffPath) {
        return fail(StrL("ERROR bad-args"));
    }

    PdfCreator pdf;
    if (!pdf.ctx || !pdf.doc) {
        return fail(StrL("ERROR pdf-create"));
    }
    fz_context* ctx = pdf.ctx;
    fz_pixmap* pix = nullptr;
    fz_buffer* jpegBuf = nullptr;
    Str jpegSrc;
    bool built = false;
    fz_var(pix);
    fz_var(jpegBuf);

    fz_try(ctx) {
        pix = fz_new_pixmap(ctx, fz_device_cmyk(ctx), 32, 32, nullptr, 0);
        fz_clear_pixmap(ctx, pix);
        for (int y = 0; y < 32; y++) {
            u8* p = pix->samples + ((size_t)y * (size_t)pix->stride);
            for (int x = 0; x < 32; x++) {
                p[0] = 200;
                p[1] = 10;
                p[2] = 20;
                p[3] = 30;
                p += 4;
            }
        }
        // Adobe polarity so PdfCreator (standalone JPEG → Decode invert) embeds
        // a typical Photoshop-style DeviceCMYK JPEG.
        jpegBuf = fz_new_buffer_from_pixmap_as_jpeg(ctx, pix, fz_default_color_params, 95, 1);
        unsigned char* data = nullptr;
        size_t n = fz_buffer_storage(ctx, jpegBuf, &data);
        if (data && n > 0 && n <= (size_t)INT_MAX) {
            jpegSrc = Str((char*)data, (int)n);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        jpegSrc = {};
    }
    if (jpegSrc) {
        built = pdf.AddPageFromImageData(jpegSrc, 72.f);
    }
    fz_drop_buffer(ctx, jpegBuf);
    fz_drop_pixmap(ctx, pix);
    if (!built) {
        return fail(StrL("ERROR embed-cmyk-jpeg"));
    }

    TempStr pdfPath = GetTempFilePathTemp(StrL("cmyksave"));
    if (!pdfPath || !pdf.SaveToFile(pdfPath)) {
        return fail(StrL("ERROR save-pdf"));
    }

    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        file::Delete(pdfPath);
        return fail(StrL("ERROR engine-create-failed"));
    }
    if (!engine->BenchLoadPage(1)) {
        SafeEngineRelease(&engine);
        file::Delete(pdfPath);
        return fail(StrL("ERROR page-load-failed"));
    }
    Vec<IPageElement*> els = engine->GetElements(1);
    IPageElement* imgEl = nullptr;
    for (IPageElement* el : els) {
        if (el && el->Is(kindPageElementImage)) {
            imgEl = el;
            break;
        }
    }
    if (!imgEl) {
        SafeEngineRelease(&engine);
        file::Delete(pdfPath);
        return fail(StrL("ERROR no-image-element"));
    }
    Str jpeg = engine->GetImageDataForPageElement(imgEl);
    SafeEngineRelease(&engine);
    file::Delete(pdfPath);
    if (!jpeg) {
        return fail(StrL("ERROR no-image-data"));
    }
    bool wroteJpeg = file::WriteFile(jpegPath, jpeg);
    bool wroteTiff = TrySaveOriginalAsCmykTiff(jpeg, tiffPath);
    int nComp = JpegSofComponents(jpeg);
    int cw = 0, ch = 0, cstride = 0;
    Vec<u8> cmyk;
    bool decoded = DecodeJpegToCmyk(jpeg, cw, ch, cstride, cmyk);
    int jc = 0, jm = 0, jy = 0, jk = 0;
    if (decoded && cstride >= 4 && len(cmyk) >= 4) {
        jc = cmyk[0];
        jm = cmyk[1];
        jy = cmyk[2];
        jk = cmyk[3];
    }
    Str tiff = file::ReadFile(tiffPath);
    u16 photo = TiffPhotometric(tiff);
    str::Free(tiff);
    str::Free(jpeg);

    out.Append(fmt("jpeg_ok=%d jpeg_n=%d jpeg_c=%d jpeg_m=%d jpeg_y=%d jpeg_k=%d tiff_ok=%d tiff_photo=%d\n",
                   (int)wroteJpeg, nComp, jc, jm, jy, jk, (int)wroteTiff, (int)photo));
    if (exitCodeOut) {
        *exitCodeOut = (wroteJpeg && wroteTiff && nComp == 4 && photo == 5) ? 0 : 1;
    }
    return ToStrTemp(out);
}

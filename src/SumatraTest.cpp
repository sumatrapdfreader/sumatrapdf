/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/ByteReaderWriter.h"
#include "base/Win.h"

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "Settings.h"
#include "AppSettings.h"
#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"
#include "gui/BrowserView.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "ImageReader.h"
#include "ImageSaveCropResize.h"
#include "PdfCreator.h"
#include "PdfCad.h"
#include "DisplayModel.h"
#include "PdfSync.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "WindowTab.h"
#include "PagePosition.h"
#include "Selection.h"
#include "SearchAndDDE.h"
#include "ReadAloud.h"
#include "Translations.h"
#include "MarkdownModel.h"
#include "PageThumbnails.h"
#include "TableOfContents.h"
#include "SidebarPanel.h"

#include <chm.h>
#include "EbookBase.h"
#include "ChmFile.h"
#include "RefHover.h"
#include "SumatraTest.h"

// internal LZX test hook, defined in chm.c but not exposed in chm.h
extern "C" int LZX_test_pretree_make_decode_table(void);

void EnsureTestSettings() {
    // engine creation reads a few fields off gSettings (e.g. disableAntiAlias)
    if (!gSettings) {
        gSettings = NewSettings({});
    }
    // Headless -dbg-control tests don't need form JavaScript. Force it off even
    // when LoadSettings() already ran (the test harness overrides user prefs).
    EngineMupdfSetDisableJavaScript(true);
}

// Headless synctex forward-search test for issue #5633. Loads the pdf, builds
// the synctex index (decompressing .synctex/.synctex.gz as needed) and runs a
// SourceToDoc query, returning a machine-readable result line.
TempStr SynctexResultTemp(Str pdfPath, Str srcPath, int line) {
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

// Headless CHM exercise test. Runs an isolated PRETREE make_decode_table check (so
// ASan can catch the lzx overflow on a heap buffer), opens the chm via chm_open,
// reads every entry, and optionally loads ChmFile / EngineChm.
// Used by tests/issue-chm-lzx.ts; not meant for end users.
TempStr ChmResultTemp(Str chmPath, int* exitCodeOut) {
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
    if (len(fileData) == 0) {
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
        if (len(doc->tocPath) > 0) {
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

// Mouse-drag text selection of rotated glyphs (issue #4839). Finds `word`,
// clicks the first glyph and drags to the last, then reports whether that
// stayed a text selection with tilted quads rather than a rubber-band rect.
TempStr RotatedTextMouseDragResultTemp(Str word, int* exitCodeOut) {
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
    if (!dm || !win->hwndCanvas) {
        return fail(StrL("NOTREADY no-doc"));
    }
    EngineBase* engine = dm->GetEngine();
    const int pageNo = 1;
    Rect* coords = nullptr;
    QuadF* quads = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords, &quads);
    if (len(text) == 0 || !coords) {
        return fail(StrL("ERROR no-page-text"));
    }
    int startGlyph = -1;
    int endGlyph = -1;
    int wordLen = Utf8CodepointCount(word);
    for (int i = 0; i <= textLen - wordLen; i++) {
        if (str::Eq(Utf8SliceByCodepoints(text, i, wordLen), word)) {
            startGlyph = i;
            endGlyph = i + wordLen;
            break;
        }
    }
    if (startGlyph < 0) {
        return fail(StrL("ERROR word-not-found"));
    }
    int first = startGlyph;
    int last = endGlyph - 1;
    for (; first < endGlyph && !coords[first].x && !coords[first].dx; first++) {
    }
    for (; last > first && !coords[last].x && !coords[last].dx; last--) {
    }
    if (first >= endGlyph) {
        return fail(StrL("ERROR empty-glyph-boxes"));
    }
    bool firstTilted = quads && quads[first].IsRotated();
    out.Append(fmt("quads=%d firstTilted=%d start=%d end=%d\n", quads ? 1 : 0, firstTilted ? 1 : 0, first, last));

    PointF p0{(float)(coords[first].x + (coords[first].dx / 2.0)), (float)(coords[first].y + (coords[first].dy / 2.0))};
    PointF p1{(float)(coords[last].x + coords[last].dx), (float)(coords[last].y + (coords[last].dy / 2.0))};
    if (quads) {
        p0 = quads[first].Center();
        // past the last glyph along its baseline so the final letter is included
        p1 = {(quads[last].ur.x + quads[last].lr.x) / 2.f, (quads[last].ur.y + quads[last].lr.y) / 2.f};
    }
    Point s0 = dm->CvtToScreen(pageNo, p0);
    Point s1 = dm->CvtToScreen(pageNo, p1);
    out.Append(fmt("screen0=%d,%d screen1=%d,%d overText0=%d overText1=%d\n", s0.x, s0.y, s1.x, s1.y,
                   dm->IsOverText(s0) ? 1 : 0, dm->IsOverText(s1) ? 1 : 0));
    if (!dm->IsOverText(s0)) {
        return fail(StrL("ERROR start-not-over-text"));
    }

    DeleteOldSelectionInfo(win, true);
    LPARAM lp0 = MAKELPARAM(s0.x, s0.y);
    LPARAM lp1 = MAKELPARAM(s1.x, s1.y);
    SendMessageW(win->hwndCanvas, WM_LBUTTONDOWN, 0, lp0);
    int actionDown = (int)win->mouseAction;
    SendMessageW(win->hwndCanvas, WM_MOUSEMOVE, MK_LBUTTON, lp1);
    SendMessageW(win->hwndCanvas, WM_LBUTTONUP, 0, lp1);
    int actionUp = (int)win->mouseAction;

    WindowTab* tab = win->CurrentTab();
    bool isTextOnly = false;
    TempStr selected = tab ? GetSelectedTextTemp(tab, StrL(" "), isTextOnly) : TempStr{};
    int nrects = (tab && tab->selectionOnPage) ? len(*tab->selectionOnPage) : 0;
    int nQuads = 0;
    int nTilted = 0;
    if (tab && tab->selectionOnPage) {
        for (SelectionOnPage& onPage : *tab->selectionOnPage) {
            if (onPage.HasQuad()) {
                nQuads++;
                if (onPage.quad.IsRotated()) {
                    nTilted++;
                }
            }
        }
    }
    out.Append(fmt("actionDown=%d actionUp=%d isTextOnly=%d nrects=%d nQuads=%d nTilted=%d selected=%s\n", actionDown,
                   actionUp, isTextOnly ? 1 : 0, nrects, nQuads, nTilted, selected));

    bool ok =
        (actionDown == (int)MouseAction::SelectingText) && isTextOnly && nTilted > 0 && str::ContainsI(selected, word);
    if (!ok) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Headless triple-click line-selection test (issue #5712). Loads the pdf, clicks
// the middle of <clickWord>, runs the same TextSelection steps as a double-click
// followed by a triple-click (without the mouse-up trim), and checks the result.
TempStr TripleClickLineSelectResultTemp(Str pdfPath, Str clickWord, Str expectedLine, int* exitCodeOut) {
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
    TempStr selected = ts.ExtractTextTemp(StrL(" "));

    // simulate the old mouse-up bug: re-selecting to the click point trims the line
    TextSelection trimmed(engine);
    trimmed.SelectWordAt(pageNo, x, y);
    trimmed.SelectLineAt(pageNo, x, y);
    trimmed.SelectUpTo(pageNo, x, y);
    TempStr trimmedText = trimmed.ExtractTextTemp(StrL(" "));
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
    if (expectPage <= 0 && dest->loc.chapter >= 1) {
        // chaptered doc: the dest carries a chapter, not yet a resolved page
        Location loc = win->ctrl->ResolveDest(dest);
        expectPage = win->ctrl->PageNoFromLocation(loc);
    }
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

// Drives the real deferred TOC-click path (GoToTocItem, unlike
// TocNavigateResultTemp() doesn't pre-resolve the dest); poll chapterInfo()
// for the "NAVIGATING" result to land. Used by tests/ad-hoc-chapters.ts.
TempStr TocSidebarNavResultTemp(int destNo, int* exitCodeOut) {
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
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    EngineBase* engine = win->AsFixed() ? win->AsFixed()->GetEngine() : nullptr;
    TocTree* toc = engine ? engine->GetToc() : nullptr;
    if (!toc || !toc->root) {
        return fail(StrL("ERROR no-toc"));
    }
    int counter = 0;
    TocItem* item = NthTocItemWithDest(toc->root, destNo, counter);
    if (!item) {
        return fail(fmt("ERROR no-dest destNo=%d", destNo));
    }

    GoToTocItem(win, item);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    out.Append(fmt("NAVIGATING dest=%d\n", destNo));
    return ToStrTemp(out);
}

// Seeds a rectangle + text selection, lays out a chapter (forcing
// SyncWithEngineLayout/PagesRenumbered), reports if both survived.
TempStr RenumberSelResultTemp(int layoutChapter, int* exitCodeOut) {
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
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !engine->HasChapters()) {
        return fail(StrL("ERROR not-chaptered"));
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return fail(StrL("ERROR no-tab"));
    }

    DeleteOldSelectionInfo(win, true);
    RectF r(10, 10, 50, 20);
    tab->selectionOnPage = new Vec<SelectionOnPage>();
    VecAppend(*tab->selectionOnPage, SelectionOnPage(1, &r, nullptr));
    win->showSelection = true;

    // by glyph index, not screen coords: robust regardless of page layout.
    // Scan for the first early page with extractable text (e.g. a cover-only
    // page 1 has none) instead of assuming page 1 has some.
    int textLenBefore = 0;
    for (int p = 1; p <= 5 && p <= dm->PageCount(); p++) {
        int n = 0;
        engine->GetTextForPage(p, &n);
        if (n < 2) {
            continue;
        }
        dm->textSelection->StartAt(p, 0);
        dm->textSelection->SelectUpTo(p, std::min(n, 10));
        textLenBefore = len(dm->textSelection->result);
        if (textLenBefore > 0) {
            break;
        }
    }

    dm->ChapterPageCount(layoutChapter); // lays out the chapter and resyncs dm

    bool survived = tab->selectionOnPage && len(*tab->selectionOnPage) > 0;
    int pageNo = survived ? (*tab->selectionOnPage)[0].pageNo : -1;
    bool textSurvived = textLenBefore > 0 && len(dm->textSelection->result) == textLenBefore;
    if (survived) {
        out.Append(fmt("OK survived=1 pageNo=%d\n", pageNo));
    } else {
        out.Append(StrL("FAIL survived=0\n"));
    }
    out.Append(fmt("textSurvived=%d textLen=%d\n", (int)textSurvived, len(dm->textSelection->result)));
    if (exitCodeOut) {
        *exitCodeOut = (survived && textSurvived) ? 0 : 1;
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

// Verifies Tr resolves error-path strings through the translation table.
TempStr I18nErrorStringResultTemp(int* exitCodeOut) {
    str::Builder out;
    Str err = Tr("Error");
    Str crash = Tr("SumatraPDF crashed");
    Str printers = Tr("SumatraPDF - Show Printers");
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

// Open any document, render page 1, and report dest size plus how many
// red-ish / non-white pixels it has. Used to check that a WebP inside an
// EPUB actually paints (issue #3415) instead of the IMAGE placeholder.
TempStr PageRenderColorsResultTemp(Str path, int* exitCodeOut, int pageNo) {
    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    EngineBase* engine = nullptr;
    bool ownEngine = true;
    if (len(gWindows) > 0 && gWindows[0]) {
        WindowTab* tab = gWindows[0]->CurrentTab();
        if (tab && tab->filePath && str::EqI(tab->filePath, path)) {
            DisplayModel* dm = tab->AsFixed();
            engine = dm ? dm->GetEngine() : nullptr;
            ownEngine = false;
        }
    }
    if (!engine) {
        engine = CreateEngineFromFile(path, nullptr, false);
    }
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", path));
    }
    if (pageNo < 1) {
        pageNo = 1;
    }
    auto release = [&]() {
        if (ownEngine) {
            SafeEngineRelease(&engine);
        }
    };
    if (pageNo > engine->PageCount()) {
        int nPages = engine->PageCount();
        release();
        return fail(fmt("ERROR bad-page page=%d pages=%d\n", pageNo, nPages));
    }
    if (!engine->BenchLoadPage(pageNo)) {
        release();
        return fail(StrL("ERROR page-load-failed\n"));
    }

    RenderPageArgs rargs(pageNo, 1.f, 0, nullptr, RenderTarget::Export);
    Pixmap* bmp = engine->RenderPage(rargs);
    if (!bmp || !bmp->data) {
        FreePixmap(bmp);
        int nPages = engine->PageCount();
        release();
        return fail(fmt("ERROR render-failed page=%d pages=%d\n", pageNo, nPages));
    }
    Pixmap* rgb = bmp;
    if (bmp->format != PixmapFormat::BGRA8 && bmp->format != PixmapFormat::BGR8 && bmp->format != PixmapFormat::RGBA8) {
        rgb = PixmapCopyAs32bppDIB(bmp);
    }
    if (!rgb || !rgb->data) {
        FreePixmap(bmp);
        release();
        return fail(fmt("ERROR pixmap-convert-failed fmt=%d\n", (int)bmp->format));
    }
    int bpp = PixmapBytesPerPixel(rgb->format);
    int red = 0;
    int blue = 0;
    int nonWhite = 0;
    int rMin = 255, rMax = 0, gMin = 255, gMax = 0, bMin = 255, bMax = 0;
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
                if (b > 180 && r < 80 && g < 80) {
                    blue++;
                }
                if (r < rMin) {
                    rMin = r;
                }
                if (r > rMax) {
                    rMax = r;
                }
                if (g < gMin) {
                    gMin = g;
                }
                if (g > gMax) {
                    gMax = g;
                }
                if (b < bMin) {
                    bMin = b;
                }
                if (b > bMax) {
                    bMax = b;
                }
            }
        }
    }
    int spread = (rMax - rMin) + (gMax - gMin) + (bMax - bMin);
    out.Append(fmt("red=%d nonwhite=%d size=%dx%d pages=%d page=%d blue=%d spread=%d\n", red, nonWhite, rgb->width,
                   rgb->height, engine->PageCount(), pageNo, blue, spread));
    if (rgb != bmp) {
        FreePixmap(rgb);
    }
    FreePixmap(bmp);
    if (ownEngine) {
        SafeEngineRelease(&engine);
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Extracts the first image of a page the way Copy Image / Save Image do and
// reports its size and corner colors, so a test can check it is oriented the
// way it is drawn on the page (issue #6214).
// writes px as a 24-bit BMP, for eyeballing the extracted image
static void SavePixmapAsBmp(Pixmap* px, Str bmpPath) {
    int w = px->width;
    int h = px->height;
    int bpp = PixmapBytesPerPixel(px->format);
    int rowBytes = ((w * 3) + 3) & ~3;
    int dataSize = rowBytes * h;
    BITMAPFILEHEADER bfh{};
    BITMAPINFOHEADER bih{};
    bfh.bfType = 0x4d42; // "BM"
    bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
    bfh.bfSize = bfh.bfOffBits + dataSize;
    bih.biSize = sizeof(bih);
    bih.biWidth = w;
    bih.biHeight = h; // bottom-up
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biSizeImage = dataSize;
    str::Builder out;
    out.Append(Str((char*)&bfh, sizeof(bfh)));
    out.Append(Str((char*)&bih, sizeof(bih)));
    Vec<u8> row;
    u8* rowData = VecAppendBlanks(row, rowBytes);
    for (int y = h - 1; y >= 0; y--) {
        const u8* sp = px->data + ((size_t)y * (size_t)px->stride);
        u8* dp = rowData;
        for (int x = 0; x < w; x++) {
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            sp += bpp;
            dp += 3;
        }
        out.Append(Str((char*)rowData, rowBytes));
    }
    file::WriteFile(bmpPath, ToStrTemp(out));
}

TempStr ImageOrientationResultTemp(Str pdfPath, int pageNo, Str bmpPath, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        out.AppendChar('\n');
        return ToStrTemp(out);
    };
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        return fail(StrL("ERROR engine-create-failed"));
    }
    if (!engine->BenchLoadPage(pageNo)) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR page-load-failed"));
    }
    IPageElement* imgEl = nullptr;
    Vec<IPageElement*> els = engine->GetElements(pageNo);
    for (IPageElement* el : els) {
        if (el && el->Is(kindPageElementImage)) {
            imgEl = el;
            break;
        }
    }
    if (!imgEl) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR no-image-element"));
    }
    RenderedBitmap* bmp = engine->GetImageForPageElement(imgEl);
    SafeEngineRelease(&engine);
    if (!bmp) {
        return fail(StrL("ERROR no-image"));
    }
    Pixmap* px = PixmapToBgra(PixmapFromRenderedBitmap(bmp)); // takes ownership of bmp
    if (!px || !px->data) {
        FreePixmap(px);
        return fail(StrL("ERROR no-pixmap"));
    }
    int w = px->width;
    int h = px->height;
    out.Append(fmt("w=%d h=%d tl=%s tr=%s bl=%s br=%s\n", w, h, PixmapRgbHexTemp(px, 0, 0),
                   PixmapRgbHexTemp(px, w - 1, 0), PixmapRgbHexTemp(px, 0, h - 1), PixmapRgbHexTemp(px, w - 1, h - 1)));
    if (len(bmpPath) > 0) {
        SavePixmapAsBmp(px, bmpPath);
    }
    FreePixmap(px);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Current chapter/page and chapter table state of the front window's doc.
// Used by tests/ad-hoc-chapters.ts.
TempStr ChapterInfoResultTemp(int* exitCodeOut) {
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
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    DocController* ctrl = win->ctrl;
    Location cur = ctrl->CurrentLocation();
    bool hasChapters = ctrl->HasChapters();
    int laidOut = 0;
    DisplayModel* dm = ctrl->AsFixed();
    if (dm && dm->GetEngine()) {
        laidOut = dm->GetEngine()->ChaptersLaidOut();
    }
    bool chapterUi = ShowChapterUi(ctrl);
    out.Append(
        fmt("OK chapter=%d page=%d chapterCount=%d chapterPageCount=%d pageCount=%d hasChapters=%d "
            "laidOut=%d chapterUi=%d\n",
            cur.chapter, cur.page, ctrl->ChapterCount(), ctrl->ChapterPageCount(cur.chapter), ctrl->PageCount(),
            hasChapters ? 1 : 0, laidOut, chapterUi ? 1 : 0));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Seeds a glyph-level (quad) text selection on `pageNo` of the current tab, the
// way a left-drag across the page would, and reports the flat page numbers it
// holds. Coordinates would have to be hunted for, so select by glyph index.
//
// A rectangle selection is useless for the stale-page question: its paint path
// (SelectionOnPage::GetRect) null-checks GetPageInfo and silently draws
// nothing. Only a quad selection reaches DisplayModel::CvtToScreen unguarded,
// which is where crash 2026-09-12-09-59-1328 reported.
// Used by tests/epub-relayout-stale-page.ts.
void DiscardUnsavedChangesInAllTabs() {
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            ResolveUnsavedChanges(tab, UnsavedChangesAction::Discard);
        }
    }
}

// action: "discard" | "save" (every tab) | "save-as" <path> (current tab).
TempStr ResolveUnsavedChangesResultTemp(Str action, Str path, int* exitCodeOut) {
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
    if (str::Eq(action, StrL("save-as"))) {
        WindowTab* tab = gWindows[0]->CurrentTab();
        if (len(path) == 0 || !tab) {
            return fail(StrL("ERROR save-as needs a path and a document"));
        }
        if (!ResolveUnsavedChanges(tab, UnsavedChangesAction::SaveNew, path)) {
            return fail(fmt("ERROR save-as '%s' failed", path));
        }
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        out.Append(StrL("OK tabs=1\n"));
        return ToStrTemp(out);
    }

    UnsavedChangesAction act;
    if (str::Eq(action, StrL("discard"))) {
        act = UnsavedChangesAction::Discard;
    } else if (str::Eq(action, StrL("save"))) {
        act = UnsavedChangesAction::SaveExisting;
    } else {
        return fail(fmt("ERROR unknown action '%s'", action));
    }
    int nTabs = 0;
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (!ResolveUnsavedChanges(tab, act)) {
                return fail(fmt("ERROR save of '%s' failed", tab->filePath));
            }
            nTabs++;
        }
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    out.Append(fmt("OK tabs=%d\n", nTabs));
    return ToStrTemp(out);
}

TempStr SeedTextSelectionResultTemp(int pageNo, int* exitCodeOut) {
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
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return fail(StrL("ERROR no-tab"));
    }
    if (!dm->ValidPageNo(pageNo)) {
        return fail(fmt("ERROR invalid-page pageNo=%d pageCount=%d", pageNo, dm->PageCount()));
    }

    EngineBase* engine = dm->GetEngine();
    int textLen = 0;
    engine->GetTextForPage(pageNo, &textLen);
    if (textLen < 2) {
        return fail(fmt("ERROR no-text pageNo=%d", pageNo));
    }

    DeleteOldSelectionInfo(win, true);
    dm->textSelection->StartAt(pageNo, 0);
    dm->textSelection->SelectUpTo(pageNo, textLen - 1);
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = tab->selectionOnPage != nullptr;
    if (!tab->selectionOnPage) {
        return fail(fmt("ERROR empty-selection pageNo=%d", pageNo));
    }

    int first = (*tab->selectionOnPage)[0].pageNo;
    int last = VecLast(*tab->selectionOnPage).pageNo;
    int quads = 0;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        if (sel.HasQuad()) {
            quads++;
        }
    }
    out.Append(fmt("OK parts=%d quads=%d first=%d last=%d pageCount=%d\n", len(*tab->selectionOnPage), quads, first,
                   last, dm->PageCount()));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Renders a blank strip of pages 1 and 2 as one selection image and counts
// its white pixels. Used by tests/render-selections-8bpp.ts.
TempStr RenderSelectionsResultTemp(int* exitCodeOut) {
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
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm || dm->PageCount() < 2) {
        return fail(StrL("NOTREADY need-two-pages"), 2);
    }

    Vec<SelectionOnPage> sels;
    RectF r(72, 300, 200, 40);
    VecAppend(sels, SelectionOnPage(1, &r, nullptr));
    VecAppend(sels, SelectionOnPage(2, &r, nullptr));
    RenderedBitmap* rb = RenderSelectionsAsRenderedBitmap(dm, sels);
    Pixmap* px = PixmapFromRenderedBitmap(rb);
    if (!px || !px->data) {
        FreePixmap(px);
        return fail(StrL("ERROR no-bitmap"));
    }
    int white = 0;
    int total = px->width * px->height;
    if (px->format == PixmapFormat::BGRA8) {
        for (int y = 0; y < px->height; y++) {
            const u32* row = (const u32*)(px->data + ((size_t)y * px->stride));
            for (int x = 0; x < px->width; x++) {
                if ((row[x] & 0xffffff) == 0xffffff) {
                    white++;
                }
            }
        }
    }
    TempStr res = fmt("OK w=%d h=%d format=%d white=%d total=%d", px->width, px->height, (int)px->format, white, total);
    FreePixmap(px);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return res;
}

// Citation hover popup state. action "show" first opens the popup for the link
// at canvas point (x, y), as hovering does: a test's cursor can't hold a hover.
// Used by tests/issue-6252.ts.
TempStr RefHoverResultTemp(Str action, int x, int y, int* exitCodeOut) {
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (str::Eq(action, StrL("show")) && dm) {
        if (!win->refHover) {
            win->refHover = RefHoverCreate(win->hwndCanvas);
        }
        win->refHover->ctrl = win->ctrl;
        win->refHover->linkHandler = win->linkHandler;
        IPageElement* el = dm->GetElementAtPos({x, y}, nullptr);
        if (!RefHoverScheduleLink(win->refHover, win->hwndCanvas, dm, x, y, el, 0)) {
            if (exitCodeOut) {
                *exitCodeOut = 1;
            }
            return fmt("ERROR no-link x=%d y=%d", x, y);
        }
    }
    RefHoverState* s = win ? win->refHover : nullptr;
    if (!s || !s->hwndPopup || !HwndIsVisible(s->hwndPopup)) {
        return fmt("OK visible=0");
    }
    auto& d = s->displayed;
    return fmt("OK visible=1 hwnd=%d page=%d y=%d zoom=%d", (int)(INT_PTR)s->hwndPopup, d.destPage, (int)d.region.y,
               (int)(d.userZoom * 100));
}

// The current tab's pages: each page's width, which a test gives a unique value
// per page, and where the bookmarks point. Used by tests/issue-6070.ts.
TempStr PageInfoResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-document")));
    }
    str::Builder out;
    EngineBase* engine = dm->GetEngine();
    out.Append(fmt("pages=%d widths=", engine->PageCount()));
    for (int i = 1; i <= engine->PageCount(); i++) {
        out.Append(fmt(i == 1 ? "%d" : ",%d", (int)lroundf(engine->PageMediabox(i).dx)));
    }
    out.Append(StrL(" toc="));
    TocTree* toc = engine->GetToc();
    for (TocItem* it = toc && toc->root ? toc->root->child : nullptr; it; it = it->next) {
        out.Append(fmt("%s:%d;", it->title, it->pageNo));
    }
    return finish(0, fmt("OK %s", ToStrTemp(out)));
}

// The sidebar's panels (HWND, visible, view, the view icons' enabled / selected
// state and client rects) and its Thumbnails view: whether it shows, the current
// page and, for clicking, each visible thumbnail in
// its panel's client coords. Used by tests/sidebar-thumbnails.ts.
TempStr SidebarThumbnailsResultTemp(int* exitCodeOut) {
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    PageThumbnailsCtrl* thumbs = win ? win->pageThumbs : nullptr;
    if (!thumbs) {
        if (exitCodeOut) {
            *exitCodeOut = 2;
        }
        return str::DupTemp(StrL("NOTREADY no-window"));
    }
    SidebarPanel* shows = SidebarPanelShowing(win, SidebarView::Thumbnails);
    HWND hwnd = shows ? shows->hwnd : win->sidebarTop->hwnd;
    str::Builder sb;
    sb.Append(fmt("hwnd=%d thumbnails=%d count=%d current=%d rendered=%d", (int)(intptr_t)hwnd,
                  (int)thumbs->IsVisible(), thumbs->pageCount, thumbs->selectedPage, thumbs->RenderedCount()));
    // the focus ring is drawn while the thumbnails have the (virtual) focus
    sb.Append(fmt(" ring=%d", (int)thumbs->HasFlag(vwfFocused)));
    // e.g. top=1234,1,thumbnails,110,010:2,2,22,22;26,2,22,22;50,2,22,22
    // (enabled icons, selected icon, icon rects in order B, T, F)
    SidebarPanel* panels[] = {win->sidebarTop, win->sidebarBottom};
    bool visible[] = {win->uiState.sidebarTopVisible, win->uiState.sidebarBottomVisible};
    Str names[] = {StrL("top"), StrL("bottom")};
    for (int i = 0; i < 2; i++) {
        SidebarPanel* p = panels[i];
        sb.Append(fmt(" %s=%d,%d,%s,", names[i], (int)(intptr_t)p->hwnd, (int)visible[i], SidebarViewToStr(p->view)));
        for (VirtIconButton* b : p->viewBtns) {
            sb.Append(fmt("%d", (int)b->IsEnabled()));
        }
        sb.Append(StrL(","));
        for (VirtIconButton* b : p->viewBtns) {
            sb.Append(fmt("%d", (int)b->isSelected));
        }
        sb.Append(StrL(":"));
        for (int j = 0; j < kSidebarViewCount; j++) {
            Rect r = p->viewBtns[j]->BoundsInWindow();
            sb.Append(fmt(j == 0 ? "%d,%d,%d,%d" : ";%d,%d,%d,%d", r.x, r.y, r.dx, r.dy));
        }
    }
    sb.Append(StrL(" rects="));
    for (int pageNo = 1; pageNo <= thumbs->pageCount; pageNo++) {
        Rect r = thumbs->PageRect(pageNo);
        sb.Append(fmt("%d:%d,%d,%d,%d;", pageNo, r.x, r.y, r.dx, r.dy));
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(sb);
}

// The first window's frame: whether it's maximized and the non-client strips
// its WM_NCPAINT fills (window coords). Used by tests/issue-6259.ts.
TempStr FrameNcStripsResultTemp(int* exitCodeOut) {
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        if (exitCodeOut) {
            *exitCodeOut = 2;
        }
        return str::DupTemp(StrL("NOTREADY no-window"));
    }
    Vec<Rect> strips;
    GetFrameNcStrips(win, strips);
    str::Builder sb;
    sb.Append(fmt("zoomed=%d strips=%d", (int)IsZoomed(win->hwndFrame), len(strips)));
    for (Rect& r : strips) {
        sb.Append(fmt(" %d,%d,%d,%d", r.x, r.y, r.dx, r.dy));
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(sb);
}

// Sends the frame a mouse wheel while the window is marked as being closed, as
// when a wheel arrives during CloseWindow(). Used by tests/wheel-while-closing.ts.
TempStr WheelWhileClosingResultTemp(int* exitCodeOut) {
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win || !win->IsDocLoaded()) {
        if (exitCodeOut) {
            *exitCodeOut = 2;
        }
        return str::DupTemp(StrL("NOTREADY no-document"));
    }
    win->isBeingClosed = true;
    SendMessageW(win->hwndFrame, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), 0);
    win->isBeingClosed = false;
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return str::DupTemp(StrL("OK"));
}

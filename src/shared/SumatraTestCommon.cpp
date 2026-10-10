/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/ByteReaderWriter.h"
#include "base/GuessFileType.h"

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "gui/UIModels.h"
#include "gui/BrowserView.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
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
#include "TableOfContents.h"
#include "SumatraTest.h"

#include "SumatraLog.h"

// Answers to -dbg-control test commands that orig and ng compute the same way.
// ng has no listener thread on wasm, so nothing calls them there.
#if !OS_WASM

// Headless inverse-search test for issue #5702. Loads the pdf, creates a
// Synchronizer, and resolves (page, point) -> (srcfile, line, col) via
// DocToSource, returning a machine-readable result line.
TempStr InverseSearchResultTemp(Str pdfPath, int pageNo, int x, int y) {
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
        if (triedPassword || len(password) == 0) {
            return {};
        }
        triedPassword = true;
        return str::Dup(password);
    }
};

TempStr SearchResultTemp(Str pdfPath, Str needle, Str password) {
    str::Builder out;
    TestPasswordUI pwdUI(password);
    EngineBase* engine = CreateEngineFromFile(pdfPath, password ? &pwdUI : nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        auto* ts = new TextSearch(engine);
        ts->SetDirection(TextSearch::Direction::Forward);
        ts->SetMatchCase(false);
        Vec<TextSel>* sel = ts->FindFirst(1, needle);
        if (sel && len(*sel) > 0) {
            out.Append(fmt("FOUND needle=%s page=%d\n", needle, (*sel)[0].pageNo));
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
    Vec<bool> allowed;
    if (spec) {
        if (!ParseFindPageRange(spec, engine->PageCount(), allowed)) {
            VecReset(allowed);
        }
    } else if (first > 0 || last > 0) {
        int lo = first > 0 ? first : 1;
        int hi = last > 0 ? last : ts->nPages;
        if (lo > hi) {
            std::swap(lo, hi);
        }
        VecResize(allowed, ts->nPages);
        for (int page = 1; page <= ts->nPages; page++) {
            allowed[page - 1] = page >= lo && page <= hi;
        }
    }
    ts->SetAllowedPages(allowed);
    int n = 0;
    Vec<TextSel>* sel = ts->FindFirst(ts->RestrictFirst(), needle);
    while (sel && len(*sel) > 0) {
        out.Append(fmt("page=%d\n", (*sel)[0].pageNo));
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
IPageDestination* NthDestInToc(TocItem* item, int target, int& counter) {
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

TocItem* NthTocItemWithDest(TocItem* item, int target, int& counter) {
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
            out.Append(fmt("dest=%d page=%d zoom=%g\n", destNo, PageDestGetPageNo(dest), dest->GetZoom()));
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
// (CleanRemoteDestNameInPlace + GetNamedDest), returning the resolved page.
// Used by tests/issue-5642.ts.
TempStr NamedDestResultTemp(Str pdfPath, Str destName) {
    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        Str name = destName;
        CleanRemoteDestNameInPlace(name);
        IPageDestination* dest = engine->GetNamedDest(name);
        if (dest) {
            out.Append(fmt("name=%s page=%d\n", destName, PageDestGetPageNo(dest)));
        } else {
            out.Append(fmt("name=%s NOTFOUND\n", destName));
        }
        SafeEngineRelease(&engine);
    }

    return ToStrTemp(out);
}

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

// find the [start, end) glyph range of the first occurrence of `word` on a page
static bool FindWordGlyphRange(EngineBase* engine, int pageNo, Str word, int* startOut, int* endOut) {
    if (!engine || len(word) == 0 || !startOut || !endOut) {
        return false;
    }
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen);
    if (len(text) == 0) {
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
// got an empty result (len(*result) == 0), tripped a ReportIf, and failed to
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
    bool hasResult = len(ts->result) > 0;

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

bool FindWordCenter(EngineBase* engine, int pageNo, Str word, double* xOut, double* yOut) {
    if (!engine || len(word) == 0 || !xOut || !yOut) {
        return false;
    }
    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (len(text) == 0) {
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
                   dest ? dest->GetZoom() : 0.f, dest ? PageDestGetPageNo(dest) : 0, dm->CurrentPageNo(), zoomBefore,
                   zoomAfter, gSettings->ignoreDestinationZoom ? 1 : 0));
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
        return finish(fmt("NAVIGATING dest=%d name=%s", destNo, item->dest->GetName()), 0);
    }

    Point pos = BrowserViewGetScrollPos(mm->docView);
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
        if (len(href) == 0) {
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
        Str value = dest->GetValue();
        TempStr valueShown = str::ReplaceTemp(value, StrL("\r\n"), StrL("|"));
        valueShown = str::ReplaceTemp(valueShown, StrL("\n"), StrL("|"));
        RectF src = el->GetRect();
        RectF destRc = dest->GetRect();
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

int CountNonWhitePixels(Pixmap* bmp) {
    if (!bmp || !bmp->data) {
        return 0;
    }
    int bpp = PixmapBytesPerPixel(bmp->format);
    if (bpp < 3) {
        return 0;
    }
    int n = 0;
    for (int y = 0; y < bmp->height; y++) {
        const u8* row = bmp->data + ((size_t)y * (size_t)bmp->stride);
        for (int x = 0; x < bmp->width; x++) {
            const u8* px = row + ((size_t)x * bpp);
            int r, g, b;
            if (bmp->format == PixmapFormat::RGBA8) {
                r = px[0];
                g = px[1];
                b = px[2];
            } else {
                b = px[0];
                g = px[1];
                r = px[2];
            }
            if (r < 250 || g < 250 || b < 250) {
                n++;
            }
        }
    }
    return n;
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
        int e = (int)ifd + 2 + ((int)i * 12);
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
    if (len(jpegPath) == 0 || len(tiffPath) == 0) {
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
    if (len(pdfPath) == 0 || !pdf.SaveToFile(pdfPath)) {
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
    if (len(jpeg) == 0) {
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

TempStr PixmapRgbHexTemp(Pixmap* px, int x, int y) {
    int bpp = PixmapBytesPerPixel(px->format);
    u8* p = px->data + ((size_t)y * (size_t)px->stride) + ((size_t)x * (size_t)bpp);
    // BGR order in memory
    return fmt("%02x%02x%02x", (int)p[2], (int)p[1], (int)p[0]);
}

// Toggle the idx-th (0-based) checkbox / radio widget on pageNo, as a click
// on it would. Reports the field value before and after.
TempStr ToggleFormButtonResultTemp(int pageNo, int idx, int* exitCodeOut) {
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
    if (!dm->ValidPageNo(pageNo)) {
        return fail(fmt("ERROR invalid-page pageNo=%d pageCount=%d", pageNo, dm->PageCount()));
    }

    Vec<Annotation*> widgets;
    EngineMupdfGetPageWidgets(dm->GetEngine(), pageNo, widgets);
    Annotation* button = nullptr;
    int nButtons = 0;
    for (Annotation* w : widgets) {
        int wt = GetWidgetType(w);
        if (wt != PDF_WIDGET_TYPE_CHECKBOX && wt != PDF_WIDGET_TYPE_RADIOBUTTON) {
            continue;
        }
        if (nButtons == idx) {
            button = w;
        }
        nButtons++;
    }
    if (!button) {
        return fail(fmt("ERROR no-button idx=%d buttons=%d", idx, nButtons));
    }

    TempStr before = str::DupTemp(GetWidgetValue(button));
    bool toggled = ToggleFormButton(button);
    TempStr after = str::DupTemp(GetWidgetValue(button));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    out.Append(fmt("OK toggled=%d before='%s' after='%s' buttons=%d\n", (int)toggled, before, after, nButtons));
    return ToStrTemp(out);
}

// A BGRA8 heap pixmap is readable. A GDI DIB is readable only after a copy.
Pixmap* EnsureReadablePixmap(Pixmap* p) {
    if (!p) {
        return nullptr;
    }
    if (p->data && p->format == PixmapFormat::BGRA8) {
        return p;
    }
#if OS_WIN
    return PixmapCopyAs32bppDIB(p);
#else
    return nullptr;
#endif
}

// a spot on pageNo with no text under it, in canvas pixels
Point FindEmptySpotOnPage(MainWindow* win, DisplayModel* dm, int pageNo) {
    Rect client = Rect(0, 0, win->canvasRc.dx, win->canvasRc.dy);
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
            if (dm->GetElementAtPos(pt, nullptr)) {
                continue;
            }
            return pt;
        }
    }
    return Point{};
}

// Color histogram of a page rendered with the CAD enhancement forced on.
// tests/issue-5937.ts.
TempStr CadEnhanceColorsResultTemp(Str path, int pageNo, int zoomPercent, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

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

    float zoom = (float)zoomPercent / 100.f;
    RenderPageArgs args(pageNo, zoom, 0);
    Pixmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR render-failed\n"));
    }
    Pixmap* rgb = EnsureReadablePixmap(bmp);
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
            if (px[0] == px[1] && px[1] == px[2]) {
                counts[px[0]]++;
            }
        }
    }
    out.Append(fmt("size=%dx%d\n", rgb->width, rgb->height));
    if (engine->HasErrors()) {
        out.Append(fmt("errors=%s\n", engine->GetErrorsTextTemp()));
    }
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
// clipKind values of ImageRenderEdgesResultTemp
constexpr int kClipSelection = 1;

constexpr int kClipRightHalfTile = 2;

constexpr int kClipFullPageTile = 3;

// Render an image page and report dest size plus the RGB of the left and right
// edge pixels. clipKind 1 is the copy-selection rect, 2 a right-half tile
// after a full-page render, 3 the page/pixel round trip.
TempStr ImageRenderEdgesResultTemp(Str path, int zoomPercent, int clipKind, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
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
    if (clipKind == kClipSelection) {
        // same half-pixel pull-back CvtFromScreen applies to a pixel-aligned
        // selection of the whole image
        clip = RectF(-0.499f, -0.499f, box.dx, box.dy);
        pageRect = &clip;
    }
    if (clipKind == kClipRightHalfTile) {
        // full render first so mupdf caches the whole decoded image, then a
        // tile of the right half
        RenderPageArgs full(1, zoom, 0, nullptr, RenderTarget::Export);
        FreePixmap(engine->RenderPage(full));
        clip = RectF(box.dx / 2, 0, box.dx / 2, box.dy);
        pageRect = &clip;
    }
    Rect tile;
    if (clipKind == kClipFullPageTile) {
        tile = engine->Transform(box, 1, zoom, 0).Round();
        clip = engine->Transform(ToRectF(tile), 1, zoom, 0, true);
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
        int srcFmt = (int)bmp->format;
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR pixmap-fmt=%d\n", srcFmt));
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
    out.Append(fmt("size=%dx%d left=%d,%d,%d right=%d,%d,%d", bmp->width, bmp->height, lr, lg, lb, rr, rg, rb));
    if (clipKind == kClipFullPageTile) {
        out.Append(fmt(" tile=%dx%d", tile.dx, tile.dy));
    }
    out.Append(StrL("\n"));

    FreePixmap(bmp);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// stamp an image onto page 1 and count red pixels. tests/issue-1744.ts.
TempStr ImageInsertResultTemp(Str pdfPath, Str imagePath, int* exitCodeOut) {
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
    Pixmap* rgb = EnsureReadablePixmap(bmp);
    if (!rgb || !rgb->data) {
        int srcFmt = (int)bmp->format;
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR pixmap-convert-failed fmt=%d\n", srcFmt));
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

// View vs print ink. A print-only OCG must still paint for print (issue #6101).
TempStr PageRenderViewPrintResultTemp(Str path, int* exitCodeOut) {
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

    RenderPageArgs viewArgs(1, 1.f, 0, nullptr, RenderTarget::View);
    RenderPageArgs printArgs(1, 1.f, 0, nullptr, RenderTarget::Print);
    Pixmap* viewRaw = engine->RenderPage(viewArgs);
    Pixmap* printRaw = engine->RenderPage(printArgs);
    if (!viewRaw || !viewRaw->data || !printRaw || !printRaw->data) {
        FreePixmap(viewRaw);
        FreePixmap(printRaw);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR render-failed\n"));
    }
    Pixmap* view = EnsureReadablePixmap(viewRaw);
    Pixmap* print = EnsureReadablePixmap(printRaw);
    if (!view || !view->data || !print || !print->data) {
        if (view != viewRaw) {
            FreePixmap(view);
        }
        if (print != printRaw) {
            FreePixmap(print);
        }
        FreePixmap(viewRaw);
        FreePixmap(printRaw);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR pixmap-convert-failed\n"));
    }

    int viewN = CountNonWhitePixels(view);
    int printN = CountNonWhitePixels(print);
    out.Append(fmt("viewNonwhite=%d printNonwhite=%d viewSize=%dx%d printSize=%dx%d\n", viewN, printN, view->width,
                   view->height, print->width, print->height));
    if (view != viewRaw) {
        FreePixmap(view);
    }
    if (print != printRaw) {
        FreePixmap(print);
    }
    FreePixmap(viewRaw);
    FreePixmap(printRaw);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Navigate to {chapter, page} (clamped) and report where it landed.
TempStr GoToLocationResultTemp(int chapter, int page, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    if (!win->IsDocLoaded() || !win->ctrl) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    DocController* ctrl = win->ctrl;
    Location want = ctrl->ClampLocation({chapter, page});
    ctrl->GoToLocation(want, true);
    Location got = ctrl->CurrentLocation();
    out.Append(fmt("OK chapter=%d page=%d\n", got.chapter, got.page));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// GoToPage on a background tab so UpdateScrollbars sees a non-current dm.
TempStr HiddenTabGoToPageResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code) -> TempStr {
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
    if (!win) {
        return fail(StrL("NOTREADY no-window"), 2);
    }

    DisplayModel* dm = nullptr;
    for (WindowTab* tab : win->Tabs()) {
        if (tab && tab != win->CurrentTab() && tab->AsFixed()) {
            dm = tab->AsFixed();
            break;
        }
    }
    if (!dm) {
        return fail(StrL("NOTREADY no-hidden-doc"), 2);
    }

    dm->GoToPage(dm->CurrentPageNo(), false);
    out.Append(StrL("OK\n"));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

#endif // !OS_WASM
